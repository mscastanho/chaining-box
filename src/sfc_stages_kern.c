#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/pkt_cls.h>

#include <stdint.h>
#include <stdlib.h>

#include "bpf_endian.h"
#include "bpf_helpers.h"
#include "bpf_custom.h"
#include "bpf_elf.h"
#include "common.h"
#include "nsh.h"

#define ADJ_STAGE 1
#define FWD_STAGE 2

// Internal return codes
enum cb_ret {
    CB_OK,
    CB_END_CHAIN,
    CB_PASS,
    CB_DROP,
};

MAP(nsh_data, BPF_MAP_TYPE_HASH, sizeof(struct ip_5tuple),
    sizeof(struct nshhdr), 2048, PIN_GLOBAL_NS);

MAP(src_mac, BPF_MAP_TYPE_HASH, sizeof(__u8),
    ETH_ALEN, 1, PIN_GLOBAL_NS);

MAP(fwd_table, BPF_MAP_TYPE_HASH, sizeof(__u32),
    sizeof(struct fwd_entry), 2048, PIN_GLOBAL_NS);

// The pointer passed to this function must have been
// bounds checked already
static inline int set_src_mac(struct ethhdr *eth){
	void* smac;
	__u8 zero = 0;

	// Get src MAC from table. Is there a better way?
	smac = bpf_map_lookup_elem(&src_mac,&zero);
	if(smac == NULL){
		#ifdef DEBUG
		bpf_printk("[SET_SRC_MAC]: No source MAC configured\n");
		#endif /* DEBUG */
		return -1;
	}

	__builtin_memmove(eth->h_source,smac,ETH_ALEN);

	return 0;
}

SEC("xdp/decap")
int decap_nsh(struct xdp_md *ctx)
{
	void *data_end = (void *)(long)ctx->data_end;
	void *data = (void *)(long)ctx->data;
	struct ethhdr *eth;
	struct vlanhdr *vlan;
	struct nshhdr *nsh;
	struct iphdr *ip;
	struct ip_5tuple key = { }; // Verifier does not allow uninitialized keys
	int offset = 0;
    uint16_t h_proto = 0;
    void *smac;
	int ret = 0;
	__u8 zero = 0;
	eth = data;

	if(data + sizeof(struct ethhdr) > data_end)
		return XDP_PASS;

    offset += sizeof(struct ethhdr);

	smac = bpf_map_lookup_elem(&src_mac,&zero);
	if(smac == NULL){
		#ifdef DEBUG
		bpf_printk("[DECAP]: No source MAC configured\n");
		#endif /* DEBUG */

		return XDP_PASS;
	}

	char* mymac = (char*) smac;
	char* dstmac = (char*) data;

	// If broadcast or multicast, pass
    if(dstmac[0] & 1)
        return XDP_PASS;

	// Hack to compare MAC addresses
	// At the time of writing, __builtin_memcmp()
	// was not working.
    #pragma clang loop unroll(full)
	for(int i = 5 ; i >= 0 ; i--){
		if(mymac[i] ^ dstmac[i]){
			#ifdef DEBUG
			bpf_printk("[DECAP] Not for me. Dropping. (1/2)\n");
			bpf_printk("[DECAP] MAC: %02x %02x %02x", dstmac[5], dstmac[4], dstmac[3]);
			bpf_printk(" %02x %02x %02x\n (2/2)", dstmac[2], dstmac[1], dstmac[0]);
            #endif /* DEBUG */

			return XDP_DROP;
		}
	}

	#ifdef DEBUG
	bpf_printk("[DECAP] oeth->proto: 0x%x\n",bpf_ntohs(eth->h_proto));
	#endif /* DEBUG */

    // TODO: Handle other VLAN types
	if(bpf_ntohs(eth->h_proto) == ETH_P_8021Q){
        vlan = (void*)eth + sizeof(struct ethhdr);
        offset += sizeof(struct vlanhdr);

        if((void*)vlan + sizeof(struct vlanhdr) > data_end)
            return XDP_DROP;

        #ifdef DEBUG
        bpf_printk("[DECAP] vlan->proto: 0x%x\n",bpf_ntohs(vlan->h_vlan_encapsulated_proto));
        #endif /* DEBUG */

        h_proto = bpf_ntohs(vlan->h_vlan_encapsulated_proto); 
    }else{
        h_proto = bpf_ntohs(eth->h_proto);
    }

	if(h_proto != ETH_P_NSH)
		return XDP_PASS;

	nsh = data + offset;
    offset += sizeof(struct nshhdr);

	// Check if we can access NSH
	if ((void*) nsh + sizeof(struct nshhdr) > data_end)
		return XDP_DROP;

	#ifdef DEBUG
	bpf_printk("[DECAP] SPH: 0x%x\n",bpf_ntohl(nsh->serv_path));
	#endif /* DEBUG */

    // Check if next protocol is Ethernet
	if(nsh->next_proto != NSH_NEXT_PROTO_ETHER)
		return XDP_DROP;

	// Single check for Inner Ether + IP
	if((void*)nsh + sizeof(struct nshhdr) + sizeof(struct ethhdr) + sizeof(struct iphdr) > data_end)
		return XDP_DROP;

	// bpf_printk("[DECAP] \n");

	ip = (void*)nsh + sizeof(struct nshhdr) + sizeof(struct ethhdr);

	// Retrieve IP 5-tuple
	ret = get_tuple(ip,data_end,&key);
	if(ret){
		#ifdef DEBUG
		bpf_printk("[DECAP] get_tuple() failed.\n");
		#endif /* DEBUG */

		return XDP_DROP;
	}

	// Save NSH data
	ret = bpf_map_update_elem(&nsh_data, &key, nsh, BPF_ANY);
	if(ret == 0){
		#ifdef DEBUG
		bpf_printk("[DECAP] Stored NSH in table.\n");
		#endif /* DEBUG */
	}

	#ifdef DEBUG
	bpf_printk("[DECAP] Previous size: %d\n",data_end-data);
	#endif /* DEBUG */

	// Remove outer encapsulation
	bpf_xdp_adjust_head(ctx, (int)(offset));

	data_end = (void *)(long)ctx->data_end;
	data = (void *)(long)ctx->data;

	// eth = data;
	// if(eth + sizeof(*eth) > data_end){
	// 	bpf_printk("[DECAP] Weird error, dropping...\n");
	// 	return XDP_DROP;
	// }

	#ifdef DEBUG
	bpf_printk("[DECAP] Decapsulated packet; Size after: %d\n",data_end-data);
	#endif /* DEBUG */

	return XDP_PASS;
}

//SEC("adjust")
int adjust_nsh(struct __sk_buff *skb)
{
    void *data_end = (void *)(long)skb->data_end;
	void *data = (void *)(long)skb->data;
	int ret = 0;
	struct ip_5tuple key = { }; // Verifier does not allow uninitialized keys
	struct nshhdr *prev_nsh, *nsh;
	struct ethhdr *ieth, *oeth;
	struct iphdr *ip;
	__u16 prev_proto;
	__be32 sph, spi, si;
	struct fwd_entry *next_hop;


	// __u32 prev_len = data_end - data;
	// struct nshhdr nop2 = {0,0,0,0};

	if(data + sizeof(struct ethhdr) + sizeof(struct iphdr) > data_end){
		#ifdef DEBUG
		bpf_printk("[ADJUST]: Bounds check failed\n");
		#endif /* DEBUG */

		return CB_PASS;
	}
 
    // Check if frame contains IP. Pass along otherwise
	oeth = data;
    if(oeth->h_proto != bpf_htons(ETH_P_IP))
        return CB_PASS;

    ip = data + sizeof(struct ethhdr);

	ret = get_tuple(ip,data_end,&key);
	if (ret < 0) {
		#ifdef DEBUG
		bpf_printk("[ADJUST]: get_tuple() failed: %d\n", ret);
		#endif /* DEBUG */

		return CB_PASS;
	}

	// Check if we have a corresponding NSH saved
    prev_nsh = bpf_map_lookup_elem(&nsh_data,&key);
	if(prev_nsh == NULL){
		#ifdef DEBUG
		bpf_printk("[ADJUST] NSH header not found in table.\n");
		#endif /* DEBUG */
		// bpf_map_update_elem(&not_found, &key, &nop2, BPF_ANY);
		return CB_PASS;
	}

	#ifdef DEBUG
	bpf_printk("[ADJUST]: First look: SPH = 0x%x\n",bpf_ntohl(prev_nsh->serv_path));
	#endif /* DEBUG */

	sph = bpf_ntohl(prev_nsh->serv_path);
	spi = sph & 0xFFFFFF00;
	si = sph & 0x000000FF;
	#ifdef DEBUG
	bpf_printk("[ADJUST]: SPH = 0x%x ; SPI = 0x%x ; SI = 0x%x \n",sph,spi>>8,si);
	#endif /* DEBUG */

	if(si == 0){
		#ifdef DEBUG
		bpf_printk("[ADJUST]: Anomalous SI number. Dropping packet.\n");
		#endif /* DEBUG */
		return CB_DROP;
	}

	// Update the SI this way is safe because we
	// already checked if it is 0
	sph--;

	// Check if it is end of chain
	// If it is, we don't have to re-encapsulate it
	// TODO: This lookup is repeated in FWD stage.
	// 		 Ideally, this should only be done once
	#ifdef DEBUG
	bpf_printk("[ADJUST]: Pre-lookup: SPH = 0x%x\n",sph);
	#endif /* DEBUG */

	__u32 fkey = bpf_htonl(sph);
	next_hop = bpf_map_lookup_elem(&fwd_table,&fkey);
	if(likely(next_hop) && (next_hop->flags & 0x1)){
		#ifdef DEBUG
		bpf_printk("[ADJUST]: End of chain 1! SPH = 0x%x\n",sph);
		#endif /* DEBUG */
		return CB_END_CHAIN;
	}

	// Save previous EtherType before it is overwritten
	ieth = data;
	prev_proto = ieth->h_proto;

    // Add outer encapsulation
    ret = bpf_skb_adjust_room(skb,sizeof(struct ethhdr) + sizeof(struct nshhdr),BPF_ADJ_ROOM_MAC,0);

    if (ret < 0) {
		#ifdef DEBUG
		bpf_printk("[ADJUST]: Failed to add extra room: %d\n", ret);
		#endif /* DEBUG */

		return TC_ACT_SHOT;
    }

	data = (void *)(long)skb->data;
	data_end = (void *)(long)skb->data_end;

	// Bounds check to please the verifier
	if(data + 2*sizeof(struct ethhdr) + sizeof(struct nshhdr) > data_end){
		#ifdef DEBUG
		bpf_printk("[ADJUST]: Bounds check failed.\n");
		#endif /* DEBUG */
		return CB_DROP;
	}

	oeth = data;
	nsh = (void*) oeth + sizeof(struct ethhdr);
	ieth = (void*) nsh + sizeof(struct nshhdr);
	ip = (void*) ieth + sizeof(struct ethhdr);

	// Original Ethernet is outside, let's copy it to the inside
	__builtin_memcpy(ieth,oeth,sizeof(struct ethhdr));

	oeth->h_proto = bpf_ntohs(ETH_P_NSH);
	ieth->h_proto = prev_proto;
	// oeth->h_dest and oeth->h_src will be set by fwd stage

	// Re-add NSH
	__builtin_memcpy(nsh,prev_nsh,sizeof(struct nshhdr));
	nsh->serv_path = bpf_htonl(sph);
	#ifdef DEBUG
	bpf_printk("[ADJUST] Re-added NSH header!\n");
	#endif /* DEBUG */

	// bpf_tail_call(skb, &jmp_table, FWD_STAGE);

	// With the tail call, this is unreachable code.
	// It's here just to be safe.
	return CB_OK;

}

SEC("action/forward")
int sfc_forwarding(struct __sk_buff *skb)
{
	void *data;
	void *data_end;
	struct ethhdr *eth;
	struct nshhdr *nsh;
	struct fwd_entry *next_hop;
	int ret;
    int ready2send = 0;

	// TODO: The link between these two progs
	// 		 should be a tail call instead
	ret = adjust_nsh(skb);
    switch(ret){
        case CB_OK:
            break;                              // Continue execution
        case CB_DROP:
            return TC_ACT_SHOT;                 // Drop packet
        case CB_END_CHAIN:
            ready2send = 1;                     // Signal pkt should be sent
            break;                              // after MAC update
        case CB_PASS:
        default:
            return TC_ACT_OK;                   // Send packet rightaway
    }

	// We can only make these attributions here since
	// adjust_nsh() changes the packet, making previous
	// values of data and data_end invalid.
	// This should go away when we use the tail call
	data     = (void *)(long)skb->data;
	data_end = (void *)(long)skb->data_end;

	eth = data;

	if (data + sizeof(*eth) > data_end){
		#ifdef DEBUG
		bpf_printk("[FORWARD]: Bounds check failed.\n");
		#endif /* DEBUG */
		return TC_ACT_SHOT;
	}

    // Adjust source MAC and send
    if(ready2send){
        if(set_src_mac(eth))
            return TC_ACT_SHOT;

        return TC_ACT_OK;
    }

	// Keep regular traffic working
	//if (eth->h_proto != bpf_htons(ETH_P_NSH)){
	//	#ifdef DEBUG
	//	bpf_printk("[FORWARD]: Not NSH. Passing along...\n");
	//	#endif /* DEBUG */
    //    return TC_ACT_OK;
    //}

	nsh = (void*) eth + sizeof(*eth);

	if ((void*) nsh + sizeof(*nsh) > data_end){
		#ifdef DEBUG
		bpf_printk("[FORWARD]: Bounds check failed.\n");
		#endif /* DEBUG */
		return TC_ACT_SHOT;
	}

	// bpf_printk("[FORWARD] SPH = 0x%x\n",sph);
	next_hop = bpf_map_lookup_elem(&fwd_table,&nsh->serv_path);
	if(likely(next_hop)){

		// Check if is end of chain
		if(next_hop->flags & 0x1){
			// bpf_printk("[FORWARD] Size before: %d ; EtherType = 0x%x\n",data_end-data,bpf_ntohs(eth->h_proto));
			#ifdef DEBUG
			__u32 sph = bpf_ntohl(nsh->serv_path);
			bpf_printk("[FORWARD]: End of chain 2! SPH = 0x%x\n",sph);
			#endif /* DEBUG */

			// #pragma clang loop unroll(full)
			// for(int i = 0 ; i < 8 ; i++){
			// 	ret = bpf_skb_vlan_pop(skb);
			// 	if (ret < 0) {
			// 		bpf_printk("[ADJUST]: Failed to add extra room: %d\n", ret);
			// 		return BPF_DROP;
			// 	}
			// }

			// data     = (void *)(long)skb->data;
			// data_end = (void *)(long)skb->data_end;
			// eth = data;

			// if(eth+sizeof(*eth) > data_end)
			// 	return BPF_DROP;

			// bpf_printk("[FORWARD] Size after: %d ; EtherType = 0x%x\n",data_end-data,bpf_ntohs(eth->h_proto));
			return TC_ACT_OK; //TODO: Change this
			// Remove external encapsulation
		}else{
			#ifdef DEBUG
			bpf_printk("[FORWARD]: Updating next hop info\n");
			#endif /* DEBUG */
			// Update MAC addresses
			if(set_src_mac(eth)) return BPF_DROP;
			__builtin_memmove(eth->h_dest,next_hop->address,ETH_ALEN);
		}
	}else{
		#ifdef DEBUG
		__u32 sph = bpf_ntohl(nsh->serv_path);
		bpf_printk("[FORWARD]: No corresponding rule. SPH = 0x%x\n",sph);
		#endif /* DEBUG */
		// No corresponding rule. Drop the packet.
		return TC_ACT_SHOT;
	}

	#ifdef DEBUG
	bpf_printk("[FORWARD]: Successfully forwarded the pkt!\n");
	#endif /* DEBUG */
	return TC_ACT_OK;
}

char _license[] SEC("license") = "GPL";
