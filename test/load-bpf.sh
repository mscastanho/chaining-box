#!/usr/bin/env bash

if [ -z $1 ] || [ -z $2 ]; then
    echo "Usage: $0 <bpf_code.o> <iface>"
    exit 1
fi

function load_bpf {
    BPFOBJ="$1"
    DEV="$2"

    # Load decap XDP code
    ip -force link set dev $DEV xdp \
        obj $BPFOBJ sec decap

    # Load encap code
    # Headroom = sizeof(Ethernet) + sizeof(NSH) = 14 + 8 = 22
    # ip route add $IPVETH1/32 encap bpf headroom 22 \
    #     xmit obj $BPFOBJ section encap dev $VETH0
    # ip route add 0.0.0.0/0 encap bpf headroom 22 \
    #     xmit obj $BPFOBJ section encap dev $VETH2

    # Load adjust + forward codes
    tc qdisc add dev $DEV clsact 2> /dev/null
    tc filter del dev $DEV egress 2> /dev/null
    tc filter add dev $DEV egress bpf da obj $BPFOBJ \
        sec forward
}

PROG="$1"
IFACE="$2"

load_bpf $PROG $IFACE