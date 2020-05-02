#!/usr/bin/env bash

source map-common.sh

iface="bond0"

if [ -n "$1" ]; then 
  if [ "$1" == "--help" ]; then
    echo "Usage: $0 [if-name]"
    exit 0
  else
    iface="$1"
  fi
fi

macbytes="$(ip link show $iface | grep ether -m 1 | awk '{print $2}' | tr ':' ' ')"
key="01 00 00 00"
val="$macbytes"

bpftool map update pinned /sys/fs/bpf/xdp/globals/src_mac key hex $key value hex $val any
