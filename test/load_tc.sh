#!/usr/bin/env bash

if [ -z $1 ] || [ -z $2 ] || [ -z $3 ]; then
    echo "Usage: $0 <bpf_code.o> <section> <iface>"
    exit 1
fi

bpf_obj="$1"
bpf_section="$2"
qdisc_name="clsact"
iface="$3"
direction="egress"

# bpf_path="$1"
# bpf_obj="$2"
# bpf_section="$3"
# qdisc_name="$4"
# iface="$5"
# direction="$6"

# Check if qdisc exists already. If not, create it
if [ -z "$(tc qdisc list | grep $qdisc_name)" ]; then
    tc qdisc add dev $iface clsact
    echo "=> qdisc created"
else
    echo "=> qdisc already exists"
fi

# Unload BPF code if already loaded
if [ -n "$(tc filter show dev $iface $direction)" ]; then
    tc filter del dev $iface $direction
    echo "=> BPF code unloaded"
fi

# (Re)load BPF code
tc filter add dev $iface $direction bpf da obj $bpf_obj \
    sec $bpf_section && echo "=> BPF code loaded"