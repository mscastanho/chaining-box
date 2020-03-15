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

  for sf in $@; do
    nlist="$nlist\"sf1\",\"${sf}\","
  done

  echo "{ \"id\": $id, \"nodes\": ${nlist}\"sf1\"] }"
}

len="$1"
id="$2"
remote="$3"

sflist=""
funcs="[$(sf2json sf1 "tc-redirect" $remote),"
for n in `seq 2 $(($len + 1))`; do
  sflist="$sflist sf$n"
  funcs="$funcs$(sf2json sf$n "tc-redirect" $remote),"
done

echo "{ \"chains\": [$(chain2json $id $sflist)], \"functions\": ${funcs%,*}]}" | jq ''
