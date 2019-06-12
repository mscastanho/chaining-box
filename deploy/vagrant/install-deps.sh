#!/usr/bin/env bash

if [ $EUID -ne 0 ]; then
    echo "Please run as sudo."
    exit 1
fi

# Install some stuff
sudo apt install iperf3 jq -y

## Install newest iproute2 version
iprdir="/home/vagrant/iproute2"

# TODO: Find out why the official repo is failing to clone
git clone https://github.com/shemminger/iproute2 $iprdir
cd $iprdir
./configure && make && make install

# Install bpftrace
sudo snap install --devmode bpftrace
sudo snap connect bpftrace:system-trace
