#!/usr/bin/env python3
import sys
import os
sys.path.append(os.path.abspath('../plots'))
import matplotlib.pyplot as plt
import numpy as np
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

def plot(results,outfile):
    plt.close()
    
    # We will use results without veth pairs and pkt size = 1462
    cases = list(filter(lambda x : x['pktsz'] == 1462, results['noveth']))
  
    x = [v['len'] for v in cases]
    y = [v['rtt_median'] for v in cases]
    yerr = [v['rtt_mdev'] for v in cases]
    
    #  x = results.keys()
    index = np.arange(len(x)) # the x locations for the groups
    width = 0.35               # the width of each bar cluster
    plt.grid(alpha=0.3, linestyle='--')
    plt.bar(index,y,yerr=yerr,width=width,edgecolor='k', label='RTT')
    
    # Plot horizontal line with baseline
    ybaseline = results['baseline'][0]['rtt_median']
    left, right = plt.xlim()
    plt.hlines(ybaseline, left, right, color='r', linestyle='--', label='Baseline') # ybaseline, width, yerr=ybaselineerr, edgecolor='k' ,hatch='/')
    
    # Plot linear regressions
    xrl, yrl = reglin(x,y)
    plt.plot(index, yrl, marker='o', ms=7, linestyle='--', label='Linear regression')

    plt.xticks(index,x, fontsize=defs.ftsz['ticks'])
    plt.yticks(fontsize=defs.ftsz['ticks'])
    plt.legend(framealpha=1, fontsize=defs.ftsz['legend'])
    plt.ylabel('Latency (ms)',  fontsize=defs.ftsz['axes'])
    plt.xlabel('Chain length (# SFs)', fontsize=defs.ftsz['axes'])
    plt.tight_layout()
    plt.ylim(0,1.5*max(y))
    plt.savefig(outfile)
    plt.show()

if __name__ == '__main__':
    pktsizes = set()

    if len(sys.argv) != 2:
        print("Usage: {} <csv-file>".format(sys.argv[0]))
        exit(1)

    res = lp.process_data(sys.argv[1])

    plot(res,'chainlen-lat.pdf')
