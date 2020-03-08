#!/usr/bin/env bash

source common.sh
outfile="${logdir}/throughput.csv"
tmpfile="${logdir}/tput.res"

# Setup environment
cbox_deploy_ovs

echo "pkt size;trial number;tx packets;rx packets;dropped packets;duration(ms)" > ${outfile}

echo "Starting pktgen..."
sudo $pktgen $pktgen_args -f throughput.lua
