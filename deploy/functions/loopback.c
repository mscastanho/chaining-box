#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>             // close()
#include <errno.h>
#include <signal.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>    // struct sockaddr_ll
#include <arpa/inet.h>          // htons() inter_ntop()
#include <net/if.h>             // if_nametoindex()
#include <netinet/in.h>

#include <linux/ip.h>

#define MAX_PKT_SIZE 4096

int rawsock;
long pkt_count;

void onexit(int signum) {
    printf("\nForwarded %ld packets\nExiting...\n", pkt_count);
    close(rawsock);
	exit(0);
}

int main (int argc, char **argv) {

    int ifindex = 0;
    char packet[MAX_PKT_SIZE];
    ssize_t recbytes = 0;
    struct sockaddr_ll skaddr;
    struct ethhdr *eth;
    struct iphdr *ip;
    int filterip = 0;

    // Argument parsing
    if(argc < 3){
        printf("Usage: %s <iface-name> <src-ip-filter>\n",argv[0]);
        exit(1);   
    }

    ifindex = if_nametoindex(argv[1]);
    if(ifindex == 0){
        fprintf(stderr,"Interface doesn't exist. Maybe wrong? Given: %s\n", argv[1]);
        exit(1);
    }

    if(inet_pton(AF_INET, argv[2], &filterip) == -1){
        fprintf(stderr,"Failed to parse IP. Given: %s\n", argv[2]);
        exit(1);
    }

    // Initialization
	pkt_count = 0;
	signal(SIGINT, onexit);
    memset(packet,0,MAX_PKT_SIZE);

    rawsock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if(rawsock == -1){
        switch(errno){
            case EPERM:
                fprintf(stderr, "Please, run as sudo.\n");
                break;
            default:
                fprintf(stderr,"ERROR: Failed to open raw socket. errno = %d\n", errno);
        }

        exit(1);
    }

    skaddr.sll_family = AF_PACKET;
    skaddr.sll_protocol = htons(ETH_P_ALL);
    skaddr.sll_ifindex = ifindex;

    if(bind(rawsock, (struct sockaddr*)&skaddr, sizeof(skaddr)) == -1){
        fprintf(stderr,"Failed to bind socket to interface %s\n", argv[1]);
        exit(1);
    }

    char ipstr[INET_ADDRSTRLEN];

    // Main loop
    while(1){
        recbytes = recv(rawsock, packet, MAX_PKT_SIZE, 0);
        eth = (struct ethhdr *) packet;

        // Process only IPv4 packets
        if(eth->h_proto == htons(ETH_P_IP)){
            ip = (struct iphdr *) ((char*)eth + sizeof(struct ethhdr));
            
            // Only resend packets source IP == filterip
            if(ip->saddr == filterip){ 
    			pkt_count++;
			    send(rawsock, packet, recbytes, 0);
			}
            //inet_ntop(AF_INET, &(ip->saddr), ipstr, INET_ADDRSTRLEN);
            // printf("Received IP packet! saddr = %s%s", 
            //          ipstr, ip->saddr == filterip ? " <- Match!\n" : "\n");
        }
    }

    close(rawsock);

    return 0;
}
