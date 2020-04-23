#!/usr/bin/env python3
import sys
import os
sys.path.append(os.path.abspath('../plots'))
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Patch

import tput_parse as tp
import plot_defaults as defs

def plot(results,outfile):
    plt.close()

    # Get set of lenghts
    x = list(set([x['pktsz'] for x in results['baseline']]))

    #  x = results.keys()
    index = np.arange(len(x)) # the x locations for the groups
    #  y = [results[sz]['rate_gbps'] for sz in x]
    plt.grid(alpha=0.3, linestyle='--')

    width=0.35
    y = [v['rate_gbps'] for v in sorted(results['baseline'],key=lambda x : x['pktsz'])]

    print(x)
    print(y)
    plt.bar(index, y, width, edgecolor='k', label='Baseline')

    plt.xticks(index,x, fontsize=defs.ftsz['ticks'])
    plt.yticks(fontsize=defs.ftsz['ticks'])
    plt.legend(ncol=5,fontsize=defs.ftsz['legend'])
    #  plt.ylim(0,1.5)
    plt.ylabel('Throughput (Gbps)',  fontsize=defs.ftsz['axes'])
    plt.xlabel('Packet size (B)', fontsize=defs.ftsz['axes'])
    plt.tight_layout()
    plt.savefig(outfile)
    plt.show()

if __name__ == '__main__':
    pktsizes = set()

    if len(sys.argv) != 2:
        print("Usage: {} <csv-file>".format(sys.argv[0]))
        exit(1)

    res = tp.process_data(sys.argv[1])
    plot(res,'baseline-tput.pdf')
