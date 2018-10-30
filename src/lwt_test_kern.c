/* Copyright (c) 2016 Thomas Graf <tgraf@tgraf.ch>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 */

// #include <stdint.h>
#include <stddef.h>
#include <linux/bpf.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <uapi/linux/tcp.h>
#include <linux/udp.h>
#include <linux/icmpv6.h>
#include <linux/if_ether.h>
#include "bpf_helpers.h"
// #include <string.h>

# define printk(fmt, ...)						\
		({							\
			char ____fmt[] = fmt;				\
			bpf_trace_printk(____fmt, sizeof(____fmt),	\
				     ##__VA_ARGS__);			\
		})

#define CB_MAGIC 1234

/* Test: Verify skb data can be read */
SEC("test_data")
int do_test_data(struct __sk_buff *skb)
{
	void *data = (void *)(long)skb->data;
	void *data_end = (void *)(long)skb->data_end;
	struct iphdr *iph = data;

	if (data + sizeof(*iph) > data_end) {
		printk("packet truncated\n");
		return BPF_DROP;
	}

	printk("src: %x dst: %x\n", iph->saddr, iph->daddr);

	return BPF_OK;
}

#define IP_CSUM_OFF offsetof(struct iphdr, check)
#define IP_DST_OFF offsetof(struct iphdr, daddr)
#define IP_SRC_OFF offsetof(struct iphdr, saddr)
#define IP_PROTO_OFF offsetof(struct iphdr, protocol)
#define TCP_CSUM_OFF offsetof(struct tcphdr, check)
#define UDP_CSUM_OFF offsetof(struct udphdr, check)
#define IS_PSEUDO 0x10


static inline int __do_push_ll(struct __sk_buff *skb)
{
	uint64_t smac = 0xefefefefefefefef, dmac = 0xbbbbbbbbbbbbbbbb;
	int ret;
	struct ethhdr ehdr;

	ret = bpf_skb_change_head(skb, 14, 0);
	if (ret < 0) {
		printk("skb_change_head() failed: %d\n", ret);
	}

	ehdr.h_proto = __constant_htons(ETH_P_IP);
	memcpy(&ehdr.h_source, &smac, 6);
	memcpy(&ehdr.h_dest, &dmac, 6);

	ret = bpf_skb_store_bytes(skb, 0, &ehdr, sizeof(ehdr), 0);
	if (ret < 0) {
		printk("skb_store_bytes() failed: %d\n", ret);
		return BPF_DROP;
	}

	printk("Successfully encapsulated packet! %d\n", ret);

	return BPF_OK;
}

SEC("push_ll")
int do_push_ll(struct __sk_buff *skb)
{
	return __do_push_ll(skb);
}

char _license[] SEC("license") = "GPL";