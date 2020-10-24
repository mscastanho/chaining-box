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

import plot_defaults as defs

def process_raw_data(filename):
    lines = None
    results = { 'baseline': {}, 'star': {}, 'linear': {} }

    with open(filename) as csvin:
        reader = csv.DictReader(csvin,delimiter=';')
        for line in reader:

            name = line['name']
            key = ''
            length = 0

            if name == 'baseline':
                key = 'baseline'
            else:
                key = 'star' if '-star' in name else 'linear'
                length = name.split('-')[1]

            rtts=list(map(float,line['rtts'].split(' ')))
            rtts.sort()

            # Remove some outliers
            rtts = rtts[5:-5]

            results[key][int(length)] = {
                'rx':           int(line['rx']),
                'tx':           int(line['tx']),
                'rtt_min':      np.min(rtts),
                'rtt_avg':      np.average(rtts),
                'rtt_max':      np.max(rtts),
                'rtt_mdev':     np.std(rtts),
                'rtt_median':   np.median(rtts),
            }

        #  print("Results for >> {} <<:".format(filename))
        #  for k,v in results.items():
            #  print("{}: {}".format(k,v))
        #  print()

    return results

def reglin(x,y):
    xa = np.array(x)
    ya = np.array(y)
    A = np.vstack([xa, np.ones(len(xa))]).T
    m,c = np.linalg.lstsq(A, y, rcond=None)[0]
    print("Derivative: {} // Linear coef.: {}".format(m,c))

    return xa, m*xa + c

def plot_sidebyside(results, outfile):
    plt.close()

    baseline_rtt = results['baseline'][0]['rtt_median']

    # How many functions?
    nfuncs = len(results['linear'])

    prev_linear_rtt = baseline_rtt
    prev_star_rtt = baseline_rtt

    ind = np.arange(2)
    width = 0.25
    funcs = ['VPN', 'Flow Monitor', 'Firewall', 'Load Balancer']

    colors = ['tab:blue', 'tab:orange', 'tab:green', 'tab:purple' ]
    patterns = ['x', '.', '/', 'o']
    for i in range(nfuncs,0,-1):
        linear_rtt = results['linear'][i]['rtt_median']
        star_rtt = results['star'][i]['rtt_median']

        y = (linear_rtt, star_rtt)
        plt.bar(ind, y, width, edgecolor='k', label=funcs[i-1],
                color=colors[i-1])#, hatch=patterns[i-1])


    ybaseline = (baseline_rtt, baseline_rtt)
    left, right = plt.xlim()
    plt.hlines(ybaseline, left, right, color='r', linestyle='--', label='Baseline') # ybaseline, width, yerr=ybaselineerr, edgecolor='k' ,hatch='/')

    plt.yticks(fontsize=defs.ftsz['ticks'])
    plt.xticks(ind, ('Chaining-Box', 'PhantomSFC-like'), fontsize=defs.ftsz['axes'])

    plt.legend(fontsize=defs.ftsz['legend'], loc='upper center')
    plt.grid(alpha=0.3, linestyle='--')
    plt.ylabel('Latency (ms)',  fontsize=defs.ftsz['axes'])
    plt.tight_layout()
    plt.savefig(outfile)

    plt.show()

    return

if __name__ == '__main__':
    pktsizes = set()

    if len(sys.argv) != 2:
        print("Usage: {} <csv-file>".format(sys.argv[0]))
        exit(1)

    res = process_raw_data(sys.argv[1])

    # import pprint
    # pp = pprint.PrettyPrinter(indent=2)
    # pp.pprint(res)

    plot_sidebyside(res, 'realfuncs-lat.pdf')
