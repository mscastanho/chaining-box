/*  TC (Traffic Control) eBPF redirect benchmark
 *
 *  NOTICE: TC loading is different from XDP loading. TC bpf objects
 *          use the 'tc' cmdline tool from iproute2 for loading and
 *          attaching bpf programs.
 *
 *  Copyright(c) 2017 Jesper Dangaard Brouer, Red Hat Inc.
 */
#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/tcp.h>
#include <linux/udp.h>

#include <linux/pkt_cls.h>

#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

#include "cb_helpers.h"

/* Notice: TC and iproute2 bpf-loader uses another elf map layout */
struct bpf_elf_map {
	__u32 type;
	__u32 size_key;
	__u32 size_value;
	__u32 max_elem;
	__u32 flags;
	__u32 id;
	__u32 pinning;
};

/* TODO: Describe what this PIN_GLOBAL_NS value 2 means???
 *
 * A file is automatically created here:
 *  /sys/fs/bpf/tc/globals/egress_ifindex
 */
#define PIN_GLOBAL_NS	2

struct bpf_elf_map SEC("maps") egress_ifindex = {
	.type = BPF_MAP_TYPE_ARRAY,
	.size_key = sizeof(int),
	.size_value = sizeof(int),
	.pinning = PIN_GLOBAL_NS,
	.max_elem = 1,
};

struct info {
  __u32 srcip;
  __u8 dstmac[6];
  __u8 ops;
}__attribute__((packed));

#define SET_DST_MAC 0x1
#define SWAP_IP     0x2
#define SWAP_MAC    0x4

struct bpf_elf_map SEC("maps") infomap = {
	.type = BPF_MAP_TYPE_ARRAY,
	.size_key = sizeof(int),
	.size_value = sizeof(struct info),
	.pinning = PIN_GLOBAL_NS,
	.max_elem = 1,
};

static void swap_src_dst_mac(void *data)
{
	unsigned short *p = data;
	unsigned short dst[3];

	dst[0] = p[0];
	dst[1] = p[1];
	dst[2] = p[2];
	p[0] = p[3];
	p[1] = p[4];
	p[2] = p[5];
	p[3] = dst[0];
	p[4] = dst[1];
	p[5] = dst[2];
}

static void set_dst_mac(void *data, void *mac)
{
	unsigned short *dst = data;
	unsigned short *p = mac;

	dst[0] = p[0];
	dst[1] = p[1];
	dst[2] = p[2];
}

static void swap_src_dst_ip(struct iphdr *ip){
  unsigned int aux;
  aux = ip->saddr;
  ip->saddr = ip->daddr;
  ip->daddr = aux;
}

/* Notice this section name is used when attaching TC filter
 *
 * Like:
 *  $TC qdisc   add dev $DEV clsact
 *  $TC filter  add dev $DEV ingress bpf da obj $BPF_OBJ sec ingress_redirect
 *  $TC filter show dev $DEV ingress
 *  $TC filter  del dev $DEV ingress
 *
 * Does TC redirect respect IP-forward settings?
 *
 */
SEC("ingress_redirect")
int _ingress_redirect(struct __sk_buff *skb)
{
  cb_debug("Starting TC redirect\n");

	void *data     = (void *)(long)skb->data;
	void *data_end = (void *)(long)skb->data_end;
	struct ethhdr *eth = data;
	struct info *info;
  int key = 0, *ifindex;
  __u32 sip;

	if (data + sizeof(*eth) > data_end)
		return TC_ACT_OK;

	/* Keep ARP resolution working */
	if (eth->h_proto == bpf_htons(ETH_P_ARP))
		return TC_ACT_OK;

	/* Lookup what ifindex to redirect packets to */
	ifindex = bpf_map_lookup_elem(&egress_ifindex, &key);
	if (!ifindex)
		return TC_ACT_OK;

  /* Simulate some processing by just wasting some cycles here.
   *
   * volatile used to avoid clang removing this loop. */
  /* #define ITERATIONS 10000 */
  /* volatile int count = 0; */
  /* for(int i = 0 ; i < ITERATIONS; i++){ */
  /*  count++; */
  /* } */

  cb_debug("ifi OK. Checking sip...\n");

  struct iphdr *ip = (struct iphdr*)(data + sizeof(struct ethhdr));
  if((void*)ip + sizeof(struct iphdr) > data_end)
    return TC_ACT_OK;


  info = bpf_map_lookup_elem(&infomap, &key);
  if (!info) /* check required by verifier */
    return TC_ACT_OK;

  sip = info->srcip;

  /* If srcip is not set (== 0) then the program will not impose any restrictions
   * and just forward *all* traffic to the egress port.
   *
   * Otherwise, only packets with src IP matching srcip will be redirected, the
   * rest will be passed along the stack. */
  if (sip) {
    if(ip->protocol == 1){
      cb_debug("IT'S A PING!!!\n");
    }

    if(sip != ip->saddr){
      cb_debug("sip check failed. Passing along... %x != %x\n",sip,ip->saddr);
      return TC_ACT_OK;
    }else{
      cb_debug("IT'S A MATCH!!!\n");
    }
  }

  if (info->ops & SWAP_IP)
    swap_src_dst_ip(ip);

  if (info->ops & SWAP_MAC)
    swap_src_dst_mac(data);

  if (info->ops & SET_DST_MAC)
    set_dst_mac(data, &info->dstmac);

  return bpf_redirect(*ifindex, 0);
}

char _license[] SEC("license") = "GPL";
