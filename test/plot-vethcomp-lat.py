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


def reglin(x,y):
    xa = np.array(x)
    ya = np.array(y)
    A = np.vstack([xa, np.ones(len(xa))]).T
    m,c = np.linalg.lstsq(A, y, rcond=None)[0]

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

    plt.bar(ind - 0.5*width, yveth, width, yerr=yvetherr, edgecolor='k', hatch='o')
    plt.bar(ind + 0.5*width, ynoveth, width, yerr=ynovetherr, edgecolor='k', hatch='x')

    # Plot horizontal line with baseline
    left, right = plt.xlim()
    plt.hlines(ybaseline, left, right, color='r', linestyle='--') # ybaseline, width, yerr=ybaselineerr, edgecolor='k' ,hatch='/')

    plt.xticks(ind, xveth, fontsize=14)
    plt.yticks(fontsize=14)
    #  plt.title('Latency: Direct Links vs No Direct Links', fontsize=14)
    plt.legend(['Baseline', 'w/ direct links', 'w/o direct links', ''], fontsize=14)

    # Plot linear regressions
    xnovrl, ynovrl = reglin(xveth,ynoveth)
    xvrl, yvrl = reglin(xveth,yveth)
    plt.plot(ind - 0.5*width, yvrl, 'blue', marker='o', ms=7, linestyle='--')
    plt.plot(ind + 0.5*width, ynovrl, 'orange', marker='x', ms=7, linestyle='--')

    plt.ylabel('Latency (ms)',  fontsize=18)
    plt.xlabel('Chain Length (# SFs)', fontsize=18)
    plt.tight_layout()
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
