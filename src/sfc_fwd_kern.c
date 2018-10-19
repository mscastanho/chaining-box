#include <linux/if_ether.h>
#include <linux/ip.h>
// #include <arpa/inet.h>
// #include <stdlib.h>
#include <linux/in.h>
// #include <string.h>
// #include <stdio.h>
#include <uapi/linux/bpf.h>
#include "nsh.h"
#include "bpf_endian.h"
#include "bpf_helpers.h"

struct fwd_entry {
    // Flags:
    // Bit 0 : is end of chain
    // Bits 1-7 : reserved
    uint8_t flags;
    unsigned char address[ETH_ALEN];
};

struct bpf_map_def SEC("maps") sfc_forwarding = {
    .type = BPF_MAP_TYPE_HASH,
    .key_size = NSH_SPH_LEN, // 4 bytes
    .value_size = sizeof(struct fwd_entry), // Value is MAC address + end byte
    .max_entries = 1024,
};

uint64_t prog(struct packet *pkt)
{
    struct ethhdr *eth = pkt->eth;
    struct nsh_hdr *nsh = (struct nsh_hdr *) ( ((char*) eth) + ETH_HLEN );
    struct fwd_entry *next_hop;
    uint32_t sph = nsh->serv_path;

    unsigned int spi = (sph & 0xFFFF00)>>8;
    unsigned int si = (sph & 0xFF);
    
    if(bpf_map_lookup_elem(&sfc_forwarding,&sph,&next_hop) != -1){
        // bpf_debug("Found it!\n");
        if(next_hop->flags & 0x1){
            // Remove external encapsulation
            // bpf_debug("End of chain...");
            bpf_pop_header(pkt,0,ETH_HLEN + NSH_HLEN_NO_META);
        }else{
            // Update destination MAC address`
            // OBS: Source MAC should be updated by underlying software
            // dump("=== pkt b4 ===",pkt->eth,pkt->metadata->length);
            memmove(eth->h_dest,next_hop->address,ETH_ALEN);
            sph = ntohl(sph);
            sph--;
            // bpf_notify(2,next_hop->address,ETH_ALEN);
            // bpf_notify(2,eth->h_dest,ETH_ALEN);
            nsh->serv_path = htonl(sph);
            // dump("=== NSH after ===",pkt->eth,pkt->metadata->length);
            // bpf_notify(1,&nsh->serv_path,4);
        }
    }else{
        // bpf_debug("Didnt find it =(\n");        
        // No corresponding rule.
        // We should drop the packet
        return DROP;
    }

    // dump("=== NSH packet ===",pkt->eth,pkt->metadata->length);

    // Send it out through the default port
    return 0;
}
char _license[] SEC("license") = "GPL";