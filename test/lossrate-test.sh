#!/usr/bin/env bash

if [ $EUID -ne 0 ]; then
    echo "Please run with sudo."
    exit 1
fi

if [ -z "$1" ]; then
    echo "Usage: $0 <server-ip>"
    exit 1
fi

# Experiment metadefs
outdir="/tmp/lossrate-$(date +%Y%m%d-%H%M%S)"
summfile="$outdir/lossrate-summary.csv"
tmpfile="/tmp/out_iperf.json"

# iperf parameters
serverip="$1"
delay=5
rounds=3
duration=30
interval=1
sport=1000
dport=2000
length=1400

rate_min=50
rate_step=50
rate_max=950
# Create dir to store test data
mkdir -p $outdir

# Create summary file (erase if it already exists)
echo -n "" > $summfile

# Set CSV file header based on number of rounds
csv_hdr="Rate"
for i in `seq 1 $rounds`; do 
    csv_hdr="$csv_hdr;R$i"
done

echo $csv_hdr > $summfile

# Switch to output dir to start creating files there
cd $outdir

# Run experiment
for rate in `seq $rate_min $rate_step $rate_max`; do
    # Create dir to store results for this experiment
    rdir=" $(printf %04d $rate)"
    mkdir $rdir
    cd $rdir

    line="$rate"
    rateM=$(printf "%dM" $rate)
    echo "Starting to send at ${rateM}bps..."
    
    for i in $(seq 1 $rounds); do
        outfile="round$i.json"

        echo "$i out of $rounds"
        iperf3 -c $serverip --cport $sport -p $dport -l $length -u -b $rateM -t $duration -i $interval -J > $outfile
        line+=";$(jq '.end.streams[0].udp.lost_percent' < $outfile)"
        sleep $delay
    done

    echo "$line" >> $summfile
    
    cd .. # Go back to main dir
done
