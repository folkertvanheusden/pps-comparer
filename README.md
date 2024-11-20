To be able to compile this, you need the pps-tools and cmake packages. libgps-dev is optional.
To run:
    sudo ./pps-comparer -1 /dev/pps0 -2 /dev/pps1 -l log.dat

Press ctrl+c to stop measuring

When you've got libgps-dev installed during build, -g <host> adds gpsd stats to the output.

If you have saved the output to e.g. test.dat (-l test.dat), you can plot the Allan deviation plot with:

    ./plot-allan.py test.dat test.svg

test.svg is then the output-graph.

For a histogram or as a time-series, use plot-hist.py and plot-ts.py.

Note: when pulses are missing(! e.g. when GPS loses fix), the comparison goes haywire.


Released under the MIT license by Folkert van Heusden.
