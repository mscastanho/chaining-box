/*
 * This program was automatically generated with libkefir.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/pkt_cls.h>
#include <linux/swab.h>
#include <linux/tcp.h>

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define bpf_ntohs(x) (__builtin_constant_p(x) ?\
	___constant_swab16(x) : __builtin_bswap16(x))
#else
#define bpf_ntohs(x) (x)
#endif

#define BPF_ANNOTATE_KV_PAIR(name, type_key, type_val)		\
	struct ____btf_map_##name {				\
		type_key key;					\
		type_val value;					\
	};							\
	struct ____btf_map_##name				\
	__attribute__((section(".maps." #name), used))		\
		____btf_map_##name = { }

static void *(*bpf_map_lookup_elem)(void *map, void *key) =
	(void *) BPF_FUNC_map_lookup_elem;

#define RET_PASS TC_ACT_OK
#define RET_DROP TC_ACT_SHOT

struct filter_key {
	uint16_t		ethertype;
	uint8_t		processed_l4;
	uint32_t	ipv4_src;
	uint32_t	ipv4_dst;
	uint16_t	l4proto;
	uint16_t	l4port_src;
	uint16_t	l4port_dst;
};

enum match_type {
	MATCH_IPV4_SRC		= 5,
	MATCH_IPV4_DST		= 6,
	MATCH_IPV4_L4PROTO	= 10,
	MATCH_IPV4_L4PORT_SRC	= 12,
	MATCH_IPV4_L4PORT_DST	= 13,
};

enum comp_operator {
	OPER_EQUAL	= 0,
};

enum action_code {
	ACTION_CODE_DROP	= 0,
	ACTION_CODE_PASS	= 1,
};

#define MATCH_FLAGS_USE_MASK	1

struct rule_match {
	enum match_type		match_type;
	enum comp_operator	comp_operator;
	__u64			value[2];
	__u64	flags;
	__u64	mask[2];
};

struct filter_rule {
	enum action_code	action_code;
	struct rule_match	matches[5];
};

struct bpf_elf_map {
	__u32 type;
	__u32 size_key;
	__u32 size_value;
	__u32 max_elem;
	__u32 flags;
	__u32 id;
	__u32 pinning;
	__u32 inner_id;
	__u32 inner_idx;
};

struct bpf_elf_map __attribute__((section("maps"), used)) rules = {
	.type		= BPF_MAP_TYPE_ARRAY,
	.size_key	= sizeof(__u32),
	.size_value	= sizeof(struct filter_rule),
	.max_elem	= 2,
	.pinning = 2 // PIN_GLOBAL_NS
};
BPF_ANNOTATE_KV_PAIR(rules, __u32, struct filter_rule);

static int process_l4(void *data, void *data_end, __u32 l4_off, struct filter_key *key)
{
	struct tcphdr *tcph = data + l4_off;

	if ((void *)(tcph + 1) > data_end)
		return 0;

	key->processed_l4 = 1;
	key->l4port_src = tcph->source;
	key->l4port_dst = tcph->dest;

	return 0;
}

static int process_ipv4(void *data, void *data_end, uint8_t nh_off,
		 struct filter_key *key)
{
	struct iphdr *iph = data + nh_off;

	if ((void *)(iph + 1) > data_end)
		return -1;
	if ((void *)iph + 4 * iph->ihl > data_end)
		return -1;

	key->ipv4_src = iph->saddr;
	key->ipv4_dst = iph->daddr;
	key->l4proto = iph->protocol;

	if (process_l4(data, data_end, nh_off + 4 * iph->ihl, key))
		return -1;

	return 0;
}

static int extract_key(void *data, void *data_end, struct filter_key *key)
{
	struct ethhdr *eth = data;
	unsigned int i;
	uint8_t nh_off;

	nh_off = sizeof(struct ethhdr);
	if (data + nh_off > data_end)
		return -1;
	key->ethertype = bpf_ntohs(eth->h_proto);

#pragma clang loop unroll(full)
	for (i = 0; i < 2; i++) {
		if (key->ethertype == ETH_P_8021Q || key->ethertype == ETH_P_8021AD) {
			void *vlan_hdr;

			vlan_hdr = data + nh_off;
			nh_off += 4;
			if (data + nh_off > data_end)
				return -1;
			key->ethertype = *(uint16_t *)(data + nh_off - 2);
			key->ethertype = bpf_ntohs(key->ethertype);
		}
	}

	switch (key->ethertype) {
	case ETH_P_IP:
		if (process_ipv4(data, data_end, nh_off, key))
			return 0;
		break;
	default:
		return 0;
	}

	return 0;
}

static bool check_match(void *matchval, size_t matchlen, struct rule_match *match)
{
	uint64_t copy[2] = {0};
	size_t i;

	memcpy(copy, matchval, matchlen);

	for (i = 0; i < 2; i++)
		copy[i] &= (match->flags & MATCH_FLAGS_USE_MASK) ?
			match->mask[i] : 0xffffffffffffffff;


	if (match->comp_operator == OPER_EQUAL) {
		if (copy[0] != match->value[0])
			return false;
		if (matchlen > sizeof(__u64) &&
		    copy[1] != match->value[1])
			return false;
		return true;
	}

	return false;
}

static int get_retval(enum action_code code)
{
	switch (code) {
	case ACTION_CODE_DROP:
		return RET_DROP;
	case ACTION_CODE_PASS:
		return RET_PASS;
	default:
		return RET_PASS;
	}
}

static int check_nth_rule(struct filter_key *key, int n, int *res)
{
	struct filter_rule *rule;
	struct rule_match *match;
	bool does_match = true;
	size_t i;

	rule = (struct filter_rule *)bpf_map_lookup_elem(&rules, &n);
	if (!rule)
		return 0;

	for (i = 0; i < 5; i++) {
		match = &rule->matches[i];

		switch (match->match_type) {
		case MATCH_IPV4_SRC:
			does_match = does_match &&
				(key->ethertype == ETH_P_IP) &&
				check_match(&key->ipv4_src,
					    sizeof(key->ipv4_src), match);
			break;
		case MATCH_IPV4_DST:
			does_match = does_match &&
				(key->ethertype == ETH_P_IP) &&
				check_match(&key->ipv4_dst,
					    sizeof(key->ipv4_dst), match);
			break;
		case MATCH_IPV4_L4PROTO:
			does_match = does_match &&
				(key->ethertype == ETH_P_IP) &&
				check_match(&key->l4proto,
					    sizeof(key->l4proto), match);
			break;
		case MATCH_IPV4_L4PORT_SRC:
			does_match = does_match &&
				(key->ethertype == ETH_P_IP) &&
				key->processed_l4 &&
				check_match(&key->l4port_src,
					    sizeof(key->l4port_src), match);
			break;
		case MATCH_IPV4_L4PORT_DST:
			does_match = does_match &&
				(key->ethertype == ETH_P_IP) &&
				key->processed_l4 &&
				check_match(&key->l4port_dst,
					    sizeof(key->l4port_dst), match);
			break;
		default:
			break;
		}

		if (!does_match)
			break;
	}

	if (does_match) {
		*res = get_retval(rule->action_code);
		return 1;
	} else {
		return 0;
	}
}

__attribute__((section("classifier"), used))
int cls_main(struct __sk_buff *ctx)
{
	void *data_end = (void *)(long)ctx->data_end;
	void *data = (void *)(long)ctx->data;
	struct filter_key key = {0};
	struct ethhdr *eth = data;
	uint8_t nh_off;
	int res, i = 0;

	if (extract_key(data, data_end, &key))
		return RET_PASS;

	for (i = 0; i < 2; i++)
		if (check_nth_rule(&key, i, &res))
			return res;

	return RET_PASS;
}

char _license[] __attribute__((section("license"), used)) = "Dual BSD/GPL";
/*
 * This BPF program was generated from the following filter:
 *
 *  - rule  0
 * 	match  0: IPv4 source address              | operator  0: == | value  0: 127.181.232.192  | mask  0: ff ff ff c
 * 	match  1: IPv4 destination address         | operator  1: == | value  1: 225.224.192.107  | mask  1: ff ff c0 ff
 * 	match  2: IPv4, L4 protocol                | operator  2: == | value  2: 6               
 * 	match  3: IPv4, L4 destination port        | operator  3: == | value  3: 2124            
 * 	match  4: IPv4, L4 source port             | operator  4: == | value  4: 30793           
 * 	action: drop
 *  - rule  1
 * 	match  0: IPv4 source address              | operator  0: == | value  0: 112.198.145.147  | mask  0: f0 ff ff ff
 * 	match  1: IPv4 destination address         | operator  1: == | value  1: 209.144.108.248  | mask  1: ff ff ff fc
 * 	match  2: IPv4, L4 protocol                | operator  2: == | value  2: 6               
 * 	match  3: IPv4, L4 destination port        | operator  3: == | value  3: 12861           
 * 	match  4: IPv4, L4 source port             | operator  4: == | value  4: 22888           
 * 	action: drop
 */
