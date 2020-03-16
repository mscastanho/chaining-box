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
    results = { 'baseline': [], 'star': [], 'linear': [] }

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
                key = 'star' if '-star' in name else 'linear'
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

def reglin(x,y):
    xa = np.array(x)
    ya = np.array(y)
    A = np.vstack([xa, np.ones(len(xa))]).T
    m,c = np.linalg.lstsq(A, y, rcond=None)[0]
    return xa, m*xa + c

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

def plot_sidebyside(results, outfile):
    plt.close()

    # Latency with linear pairs
    xlinear = [v['len'] for v in results['linear']]
    ylinear = [v['rtt_median'] for v in results['linear']]
    ylinearerr = [v['rtt_mdev'] for v in results['linear']]

    # Latency without linear pairs
    ystar = [v['rtt_median'] for v in results['star']]
    ystarerr = [v['rtt_mdev'] for v in results['star']]

    # Baseline latency
    ybaseline = results['baseline'][0]['rtt_median']
    ybaselineerr = results['baseline'][0]['rtt_mdev']

    # The x locations for the bars
    ind = np.arange(len(xlinear))
    width = 0.25
    print(ind)

    plt.grid(alpha=0.3, linestyle='--')

    plt.bar(ind + 0.5*width, ystar, width, yerr=ystarerr, edgecolor='k', hatch='o', label='Star')
    plt.bar(ind - 0.5*width, ylinear, width, yerr=ylinearerr, edgecolor='k', hatch='x', label='Chaining-Box')

    # Plot horizontal line with baseline
    left, right = plt.xlim()
    plt.hlines(ybaseline, left, right, color='r', linestyle='--', label='Baseline') # ybaseline, width, yerr=ybaselineerr, edgecolor='k' ,hatch='/')

    plt.xticks(ind, xlinear, fontsize=14)
    plt.yticks(fontsize=14)
    #  plt.title('Latency: Direct Links vs No Direct Links', fontsize=14)
    plt.legend(fontsize=14)

    # Plot linear regressions
    xlinrl, ylinrl = reglin(xlinear,ylinear)
    xstarrl, ystarrl = reglin(xlinear,ystar)
    plt.plot(ind + 0.5*width, ystarrl, 'blue', marker='o', ms=7, linestyle='-.')
    plt.plot(ind - 0.5*width, ylinrl, 'orange', marker='x', ms=7, linestyle='-.')

    plt.ylabel('Latency (ms)',  fontsize=18)
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
    #  pktsizes.update(list(res.keys()))
    #  pktsizes = list(pktsizes)
    #  pktsizes.sort()

    plot_sidebyside(res, 'latency.pdf')
