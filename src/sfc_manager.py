#!/usr/bin/env python3
from bcc import BPF
from pyroute2 import IPRoute
from pyroute2.netlink.exceptions import NetlinkError
import subprocess as sb
import os

class SFCManager():
    sfc_decap_file = "sfc_decap_kern"
    sfc_encap_file = "sfc_encap_kern"
    sfc_fwd_file = "sfc_fwd_kern"
    ipr = IPRoute()

    def __init__(self):
        pass
    
    def load_fwd_stage(self):
        src = "%s.c" % self.sfc_fwd_file
        b = BPF(src_file=src, cflags=["-D _BCC"])
        print b
        fn = b.load_func("sfc_forwarding", BPF.SCHED_CLS)
        self.tc_load_bpf("enp0s8","egress",fn)
    
    # Function to load BPF code to TC filter
    # @params:
    #   iface: target interface
    #   dir: flow direction (ingress or egress)
    #   sec: name of obj file section to be loaded
    def tc_load_bpf(self, iface, dir, func):
        # sb.call(["./load_bpf.sh","./",""])
        idx = self.ipr.link_lookup(ifname=iface)[0]
        try:
            self.ipr.tc("add", "clsact", idx)
        except NetlinkError:
            print "=> clsact already exists"
        
        # Not sure about :1, 1: and classid
        self.ipr.tc("add-filter", "bpf", idx, ":1", fd=func.fd, name=func.name, parent="1:", classid=1, direct_action=True)
        
        # ipr.tc("filter")
        # help(ipr.tc("modules"))


if __name__ == "__main__":
    m = SFCManager()
    m.load_fwd_stage()
