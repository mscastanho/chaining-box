#!/usr/bin/env python3
import sys
import os
sys.path.append(os.path.abspath('../plots'))
import matplotlib.pyplot as plt
import numpy as np
import csv
import fnmatch
from matplotlib.patches import Patch

# Our defaults to plot stuff. Ex: colors, progs...

# ====== Script params ======
max_speed_gbps = 10 # Gbps
#  results_dir = os.path.dirname(os.path.abspath(__file__)) + '/results-old'

# Infer tests from .csv files in this directory
#  tests = [f.split('.')[0] for f in fnmatch.filter(os.listdir(results_dir), '*.csv')]
# ===========================

def process_tput_vs_pktsz(filename):
    lines = None
    results = {}

    #print(filename)
    with open(filename) as csvin:
        reader = csv.DictReader(csvin,delimiter=';')
        for line in reader:
            # print(line['pkt size'], line['trial number'])

            # For now this script only processes 1 trial
            # per packet size. If more are required, changes
            # must be made.

            pktsz     = int(line['pkt size'])
            tx        = int(line['tx packets'])
            rx        = int(line['rx packets'])
            dropped   = int(line['dropped packets'])
            duration  = int(line['duration(ms)'])
            rate_gbps = max_speed_gbps*(rx/tx)
            rate_mpps = (rx/(duration/1000))/10**6

            results[pktsz] = {
                'rx':           rx,
                'tx':           tx,
                'dropped':      dropped,
                'duration':     duration,
                'rate_gbps':    rate_gbps,
                'rate_mpps':    rate_mpps,
            }

        print("Results for >> {} <<:".format(filename))
        for k,v in results.items():
            print("{}: {}".format(k,v))

        print()

    return results

def plot_gbps(results,outfile,ymax):
    plt.close()
    # return
    # bwidth = width/len(experiments)     # width of each bar

    #  fig, ax = plt.subplots(results.keus(),results.values())

    # x = list(experiments.keys())

    #  ax.grid()
    #  i = 0
    #  ncolors = len(df.colors)
    x = results.keys()
    index = np.arange(len(x)) # the x locations for the groups
    width = 0.35               # the width of each bar cluster
    y = [results[sz]['rate_gbps'] for sz in x]
    plt.grid(alpha=0.3, linestyle='--')
    plt.bar(index,y,width=width)
    plt.xticks(index,x)
    plt.title('Throughput vs packet size', fontsize=14)
    plt.ylabel('Throughput (Gbps)',  fontsize=13)
    plt.xlabel('Packet size (B)', fontsize=13)
    plt.tight_layout()
    plt.savefig(outfile)
    return

    #  for exp in experiments:
        #  reso = exp[0]['results'] # old project
        #  resn = exp[1]['results'] # new project
        #  name = exp[0]['name']
#
        #  y = v['rate_gbps'][pktsize] for v in res.values()]
        #  cidx = i%ncolors
        #  rects = ax.bar(index[i] - width/2, reso[pktsize]['rate_gbps'], width/2,
                        #  color=df.palette[i],label=name,align='edge',
                        #  edgecolor='black', linewidth=0.6)
#
        #  rects = ax.bar(index[i], resn[pktsize]['rate_gbps'], width/2,
                        #  color=df.palette[i],label=name,align='edge',
                        #  edgecolor='black', linewidth=0.6, hatch='//')
        #  i += 1
#
    #  plt.ylim(0,ymax)
    #  plt.margins(0.02)
    #  plt.xticks(rotation=45)
#
    #  ax.set_axisbelow(True)
    #  ax.xaxis.grid(False)
    #  xlabels = [df.params[exp[0]['name']][0] for exp in experiments]
    #  xlabels = ['']*len(experiments)
    #  ax.set_xticks(index)
    #  ax.set_xticklabels(xlabels)
    #  ax.set_ylabel('Throughput (Gbps)', fontsize=18)
    #  ax.set_xlabel('Packet size (B)', fontsize=13)
    #  ax.tick_params(axis='both', which='major', labelsize=16)
    #  ax.legend(fontsize=11,ncol=4)
    #  custom_legend = [Patch(facecolor='white',edgecolor='black',
                        #  label='Standard'),
                     #  Patch(facecolor='white',edgecolor='black',
                        #  hatch='//',label='Optimized')]
#
    #  plt.legend(handles=custom_legend,fontsize=14,ncol=2)
    #  plt.show()

if __name__ == '__main__':
    pktsizes = set()

    if len(sys.argv) != 2:
        print("Usage: {} <csv-file>".format(sys.argv[0]))
        exit(1)

    res = process_raw_data(sys.argv[1])
    #  pktsizes.update(list(res.keys()))
    #  pktsizes = list(pktsizes)
    #  pktsizes.sort()

    plot_gbps(res,'tput-gbps.pdf',10.5)
