#!/usr/bin/env bash

if [ $EUID -ne 0 ]; then
    echo "Please run as sudo."
    exit 1
fi

# Hardcoded name defined on net definition on Vagrantfile
netname="sfcnet"

br=$(virsh net-info $netname | grep Bridge | awk '{print $2}')

# Disable STP on $br
brctl stp $br off

# Get interfaces connected to $br
ifaces=$(brctl show $br | tail -n+3 | awk '{print $1}')

for vif in $ifaces; do
    mac=$(ip link show $vif | grep link/ether | awk '{print $2}')
    
    # We need the MAC for the interfaces inside the VMs
    # They follow the pattern: 00:00:00:00:00:xx
    # Their counterpart connected to the bridge outside
    # the VM have the most significant byte replaced by fe
    # So to get the 'internal' MAC we need to replace this
    # value for 00
    imac="00:${mac:3}"

    # Disable MAC learning and flooding
    bridge link set dev $vif learning off
    bridge link set dev $vif flood off

    # Add static entry do forwarding table
    bridge fdb add $imac dev $vif master temp
done
