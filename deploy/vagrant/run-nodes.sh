#!/usr/bin/env bash

# Create directory for temp files
tmp="/tmp/cb-vagrant"
mkdir -p $tmp

# Get list of hosts. This is faster than calling `vagrant status`
# and parsing the output.
hosts=($(vagrant status | grep running | awk '{print $1}'))

echo -n "Total of ${#hosts} running hosts detected. Continue? (y/n) "
read ans

if [ "$ans" != "y" ]; then
    echo "Exiting..."
    exit 0
fi
    
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
home="/home/vagrant"
ksrcdir="$home/linux-5.3"
cfgcmd="sudo CFLAGS=\"$CFLAGS\" $home/chaining-box/test/config-sfc.sh"
scpcmd="scp -F $sshcfg"
rulcmd="sudo $home/hardcoded-rules.sh"
sshcmd="ssh -F $sshcfg"
killcmd="for p in \$(ps aux | grep loopback.py | awk '{print \$2}'); do sudo kill -9 \$p; done"
srcip="10.10.10.10"
loopcmd="sudo nohup $home/chaining-box/test/loopback.py eth1 $srcip > /dev/null 2>&1 &"


# src - needs classifier
echo "~~> Configuring classifier ($src)..."
$scpcmd ./hardcoded-rules.sh $src:~/ &> /dev/null
$sshcmd $src "$cfgcmd cls eth1 $ksrcdir $home/chaining-box && $rulcmd"

# sf - needs all stages    
for h in ${hosts[@]}; do 
    echo "~~> Configuring SF ($h)..."
    
    echo "  - Installing BPF stages..."
    $scpcmd ./hardcoded-rules.sh $h:~/ &> /dev/null
    $sshcmd $h "$cfgcmd sf eth1 $ksrcdir $home/chaining-box && $rulcmd"

    echo "  - Killing previous SF processes..."
    #echo "todo: $killcmd; $loopcmd"
    $sshcmd $h "$killcmd"
    $sshcmd $h "$loopcmd"
done

# dst - no configuration to be done

