#!/usr/bin/env bash
# Script to configure ChainingBox hosts for testing

# Vars to set output text mode
b=$(tput bold) # bold
n=$(tput sgr0) # normal

# Function to echo bold text
function echob {
    echo "${b}${@}${n}"
}

# Run script to set FUTEBOL env vars
source export-hosts.sh

# Create directory for temp files
tmp="/tmp/cb-vagrant"
mkdir -p $tmp

# Get list of hosts. FUTHOSTS is and environment variable
# created by the export-hosts.sh
hosts=($FUTHOSTS)

echo -n "Total of ${#hosts[@]} hosts detected. Continue? (y/n) "
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

#### Load and config stages on each host ####
home="/home/$FUTUSER"
cfgcmd="sudo $home/chaining-box/test/config-sfc.sh"
rscmd="rsyncfut"
rulcmd="sudo $home/hardcoded-rules.sh"
sshcmd="sshfut"
killcmd="for p in \$(pgrep loopback); do sudo kill -9 \$p; done"
iface="ens3"
loopcmd="sudo nohup $home/chaining-box/test/loopback.py $iface > /dev/null 2>&1 &"

# src - needs classifier
echob "~~> Configuring classifier ($src)..."
$rscmd $src ./hardcoded-rules.sh "$home" &> /dev/null
remote_cmd="$cfgcmd cls $iface $home/chaining-box && $rulcmd"
$sshcmd $src \'$remote_cmd\'

# sf - needs all stages    
for h in ${hosts[@]}; do 
    echob "~~> Configuring SF ($h)..."
    
    echo "  - Installing BPF stages..."
    $rscmd $h ./hardcoded-rules.sh "$home" &> /dev/null
    $sshcmd $h \'"$cfgcmd sf $iface $home/chaining-box && $rulcmd"\' 
    
    echo "  - Killing previous SF processes..."
    #echo "todo: $killcmd; $loopcmd"
    #$sshcmd 
    $sshcmd $h \'$killcmd\'
    echo "  - Starting loopback.py"
    $sshcmd $h \'$loopcmd\'
done

# dst - no configuration to be done

