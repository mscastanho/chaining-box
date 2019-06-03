#!/usr/bin/env bash

# Check if is root
if [ "$EUID" -ne 0 ]
  then echo "Please run as root."
  exit
fi

function replace_address {
    SUBNET=$1
    HOSTID=$2
    echo "$(echo $SUBNET | sed "s/\.[^\.]*$/.${HOSTID}/g" )"
}

TESTSUBNET="10.1.0.0" # Will use /24
IPOFFSET="10"
NSFS="2"

# Creates a namespace with an associated
# veth interface for network communication
function create_namespace {
    if [ -z "$1" ]; then
        echo "Namespace ID required"
        exit 1
    fi

    ID="$1"

    # Allow custom namespace name
    if [ -z "$2" ]; then
        NS="ns${ID}"
    else
        NS="$2"
    fi

    NSCMD="ip netns exec $NS"
    EXT="veth${ID}" # External iface
    INT="${EXT}i"   # Internal iface
    IPADDR="$(replace_address $TESTSUBNET $(($IPOFFSET + $ID)))"

    # Create namespaces
    ip netns add $NS

    # Create veth pair for communication
    ip link add dev $EXT type veth peer name $INT
    ip link set dev $INT netns $NS

    # Set IP address inside namespace
    $NSCMD ip addr add ${IPADDR}/24 dev $INT

    # Set interfaces up
    ip link set dev $EXT up 
    $NSCMD ip link set dev $INT up

    # Add external interface to Linux bridge
    if [ -n "$3" ]; then
        BRIDGE="$3"
        brctl addif $BRIDGE $EXT
    fi
}

function destroy_namespace {
    if [ -z "$1" ]; then
        echo "Namespace ID required"
        exit 1
    fi

    ID="$1"

    # Allow custom namespace name
    if [ -z "$2" ]; then
        NS="ns${ID}"
    else
        NS="$2"
    fi

    NSCMD="ip netns exec $NS"
    INT="veth${ID}i"

    $NSCMD ip link set dev $INT down
    $NSCMD ip link set dev $INT netns 1

    ip link del dev $INT

    ip netns del $NS
}

function create_infra {
    BR="br0"

    # Create and configure bridge
    brctl addbr $BR
    ip link set dev $BR up

    # Create namespaces for SFs
    for i in $(seq 1 $NSFS); do
        create_namespace $i sf$i $BR
    done

    create_namespace 0 src $BR
    create_namespace $((NSFS+1)) dst $BR

    read -n1 kbd

    # Destroy SF namespaces
    for i in $(seq 1 $NSFS); do
        destroy_namespace $i sf$i
    done

    destroy_namespace 0 src
    destroy_namespace $((NSFS+1)) dst

    ip link set dev $BR down
    brctl delbr $BR
}

# Main
create_infra