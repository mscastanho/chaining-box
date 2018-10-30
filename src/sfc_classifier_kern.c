#include <linux/if_ether.h>
#include <stdlib.h>
#include <string.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/ip.h>
#include <stdio.h>
#include <linux/in.h>
#include "nsh.h"
#include "bpf_api.h"
#include "common.h"

struct class_res {
    uint32_t sph;
    unsigned char next_hop[ETH_ALEN];
}__attribute__((packed));

struct bpf_elf_map __section_maps sfc_classification = {
    .type = BPF_MAP_TYPE_HASH,
    .size_key = sizeof(struct ip_5tuple),
    .size_value = sizeof(struct class_res), // Value is sph + MAC address
    .max_elem = 1024,
};

// static inline void get_tuple(struct ethhdr *eth, struct ip_5tuple *t){
//     struct iphdr *ip = (struct iphdr *) (((char*) eth) + sizeof(struct ethhdr));
//     struct udphdr *udp;
//     struct tcphdr *tcp;

//     t->ip_src = ip->saddr;
//     t->ip_dst = ip->daddr;
//     t->proto = ip->protocol;

//     switch(t->proto){
//         case IPPROTO_UDP:
//             udp = (struct udphdr *) (((char*) ip) + sizeof(struct iphdr));
//             t->dport = udp->source;
//             t->sport = udp->dest;
//             break;
//         case IPPROTO_TCP:
//             tcp = (struct tcphdr *) (((char*) ip) + sizeof(struct iphdr));
//             t->dport = tcp->source;
//             t->sport = tcp->dest;
//             break;
//         default:
//             t->dport = 0;
//             t->sport = 0;
//     }
// }

/* Initializes the NSH header with default values */
static inline int nsh_init_header(struct nsh_hdr *nsh_header){
   
    if(nsh_header == NULL)
        return -1;
    
    /* Default value for basic_info | md_type | next_proto
     * is 0x0FC20203 
     */
    nsh_header->basic_info =  ((uint16_t) 0)    |  
                              NSH_TTL_DEFAULT   | 
                              NSH_BASE_LENGHT_MD_TYPE_2;               
    nsh_header->md_type = NSH_MD_TYPE_2;
    nsh_header->next_proto = NSH_NEXT_PROTO_ETHER;
    nsh_header->serv_path = 0;
    
    return 0;
}

__section_cls_entry
int cls_entry(struct __sk_buff *skb)
{
    struct nsh_hdr *nsh;
    char out_encap[ETH_HLEN + NSH_HLEN_NO_META];
    struct class_res *class;
    struct ethhdr *eth_encap;
    struct ip_5tuple t;

    // Only encapsulate IP packets coming from the outside
    if(ntohs(pkt->eth->h_proto) == ETH_P_IP && pkt->metadata->in_port == 0 ){
        // dump("=== In packet ===", pkt->eth, pkt->metadata->length);

        // sph = 0x000027FF;
        eth_encap = (struct ethhdr *) out_encap;
        nsh = (struct nsh_hdr *) ( ((char*) out_encap) + ETH_HLEN);
        get_tuple(pkt->eth,&t);

        // bpf_notify(1,&t,sizeof(struct ip_5tuple));

        // printf("IP pkt received!");
        if( bpf_map_lookup_elem(&sfc_classification,&t,&class) != -1 ){
            bpf_notify(1,&t,sizeof(struct ip_5tuple));
            // printf("Match!!");
            nsh_init_header(nsh);
            nsh->serv_path = class->sph;
            eth_encap->h_proto = htons(ETH_P_NSH);
            memmove(eth_encap->h_dest,class->next_hop,ETH_ALEN);

            // Add Ethernet + NSH encapsulation
            bpf_push_header(pkt,0,sizeof(out_encap),out_encap);
        }else{
            return DROP;
        }
        // dump("=== Out packet ===", pkt->eth, pkt->metadata->length);    
    }

    return pkt->metadata->in_port ^ 0x1; 

}
char _license[] SEC("license") = "GPL";
