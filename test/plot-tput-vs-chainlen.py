#!/usr/bin/env python3
import sys
import os
sys.path.append(os.path.abspath('../plots'))
import matplotlib.pyplot as plt
import numpy as np
import csv
import fnmatch
from matplotlib.patches import Patch

max_speed_gbps = 10 # Gbps

def process_raw_data(filename):
    lines = None
    results = { 'baseline': [], 'veth': [], 'noveth': [] }

    with open(filename) as csvin:
        reader = csv.DictReader(csvin,delimiter=';')
        for line in reader:

            name = line['name']
            key = ''
            length = 0

            if name == 'baseline':
                key = 'baseline'
            else:
                key = 'veth' if '-veth' in name else 'noveth'
                length = name.split('-')[1]

            tx        = int(line['tx packets'])
            rx        = int(line['rx packets'])
            dropped   = int(line['dropped packets'])
            duration  = int(line['duration(ms)'])
            rate_gbps = max_speed_gbps*(rx/tx)
            rate_mpps = (rx/(duration/1000))/10**6

            results[key].append({
                'len':          length,
                'rx':           rx,
                'tx':           tx,
                'dropped':      dropped,
                'duration':     duration,
                'rate_gbps':    rate_gbps,
                'rate_mpps':    rate_mpps,
            })

        print("Results for >> {} <<:".format(filename))
        for k,v in results.items():
            print("{}: {}".format(k,v))

        print()

    return results

def plot_sidebyside(results, outfile):
    plt.close()

    # Latency with veth pairs
    xveth = [v['len'] for v in results['veth']]
    yveth = [v['rate_gbps'] for v in results['veth']]

    # Latency without veth pairs
    ynoveth = [v['rate_gbps'] for v in results['noveth']]

    # Baseline latency
    ybaseline = results['baseline'][0]['rate_gbps']

    # The x locations for the bars
    ind = np.arange(len(xveth))
    width = 0.25

    plt.grid(alpha=0.3, linestyle='--')

    #  plt.bar(ind - 1.5*width, ybaseline, width, edgecolor='k' ,hatch='/')
    plt.bar(ind - 0.5*width, yveth, width, edgecolor='k', hatch='o')
    plt.bar(ind + 0.5*width, ynoveth, width, edgecolor='k', hatch='x')

    plt.xticks(ind, xveth, fontsize=14)
    plt.yticks(fontsize=14)
    #  plt.title('Latency: Direct Links vs No Direct Links', fontsize=14)
    plt.legend(['w/ direct links', 'w/o direct links'], fontsize=14)
    plt.ylabel('Throughput (Gbps)',  fontsize=18)
    plt.xlabel('Chain Length (# SFs)', fontsize=18)
    plt.tight_layout()
    #  plt.savefig(outfile)
    plt.show()

    return

if __name__ == '__main__':
    pktsizes = set()

    if len(sys.argv) != 2:
        print("Usage: {} <csv-file>".format(sys.argv[0]))
        exit(1)

    res = process_raw_data(sys.argv[1])
    plot_sidebyside(res,'tput-gbps.pdf')
