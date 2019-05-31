#include <linux/bpf.h>
#include <linux/if_ether.h>
// #include <linux/if_vlan.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/pkt_cls.h>

#include <stdint.h>
#include <stdlib.h>

#include "bpf_endian.h"
#include "bpf_helpers.h"
#include "bpf_elf.h"
#include "common.h"
#include "nsh.h"

#define ADJ_STAGE 1
#define FWD_STAGE 2

#define EXTRA_BYTES 6

#define BPF_END_CHAIN 100
#define VLAN_TCI 0x000A

struct bpf_map_def SEC("maps") cls_table = {
	.type = BPF_MAP_TYPE_HASH,
	.key_size = sizeof(struct ip_5tuple),
	.value_size = sizeof(struct cls_entry), // Value is sph + MAC address
	.max_entries = 2048,
	//.pinning = PIN_GLOBAL_NS,
};

struct bpf_map_def SEC("maps") nsh_data = {
	.type = BPF_MAP_TYPE_HASH,
	.key_size = sizeof(struct ip_5tuple),
	.value_size = sizeof(struct nshhdr),
	.max_entries = 2048,
	//.pinning = PIN_GLOBAL_NS,
};

struct bpf_map_def SEC("maps") src_mac = {
	.type = BPF_MAP_TYPE_HASH,
	.key_size = sizeof(__u8),
	.value_size = ETH_ALEN,
	.max_entries = 1,
	//.pinning = PIN_GLOBAL_NS,
};

// struct bpf_map_def SEC("maps") not_found = {
// 	.type = BPF_MAP_TYPE_HASH,
// 	.key_size = sizeof(struct ip_5tuple),
// 	.value_size = sizeof(struct nshhdr),
// 	.max_entries = 2048,
// 	//.pinning = PIN_GLOBAL_NS,
// };

struct bpf_map_def SEC("maps") fwd_table = {
	.type = BPF_MAP_TYPE_HASH,
	.key_size = sizeof(__u32), // Key is SPH (SPI + SI) -> 4 Bytes
	.value_size = sizeof(struct fwd_entry), // Value is MAC address + end byte
	.max_entries = 2048,
	//.pinning = PIN_GLOBAL_NS,
};

// struct bpf_map_def SEC("maps") jmp_table = {
// 	.type = BPF_MAP_TYPE_PROG_ARRAY,
// 	.key_size = sizeof(u32),
// 	.value_size = sizeof(u32),
// 	.max_entries = 2,
// };

// The pointer passed to this function must have been
// bounds checked already
static inline int set_src_mac(struct ethhdr *eth){
	void* smac;
	__u8 zero = 0;

	// Get src MAC from table. Is there a better way?
	smac = bpf_map_lookup_elem(&src_mac,&zero);
	if(smac == NULL){
		#ifdef DEBUG
		printk("[FORWARD]: No source MAC configured\n");
		#endif /* DEBUG */
		return -1;
	}

	__builtin_memmove(eth->h_source,smac,ETH_ALEN);

	return 0;
}

static inline int get_tuple(void* ip_data, void* data_end, struct ip_5tuple *t){
	struct iphdr *ip;
	struct udphdr *udp;
	struct tcphdr *tcp;
	__u64 *init;

	ip = ip_data;
	if((void*) ip + sizeof(*ip) > data_end){
		#ifdef DEBUG
		printk("get_tuple(): Error accessing IP hdr\n");
		#endif /* DEBUG */

		return -1;
	}

	init = (__u64*) ip_data;

	t->ip_src = ip->saddr;
	t->ip_dst = ip->daddr;
	t->proto = ip->protocol;

	switch(ip->protocol){
		case IPPROTO_UDP:
			udp = ip_data + sizeof(*ip);
			if((void*) udp + sizeof(*udp) > data_end){
				#ifdef DEBUG
				printk("get_tuple(): Error accessing UDP hdr\n");
				#endif /* DEBUG */

				return -1;
			}

			t->dport = udp->dest;
			t->sport = udp->source;
			break;
		case IPPROTO_TCP:
			tcp = ip_data + sizeof(*ip);
			if((void*) tcp + sizeof(*tcp) > data_end){
				#ifdef DEBUG
				printk("get_tuple(): Error accessing TCP hdr\n");
				#endif /* DEBUG */
				

				return -1;
			}

			t->dport = tcp->dest;
			t->sport = tcp->source;
			break;
		case IPPROTO_ICMP:
			t->dport = 0;
			t->sport = 0;
			break;
		default:
			return -1;
	}

	return 0;
};

SEC("xdp/classify")
int classify_pkt(struct xdp_md *ctx)
{
	void *data_end = (void *)(long)ctx->data_end;
	void *data = (void *)(long)ctx->data;
	struct nshhdr *nsh;
	struct cls_entry *cls;
	struct ethhdr *eth;
	struct vlanhdr *vlan;
	struct iphdr *ip;
	void* extra_bytes;
	struct ip_5tuple key = { };
	int ret;

	if(data + sizeof(struct ethhdr) + sizeof(struct iphdr) > data_end){
		#ifdef DEBUG
		printk("[CLASSIFY] Bounds check #1 failed.\n");
		#endif /* DEBUG */

		return XDP_DROP;
	}

	eth = data;
	ip = (void*)eth + sizeof(struct ethhdr);

	if(bpf_ntohs(eth->h_proto) != ETH_P_IP){
		// printk("[CLASSIFY] Not an IPv4 packet, dropping.\n");
		return XDP_DROP;
	}

	ret = get_tuple(ip,data_end,&key);
	if (ret < 0){
		#ifdef DEBUG
		printk("[CLASSIFY] get_tuple() failed: %d\n",ret);
		#endif /* DEBUG */

		return XDP_DROP;
	}

	cls = bpf_map_lookup_elem(&cls_table,&key);
	if(cls == NULL){
		#ifdef DEBUG
		printk("[CLASSIFY] No rule for packet.\n");
		#endif /* DEBUG */

		return XDP_DROP;
	}

	#ifdef DEBUG
	printk("[CLASSIFY] Matched packet flow.\n");
	#endif /* DEBUG */

	// Insert outer Ethernet + NSH headers + EXTRA_BYTES
	// Extra bytes needed by the hack on encapsulation. See
	// ajust_nsh() below
	if(bpf_xdp_adjust_head(ctx, 0 - (int)(sizeof(struct ethhdr) + sizeof(struct vlanhdr) + sizeof(struct nshhdr) + EXTRA_BYTES))){
		#ifdef DEBUG
		printk("[CLASSIFY] Error creating room in packet.\n");
		#endif /* DEBUG */

		return XDP_DROP;
	}

	// Pointers have changed after call to bpf_xdp_adjust_head()
	data_end = (void *)(long)ctx->data_end;
	data = (void *)(long)ctx->data;

	if(data + sizeof(struct ethhdr) + sizeof(struct vlanhdr) + sizeof(struct nshhdr) + EXTRA_BYTES > data_end){
		#ifdef DEBUG
		printk("[CLASSIFY] Bounds check #2 failed.\n");
		#endif /* DEBUG */

		return XDP_DROP;
	}

	eth = data; // Points to the new outermost Ethernet header
	vlan = (void*) eth + sizeof(struct ethhdr);
	nsh = (void*)vlan + sizeof(struct vlanhdr);
	extra_bytes = (void*)nsh + sizeof(struct nshhdr);

	if(set_src_mac(eth)) return BPF_DROP;
	__builtin_memmove(eth->h_dest,cls->next_hop,ETH_ALEN);
	eth->h_proto = bpf_htons(ETH_P_8021Q);

	vlan->h_vlan_encapsulated_proto = bpf_htons(ETH_P_NSH);
	vlan->h_vlan_TCI = bpf_htons(VLAN_TCI);

	nsh->basic_info = ((uint16_t) 0) 		|
						NSH_TTL_DEFAULT 	|
						NSH_BASE_LENGHT_MD_TYPE_2;
	nsh->md_type 	= NSH_MD_TYPE_2;
	nsh->next_proto = NSH_NEXT_PROTO_ETHER;
	nsh->serv_path 	= bpf_htonl(cls->sph);

	// Set extra bytes to 0
	// To understand why these are needed, check adjust()
	__builtin_memset(extra_bytes,0,EXTRA_BYTES);

	#ifdef DEBUG
	printk("[CLASSIFY] NSH added.\n");
	#endif /* DEBUG */

	// TODO: This should be bpf_redirect_map(), to allow
	// redirecting the packet to another interface
	return XDP_TX;
}

// SEC("classify_tc_tx")
// int classify_tc(struct __sk_buff *skb)
// {
// 	void *data_end = (void *)(long)skb->data_end;
// 	void *data = (void *)(long)skb->data;
// 	struct nshhdr *nsh;
// 	struct cls_entry *cls;
// 	struct ethhdr *eth,*ieth,*oeth;
// 	// struct vlanhdr *vlan;
// 	struct iphdr *ip;
// 	void* extra_bytes;
// 	struct ip_5tuple key = { };
// 	int ret;
// 	__u16 prev_proto;

// 	if(data + sizeof(struct ethhdr) + sizeof(struct iphdr) > data_end){
// 		#ifdef DEBUG
// 		printk("[CLASSIFY] Bounds check #1 failed.\n");
// 		#endif /* DEBUG */

// 		return XDP_DROP;
// 	}

// 	eth = data;
// 	ip = (void*)eth + sizeof(struct ethhdr);

// 	if(bpf_ntohs(eth->h_proto) != ETH_P_IP){
// 		#ifdef DEBUG
// 		printk("[CLASSIFY] Not an IPv4 packet, passing along.\n");
// 		#endif /* DEBUG */

// 		return TC_ACT_OK;
// 	}

// 	ret = get_tuple(ip,data_end,&key);
// 	if (ret < 0){
// 		#ifdef DEBUG
// 		printk("[CLASSIFY] get_tuple() failed: %d\n",ret);
// 		#endif /* DEBUG */

// 		return TC_ACT_SHOT;
// 	}

// 	cls = bpf_map_lookup_elem(&cls_table,&key);
// 	if(cls == NULL){
// 		#ifdef DEBUG
// 		printk("[CLASSIFY] No rule for packet.\n");
// 		#endif /* DEBUG */

// 		return TC_ACT_OK;
// 	}

// 	#ifdef DEBUG
// 	printk("[CLASSIFY] Matched packet flow.\n");
// 	#endif /* DEBUG */

// 	// Save previous proto
// 	ieth = data;
// 	prev_proto = ieth->h_proto;

// 	#ifdef DEBUG
// 	printk("[CLASS-TC] Size before: %d\n",data_end-data);
// 	#endif /* DEBUG */

// 	// Hack to add space for NSH + Outer Ethernet
// 	// The only way we found to add bytes to packet
// 	// was using vlan_push. Not at all ideal.
// 	// vlan_push adds 4 bytes (802.1q header)
// 	// We need to add 26 bytes, which is not possible using
// 	// vlan tags. Ideally we would add 28 bytes, but apparently
// 	// XDP is dropping packets if the new addition is not multiple
// 	// of 8 bytes. So we'll add 8 tags (32 bytes) and try to
// 	// deal with the extra 6 bytes.
//  	#pragma clang loop unroll(full)
// 	for(int i = 0 ; i < 8 ; i++){
// 		ret = bpf_skb_vlan_push(skb,bpf_ntohs(ETH_P_8021Q),VLAN_TCI);
// 		if (ret < 0) {
// 			#ifdef DEBUG
// 			printk("[ADJUST]: Failed to add extra room: %d\n", ret);
// 			#endif /* DEBUG */

// 			return BPF_DROP;
// 		}
// 	}

// 	data = (void *)(long)skb->data;
// 	data_end = (void *)(long)skb->data_end;

// 	#ifdef DEBUG
// 	printk("[CLASS-TC] Size after: %d\n",data_end-data);
// 	#endif /* DEBUG */

// 	// Bounds check to please the verifier
// 	// EXTRA_BYTES is due to the hack used above to enable encapsulation
// 	if(data + 2*sizeof(struct ethhdr) + sizeof(struct nshhdr) + EXTRA_BYTES > data_end){
// 		#ifdef DEBUG
// 		printk("[CLASSIFY] Bounds check #2 failed.\n");
// 		#endif /* DEBUG */
// 		return TC_ACT_SHOT;
// 	}

// 	oeth = data;
// 	// vlan = (void*) oeth + sizeof(struct ethhdr);
// 	nsh = (void*) oeth + sizeof(struct ethhdr);
// 	extra_bytes = (void*) nsh + sizeof(struct nshhdr);
// 	ieth = (void*) extra_bytes + EXTRA_BYTES;
// 	ip = (void*) ieth + sizeof(struct ethhdr);

// 	// Original Ethernet is outside, let's copy it to the inside
// 	__builtin_memcpy(ieth,oeth,sizeof(struct ethhdr));
// 	// __builtin_memset(oeth,0,2*sizeof(struct ethhdr) + sizeof(struct nshhdr) + EXTRA_BYTES);
// 	// __builtin_memset(oeth,0,sizeof(struct ethhdr));

// 	oeth->h_proto = bpf_ntohs(ETH_P_NSH);
// 	ieth->h_proto = prev_proto;
// 	// oeth->h_dest and oeth->h_src will be set by fwd stage

// 	if(set_src_mac(oeth)) return TC_ACT_SHOT;
// 	__builtin_memmove(oeth->h_dest,cls->next_hop,ETH_ALEN);

// 	// oeth->h_proto = bpf_htons(ETH_P_8021Q);
// 	// vlan->h_vlan_encapsulated_proto = bpf_htons(ETH_P_NSH);
// 	// vlan->h_vlan_TCI = bpf_htons(VLAN_TCI);

// 	nsh->basic_info = ((uint16_t) 0) 		|
// 						NSH_TTL_DEFAULT 	|
// 						NSH_BASE_LENGHT_MD_TYPE_2;
// 	nsh->md_type 	= NSH_MD_TYPE_2;
// 	nsh->next_proto = NSH_NEXT_PROTO_ETHER;
// 	nsh->serv_path 	= bpf_htonl(cls->sph);

// 	// Set extra bytes to 0
// 	// To understand why these are needed, check adjust()
// 	__builtin_memset(extra_bytes,0,EXTRA_BYTES);

// 	#ifdef DEBUG
// 	printk("[CLASSIFY] NSH added.\n");
// 	#endif /* DEBUG */

// 	// TODO: This should be bpf_redirect_map(), to allow
// 	// redirecting the packet to another interface
// 	return TC_ACT_OK;
// }

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
	void *smac;
	int ret = 0;
	__u8 zero = 0;
	eth = data;

	if(data + sizeof(struct ethhdr) > data_end)
		return XDP_DROP;

	smac = bpf_map_lookup_elem(&src_mac,&zero);
	if(smac == NULL){
		#ifdef DEBUG
		printk("[DECAP]: No source MAC configured\n");
		#endif /* DEBUG */

		return XDP_DROP;
	}

	// Hack to compare MAC address
	// At the time of writing, __builtin_memcmp()
	// was not working.
	char* mymac = (char*) smac;
	char* dstmac = (char*) data;

	// TODO: If broadcast, pass
	#pragma clang loop unroll(full)
	for(int i = 5 ; i >= 0 ; i--){
		if(mymac[i] ^ dstmac[i]){
			#ifdef DEBUG
			printk("[DECAP] Not for me. Dropping.\n");
			#endif /* DEBUG */

			return XDP_DROP;
		}
	}

	#ifdef DEBUG
	printk("[DECAP] oeth->proto: 0x%x\n",bpf_ntohs(eth->h_proto));
	#endif /* DEBUG */

	if(bpf_ntohs(eth->h_proto) != ETH_P_8021Q)
		return XDP_PASS;

	vlan = (void*)eth + sizeof(struct ethhdr);

	if((void*)vlan + sizeof(struct vlanhdr) > data_end)
		return XDP_DROP;

	#ifdef DEBUG
	printk("[DECAP] vlan->proto: 0x%x\n",bpf_ntohs(vlan->h_vlan_encapsulated_proto));
	#endif /* DEBUG */

	if(bpf_ntohs(vlan->h_vlan_encapsulated_proto) != ETH_P_NSH)
		return XDP_PASS;

	nsh = (void*)vlan + sizeof(struct vlanhdr);

	// Check if we can access NSH
	if ((void*) nsh + sizeof(struct nshhdr) > data_end)
		return XDP_DROP;

	#ifdef DEBUG
	printk("[DECAP] SPH: 0x%x\n",bpf_ntohl(nsh->serv_path));
	#endif /* DEBUG */
	// Check if next protocol is Ethernet
	if(nsh->next_proto != NSH_NEXT_PROTO_ETHER)
		return XDP_DROP;

	// Single check for Inner Ether + IP
	if((void*)nsh + sizeof(struct nshhdr) + EXTRA_BYTES + sizeof(struct ethhdr) + sizeof(struct iphdr) > data_end)
		return XDP_DROP;

	// printk("[DECAP] \n");

	ip = (void*)nsh + sizeof(struct nshhdr) + EXTRA_BYTES + sizeof(struct ethhdr);

	// Retrieve IP 5-tuple
	ret = get_tuple(ip,data_end,&key);
	if(ret){
		#ifdef DEBUG
		printk("[DECAP] get_tuple() failed.\n");
		#endif /* DEBUG */

		return XDP_DROP;
	}

	// Save NSH data
	ret = bpf_map_update_elem(&nsh_data, &key, nsh, BPF_ANY);
	if(ret == 0){
		#ifdef DEBUG
		printk("[DECAP] Stored NSH in table.\n");
		#endif /* DEBUG */
	}

	#ifdef DEBUG
	printk("[DECAP] Previous size: %d\n",data_end-data);
	#endif /* DEBUG */

	// Remove outer Ethernet + NSH headers
	bpf_xdp_adjust_head(ctx, (int)(sizeof(struct ethhdr) + sizeof(struct vlanhdr) + sizeof(struct nshhdr) + EXTRA_BYTES));

	data_end = (void *)(long)ctx->data_end;
	data = (void *)(long)ctx->data;

	// eth = data;
	// if(eth + sizeof(*eth) > data_end){
	// 	printk("[DECAP] Weird error, dropping...\n");
	// 	return XDP_DROP;
	// }

	#ifdef DEBUG
	printk("[DECAP] Decapsulated packet; Size after: %d\n",data_end-data);
	#endif /* DEBUG */

	return XDP_PASS;
}

// SEC("adjust")
// int adjust_nsh(struct __sk_buff *skb)
// {
//     void *data_end = (void *)(long)skb->data_end;
// 	void *data = (void *)(long)skb->data;
// 	int ret = 0;
// 	struct ip_5tuple key = { }; // Verifier does not allow uninitialized keys
// 	struct nshhdr *prev_nsh, *nsh;
// 	struct ethhdr *ieth, *oeth;
// 	struct iphdr *ip;
// 	__u16 prev_proto;
// 	__be32 sph, spi, si;
// 	struct fwd_entry *next_hop;


// 	// __u32 prev_len = data_end - data;
// 	// struct nshhdr nop2 = {0,0,0,0};

// 	if(data + sizeof(struct ethhdr) + sizeof(struct iphdr) > data_end){
// 		#ifdef DEBUG
// 		printk("[ADJUST]: Bounds check failed\n");
// 		#endif /* DEBUG */

// 		return TC_ACT_SHOT;
// 	}

// 	// TODO: Check EtherType indeed corresponds to IP
// 	ip = data + sizeof(struct ethhdr);

// 	ret = get_tuple(ip,data_end,&key);
// 	if (ret < 0) {
// 		#ifdef DEBUG
// 		printk("[ADJUST]: get_tuple() failed: %d\n", ret);
// 		#endif /* DEBUG */

// 		return TC_ACT_SHOT;
// 	}

// 	// Check if we have a correponding NSH saved
//     prev_nsh = bpf_map_lookup_elem(&nsh_data,&key);
// 	if(prev_nsh == NULL){
// 		#ifdef DEBUG
// 		printk("[ADJUST] NSH header not found in table.\n");
// 		#endif /* DEBUG */
// 		// bpf_map_update_elem(&not_found, &key, &nop2, BPF_ANY);
// 		return TC_ACT_SHOT;
// 	}

// 	#ifdef DEBUG
// 	printk("[ADJUST]: First look: SPH = 0x%x\n",bpf_ntohl(prev_nsh->serv_path));
// 	#endif /* DEBUG */

// 	sph = bpf_ntohl(prev_nsh->serv_path);
// 	spi = sph & 0xFFFFFF00;
// 	si = sph & 0x000000FF;
// 	#ifdef DEBUG
// 	printk("[ADJUST]: SPH = 0x%x ; SPI = 0x%x ; SI = 0x%x \n",sph,spi>>8,si);
// 	#endif /* DEBUG */

// 	if(si == 0){
// 		#ifdef DEBUG
// 		printk("[ADJUST]: Anomalous SI number. Dropping packet.\n");
// 		#endif /* DEBUG */
// 		return TC_ACT_SHOT;
// 	}

// 	// Update the SI this way is safe because we
// 	// already checked if it is 0
// 	sph--;

// 	// Check if it is end of chain
// 	// If it is, we don't have to re-encapsulate it
// 	// TODO: This lookup is repeated in FWD stage.
// 	// 		 Ideally, this should only be done once
// 	#ifdef DEBUG
// 	printk("[ADJUST]: Pre-lookup: SPH = 0x%x\n",sph);
// 	#endif /* DEBUG */

// 	__u32 fkey = bpf_htonl(sph);
// 	next_hop = bpf_map_lookup_elem(&fwd_table,&fkey);
// 	if(likely(next_hop) && (next_hop->flags & 0x1)){
// 		#ifdef DEBUG
// 		printk("[ADJUST]: End of chain 1! SPH = 0x%x\n",sph);
// 		#endif /* DEBUG */
// 		return BPF_END_CHAIN;
// 	}

// 	// Save previous EtherType before it is overwritten
// 	ieth = data;
// 	prev_proto = ieth->h_proto;

// 	// // printk("[ADJUST]: skb_max_len = %x\n", skb->dev->mtu + skb->dev->hard_header_len);
// 	// if(ieth+1 > data_end){
// 	// 	return BPF_DROP;
// 	// }

// 	// __be16 proto = skb->protocol;
// 	// printk("[ADJUST]: skb->protocol = 0x%x ; eth->h_proto = 0x%x ; ETH_P_IP = 0x%x\n", proto,bpf_htons(ieth->h_proto),bpf_htons(ETH_P_IP));

// 	// Hack to add space for NSH + Outer Ethernet
// 	// The only way we found to add bytes to packet
// 	// was using vlan_push. Not at all ideal.
// 	// vlan_push adds 4 bytes (802.1q header)
// 	// We need to add 26 bytes, which is not possible using
// 	// vlan tags. Ideally we would add 28 bytes, but apparently
// 	// XDP is dropping packets if the new addition is not multiple
// 	// of 8 bytes. So we'll add 8 tags (32 bytes) and try to
// 	// deal with the extra 6 bytes.
//  	#pragma clang loop unroll(full)
// 	for(int i = 0 ; i < 8 ; i++){
// 		ret = bpf_skb_vlan_push(skb,bpf_ntohs(ETH_P_8021Q),VLAN_TCI);
// 		if (ret < 0) {
// 			#ifdef DEBUG
// 			printk("[ADJUST]: Failed to add extra room: %d\n", ret);
// 			#endif /* DEBUG */
// 			return TC_ACT_SHOT;
// 		}
// 	}

// 	data = (void *)(long)skb->data;
// 	data_end = (void *)(long)skb->data_end;

// 	// Bounds check to please the verifier
// 	// EXTRA_BYTES is due to the hack used above to enable encapsulation
// 	if(data + 2*sizeof(struct ethhdr) + sizeof(struct nshhdr) + EXTRA_BYTES > data_end){
// 		#ifdef DEBUG
// 		printk("[ADJUST]: Bounds check failed.\n");
// 		#endif /* DEBUG */
// 		return TC_ACT_SHOT;
// 	}

// 	oeth = data;
// 	nsh = (void*) oeth + sizeof(struct ethhdr);
// 	ieth = (void*) nsh + sizeof(struct nshhdr) + EXTRA_BYTES;
// 	ip = (void*) ieth + sizeof(struct ethhdr);

// 	// Original Ethernet is outside, let's copy it to the inside
// 	__builtin_memcpy(ieth,oeth,sizeof(struct ethhdr));
// 	// __builtin_memset(oeth,0,2*sizeof(struct ethhdr) + sizeof(struct nshhdr) + EXTRA_BYTES);
// 	// __builtin_memset(oeth,0,sizeof(struct ethhdr));

// 	oeth->h_proto = bpf_ntohs(ETH_P_NSH);
// 	ieth->h_proto = prev_proto;
// 	// oeth->h_dest and oeth->h_src will be set by fwd stage

// 	// Re-add NSH
// 	__builtin_memcpy(nsh,prev_nsh,sizeof(struct nshhdr));
// 	nsh->serv_path = bpf_htonl(sph);
// 	#ifdef DEBUG
// 	printk("[ADJUST] Re-added NSH header!\n");
// 	#endif /* DEBUG */

// 	// bpf_tail_call(skb, &jmp_table, FWD_STAGE);

// 	// With the tail call, this is unreachable code.
// 	// It's here just to be safe.
// 	return TC_ACT_OK;

// }

SEC("action/forward")
int sfc_forwarding(struct __sk_buff *skb)
{
	void *data;
	void *data_end;
	struct ethhdr *eth;
	struct nshhdr *nsh;
	struct fwd_entry *next_hop;
	// int ret;

	// TODO: The link between these two progs
	// 		 should be a tail call instead
	// ret = adjust_nsh(skb);
	// if(ret == TC_ACT_SHOT){
	// 	return TC_ACT_SHOT;
	// }else if(ret == BPF_END_CHAIN){
	// 	// Nothing else to do, just send to network
	// 	return TC_ACT_OK;
	// }

	// We can only make these attributions here since
	// adjust_nsh() changes the packet, making previous
	// values of data and data_end invalid.
	// This should go away when we use the tail call
	data     = (void *)(long)skb->data;
	data_end = (void *)(long)skb->data_end;

	eth = data;

	if (data + sizeof(*eth) > data_end){
		#ifdef DEBUG
		printk("[FORWARD]: Bounds check failed.\n");
		#endif /* DEBUG */
		return TC_ACT_SHOT;
	}

	// Keep regular traffic working
	// if (eth->h_proto != bpf_htons(ETH_P_NSH))
	// 	return TC_ACT_OK;

	nsh = (void*) eth + sizeof(*eth);

	if ((void*) nsh + sizeof(*nsh) > data_end){
		#ifdef DEBUG
		printk("[FORWARD]: Bounds check failed.\n");
		#endif /* DEBUG */
		return TC_ACT_SHOT;
	}

	// printk("[FORWARD] SPH = 0x%x\n",sph);
	next_hop = bpf_map_lookup_elem(&fwd_table,&nsh->serv_path);
	if(likely(next_hop)){

		// Check if is end of chain
		if(next_hop->flags & 0x1){
			// printk("[FORWARD] Size before: %d ; EtherType = 0x%x\n",data_end-data,bpf_ntohs(eth->h_proto));
			#ifdef DEBUG
			__u32 sph = bpf_ntohl(nsh->serv_path);
			printk("[FORWARD]: End of chain 2! SPH = 0x%x\n",sph);
			#endif /* DEBUG */

			// #pragma clang loop unroll(full)
			// for(int i = 0 ; i < 8 ; i++){
			// 	ret = bpf_skb_vlan_pop(skb);
			// 	if (ret < 0) {
			// 		printk("[ADJUST]: Failed to add extra room: %d\n", ret);
			// 		return BPF_DROP;
			// 	}
			// }

			// data     = (void *)(long)skb->data;
			// data_end = (void *)(long)skb->data_end;
			// eth = data;

			// if(eth+sizeof(*eth) > data_end)
			// 	return BPF_DROP;

			// printk("[FORWARD] Size after: %d ; EtherType = 0x%x\n",data_end-data,bpf_ntohs(eth->h_proto));
			return TC_ACT_OK; //TODO: Change this
			// Remove external encapsulation
		}else{
			#ifdef DEBUG
			printk("[FORWARD]: Updating next hop info\n");
			#endif /* DEBUG */
			// Update MAC addresses
			if(set_src_mac(eth)) return BPF_DROP;
			__builtin_memmove(eth->h_dest,next_hop->address,ETH_ALEN);
		}
	}else{
		#ifdef DEBUG
		__u32 sph = bpf_ntohl(nsh->serv_path);
		printk("[FORWARD]: No corresponding rule. SPH = 0x%x\n",sph);
		#endif /* DEBUG */
		// No corresponding rule. Drop the packet.
		return TC_ACT_SHOT;
	}

	#ifdef DEBUG
	printk("[FORWARD]: Successfully forwarded the pkt!\n");
	#endif /* DEBUG */
	return TC_ACT_OK;
}

char _license[] SEC("license") = "GPL";
