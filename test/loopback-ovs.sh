#!/usr/bin/env bash

if [ -z $1 ] ; then
    echo "Usage: $0 <iface>"
    exit 1
fi

# This script creates an OVS bridge that receives and forwards
# packets on the same interface. Basically a virtual wire

IFACE0="$1"
OVS_DIR=/usr/share/openvswitch
SCRIPTS_DIR="/usr/share/openvswitch/scripts"

pkill ovs

# Start OVS processes
$SCRIPTS_DIR/ovs-ctl start

ovs_get_iface_number () {
    ovs-vsctl get interface $1 ofport
}

BRIDGE=brloop
ovs-vsctl --may-exist add-br $BRIDGE -- set bridge $BRIDGE datapath_type=netdev
ovs-vsctl --may-exist add-port $BRIDGE $IFACE0

# Create loopback rule
ovs-ofctl del-flows $BRIDGE
ovs-ofctl add-flow $BRIDGE in_port=$(ovs_get_iface_number $IFACE0),actions=in_port