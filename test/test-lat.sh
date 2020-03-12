#!/usr/bin/env bash

function measure_latency {
  name="$1"

  # echo "Priming OVS' learning switch table..."
  sudo ip netns exec ns0 ping -q -c 1 10.10.0.2 > /dev/null 2>&1
  sudo ip netns exec ns1 ping -q -c 1 10.10.0.1 > /dev/null 2>&1

  echo "Starting measurement for case '$name'..."
  sudo ip netns exec ns0 ping -i $interval -c $count 10.10.0.2 | tee $tmpfile

  results=($(cat $tmpfile | grep -o -e 'time=[0-9]*\.[0-9]*' | cut -d'=' -f2 | sort -n))
  med_idx=$(echo "${#results[@]} / 2" | bc )
  rttmedian="${results[$med_idx]}"
  tx="$(grep -o -e '[0-9]* packets transmitted' $tmpfile | awk '{print $1}')"
  rx="$(grep -o -e '[0-9]* received' $tmpfile | awk '{print $1}')"
  #loss="$(grep -o -e '[0-9]*% packet loss' $tmpfile | awk '{print $1}')"
  stats="$(grep rtt $tmpfile | awk '{print $4}')"
  rttmin=$(echo $stats | cut -d'/' -f1)
  rttavg=$(echo $stats | cut -d'/' -f2)
  rttmax=$(echo $stats | cut -d'/' -f3)
  rttmdev=$(echo $stats | cut -d'/' -f4)
  # Write results to output file
  echo "$name;$tx;$rx;$rttmin;$rttavg;$rttmax;$rttmedian;$rttmdev" >> $outfile
}

source common.sh

outfile="${logdir}/latency.csv"
tmpfile="${logdir}/lat.res"
cfgdir="chains-config"

# Ping parameters
count=100
interval="0.5"

# Write CSV headers to results file
echo "name;tx;rx;rtt_min;rtt_avg;rtt_max;rtt_median;rtt_mdev" > $outfile

tests=(
  "${cfgdir}/len-2-noveth.json"
  "${cfgdir}/len-2-veth.json"
)

for t in ${tests[@]}; do
  # Setup environment
  cbox_deploy_ovs $t

  # strip dir path and extension
  simple_name="$(bname=$(basename $t); echo ${bname%.*})"
  measure_latency ${simple_name}
done


# Measure latency without the chaining
# This needs to come last as we need to use the bridge created by a previous test
sudo ovs-ofctl del-flows cbox-br
sudo ovs-ofctl add-flow cbox-br "actions=NORMAL"
measure_latency "baseline"
