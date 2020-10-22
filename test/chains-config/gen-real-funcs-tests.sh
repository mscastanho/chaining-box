#!/usr/bin/env bash

# This script generates a few config files using real service functions
# implemented as prototypes for testing. The source code for each can be found on their
# correponding directories under src/functions.

# Generate separate files with the following chains:
# 1) chacha
# 2) chacha -> flow monitor
# 3) chacha -> flow monitor -> firewall
# 4) chacha -> flow monitor -> firewall -> load balancer

function sf2json {
  name="$1"
  type="$2"
  remote="$3"

  echo "{ \"tag\": \"$name\", \"type\": \"$type\", \"remote\": $remote }"
}

function chain2json {
  id="$1"
  shift 1

  nlist="["

  for n in `seq 1 $#`; do
    sf="$(eval echo \$$n)"
    nlist="$nlist\"${sf}\","
  done

  echo "{ \"id\": $id, \"nodes\": ${nlist%,*}] }"
}

realfuncs=(chacha flow-monitor firewall l4lb)

if [ $# -ne 3 ]; then
    echo "Usage: $0 <len: int (max is ${#realfuncs[@]})> <chain-id: int> <remote: true|false>"
    exit 1
fi

len="$1"; shift 1
id="$1"; shift 1
remote="$1";

if [ $len -gt ${#realfuncs[@]} ]; then
    echo "len should not be higher than ${#realfuncs[@]}"
    exit 1
fi

sflist=""
funcs="["

for i in `seq 0 $(($len - 1 ))`; do
    func="${realfuncs[$i]}"
    sfname="sf$(($i+1))"
    sflist="$sflist ${sfname}"
    funcs="$funcs$(sf2json ${sfname} "${func}" $remote),"
done

echo "{ \"chains\": [$(chain2json $id $sflist)], \"functions\": ${funcs%,*}]}" | jq ''
