#!/usr/bin/env bash

# Run the config script in the current shell
. config-cb.sh $1

# Run SSH config script
. setup-ssh.sh

IPERFPORT=10000
IPERFSERVIP="$(cat $sshcfg | grep -A 6 $src | grep HostName | awk '{ print $2 }')"
TESTDIR="$home/chaining-box/test"
TESTSCRIPT="lossrate-test.sh"

# iperf will be run with the -R (reverse) option, so we have
# data about lost packets as well. So the sender will run as server
# and the receiver will be the client

# Start iperf server on source
echob "~~> Starting iperf server on $src..." 
sshfut $src "pkill iperf3; iperf3 -s -p $IPERFPORT -D"

# Run throughput test script on destination
echob "~~> Running lossrate-test.sh on $dst (server IP: $IPERFSERVIP)"
sshfut $dst "sudo $TESTDIR/$TESTSCRIPT $IPERFSERVIP"

# Copy test results
resdir=$(sshfut $dst "ls /tmp/ | grep lossrate | tail -n1")
scpfut -r $dst:/tmp/$resdir ./ 

## TODO: Plot results
