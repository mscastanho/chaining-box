#!/usr/bin/env bash

# Script to start and configure host for SFC
# using ChainingBox

if [ $# -lt 2 ]; then
    echo "Usage: $0 <node-type> <interface> [src-path]"
    echo "node-type: [ sf | cls ]"
    exit 1 
else
    node_type="$1"
    iface="$2"
fi

# Grab path to chaining-box from user or set default otherwise
if [ -n "$3" ]; then
    CBDIR="$3"
else
    CBDIR="/home/$USER/chaining-box"
fi

CBOBJ="sfc_stages_kern.o"

cd $CBDIR/src

# Compile code if needed
if [ ! -f "$CBOBJ" ]; then
    make
fi

# Load stages into the kernel
cd "$CBDIR/test"
bash load-bpf.sh $CBDIR/src/$CBOBJ $iface $node_type
