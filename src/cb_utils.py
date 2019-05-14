import pyroute2
from bcc import BPF

# Attach BPF program to TC
# @params:
#   iface: target interface
#   dir: flow direction (ingress or egress)
#   func: function name to be loaded
def tc_attach_bpf(prog, iface, dir, func):
    fn = prog.load_func(func, BPF.SCHED_CLS)
    # Get handle to iproute subsystem
    ipr = pyroute2.IPRoute()
    idx = get_ifindex(iface)

    try:
        ipr.tc('add', 'clsact', idx)
    except:
        print('=> clsact already exists')

    ipr.tc("add-filter", "bpf", idx, ":1", fd=fn.fd, name=fn.name,
            parent="ffff:fff2", classid=1, direct_action=True)

# Attach BPF program to XDP layer
# @params:
#   prog: BPF program object
#   iface: target interface
#   func: function name to be loaded
#   flags: optional XDP flags
def xdp_attach_bpf(prog, iface, func, flags):
    fn = prog.load_func(func, BPF.XDP)
    prog.attach_xdp(iface, fn, flags)

# Retusn ifindex for given interface
# @params:
#   iface: interface name
# @return:
#   corresponding ifindex
def get_ifindex(iface):
    with pyroute2.NDB() as ndb:
        return ndb.interfaces[iface]['index']