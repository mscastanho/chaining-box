#!/usr/bin/env python3

from scapy.all import *
from scapy.contrib.nsh import NSH
import netifaces
import sys

if len(sys.argv) != 3:
    print('Usage: {} <src-mac> <dst-mac>'.format(sys.argv[0]))
    exit(1)

src_mac = sys.argv[1]
dst_mac = sys.argv[2]
#  out_src_mac = netifaces.ifaddresses(out_iface)[netifaces.AF_LINK][0]['addr']

# Generate input packets
i_eth = Ether(src=src_mac,dst=dst_mac)
ip = IP(src="10.10.0.1",dst="10.10.0.2")
udp = UDP(sport=1000,dport=2000)
o_eth = Ether(src=src_mac,dst='02:42:c0:a8:64:02')

headers = o_eth/NSH(mdtype=2, spi=100, si=255)/i_eth/ip/udp
final_size = 1500
payload = Raw(RandString(size=(final_size - len(headers))))

pkt = headers/payload

print("Beautiful packet!\n")
pkt.show()
print("Ugly packet!\n")
hexdump(pkt)

# Save to PCAP file
wrpcap('nsh-traffic.pcap',pkt)
