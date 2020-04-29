#!/usr/bin/env bash

function turn_even {
    KEY="$1"

    if [ $((${#KEY} % 2)) -ne 0 ]; then
        KEY="0$KEY"
    fi

    echo "$KEY"
}

function split_string {
    STRING="$1"
    SPLITTED=""

    for i in `seq 0 2 $((${#STRING}-1))`; do
        SPLITTED="$SPLITTED ${STRING:$i:2}"
    done

    # Remove initial space and return
    echo "${SPLITTED:1}"
}

function ip_to_hex {
    IP="$1"
    printf '%02x' ${IP//./ }
    echo
}

function number_to_hex {
    N="$1"
    NDIGITS="$2"
    printf "%0${NDIGITS}x" $N
}

# IP 5-tuple to hex format
function tuple_to_hex {
    ipsrc="$(ip_to_hex $1)"
    ipdst="$(ip_to_hex $2)"
    sport="$(number_to_hex $3 4)"
    dport="$(number_to_hex $4 4)"
    proto="$(number_to_hex $5 2)"

    echo "${ipsrc}${ipdst}${sport}${dport}${proto}"
}

# Reverse space-separated strings
function reverse_bytes {
    PAIRS="$@"
    echo -n "$PAIRS " | tac -s ' '
}
