#!/usr/bin/env bash

source common.sh

single="$tmpcfgdir/len-2-len.json"
./chains-config/gen-len-tests.sh 2 100 true > $single

# Setup environment
cbox_deploy_ovs $single

sudo perf record -e cpu-cycles -a -F 5000 -- sleep 30 &
echo "Starting pktgen..."
sudo $pktgen $pktgen_args -f tput.lua
