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

#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
//#include <linux/nsh.h>

#include "bpf_helpers.h"
#include "nsh.h"

// BPF_FUNC_skb_change_head()
// BPF_FUNC_skb_change_tail()

// BPF_HASH(nsh_info,u16,struct nshhdr,);
int xdp_prog(struct xdp_md *ctx)
{
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;
    struct ethhdr *eth = data;
    struct nshhdr *nsh;

    if(data >= data_end) {
        return XDP_DROP;
    }

	if(ntohs(eth->h_proto) != ETH_P_NSH){
		return XDP_PASS;
	}else if(nsh->next_proto != NSH_NEXT_PROTO_ETHER){
		return XDP_PASS;
	}else{
		// Remove outer Ethernet + NSH headers
		if(bpf_xdp_adjust_head(ctx, 0 - (int)sizeof(struct nshhdr)))
			return XDP_DROP;
	} 

    return XDP_PASS;
}