#!/usr/bin/env bash

scriptdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
source $scriptdir/map-common.sh

if [ -z "$1" ] || [ -z "$2" ]; then
  echo "Usage: $0 <srcip> <dst mac> [swap]"
  exit 0
fi

srcip="$1"
dstmac="$2"

if [ -n "$3" ] && [ "$3" == "swap" ]; then
  swap="11"
else
  swap="10"
fi

key="00 00 00 00"
val="$(split_string $(ip_to_hex $srcip)) ${2//:/ } $swap"

bpftool map update pinned /sys/fs/bpf/tc/globals/infomap key hex $key value hex $val any
