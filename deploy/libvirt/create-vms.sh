#!/usr/bin/env bash

setup_script=/tmp/img-setup.sh
base_image=cbox-base.qcow2
number_sfs=2

cat - > ${setup_script} <<EOF
apt install --no-install-recommends -y \
    ethtool dwarves iputils-ping tcpdump iperf3 xxd \
    linux-image-5.3.0-70-generic linux-tools-5.3.0-70-generic

useradd -m -s /bin/bash -G sudo cbox
echo -e "cbox\ncbox" | passwd cbox
EOF


# Create base image if not there yet
[ ! -f ${base_image} ] && {
    virt-builder ubuntu-18.04 \
                 -o ${base_image} \
                 --format qcow2 \
                 --size 10G \
                 --hostname cbox \
                 --root-password password:chaining-box \
                 --update \
                 --run ${setup_script} \
                 --verbose
}

# TODO: create libvirt network to be used by VMs for communication

for i in `seq 1 ${number_sfs}`; do
    name="cbox-sf${i}"

    # Delete any running VM with the same name
    {
        virsh destroy ${name}
        virsh undefine ${name}
    } 2> /dev/null

    # Create VM image
    qemu-img create \
             -f qcow2 \
             -o backing_file=${base_image},backing_fmt=qcow2 \
             ${name}.qcow2

    # Start VM
    virt-install \
        --name ${name} \
        --ram 2048 \
        --os-type linux --os-variant ubuntubionic \
        --disk path=${name}.qcow2,format=qcow2 \
        --import \
        --noautoconsole

    # TODO: copy SSH key into VM for password-less login
    # TODO: install cbox files on VM
done
