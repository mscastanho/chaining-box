#!/usr/bin/env python

import struct
import sys,os
import socket
import binascii
import signal

def signal_handler(signal, frame):
    print "\n\n" + str(count) + " packets received"
    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)

if len(sys.argv) != 3:
    print "Usage: %s <interface> <flow src IP>" % sys.argv[0]
    exit(1)

iface = sys.argv[1]
ipsrc = sys.argv[2]

rawSocket=socket.socket(socket.AF_PACKET,socket.SOCK_RAW,socket.htons(0x0800))
rawSocket.bind((iface,0))

count=0
"Sniffing..."

while True:
    receivedPacket=rawSocket.recv(65536)

    ipHeader=receivedPacket[14:34]
    ipHdr=struct.unpack("!12s4s4s",ipHeader)
    destinationIP=socket.inet_ntoa(ipHdr[2])
    sourceIP=socket.inet_ntoa(ipHdr[1])

    if sourceIP != ipsrc:
        #print "src: " + sourceIP + " dst: " + destinationIP
        continue

    try:
        rawSocket.send(receivedPacket)
    except Exception as e:
        pass
    else:
        count+=1
