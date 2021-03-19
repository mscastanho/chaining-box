#!/usr/bin/env bash

scriptdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

###### Check if DPDK vars are set ######
[ "$EUID" -ne 0 ] && echo "Sorry, this script must be run as root..."

RTE_SDK=${scriptdir}/dpdk-19.11.6

[ ! -d ${RTE_SDK} ] && {
    echo -e "Couldn't find directory ${RTE_SDK}\nMaybe run 'make dpdk'?"
}

# Number of hugepages to reserve. If not set, reserve 3072
npages=${1:-3072}

echo "Allocating ${npages} hugepages..."

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
    insmod $RTE_SDK/build/kmod/igb_uio.ko
fi
