#!/usr/bin/env bash

source common.sh

# Setup environment
cbox_deploy_ovs

echo "Starting pktgen..."
sudo $pktgen $pktgen_args -f throughput.lua
