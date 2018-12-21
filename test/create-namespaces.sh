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

if [ -n "$1" ]; then
    if [ "$1" = "-d" ]; then
        del="true"
    else
        echo "Usage: $0 [-d]"
        exit 1
    fi
else
    del="false"
fi

# Namespaces
NSCMD="ip netns exec"
NS0="ns0"
NS1="ns1"

# Interface 0
I0="enp10s0f0"
IP0="10.10.10.10"
MAC0="$(ip link show $I0 | grep link/ether | awk '{print $2}')"

# Interface 1
I1="enp10s0f2"
IP1="10.10.10.11"
MAC1="$(ip link show $I1 | grep link/ether | awk '{print $2}')"

if [ del = "true" ]; then
    destroy_ns 
else
    create_ns
fi