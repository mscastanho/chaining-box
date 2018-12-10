#!/usr/bin/env bash

# This script configures two static chains,
# one for each direction of communication of
# a specific flow.

if [ -z "$1" ]; then
    echo "Usage: $0 <number-of-SFs>"
    exit 1
fi

NB_SFS="$1"
SRC_DIR="/home/vagrant/dsfc/src"
OBJ="sfc_stages_kern.o"
IFACE="eth1"
for i in `seq 1 $NB_SFS`; do
    # VMs are configured with forwarded ports for SSH connections
    ssh -t -p "$((2000 + $i))" vagrant@localhost "sudo su; cd $SRC_DIR; make; ../test/load-bpf $OBJ $IFACE"
done
