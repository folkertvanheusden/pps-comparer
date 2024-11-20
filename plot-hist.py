#! /usr/bin/python3

import datetime
import matplotlib.pyplot as plt
import numpy as np
import sys


fh = open(sys.argv[1], 'r')
data = [p.split() for p in fh.readlines()[1:-1]]
fh.close()

mul = 5
plt.figure(figsize=(6.4 * mul, 4.8 * mul), dpi=100 * mul)
plt.title('pps time difference')

plt.ylabel('count')

values = np.array([float(row[3]) for row in data])

q25, q75 = np.percentile(values, [25, 75])
bin_width = 2 * (q75 - q25) * len(values) ** (-1/3)
bins = round((values.max() - values.min()) / bin_width)
print("Freedman–Diaconis number of bins:", bins)
n, bins, patches = plt.hist(values, bins=bins, label='difference (s)')

plt.legend()

plt.savefig(sys.argv[2], format='svg')
