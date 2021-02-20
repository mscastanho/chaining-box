#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/icmp.h>
#include <linux/pkt_cls.h>

#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

#include "cb_helpers.h"
#include "bpf_elf.h"
#include "cb_common.h"
#include "nsh.h"

static __always_inline void set_src_mac(struct ethhdr *eth){
  eth->h_source[0]=0xaa;
  eth->h_source[1]=0xbb;
  eth->h_source[2]=0xcc;
  eth->h_source[3]=0xdd;
  eth->h_source[4]=0xee;
  eth->h_source[5]=0xff;

  /* TODO: This is hardcoded for the value we use in the testbed. Should be
     configured using a map like we do for the xdp/decap stage. */
  eth->h_dest[0]=0x00;
  eth->h_dest[1]=0x15;
  eth->h_dest[2]=0x4d;
  eth->h_dest[3]=0x13;
  eth->h_dest[4]=0x81;
  eth->h_dest[5]=0x7f;
}

SEC("xdp/decap")
int decap_ipip(struct xdp_md *ctx)
{
  void *data_end = (void *)(long)ctx->data_end;
  void *data = (void *)(long)ctx->data;
  struct ethhdr *eth;
  struct iphdr *ip;
  struct icmphdr *icmp;
  int offset = 0;
  __u16 h_proto = 0;
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

  cb_debug("[RIPIP] Decapsulated packet; Size after: %d\n",data_end-data);

  data_end = (void *)(long)ctx->data_end;
  data = (void *)(long)ctx->data;

  eth = data;
  if (data + sizeof(struct ethhdr) > data_end)
    return XDP_PASS;

  eth = data;
  ip = data + sizeof(struct ethhdr); // We already know there is an IP header there

  if ((void*)ip + sizeof(struct iphdr) > data_end)
    return XDP_PASS;

  if (ip->protocol != IPPROTO_ICMP)
    return XDP_PASS;

  cb_debug("[RIPIP] Got ICMP packet\n");

  icmp = (void*)ip + sizeof(struct iphdr);

  if ((void*)icmp + sizeof(struct icmphdr) > data_end)
    return XDP_PASS;

  if (icmp->type != ICMP_ECHO)
    return XDP_PASS;

  cb_debug("[RIPIP] Got a ping request\n");

  /* Prepare reply */
  set_src_mac(eth);
  icmp->type = ICMP_ECHOREPLY;
  __be32 aux = ip->saddr;
  ip->saddr = ip->daddr;
  ip->daddr = aux;
  icmp->checksum = 0; /* Ignore the checksum for now. */

  cb_debug("[RIPIP] Sending ping reply \n");

  return XDP_TX;
}

char _license[] SEC("license") = "GPL";
