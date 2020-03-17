#!/usr/bin/env bash

function measure_latency {
  name="$1"
  pktsz=$2

  # Set default value for packet size
  [ -z "$pktsz" ] && pktsz=1462

  # We need to generate a slightly smaller packet thant the MTU
  # to guarantee we have space for the extra encapsulation. Subtracting
  # the bytes needed for that, our effective MTU is of 1462.
  # The packet size given to ping is the size of the *data*, that value
  # will be added to ethernet (14B) + ip (20B) + icmp (8B) = 42B.
  # E.g. To set our final packet size to 1462 we need to set the size to
  # 1462 - 42 = 1420 to create a packet of size 1462 in the end.
  datasz=$(($pktsz - 42))

  # echo "Priming OVS' learning switch table..."
  sudo ip netns exec ns0 ping -q -c 1 10.10.0.2 > /dev/null 2>&1
  sudo ip netns exec ns1 ping -q -c 1 10.10.0.1 > /dev/null 2>&1

  echo "Starting measurement for case '$name' and size ${pktsz} Bytes..."
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
  echo "$name;$pktsz;$tx;$rx;$results" >> $outfile

  if [ $rx != $tx ]; then
    return 1
  else
    return 0
  fi
}

function run_test {
  pktsz=$1

  # strip dir path and extension
  simple_name="$(bname=$(basename $t); echo ${bname%.*})"
  measure_latency ${simple_name} $pktsz

  if [ $? -ne 0 ]; then
    echo "Something went wrong, trying again..."
    cbox_deploy_ovs $t
    measure_latency ${simple_name} $pktsz
  fi
}

function setup_baseline {
  pktsz=$1

  # Measure latency without the chaining
  # This needs to come last as we need to use the bridge created by a previous test
  sudo ovs-ofctl del-flows cbox-br
  sudo ovs-ofctl add-flow cbox-br "actions=NORMAL"
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
echo "name;pktsz;tx;rx;rtts" > $outfile

tests=(
#  "${cfgdir}/len-2-noveth.json"
#  "${cfgdir}/len-2-veth.json"
)

# Generate test cases based on the experiment we want
if [ "$experiment" == "lengths" ]; then
  # Generate config files for different lengths
  for len in `seq 1 1 10`; do
    noveth="$tmpcfgdir/len-${len}-noveth.json"
    ${cfgdir}/gen-len-tests.sh $len 100 true > $noveth
    tests+=($noveth)

    veth="$tmpcfgdir/len-${len}-veth.json"
    ${cfgdir}/gen-len-tests.sh $len 100 false > $veth
    tests+=($veth)
  done

  for t in ${tests[@]}; do
    # Setup environment
    cbox_deploy_ovs $t

    for sz in 64 128 256 512 1024 1280 1462; do
      run_test $sz
    done
  done

  setup_baseline

  for sz in 64 128 256 512 1024 1280 1462; do
    measure_latency "baseline" $pktsz
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

  for t in ${tests[@]}; do
    # Setup environment
    cbox_deploy_ovs $t
    run_test
  done

  setup_baseline
  measure_latency "baseline"

elif [ "$experiment" == "single" ]; then
    single="$tmpcfgdir/len-2-len.json"
    ${cfgdir}/gen-len-tests.sh 2 100 true > $single
    tests+=($single)

    run_tests
    baseline
else
  echo "Unknown experiment type: '$experiment'" && exit 1
fi

