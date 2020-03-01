#!/usr/bin/env bash

source common.sh

echo "Deleting old resources..."
sudo ovs-vsctl del-br cbox-br
pkill cb_manager
for c in $(list_sfs); do
  { 
    docker kill $c && docker rm $c
  } > /dev/null 2>&1
done

echo "Creating containers..."
sudo ${objdir}/cb_docker -c $chainscfg -d ../ -n ovs > $dockerlog 2>&1 || fail_exit

echo "Adding physical ifaces to OVS bridge"
sudo ovs-vsctl add-port cbox-br $phys0
sudo ovs-vsctl add-port cbox-br $phys1

echo "Starting manager..."
${objdir}/cb_manager $chainscfg > $managerlog 2>&1 &

echo "Generating PCAP files..."
sf1mac="$(get_if_addr sf1 eth1)"
tgen0mac="00:15:4d:13:81:7f" # Hardcoded from interfaces used by pktgen 
tgen1mac="00:15:4d:13:81:80"
${scriptsdir}/gen-nsh-pcap.py $tgen0mac $tgen1mac $tgen0mac $sf1mac

echo "Priming OVS' learning switch table..."
for c in $(list_sfs); do
  docker exec -it $c ping -q -c 1 -i 0.1 192.168.100.250 > /dev/null 2>&1
done

sleep 3

echo "Starting pktgen..."
sudo $pktgen $pktgen_args -s 0:nsh-traffic-1500.pcap
