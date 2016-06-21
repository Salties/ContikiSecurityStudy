#!/usr/bin/python3

import numpy as np;
import matplotlib.pyplot as plt;
import matplotlib.animation as animation;
import sys;

HelpMsg="Usage: Plot data from stdin."

windowsize = 200; #Window size
rndv = [0.5] * windowsize; #Initialise buffer to 0.5.
plotrange = 0.1;


def update(data):
    line.set_ydata(data);
    return line,

def data_gen():
    while True:
        #Read new value from stdin.
        newval = input();
        #Pop out the oldest value.
        rndv.pop(0);
        #Append the latest value to the end.
        rndv.append(newval);
        print(rndv); #For debug use.
        yield rndv;

fig, ax = plt.subplots();
ax.set_ylim(0.5 - plotrange, 0.5 + plotrange); #Y axis
line, = ax.plot(np.random.rand(windowsize));

def main(argc, argv):
    global ax;

    if "-h" in argv:
        print(HelpMsg);
        exit(0);

    ani = animation.FuncAnimation(fig, update, data_gen, interval=10);
    plt.grid(True);
    plt.show();
    exit(0);

if __name__ == "__main__":
     main(len(sys.argv),sys.argv);
