To be able to compile this, you need the pps-tools and cmake packages.

To run:
sudo ./pps-comparer /dev/pps0 /dev/pps1

Press ctrl+c to stop measuring and see the results.

If you have saved the output to e.g. test.dat, you can plot the Allan deviation plot with:

    ./plot-allan.py test.dat test.svg

test.svg is then the output-graph.

For a histogram or as a time-series, use plot-hist.py and plot-ts.py.


Released under the MIT license by Folkert van Heusden.
