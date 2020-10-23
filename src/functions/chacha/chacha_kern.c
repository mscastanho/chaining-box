

//------------------------------------------------------------------
// Includes.
//------------------------------------------------------------------
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <linux/bpf.h>
#include <linux/icmp.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/icmp.h>
#include <linux/pkt_cls.h>
#include <linux/udp.h>
#include <sys/time.h>

#include "bpf_endian.h"
#include "bpf_helpers.h"
#include "bpf_elf.h"
#include "cb_helpers.h"

#ifndef IP_FRAGMENTED
#define IP_FRAGMENTED 65343
#endif

#ifndef __inline
#define __inline inline __attribute__((always_inline))
#endif

#ifndef CHACHA_ROUNDS
#define CHACHA_ROUNDS 8
#endif

#ifndef memset
#define memset(dest, c_int, n) __builtin_memset((dest), (c_int), (n))
#endif

//------------------------------------------------------------------
// Types.
//------------------------------------------------------------------
// The chacha state context.
typedef struct
{
  uint32_t state[16];
  uint8_t rounds;
} chacha_ctx;

struct bpf_elf_map SEC("maps") chacha_egress = {
	.type = BPF_MAP_TYPE_ARRAY,
	.size_key = sizeof(int),
	.size_value = sizeof(int),
	.pinning = PIN_GLOBAL_NS,
	.max_elem = 1,
};

//------------------------------------------------------------------
// Macros.
//------------------------------------------------------------------
// Basic 32-bit operators.
#define ROTATE(v,c) ((uint32_t)((v) << (c)) | ((v) >> (32 - (c))))
#define XOR(v,w) ((v) ^ (w))
#define PLUS(v,w) ((uint32_t)((v) + (w)))
#define PLUSONE(v) (PLUS((v), 1))

// Little endian machine assumed (x86-64).
#define U32TO8_LITTLE(p, v) (((uint32_t*)(p))[0] = v)
#define U8TO32_LITTLE(p) (((uint32_t*)(p))[0])

#define QUARTERROUND(a, b, c, d) \
  x[a] = PLUS(x[a],x[b]); x[d] = ROTATE(XOR(x[d],x[a]),16); \
  x[c] = PLUS(x[c],x[d]); x[b] = ROTATE(XOR(x[b],x[c]),12); \
  x[a] = PLUS(x[a],x[b]); x[d] = ROTATE(XOR(x[d],x[a]), 8); \
  x[c] = PLUS(x[c],x[d]); x[b] = ROTATE(XOR(x[b],x[c]), 7);


//------------------------------------------------------------------
// Constants.
//------------------------------------------------------------------
//static const uint8_t SIGMA[16] = "expand 32-byte k";
static const uint8_t TAU[16]   = "expand 16-byte k";


//------------------------------------------------------------------
// doublerounds()
//
// Perform rounds/2 number of doublerounds.
// TODO: Change output format to 16 words.
//------------------------------------------------------------------
static __inline void doublerounds(uint8_t output[64], const uint32_t input[16], uint8_t rounds)
{
  uint32_t x[16];
  int32_t i;

  #pragma clang loop unroll (full)
  for (i = 0;i < 16;++i) {
    x[i] = input[i];
  }

  #pragma clang loop unroll (full)
  for (i = rounds ; i > 0 ; i -= 2) {
    QUARTERROUND( 0, 4, 8,12)
    QUARTERROUND( 1, 5, 9,13)
    QUARTERROUND( 2, 6,10,14)
    QUARTERROUND( 3, 7,11,15)

    QUARTERROUND( 0, 5,10,15)
    QUARTERROUND( 1, 6,11,12)
    QUARTERROUND( 2, 7, 8,13)
    QUARTERROUND( 3, 4, 9,14)
  }

  #pragma clang loop unroll (full)
  for (i = 0;i < 16;++i) {
    x[i] = PLUS(x[i], input[i]);
  }

  #pragma clang loop unroll (full)
  for (i = 0;i < 16;++i) {
    U32TO8_LITTLE(output + 4 * i, x[i]);
  }
}



//------------------------------------------------------------------
// init()
//
// Initializes the given cipher context with key, iv and constants.
// This also resets the block counter.
//------------------------------------------------------------------
static __inline void init(chacha_ctx *x, uint8_t *key, uint32_t keylen, uint8_t *iv)
{
  /*
  if (keylen == 256) {
    // 256 bit key.
    x->state[0]  = U8TO32_LITTLE(SIGMA + 0);
    x->state[1]  = U8TO32_LITTLE(SIGMA + 4);
    x->state[2]  = U8TO32_LITTLE(SIGMA + 8);
    x->state[3]  = U8TO32_LITTLE(SIGMA + 12);
    x->state[4]  = U8TO32_LITTLE(key + 0);
    x->state[5]  = U8TO32_LITTLE(key + 4);
    x->state[6]  = U8TO32_LITTLE(key + 8);
    x->state[7]  = U8TO32_LITTLE(key + 12);
    x->state[8]  = U8TO32_LITTLE(key + 16);
    x->state[9]  = U8TO32_LITTLE(key + 20);
    x->state[10] = U8TO32_LITTLE(key + 24);
    x->state[11] = U8TO32_LITTLE(key + 28);
  }

  else {
    // 128 bit key.
    x->state[0]  = U8TO32_LITTLE(TAU + 0);
    x->state[1]  = U8TO32_LITTLE(TAU + 4);
    x->state[2]  = U8TO32_LITTLE(TAU + 8);
    x->state[3]  = U8TO32_LITTLE(TAU + 12);
    x->state[4]  = U8TO32_LITTLE(key + 0);
    x->state[5]  = U8TO32_LITTLE(key + 4);
    x->state[6]  = U8TO32_LITTLE(key + 8);
    x->state[7]  = U8TO32_LITTLE(key + 12);
    x->state[8]  = U8TO32_LITTLE(key + 0);
    x->state[9]  = U8TO32_LITTLE(key + 4);
    x->state[10] = U8TO32_LITTLE(key + 8);
    x->state[11] = U8TO32_LITTLE(key + 12);
  }
  */
  // 128 bit key.
    x->state[0]  = U8TO32_LITTLE(TAU + 0);
    x->state[1]  = U8TO32_LITTLE(TAU + 4);
    x->state[2]  = U8TO32_LITTLE(TAU + 8);
    x->state[3]  = U8TO32_LITTLE(TAU + 12);
    x->state[4]  = U8TO32_LITTLE(key + 0);
    x->state[5]  = U8TO32_LITTLE(key + 4);
    x->state[6]  = U8TO32_LITTLE(key + 8);
    x->state[7]  = U8TO32_LITTLE(key + 12);
    x->state[8]  = U8TO32_LITTLE(key + 0);
    x->state[9]  = U8TO32_LITTLE(key + 4);
    x->state[10] = U8TO32_LITTLE(key + 8);
    x->state[11] = U8TO32_LITTLE(key + 12);

  // Reset block counter and add IV to state.
  x->state[12] = 0;
  x->state[13] = 0;
  x->state[14] = U8TO32_LITTLE(iv + 0);
  x->state[15] = U8TO32_LITTLE(iv + 4);
}


//------------------------------------------------------------------
// next()
//
// Given a pointer to the next block m of 64 cleartext bytes will
// use the given context to transform (encrypt/decrypt) the
// block. The result will be stored in c.
//------------------------------------------------------------------
static __inline void next(chacha_ctx *ctx, uint8_t *m, const uint8_t *m_end)
{
  // Temporary internal state x.
  uint8_t x[64];
  uint8_t i;

  // Update the internal state and increase the block counter.
  doublerounds(x, ctx->state, ctx->rounds);
  ctx->state[12] = PLUSONE(ctx->state[12]);
  if (!ctx->state[12]) {
    ctx->state[13] = PLUSONE(ctx->state[13]);
  }

  // XOR the input block with the new temporal state to
  // create the transformed block.
  /*
  if (m+64 > m_end) {
    return;
  }

  #pragma clang loop unroll (full)
  for (i = 0 ; i < 64 ; ++i) {
    //c[i] = m[i] ^ x[i];
    m[i] ^= x[i];
  }
  */
  uint64_t * m_pos;
  uint64_t * x_pos;
  #pragma clang loop unroll (full)
  for (i = 0 ; i < 8 ; ++i) {
    //c[i] = m[i] ^ x[i];
    m_pos = (uint64_t*)(m) + i;
    x_pos = (uint64_t*)(x) + i;
    *m_pos ^= *x_pos;
  }
}


//------------------------------------------------------------------
// init_ctx()
//
// Init a given ChaCha context by setting state to zero and
// setting the given number of rounds.
//------------------------------------------------------------------
static __inline void init_ctx(chacha_ctx *ctx, uint8_t rounds)
{
  uint8_t i;

  #pragma clang loop unroll (full)
  for (i = 0 ; i < 16 ; i++) {
    ctx->state[i] = 0;
  }
  ctx->rounds = rounds;
}

/*
static __inline int parse_tcp(struct __sk_buff *ctx, __u64 nf_off) {
	void *data_end = (void*)(long)ctx->data_end;
	void *data = (void*)(long)ctx->data;
	chacha_ctx cha_ctx;
	//struct tcphdr *tcph;
	uint8_t *pkt_data;
	__u32 tcp_off;
	//tcph = data + nf_off;
	// pkt_data is the tcp payload
	tcp_off = sizeof(struct tcphdr);
	nf_off += tcp_off;
	if (data + nf_off > data_end) {
		return TC_ACT_SHOT;
	}
	pkt_data = data + nf_off;
    return TC_ACT_OK;
}*/

static __always_inline bool parse_transport(void *data, __u64 off, void *data_end) {
	struct udphdr *tudp;
	tudp = data + off;
	if (tudp + 1 > data_end) {
		return false;
	}
	else {
		return true;
	}
}


static __inline int parse_ip(struct __sk_buff *ctx, __u64 nf_off) {
	void *data_end = (void*)(long)ctx->data_end;
	void *data = (void*)(long)ctx->data;
	__u16 *next_iph_u16;
	struct iphdr *iph;
	__u32 ip_off;
	__u32 csum = 0;
	__u8 ip_protocol;
	__u16 payload_len;
	int key = 0;

	iph = data + nf_off;
	if (iph + 1 > data_end) {
		return TC_ACT_SHOT;
	}
	if (iph->ihl != 5) {
		return TC_ACT_SHOT;
	}
      	payload_len = bpf_ntohs(iph->tot_len);
	ip_protocol = iph->protocol;
	ip_off = sizeof(struct iphdr);
	nf_off += ip_off;

	if (iph->frag_off & IP_FRAGMENTED) {
		return TC_ACT_SHOT;
	}

	cb_debug("[CHACHA] Inspecting protocol...\n");

	if (ip_protocol == IPPROTO_TCP) {
		if (!parse_transport(data, nf_off, data_end)) {
			return TC_ACT_SHOT;
		}
		else {
			nf_off += sizeof(struct tcphdr);
		}
	}
	else if (ip_protocol == IPPROTO_UDP) {
		if (!parse_transport(data, nf_off, data_end)) {
			return TC_ACT_SHOT;
		}
		else {
			nf_off += sizeof(struct udphdr);
		}
	}
	else if (ip_protocol == IPPROTO_ICMP) {
	  if (data + nf_off + sizeof(struct icmphdr)  > data_end)
	    return TC_ACT_SHOT;

	  struct icmphdr *icmp = data + nf_off;
	  nf_off += sizeof(struct icmphdr);

	  /* The ping tool adds some bytes representing a struct timeval to the
	     first bytes of the payload of the ping request packet to help it
	     calculate the round-trip time. Let's also skip these first bytes to
	     allow ping to work properly even in the presence of encryption. */
	  if(icmp->type == ICMP_ECHO) {
	    if(data + nf_off + sizeof(struct timeval) > data_end){
	      return TC_ACT_SHOT;
	    }else{
	      nf_off += sizeof(struct timeval);
	    }
	  }
	} else {
		return TC_ACT_OK;
	}

	/* extend the packet for ip header encapsulation */
	if (bpf_skb_adjust_room(ctx, (int)sizeof(struct iphdr), BPF_ADJ_ROOM_MAC, 0))
		return TC_ACT_SHOT;

	cb_debug("[CHACHA] Successfully added room for IP encap!\n");

	data = (void *)(long)ctx->data;
	data_end = (void *)(long)ctx->data_end;

	if (data + nf_off + sizeof(struct iphdr) > data_end)
	  return TC_ACT_SHOT;
	else
	  nf_off += sizeof(struct iphdr);

	/* relocate ethernet header to start of packet and set MACs */
	struct ethhdr *eth = data;
	struct iphdr *outer_iph = data + sizeof(*eth);
	struct iphdr *inner_iph = data + sizeof(*eth) + sizeof(*outer_iph);

	if (eth + 1 > data_end || outer_iph + 1 > data_end ||
	    inner_iph + 1 > data_end)
		return TC_ACT_SHOT;

	/* create an additional ip header for encapsulation */
	struct iphdr iph_tnl;
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

	cb_debug("[CHACHA] Starting payload encryption...\n");

	/* Encrypt using chacha algorithm */
	chacha_ctx cha_ctx;
	uint8_t *pkt_data = data + nf_off;
	/*
	uint8_t t_result[64] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  */
    uint8_t t_key[32] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    uint8_t t_iv[8]   = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    init_ctx(&cha_ctx, CHACHA_ROUNDS);
    init(&cha_ctx, t_key, 128, t_iv);

    // loop here
    int32_t i;
    #pragma clang loop unroll (full)
    for(i=0; i <= 23; i++) {
      if (pkt_data + 64 > data_end) {
        break;
      }
      //next(&cha_ctx, pkt_data, data_end, t_result);
      next(&cha_ctx, pkt_data, data_end);
      //memcpy(pkt_data, t_result, sizeof(t_result));
      pkt_data += (__u64)64;
    }

    /* Lookup what ifindex to redirect packets to */
    int *tx_port = bpf_map_lookup_elem(&chacha_egress, &key);
    if (!tx_port) {
      cb_debug("[CHACHA] Didn't find TX ifindex. Is map populated?\n");
      return TC_ACT_SHOT;
    }

    cb_debug("[CHACHA] Redirecting packet to iface %d\n", *tx_port);
    return bpf_redirect(*tx_port, 0);
}


SEC("chacha")
int cha(struct __sk_buff *ctx){
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;
    struct ethhdr *eth = data;
    __u32 eth_proto;
    __u32 nh_off;
    nh_off = sizeof(struct ethhdr);
    if (data + nh_off > data_end)
        return TC_ACT_OK;
    eth_proto = eth->h_proto;

    // the demo program only accepts IPv4 packets.
    if (eth_proto == bpf_htons(ETH_P_IP)) {
      return parse_ip(ctx, nh_off);
    }
    else {
      return TC_ACT_OK;
    }
}

char _license[] SEC("license") = "GPL";
