#!/usr/bin/env bash

scriptdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
source $scriptdir/map-common.sh

if [ -z "$1" ]; then
  echo "Usage: $0 <srcip> [swap]"
  exit 0
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
