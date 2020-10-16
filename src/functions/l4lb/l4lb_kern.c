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
	.type = BPF_MAP_TYPE_ARRAY,
	.size_key = sizeof(int),
	.size_value = sizeof(int),
	.pinning = PIN_GLOBAL_NS,
	.max_elem = 1,
};

/* static __always_inline struct dest_info *hash_get_dest(struct pkt_meta *pkt) */
/* { */
/* 	__u32 key; */
/* 	struct dest_info *tnl; */

/* 	/\* hash packet source ip with both ports to obtain a destination *\/ */
/* 	key = jhash_2words(pkt->src, pkt->ports, MAX_SERVERS) % MAX_SERVERS; */

/* 	/\* get destination's network details from map *\/ */
/* 	tnl = bpf_map_lookup_elem(&servers, &key); */
/* 	if (!tnl) { */
/* 		/\* if entry does not exist, fallback to key 0 *\/ */
/* 		key = 0; */
/* 		tnl = bpf_map_lookup_elem(&servers, &key); */
/* 	} */
/* 	return tnl; */
/* } */

/* For now our load balancer will simply use the same dest IP as the internal
   packet. A proper implementation with the map will be done later. */
/* static __always_inline struct dest_info *hash_get_dest(struct pkt_meta *pkt, struct dest_info tnl) */
/* { */
/* 	tnl.saddr = pkt->src; */
/* 	tnl.daddr = pkt->dst; */
/* 	/\* Other fields are unused. *\/ */

/* 	return tnl; */
/* } */

static __always_inline bool parse_udp(void *data, __u64 off, void *data_end,
				      struct pkt_meta *pkt)
{
	struct udphdr *udp;

	udp = data + off;
	if (udp + 1 > data_end)
		return false;

	pkt->port16[0] = udp->source;
	pkt->port16[1] = udp->dest;

	return true;
}

static __always_inline bool parse_tcp(void *data, __u64 off, void *data_end,
				      struct pkt_meta *pkt)
{
	struct tcphdr *tcp;

	tcp = data + off;
	if (tcp + 1 > data_end)
		return false;

	pkt->port16[0] = tcp->source;
	pkt->port16[1] = tcp->dest;

	return true;
}

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
	struct pkt_meta pkt = {};
	struct ethhdr *eth;
	/* struct ethhdr *old_eth; */
	/* struct dest_info tnl; */
	struct iphdr iph_tnl;
	struct iphdr *outer_iph;
	struct iphdr *inner_iph;
	__u16 *next_iph_u16;
	/* __u16 pkt_size; */
	__u16 payload_len;
	__u8 protocol;
	int *tx_port;
	u32 csum = 0;
	int key = 0;

	outer_iph = data + off;
	if (outer_iph + 1 > data_end)
		return TC_ACT_SHOT;
	if (outer_iph->ihl != 5)
		return TC_ACT_SHOT;

	protocol = outer_iph->protocol;
	payload_len = bpf_ntohs(outer_iph->tot_len);
	off += sizeof(struct iphdr);

	/* do not support fragmented packets as L4 headers may be missing */
	if (outer_iph->frag_off & IP_FRAGMENTED)
		return TC_ACT_SHOT;

	pkt.src = outer_iph->saddr;
	pkt.dst = outer_iph->daddr;

	/* obtain port numbers for UDP and TCP traffic */
	if (protocol == IPPROTO_TCP) {
		if (!parse_tcp(data, off, data_end, &pkt))
			return TC_ACT_SHOT;
	} else if (protocol == IPPROTO_UDP) {
		if (!parse_udp(data, off, data_end, &pkt))
			return TC_ACT_SHOT;
	} else if (protocol == IPPROTO_ICMP) {
	  pkt.ports = 0;
	} else {
		return TC_ACT_OK;
	}

	/* allocate a destination using packet hash and map lookup */
	/* tnl = hash_get_dest(&pkt); */
	/* if (!tnl) */
	/* 	return TC_ACT_SHOT */;

	/* extend the packet for ip header encapsulation */
	if (bpf_skb_adjust_room(ctx, (int)sizeof(struct iphdr), BPF_ADJ_ROOM_MAC, 0))
		return TC_ACT_SHOT;

	data = (void *)(long)ctx->data;
	data_end = (void *)(long)ctx->data_end;

	/* relocate ethernet header to start of packet and set MACs */
	eth = data;
	outer_iph = data + sizeof(*eth);
	inner_iph = data + sizeof(*eth) + sizeof(*outer_iph);

	if (eth + 1 > data_end || outer_iph + 1 > data_end ||
	    inner_iph + 1 > data_end)
		return TC_ACT_SHOT;

	/* set_ethhdr(new_eth, old_eth, tnl, bpf_htons(ETH_P_IP)); */

	/* create an additional ip header for encapsulation */
	iph_tnl.version = 4;
	iph_tnl.ihl = sizeof(*outer_iph) >> 2;
	iph_tnl.frag_off = 0;
	iph_tnl.protocol = IPPROTO_IPIP;
	iph_tnl.check = 0;
	iph_tnl.id = 0;
	iph_tnl.tos = 0;
	iph_tnl.tot_len = bpf_htons(payload_len + sizeof(*outer_iph));
	iph_tnl.daddr = inner_iph->daddr;
	iph_tnl.saddr = inner_iph->saddr;
	iph_tnl.ttl = 8;

	/* calculate ip header checksum */
	next_iph_u16 = (__u16 *)&iph_tnl;
	#pragma clang loop unroll(full)
	for (int i = 0; i < (int)sizeof(*outer_iph) >> 1; i++)
		csum += *next_iph_u16++;
	iph_tnl.check = ~((csum & 0xffff) + (csum >> 16));

	*outer_iph = iph_tnl;

	/* increment map counters */
	/* pkt_size = (__u16)(data_end - data); /\* payload size excl L2 crc *\/ */
	/* __sync_fetch_and_add(&tnl->pkts, 1); */
	/* __sync_fetch_and_add(&tnl->bytes, pkt_size); */

	/* Lookup what ifindex to redirect packets to */
	tx_port = bpf_map_lookup_elem(&l4lb_egress, &key);
	if (!tx_port)
		return TC_ACT_SHOT;

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
