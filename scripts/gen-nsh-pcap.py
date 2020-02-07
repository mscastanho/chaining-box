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

html  = """
<html>
<head>
    <title>Messages from the SFs</title>
</head>
<body>
</body>
</html>
"""

http_resp  = "HTTP/1.1 200 OK\r\n"
http_resp += "Server: exampleServer\r\n"
http_resp += "Content-Length: " + str(len(html)) + "\r\n"
http_resp += "\r\n"
http_resp += html

# Generate input packets
i_eth = Ether(src="AA:BB:CC:DD:EE:FF",dst="AA:AA:AA:AA:AA:AA")
ip = IP(src="10.10.0.1",dst="10.10.0.2")
udp = UDP(sport=1000,dport=2000)
o_eth = Ether(src=src_mac,dst=dst_mac)

pkt = o_eth/NSH(mdtype=2, spi=0x1, si=0xFF)/i_eth/ip/udp/http_resp

print("Beautiful packet!\n")
pkt.show()
print("Ugly packet!\n")
hexdump(pkt)

# Save to PCAP file
wrpcap('nsh-traffic.pcap',pkt)
