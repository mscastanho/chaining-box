#include <uapi/linux/bpf.h>
#include <uapi/linux/if_ether.h>
#include <uapi/linux/in.h>
#include <uapi/linux/ip.h>
#include <uapi/linux/tcp.h>
#include <uapi/linux/udp.h>
#include <uapi/linux/pkt_cls.h>

#include "bpf_helpers.h"
#include "common.h"
#include "nsh.h"

#define ADJ_STAGE 1
#define FWD_STAGE 2

struct bpf_elf_map SEC("maps") cls_table = {
    .type = BPF_MAP_TYPE_HASH,
    .size_key = sizeof(struct ip_5tuple),
    .size_value = sizeof(struct cls_entry), // Value is sph + MAC address
    .max_elem = 2048,
    .pinning = PIN_GLOBAL_NS,
};

struct bpf_elf_map SEC("maps") nsh_data = {
    .type = BPF_MAP_TYPE_HASH,
    .size_key = sizeof(struct ip_5tuple),
    .size_value = sizeof(struct nshhdr),
    .max_elem = 2048,
    .pinning = PIN_GLOBAL_NS,
};

// struct bpf_elf_map SEC("maps") not_found = {
//     .type = BPF_MAP_TYPE_HASH,
//     .size_key = sizeof(struct ip_5tuple),
//     .size_value = sizeof(struct nshhdr),
//     .max_elem = 2048,
//     .pinning = PIN_GLOBAL_NS,
// };

struct bpf_elf_map SEC("maps") fwd_table = {
    .type = BPF_MAP_TYPE_HASH,
    .size_key = sizeof(__u32), // Key is SPH (SPI + SI) -> 4 Bytes
    .size_value = sizeof(struct fwd_entry), // Value is MAC address + end byte
    .max_elem = 2048,
    .pinning = PIN_GLOBAL_NS,
};

// struct bpf_map_def SEC("maps") jmp_table = {
// 	.type = BPF_MAP_TYPE_PROG_ARRAY,
// 	.key_size = sizeof(u32),
// 	.value_size = sizeof(u32),
// 	.max_entries = 2,
// };

static inline int get_tuple(void* ip_data, void* data_end, struct ip_5tuple *t){
    struct iphdr *ip;
    struct udphdr *udp;
    struct tcphdr *tcp;

	ip = ip_data;
    if((void*) ip + sizeof(*ip) > data_end){
		printk("get_tuple(): Error accessing IP hdr\n");
		return -1;
	}

	t->ip_src = ip->saddr;
    t->ip_dst = ip->daddr;
	t->proto = ip->protocol;

    switch(ip->protocol){
        case IPPROTO_UDP:
            udp = ip_data + sizeof(*ip);
            if((void*) udp + sizeof(*udp) > data_end){
				printk("get_tuple(): Error accessing UDP hdr\n");
				return -1;
			}

			t->dport = udp->dest;
            t->sport = udp->source;
            break;
        case IPPROTO_TCP:
            tcp = ip_data + sizeof(*ip);
            if((void*) tcp + sizeof(*tcp) > data_end){
				printk("get_tuple(): Error accessing TCP hdr\n");
				return -1;
			}

	        t->dport = tcp->dest;
            t->sport = tcp->source;
            break;
		/* TODO: Add ICMP */
        default: ; 
    }

	return 0;
};

SEC("classify")
int classify_pkt(struct xdp_md *ctx)
{
	void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;
    struct nshhdr *nsh;
    struct cls_entry *cls;
    struct ethhdr *eth;
	struct iphdr *ip;
	struct ip_5tuple key = { };
	int ret;

	if(data + sizeof(struct ethhdr) + sizeof(struct iphdr) > data_end){
		printk("[CLASSIFY] Bounds check #1 failed.\n");
		return XDP_DROP;
	}
	
	eth = data;
	ip = (void*)eth + sizeof(struct ethhdr);

    if(ntohs(eth->h_proto) != ETH_P_IP){
		printk("[CLASSIFY] Not an IPv4 packet, dropping.\n");
		return XDP_DROP;
	}

	ret = get_tuple(ip,data_end,&key);
	if (ret < 0){
		printk("[CLASSIFY] get_tuple() failed: %d\n",ret);
		return XDP_DROP;
	}

	cls = bpf_map_lookup_elem(&cls_table,&key);
	if(cls == NULL){
		printk("[CLASSIFY] No rule for packet.\n");
		return XDP_DROP;
	}

	printk("[CLASSIFY] Matched packet flow.\n");

	// Insert outer Ethernet + NSH headers
	if(bpf_xdp_adjust_head(ctx, 0 - (int)(sizeof(struct nshhdr) + sizeof(struct ethhdr)))){
		printk("[CLASSIFY] Error creating room in packet.\n");
		return XDP_DROP;
	}

	if(data + sizeof(struct ethhdr) + sizeof(struct nshhdr) > data_end){
		printk("[CLASSIFY] Bounds check #2 failed.\n");
		return XDP_DROP;
	}

	eth = data; // Points to the new outermost Ethernet header
	nsh = (void*)eth + sizeof(struct ethhdr);

	memmove(eth->h_dest,cls->next_hop,ETH_ALEN);
	// TODO: Set proper eth->h_source
	eth->h_proto = htons(ETH_P_NSH);

    nsh->basic_info = ((uint16_t) 0) 	    |  
						NSH_TTL_DEFAULT 	| 
						NSH_BASE_LENGHT_MD_TYPE_2;               
    nsh->md_type 	= NSH_MD_TYPE_2;
    nsh->next_proto = NSH_NEXT_PROTO_ETHER;
	nsh->serv_path 	= htonl(cls->sph);

	printk("[CLASSIFY] NSH added.\n");

    return XDP_TX; 
}

SEC("decap")
int decap_nsh(struct xdp_md *ctx)
{
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;
    struct ethhdr *eth = data;
    struct nshhdr *nsh = (void*)eth + sizeof(*eth);
	struct iphdr *ip = (void*)nsh + sizeof(*nsh) + sizeof(*eth);
	struct ip_5tuple key = { }; // Verifier does not allow uninitialized keys
	int ret = 0;

    if(data + sizeof(*eth) > data_end)
    	return XDP_DROP;

	if(ntohs(eth->h_proto) != ETH_P_NSH)
		return XDP_PASS;
	
	nsh = (void*) eth + sizeof(*eth);

	// Check if we can access NSH
    if ((void*) nsh + sizeof(*nsh) > data_end)
        return XDP_DROP;
	
	// Check if next protocol is Ethernet
	if(nsh->next_proto != NSH_NEXT_PROTO_ETHER)
		return XDP_PASS;

	// Single check for Inner Ether + IP
	if((void*)nsh + sizeof(*eth) + sizeof(*ip) > data_end)
		return XDP_DROP;

	// Retrieve IP 5-tuple
	ret = get_tuple(ip,data_end,&key);
	if(ret){
		printk("[DECAP] get_tuple() failed.\n");
		return XDP_DROP;
	}

	// Save NSH data
	ret = bpf_map_update_elem(&nsh_data, &key, nsh, BPF_ANY);
	if(ret == 0){
		printk("[DECAP] Stored NSH in table.\n");
	}

	// Remove outer Ethernet + NSH headers
	if(bpf_xdp_adjust_head(ctx, (int)(sizeof(struct nshhdr) + sizeof(struct ethhdr))))
		return XDP_DROP;

	printk("[DECAP] Decapsulated packet.\n");

    return XDP_PASS;
}

/* 
 * This program will be responsible for adding room
 * for the NSH + Ethernet encapsulations. It will be
 * attached to the LWT hook, which only sees an L3
 * packet as context. At the end, the packet will have
 * newly allocated bytes between Ethernet and IP headers.
 * We will fill this gap and readjust the headers in the
 * next stage, where we'll have access to the entire pkt.
*/
SEC("encap")
int encap_nsh(struct __sk_buff *skb)
{
    void *data = (void *)(long)skb->data;
    void *data_end = (void *)(long)skb->data_end;
	struct ip_5tuple key = { };
	struct nshhdr *nsh;
	int ret;

	ret = get_tuple(data,data_end,&key);
	if (ret < 0) {
		printk("[ENCAP]: get_tuple() failed: %d\n", ret);
        return BPF_DROP;
	}

	nsh = bpf_map_lookup_elem(&nsh_data,&key);
	if(nsh == NULL){
		printk("[ENCAP]: Packet not previously decapped. Dropping.\n");
		return BPF_DROP;
	}

    // Add space for NSH before IP header
	// This space will be filled by the next stage
	ret = bpf_skb_change_head(skb, sizeof(struct nshhdr) + sizeof(struct ethhdr), 0);
	if (ret < 0) {
		printk("[ENCAP]: Failed to add extra room: %d\n", ret);
        return BPF_DROP;
	}

	// data = (void *)(long)skb->data;
    // data_end = (void *)(long)skb->data_end;

	// ip = data + 2*sizeof(struct ethhdr) + sizeof(nshhdr);

	// if(ip + sizeof(*ip) > data_end)
	// 	return BPF_DROP;

	// // This is a hack to help the adjust_nsh stage to know
	// // which packets to encapsulate
	// ip->version = IP_VERSION_UNASSIGNED;

	printk("[ENCAP]: Resized packet!\n");

	return BPF_OK;
}

// SEC("adjust")
int adjust_nsh(struct __sk_buff *skb)
{
	void *data = (void *)(long)skb->data;
    void* data_end = (void *)(long)skb->data_end;
	int ret;
	struct ip_5tuple key = { }; // Verifier does not allow uninitialized keys
	struct nshhdr *nsh, *prev_nsh;
	struct ethhdr *ieth, *oeth;
	struct iphdr *ip;
	// struct nshhdr nop2 = {0,0,0,0};
	
	// Bounds check to please the verifier
	if(data + 2*sizeof(struct ethhdr) + sizeof(struct nshhdr) > data_end){
		printk("[ADJUST]: Bounds check failed.\n");
		return BPF_DROP;
	}
	
	oeth = data;
	nsh = (void*) oeth + sizeof(struct ethhdr);
	ieth = (void*) nsh + sizeof(struct nshhdr);
	ip = (void*) ieth + sizeof(struct ethhdr);

	// Original Ethernet is outside, we have to move it inside
	// and clean the outer space
	__builtin_memmove(ieth,oeth,sizeof(struct ethhdr));
	__builtin_memset(oeth,0,sizeof(struct ethhdr));
	oeth->h_proto = ntohs(ETH_P_NSH);
	// oeth->h_dest and oeth->h_src will be set by fwd stage

	// Get IP 5-tuple
	ret = get_tuple(ip,data_end,&key);
	if(ret){
		printk("[ADJUST]: get_tuple() failed: %d\n", ret);
		return BPF_DROP;
	}

	// Re-add corresponding NSH
    prev_nsh = bpf_map_lookup_elem(&nsh_data,&key);
	if(prev_nsh == NULL){
		printk("[ADJUST] NSH header not found in table: (%04x,%04x,...\n",
		key.ip_src,key.ip_dst);
		printk("[ADJUST] ...%02x,%02x,%02x)\n",
		key.sport,key.dport,key.proto);
		// bpf_map_update_elem(&not_found, &key, &nop2, BPF_ANY);
		return BPF_DROP;
	}else{
		memcpy(nsh,prev_nsh,sizeof(struct nshhdr));
		if (ret < 0) {
			printk("[ADJUST] Failed to load NSH to packet: %d\n", ret);
			return BPF_DROP;
		}
		printk("[ADJUST] Re-added NSH header!\n");
	}

	// bpf_tail_call(skb, &jmp_table, FWD_STAGE);

	// With the tail call, this is unreachable code.
	// It's here just to be safe.
	return BPF_OK;

}

SEC("forward")
int sfc_forwarding(struct __sk_buff *skb)
{
    void *data     = (void *)(long)skb->data;
    void *data_end = (void *)(long)skb->data_end;

    struct ethhdr *eth = data;
    struct nshhdr *nsh;
    struct fwd_entry *next_hop;
    __u32 sph;
    __u32 si, spi;
	int ret;

	// TODO: The link between these two progs
	// 		 should be a tail call instead
	ret = adjust_nsh(skb);
	if(ret == BPF_DROP){
		return TC_ACT_SHOT;
	}

    if (data + sizeof(*eth) > data_end){
		printk("[FORWARD]: Bounds check failed.\n");
	    return TC_ACT_SHOT;
	}

    // Keep regular traffic working
    // if (eth->h_proto != htons(ETH_P_NSH))
	// 	return TC_ACT_OK;

    nsh = (void*) eth + sizeof(*eth);

    if ((void*) nsh + sizeof(*nsh) > data_end){
		printk("[FORWARD]: Bounds check failed.\n");     
	    return TC_ACT_SHOT;
	}
    
    // TODO: Check if endianness is correct!
    sph = ntohl(nsh->serv_path);
    spi = (sph & 0xFFFF00)>>8;
    si = (sph & 0xFF);

    next_hop = bpf_map_lookup_elem(&fwd_table,&sph);

    if(next_hop){ // Use likely() here?
        if(next_hop->flags & 0x1){
            printk("[FORWARD]: End of chain.\n");
            // Remove external encapsulation
        }else{
            printk("[FORWARD]: Updating next hop info\n");
            // Update MAC daddr
            // OBS: Source MAC should be updated
            __builtin_memmove(eth->h_dest,next_hop->address,ETH_ALEN);
            sph = ntohl(sph);
            sph--; // TODO: Fix this operation. Take care with endianness
            nsh->serv_path = htonl(sph);
		}
    }else{
		printk("[FORWARD]: No corresponding rule.\n");
        // No corresponding rule. Drop the packet.
        return TC_ACT_SHOT;
    }

	printk("[FORWARD]: Successfully forwarded the pkt!\n");
    return TC_ACT_OK;
}

char _license[] SEC("license") = "GPL";