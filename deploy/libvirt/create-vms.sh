#!/usr/bin/env bash

scriptdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
imgdir="/tmp/cb/img"
setup_script=/tmp/img-setup.sh
base_image="${imgdir}/cbox-base.qcow2"
number_sfs=2

# Create directory for image files
mkdir -p ${imgdir}

cat - > ${setup_script} <<EOF
apt install --no-install-recommends -y \
    dwarves \
    ethtool \
    iperf3 \
    iproute2 \
    iputils-ping \
    linux-image-5.3.0-70-generic \
    linux-tools-5.3.0-70-generic \
    tcpdump \
    xxd

useradd -m -s /bin/bash -G sudo cbox
bash -c 'echo -e "cbox\ncbox" | passwd cbox'
mkdir /home/cbox/chaining-box
chown -R cbox:cbox /home/cbox/chaining-box
EOF

# Create base image if not there yet
[ -f ${base_image} ] && {
    echo "Base image already exists, won't recreate it..."
} || {
    virt-builder ubuntu-18.04 \
                 -o ${base_image} \
                 --format qcow2 \
                 --size 10G \
                 --hostname cbox \
                 --root-password password:chaining-box \
                 --update \
                 --run ${setup_script} \
                 --verbose

    [ $? -ne 0 ] && {
        echo "Something went wrong during base image creation, exiting..."
        exit 1
    }

    # Copy SSH key into VM for password-less login
    sshkey="${HOME}/.ssh/id_rsa.pub"
    [ -f ${sshkey} ] && {
        virt-sysprep -a ${base_image} --ssh-inject cbox:file:${sshkey}
    }
}

# Connect to the system's QEMU by default
export LIBVIRT_DEFAULT_URI=qemu:///system

hosts=()
for i in `seq 1 ${number_sfs}`; do
    hosts+=("cbox-sf${i}")
done

mgmt_net="cb-management"

# Create network if it doesn't exist yet
{ virsh net-info ${mgmt_net}; } > /dev/null 2>&1
[ $? -ne 0 ] && {
    netxml="/tmp/cbmgmt.xml"
    cat - > ${netxml} <<EOF
<network>
  <name>cb-management</name>
  <bridge name='cbmgmt-br' delay='0'/>
  <ip address='10.10.10.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.10.10.10' end='10.10.10.254'/>
    </dhcp>
  </ip>
</network>
EOF
    virsh net-create ${netxml}
}

ovs_net="cbox-br"

# Create OVS bridge
sudo ovs-vsctl add-br ${ovs_net}

for (( i = 0 ; i < ${#hosts[@]} ; i++ )) ; do
    name="${hosts[i]}"
    img="${imgdir}/${name}.qcow2"

    # Delete any running VM with the same name
    {
        virsh destroy ${name}
        virsh undefine ${name}
    } 2> /dev/null

    # Create VM image
    qemu-img create \
             -f qcow2 \
             -o backing_file=${base_image},backing_fmt=qcow2 \
             ${img}

    # Install cbox files on VM
    # We could save some cycles by copying this only once to the base image, but
    # regenerating that takes some time, and it wouldn't be nice to have to
    # wait for that everytime we make changes to the project...
    builddir=../../src/build
    [ -d ${builddir} ] && {
        virt-copy-in -a ${img} ../../src/build /home/cbox/chaining-box
    } || {
        echo -e "Couldn't find build directory: ${builddir}.\nAborting..."
        exit 1
    }

    # Setup network configuration
    hostnetcfg="/tmp/cb/01-netcfg.yaml"
    cat - > ${hostnetcfg} <<EOF
# Created by script ${0}
network:
  version: 2
  renderer: networkd
  ethernets:
    enp3s0:
     dhcp4: no
     addresses: [10.10.10.$(($i + 10))/24]
     gateway4: 10.10.10.1
EOF

    virt-copy-in -a ${img} ${hostnetcfg} /etc/netplan

    macaddress="52:54:00:00:00:$(printf '%02x' $(($i + 10)))"

    vmxml="vm.xml"

    # Start VM
    virt-install \
        --name ${name} \
        --ram 2048 \
        --os-type linux --os-variant ubuntubionic \
        --disk path=${img},format=qcow2 \
        --network network=${mgmt_net} \
        --import \
        --noautoconsole \
        --print-xml > ${vmxml}

    # Edit XML to add an interface connected to an OVS bridge
    python3 ${scriptdir}/add-ovs-network.py ${ovs_net} ${macaddress} ${vmxml} ${vmxml}

    virsh create ${vmxml}

    rm ${vmxml}
done
