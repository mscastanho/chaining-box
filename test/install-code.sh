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
#PWD="vagrant"
read -p 'Password:' -s PWD

for i in `seq 1 $NB_SFS`; do
    # VMs are configured with forwarded ports for SSH connections
    echo $PWD | ssh -p "$((2000 + $i))" vagrant@localhost "cd $SRC_DIR; make; echo $PWD | sudo -S ../test/load-bpf.sh $OBJ $IFACE"
done
