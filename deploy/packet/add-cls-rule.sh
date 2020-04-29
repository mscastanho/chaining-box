#!/usr/bin/env bash

source map-common.sh

if [ -z "$1" ] || [ -z $2 ] || [ -z $3 ] || [ -z $4 ] || [ -z $5 ] || [ -z $6 ]; then
  echo "Usage: $0 <srcip> <dstip> <sport> <dport> <proto> <spi>"
fi

ipsrc="$1"
ipdst="$2"
sport="$3"
dport="$4"
proto="$5"
spi="$6"

macbytes="$(docker exec -it sf1 ip link show eth1 | grep ether | awk '{print $2}' | tr ':' ' ')"
key="$(split_string $(tuple_to_hex $ipsrc $ipdst $sport $dport $proto))"
val="$(split_string $(number_to_hex $spi 6)) ff $macbytes"


bpftool map update pinned /sys/fs/bpf/xdp/globals/cls_table key hex $key value hex $val any
