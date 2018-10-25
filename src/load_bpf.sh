#!/usr/bin/env bash

bpf_path="/home/vagrant/prototype-kernel/kernel/samples/bpf"
bpf_obj="sfc_fwd_kern.o"
bpf_section="sfc_fwd"
qdisc_name="clsact"
iface="enp0s8"
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
tc filter add dev $iface $direction bpf da obj $bpf_path/$bpf_obj \
    sec $bpf_section && echo "=> BPF code loaded"