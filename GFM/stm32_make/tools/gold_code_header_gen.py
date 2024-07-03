#!/usr/bin/env python3

import sys
import math
import textwrap
import contextlib

import numpy as np
import scipy.signal as sig

# From https://github.com/mubeta06/python/blob/master/signal_processing/sp/gold.py
preferred_pairs = {5:[[2],[1,2,3]], 6:[[5],[1,4,5]], 7:[[4],[4,5,6]],
                        8:[[1,2,3,6,7],[1,2,7]], 9:[[5],[3,5,6]], 
                        10:[[2,5,9],[3,4,6,8,9]], 11:[[9],[3,6,9]]}

def gen_gold(seq1, seq2):
    gold = [seq1, seq2]
    for shift in range(len(seq1)):
        gold.append(seq1 ^ np.roll(seq2, -shift))
    return gold

def gold(n):
    n = int(n)
    if not n in preferred_pairs:
        raise KeyError('preferred pairs for %s bits unknown' % str(n))
    t0, t1 = preferred_pairs[n]
    (seq0, _st0), (seq1, _st1) = sig.max_len_seq(n, taps=t0), sig.max_len_seq(n, taps=t1)
    return gen_gold(seq0, seq1)

@contextlib.contextmanager
def print_include_guards(macro_name):
    print(f'#ifndef {macro_name}')
    print(f'#define {macro_name}')
    yield
    print(f'#endif /* {macro_name} */')

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('n', type=int, choices=preferred_pairs, help='bit width of shift register. Generate 2**n + 1 sequences of length 2**n - 1.')
    parser.add_argument('-v', '--variable', default='gold_code_table', help='Name for weak alias of generated table')
    parser.add_argument('-h', '--header', action='store_true', help='Generate header file')
    parser.add_argument('-c', '--source', action='store_true', help='Generate table source file')
    args = parser.parse_args()

    if not args.header != args.source:
        print('Exactly one of --header and --source must be given.', file=sys.stderr)
        sys.exit(1)

    nbytes = math.ceil((2**args.n-1)/8)

    if args.source:
        print('/* THIS IS A GENERATED FILE. DO NOT EDIT! */')
        print('#include <unistd.h>')
        print('#include <stdint.h>')
        print()
        print(f'/* {args.n} bit gold sequences: {2**args.n+1} sequences of length {2**args.n-1} bit.')
        print(f' *')
        print(f' * Each code is packed left-aligned into {nbytes} bytes in big-endian byte order.')
        print(f' */')
        print(f'const uint8_t {args.variable}[{2**args.n+1}][{nbytes}] = {{')
        for i, code in enumerate(gold(args.n)):
            par = '{' + ' '.join(f'0x{d:02x},' for d in np.packbits(code)) + f'}}, /* {i: 3d} "{"".join(str(x) for x in code)}" */'
            print(textwrap.fill(par, initial_indent=' '*4, subsequent_indent=' '*4, width=120))
        print('};')
        print()
    else:
        print('/* THIS IS A GENERATED FILE. DO NOT EDIT! */')
        with print_include_guards(f'__GOLD_CODE_GENERATED_HEADER_{args.n}__'):
            print(f'extern const uint8_t {args.variable}[{2**args.n+1}][{nbytes}];')
