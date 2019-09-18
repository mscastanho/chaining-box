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
delay=7
rounds=10
duration=55
interval=1
sport=10000 
dport=20000
length=1400
omit=2 # Omit x seconds of warmup from results

rate_min=50
rate_step=50
rate_max=950

max_retries=2
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
    
    i=1
    retries=0
    while [ $i -le $rounds ]; do
        outfile="round$i.json"

        echo -n "(${i}/${rounds}) : "
        iperf3 -c $serverip --cport $dport -p $sport -l $length -u -b $rateM -t $duration -i $interval -O $omit -R -J > $outfile
        result="$(jq '.end.streams[0].udp.lost_percent' < $outfile)"
        echo -n "$result"

        # Check if measurement failed (== null or < 0)
        if [ "$result" = "null" ] || [ $(echo "$result < 0" | bc) == "1" ] ; then
            echo " <- Weird result, retrying... "
            retries=$((retries + 1))

            # Save error output with error for debugging purposes
            mkdir -p "error"
            mv $outfile "error/round$i-e${retries}.json"

            if [ $retries -gt $max_retries ]; then
                echo -e "\n\nFAIL: Retried too many times already. Exiting..."
                exit 1
            fi
        else
            echo ""
            retries=0
            i=$((i+1))
            line+=";$result"

        fi
        
        sleep $delay
    done

    echo "$line" >> $summfile
    
    cd .. # Go back to main dir
done
