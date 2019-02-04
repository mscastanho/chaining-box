#!/usr/bin/env python3
#
# chaining-box.py: Enable and configure SFC using eBPF infrastructure

from bcc import BPF
import pyroute2
import time
import sys
import ctypes as ct

flags = 0
def usage():
    print("Usage: {0} <in ifdev>".format(sys.argv[0]))
    print("e.g.: {0} eth0\n".format(sys.argv[0]))
    exit(1)

if len(sys.argv) != 2:
    usage()

in_if = sys.argv[1]

ip = pyroute2.IPRoute()
# TODO: get MAC address
# out_idx = ip.link_lookup(ifname=out_if)[0]

# load BPF program
b = BPF(src_file = "sfc_stages_kern.c")

# cls_table = b.get_table("cls_table")
# nsh_data = b.get_table("nsh_data")
# src_mac = b.get_data("src_mac")
# fwd_table = b.get_data("fwd_table")

# decap_fn = b.load_func("decap_nsh", BPF.XDP)
# fwd_fn = b.load_func("sfc_forwarding", BPF.SCHED_CLS)

# b.attach_xdp(in_if, in_fn, flags)
# b.attach_xdp(out_if, out_fn, flags)

# rxcnt = b.get_table("rxcnt")
# prev = 0
print("Printing redirected packets, hit CTRL+C to stop")
while 1:
    try:
        time.sleep(1)
    except KeyboardInterrupt:
        print("Removing filter from device")
        break

b.remove_xdp(in_if, flags)
b.remove_xdp(out_if, flags)