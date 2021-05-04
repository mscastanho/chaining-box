#!/usr/bin/env bash

scriptdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
imgdir="${scriptdir}/images"
setup_script=/tmp/img-setup.sh
base_image="${imgdir}/base.qcow2"
tmpdir="/tmp/cb"
libvirt_imgdir="/var/lib/libvirt/images"

# Create temp directory
mkdir -p "${tmpdir}"

# Create directory for image files
mkdir -p ${imgdir}
chmod a+x ${imgdir}

# Remove old images from previous runs
rm -f ${imgdir}/cbox-*

cat - > ${setup_script} <<EOF
apt install --no-install-recommends -y \
    binutils-dev \
    bison \
    dwarves \
    elfutils \
    ethtool \
    flex \
    gcc \
    gcc-multilib \
    iperf3 \
    iproute2 \
    iputils-ping \
    libcap-dev \
    libdw-dev \
    libelf-dev \
    libmnl-dev \
    linux-image-5.4.0-48-generic \
    linux-tools-5.4.0-48-generic \
    make \
    pkg-config \
    tcpdump \
    xxd

# Install iproute2 v5.4.0
cd /tmp && wget https://git.kernel.org/pub/scm/network/iproute2/iproute2.git/snapshot/iproute2-5.4.0.tar.gz
tar -xf iproute2-5.4.0.tar.gz
cd iproute2-5.4.0 && make && make install

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
                 --firstboot-command "ssh-keygen -A" \
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

hosts=($@)

# Check if OVS bridge already exists
ovs_net="cbox-br"
sudo ovs-vsctl br-exists ${ovs_net} || {
    echo "Failed to find OVS bridge ${ovs_net}. Is it created?"
    exit 1
}

mgmt_net="cb-management"

# Create network if it doesn't exist yet
{ virsh net-info ${mgmt_net}; } > /dev/null 2>&1
[ $? -ne 0 ] && {
    netxml="${tmpdir}/cbmgmt.xml"
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

for (( i = 0 ; i < ${#hosts[@]} ; i++ )) ; do
    name="${hosts[i]}"
    imgbasename="cbox-${name}.qcow2"
    img="${imgdir}/${imgbasename}"

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
    builddir=$(realpath ${CB_DIR:-"${scriptdir}/../.."}/src/build)
    [ -d ${builddir} ] && {
        virt-copy-in -a ${img} ${builddir} /home/cbox/chaining-box
    } || {
        echo -e "Couldn't find build directory: ${builddir}.\nAborting..."
        exit 1
    }

    # Setup network configuration
    # Addresses will start from 10.10.*.11
    hostnetcfg="${tmpdir}/01-netcfg.yaml"
    cat - > ${hostnetcfg} <<EOF
# Created by script ${0}
network:
  version: 2
  renderer: networkd
  ethernets:
    ens3:
     dhcp4: no
     addresses: [10.10.10.$(($i + 11))/24]
     gateway4: 10.10.10.1
    ens4:
     dhcp4: no
     addresses: [10.10.20.$(($i + 11))/24]
     gateway4: 10.10.20.1
EOF

    virt-copy-in -a ${img} ${hostnetcfg} /etc/netplan

    # Setup proper hostname
    hostnamecfg="${tmpdir}/hostname"
    cat - > ${hostnamecfg} <<EOF
${name}
EOF
    virt-copy-in -a ${img} ${hostnamecfg} /etc/

    macaddress="52:54:00:00:00:$(printf '%02x' $(($i + 10)))"

    vmxml="vm.xml"

    # Need to copy the image to a place where libvirt can access it
    sudo mv ${img} ${libvirt_imgdir}
    img=${libvirt_imgdir}/${imgbasename}

    # Start VM
    virt-install \
        --name ${name} \
        --ram 2048 \
        --os-type linux --os-variant generic \
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
