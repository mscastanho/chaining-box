#!/usr/bin/env bash

[ $# -lt 2 ] && echo "Usage: $0 <local-iface> <next-hop-mac> [tc-direction]" && exit 1

IFACE="$1"
NEXTHOPMAC="$2" # <---- This should be a MAC address. I trust you!
DIRECTION="$3"
SRCMAC_SPACES="$(ip link show $IFACE | grep ether | awk '{print $2}' | tr ':' ' ')"
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
# Load BPF classifier program
$DIR/load-bpf.sh $DIR/../src/build/sfc_classifier_kern.o $IFACE cls $DIRECTION

# 5tuple : 10.10.0.1:1000 -> 10.10.0.2:2000 UDP
# chain : SPI = 100 / SI = 255 -> MAC: NEXTHOPMAC
bpftool map update pinned /sys/fs/bpf/tc/globals/cls_table \
key hex 0a 0a 00 01 0a 0a 00 02 27 10 4e 20 11 \
value hex 00 00 64 ff ${NEXTHOPMAC//:/ } any

# 5tuple : 10.10.0.1:0 -> 10.10.0.2:0 ICMP
# chain : SPI = 100 / SI = 255 -> MAC: NEXTHOPMAC
bpftool map update pinned /sys/fs/bpf/tc/globals/cls_table \
key hex 0a 0a 00 01 0a 0a 00 02 00 00 00 00 01 \
value hex 00 00 64 ff ${NEXTHOPMAC//:/ } any

# Install source mac
bpftool map update pinned /sys/fs/bpf/tc/globals/src_mac \
key hex 01 00 00 00 \
value hex $SRCMAC_SPACES any
