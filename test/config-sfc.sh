#!/usr/bin/env bash

# Script to start and configure host for SFC
# using ChainingBox

if [ -z "$1" ] || [ -z "$2" ]; then
    echo "Usage: $0 <node-type> <interface>"
    echo "node-type: [ sf | cls ]"
    exit 1 
fi

CBDIR="~/chaining-box"
CBOBJ="sfc_stages_kern.o"

node_type="$1"
iface="$2"

cd $CBDIR

# Compile code if needed
if [ ! -f "$CBOBJ" ]; then
    cd src
    make
    cd -
fi

# Load stages into the kernel
cd test
bash load-bpf.sh $CBDIR/src/$CBOBJ $node_type $iface

# Load hardcoded rules
bash hardcoded-rules.sh
