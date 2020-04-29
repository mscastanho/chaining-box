#!/usr/bin/env bash

if [ -z $1 ] || [ -z $2 ] || [ -z $3 ]; then
    echo "Usage: $0 <bpf_code.o> <iface> <type>"
    exit 1
fi

# Allowed 'type' values:
#   cls    : classifier
#   sf     : service function
#   redir  : tc_redirect

# Script dir
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

function load_sf {
    BPFOBJ="$1"
    DEV="$2"

    # Load decap XDP code
    ip -force link set dev $DEV xdp \
        obj $BPFOBJ sec xdp/decap

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
        sec action/forward
}

function load_redirect {
    BPFOBJ="$1"
    DEV="$2"

    # Load tc_redirect program
    tc filter add dev $DEV ingress prio 1 handle 1 \
        bpf da obj $BPFOBJ sec ingress_redirect

    # Load redirect code
    tc qdisc add dev $DEV clsact 2> /dev/null
    tc filter del dev $DEV ingress 2> /dev/null
    tc filter replace dev $DEV ingress prio 1 handle 1 bpf da obj $BPFOBJ \
        sec ingress_redirect
}

function load_classifier {
    BPFOBJ="$1"
    DEV="$2"
    DIR="$3"
    [ -z $DIR ] && DIR="egress"
    # Load decap XDP code
    #ip -force link set dev $DEV xdp \
    #    obj $BPFOBJ sec classify

    tc qdisc add dev $DEV clsact 2> /dev/null
    tc filter del dev $DEV $DIR 2> /dev/null
    tc filter add dev $DEV $DIR bpf da obj $BPFOBJ \
        sec classify

}

PROG="$1"
IFACE="$2"
TYPE="$3"

if [ $TYPE = "sf" ]; then
    load_sf $PROG $IFACE
elif [ $TYPE = "cls" ]; then
    DIR="$4"
    load_classifier $PROG $IFACE $DIR
elif [ $TYPE = "redir" ]; then
    load_redirect $PROG $IFACE
else
    echo "Error: Unknown type $TYPE"
    exit 1
fi
