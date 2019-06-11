#!/usr/bin/env bash

# Create directory for temp files
tmp="/tmp/cb-vagrant"
mkdir -p $tmp

# Get list of hosts. This is faster than calling `vagrant status`
# and parsing the output.
hosts=($(ls .vagrant/machines))

# TODO: Check if all hosts are running. Fail otherwise.

# The first vm in the list will be our source and the last our
# destination during tests. All the others will be SFs
src=${hosts[0]} 
dst=${hosts[-1]}
unset hosts[0]
unset hosts[-1]

# Get SSH configuration to access hosts
sshcfg="$tmp/ssh.config"
vagrant ssh-config > $sshcfg

#### Load and config stages on each host ####
cfgcmd="sudo ~/chaining-box/test/config-sfc.sh"
scpcmd="scp -F $sshcfg"
rulcmd="sudo ~/hardcoded-rules.sh"

sshcmd="ssh -F $sshcfg"

# src - needs classifier
$scpcmd $src ./hardcoded-rules.sh $src:~/
$sshcmd $src "$cfgcmd cls eth1 && $rulcmd"

# sf - needs all stages    
for h in $hosts; do 
    $scpcmd $h ./hardcoded-rules.sh $src:~/
    $sshcfg $h "$cfgcmd sf eth1 && $rulcmd" 
done

# dst - no configuration to be done

### Setup static entries on local bridge
netname="sfcnet"

# Get bridge name
br=$(virsh net-info $netname | grep Bridge | awk '{print $2}')

bash static-entries.sh $br

### Start loopback function inside each SF
killcmd='for p in $(ps aux | grep nano | awk \'{print $2}\'); do kill -9 $p $
# Kill old instances to avoid duplicating packets
