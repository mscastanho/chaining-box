#ifndef CB_HELPERS_H
#define CB_HELPERS_H

#include <linux/in.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <bpf/bpf_helpers.h>

#include "cb_common.h"

#ifdef BPFMAPDEF
extern struct bpf_map_def prog_stats;
#else
extern struct bpf_elf_map prog_stats;
#endif /* BPFMAPDEF */

#ifdef DEBUG
#define cb_debug(...) bpf_printk(__VA_ARGS__)
#else
#define cb_debug(...) do{;}while(0) /* Do nothing */
#endif

static inline int get_tuple(void* ip_data, void* data_end, struct ip_5tuple *t){
	struct iphdr *ip;
	struct udphdr *udp;
	struct tcphdr *tcp;

	ip = ip_data;
	if((void*) ip + sizeof(*ip) > data_end){
		cb_debug("get_tuple(): Error accessing IP hdr\n");
		return -1;
	}

	/* If using IP-in-IP encapsulation, skip the first IP header. */
	if(ip->protocol == IPPROTO_IPIP){
		ip += 1;

		if((void*)ip + sizeof(*ip) > data_end){
			cb_debug("get_tuple(): Error accessing second IP header\n");
			return -1;
		}
	}

	t->ip_src = ip->saddr;
	t->ip_dst = ip->daddr;
	t->proto = ip->protocol;

	switch(ip->protocol){
		case IPPROTO_UDP:
			udp = ip_data + sizeof(*ip);
			if((void*) udp + sizeof(*udp) > data_end){
				cb_debug("get_tuple(): Error accessing UDP hdr\n");
				return -1;
			}

			t->dport = udp->dest;
			t->sport = udp->source;
			break;
		case IPPROTO_TCP:
			tcp = ip_data + sizeof(*ip);
			if((void*) tcp + sizeof(*tcp) > data_end){
				cb_debug("get_tuple(): Error accessing TCP hdr\n");
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
		        cb_debug("get_tuple(): Unrecognized L4 protocol: %d\n",ip->protocol);
			return -1;
	}

	return 0;
};

/* Mark beginning of program */
static inline void cb_mark_init(void){
#ifdef ENABLE_STATS
  __u8 zero = 0;
  struct stats init_stats = {0,0,0,0,0};
  __u64 ts = bpf_ktime_get_ns();
  struct stats *s = bpf_map_lookup_elem(&prog_stats, &zero);

  if(s){
      lock_xadd(&s->rx, 1);
      s->init_ts = ts;
  } else bpf_map_update_elem(&prog_stats, &zero, &init_stats, BPF_NOEXIST);
#endif /* ENABLE_STATS */

  /* Do nothing */
}

/* Return and count as OK */
static inline int cb_retok(int code){
#ifdef ENABLE_STATS
  __u8 zero = 0;
  struct stats init_stats = {0,0,0,0,0};
  __u64 ts = bpf_ktime_get_ns();
  struct stats *s = bpf_map_lookup_elem(&prog_stats, &zero);

  if(s){
    lock_xadd(&s->tx, 1);
    lock_xadd(&s->lat_avg_sum,ts - s->init_ts);
  }else{
    bpf_map_update_elem(&prog_stats, &zero, &init_stats, BPF_NOEXIST);
  }
#endif /* ENABLE_STATS */

  return code;
}

/* Return and count as exited for 'other' purpose */
static inline int cb_retother(int code){
#ifdef ENABLE_STATS
  __u8 zero = 0;
  struct stats init_stats = {0,0,0,0,0};
  struct stats *s = bpf_map_lookup_elem(&prog_stats, &zero);
  if(s) {
    lock_xadd(&s->tx_other, 1);
  }else bpf_map_update_elem(&prog_stats, &zero, &init_stats, BPF_NOEXIST);
#endif /* ENABLE_STATS */

  return code;
}

#endif /* CB_HELPERS_H */
