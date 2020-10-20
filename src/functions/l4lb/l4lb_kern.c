// SPDX-License-Identifier: (GPL-2.0 OR BSD-2-Clause)
// Copyright (c) 2018 Netronome Systems, Inc.

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <linux/bpf.h>
#include <linux/icmp.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/if_packet.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>

#include <linux/pkt_cls.h>

#include "bpf_elf.h"
#include "bpf_endian.h"
#include "bpf_helpers.h"
#include "jhash.h"

#include "cb_helpers.h"

#define MAX_SERVERS 512
/* 0x3FFF mask to check for fragment offset field */
#define IP_FRAGMENTED 65343

#define PIN_GLOBAL_NS	2

struct pkt_meta {
	__be32 src;
	__be32 dst;
	union {
		__u32 ports;
		__u16 port16[2];
	};
};

struct dest_info {
	__u32 saddr;
	__u32 daddr;
	__u64 bytes;
	__u64 pkts;
	__u8 dmac[6];
};

/* struct bpf_elf_map SEC("maps") servers = { */
/* 	.type = BPF_MAP_TYPE_HASH, */
/* 	.size_key = sizeof(__u32), */
/* 	.size_value = sizeof(struct dest_info), */
/* 	.pinning = PIN_GLOBAL_NS, */
/* 	.max_elem = MAX_SERVERS, */
/* }; */

struct bpf_elf_map SEC("maps") l4lb_egress = {
	.type = BPF_MAP_TYPE_HASH,
	.size_key = sizeof(int),
	.size_value = sizeof(int),
	.pinning = PIN_GLOBAL_NS,
	.max_elem = MAX_SERVERS,
};

static __always_inline int *hash_get_dest(struct ip_5tuple *tuple)
{
	__u32 key, ports;
	int *tnl;

	ports = (tuple->sport << 16) + tuple->dport;

	/* hash packet source ip with both ports to obtain a destination */
	key = jhash_2words(tuple->ip_src, ports, MAX_SERVERS) % MAX_SERVERS;

	/* get destination's network details from map */
	tnl = bpf_map_lookup_elem(&l4lb_egress, &key);
	if (!tnl) {
		/* if entry does not exist, fallback to key 0 */
		key = 0;
		tnl = bpf_map_lookup_elem(&l4lb_egress, &key);
	}
	return tnl;
}

/* For now our load balancer will simply use the same dest IP as the internal
   packet. A proper implementation with the map will be done later. */
/* static __always_inline struct dest_info *hash_get_dest(struct pkt_meta *pkt, struct dest_info tnl) */
/* { */
/* 	tnl.saddr = pkt->src; */
/* 	tnl.daddr = pkt->dst; */
/* 	/\* Other fields are unused. *\/ */

/* 	return tnl; */
/* } */

/* static __always_inline void set_ethhdr(struct ethhdr *new_eth, */
/* 				       const struct ethhdr *old_eth, */
/* 				       const struct dest_info *tnl, */
/* 				       __be16 h_proto) */
/* { */
/* 	memcpy(new_eth->h_source, old_eth->h_dest, sizeof(new_eth->h_source)); */
/* 	memcpy(new_eth->h_dest, tnl->dmac, sizeof(new_eth->h_dest)); */
/* 	new_eth->h_proto = h_proto; */
/* } */

static __always_inline int process_packet(struct __sk_buff *ctx, __u64 off)
{
	void *data_end = (void *)(long)ctx->data_end;
	void *data = (void *)(long)ctx->data;
	struct iphdr *outer_iph;
	struct ip_5tuple tuple;

	cb_debug("[L4LB] Processing packet...\n");

	outer_iph = data + off;
	if (outer_iph + 1 > data_end)
		return TC_ACT_SHOT;
	if (outer_iph->ihl != 5)
		return TC_ACT_SHOT;

	/* do not support fragmented packets as L4 headers may be missing */
	if (outer_iph->frag_off & IP_FRAGMENTED) {
	  cb_debug("[L4LB] Fragmented packet. Dropping.\n");
	  return TC_ACT_SHOT;
	}

	if (get_tuple (outer_iph, data_end, &tuple)) {
	  cb_debug("[L4LB] Failed to get 5-tuple\n");
	  return TC_ACT_SHOT;
	}

	/* allocate a destination using packet hash and map lookup */
	int *tx_port = hash_get_dest(&tuple);
	if (!tx_port) {
	  cb_debug("[L4LB] No egress interface configured. Dropping.\n");
	  return TC_ACT_SHOT;
	}

	cb_debug("[L4LB] Redirecting packet to port %d.\n", *tx_port);
	return bpf_redirect(*tx_port, 0);
}

SEC("l4lb")
int loadbal(struct __sk_buff *ctx)
{
	void *data_end = (void *)(long)ctx->data_end;
	void *data = (void *)(long)ctx->data;
	struct ethhdr *eth = data;
	__u32 eth_proto;
	__u32 nh_off;

	nh_off = sizeof(struct ethhdr);
	if (data + nh_off > data_end)
		return TC_ACT_SHOT;
	eth_proto = eth->h_proto;

	/* demo program only accepts ipv4 packets */
	if (eth_proto == bpf_htons(ETH_P_IP))
		return process_packet(ctx, nh_off);
	else
		return TC_ACT_OK;
}

char _license[] SEC("license") = "GPL";
