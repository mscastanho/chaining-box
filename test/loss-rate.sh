#!/usr/bin/env bash

if [ $EUID -ne 0 ]; then
    echo "Please run with sudo."
    exit 1
fi

delay=5
rounds=1
seconds=10
outfile="out_iperf.json"
csv_hdr="Rate;1;2;3;4;5;6;7;8;9;10;Avg% Dropped;StDev\n"
#echo $csv_hdr > lossrate_output.csv

for rate in {50..950..50}; do
    line="$rate"
    rateM=$(printf "%dM" $rate)
    echo "Starting to send at ${rateM}bps..."
    for i in $(seq 1 $rounds); do
        echo "$i out of $rounds"
        iperf3 -c 10.10.10.13 --cport 1000 -p 2000 -l 1400 -u -b $rateM -t $seconds -J > $outfile
        line+=";$(jq '.end.streams[0].udp.lost_percent' < $outfile)"
        sleep $delay
    done
    echo "$line" >> lossrate_output.csv
done
