#!/usr/bin/env bash

# Script to perform all necessary configurations on
# FUTEBOL nodes before starting the experiment

if [ $# -ne 1 ]; then
    echo "Usage: $0 <chaining-box-ldir>"
    exit 1
fi

# Echo message with pretty prefix
function echom {
    echo "~~> $@"
}

# Run command and check the output for errors
function run_n_check {
    $@ &> /dev/null

    if [ $? -ne 0 ]; then
        echo "Failed to executed command: $1 $2 $3..."
        exit 1
    fi
}

# Export vars and funcs to access nodes
. export-hosts.sh

#host="$1"
cbldir="$1"
destdir="/home/${FUTUSER}"

for host in $FUTHOSTS; do
    
    echo -e "\nBootstraping $host..."
    # If the host was exported by the previous
    # script, then there should be a var with the
    # corresponding name
    if [ -z "${!host}" ]; then
        echo "Host $1 not found. Exiting..."
        exit 1    
    fi

    # Change hostname
    echom "Setting hostname..."
    run_n_check sshfut $host "sudo hostnamectl set-hostname $host"

    # Copy configuration script to host
    echom "Copying configuration script to host..."
    run_n_check rsyncfut $host ./setup.sh $destdir

    # Run script remotely.
    echom "Running setup.sh on the host..."
    run_n_check sshfut $host 'sudo $destdir/setup.sh $FUTUSER'

    # Copy ChainingBox source code
    echom "Copying ChainingBox source code..."
    run_n_check rsyncfut $host $cbldir $destdir/chaining-box --exclude='.git*'

done
