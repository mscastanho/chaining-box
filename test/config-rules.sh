#!/usr/bin/env bash

# This script configures two static chains,
# one for each direction of communication of
# a specific flow.

if [ -z "$1" ]; then
    echo "Usage: $0 <number-of-SFs>"
    exit 1
fi

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
    IPSRC="$(ip_to_hex $1)"
    IPDST="$(ip_to_hex $2)"
    SPORT="$(number_to_hex $3 4)"
    DPORT="$(number_to_hex $4 4)"
    PROTO="$(number_to_hex $5 2)"

    echo "${IPSRC}${IPDST}${SPORT}${DPORT}${PROTO}"
}

function install_rule {
    LAYER="$1" # TC, XDP...
    MAP="$2"
    KEY="$3"
    VALUE="$4"

    # Account for odd sized strings
    KEY=$(turn_even $KEY)
    VALUE=$(turn_even $VALUE)

    KEY=$(split_string $KEY)
    VALUE=$(split_string $VALUE)

    # echo "$SPLITTED_KEY"
    # echo "$SPLITTED_VALUE"

    echo "bpftool map update pinned /sys/fs/bpf/$LAYER/globals/$MAP key hex $KEY value hex $VALUE any"
}

# Flow definition
IPSRC="10.10.10.10"
IPDST="10.10.10.11"
SPORT="1000"
DPORT="2000"
PROTO="6" # TCP

NB_SFS="$1"
FLOW="$(tuple_to_hex $IPSRC $IPDST $SPORT $DPORT $PROTO)"
SPI="1"
PWD="vagrant"

for i in `seq 0 $((NB_SFS-1))`; do
    outfile="rules-sfc$i.txt"
    echo > $outfile
    
    # Install src_mac rule
    MAC="00:00:00:00:00:$(number_to_hex $((10+$i)) 2)"
    KEY="00"
    VALUE="${MAC//:/}" # Remove colons from MAC"
    # ssh -p "$((2000 + $i))" vagrant@localhost "echo $PWD | sudo -S $(install_rule tc src_mac $KEY $VALUE)"
    echo "sudo -S $(install_rule tc src_mac $KEY $VALUE)" >> $outfile

    NEXT_MAC="00:00:00:00:00:$(number_to_hex $((11+$i)) 2)"

    # Install SFC forwarding rule
    SI="$((255-$i))"
    [[ $i = $NB_SFS ]] && END_OF_CHAIN="01" || END_OF_CHAIN="00"
    KEY="$(number_to_hex $SPI 6)$(number_to_hex $SI 2)"
    VALUE="${END_OF_CHAIN}${NEXT_MAC//:/}" # Remove colons from MAC"

    # VMs are configured with forwarded ports for SSH connections
    # ssh -p "$((2000 + $i))" vagrant@localhost "echo $PWD | sudo -S $(install_rule tc fwd_table $KEY $VALUE)"
    echo "sudo -S $(install_rule tc fwd_table $KEY $VALUE)" >> $outfile

done
