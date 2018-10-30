#!/usr/bin bash

NS0=nslwt0
NS1=nslwt1
VETH0=veth_lwt0
VETH1=veth_lwt1
IPVETH0="10.1.0.50"
IPVETH1="10.1.0.51"
NSCMD0="ip netns exec $NS0"
NSCMD1="ip netns exec $NS1"
DUMPFILE=tcpdump_tmp.out

function destroy_infra {
    ip link del $VETH0 2> /dev/null
    ip link del $VETH1 2> /dev/null
    ip netns delete $NS0 2> /dev/null
    ip netns delete $NS1 2> /dev/null
    rm -f $DUMPFILE 2> /dev/null
}

destroy_infra
echo > /sys/kernel/debug/tracing/trace # Clear trace buffer

ip netns add $NS0
ip netns add $NS1

ip link add $VETH0 type veth peer name $VETH1

ip link set $VETH0 netns $NS0
ip link set $VETH1 netns $NS1

$NSCMD0 ip link set dev $VETH0 up
$NSCMD1 ip link set dev $VETH1 up

$NSCMD0 ip addr add $IPVETH0/24 dev $VETH0
$NSCMD1 ip addr add $IPVETH1/24 dev $VETH1

$NSCMD1 ip link set promisc on $VETH1

# Headroom = sizeof(Ethernet) + sizeof(NSH) = 14 + 8 = 22
$NSCMD0 ip route add 0.0.0.0/0 encap bpf headroom 22 \
    xmit obj ../src/sfc_proxy_kern.o section encap dev $VETH0

$NSCMD0 ip route del "10.1.0.0"/24 dev $VETH0

# $NSCMD0 ip route add "10.1.0.0"/24 encap bpf headroom 14 \
#    xmit obj ../src/lwt_test_kern.o section push_ll dev $VETH0

# echo "Environment setup."

$NSCMD1 tcpdump -i $VETH1 -XX not ip6 & > $DUMPFILE

$NSCMD0 ping -c 1 $IPVETH1

output="$(cat /sys/kernel/debug/tracing/trace)"

echo "==========================================="
echo "Output from /sys/kernel/debug/tracing/trace"
echo "==========================================="
echo
echo "$output"
echo

echo "================================================="
echo "Output from \"tcpdump -i $VETH1\" -XX at $NS1"
echo "================================================="
echo
echo "$(cat $DUMPFILE)"

destroy_infra