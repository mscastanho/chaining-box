#!/usr/bin/env bash

###### Check if DPDK vars are set ######

#export RTE_SDK=/usr/src/dpdk-18.05.1
#export RTE_TARGET=x86_64-native-linuxapp-gcc

: ${RTE_SDK:?"Please set 'RTE_SDK' before running this script."}
: ${RTE_TARGET:?"Please set 'RTE_TARGET' before running this script."}

npages="1024" # Number of hugepages to reserve

function configure_dpdk {
    ###### Setup hugepages ######
    huge_dir=/mnt/huge

    # Create hugepage dir if it doesn't exist
    if [ ! -d $huge_dir ]; then
        mkdir $huge_dir
    fi

    # Mount 1GB hugepages
    # echo $1 > /sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages
    # mount -t hugetlbfs -o pagesize=1G none $huge_dir

    # Mount 2MB hugepages
    echo $npages > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
    mount -t hugetlbfs -o pagesize=2M none $huge_dir

    ###### Insert DPDK driver module ######

    if ! lsmod | grep uio &> /dev/null ; then
        modprobe uio
    fi

    if ! lsmod | grep igb_uio &> /dev/null ; then
        insmod $RTE_SDK/x86_64-native-linuxapp-gcc/kmod/igb_uio.ko
    fi
}

configure_dpdk
