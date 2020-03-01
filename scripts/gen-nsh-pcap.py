#!/usr/bin/env python3

from scapy.all import *
from scapy.contrib.nsh import NSH
import netifaces
import sys

if len(sys.argv) != 5:
    print('Usage: {} <i-src-mac> <i-dst-mac> <o-src-mac> <o-dst-mac>'.format(sys.argv[0]))
    exit(1)

i_src_mac = sys.argv[1]
i_dst_mac = sys.argv[2]
o_src_mac = sys.argv[3]
o_dst_mac = sys.argv[4]

# Generate input packets
i_eth = Ether(src=i_src_mac,dst=i_dst_mac)
ip = IP(src="10.10.0.1",dst="10.10.0.2")
udp = UDP(sport=1000,dport=2000)
o_eth = Ether(src=o_src_mac,dst=o_dst_mac)

headers = o_eth/NSH(mdtype=2, spi=100, si=255)/i_eth/ip/udp

for final_size in [64, 250, 500, 1000, 1500]:
    payload = Raw(RandString(size=(final_size - len(headers))))
    pkt = headers/payload

    #  print("Beautiful packet!\n")
    #  pkt.show()
    #  print("Ugly packet!\n")
    #  hexdump(pkt)

    # Save to PCAP file
    wrpcap('nsh-traffic-{}.pcap'.format(final_size), pkt)
