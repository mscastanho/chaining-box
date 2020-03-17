import numpy as np
import csv

def process_data(filename):
    lines = None
    results = { 'baseline': [], 'veth': [], 'noveth': [] }

    #print(filename)
    with open(filename) as csvin:
        reader = csv.DictReader(csvin,delimiter=';')
        for line in reader:

            name = line['name']
            key = ''
            length = 0

            if name == 'baseline':
                key = 'baseline'
            else:
                key = 'veth' if '-veth' in name else 'noveth'
                length = name.split('-')[1]

            rtts=list(map(float,line['rtts'].split(' ')))
            rtts.sort()

            # Remove some outliers
            rtts = rtts[10:-10]
            print(rtts)

            results[key].append({
                'len':          int(length),
                'pktsz':        int(line['pktsz']),
                'rx':           int(line['rx']),
                'tx':           int(line['tx']),
                'rtt_min':      np.min(rtts),
                'rtt_avg':      np.average(rtts),
                'rtt_max':      np.max(rtts),
                'rtt_mdev':     np.std(rtts),
                'rtt_median':   np.median(rtts),
            })

        #  print("Results for >> {} <<:".format(filename))
        #  for k,v in results.items():
            #  print("{}: {}".format(k,v))
        #  print()

    return results
