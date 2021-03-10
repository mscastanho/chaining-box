#!/usr/bin/env bash

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
mkdir /home/cbox/.ssh /home/cbox/chaining-box
chown -R cbox:cbox /home/cbox/.ssh /home/cbox/chaining-box
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
        # This is safe because we know the destination file will be empty,
        # otherwise we would overwrite all other keys in the file.
        cat ${sshkey} > /tmp/authorized_keys
        virt-copy-in -a ${base_image} /tmp/authorized_keys /home/cbox/.ssh
    }

    # TODO: Do the SSH key injection using virt-sysprep --ssh-inject

    # TODO: Enable DHCP on enp1s0 by default
}

# Connect to the system's QEMU by default
export LIBVIRT_DEFAULT_URI=qemu:///system

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

for i in `seq 1 ${number_sfs}`; do
    name="cbox-sf${i}"
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

    # Start VM
    virt-install \
        --name ${name} \
        --ram 2048 \
        --os-type linux --os-variant ubuntubionic \
        --disk path=${img},format=qcow2 \
        --network network=${mgmt_net} \
        --import \
        --noautoconsole

    # TODO: Find a way to pre-determine the VM's IP address
done
