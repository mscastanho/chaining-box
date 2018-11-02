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

rawSocket=socket.socket(socket.AF_PACKET,socket.SOCK_RAW,socket.htons(0x0800))

# print 'Buffer size: ' + str(rawSocket.getsockopt(socket.SOL_SOCKET,socket.SO_RCVBUF)) 
# rawSocket.setsockopt(socket.SOL_SOCKET,socket.SO_RCVBUF,4096)
# print 'Buffer size: ' + str(rawSocket.getsockopt(socket.SOL_SOCKET,socket.SO_RCVBUF)) 

count=0

print "Sniffing..."
while True:
    receivedPacket=rawSocket.recv(65536)

    ipHeader=receivedPacket[14:34]
    ipHdr=struct.unpack("!12s4s4s",ipHeader)
    destinationIP=socket.inet_ntoa(ipHdr[2])
    sourceIP=socket.inet_ntoa(ipHdr[1])

    if sourceIP != "10.0.2.2":
        print "src: " + sourceIP + " dst: " + destinationIP
        count += 1   
   
# s.send(receivedPacket)