#! /usr/bin/python3

# pip3 install allantools

import allantools
import math
import numpy as np
import sys


fh = open(sys.argv[1], 'r')
data = [(float(p.split()[0]), float(p.split()[1]), float(p.split()[2])) for p in fh.readlines()[0:-1]]
fh.close()

def get_values(data, column):
    values = []
    prev_value = None
    for row in data:
        v = row[column]
        if len(values) > 0:
            n_to_add = int(v) - int(prev_value) - 1
            values += [math.nan] * n_to_add
        values.append(v)
        prev_value = v
    return values

v1 = get_values(data, 0)
v2 = get_values(data, 1)

a1 = allantools.Dataset(data=np.asarray(v1), data_type='phase', rate=1)
a1.compute('gradev')

a2 = allantools.Dataset(data=np.asarray(v2), data_type='phase', rate=1)
a2.compute('gradev')

b = allantools.Plot(no_display=True)
b.ax.set_xlabel("Tau (s)")
b.plt.title('pps sources compared')
b.plot(a1, errorbars=True, grid=True, label='source 1')
b.plot(a2, errorbars=True, grid=True, label='source 2')
b.plt.legend()
b.plt.savefig(sys.argv[2], format='svg')
