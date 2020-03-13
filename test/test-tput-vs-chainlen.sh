#!/usr/bin/env bash

function measure_throughput {
  name="$1"

  echo "Starting pktgen..."
  sudo $pktgen $pktgen_args -f tput.lua

  echo "$name;$(cat ${tmpfile})" >> $outfile
}

source common.sh

outfile="${logdir}/tput-vs-chainlen.csv"
tmpfile="${logdir}/tput.csv"
cfgdir="chains-config"

# Experiment type
experiment="$1"

# Write CSV headers to results file
echo "name;trial number;tx packets;rx packets;dropped packets;duration(ms)" > $outfile

tests=()

# Generate test cases based on the experiment we want
if [ "$experiment" == "lengths" ]; then
  # Generate config files for different lengths
  for len in `seq 1 1 6`; do
    noveth="$tmpcfgdir/len-${len}-noveth.json"
    ${cfgdir}/gen-len-tests.sh $len 100 true > $noveth
    tests+=($noveth)

    veth="$tmpcfgdir/len-${len}-veth.json"
    ${cfgdir}/gen-len-tests.sh $len 100 false > $veth
    tests+=($veth)
  done
elif [ "$experiment" == "star" ]; then
  # Generate config files for different lengths in a start topology
  for len in `seq 1 1 6`; do
    star="$tmpcfgdir/len-${len}-star.json"
    ${cfgdir}/gen-star-tests.sh $len 100 true > $star
    tests+=($star)

    linear="$tmpcfgdir/len-${len}-linear.json"
    ${cfgdir}/gen-len-tests.sh $len 100 true > $linear
    tests+=($linear)
  done
else
  echo "Unknown experiment type: '$experiment'" && exit 1
fi

for t in ${tests[@]}; do
  # Setup environment
  cbox_deploy_ovs $t

  # strip dir path and extension
  simple_name="$(bname=$(basename $t); echo ${bname%.*})"
  measure_throughput ${simple_name}

  if [ $? -ne 0 ]; then
    echo "Something went wrong, trying again..."
    cbox_deploy_ovs $t
    measure_throughput ${simple_name}
  fi
done

# Measure throughput without the chaining
# This needs to come last as we need to use the bridge created by a previous test
sudo ovs-ofctl del-flows cbox-br
sudo ovs-ofctl add-flow cbox-br "actions=NORMAL"
measure_throughput "baseline"
