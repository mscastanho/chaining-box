#!/usr/bin/env bash

# Script to perform all necessary configurations on
# FUTEBOL nodes before starting the experiment

if [ $# -ne 1 ]; then
    echo "Usage: $0 <kernel-img-path>"
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
        echo "Failed to executed command: $@"
        exit 1
    fi
}

# Export vars and funcs to access nodes
. setup-ssh.sh

# Parse kernel image argument
kimgpath="$1"
kimgfile="$(basename $kimgpath)"
if [[ $kimgfile != *.deb ]]; then
    echo "First arg should be a .deb package to install specific kernel"
    exit 1
fi

cbldir="$( cd "$(dirname $0)" ; pwd -P)/../.." # chaining-box dir
destdir="/home/${FUTUSER}"

for host in $FUTHOSTS; do
    
    echo -e "\nBootstraping $host"
    # If the host was exported by the previous
    # script, then there should be a var with the
    # corresponding name
    if [ -z "$host" ]; then
        echo "Host not found. Exiting..."
        exit 1
    fi

    # Change hostname
    echom "Setting hostname"
    run_n_check sshfut $host "sudo hostnamectl set-hostname $host"

    # Copy configuration script to host
    echom "Copying configuration script to host"
    run_n_check rsyncfut -azvh ./init.sh $host:$destdir

    # Run script remotely.
    echom "Running script init.sh on the host (this may take a while)"
    run_n_check sshfut $host "sudo $destdir/init.sh $FUTUSER"

    # Copy ChainingBox source code
    echom "Copying ChainingBox source code"
    run_n_check rsyncfut -azvh $cbldir $host:$destdir/chaining-box --exclude='.git*'

    # Install custom kernel
    echom "Installing custom kernel from file '$kimgpath'"
    run_n_check scpfut $kimgpath $host:$destdir
    run_n_check sshfut $host "sudo dpkg -i $destdir/$(echo $kimgfile)"

    # Rebooting host
    echom "Reboot issued. Moving on..."
    sshfut $host "sudo reboot" &> /dev/null
done
