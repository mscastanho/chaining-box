#!/usr/bin/env bash

IIFACE="$1"
EIFACE="$2"
SRCIP="$3"
[ $# -ne 3 ] && echo "Usage: $0 <iiface> <eiface>"

FWKERN="firewall_kern.o"
[ ! -f $FWKERN ] && echo "Could not find file $FWKERN" && exit 1


tc qdisc add dev $IIFACE clsact
tc filter add dev $IIFACE ingress pref 1 handle 1 bpf da obj classifier
../tc-redirect/tc_bench01_redirect --ingress $IIFACE --egress $EIFACE
