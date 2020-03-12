#!/usr/bin/env python3
import sys
import os
sys.path.append(os.path.abspath('../plots'))
import matplotlib.pyplot as plt
import numpy as np
import csv
import fnmatch
import operator
from matplotlib.patches import Patch

# Our defaults to plot stuff. Ex: colors, progs...

# ====== Script params ======
max_speed_gbps = 10 # Gbps
#  results_dir = os.path.dirname(os.path.abspath(__file__)) + '/results-old'

# Infer tests from .csv files in this directory
#  tests = [f.split('.')[0] for f in fnmatch.filter(os.listdir(results_dir), '*.csv')]
# ===========================

def process_raw_data(filename):
    lines = None
    results = { 'baseline': [], 'veth': [], 'noveth': [] }

    #print(filename)
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
            
            results[key].append({
                'len':          int(length),
                'rx':           int(line['rx']),
                'tx':           int(line['tx']),
                'rtt_min':      float(line['rtt_min']),
                'rtt_avg':      float(line['rtt_avg']),
                'rtt_max':      float(line['rtt_max']),
                'rtt_mdev':     float(line['rtt_mdev']),
                'rtt_median':   float(line['rtt_median']),
            })

        print("Results for >> {} <<:".format(filename))
        for k,v in results.items():
            print("{}: {}".format(k,v))

        print()

    return results

def plot_lines(results,outfile,ymax):
    plt.close()
    
    # Latency without veth pairs
    x1 = [v['len'] for v in results['noveth']]
    y1 = [v['rtt_median'] for v in results['noveth']]

    # Latency without veth pairs
    x2 = [v['len'] for v in results['veth']]
    y2 = [v['rtt_median'] for v in results['veth']]

    plt.grid(alpha=0.3, linestyle='--')
    plt.plot(x1,y1,'x-')
    plt.plot(x2,y2,'rx-')

    #  plt.xticks(x1)
    plt.title('Latency: veth vs no-veth', fontsize=14)
    plt.ylabel('Latency (ms)',  fontsize=13)
    plt.xlabel('Chain Length (# SFs)', fontsize=13)
    plt.tight_layout()
    #  plt.savefig(outfile)
    plt.show()

    return

def plot_stacked(results, outfile):
    plt.close()
   
    # Latency with veth pairs
    xveth = [v['len'] for v in results['veth']]
    yveth = [v['rtt_median'] for v in results['veth']]

    # Latency without veth pairs
    ynoveth = [v['rtt_median'] for v in results['noveth']]
    
    # Baseline latency
    ybaseline = results['baseline'][0]['rtt_median']

    diff1 = [y - ybaseline for y in yveth]
    diff2 = list(map(operator.sub, ynoveth, yveth))
    #  print('baseline: ' + str(ybaseline))
    #  print('diff1: ' + str(len(diff1)))
    #  print('diff2: ' + str(len(diff2)))

    # The x locations for the bars
    ind = np.arange(len(xveth))
    width = 0.35

    plt.grid(alpha=0.3, linestyle='--')

    plt.bar(ind, ybaseline, width)
    plt.bar(ind, diff1, width, bottom=ybaseline)
    plt.bar(ind, diff2, width, bottom=yveth)

    plt.xticks(ind,xveth)
    plt.title('Latency: veth vs no-veth', fontsize=14)
    plt.legend(['Baseline', 'Veth pairs', 'No veth pairs'])
    plt.ylabel('Latency (ms)',  fontsize=13)
    plt.xlabel('Chain Length (# SFs)', fontsize=13)
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
    #  pktsizes.update(list(res.keys()))
    #  pktsizes = list(pktsizes)
    #  pktsizes.sort()

    plot_stacked(res, 'latency.pdf')