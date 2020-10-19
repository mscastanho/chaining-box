#!/usr/bin/env bash

IIFACE="$1"
EIFACE="$2"
[ $# -ne 2 ] && echo "Usage: $0 <iiface> <eiface>" && exit 1

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

FWKERN="${SCRIPT_DIR}/firewall_kern.o"
[ ! -f ${FWKERN} ] && echo "Could not find file $FWKERN" && exit 1

TCREDIR="${SCRIPT_DIR}/tc_bench01_redirect"
[ ! -f ${TCREDIR} ] && echo "Could not find redirection code $TCREDIR" && exit 1

FILL="${SCRIPT_DIR}/fill-map"
[ ! -f ${FILL} ] && echo "Could not find prog $FILL" && exit 1

tc qdisc add dev $IIFACE clsact
tc filter add dev $IIFACE ingress pref 1 handle 1 bpf da obj $FWKERN sec classifier
$TCREDIR --ingress $IIFACE --egress $EIFACE
$FILL rules.json
