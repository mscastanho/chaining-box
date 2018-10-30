// Algorithm:
// 	receive packet
// 	inspect EtherType
// 		if not NSH
// 			return XDP_PASS
// 		else if NSH->next_proto is not Ethernet
// 			return XDP_PASS
// 		else
// 			save NSH info to table
// 			decap packet
// 			push id label to packet (ipid?)
//
// Maybe take care of V(X)LANs?		

#include <uapi/linux/bpf.h>
#include <uapi/linux/if_ether.h>
#include <uapi/linux/in.h>
#include <uapi/linux/ip.h>
#include <uapi/linux/tcp.h>
#include <uapi/linux/udp.h>

//#include <linux/nsh.h>

#include "bpf_helpers.h"
#include "common.h"
#include "nsh.h"

// BPF_FUNC_skb_change_head()
// BPF_FUNC_skb_change_tail()

// BPF_HASH(nsh_info,u16,struct nshhdr,);

struct bpf_map_def SEC("maps") nsh_data = {
	.type = BPF_MAP_TYPE_HASH,
	.key_size = sizeof(struct ip_5tuple),
	.value_size = sizeof(struct nsh_hdr),
	.max_entries = 2048,
};

static int get_tuple(void* ip_data, void* data_end, struct ip_5tuple *t){
    struct iphdr *ip;
    struct udphdr *udp;
    struct tcphdr *tcp;
	char fmt[] = "sfc_decap_kern: ERROR! step %d\n";

	ip = ip_data;
    if((void*) ip + sizeof(*ip) > data_end){
		bpf_trace_printk(fmt,sizeof(fmt),2);
		return -1;
	}

	t->ip_src = ip->saddr;
    t->ip_dst = ip->daddr;
	t->proto = ip->protocol;

    switch(ip->protocol){
        case IPPROTO_UDP:
            udp = ip_data + sizeof(*ip);
            if((void*) udp + sizeof(*udp) > data_end){
				bpf_trace_printk(fmt,sizeof(fmt),3);
				return -1;
			}

			t->dport = udp->dest;
            t->sport = udp->source;
            break;
        case IPPROTO_TCP:
            tcp = ip_data + sizeof(*ip);
            if((void*) tcp + sizeof(*tcp) > data_end){
				bpf_trace_printk(fmt,sizeof(fmt),4);
				return -1;
			}

	        t->dport = tcp->dest;
            t->sport = tcp->source;
            break;
        default: ; /* TODO: Add ICMP? */
    }

	return 0;
};

int xdp_prog(struct xdp_md *ctx)
{
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;
    struct ethhdr *eth = data;
    struct nsh_hdr *nsh = (void*)eth + sizeof(*eth);
	struct iphdr *ip = (void*)nsh + sizeof(*nsh) + sizeof(*eth);
	struct ip_5tuple key = { }; // Verifier does not allow uninitialized keys
	int ret = 0;
	char fmt[] = "sfc_decap_kern: ERROR! step %d\n";

    if(data + sizeof(*eth) > data_end)
    	return XDP_DROP;

	if(ntohs(eth->h_proto) != ETH_P_NSH)
		return XDP_PASS;
	
	nsh = (void*) eth + sizeof(*eth);

	// Check if we can access NSH
    if ((void*) nsh + sizeof(*nsh) > data_end)
        return XDP_DROP;
	
	// Check if next protocol is Ethernet
	if(nsh->next_proto != NSH_NEXT_PROTO_ETHER)
		return XDP_PASS;

	// Single check for Inner Ether + IP
	if((void*)nsh + sizeof(*eth) + sizeof(*ip) > data_end)
		return XDP_DROP;

	// Retrieve IP 5-tuple
	ret = get_tuple(ip,data_end,&key);
	if(ret){
		bpf_trace_printk(fmt,sizeof(fmt),1);
		return XDP_DROP;
	}

	// Save NSH data
	bpf_map_update_elem(&nsh_data, &key, nsh, BPF_ANY);

	// Remove outer Ethernet + NSH headers
	if(bpf_xdp_adjust_head(ctx, (int)(sizeof(*nsh) + sizeof(*eth))))
		return XDP_DROP;

    return XDP_PASS;
}

char _license[] SEC("license") = "GPL";