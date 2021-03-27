#!/usr/bin/env bash

SCRIPTDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
CBOXDIR="$SCRIPTDIR/.."

if [ -z "$1" ]; then
  echo "Usage: $0 <chains-config>"
  exit 1
fi

cfg="$1"

if [ ! -f "$cfg" ]; then
  echo "Failed to find file. Does it really exist? File: $cfg"
  exit 1
fi

# Kill previous docker containers
{
  docker kill src dst $(grep tag $cfg | awk '{print $2}' | tr '"' '\0' | tr ',' '\0')
} &> /dev/null

# Remove OVS bridge
sudo ovs-vsctl del-br cbox-br

# Start new containers and wait to start the manager
sudo ${CBOXDIR}/src/build/cb_deploy -c $cfg -d ${CBOXDIR} -n ovs -t && ${CBOXDIR}/src/build/cb_manager $cfg
