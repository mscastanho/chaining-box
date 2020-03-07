#!/usr/bin/env bash

source common.sh
outfile="${logdir}/throughput.csv"
tmpfile="${logdir}/tput.res"

echo "Generating PCAP files..."
sf1mac="$(get_if_addr sf1 eth1)"
tgen0mac="00:15:4d:13:81:7f" # Hardcoded from interfaces used by pktgen
tgen1mac="00:15:4d:13:81:80"
${scriptsdir}/gen-nsh-pcap.py $tgen0mac $tgen1mac $tgen0mac $sf1mac

# Setup environment
cbox_deploy_ovs

echo "pkt size;trial number;tx packets;rx packets;dropped packets;duration(ms)" > ${outfile}

echo "Starting pktgen..."
for pktsize in 64 128 256 512 1024 1280 1500; do
  sudo $pktgen $pktgen_args -f throughput.lua -s 0:${pcapdir}/nsh-traffic-${pktsize}.pcap
  for line in $(tail -n+2 ${tmpfile}); do
    echo "${pktsize};${line}" >> ${outfile}
  done
done
