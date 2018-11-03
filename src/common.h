#ifndef COMMON_H_
#define COMMON_H_

#include <linux/if_ether.h>

// Pins a map to the global namespace.
// This will make it available at 
//      /sys/fs/bpf/tc/globals/<map name>
// Other programs can interact with it through 
// the BPF_OBJ_GET bpf command.
#define PIN_GLOBAL_NS 2
#define PIN_OBJECT_NS 1

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
};

struct cls_entry {
    uint32_t sph;
    unsigned char next_hop[ETH_ALEN];
}__attribute__((packed));

// Specific map structure used by tc and iproute2
// Extracted from iproute2 source code:
//      iproute2/include/bpf_elf.h
struct bpf_elf_map {
	__u32 type;
	__u32 size_key;
	__u32 size_value;
	__u32 max_elem;
	__u32 flags;
	__u32 id;
	__u32 pinning;
};

struct ip_5tuple {
    __u32 ip_src;
    __u32 ip_dst;
    __u16 sport;
    __u16 dport;
    __u8 proto;
}__attribute__((packed));

#endif /* COMMON_H_ */