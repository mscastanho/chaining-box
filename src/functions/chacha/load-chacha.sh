#!/usr/bin/env bash

IIFACE="$1"
EIFACE="$2"
[ $# -ne 2 ] && echo "Usage: $0 <iiface> <eiface>" && exit 1

CHAKERN="chacha.o"
[ ! -f $CHAKERN ] && echo "Could not find file $CHAKERN" && exit 1

TCREDIR="tc_bench01_redirect"
[ ! -f $TCREDIR ] && echo "Could not find redirection code $TCREDIR" && exit 1

tc qdisc add dev $IIFACE clsact
tc filter add dev $IIFACE ingress pref 1 handle 1 bpf da obj $CHAKERN sec chacha
$TCREDIR --ingress $IIFACE --egress $EIFACE
