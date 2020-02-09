#!/usr/bin/env bash

###### Check if DPDK vars are set ######

function usage {
    echo "usage: $0 <do|undo>"
}

if [ -z $1 ]; then
    usage
    exit 1
fi

export RTE_SDK=/usr/src/dpdk-19.11
export RTE_TARGET=x86_64-native-linuxapp-gcc

command="$1"
PCI_ID0="0000:02:00.0" # Change this if PCI slot is physically changed

function set_ifaces {
    ###### Bind interfaces to DPDK ######
    python3 $RTE_SDK/usertools/dpdk-devbind.py -b igb_uio $PCI_ID0
}

function reset_ifaces { 
    ###### Unbind interfaces from DPDK ######
    python3 $RTE_SDK/usertools/dpdk-devbind.py -b nfp $PCI_ID0
}

if [ $command == "do" ]; then
    set_ifaces
elif [ $command == "undo" ]; then
    reset_ifaces
else
   echo "ERROR: unknown command '$command'"
   echo
   usage
fi
