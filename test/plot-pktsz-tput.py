#!/usr/bin/env python3
import sys
import os
sys.path.append(os.path.abspath('../plots'))
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Patch

import tput_parse as tp

def plot_gbps(results,outfile,ymax):
    plt.close()
   
    # Get set of lenghts
    # REALLY HACKY, CAREFUL NOT TO GET INFECTED!!!
    lens = list(set([x['len'] for x in results['noveth'] ]))
    x = list(set([x['pktsz'] for x in results['noveth']]))
     
    #  x = results.keys()
    index = np.arange(len(x)) # the x locations for the groups
    width = 1               # the width of each bar cluster
    #  y = [results[sz]['rate_gbps'] for sz in x]
    plt.grid(alpha=0.3, linestyle='--')

    i=0
    bw=width/(len(lens) + 1)
    for l in lens:
        print("len: " + str(l))
        y = [v['rate_gbps'] for v in sorted(results['noveth'],key=lambda x : x['pktsz']) if v['len'] == l]
        plt.bar(index - width/2 + i*bw, y, width=bw, edgecolor='k', label='Len = {}'.format(l) )
        i += 1
    
    plt.xticks(index,x)
    plt.legend()
    plt.ylabel('Throughput (Gbps)',  fontsize=13)
    plt.xlabel('Packet size (B)', fontsize=13)
    plt.tight_layout()
    plt.savefig(outfile)
    plt.show()

if __name__ == '__main__':
    pktsizes = set()

    if len(sys.argv) != 2:
        print("Usage: {} <csv-file>".format(sys.argv[0]))
        exit(1)

    res = tp.process_data(sys.argv[1])

    plot_gbps(res,'pktsz-tput.pdf',10.5)
