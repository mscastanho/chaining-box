#!/usr/bin/env bash

source map-common.sh

if [ -z "$1" ]; then
  echo "Usage: $0 <srcip> [swap]"
fi

srcip="$1"

if [ -n "$2" ] && [ "$2" == "swap" ]; then
  swap="01"
else
  swap="00"
fi

key="00 00 00 00"
val="$(split_string $(ip_to_hex $srcip)) $swap"

bpftool map update pinned /sys/fs/bpf/tc/globals/infomap key hex $key value hex $val any
