#!/usr/bin/env python3
import sys
import os
sys.path.append(os.path.abspath('../plots'))
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Patch

import tput_parse as tp
import plot_defaults as defs

def plot_gbps(results,outfile,ymax):
    plt.close()
    
    # We will use results without veth pairs and pkt size = 1462
    cases = list(filter(lambda x : x['pktsz'] == 1462, results['noveth']))
  
    print(cases)
    x = [v['len'] for v in cases]
    y = [v['rate_gbps'] for v in cases]
    print(x)
    print(y)
    #  x = results.keys()
    index = np.arange(len(x)) # the x locations for the groups
    width = 0.35               # the width of each bar cluster
    #  y = [results[sz]['rate_gbps'] for sz in x]
    plt.grid(alpha=0.3, linestyle='--')
    plt.bar(index,y,width=width,edgecolor='k')
    plt.xticks(index,x, fontsize=defs.ftsz['ticks'])
    plt.yticks(fontsize=defs.ftsz['ticks'])
    #  plt.title('Throughput vs packet size', fontsize=14)
    plt.ylabel('Throughput (Gbps)',  fontsize=defs.ftsz['axes'])
    plt.xlabel('Chain length (# SFs)', fontsize=defs.ftsz['axes'])
    plt.tight_layout()
    plt.savefig(outfile)
    plt.show()

if __name__ == '__main__':
    pktsizes = set()

    if len(sys.argv) != 2:
        print("Usage: {} <csv-file>".format(sys.argv[0]))
        exit(1)

    res = tp.process_data(sys.argv[1])

    plot_gbps(res,'chainlen-tput.pdf',10.5)
