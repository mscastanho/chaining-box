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

    # Get set of lenghts
    # REALLY HACKY, CAREFUL NOT TO GET INFECTED!!!
    lens = list(set([x['len'] for x in results['noveth'] ]))
    x = list(set([x['pktsz'] for x in results['noveth']]))

    #  x = results.keys()
    index = np.arange(len(x)) # the x locations for the groups
    #  y = [results[sz]['rate_gbps'] for sz in x]
    plt.grid(alpha=0.3, linestyle='--')

    i=0
    spacing = 1
    width=0.9*spacing
    bw=width/(len(lens)+1)
    y = [v['rate_gbps'] for v in sorted(results['baseline'],key=lambda x : x['pktsz'])]
    plt.bar(index - width/2 + i*bw, y, width=bw, edgecolor='k', label='Baseline' )
    i += 1

    for l in lens:
        print("len: " + str(l))
        y = [v['rate_gbps'] for v in sorted(results['noveth'],key=lambda x : x['pktsz']) if v['len'] == l]
        plt.bar(index - width/2 + i*bw, y, width=bw, edgecolor='k', label='Len = {}'.format(l) )
        i += 1

    plt.xticks(index,x, fontsize=defs.ftsz['ticks'])
    plt.yticks(fontsize=defs.ftsz['ticks'])
    plt.legend(fontsize=13)
    plt.ylabel('Throughput (Gbps)',  fontsize=defs.ftsz['axes'])
    plt.xlabel('Packet size (B)', fontsize=defs.ftsz['axes'])
    plt.tight_layout()
    plt.savefig(outfile)
    plt.show()

def plot_mpps(results,outfile,ymax):
    #  plt.close()

    # Get set of lenghts
    # REALLY HACKY, CAREFUL NOT TO GET INFECTED!!!
    lens = list(set([x['len'] for x in results['noveth'] ]))
    x = list(set([x['pktsz'] for x in results['noveth']]))

    #  x = results.keys()
    index = np.arange(len(x)) # the x locations for the groups
    #  y = [results[sz]['rate_gbps'] for sz in x]
    plt.grid(alpha=0.3, linestyle='--')

    i=0
    spacing = 1
    width=0.9*spacing
    bw=width/(len(lens)+1)
    y = [v['rate_mpps'] for v in sorted(results['baseline'],key=lambda x : x['pktsz'])]
    plt.bar(index - width/2 + i*bw, y, width=bw, edgecolor='k', label='Baseline' )
    i += 1

    for l in lens:
        print("len: " + str(l))
        y = [v['rate_mpps'] for v in sorted(results['noveth'],key=lambda x : x['pktsz']) if v['len'] == l]
        plt.bar(index - width/2 + i*bw, y, width=bw, edgecolor='k', label='Len = {}'.format(l) )
        i += 1

    plt.xticks(index,x, fontsize=defs.ftsz['ticks'])
    plt.yticks(fontsize=defs.ftsz['ticks'])
    plt.legend(ncol=5,fontsize=10)
    plt.ylim(0,1.5)
    plt.ylabel('Throughput (Mpps)',  fontsize=defs.ftsz['axes'])
    plt.xlabel('Packet size (B)', fontsize=defs.ftsz['axes'])
    plt.tight_layout()
    plt.savefig(outfile)
    plt.show()

def plot_both(results,outfile,ymax):
    #  plt.close()

    # Get set of lenghts
    # REALLY HACKY, CAREFUL NOT TO GET INFECTED!!!
    lens = list(set([x['len'] for x in results['noveth'] ]))
    x = list(set([x['pktsz'] for x in results['noveth']]))

    #  x = results.keys()
    index = np.arange(len(x)) # the x locations for the groups
    #  y = [results[sz]['rate_gbps'] for sz in x]

    fig, (ax1,ax2) = plt.subplots(1,2)

    i=0
    spacing = 1
    width=0.9*spacing
    bw=width/(len(lens)+1)
    ygbps = [v['rate_gbps'] for v in sorted(results['baseline'],key=lambda x : x['pktsz'])]
    ympps = [v['rate_mpps'] for v in sorted(results['baseline'],key=lambda x : x['pktsz'])]
    ax1.bar(index - width/2 + i*bw, ygbps, width=bw, edgecolor='k', label='Baseline' )
    ax2.bar(index - width/2 + i*bw, ympps, width=bw, edgecolor='k', label='Baseline' )
    i += 1

    for l in lens:
        print("len: " + str(l))
        ygbps = [v['rate_gbps'] for v in sorted(results['noveth'],key=lambda x : x['pktsz']) if v['len'] == l]
        ympps = [v['rate_mpps'] for v in sorted(results['noveth'],key=lambda x : x['pktsz']) if v['len'] == l]
        ax1.bar(index - width/2 + i*bw, ygbps, width=bw, edgecolor='k', label='Len = {}'.format(l) )
        ax2.bar(index - width/2 + i*bw, ympps, width=bw, edgecolor='k', label='Len = {}'.format(l) )
        i += 1

    for ax in (ax1,ax2):
        ax.set_xticks(index)
        ax.set_xticklabels(x)
        ax.tick_params(labelsize=defs.ftsz['ticks'])
        #  ax.set_aspect(aspect=1)
        ax.grid(alpha=0.3, linestyle='--')
        #  ax.set_yticks(fontsize=defs.ftsz['ticks'])
        #  plt.ylim(0,1.5)
        ax.set_xlabel('Packet size (B)', fontsize=defs.ftsz['axes'])

    ax1.legend(fontsize=12)
    ax2.yaxis.tick_right()
    ax2.yaxis.set_label_position("right")
    plt.tight_layout()
    #  plt.savefig(outfile)
    ax1.set_ylabel('Throughput (Gbps)',  fontsize=defs.ftsz['axes'])
    ax2.set_ylabel('Throughput (Mpps)',  fontsize=defs.ftsz['axes'])
    plt.show()


if __name__ == '__main__':
    pktsizes = set()

    if len(sys.argv) != 2:
        print("Usage: {} <csv-file>".format(sys.argv[0]))
        exit(1)

    res = tp.process_data(sys.argv[1])

    #  plot_gbps(res,'pktsz-tput-gbps.pdf',10.5)
    #  plot_mpps(res,'pktsz-tput-mpps.pdf',10.5)
    plot_both(res,'pktsz-tput-both.pdf', 10.5)
