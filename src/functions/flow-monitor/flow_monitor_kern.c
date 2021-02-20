#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/pkt_cls.h>


#include "bpf_endian.h"
#include "cb_helpers.h"
#include "bpf_helpers.h"
#include "bpf_elf.h"
#include "cb_common.h"
#include "nsh.h"

#define HISTO_BUCKETS 24

struct flow_stats {
  __u64 pkts;
  __u64 bytes;
};

MAP(stats, BPF_MAP_TYPE_HASH, sizeof(struct ip_5tuple),
  sizeof(struct flow_stats), 2048, PIN_GLOBAL_NS);

/* Histogram of packet size distribution */
MAP(szhist, BPF_MAP_TYPE_ARRAY, sizeof(__u32),
  sizeof(__u64), HISTO_BUCKETS, PIN_GLOBAL_NS);

MAP(egress_ifindex, BPF_MAP_TYPE_ARRAY, sizeof(int),
  sizeof(int), 1, PIN_GLOBAL_NS);

MAP(srcip, BPF_MAP_TYPE_ARRAY, sizeof(int),
  sizeof(int), 1, PIN_GLOBAL_NS);

SEC("ingress_redirect")
int flow_monitor(struct __sk_buff *skb) {
  void *data_end = (void *)(long)skb->data_end;
  void *data = (void *)(long)skb->data;
  struct ethhdr *eth;
  struct iphdr *ip;
  struct flow_stats *sentry;
  __u64 *hentry;
  struct ip_5tuple key = { };
  struct flow_stats one = { };
  __u32 idx;
  int ret, zero = 0, *ifindex;
  __u32 *sip;

  if(data + sizeof(struct ethhdr) + sizeof(struct iphdr) > data_end){
    cb_debug("[FLMON] Bounds check #1 failed.\n");
    return cb_retother(TC_ACT_OK);
  }

  eth = data;
  ip = (void*)eth + sizeof(struct ethhdr);

  if(bpf_ntohs(eth->h_proto) != ETH_P_IP){
    cb_debug("[FLMON] Not an IPv4 packet, passing along.\n");
    return cb_retother(TC_ACT_OK);
  }

  ret = get_tuple(ip,data_end,&key);
  if (ret < 0){
    cb_debug("[FLMON] get_tuple() failed: %d\n",ret);
    return cb_retother(TC_ACT_OK);
  }

  /* ==== Flow monitoring code ==== */

  /* Update statistics */
  sentry = bpf_map_lookup_elem(&stats, &key);
  if(sentry){
    lock_xadd(&sentry->pkts, 1);
    lock_xadd(&sentry->bytes, skb->len);
  }else{
    one.pkts = 1;
    one.bytes = skb->len;
    bpf_map_update_elem(&stats, &key, &one, BPF_ANY);
  }

  /* Update histogram */
  idx = skb->len / 64;
  hentry = bpf_map_lookup_elem(&szhist, &idx);
  if(hentry){
    lock_xadd(hentry, 1);
  }

  /* ==== Packet redirection code ==== */

  /* Lookup what ifindex to redirect packets to */
  ifindex = bpf_map_lookup_elem(&egress_ifindex, &zero);
  if (!ifindex)
    return TC_ACT_OK;

  // struct iphdr *ip = (struct iphdr*)(data + sizeof(struct ethhdr));
  // if((void*)ip + sizeof(struct iphdr) > data_end)
    // return TC_ACT_OK;

  sip = bpf_map_lookup_elem(&srcip, &zero);
  if (!sip)
    return TC_ACT_OK;
//
  if(ip->protocol == 1){
    cb_debug("IT'S A PING!!!\n");
  }
//
  if(*sip != ip->saddr){
    cb_debug("sip check failed. Passing along... %x != %x\n",*sip,ip->saddr);
    return TC_ACT_OK;
  }else{
    cb_debug("IT'S A MATCH!!!\n");
  }

  return bpf_redirect(*ifindex, 0); // __bpf_tx_skb / __dev_xmit_skb
}
char _license[] SEC("license") = "GPL";
