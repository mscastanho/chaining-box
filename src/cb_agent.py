#!/usr/bin/env python3

from bcc import BPF
import pyroute2
from pyroute2.netlink.exceptions import NetlinkError
import subprocess as sb
import sys
import argparse
import time

import cb_utils as cbu

class CBAgent():
    src_file = 'sfc_stages_kern'
    prog = None
    iface = None

    def __init__(self):
        self.compile()

    def compile(self):
        src = '%s.c' % self.src_file
        self.prog = BPF(src_file=src, cflags=['-w','-D _BCC'])

    def shutdown(self):
        ipr = pyroute2.IPRoute()

        # Cleanup
        if self.iface:
            self.prog.remove_xdp(self.iface, 0)
            ipr.tc('del', 'clsact', cbu.get_ifindex(self.iface))

    def load_dec_stage(self,iface):
        cbu.xdp_attach_bpf(self.prog,iface,'decap_nsh',0)

    def load_fwd_stage(self,iface):
        cbu.tc_attach_bpf(self.prog,iface,'egress','sfc_forwarding')

    def load_stages(self,iface):
        self.load_dec_stage(iface)
        self.load_fwd_stage(iface)
        self.iface = iface

def main(argv):
    parser = argparse.ArgumentParser(prog=argv[0],
    description='Starts ChainingBox Agent')

    parser.add_argument('-i', '--iface',
        help='Interface to load programs to', required=True)

    args = vars(parser.parse_args())

    iface = args['iface']

    m = CBAgent()
    m.load_stages(iface)

    while True:
        try:
            time.sleep(1)
        except KeyboardInterrupt:
            print("Removing filter from device")
            m.shutdown()
            break;

if __name__ == '__main__':
    main(sys.argv)