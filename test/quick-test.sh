#!/usr/bin/env bash

SCRIPTDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
CBOXDIR="$SCRIPTDIR/.."

if [ -z "$1" ]; then
  echo "Usage: $0 <chains-config>"
fi

cfg="$1"

# Kill previous docker containers
docker kill src dst sf1 sf2

# Remove OVS bridge
sudo ovs-vsctl del-br cbox-br

# Start new containers and wait to start the manager
sudo ${CBOXDIR}/src/build/cb_docker -c $cfg -d ${CBOXDIR} -n ovs -t && ${CBOXDIR}/src/build/cb_manager $cfg
