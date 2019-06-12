#!/usr/bin/env bash

# This scripts enables easy SSH access to nodes 
# created on FUTEBOL testbeds.
# Usage:
#   - Create your experiment on JFed
#   - Double-click each node and copy the entire
#     SSH connect command that shows up in the terminal
#   - Paste the contents for a single node on a .txt
#     file on /tmp with fut* prefix
#       Ex: /tmp/fut01.txt
#   - Source this script:
#       Ex: . export-hosts.sh
#       Ex: source export-hosts.sh
#   - Repeat the process for every node 
#
# Now to access node fut01 (depends only on the name
# of the file you created) just run:
#   sshfut fut01
#
# To copy files **to** the node fut01 using rsync, run:
#   rsyncfut fut01 <local-path> <remote-path> [extra-params]
#
# Note that sourcing a file only sets variables for the
# current session. If opening a new terminal, you'll need
# to run the script again.

hostsdir="/tmp/futhosts"

if [ ! -d $hostsdir ]; then
    echo "Directory '$hostsdir' does not exist."
    echo "Please create it and put all your host config files there."
    exit 1
fi

# Create array of hosts
declare -a hosts

for f in $(ls $hostsdir | grep -e ".txt"); do
    cmd="$(cat $hostsdir/$f | tail -n+2)"
    envvars="$(cat $hostsdir/$f | head -n1)"

    if [ -z "${FUTVARS}" ]; then
        FUTVARS="$envvars"
    fi

    hostname="${f%.*}"
    address="$(echo $cmd | cut -d' ' -f6)"
    export "$hostname"="$address"

    proxycmd="$(echo $cmd | cut -d' ' -f1-5,7-)"
    export "${hostname}_proxy"="$proxycmd"

    if [ -z "${FUTUSER}" ]; then
        FUTUSER="$(echo $address | cut -d'@' -f1)"
    fi

    # Add host to list of hosts
    hosts+=($hostname)
done

# Create env var with list of hosts
export FUTHOSTS="${hosts[@]}"
unset hosts

eval "$FUTVARS"

# Connect to FUTEBOL node over SSH
function sshfut {
    hostname="$1"
    proxycmd=$hostname\_proxy
    
    # Skip first arg
    shift
    
    #Get remaining arguments
    params="$@"

    eval "${!proxycmd} ${!hostname} $params"
}

# **Send** files to FUTEBOL node using rsync
function rsyncfut {
    hostname="$1"
    lpath="$2"
    rpath="$3"
    params="$4"
    proxycmd=$hostname\_proxy
    
    eval "rsync -azvh $params -e '${!proxycmd}' $lpath ${!hostname}:$rpath"
}
