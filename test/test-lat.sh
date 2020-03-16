#!/usr/bin/env bash

function measure_latency {
  name="$1"

  # We need to generate a slightly smaller packet thant the MTU
  # to guarantee we have space for the extra encapsulation. Subtracting
  # the bytes needed for that, our effective MTU is of 1462. The packet
  # size given to ping is the size of the *data*, that value will be added
  # to ethernet (14B) + ip (20B) + icmp (8B) = 42B. So we need to set the
  # size to 1462 - 42 = 1420 to create a packet of size 1462 in the end.
  datasz=1420

  # echo "Priming OVS' learning switch table..."
  sudo ip netns exec ns0 ping -q -c 1 10.10.0.2 > /dev/null 2>&1
  sudo ip netns exec ns1 ping -q -c 1 10.10.0.1 > /dev/null 2>&1

  echo "Starting measurement for case '$name'..."
  sudo ip netns exec ns0 ping -s $datasz -i $interval -c $count 10.10.0.2 > $tmpfile

  results=$(cat $tmpfile | grep -o -e 'time=[0-9]*\.[0-9]*' | cut -d'=' -f2 | tr '\n' ' ' | head -c-1)
  # results=($(cat $tmpfile | grep -o -e 'time=[0-9]*\.[0-9]*' | cut -d'=' -f2 | sort -n))
  # med_idx=$(echo "${#results[@]} / 2" | bc )
  # rttmedian="${results[$med_idx]}"
  tx="$(grep -o -e '[0-9]* packets transmitted' $tmpfile | awk '{print $1}')"
  rx="$(grep -o -e '[0-9]* received' $tmpfile | awk '{print $1}')"
  # stats="$(grep rtt $tmpfile | awk '{print $4}')"
  # rttmin=$(echo $stats | cut -d'/' -f1)
  # rttavg=$(echo $stats | cut -d'/' -f2)
  # rttmax=$(echo $stats | cut -d'/' -f3)
  # rttmdev=$(echo $stats | cut -d'/' -f4)

  # Write results to output file
  echo "$name;$tx;$rx;$results" >> $outfile

  if [ $rx != $tx ]; then
    return 1
  else
    return 0
  fi
}

source common.sh

outfile="${logdir}/latency.csv"
tmpfile="${logdir}/lat.res"
cfgdir="chains-config"

# Ping parameters
count=120
interval="0.1"

# Experiment type
experiment="$1"

# Write CSV headers to results file
echo "name;tx;rx;rtts" > $outfile

tests=(
#  "${cfgdir}/len-2-noveth.json"
#  "${cfgdir}/len-2-veth.json"
)

# Generate test cases based on the experiment we want
if [ "$experiment" == "lengths" ]; then
  # Generate config files for different lengths
  for len in `seq 2 2 20`; do
    noveth="$tmpcfgdir/len-${len}-noveth.json"
    ${cfgdir}/gen-len-tests.sh $len 100 true > $noveth
    tests+=($noveth)

    veth="$tmpcfgdir/len-${len}-veth.json"
    ${cfgdir}/gen-len-tests.sh $len 100 false > $veth
    tests+=($veth)
  done
elif [ "$experiment" == "star" ]; then
  # Generate config files for different lengths in a start topology
  for len in `seq 1 1 10`; do
    star="$tmpcfgdir/len-${len}-star.json"
    ${cfgdir}/gen-star-tests.sh $len 100 true > $star
    tests+=($star)

    linear="$tmpcfgdir/len-${len}-linear.json"
    ${cfgdir}/gen-len-tests.sh $len 100 true > $linear
    tests+=($linear)
  done
elif [ "$experiment" == "single" ]; then
    single="$tmpcfgdir/len-2-len.json"
    ${cfgdir}/gen-len-tests.sh 2 100 true > $single
    tests+=($single)
else
  echo "Unknown experiment type: '$experiment'" && exit 1
fi

for t in ${tests[@]}; do
  # Setup environment
  cbox_deploy_ovs $t

  # strip dir path and extension
  simple_name="$(bname=$(basename $t); echo ${bname%.*})"
  measure_latency ${simple_name}

  if [ $? -ne 0 ]; then
    echo "Something went wrong, trying again..."
    cbox_deploy_ovs $t
    measure_latency ${simple_name}
  fi

done


# Measure latency without the chaining
# This needs to come last as we need to use the bridge created by a previous test
sudo ovs-ofctl del-flows cbox-br
sudo ovs-ofctl add-flow cbox-br "actions=NORMAL"
measure_latency "baseline"
