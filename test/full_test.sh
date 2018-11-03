#!/usr/bin bash

if [ "$EUID" -ne 0 ]
  then echo "Please run as root."
  exit
fi

function replace_address {
    SUBNET=$1
    HOSTID=$2
    echo "$(echo $SUBNET | sed "s/\.[^\.]*$/.${HOSTID}/g" )"
}

NS1=testns1
VETH0=testveth0
VETH1=testveth1
TESTSUBNET="10.1.0.0" # Will use /24
IPVETH0=$(replace_address $TESTSUBNET 50)
IPVETH1=$(replace_address $TESTSUBNET 51)
NSCMD1="ip netns exec $NS1"
DUMPFILE=tcpdump_tmp.out
KEEPINFRA="false"
BPFOBJ=../src/sfc_stages_kern.o


function unload_bpf {
    echo "Unloading BPF code"
    ip link set dev $VETH0 xdp off
    ip route del 0.0.0.0/0 dev $VETH0
    tc filter del dev $VETH0 egress
}

function load_bpf {
    echo "Loading BPF code"
    # Load decap XDP code
    ip -force link set dev $VETH0 xdp \
        obj ../src/sfc_stages_kern.o sec decap

    # Load encap code
    # Headroom = sizeof(Ethernet) + sizeof(NSH) = 14 + 8 = 22
    # ip route add $IPVETH1/32 encap bpf headroom 22 \
    #     xmit obj $BPFOBJ section encap dev $VETH0
    ip route add 0.0.0.0/0 encap bpf headroom 22 \
        xmit obj $BPFOBJ section encap dev $VETH0

    # # Load adjust + forward codes
    tc qdisc add dev $VETH0 clsact 2> /dev/null
    tc filter del dev $VETH0 egress 2> /dev/null
    tc filter add dev $VETH0 egress bpf da obj $BPFOBJ \
        sec forward
}

function print_usage {
    PROGNAME="$1"
    echo "Usage: $PROGNAME [keep|load|unload|help]"
}

if [ "$1" == "keep" ]; then
    KEEPINFRA="true"
elif [ "$1" == "unload" ]; then
    unload_bpf
    exit 0
elif [ "$1" == "load" ]; then
    load_bpf
    exit 0
elif [ "$1" == "help" ]; then
    print_usage $0
    exit 0
elif ! [ -z "$1" ]; then
    echo "Unknown command"
    print_usage $0 
    exit 1
fi

function destroy_infra {
    ip netns del $NS1 2> /dev/null
    ip link del $VETH0 2> /dev/null
    ip link del $VETH1 2> /dev/null
    # rm -f $DUMPFILE 2> /dev/null
}

function get_mac {
    echo "$(ip link show dev $1 | awk '{getline; print}' | awk '{print $2}')"
}

function create_infra {
    ip netns add $NS1
    
    ip link add $VETH0 type veth peer name $VETH1

    MACVETH0=$(get_mac $VETH0)
    MACVETH1=$(get_mac $VETH1)

    # echo "MACVETH0 = $MACVETH0" 
    # echo "MACVETH1 = $MACVETH1" 

    ip link set $VETH1 netns $NS1
    
    ip link set dev $VETH0 up
    ip link set dev $VETH0 promisc on

    $NSCMD1 ip link set dev $VETH1 up
    $NSCMD1 ip link set lo up

    ip addr add $IPVETH0/24 dev $VETH0
    $NSCMD1 ip addr add $IPVETH1/24 dev $VETH1

    arp -s "$IPVETH1" "$MACVETH1"
    $NSCMD1 arp -s "$IPVETH0" "$MACVETH0"

    ip route del $TESTSUBNET/24 dev $VETH0

    load_bpf

    # $NSCMD1 ip route del $TESTSUBNET/24 dev $VETH1
    
    # Add artificial entry to nsh_data map
    # key = 5-tuple(10.1.0.50,10.1.0.51,1000,2000,TCP)
    # value = NSH(0x1,0xFF)
    # bpftool map update pinned /sys/fs/bpf/tc/globals/nsh_data \
    # key hex 0a 01 00 32 0a 01 00 33 03 e8 07 d0 11 \
    # value hex 00 02 02 03 00 00 01 ff any

    # Add entries to FWD stage
    bpftool map update pinned /sys/fs/bpf/tc/globals/fwd_table \
    key hex 00 00 01 ff \
    value hex 00 ab cd ab cd ab cd any

    bpftool map update pinned /sys/fs/bpf/tc/globals/fwd_table \
    key hex 00 00 01 fe \
    value hex 01 00 00 00 00 00 00 any
}

destroy_infra
echo > /sys/kernel/debug/tracing/trace # Clear trace buffer
pkill tcpdump

create_infra

bash loopback-ovs.sh $VETH0

# tcpdump -i $VETH0 -XX not ip6 & > $DUMPFILE

# # $NSCMD1 hping3 -c 1 -I $VETH1 -a $IPVETH1 -s 1000 -p 2000 --keep -d 20 $IPVETH0
# # echo "MACVETH0 = $MACVETH0" 
# $NSCMD1 python send_packets.py -i $VETH1 -d $MACVETH0

# output="$(cat /sys/kernel/debug/tracing/trace)"

# echo "==========================================="
# echo "Output from /sys/kernel/debug/tracing/trace"
# echo "==========================================="
# echo
# echo "$output"
# echo

# echo "================================================="
# echo "Output from \"tcpdump -i $VETH0\" -XX"
# echo "================================================="
# echo
# echo "$(cat $DUMPFILE)"

if [ $KEEPINFRA == "false" ]; then
    destroy_infra
else
    echo "Keeping infra alive."
fi