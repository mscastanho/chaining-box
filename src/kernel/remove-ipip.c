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
#include "cb_helpers.h"
#include "bpf_elf.h"
#include "cb_common.h"
#include "nsh.h"

SEC("xdp/decap")
int decap_ipip(struct xdp_md *ctx)
{
  void *data_end = (void *)(long)ctx->data_end;
  void *data = (void *)(long)ctx->data;
  struct ethhdr *eth;
  struct iphdr *ip;
  int offset = 0;
  uint16_t h_proto = 0;
  int ret = 0;

  eth = data;
  if(data + sizeof(struct ethhdr) > data_end)
    return XDP_PASS;

  offset += sizeof(struct ethhdr);
  h_proto = bpf_ntohs(eth->h_proto);

  /* Only interested in IPv4 packets */
  if(h_proto != ETH_P_IP)
    return XDP_PASS;

  ip = data + offset;
  offset += sizeof(struct iphdr);

  if ((void*) ip + sizeof(struct iphdr) > data_end)
    return XDP_PASS;

  /* Check if we have IPIP */
  if(ip->protocol != IPPROTO_IPIP)
    return XDP_PASS;

  cb_debug("[RIPIP] Got IPIP packet!\n");

  /* Move Ethernet header inside */
  __builtin_memmove(data + offset - sizeof(struct ethhdr), eth, sizeof(struct ethhdr));

  /* Remove extra bytes left by outer IP header */
  ret = bpf_xdp_adjust_head(ctx, (int)(sizeof(struct iphdr)));
  if (ret)
    return XDP_DROP;

  data_end = (void *)(long)ctx->data_end;
  data = (void *)(long)ctx->data;

  cb_debug("[RIPIP] Decapsulated packet; Size after: %d\n",data_end-data);

  return XDP_PASS;
}

char _license[] SEC("license") = "GPL";
