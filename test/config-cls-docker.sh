#!/usr/bin/env bash

/cb/test/load-bpf.sh /cb/src/build/sfc_classifier_kern.o eth0 cls

bpftool map update pinned /sys/fs/bpf/tc/globals/cls_table \
key hex ac 11 00 02 ac 11 00 03 27 10 4e 20 11 \
value hex 00 00 64 ff 02 42 ac 11 00 04 any

bpftool map update pinned /sys/fs/bpf/tc/globals/cls_table \
key hex ac 11 00 02 ac 11 00 03 00 00 00 00 01 \
value hex 00 00 64 ff 02 42 ac 11 00 04 any

bpftool map update pinned /sys/fs/bpf/tc/globals/src_mac \
key hex 01 00 00 00 \
value hex 02 42 ac 11 00 02 any
