#!/usr/bin/env bash

ovs_get_iface_number () {
    ovs-vsctl get interface $1 ofport
}

ovs_del_port () {
    # $1 = bridge ; $2 = portname
    ovs-vsctl --if-exists del-port $1 $2
}

if [ -z "$1" ]; then
    echo "Usage: $0 <number-of-VMs>"
    exit 1
fi

NB_VMS="$1"
SCRIPTS_DIR="/usr/local/share/openvswitch/scripts"

IFACE0="enp10s0f0"
MAC0="a0:36:9f:3d:f6:18"
ip link set dev $IFACE0 up

IFACE1="enp10s0f2"
MAC1="a0:36:9f:3d:f6:1a"
ip link set dev $IFACE1 up

# Kill previous OVS
pkill ovs

# Start OVS processes
$SCRIPTS_DIR/ovs-ctl start

# Create bridge
ovs-vsctl --no-wait --may-exist add-br brsfc

# Create ifaces

for i in $(seq 0 $((NB_VMS-1))); do
    ifname="if-sfc$i"
    ovs-vsctl --may-exist add-port brsfc $ifname -- set interface $ifname type=internal 
done

ovs-vsctl --may-exist add-port brsfc $IFACE0
ovs-vsctl --may-exist add-port brsfc $IFACE1

# Add flows

# L2 Learning
ovs-ofctl add-flow brsfc priority=0,actions=NORMAL

# ingress flows : send ingress packets to classifier
ovs-ofctl add-flow brsfc priority=3,in_port=$(ovs_get_iface_number $IFACE0),actions=output:$(ovs_get_iface_number if-sfc0)
#ovs-ofctl add-flow brsfc priority=3,in_port=$(ovs_get_iface_number $IFACE1),actions=output:$(ovs_get_iface_number if-sfc0)

# egress flows
ovs-ofctl add-flow brsfc priority=2,dl_dst=$MAC0,actions=output:$(ovs_get_iface_number $IFACE0)
ovs-ofctl add-flow brsfc priority=2,dl_dst=$MAC1,actions=output:$(ovs_get_iface_number $IFACE1)