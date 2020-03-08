#!/usr/bin/env bash

source common.sh

# Setup environment
cbox_deploy_ovs

# Override the default args as pktgen needs to handle both send
# and receive ports on the same core to measure latency correctly.
pktgen_args="-l 0-2 -n 4 -- -P -T -m 1.[0-1]"

echo "Starting pktgen..."
sudo $pktgen $pktgen_args -f latency.lua
