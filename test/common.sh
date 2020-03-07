#!/usr/bin/env bash

# Test configuration
phys0="enp1s0f0"
phys1="enp1s0f1"
pktgen_args="-l 0-2 -n 4 -- -P -T -m 1.0,2.1"

if [ -z "$chainscfg" ]; then
  echo "Please set 'chainscfg' env var before calling the test scripts."
  exit 1
fi

# Var definitions
scriptdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cbdir="${scriptdir}/.."
objdir="${cbdir}/src/build"
scriptsdir="${cbdir}/scripts"
pktgen="/usr/src/pktgen-19.12.0/app/x86_64-native-linuxapp-gcc/pktgen"
logdir="/tmp/cb"
pcapdir="$logdir/pcap"
testdir="$cbdir/test"

[ -d $logdir ] && rm -rdf $logdir
mkdir -p $logdir $pcapdir

# Log files
dockerlog="${logdir}/docker.log"
managerlog="${logdir}/manager.log"

# Function definitions
function fail_exit {
  echo "Something went wrong with the last command. Exiting..."
  exit 1
}

function get_if_addr {
  container="$1"
  iface="$2"
  docker exec -it $container ip link show $iface | grep ether | awk '{print $2}'
}

function list_sfs {
  cat $chainscfg | grep -o -e 'sf[0-9]*' | sort | uniq
}

function cbox_deploy_ovs {
  echo "Deleting old resources..."
  sudo ovs-vsctl del-br cbox-br
  pkill cb_manager
  for c in "cls $(list_sfs)"; do
  {
    docker kill $c && docker rm $c
  } > /dev/null 2>&1
  done

  echo "Creating containers..."
  sudo ${objdir}/cb_docker -c $chainscfg -d ../ -n ovs > $dockerlog 2>&1 || fail_exit

  echo "Adding physical ifaces to OVS bridge..."
  sudo ovs-vsctl add-port cbox-br $phys0
  sudo ovs-vsctl add-port cbox-br $phys1

  echo "Setting up OVS flows..."
  # Steer all incoming traffic to the classifier container
  cls_int_ifindex=$(docker exec -it cls ip -o link show eth1 | cut -d':' -f1)
  cls_ext_name=$(ip link show | grep -o "[a-z0-9_]*@if${cls_int_ifindex}" | cut -d'@' -f1)
  sudo ovs-ofctl add-flow cbox-br in_port=${phys0},"actions=encap(nsh(md_type=2,tlv(0x1000,10,0x12345678))),set_field:0x64->nsh_spi,encap(ethernet),set_field:11:22:33:44:55:66->eth_dst,output:${cls_ext_name}"

  echo "Starting manager..."
  ${objdir}/cb_manager $chainscfg > $managerlog 2>&1 &

  echo "Priming OVS' learning switch table..."
  for c in $(list_sfs); do
    docker exec -it $c ping -q -c 1 -i 0.1 192.168.100.250 > /dev/null 2>&1
  done
}
