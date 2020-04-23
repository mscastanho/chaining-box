#!/usr/bin/env python3
import sys
import os
sys.path.append(os.path.abspath('../plots'))
import matplotlib.pyplot as plt
import numpy as np
import fnmatch
import operator
from matplotlib.patches import Patch

import lat_parse as lp
import plot_defaults as defs

def reglin(x,y):
    xa = np.array(x)
    ya = np.array(y)
    A = np.vstack([xa, np.ones(len(xa))]).T
    m,c = np.linalg.lstsq(A, y, rcond=None)[0]
    print("Derivative: {} // Linear coef.: {}".format(m,c))

    return xa, m*xa + c

def plot_sidebyside(results, outfile):
    plt.close()

    fveth = list(filter(lambda x : x['pktsz'] == 1462, results['veth']))
    fnoveth = list(filter(lambda x : x['pktsz'] == 1462, results['noveth']))
  
    # Latency with veth pairs
    xveth = [v['len'] for v in fveth]
    yveth = [v['rtt_median'] for v in fveth]
    yvetherr = [v['rtt_mdev'] for v in fveth]

    # Latency without veth pairs
    ynoveth = [v['rtt_median'] for v in fnoveth]
    ynovetherr = [v['rtt_mdev'] for v in fnoveth]

    # Baseline latency
    ybaseline = results['baseline'][0]['rtt_median']
    ybaselineerr = results['baseline'][0]['rtt_mdev']

    # The x locations for the bars
    ind = np.arange(len(xveth))
    width = 0.25
    print(ind)

    plt.grid(alpha=0.3, linestyle='--')

    plt.bar(ind + 0.5*width, ynoveth, width, yerr=ynovetherr, edgecolor='k', label='w/o direct links')
    plt.bar(ind - 0.5*width, yveth, width, yerr=yvetherr, edgecolor='k', label='w/ direct links')

    # Plot horizontal line with baseline
    left, right = plt.xlim()
    plt.hlines(ybaseline, left, right, color='r', linestyle='--', label='Baseline') # ybaseline, width, yerr=ybaselineerr, edgecolor='k' ,hatch='/')

    plt.xticks(ind, xveth, fontsize=defs.ftsz['ticks'])
    plt.yticks(fontsize=defs.ftsz['ticks'])
    #  plt.title('Latency: Direct Links vs No Direct Links', fontsize=14)
    plt.legend(framealpha=1, fontsize=defs.ftsz['legend'], loc='upper left')

    # Plot linear regressions
    xnovrl, ynovrl = reglin(xveth,ynoveth)
    xvrl, yvrl = reglin(xveth,yveth)
    plt.plot(ind + 0.5*width, ynovrl, 'blue', marker='x', ms=7, linestyle='--')
    plt.plot(ind - 0.5*width, yvrl, 'orange', marker='o', ms=7, linestyle='--')

    plt.ylabel('Latency (ms)',  fontsize=defs.ftsz['axes'])
    plt.xlabel('Chain Length (# SFs)', fontsize=defs.ftsz['axes'])
    plt.tight_layout()
    plt.ylim(0.3,1.2*max(yveth + ynoveth))
    plt.savefig(outfile)
    plt.show()

    return

if __name__ == '__main__':
    pktsizes = set()

    if len(sys.argv) != 2:
        print("Usage: {} <csv-file>".format(sys.argv[0]))
        exit(1)

    res = lp.process_data(sys.argv[1])
    plot_sidebyside(res, 'vethcomp-lat.pdf')
