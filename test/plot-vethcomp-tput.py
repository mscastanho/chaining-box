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
    
    # We will use results without veth pairs and pkt size = 1462
    veth = list(filter(lambda x : x['pktsz'] == 1462, results['veth']))
    noveth = list(filter(lambda x : x['pktsz'] == 1462, results['noveth']))
  
    x = [v['len'] for v in veth]
    yveth = [v['rate_gbps'] for v in veth]
    ynoveth = [v['rate_gbps'] for v in noveth]
    
    index = np.arange(len(x)) # the x locations for the groups
    width = 0.35               # the width of each bar cluster
    #  y = [results[sz]['rate_gbps'] for sz in x]
    plt.grid(alpha=0.3, linestyle='--')
    plt.bar(index - 0.5*width,yveth,width,edgecolor='k', label='w/ direct links')
    plt.bar(index + 0.5*width,ynoveth,width,edgecolor='k', label='w/o direct links')
    plt.xticks(index,x, fontsize=13)
    plt.yticks(fontsize=13)
    plt.legend(fontsize=14)
    plt.ylabel('Throughput (Gbps)',  fontsize=14)
    plt.xlabel('Chain length (# SFs)', fontsize=14)
    plt.tight_layout()
    plt.savefig(outfile)
    plt.show()

if __name__ == '__main__':
    pktsizes = set()

    if len(sys.argv) != 2:
        print("Usage: {} <csv-file>".format(sys.argv[0]))
        exit(1)

    res = tp.process_data(sys.argv[1])

    plot_gbps(res,'vethcomp-tput.pdf',10.5)
