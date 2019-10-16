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
#include "cb_helpers.h"
#include "bpf_helpers.h"
#include "bpf_elf.h"
#include "cb_common.h"
#include "nsh.h"

MAP(cls_table, BPF_MAP_TYPE_HASH, sizeof(struct ip_5tuple),
    sizeof(struct cls_entry), 2048, PIN_GLOBAL_NS);

MAP(src_mac, BPF_MAP_TYPE_HASH, sizeof(__u8),
    ETH_ALEN, 1, PIN_GLOBAL_NS);

#ifdef ENABLE_STATS
STATSMAP();
#endif /* ENABLE_STATS */

static inline int set_src_mac(struct ethhdr *eth){
	void* smac;
	__u8 zero = 0;

	// Get src MAC from table. Is there a better way?
	smac = bpf_map_lookup_elem(&src_mac,&zero);
	if(smac == NULL){
		cb_debug("[FORWARD]: No source MAC configured\n");
		return -1;
	}

	__builtin_memmove(eth->h_source,smac,ETH_ALEN);
	return 0;
}

SEC("action/classify")
int classify_tc(struct __sk_buff *skb)
{
  /* Start timestamps for statistics (if enabled) */
  cb_mark_init();

	void *data_end = (void *)(long)skb->data_end;
	void *data = (void *)(long)skb->data;
	struct nshhdr *nsh;
	struct cls_entry *cls;
	struct ethhdr *eth,*ieth,*oeth;
	struct iphdr *ip;
	void* extra_bytes;
	struct ip_5tuple key = { };
	int ret;
	__u16 prev_proto;

	if(data + sizeof(struct ethhdr) + sizeof(struct iphdr) > data_end){
        cb_debug("[CLASSIFY] Bounds check #1 failed.\n");
		return cb_retother(TC_ACT_OK);
	}

	eth = data;
	ip = (void*)eth + sizeof(struct ethhdr);

	if(bpf_ntohs(eth->h_proto) != ETH_P_IP){
        cb_debug("[CLASSIFY] Not an IPv4 packet, passing along.\n");
		return cb_retother(TC_ACT_OK);
	}

	ret = get_tuple(ip,data_end,&key);
	if (ret < 0){
        cb_debug("[CLASSIFY] get_tuple() failed: %d\n",ret);
		return cb_retother(TC_ACT_OK);
	}

	cls = bpf_map_lookup_elem(&cls_table,&key);
	if(cls == NULL){
        cb_debug("[CLASSIFY] No rule for packet.\n");
		return cb_retother(TC_ACT_OK);
	}

    cb_debug("[CLASSIFY] Matched packet flow.\n");

	// Save previous proto
	ieth = data;
	prev_proto = ieth->h_proto;

    cb_debug("[CLASS-TC] Size before: %d\n",data_end-data);

    // Add outer encapsulation
    ret = bpf_skb_adjust_room(skb,sizeof(struct ethhdr) + sizeof(struct nshhdr),BPF_ADJ_ROOM_MAC,0);

    if (ret < 0) {
        cb_debug("[CLASSIFY]: Failed to add extra room: %d\n", ret);
        return cb_retother(TC_ACT_SHOT);
    }
    
    data = (void *)(long)skb->data;
	data_end = (void *)(long)skb->data_end;

    cb_debug("[CLASS-TC] Size after: %d\n",data_end-data);

	// Bounds check to please the verifier
    if(data + 2*sizeof(struct ethhdr) + sizeof(struct nshhdr) > data_end){
        cb_debug("[CLASSIFY] Bounds check #2 failed.\n");
		return cb_retother(TC_ACT_OK);
	}

	oeth = data;
	nsh = (void*) oeth + sizeof(struct ethhdr);
	extra_bytes = (void*) nsh + sizeof(struct nshhdr);
	ieth = (void*) extra_bytes;
	ip = (void*) ieth + sizeof(struct ethhdr);

	// Original Ethernet is outside, let's copy it to the inside
	__builtin_memcpy(ieth,oeth,sizeof(struct ethhdr));
	// __builtin_memset(oeth,0,sizeof(struct ethhdr));

	oeth->h_proto = bpf_ntohs(ETH_P_NSH);
	ieth->h_proto = prev_proto;
	// oeth->h_dest and oeth->h_src will be set by fwd stage

	if(set_src_mac(oeth)) return cb_retother(TC_ACT_SHOT);
	__builtin_memmove(oeth->h_dest,cls->next_hop,ETH_ALEN);

	nsh->basic_info = bpf_htons( ((uint16_t) 0)     |
						         NSH_TTL_DEFAULT 	|
						         NSH_BASE_LENGHT_MD_TYPE_2);
	nsh->md_type 	= NSH_MD_TYPE_2;
	nsh->next_proto = NSH_NEXT_PROTO_ETHER;

    // No need for htonl. We add the entry in the map
    // using big endian notation.
    nsh->serv_path 	= cls->sph;

    cb_debug("[CLASSIFY] NSH added.\n");

	// TODO: This should be bpf_redirect_map(), to allow
	// redirecting the packet to another interface
	return cb_retok(TC_ACT_OK);
}
char _license[] SEC("license") = "GPL";
