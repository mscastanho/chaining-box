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

if [ $# -lt 3 ]; then
    echo "Usage: $0 <len: int (max is ${#realfuncs[@]})> <chain-id: int> <remote: true|false> [star]"
    exit 1
fi

len="$1"; shift 1
id="$1"; shift 1
remote="$1"; shift 1
star="$1"

if [ $len -gt ${#realfuncs[@]} ]; then
    echo "len should not be higher than ${#realfuncs[@]}"
    exit 1
fi

sflist=""

if [ "$star" == "star" ]; then
    funcs="[$(sf2json sf1 "tc-redirect" $remote),"
    extra_hop="sf1"
    startindex=2
    sflist="sf1"
else
    funcs="["
    extra_hop=""
    startindex=1
fi

for i in `seq 0 $(($len - 1 ))`; do
    func="${realfuncs[$i]}"
    sfname="sf$(($i+$startindex))"
    sflist="$sflist ${sfname} ${extra_hop}"
    funcs="$funcs$(sf2json ${sfname} "${func}" $remote),"
done

echo "{ \"chains\": [$(chain2json $id $sflist)], \"functions\": ${funcs%,*}]}" | jq ''
