#! /usr/bin/python3

import datetime
import matplotlib.pyplot as plt
import sys


fh = open(sys.argv[1], 'r')
data = [p.split() for p in fh.readlines()[0:-1]]
fh.close()

mul = 5
plt.figure(figsize=(6.4 * mul, 4.8 * mul), dpi=100 * mul)
plt.title('pps time difference')
plt.xlabel('time')

x = [datetime.datetime.fromtimestamp(float(row[0])) for row in data]
y = [float(row[2]) for row in data]
plt.plot(x, y, label='difference in seconds')
plt.legend()

plt.savefig(sys.argv[2], format='svg')
