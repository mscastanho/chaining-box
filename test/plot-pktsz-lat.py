#!/usr/bin/env python3
import sys
import os
sys.path.append(os.path.abspath('../plots'))
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Patch

import lat_parse as lp
import plot_defaults as defs

def plot(results, outfile, lens=None):
    plt.close()

    # Get set of lenghts
    # REALLY HACKY, CAREFUL NOT TO GET INFECTED!!!
    if not lens:
        lens = list(set([x['len'] for x in results['noveth'] ]))
    x = list(set([x['pktsz'] for x in results['noveth']]))

    #  x = results.keys()
    index = np.arange(len(x)) # the x locations for the groups
    #  width = 1               # the width of each bar cluster
    #  y = [results[sz]['rate_gbps'] for sz in x]
    plt.grid(alpha=0.3, linestyle='--')

    i=0
    spacing=0.8
    width=0.9*spacing
    bw=width/len(lens)
    maxy = 0
    for l in lens:
        print("len: " + str(l))
        y = [v['rtt_median'] for v in sorted(results['noveth'],key=lambda x : x['pktsz']) if v['len'] == l]
        yerr = [v['rtt_mdev'] for v in sorted(results['noveth'],key=lambda x : x['pktsz']) if v['len'] == l]
        plt.bar(index - width/2 + i*bw, y, yerr=yerr, width=bw, align='edge', edgecolor='k', label='Len = {}'.format(l) )

        m = max(y)
        maxy = m if m > maxy else maxy
        i += 1

    # Plot horizontal line with baseline
    ybaseline = results['baseline'][0]['rtt_median']
    left, right = plt.xlim()
    plt.hlines(ybaseline, left, right, color='r', linestyle='--', label='Baseline') # ybaseline, width, yerr=ybaselineerr, edgecolor='k' ,hatch='/')

    plt.xticks(index,x, fontsize=defs.ftsz['ticks'])
    plt.yticks(fontsize=defs.ftsz['ticks'])
    plt.legend(ncol=3, fontsize=defs.ftsz['legend'])
    plt.ylabel('Latency (ms)',  fontsize=defs.ftsz['axes'])
    plt.xlabel('Packet size (B)', fontsize=defs.ftsz['axes'])
    plt.tight_layout()
    plt.ylim(0,1.5*maxy)
    plt.savefig(outfile)
    plt.show()

if __name__ == '__main__':
    pktsizes = set()

    if len(sys.argv) != 2:
        print("Usage: {} <csv-file>".format(sys.argv[0]))
        exit(1)

    res = lp.process_data(sys.argv[1])

    plot(res,'pktsz-lat.pdf', [1,10])
