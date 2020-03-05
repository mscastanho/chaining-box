#!/usr/bin/env bash

function measure_latency {
  echo "Starting ping..."
  sudo ip netns exec ns0 ping -q -i $interval -c $count 10.10.0.2 | tee $tmpfile

  chainlen="$1"
  tx="$(grep -o -e '[0-9]* packets transmitted' $tmpfile | awk '{print $1}')"
  rx="$(grep -o -e '[0-9]* received' $tmpfile | awk '{print $1}')"
  loss="$(grep -o -e '[0-9]*% packet loss' $tmpfile | awk '{print $1}')"
  stats="$(grep rtt $tmpfile | awk '{print $4}')"
  rttmin=$(echo $stats | cut -d'/' -f1)
  rttavg=$(echo $stats | cut -d'/' -f2)
  rttmax=$(echo $stats | cut -d'/' -f3)
  rttmdev=$(echo $stats | cut -d'/' -f4)

  # Write results to output file
  echo "$chainlen;$tx;$rx;$loss;$rttmin;$rttavg;$rttmax;$rttmdev" >> $outfile
}

source common.sh

outfile="${logdir}/latency.csv"
tmpfile="${logdir}/lat.res"
# Interface used to send ping requests from
ping_iface="enp2s0np0"

# Ping parameters
count=100
interval="0.5"

echo "Deleting old resources..."
sudo ovs-vsctl del-br cbox-br
sudo ip netns exec ns0 tc filter del dev $ping_iface egress
pkill cb_manager
for c in $(list_sfs); do
{
  docker kill $c && docker rm $c
} > /dev/null 2>&1
done

echo "Creating containers..."
sudo ${objdir}/cb_docker -c $chainscfg -d ../ -n ovs > $dockerlog 2>&1 || fail_exit

echo "Adding physical ifaces to OVS bridge..."
sudo ovs-vsctl add-port cbox-br $phys0
sudo ovs-vsctl add-port cbox-br $phys1

echo "Starting manager..."
${objdir}/cb_manager $chainscfg > $managerlog 2>&1 &

echo "Priming OVS' learning switch table..."
for c in $(list_sfs); do
  docker exec -it $c ping -q -c 1 -i 0.1 192.168.100.250 > /dev/null 2>&1
done

sleep 3

# Write CSV headers to results file
echo "chainlen;tx;rx;loss;rtt_min;rtt_avg;rtt_max;rtt_mdev" > $outfile

# Measure latency without the chaining
measure_latency 0

# Measure with different chain lenghts

## Load classifier
### MAC address of the RX interface of the first SF in the chain (sf1)
nexthopmac=$(docker exec -it sf1 ip link show eth1 | grep ether | awk '{print $2}')
sudo ip netns exec ns0 bash ${testdir}/config-cls.sh $ping_iface $nexthopmac
measure_latency 2
