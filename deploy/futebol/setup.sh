#!/usr/bin/env bash

# Script to setup a FUTEBOL node for ChainingBox
# Matheus Castanho, 2019

if [ "$EUID" -ne 0 ]; then
    echo "Please run with sudo."
    exit 1
fi

if [ -z "$1" ]; then
    echo "Usage: $0 <user>"
fi 

user="$1"

# Configure MTU to access the Internet
ip link set dev ens3 mtu 1400

# Install dependencies
apt install iperf3 jq -y

# Upgrade to latest iproute2
cd ~/
git clone https://git.kernel.org/pub/scm/network/iproute2/iproute2-next.git 
cd iproute2-next
./configure && make && make install

# Create directory for ChainingBox source
cd ~/
mkdir chaining-box

# Change owner of all files in the home dir
chown -R $user /home/$user/

# Set MTU back to default to avoid interfering with experiments
ip link set dev ens3 mtu 1500
