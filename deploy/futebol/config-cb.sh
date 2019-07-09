#!/usr/bin/env bash
# Script to configure ChainingBox hosts for testing

if [ "$1" == "redir" ]; then
    tc_redir="true"

    echo -e "\n=========================="
    echo " tc_redirect mode enabled"
    echo -e "==========================\n"
fi

# Vars to set output text mode
b=$(tput bold) # bold
n=$(tput sgr0) # normal

# Function to echo bold text
function echob {
    echo "${b}${@}${n}"
}

# Run script to set FUTEBOL env vars
source setup-ssh.sh

# Create directory for temp files
tmp="/tmp/cb-vagrant"
mkdir -p $tmp

# Get list of hosts. FUTHOSTS is and environment variable
# created by setup-ssh.sh
hosts=($FUTHOSTS)

echo -en "Total of ${#hosts[@]} hosts detected: ${hosts[@]}\n~> Continue? (y/n) "
read ans

if [ "$ans" != "y" ]; then
    echo "Exiting..."
    exit 0
fi

echo ""

# Define vars and commands to be used
home="/home/$FUTUSER"
ksrcdir="$home/linux-5.2-rc6"
cfgcmd="sudo $home/chaining-box/test/config-sfc.sh"
rscmd="rsyncfut"
sshcmd="sshfut"
iface="ens3"
killcmd="for p in \$(pgrep loopback); do sudo kill -9 \$p; done"

# Get MAC and IP addresses for all hosts
declare -A addresses

echob "~~> Retrieving hosts info"
for h in ${hosts[@]}; do
    mac=$(sshfut $h "ip link show $iface | grep ether | awk {'print \$2'}")
    ip=$(sshfut $h "ip addr show ens3 | grep 'inet ' | head -n1 | awk '{print \$2}'")
    ip=${ip:0:-3} # Remove netmask
    
    # Save on hash table for later
    addresses[$h]="$mac/$ip"

     echo -e "  [$h]\n   MAC -> $mac\n   IP  -> $ip\n"
done

# The first vm in the list will be our source and the last our
# destination during tests. All the others will be SFs
src=${hosts[0]} 
dst=${hosts[-1]}
unset hosts[0]
unset hosts[-1]

#### Load and config stages on each host ####

# src - needs classifier
echob "~~> Configuring classifier ($src)"
sshfut $src "$cfgcmd cls $iface $ksrcdir $home/chaining-box"

# sf - needs all stages    
for h in ${hosts[@]}; do 
    echob "~~> Configuring stages ($h)"
    
    echo "  - Installing BPF stages"
    sshfut $h "$cfgcmd sf $iface $ksrcdir $home/chaining-box"

    echo "  - Killing previous SF processes"
    sshfut $h "for p in \$(ps aux | grep loopback | awk '{print \$2}'); do sudo kill -9 \$p; done"
    sshfut $h "sudo tc filter del dev $iface ingress"

    srcip="$(echo ${addresses[$src]} | cut -d'/' -f2)"

    if [ "$tc_redir" == "true" ]; then
        echo "  - Starting tc_redirect"
        sshfut $h "$cfgcmd redir $iface $ksrcdir $home/chaining-box"
    else
        echo "  - Starting loopback.py"
        sshfut $h "sudo nohup $home/chaining-box/test/loopback.py $iface $srcip > /dev/null 2>&1 &"
    fi

done

# dst - no configuration to be done


#### Configure SFC rules

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

# Reverse space-separated strings
function reverse_bytes {
    PAIRS="$@"
    echo -n "$PAIRS " | tac -s ' '
}

function install_rule {
    LAYER="$1" # TC, XDP...
    MAP="$2"
    KEY="$3"
    VALUE="$4"
    REVKEY="$5"
    REVVAL="$6"

    # Account for odd sized strings
    KEY=$(turn_even $KEY)
    VALUE=$(turn_even $VALUE)

    KEY=$(split_string $KEY)
    VALUE=$(split_string $VALUE)

    if [ "$REVKEY" = "1" ]; then KEY=$(reverse_bytes $KEY); fi
    if [ "$REVVAL" = "1" ]; then VALUE=$(reverse_bytes $VALUE); fi

    echo "bpftool map update pinned /sys/fs/bpf/$LAYER/globals/$MAP key hex $KEY value hex $VALUE any"
}


# Flow definitions

IPSRC=$(echo ${addresses[$src]} | cut -d'/' -f2)
IPDST=$(echo ${addresses[$dst]} | cut -d'/' -f2)
SPORT=10000
DPORT=20000

UDP_FLOW="$(tuple_to_hex $IPSRC $IPDST $SPORT $DPORT 17)"
TCP_FLOW="$(tuple_to_hex $IPSRC $IPDST $SPORT $DPORT 6)"
ICMP_FLOW="$(tuple_to_hex $IPSRC $IPDST 0 0 1)"

SPI="1" # Same SPI for all flows

#echo -e "TCP FLOW: \n $TCP_FLOW\nUDP FLOW:\n $UDP_FLOW\nICMP_FLOW:\n $ICMP_FLOW"

# Install rules on classifier
echob "~~> Configuring cls rules ($src)"

MAC="$(echo ${addresses[$src]} | cut -d'/' -f1)"
cmd="$(install_rule tc src_mac '00' ${MAC//:/})"
$sshcmd $src "sudo $cmd"

for flow in $UDP_FLOW $TCP_FLOW $ICMP_FLOW; do

    NMAC="$(echo ${addresses[${hosts[1]}]} | cut -d'/' -f1)"
    cmd=$(install_rule tc cls_table $flow "$(printf '%06x%02x' $SPI 255)${NMAC//:/}")
    $sshcmd $src "sudo $cmd"

done

# Install rules on SFs
for i in ${!hosts[@]}; do
    # Get host name
    h=${hosts[$i]}
    echob "~~> Configuring SF rules ($h)"

    # Install src_mac rule
    MAC="$(echo ${addresses[$h]} | cut -d'/' -f1)"
    KEY="00"
    VALUE="${MAC//:/}" # Remove colons from MAC"
    cmd="$(install_rule tc src_mac $KEY $VALUE)"
    #$sshcmd "$cmd"
    $sshcmd $h "sudo $cmd"

    # Install redirect rules, if necessary
    if [ "$tc_redir" == "true" ]; then

        # Get $iface ifindex to configure tc_redirect prog
        ifindex=$($sshcmd $h "ip link show $iface | head -n1 | awk  -F ':' '{print \$1}'")
        KEY="00000000"
        VALUE="$(number_to_hex $ifindex 8)"
        $sshcmd $h "sudo $(install_rule tc egress_ifindex $KEY $VALUE 0 1)"

        KEY="00000000"
        VALUE="$(ip_to_hex $srcip)"
        $sshcmd $h "sudo $(install_rule tc srcip $KEY $VALUE)"
    fi

    # If last SF, end of chain
    if [ $i -eq ${#hosts[@]} ]; then
        NEXT_MAC="00:00:00:00:00:00" # End of chain. Placeholder MAC.
    else
        NEXT_MAC="$( echo ${addresses[${hosts[$((i+1))]}]}  | cut -d'/' -f1)"
    fi

    # Install SFC forwarding rule
    SI="$((255-$i))"
    [[ $i = ${#hosts[@]} ]] && END_OF_CHAIN="01" || END_OF_CHAIN="00"
    KEY="$(number_to_hex $SPI 6)$(number_to_hex $SI 2)"
    VALUE="${END_OF_CHAIN}${NEXT_MAC//:/}" # Remove colons from MAC"

    cmd="$(install_rule tc fwd_table $KEY $VALUE)"
    $sshcmd $h "sudo $cmd"

done
