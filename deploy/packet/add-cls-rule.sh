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

if [ -z "$1" ] || [ -z $2 ] || [ -z $3 ] || [ -z $4 ] || [ -z $5 ] || [ -z $6 ]; then
  echo "Usage: $0 <layer> <map> <key> <"
fi

ipsrc="$1"
ipdst="$2"
sport="$3"
dport="$4"
proto="$5"
spi="$6"

macbytes="$(docker exec -it sf1 ip link show eth1 | grep ether | awk '{print $2}' | tr ':' ' ')"
key="$(split_string $(tuple_to_hex $ipsrc $ipdst $sport $dport $proto))"
val="$(split_string $(number_to_hex $spi 6)) ff $macbytes"


bpftool map update pinned /sys/fs/bpf/xdp/globals/cls_table key hex $key value hex $val any
