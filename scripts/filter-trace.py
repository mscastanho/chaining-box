#!/usr/bin/env python3

from scapy.all import rdpcap, PacketList, Ether, TCP, UDP, IP, wrpcap
import sys

if len(sys.argv) != 3:
    print("Usage: {} <in.pcap> <out.pcap>".format(sys.argv[0]))
    exit(1)

infile = sys.argv[1]
outfile = sys.argv[2]

pcap = rdpcap(infile)

filtered = PacketList([])

for p in pcap:
    if TCP in p or UDP in p:
        try:
            p[IP].dst = '10.10.0.2'
            p[IP].src = '10.10.0.1'
            del p[IP].len
            del p[IP].chksum
            p[Ether].src = '00:15:4d:13:81:7f'
            p[Ether].dst = '00:15:4d:13:81:80'
            filtered.append(Ether(p.build()))
        except Exception as e:
            print(e)
            continue

wrpcap(outfile, filtered)
