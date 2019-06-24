#!/usr/bin/env python3
import matplotlib.pyplot as plt
import numpy as np
import csv
import os
import fnmatch
import json
import sys
import re

# ====== Script params ======
script_dir = os.path.dirname(os.path.abspath(__file__))
# ===========================

def process_rate_dir(dirname):
    dir = os.fsencode(dirname)
    #print("Entering dir {}".format(dirname))

    for file in os.listdir(dir):
        filename = "{}/{}".format(dirname,os.fsdecode(file))

        with open(filename) as f:
            jdata = json.load(f)
            print(jdata['intervals'],end="\n\n")
            
            
def process_raw_data(filename):
    results = []

    with open(filename) as csvin:
        reader = csv.DictReader(csvin,delimiter=';')
        for line in reader:
            a = np.array([float(line[k]) for k in filter(lambda s : re.search("R[0-9]+",s), line.keys())])
            #print(a)
            #print("rate: {} avg: {} std: {}".format(line['rate'],np.average(a),np.std(a)))
            #print(rounds)
            #for k in filter
             #   print("key: {} value: {}".format(k,line[k]))
                
            res = (line['Rate'], np.mean(a), np.std(a))
            results.append(res)
            print(res)

    return results

def plot(results):

    x = [r[0] for r in results]
    y = [r[1] for r in results]
    err = [r[2] for r in results]

    plt.plot(x,y, color='red')
    plt.ylabel('Average packet loss(%)', fontsize=15)
    plt.xlabel('Throughouput (Mbps)', fontsize=15)
    plt.title('FUTEBOL - 1 SF', fontsize=15)
    #plt.xticks(np.arange(0,1,step=0.1),(0,100,200,300,400,500,600,700,800,900,1000))
    #plt.ylim(0,2)
    #plt.margins(0)

    #print(np.arange(0,1,step=0.2)*1000)
    #plt.xticks(np.arange(0,1,step=0.2)*1000)
    #ax.set_ylabel('Average packet loss (%)', fontsize=16)
    #ax.set_xlabel('Throughput (Mbps)', fontsize=16)
    #ax.tick_params(axis='both', which='major', labelsize=15)

    plt.grid()
    plt.tight_layout()
    #plt.savefig('../plots/downtime.pdf')
    plt.show()


def main():
    if len(sys.argv) != 2:
        print("Usage: {} <tput-results-dir>".format(sys.argv[0]))
        exit(1)

    root = sys.argv[1]

    # Try to open results directory
    #try:
    #    #print(sys.argv[1])
    #    tdir = os.fsencode(root)
    #    for subdir in os.listdir(tdir):
    #        rdir="{}/{}".format(root,os.fsdecode(subdir))
    #        
    #        if os.path.isdir(rdir):
    #            process_rate_dir(rdir)
    #        else:
    #            print("{} is not a directory".format(rdir))
    #
    #except Exception as e:
    #    print(e)
    #    exit(1)

    try:
        results = process_raw_data("{}/{}".format(root,"lossrate-summary.csv"))
        plot(results)
    except Exception as e:
        print(e)
        exit(1)

if __name__ == '__main__':
    main()
