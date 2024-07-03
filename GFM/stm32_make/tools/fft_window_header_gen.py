#!/usr/bin/env python3

import textwrap

import scipy.signal as sig
import numpy as np

WINDOW_TYPES = [
        'boxcar',
        'triang',
        'blackman',
        'hamming',
        'hann',
        'bartlett',
        'flattop',
        'parzen',
        'bohman',
        'blackmanharris',
        'nuttall',
        'barthann',
        'kaiser',
        'gaussian',
        'general_gaussian',
        'slepian',
        'dpss',
        'chebwin',
        'exponential',
        'tukey',
        ]

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('window', choices=WINDOW_TYPES, help='Type of window function to use')
    parser.add_argument('n', type=int, help='Width of window in samples')
    parser.add_argument('window_args', nargs='*', type=float,
            help='''Window argument(s) if required. See https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.get_window.html#scipy.signal.get_window for details.''')
    parser.add_argument('-v', '--variable', default='fft_window_table', help='Name for alias variable pointing to generated window')
    args = parser.parse_args()
    
    print(f'/* FTT window table for {args.n} sample {args.window} window.')
    if args.window_args:
        print(f' * Window arguments were: ({" ,".join(str(arg) for arg in args.window_args)})')
    print(f' */')
    winargs = ''.join(f'_{arg:.4g}'.replace('.', 'F') for arg in args.window_args)
    varname = f'fft_{args.n}_window_{args.window}{winargs}'
    print(f'const float {varname}[{args.n}] = {{')

    win = sig.get_window(args.window if not args.window_args else (args.window, *args.window_args),
            Nx=args.n, fftbins=True)
    par = ' '.join(f'{f:>013.8g},' for f in win)
    print(textwrap.fill(par,
        initial_indent=' '*4, subsequent_indent=' '*4,
        width=120,
        replace_whitespace=False, drop_whitespace=False))
    print('};')
    print()
    print(f'const float * const {args.variable} __attribute__((weak)) = {varname};')

