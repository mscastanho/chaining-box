#!/usr/bin/env bash

function random_ip {
  echo "$(($RANDOM % 256)).$(($RANDOM % 256)).$(($RANDOM % 256)).$(($RANDOM % 256))"
}

function random_rule {
  sport="$(($RANDOM + $RANDOM))"
  dport="$(($RANDOM + $RANDOM))"
  ipsrc=$(random_ip)
  ipdst=$(random_ip)
  mask1="$(($RANDOM%33))"
  mask2="$(($RANDOM%33))"
  proto="$([ $(($RANDOM % 2)) -eq 0 ] && echo udp || echo tcp)"
  action="$([ $(($RANDOM % 2)) -eq 0 ] && echo drop || echo tcp)"
  echo "protocol ip flower src_ip $ipsrc/$mask1 dst_ip $ipdst/$mask2 ip_proto $proto dst_port $dport src_port $sport action drop"
}

NRULES="$( [ -n "$1" ] && echo $1 || echo 100 )"
for i in `seq 1 $NRULES`; do
  echo "$(random_rule)"
done
