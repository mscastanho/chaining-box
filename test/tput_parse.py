import numpy as np
import csv

max_speed_gbps = 10 # Gbps

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

            pktsz     = int(line['pkt size'])
            tx        = int(line['tx packets'])
            rx        = int(line['rx packets'])
            dropped   = int(line['dropped packets'])
            duration  = int(line['duration(ms)'])
            rate_gbps = max_speed_gbps*(rx/tx)
            rate_mpps = (rx/(duration/1000))/10**6

            results[key].append({
                'len':          int(length),
                'rx':           rx,
                'tx':           tx,
                'pktsz':        pktsz,
                'dropped':      dropped,
                'duration':     duration,
                'rate_gbps':    rate_gbps,
                'rate_mpps':    rate_mpps,
            })

        #  print("Results for >> {} <<:".format(filename))
        #  for k,v in results.items():
            #  print("{}: {}".format(k,v))
#
        #  print()

    return results
