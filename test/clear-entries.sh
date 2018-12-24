#!/usr/bin/env bash

# Clears all fdb entries for bridges on the system

function master_index {
    line=($1)

    cnt=0; for w in "${line[@]}"; do
        [[ $w == "master" ]] && echo $cnt && break
        ((++cnt))
    done
}

entries=$(bridge fdb show)

while read -r e; do
    # echo $e
    idx=$(master_index "$e")
    rule=$(echo $e | cut -f 1-$((idx+1)) -d ' ')
    echo $rule
    bridge fdb del $rule
done <<< "$entries"