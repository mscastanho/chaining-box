#!/usr/bin/env bash

host=$(hostname)

# sfc0 (classifier) 
if [ $host == "sfc0" ]; then

    bpftool map update pinned /sys/fs/bpf/tc/globals/cls_table \
    key hex 0a 0a 0a 0a 0a 0a 0a 0d 03 e8 07 d0 11 \
    value hex 00 00 01 ff 00 00 00 00 00 0b any

    bpftool map update pinned /sys/fs/bpf/tc/globals/src_mac \
    key hex 00 \
    value hex 00 00 00 00 00 0a any

elif [ $host == "sfc1" ]; then

    # sfc1
    bpftool map update pinned /sys/fs/bpf/tc/globals/fwd_table \
    key hex 00 00 01 fe \
    value hex 00 00 00 00 00 00 0c any

    bpftool map update pinned /sys/fs/bpf/tc/globals/src_mac \
    key hex 00 \
    value hex 00 00 00 00 00 0b any

elif [ $host == "sfc2" ]; then
    
    # sfc2
    bpftool map update pinned /sys/fs/bpf/tc/globals/fwd_table \
    key hex 00 00 01 fd \
    value hex 01 00 00 00 00 00 00 any

    bpftool map update pinned /sys/fs/bpf/tc/globals/src_mac \
    key hex 00 \
    value hex 00 00 00 00 00 0c any

else
   echo "No rules configured for host \"$host\""
fi
