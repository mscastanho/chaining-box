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
NSCMD="ip netns exec"
NSPREFIX="ns"
NSFS="2"

# Creates a namespace with an associated
# veth interface for network communication
# create_namespace <id> [bridge]
function create_namespace {
    if [ -z "$1" ]; then
        echo "Namespace ID required"
        exit 1
    fi

    ID="$1"

    # Allow custom namespace name
    NS="${NSPREFIX}${ID}"

    EXT="veth${ID}" # External iface
    INT="${EXT}i"   # Internal iface
    IPADDR="$(replace_address $TESTSUBNET $(($IPOFFSET + $ID)))"

    # Create namespaces
    ip netns add $NS

    # Create veth pair for communication
    ip link add dev $EXT type veth peer name $INT
    ip link set dev $INT netns $NS

    # Set IP address inside namespace
    $NSCMD $NS ip addr add ${IPADDR}/24 dev $INT

    # Set interfaces up
    ip link set dev $EXT up
    $NSCMD $NS ip link set dev $INT up

    # Add external interface to Linux bridge
    if [ -n "$2" ]; then
        BRIDGE="$2"
        brctl addif $BRIDGE $EXT
    fi
}

function destroy_namespace {
    if [ -z "$1" ]; then
        echo "Namespace ID required"
        exit 1
    fi

    ID="$1"
    NS="${NSPREFIX}${ID}"

    INT="veth${ID}i"

    $NSCMD $NS ip link set dev $INT down
    $NSCMD $NS ip link set dev $INT netns 1

    ip link del dev $INT

    ip netns del $NS
}

# create_infra <#-namespaces>
function create_infra {
    BR="br0"
    NB_NS="$1"

    # Create and configure bridge
    brctl addbr $BR
    ip link set dev $BR up

    # Create namespaces
    for i in $(seq 0 $((NB_NS-1)) ); do
        create_namespace $i $BR
    done
}

# destroy_infra <#-namespaces>
function destroy_infra {
    BR="br0"
    NB_NS="$1"

    # Destroy namespaces
    for i in $(seq 0 $((NB_NS-1)) ); do
        destroy_namespace $i
    done

    # Destroy bridge
    ip link set dev $BR down
    brctl delbr $BR
}

function test_classifier {
    # Create sender and receiver namespaces
    create_infra 2

    # Load program
    $NSCMD ns0 bash ./load-bpf.sh ../src/sfc_classifier_kern.o veth0i cls

    # Start server on recv
    $NSCMD ns1 iperf3 -s -p 20000 -D

    #echo "NOT RUNNING IPERF3!"
    #sleep 60
    $NSCMD ns0 iperf3 -c "$(replace_address $TESTSUBNET 11)" --cport 10000 -p 20000 -t 0 -u -b 1G -J
    
    pkill iperf3
    
    destroy_infra 2
}

# Main
test_classifier

# Do whatever has to be done

# destroy_infra 3
