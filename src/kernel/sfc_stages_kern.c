#include <stddef.h>

#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/pkt_cls.h>


#include "bpf_endian.h"
#include "bpf_helpers.h"
#include "cb_helpers.h"
#include "bpf_elf.h"
#include "cb_common.h"
#include "nsh.h"

#define ADJ_STAGE 1
#define FWD_STAGE 2

/* Map containing source MAC */
SRCMACMAP();

/* Table to temporarily store NSH data */
MAP(nsh_data, BPF_MAP_TYPE_HASH, sizeof(struct ip_5tuple),
    sizeof(struct nshhdr_md1), 2048, PIN_GLOBAL_NS);

/* Table with SFC forwarding rules */
MAP(fwd_table, BPF_MAP_TYPE_HASH, sizeof(__u32),
    sizeof(struct fwd_entry), 2048, PIN_GLOBAL_NS);

#ifdef ENABLE_STATS
STATSMAP();
#endif /* ENABLE_STATS */

// The pointer passed to this function must have been
// bounds checked already
static inline int set_src_mac(struct ethhdr *eth, enum srcmac_idx idx){
	void* smac;
	__u32 key = (__u32) idx;

	// Get src MAC from table. Is there a better way?
	smac = bpf_map_lookup_elem(&src_mac,&key);
	if(smac == NULL){
		cb_debug("[SET_SRC_MAC]: No source MAC configured\n");
		return -1;
	}

	__builtin_memmove(eth->h_source,smac,ETH_ALEN);

	return 0;
}

SEC("xdp/decap")
int decap_nsh(struct xdp_md *ctx)
{
    /* Start timestamps for statistics (if enabled) */
    cb_mark_init();

    void *data_end = (void *)(long)ctx->data_end;
	void *data = (void *)(long)ctx->data;
	struct ethhdr *eth;
	struct vlanhdr *vlan;
	struct nshhdr_md1 *nsh;
	struct iphdr *ip;
	struct ip_5tuple key = { }; // Verifier does not allow uninitialized keys
	int offset = 0;
    __u16 h_proto = 0;
    void *smac;
	int ret = 0;
	__u32 idx = INGRESS_MAC;
	eth = data;

	if(data + sizeof(struct ethhdr) > data_end)
		return cb_retother(XDP_PASS);

    offset += sizeof(struct ethhdr);

	smac = bpf_map_lookup_elem(&src_mac,&idx);
	if(smac == NULL){
		cb_debug("[DECAP]: No source MAC configured\n");
		return cb_retother(XDP_PASS);
	}

	char* mymac = (char*) smac;
	char* dstmac = (char*) data;

	// If broadcast or multicast, pass
    if(dstmac[0] & 1)
        return cb_retother(XDP_PASS);

	// Hack to compare MAC addresses
	// At the time of writing, __builtin_memcmp()
	// was not working.
    #pragma clang loop unroll(full)
	for(int i = 5 ; i >= 0 ; i--){
		if(mymac[i] ^ dstmac[i]){
			cb_debug("[DECAP] Not for me. Dropping. (1/2)\n");
			cb_debug("[DECAP] MAC: %02x %02x %02x", dstmac[5], dstmac[4], dstmac[3]);
			cb_debug(" %02x %02x %02x\n (2/2)", dstmac[2], dstmac[1], dstmac[0]);
			return cb_retother(XDP_DROP);
		}
	}

	cb_debug("[DECAP] oeth->proto: 0x%x\n",bpf_ntohs(eth->h_proto));

    // TODO: Handle other VLAN types
	if(bpf_ntohs(eth->h_proto) == ETH_P_8021Q){
        vlan = (void*)eth + sizeof(struct ethhdr);
        offset += sizeof(struct vlanhdr);

        if((void*)vlan + sizeof(struct vlanhdr) > data_end)
            return cb_retother(XDP_DROP);

        cb_debug("[DECAP] vlan->proto: 0x%x\n",
            bpf_ntohs(vlan->h_vlan_encapsulated_proto));

        h_proto = bpf_ntohs(vlan->h_vlan_encapsulated_proto);
    }else{
        h_proto = bpf_ntohs(eth->h_proto);
    }

	if(h_proto != ETH_P_NSH)
		return cb_retother(XDP_PASS);

	nsh = data + offset;
    offset += sizeof(struct nshhdr_md1);

	// Check if we can access NSH
	if ((void*) nsh + sizeof(struct nshhdr_md1) > data_end)
		return cb_retother(XDP_DROP);

	cb_debug("[DECAP] SPH: 0x%x\n",bpf_ntohl(nsh->serv_path));

    // Check if next protocol is Ethernet
	if(nsh->next_proto != NSH_NEXT_PROTO_ETHER)
		return cb_retother(XDP_DROP);

	// Single check for Inner Ether + IP
	if((void*)nsh + sizeof(struct nshhdr_md1) + sizeof(struct ethhdr) +
      sizeof(struct iphdr) > data_end)
		return cb_retother(XDP_DROP);

	// cb_debug("[DECAP] \n");

	ip = (void*)nsh + sizeof(struct nshhdr_md1) + sizeof(struct ethhdr);

	// Retrieve IP 5-tuple
	ret = get_tuple(ip,data_end,&key);
	if(ret){
		cb_debug("[DECAP] get_tuple() failed.\n");
		return cb_retother(XDP_DROP);
	}

	// Save NSH data
	ret = bpf_map_update_elem(&nsh_data, &key, nsh, BPF_ANY);
	if(ret == 0){
		cb_debug("[DECAP] Stored NSH in table.\n");
	}

	cb_debug("[DECAP] Previous size: %d\n",data_end-data);

	// Remove outer encapsulation
	bpf_xdp_adjust_head(ctx, (int)(offset));

	data_end = (void *)(long)ctx->data_end;
	data = (void *)(long)ctx->data;

	cb_debug("[DECAP] Decapsulated packet; Size after: %d\n",data_end-data);

	return cb_retok(XDP_PASS);
}

SEC("action/encap")
int encap_nsh(struct __sk_buff *skb)
{
  void *data_end = (void *)(long)skb->data_end;
	void *data = (void *)(long)skb->data;
	int ret = 0;
	struct ip_5tuple key = { }; // Verifier does not allow uninitialized keys
	struct nshhdr_md1 *prev_nsh, *nsh;
	struct ethhdr *ieth, *oeth;
	struct iphdr *ip;
	__u16 prev_proto;
	__be32 sph, spi, si;
	struct fwd_entry *next_hop;

  /* Start timestamps for statistics (if enabled) */
  cb_mark_init();

	if(data + sizeof(struct ethhdr) + sizeof(struct iphdr) > data_end){
		cb_debug("[ENCAP]: Bounds check failed\n");
		return cb_retok(TC_ACT_OK);
	}

  // Check if frame contains IP. Pass along otherwise
	oeth = data;
  if(oeth->h_proto != bpf_htons(ETH_P_IP))
    return cb_retok(TC_ACT_OK);

  ip = data + sizeof(struct ethhdr);

	ret = get_tuple(ip,data_end,&key);
	if (ret < 0) {
		cb_debug("[ENCAP]: get_tuple() failed: %d\n", ret);
		return cb_retok(TC_ACT_OK);
	}

	// Check if we have a corresponding NSH saved
  prev_nsh = bpf_map_lookup_elem(&nsh_data,&key);
	if(prev_nsh == NULL){
		cb_debug("[ENCAP] NSH header not found in table.\n");

    /* Set src MAC and send it away */
    if(set_src_mac(oeth,EGRESS_MAC))
      return cb_retother(TC_ACT_SHOT);

    return cb_retok(TC_ACT_OK);
	}

	cb_debug("[ENCAP]: First look: SPH = 0x%x\n",bpf_ntohl(prev_nsh->serv_path));

	sph = bpf_ntohl(prev_nsh->serv_path);
	spi = sph & 0xFFFFFF00;
	si = sph & 0x000000FF;
	cb_debug("[ENCAP]: SPH = 0x%x ; SPI = 0x%x ; SI = 0x%x \n",sph,spi>>8,si);

	if(si == 0){
		cb_debug("[ENCAP]: Anomalous SI number. Dropping packet.\n");
		return cb_retother(TC_ACT_SHOT);
	}

	// Update the SI this way is safe because we
	// already checked if it is 0
	sph--;

	// Check if it is end of chain
	// If it is, we don't have to re-encapsulate it
	cb_debug("[ENCAP]: Pre-lookup: SPH = 0x%x\n",sph);

	__u32 fkey = bpf_htonl(sph);
	next_hop = bpf_map_lookup_elem(&fwd_table,&fkey);
	if(likely(next_hop) && (next_hop->flags & 0x1)){
		cb_debug("[ENCAP]: End of chain! SPH = 0x%x\n",sph);

    /* Set src MAC and send it away */
    if(set_src_mac(oeth,EGRESS_MAC))
      return cb_retother(TC_ACT_SHOT);

    return cb_retok(TC_ACT_OK);
	}

	// Save previous EtherType before it is overwritten
	ieth = data;
	prev_proto = ieth->h_proto;

  // Add outer encapsulation
  ret = bpf_skb_adjust_room(skb,sizeof(struct ethhdr) + sizeof(struct nshhdr_md1),
            BPF_ADJ_ROOM_MAC,0);

  if (ret < 0) {
    cb_debug("[ENCAP]: Failed to add extra room: %d\n", ret);
    return cb_retother(TC_ACT_SHOT);
  }

	data = (void *)(long)skb->data;
	data_end = (void *)(long)skb->data_end;

	// Bounds check to please the verifier
	if(data + 2*sizeof(struct ethhdr) + sizeof(struct nshhdr_md1) > data_end){
		cb_debug("[ENCAP]: Bounds check failed.\n");
		return cb_retother(TC_ACT_SHOT);
	}

	oeth = data;
	nsh = (void*) oeth + sizeof(struct ethhdr);
	ieth = (void*) nsh + sizeof(struct nshhdr_md1);
	ip = (void*) ieth + sizeof(struct ethhdr);

	// Original Ethernet is outside, let's copy it to the inside
	__builtin_memcpy(ieth,oeth,sizeof(struct ethhdr));

	oeth->h_proto = bpf_ntohs(ETH_P_NSH);
	ieth->h_proto = prev_proto;
	// oeth->h_dest and oeth->h_src will be set by fwd stage

	// Re-add NSH
	__builtin_memcpy(nsh,prev_nsh,sizeof(struct nshhdr_md1));
	nsh->serv_path = bpf_htonl(sph);
	cb_debug("[ENCAP] Re-added NSH header!\n");

  /* Execute next program on TC, which will be Fwd stage */
  return cb_retok(TC_ACT_UNSPEC);
}

SEC("action/forward")
int sfc_forwarding(struct __sk_buff *skb)
{
  /* Start timestamps for statistics (if enabled) */
  cb_mark_init();

  void *data;
	void *data_end;
	struct ethhdr *eth;
	struct nshhdr_md1 *nsh;
	struct fwd_entry *next_hop;

	// We can only make these attributions here since
	// adjust_nsh() changes the packet, making previous
	// values of data and data_end invalid.
	// This should go away when we use the tail call
	data     = (void *)(long)skb->data;
	data_end = (void *)(long)skb->data_end;

	eth = data;

	if (data + sizeof(*eth) > data_end){
		cb_debug("[FORWD]: Bounds check failed.\n");
		return cb_retother(TC_ACT_SHOT);
	}

	nsh = (void*) eth + sizeof(*eth);

	if ((void*) nsh + sizeof(*nsh) > data_end){
		cb_debug("[FORWD]: Bounds check failed.\n");
		return cb_retother(TC_ACT_SHOT);
	}

	next_hop = bpf_map_lookup_elem(&fwd_table,&nsh->serv_path);
	if(likely(next_hop)){

		// Check if it is end of chain
    if(next_hop->flags & 0x1){
      cb_debug("[FORWD]: Second end of chain! This should not happen!!\n",
          bpf_ntohl(nsh->serv_path));

			return cb_retok(TC_ACT_SHOT);
		}else{
			cb_debug("[FORWD]: Updating next hop info\n");
			// Update MAC addresses
			if(set_src_mac(eth,EGRESS_MAC)) return cb_retother(BPF_DROP);
			__builtin_memmove(eth->h_dest,next_hop->address,ETH_ALEN);
		}
	}else{
		cb_debug("[FORWD]: No corresponding rule. SPH = 0x%x\n",
        bpf_ntohl(nsh->serv_path));

		// No corresponding rule. Drop the packet.
		return cb_retother(TC_ACT_SHOT);
	}

	cb_debug("[FORWD]: Successfully forwarded the pkt!\n");
	return cb_retok(TC_ACT_OK);
}

char _license[] SEC("license") = "GPL";
