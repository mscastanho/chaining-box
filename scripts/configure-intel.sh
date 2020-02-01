#!/usr/bin/env bash

###### Check if DPDK vars are set ######

function usage {
    echo "usage: $0 <do|undo>"
}

if [ -z $1 ]; then
    usage
    exit 1
fi

export RTE_SDK=/usr/src/dpdk-18.05.1
export RTE_TARGET=x86_64-native-linuxapp-gcc

command="$1"
PCI_ID0="0000:01:00.0" # Change this if PCI slot is physically change
PCI_ID1="0000:01:00.1" # 		    " 

function set_ifaces {
    ###### Bind interfaces to DPDK ######
    python3 $RTE_SDK/usertools/dpdk-devbind.py -b igb_uio $PCI_ID0
    python3 $RTE_SDK/usertools/dpdk-devbind.py -b igb_uio $PCI_ID1
}

function reset_ifaces { 
    ###### Unbind interfaces from DPDK ######
    python3 $RTE_SDK/usertools/dpdk-devbind.py -b i40e $PCI_ID0
    python3 $RTE_SDK/usertools/dpdk-devbind.py -b i40e $PCI_ID1
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
