#ifndef COMMON_H_
#define COMMON_H_

#include <linux/if_ether.h>

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

// Nicer way to call bpf_trace_printk()
#define printk(fmt, ...)						\
		({							\
			char ____fmt[] = fmt;				\
			bpf_trace_printk(____fmt, sizeof(____fmt),	\
				     ##__VA_ARGS__);			\
		})
struct fwd_entry {
    // Flags:
    // Bit 0 : is end of chain
    // Bits 1-7 : reserved
    __u8 flags;
    unsigned char address[ETH_ALEN];
}__attribute__((packed));

struct cls_entry {
    __u32 sph;
    unsigned char next_hop[ETH_ALEN];
}__attribute__((packed));

struct ip_5tuple {
    __u32 ip_src;
    __u32 ip_dst;
    __u16 sport;
    __u16 dport;
    __u8 proto;
}__attribute__((packed));

struct vlanhdr {
	__be16 h_vlan_TCI;
	__be16 h_vlan_encapsulated_proto;
};

#endif /* COMMON_H_ */