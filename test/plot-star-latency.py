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

            rtts=list(map(float,line['rtts'].split(' ')))
            rtts.sort()

            # Remove some outliers
            rtts = rtts[5:-5]

            results[key].append({
                'len':          int(length),
                'rx':           int(line['rx']),
                'tx':           int(line['tx']),
                'rtt_min':      np.min(rtts),
                'rtt_avg':      np.average(rtts),
                'rtt_max':      np.max(rtts),
                'rtt_mdev':     np.std(rtts),
                'rtt_median':   np.median(rtts),
            })

        #  print("Results for >> {} <<:".format(filename))
        #  for k,v in results.items():
            #  print("{}: {}".format(k,v))
        #  print()

    return results

def plot_stacked(results, outfile):
    plt.close()

    # Latency with linear topo
    xlin = [v['len'] for v in results['linear']]
    ylin = [v['rtt_median'] for v in results['linear']]

    # Latency with start topology
    ystar = [v['rtt_median'] for v in results['star']]

    # Baseline latency
    ybaseline = results['baseline'][0]['rtt_median']

    diff1 = [y - ybaseline for y in ylin]
    diff2 = list(map(operator.sub, ystar, ylin))
    #  print('baseline: ' + str(ybaseline))
    #  print('diff1: ' + str(len(diff1)))
    #  print('diff2: ' + str(len(diff2)))

    # The x locations for the bars
    ind = np.arange(len(xlin))
    width = 0.35

    plt.grid(alpha=0.3, linestyle='--')

    plt.bar(ind, ybaseline, width)
    plt.bar(ind, diff1, width, bottom=ybaseline)
    plt.bar(ind, diff2, width, bottom=ylin)

    plt.xticks(ind,xlin)
    plt.title('Latency: linear vs start topology', fontsize=14)
    plt.legend(['Network Overhead', 'Chaining-Box', 'RFC 7665'])
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
