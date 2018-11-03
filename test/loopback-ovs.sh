if [ -z $1 ]; then
    echo "Usage: $0 <iface>"
    exit 1
fi

IFACE="$1"
OVS_DIR=/home/vagrant/ovs

# Kill all ovs processes currently running
echo "Killing ovs processes currently running..."
killall ovsdb-server ovs-vswitchd
pkill ovs
echo "Done."
###################################

# Delete and create again directories necessary for ovs
echo "Removing and recreating directories..."
rm -rf /usr/local/etc/openvswitch
rm -rf /usr/local/var/run/openvswitch/vhost-user*
rm -f /usr/local/etc/openvswitch/conf.db

mkdir -p /usr/local/etc/openvswitch
mkdir -p /usr/local/var/run/openvswitch
echo "Done."

###################################

# Create ovs database
echo "Creating ovs database..."
ovsdb-tool create /usr/local/etc/openvswitch/conf.db \
	$OVS_DIR/vswitchd/vswitch.ovsschema
sleep 1
echo "Done."

###################################

# Start ovsdb-server
echo "Starting ovsdb server..."
DB_SOCK=/usr/local/var/run/openvswitch/db.sock
ovsdb-server \
    --remote=punix:$DB_SOCK \
    --remote=db:Open_vSwitch,Open_vSwitch,manager_options \
    --private-key=db:Open_vSwitch,SSL,private_key \
    --bootstrap-ca-cert=db:Open_vSwitch,SSL,ca_cert \
    --pidfile --detach &
sleep 1
echo "Done."

###################################

echo "Starting OVS..."
ovs-vsctl --no-wait init

#Turn on daemon
ovs-vswitchd --pidfile --detach

###################################

ovs_get_iface_number () {
    ovs-vsctl get interface $1 ofport
}

BRIDGE=br0
ovs-vsctl add-br $BRIDGE -- set bridge $BRIDGE datapath_type=netdev
ovs-vsctl add-port $BRIDGE $IFACE

# Create loopback rule
ovs-ofctl add-flow $BRIDGE in_port=$(ovs_get_iface_number $IFACE),actions=in_port