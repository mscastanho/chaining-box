#include <linux/in.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>

#include <stdint.h>
#include <stdlib.h>

#include "bpf_helpers.h"
#include "common.h"

#ifdef BPFMAPDEF
extern struct bpf_map_def prog_stats;
#else
extern struct bpf_elf_map prog_stats;
#endif /* BPFMAPDEF */

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

/* Mark beginning of program */
static inline void bpf_mark_init(void){
#ifdef ENABLE_STATS
  uint8_t zero = 0;
  uint64_t ts = bpf_ktime_get_ns();
  struct stats *s = bpf_map_lookup_elem(&prog_stats, &zero);

  if(s) s->init_ts = ts;
#endif /* ENABLE_STATS */

  /* Do nothing */
}

/* Return with OK status */
static inline int bpf_retok(int code){
#ifdef ENABLE_STATS
  uint8_t zero = 0;
  uint64_t ts = bpf_ktime_get_ns();
  struct stats *s = bpf_map_lookup_elem(&prog_stats, &zero);

  if(s){
    lock_xadd(&s->tx, 1);
    lock_xadd(&s->lat_avg_sum,ts - s->init_ts);
  }
#endif /* ENABLE_STATS */

  return code;
}

/* Return dropping packet */
static inline int bpf_retdrop(int code){
#ifdef ENABLE_STATS
  uint8_t zero = 0;
  struct stats *s = bpf_map_lookup_elem(&prog_stats, &zero);
  if(s) lock_xadd(&s->dropped, 1);
#endif /* ENABLE_STATS */

  return code;
}

/* Return with error */
static inline int bpf_reterr(int code){
#ifdef ENABLE_STATS
  uint8_t zero = 0;
  struct stats *s = bpf_map_lookup_elem(&prog_stats, &zero);
  if(s) lock_xadd(&s->error, 1);
#endif /* ENABLE_STATS */

  return code;
}
