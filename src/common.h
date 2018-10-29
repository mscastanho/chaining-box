#ifndef COMMON_H_
#define COMMON_H_

#include <linux/if_ether.h>

struct fwd_entry {
    // Flags:
    // Bit 0 : is end of chain
    // Bits 1-7 : reserved
    __u8 flags;
    unsigned char address[ETH_ALEN];
};

// Pins a map to the global namespace.
// This will make it available at 
//      /sys/fs/bpf/tc/globals/<map name>
// Other programs can interact with it through 
// the BPF_OBJ_GET bpf command.
#define PIN_GLOBAL_NS 2

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

#endif /* COMMON_H_ */