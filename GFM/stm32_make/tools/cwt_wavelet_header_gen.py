#!/usr/bin/env python3

import textwrap

import scipy.signal as sig
import numpy as np

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('n', type=int, help='Window size')
    parser.add_argument('w', type=float, help='Wavelet width')
    parser.add_argument('-v', '--variable', default='cwt_ricker_table', help='Name for alias variable pointing to generated wavelet LUT')
    args = parser.parse_args()
    
    print(f'/* CWT Ricker wavelet LUT for {args.n} sample window of width {args.w}. */')
    varname = f'cwt_ricker_{args.n}_window_{str(args.w).replace(".", "F")}'
    print(f'const float {varname}[{args.n}] = {{')

    win = sig.ricker(args.n, args.w)
    par = ' '.join(f'{f:>015.12e}f,' for f in win)
    print(textwrap.fill(par,
        initial_indent=' '*4, subsequent_indent=' '*4,
        width=120,
        replace_whitespace=False, drop_whitespace=False))
    print('};')
    print()
    print(f'const float * const {args.variable} __attribute__((weak)) = {varname};')

