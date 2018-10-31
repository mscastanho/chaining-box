#include <linux/if_ether.h>
#include <linux/ip.h>
// #include <arpa/inet.h>
// #include <stdlib.h>
#include <linux/in.h>
// #include <string.h>
// #include <stdio.h>
#include <uapi/linux/bpf.h>
#include <uapi/linux/if_ether.h>
#include <uapi/linux/if_packet.h>
#include <uapi/linux/pkt_cls.h>

#include "nsh.h"
#include "common.h"

#ifndef _BCC
#include "bpf_helpers.h"
// #include "bpf_endian.h"
#endif

#define FWD_TABLE_SZ 1024

#ifndef _BCC
struct bpf_elf_map SEC("maps") fwd_table = {
    .type = BPF_MAP_TYPE_HASH,
    .size_key = sizeof(__u32), // Key is SPH (SPI + SI) -> 4 Bytes
    .size_value = sizeof(struct fwd_entry), // Value is MAC address + end byte
    .max_elem = FWD_TABLE_SZ,
    .pinning = PIN_GLOBAL_NS,
};
#else
BPF_HASH(fwd_table, __u32, struct fwd_entry, FWD_TABLE_SZ);
#endif

/* Extracted from man tc-bpf(8)
* Supported 32 bit action return codes from the C program and their
* meanings ( linux/pkt_cls.h ):
*    TC_ACT_OK (0) , will terminate the packet processing pipeline and
*    allows the packet to proceed
*    TC_ACT_SHOT (2) , will terminate the packet processing pipeline
*    and drops the packet
*    TC_ACT_UNSPEC (-1) , will use the default action configured from
*    tc (similarly as returning -1 from a classifier)
*    TC_ACT_PIPE (3) , will iterate to the next action, if available
*    TC_ACT_RECLASSIFY (1) , will terminate the packet processing
*    pipeline and start classification from the beginning
*    else , everything else is an unspecified return code
*/

#ifndef _BCC
SEC("sfc_fwd")
#endif
int sfc_forwarding(struct __sk_buff *skb)
{
    void *data     = (void *)(long)skb->data;
    void *data_end = (void *)(long)skb->data_end;

    struct ethhdr *eth = data;
    struct nshhdr *nsh;
    struct fwd_entry *next_hop;
    __u32 sph;
    __u32 si, spi;

    if (data + sizeof(*eth) > data_end)
        return TC_ACT_SHOT;

    if (eth->h_proto != htons(ETH_P_NSH)){
        eth->h_proto = ETH_P_NSH; // Breaks regular traffic
		// return TC_ACT_OK;
    }
    nsh = (void*) eth + sizeof(*eth);

    if ((void*) nsh + sizeof(*nsh) > data_end)
        return TC_ACT_SHOT;
    
    // TODO: Check if endianness is correct!
    sph = ntohl(nsh->serv_path);
    spi = (sph & 0xFFFF00)>>8;
    si = (sph & 0xFF);


    // Outputs to /sys/kernel/debug/tracing/trace_pipe
    char fmt[] = "SPH from packet: %x\n";
    bpf_trace_printk(fmt, sizeof(fmt), sph);

    // BCC-style
    // bpf_trace_printk("SPH from packet: %x\n" , sph);

#ifndef _BCC
    next_hop = bpf_map_lookup_elem(&fwd_table,&sph);
#else
    // fwd_table.lookup()
#endif

    if(next_hop){ // Use likely() here?
        // bpf_debug("Found it!\n");
        if(next_hop->flags & 0x1){
            char fmt2[] = "It's a match!! SPH: %x\n";
            bpf_trace_printk(fmt2, sizeof(fmt2), sph);        
            // Remove external encapsulation
            // bpf_pop_header(pkt,0,ETH_HLEN + NSH_HLEN_NO_META);
        }else{
            // // Update MAC daddr
            // // OBS: Source MAC should be updated
            // memmove(eth->h_dest,next_hop->address,ETH_ALEN);
            // sph = ntohl(sph);
            // sph--;
            // // bpf_notify(2,next_hop->address,ETH_ALEN);
            // // bpf_notify(2,eth->h_dest,ETH_ALEN);
            // nsh->serv_path = htonl(sph);
            // // dump("=== NSH after ===",pkt->eth,pkt->metadata->length);
            // // bpf_notify(1,&nsh->serv_path,4);
        }
    }else{
        // No corresponding rule. Drop the packet.
        // This command will cause the processing pipeline
        // to be stop, thus dropping the packet. 
        // As a consequence, if using some tool like 
        // scapy to generate packets for testing,
        // one might see an error like:
        //      "socket.error: No buffer space available"
        // That's the expected behavior.
        return TC_ACT_SHOT;
    }

    return TC_ACT_OK;
}

#ifndef _BCC
char _license[] SEC("license") = "GPL";
#endif