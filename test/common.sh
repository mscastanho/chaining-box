#!/usr/bin/env bash

# Test configuration
phys0="enp1s0f0"
phys1="enp1s0f1"
pktgen_args="-l 0-3 -n 4 -- -P -T -m 2.0,3.1"

if [ -z "$chainscfg" ]; then
  echo "Please set 'chainscfg' env var before calling the test scripts."
  exit 1
fi

# Var definitions
scriptdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cbdir="${scriptdir}/.."
objdir="${cbdir}/src/build"
scriptsdir="${cbdir}/scripts"
pktgen="/usr/src/pktgen-19.12.0/app/x86_64-native-linuxapp-gcc/pktgen"

# Log files
logdir="/tmp/cb"

[ -d $logdir ] && rm -rdf $logdir
mkdir $logdir

dockerlog="${logdir}/docker.log"
managerlog="${logdir}/manager.log"

# Function definitions
function fail_exit {
  echo "Something went wrong with the last command. Exiting..."
  exit 1
}

function get_if_addr {
  container="$1"
  iface="$2"
  docker exec -it $container ip link show $iface | grep ether | awk '{print $2}'
}

function list_sfs {
  cat $chainscfg | grep -o -e 'sf[0-9]*' | sort | uniq
}
