#!/usr/bin/env bash

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

len="$1"
id="$2"
remote="$3"

sflist=""
funcs="["
for n in `seq 1 $len`; do
  sflist="$sflist sf$n"
  funcs="$funcs$(sf2json sf$n "tc-redirect" $remote),"
done

echo "{ \"chains\": [$(chain2json $id $sflist)], \"functions\": ${funcs%,*}]}" | jq ''
