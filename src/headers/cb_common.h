#ifndef CB_COMMON_H_
#define CB_COMMON_H_

#include <linux/in.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>

#include <stdint.h>
#include <stdlib.h>

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#ifndef lock_xadd
# define lock_xadd(ptr, val)              \
   ((void)__sync_fetch_and_add(ptr, val))
#endif

#ifdef BPFMAPDEF /* Use bpf_map_def struct to declare map */
#define MAP(NAME,TYPE,KEYSZ,VALSZ,MAXE,PIN) \
    struct bpf_map_def SEC("maps") NAME = { \
      .type = TYPE, \
      .key_size = KEYSZ, \
      .value_size = VALSZ, \
      .max_entries = MAXE, \
    };
#else /* Use iproute2 map struct */
#define MAP(NAME,TYPE,KEYSZ,VALSZ,MAXE,PIN) \
    struct bpf_elf_map SEC("maps") NAME = { \
      .type = TYPE, \
      .size_key = KEYSZ, \
      .size_value = VALSZ, \
      .max_elem = MAXE, \
      .pinning = PIN, \
    };
#endif /* BPFMAPDEF */

enum srcmac_idx {
  INGRESS_MAC = 0,
  EGRESS_MAC,
};

/* Map to hold source MAC
 * This is the shared definition used by all programs.
 *  0: MAC of ingress interface
 *  1: MAC of egress interface */
#define SRCMACMAP() MAP(src_mac, BPF_MAP_TYPE_ARRAY, sizeof(__u32), \
    ETH_ALEN, 2, PIN_GLOBAL_NS);

#ifdef ENABLE_STATS

#define STATSMAP() MAP(prog_stats, BPF_MAP_TYPE_HASH, sizeof(__u8), \
    sizeof(struct stats), 1, PIN_NONE);

struct stats {
  uint32_t rx;
  uint32_t tx;
  uint32_t tx_other; /* Exited for another purpose */
  uint64_t lat_avg_sum;
  uint64_t init_ts;
};

#endif /* ENABLE_STATS */

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

#endif /* CB_COMMON_H_ */
