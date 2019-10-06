#include <linux/in.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>

#include <stdint.h>
#include <stdlib.h>

#include "bpf_helpers.h"
#include "common.h"

static inline int get_tuple(void* ip_data, void* data_end, struct ip_5tuple *t){
	struct iphdr *ip;
	struct udphdr *udp;
	struct tcphdr *tcp;
	__u64 *init;

	ip = ip_data;
	if((void*) ip + sizeof(*ip) > data_end){
		#ifdef DEBUG
		bpf_printk("get_tuple(): Error accessing IP hdr\n");
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
				bpf_printk("get_tuple(): Error accessing UDP hdr\n");
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
				bpf_printk("get_tuple(): Error accessing TCP hdr\n");
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
            #ifdef DEBUG
            bpf_printk("get_tuple(): Unrecognized L4 protocol: %d\n",ip->protocol);
            #endif /* DEBUG */
			return -1;
	}

	return 0;
};
