#!/usr/bin/env bash

# Script to configure the environment to send/receive
# packets from SFC Domain

function create_ns {
    # Create namespaces
    ip netns add $NS0
    ip netns add $NS1

    # Attribute interfaces to corresponding namespace
    ip link set dev $I0 netns $NS0
    ip link set dev $I1 netns $NS1

    # Set IP addresses
    $NSCMD $NS0 ifconfig $I0 $IP0/24 up
    $NSCMD $NS1 ifconfig $I1 $IP1/24 up

    # Add static entries on ARP table
    $NSCMD $NS0 arp -s $IP1 $MAC1
    $NSCMD $NS1 arp -s $IP0 $MAC0
}

function destroy_ns {
   $NSCMD $NS0 ip link set dev $I0 netns 1
   $NSCMD $NS1 ip link set dev $I1 netns 1

   ip netns del $NS0
   ip netns del $NS1
}

if [  "$1" == "undo" ]; then
    del="true"
elif [  "$1" == "do" ]; then
    del="false"
else
    echo "Usage: $0 [do|undo]"
    exit 1
fi

# Namespaces
NSCMD="ip netns exec"
NS0="ns0"
NS1="ns1"

# Interface 0
I0="enp2s0np0"
IP0="192.168.0.1"

# Interface 1
I1="enp2s0np1"
IP1="192.168.0.2"

if [ $del == "true" ]; then
    destroy_ns
    exit 0
fi

MAC0="$(ip link show $I0 | grep link/ether | awk '{print $2}')"
MAC1="$(ip link show $I1 | grep link/ether | awk '{print $2}')"
create_ns
