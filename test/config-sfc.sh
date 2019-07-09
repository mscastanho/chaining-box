#!/usr/bin/env bash

# Script to start and configure host for SFC
# using ChainingBox

if [ $# -lt 3 ]; then
    echo "Usage: $0 <node-type> <interface> <kernel-src-path> [src-path]"
    echo "node-type: [ sf | cls | redir]"
    exit 1 
else
    node_type="$1"
    iface="$2"
    KERNELDIR="$3"
fi

# Grab path to chaining-box from user or set default otherwise
if [ -n "$4" ]; then
    CBDIR="$4"
else
    CBDIR="/home/$USER/chaining-box"
fi

if [ $node_type == "cls" ]; then
    CBOBJ="src/sfc_classifier_kern.o"
elif [ $node_type == "redir" ]; then
    CBOBJ="test/tc_redirect/tc_bench01_redirect_kern.o"
else
    CBOBJ="src/sfc_stages_kern.o"
fi

# Compile code if needed
if [ ! -f "$CBOBJ" ]; then
    cd $CBDIR/src
    make KDIR=${KERNELDIR}
fi

# Load stages into the kernel
cd "$CBDIR/test"
bash load-bpf.sh $CBDIR/$CBOBJ $iface $node_type
