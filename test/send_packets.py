#!/usr/bin/env python

from scapy.all import *
from scapy.contrib.nsh import NSH
import netifaces
import sys

if len(sys.argv) <= 2:
    with_nsh = True
elif sys.argv[2] == "no-nsh":
    with_nsh = False
else:
    print "Usage: ./%s iface [no-nsh]" % sys.argv[0]
    exit(1)

out_iface = sys.argv[1]
out_src_mac = netifaces.ifaddresses(out_iface)[netifaces.AF_LINK][0]['addr']

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
i_ip = IP(src="10.1.0.1",dst="10.1.0.2")
i_tcp = TCP(sport=1000,dport=2000)

o_eth = Ether(src=out_src_mac,dst="00:00:00:00:00:02")

if with_nsh:
    in_pkt = o_eth/NSH(SPI=0x1,SI=0xFF)/i_eth/i_ip/i_tcp/http_resp
else:
    in_pkt = i_ip/i_tcp/http_resp

# print "Beautiful packet!\n"
# in_pkt.show()
# print "Ugly packet!\n"
# hexdump(in_pkt)

print out_iface
sendp(in_pkt,iface=out_iface)
# wrpcap('nsh.pcap',in_pkt)