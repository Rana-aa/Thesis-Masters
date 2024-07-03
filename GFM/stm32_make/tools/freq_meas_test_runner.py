#!/usr/bin/env python3

import os
from os import path
import subprocess
import json

import numpy as np
np.set_printoptions(linewidth=240)


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument(metavar='test_data_directory', dest='dir', help='Directory with test data .bin files')
    default_binary = path.abspath(path.join(path.dirname(__file__), '../build/tools/freq_meas_test'))
    parser.add_argument(metavar='test_binary', dest='binary', nargs='?', default=default_binary)
    parser.add_argument('-d', '--dump', help='Write raw measurements to JSON file')
    args = parser.parse_args()

    bin_files = [ path.join(args.dir, d) for d in os.listdir(args.dir) if d.lower().endswith('.bin') ]

    savedata = {}
    for p in bin_files:
        output = subprocess.check_output([args.binary, p], stderr=subprocess.DEVNULL)
        measurements = np.array([ float(value) for _offset, value in [ line.split() for line in output.splitlines() ] ])
        savedata[p] = list(measurements)

        # Cut off first and last sample for mean and RMS calculations as these show boundary effects.
        measurements = measurements[1:-1]
        mean = np.mean(measurements)
        rms = np.sqrt(np.mean(np.square(measurements - mean)))

        print(f'{path.basename(p):<60}:    mean={mean:<8.4f}Hz rms={rms*1000:.3f}mHz')

    if args.dump:
        with open(args.dump, 'w') as f:
            json.dump(savedata, f)

