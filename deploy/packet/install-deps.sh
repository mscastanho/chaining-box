#!/usr/bin/env bash

# Install stuff
apt update
apt install build-essential git iperf3 docker.io meson ninja-build jq ethtool hping3 \
  pciutils libnuma-dev libpcap-dev python3 liblua5.3-dev linux-image-5.3.0-46-generic \
  linux-headers-5.3.0-46-generic libbfd-dev libcap-dev libmnl-dev libelf-dev bison flex -y

# Script to update kernel
# wget https://raw.githubusercontent.com/pimlie/ubuntu-mainline-kernel.sh/master/ubuntu-mainline-kernel.sh
# chmod +x ubuntu-mainline-kernel.sh
# ./ubuntu-mainline-kernel.sh -i v5.3.0

# Download docker images
docker pull mscastanho/chaining-box:cb-build
docker pull mscastanho/chaining-box:cb-node

# Clone repo
git clone  --recurse-submodules https://github.com/mscastanho/chaining-box.git ~/chaining-box
cd chaining-box/src
./build.sh
cd

# Install DPDK
dpdk_tar="dpdk-19.11.1.tar.xz"
wget -O /usr/src/$dpdk_tar https://fast.dpdk.org/rel/$dpdk_tar
cd /usr/src
tar -xf $dpdk_tar
mv "dpdk-stable-19.11.1" "dpdk-19.11"
cd dpdk-19.11
export RTE_SDK=$(pwd)
export RTE_TARGET=x86_64-native-linuxapp-gcc
make install "T=${RTE_TARGET}" -j$(nproc)
cd

# Install pktgen DPDK
wget -O /usr/src/pktgen.tar.xz https://git.dpdk.org/apps/pktgen-dpdk/snapshot/pktgen-19.12.0.tar.xz
cd /usr/src
tar -xf pktgen.tar.xz
cd pktgen-19.12.0
make
cd

# Script to handle ifaces and containers
wget -O /usr/local/bin/pipework https://raw.githubusercontent.com/jpetazzo/pipework/master/pipework
chmod +x /usr/local/bin/pipework

# Install iproute2
git clone git://git.kernel.org/pub/scm/network/iproute2/iproute2.git
cd iproute2
make && make install
cd -

# Install bpftool
kversion="v5.3"
git clone --depth 1 --branch $kversion --single-branch https://github.com/torvalds/linux.git ~/linux-$kversion
cd ~/linux-$kversion/tools/bpf/bpftool
make && make install
cd -

# Reboot to load new kernel
# reboot

# Configure SR-IOV
# echo 6 > /sys/class/net/enp1s0f0/device/sriov_numvfs
