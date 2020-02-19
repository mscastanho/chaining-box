#!/usr/bin/env bash

IFACE="$1"
SRCMAC_SPACES="$(ip link show $IFACE | grep ether | awk '{print $2}' | tr ':' ' ')"
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
$DIR/load-bpf.sh $DIR/../src/build/sfc_classifier_kern.o $IFACE cls

# 5tuple : 10.10.0.1:1000 -> 10.10.0.2:200 UDP
# chain : SPI = 100 / SI = 255 -> MAC: 02:42:ac:11:00:02
bpftool map update pinned /sys/fs/bpf/tc/globals/cls_table \
key hex 0a 0a 00 01 0a 0a 00 02 27 10 4e 20 11 \
value hex 00 00 64 ff 02 42 ac 11 00 02  any

# 5tuple : 10.10.0.1:0 -> 10.10.0.2:0 ICMP
# chain : SPI = 100 / SI = 255 -> MAC: 02:42:ac:11:00:02
bpftool map update pinned /sys/fs/bpf/tc/globals/cls_table \
key hex 0a 0a 00 01 0a 0a 00 02 00 00 00 00 01 \
value hex 00 00 64 ff 02 42 ac 11 00 02  any

# bogus
bpftool map update pinned /sys/fs/bpf/tc/globals/src_mac \
key hex 01 00 00 00 \
value hex $SRCMAC_SPACES any

# With namespaces and MACVLAN
# bpftool map update pinned /sys/fs/bpf/tc/globals/cls_table key hex 0a 0a 00 01 0a 0a 00 02 00 00 00 00 01 val hex 00 00 64 ff 02 42 c0 a8 64 02 any
