#!/usr/bin/env python3

import math
import sys
import contextlib

import scipy.signal as sig
import numpy as np


@contextlib.contextmanager
def wrap(left='{', right='}', file=None, end=''):
    print(left, file=file, end=end)
    yield
    print(right, file=file, end=end)

@contextlib.contextmanager
def print_include_guards(macro_name):
    print(f'#ifndef {macro_name}')
    print(f'#define {macro_name}')
    print()
    yield
    print()
    print(f'#endif /* {macro_name} */')

macro_float = lambda f: f'{f}'.replace('.', 'F').replace('-', 'N').replace('+', 'P')

ordinal = lambda n: "%d%s" % (n,"tsnrhtdd"[(n//10%10!=1)*(n%10<4)*n%10::4])

SI_TABLE = {-18: 'a', -15: 'f', -12: 'p', -9: 'n', -6: 'µ', -3: 'm', 0: '', 3: 'k', 6: 'M', 9: 'G', 12: 'T', 15: 'P', 18: 'E'}
def siprefix(x, space=' ', unit=''):
    l = math.log10(x)//3*3
    if l in SI_TABLE:
        return f'{x/10**l}{space}{SI_TABLE[l]}{unit}'
    return f'{x}{space}{unit}'

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('-m', '--macro-name', default='butter_filter', help='Prefix for output macro names')
    parser.add_argument('fc', type=float, help='Corner frequency [Hz]')
    parser.add_argument('fs', type=float, help='Sampling rate [Hz]')
    parser.add_argument('n', type=int, nargs='?', default=6, help='Filter order')
    args = parser.parse_args()
    
    sos = sig.butter(args.n, args.fc, fs=args.fs, output='sos')

    print('/* THIS IS A GENERATED FILE. DO NOT EDIT! */')
    print()
    with print_include_guards(f'__BUTTER_FILTER_GENERATED_{args.n}_{macro_float(args.fc)}_{macro_float(args.fs)}__'):

        print(f'/* {ordinal(args.n)} order Butterworth IIR filter coefficients')
        print(f' *')
        print(f' * corner frequency f_c = {siprefix(args.fc)}Hz')
        print(f' * sampling rate f_s = {siprefix(args.fs)}Hz')
        print(f' */')
        print()
        print(f'#define {args.macro_name.upper()}_ORDER {args.n}')
        print(f'#define {args.macro_name.upper()}_CLEN {(args.n+1)//2}')

        # scipy.signal.butter by default returns extremely small bs for the first biquad and large ones for subsequent
        # sections. Balance magnitudes to reduce possible rounding errors.
        first_biquad_bs = sos[0][:3]
        approx_mag = round(math.log10(np.mean(first_biquad_bs)))
        mags = [approx_mag // len(sos)] * len(sos)
        mags[0] += approx_mag - sum(mags)
        sos[0][:3] /= 10**approx_mag
        sos = np.array([ sec * np.array([10**mag, 10**mag, 10**mag, 1, 1, 1]) for mag, sec in zip(mags, sos) ])

        ones = np.ones([100000])
        _, steady_state = sig.sosfilt(sos, ones, zi=np.zeros([(args.n+1)//2, 2]))

        print(f'#define {args.macro_name.upper()}_COEFF ', end='')
        for sec in sos:
            bs, ases = sec[:3], sec[4:6]

            with wrap():
                print('.b=', end='')
                with wrap():
                    print(', '.join(f'{v}' for v in bs), end='')
                print(', .a=', end='')
                with wrap():
                    print(', '.join(f'{v}' for v in ases), end='')
            print(', ', end='')
        print()

        print(f'#define {args.macro_name.upper()}_STEADY_STATE ', end='')
        for sec in steady_state:
            with wrap():
                print(', '.join(f'{v}' for v in sec), end='')
            print(', ', end='')
        print()

