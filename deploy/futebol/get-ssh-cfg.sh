#!/usr/bin/env bash


hostsdir="/tmp/futhosts"
sshcfg="/tmp/fut-cfg"

if [ ! -d $hostsdir ]; then
    echo "Directory '$hostsdir' does not exist."
    echo "Please create it and put all your host config files there."
    exit 1
fi

# Create array of hosts
declare -a hosts

# Erase and create config file
echo -n "" > $sshcfg

for f in $(ls $hostsdir | grep -e ".txt"); do
    
    # Get SSH connection params from command
    cmd="$(cat $hostsdir/$f | tail -n+2)"
    hostname="${f%.*}"
    ipaddress="$(echo $cmd | cut -d' ' -f6 | cut -d'@' -f2)"
    user="$(echo $cmd | cut -d' ' -f6 | cut -d'@' -f1)"
    idfile=$(echo $cmd | cut -d' ' -f5 | sed "s/'//g")
    proxycmd=$(echo $cmd | cut -d' ' -f8- | sed 's/-oProxyCommand=//' | sed 's/"//g')
    port="$(echo $cmd | cut -d' ' -f7 | sed 's/-oPort=//')"
    
    # Write contents to config file
    {
        echo "Host $hostname"
        echo "  User $user"
        echo "  HostName $ipaddress"
        echo "  Port $port"
        echo "  IdentityFile $idfile"
        echo "  ProxyCommand $proxycmd"
        echo "  ForwardX11 yes"
        echo "" # Skip 1 line
    } >> $sshcfg

    # Set some environment variables
    envvars="$(cat $hostsdir/$f | head -n1)"

    if [ -z "${FUTVARS}" ]; then
        FUTVARS="$envvars"
        eval "$envvars"   
    fi
    
    if [ -z "${FUTUSER}" ]; then
        export FUTUSER="$(echo $address | cut -d'@' -f1)"
    fi

    # Add host to list of hosts
    hosts+=($hostname)
done

# Create env var with list of hosts
export FUTHOSTS="${hosts[@]}"
unset hosts

# Create env var with SSH config file path
export FUTCFG="${sshcfg}"

function sshfut {
    ssh -F $FUTCFG $@
}

function rsyncfut {
    ssh -e "ssh -F $FUTCFG" $@
}
