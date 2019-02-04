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


ipr = pyroute2.IPRoute()

iface = sys.argv[1]
iface_id = ipr.link_lookup(ifname=iface)[0]

# TODO: get MAC address
# out_idx = ip.link_lookup(ifname=out_if)[0]

# load BPF program
b = BPF(src_file = "sfc_stages_kern.c")

# cls_table = b.get_table("cls_table")
# nsh_data = b.get_table("nsh_data")
# src_mac = b.get_data("src_mac")
# fwd_table = b.get_data("fwd_table")

# Configure Decap stage
decap_fn = b.load_func("decap_nsh", BPF.XDP)
b.attach_xdp(iface, decap_fn, flags)

# Configure Forwarding stage
fwd_fn = b.load_func("sfc_forwarding", BPF.SCHED_CLS)

ipr.tc("add", "clsact", iface_id)
# TODO: Check 'parent' param
ipr.tc("add-filter", "bpf", iface_id, ":1", fd=fwd_fn.fd, name=fwd_fn.name,
    parent="ffff:fff3", classid=1, direct_action=True)

# rxcnt = b.get_table("rxcnt")
# prev = 0
print("Printing redirected packets, hit CTRL+C to stop")
while 1:
    try:
        time.sleep(1)
    except KeyboardInterrupt:
        print("Removing filter from device")
        break

# Cleanup
b.remove_xdp(iface, flags)
ipr.tc("del", "clsact", iface_id)