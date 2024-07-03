import os, sys
import ctypes as C
import argparse
import binascii
import numpy as np
import timeit
import statistics

lib = C.CDLL('rslib.so')

lib.rslib_encode.argtypes = [C.c_int, C.c_size_t, C.POINTER(C.c_char), C.POINTER(C.c_char)]
lib.rslib_decode.argtypes = [C.c_int, C.c_size_t, C.POINTER(C.c_char)]
lib.rslib_gexp.argtypes = [C.c_int, C.c_int]
lib.rslib_gexp.restype = C.c_int
lib.rslib_decode.restype = C.c_int
lib.rslib_npar.restype = C.c_size_t

def npar():
    return lib.rslib_npar()

def encode(data: bytes, nbits=8):
    out = C.create_string_buffer(len(data) + lib.rslib_npar())
    lib.rslib_encode(nbits, len(data), data, out)
    return out.raw

def decode(data: bytes, nbits=8):
    inout = C.create_string_buffer(data)
    lib.rslib_decode(nbits, len(data), inout)
    return inout.raw[:-lib.rslib_npar() - 1]

def cmdline_func_test(args, print=lambda *args, **kwargs: None, benchmark=False):
    st = np.random.RandomState(seed=args.seed)

    lfsr = [lib.rslib_gexp(i, args.bits) for i in range(2**args.bits - 1)]
    print('LFSR', len(set(lfsr)), lfsr)
    assert all(0 < x < 2**args.bits for x in lfsr)
    assert len(set(lfsr)) == 2**args.bits - 1

    print('Seed', args.seed)
    for i in range(args.repeat):
        print(f'Run {i}')
        test_data = bytes(st.randint(2**args.bits, size=args.message_length, dtype=np.uint8))
        print('        Raw:', binascii.hexlify(test_data).decode())
        encoded = encode(test_data, nbits=args.bits)
        print('     Encoded:', binascii.hexlify(encoded).decode())

        indices = st.permutation(len(encoded))
        encoded = list(encoded)
        for pos in indices[:args.errors]:
            encoded[pos] = st.randint(2**args.bits)
        encoded = bytes(encoded)
        print('    Modified:', ''.join(f'\033[91m{b:02x}\033[0m' if pos in indices[:args.errors] else f'{b:02x}' for pos, b in enumerate(encoded)))

        if benchmark:
            rpt = 10000
            delta = timeit.timeit('decode(encoded, nbits=args.bits)',
                    globals={'args': args, 'decode': decode, 'encoded': encoded},
                    number=rpt)/rpt
            print(f'Decoding runtime: {delta*1e6:.3f}μs')
        decoded = decode(encoded, nbits=args.bits)
        print('     Decoded:', binascii.hexlify(decoded).decode())
        print('       Delta:', binascii.hexlify(
                bytes(x^y for x, y in zip(test_data, decoded))
            ).decode().replace('0', '.'))
        assert test_data == decoded

def cmdline_func_encode(args, **kwargs):
    data = np.frombuffer(binascii.unhexlify(args.hex_str), dtype=np.uint8)
    # Map 8 bit input to 6 bit symbol string
    data = np.packbits(np.pad(np.unpackbits(data).reshape((-1, 6)), ((0,0),(2, 0))).flatten())
    encoded = encode(data.tobytes(), nbits=args.bits)
    print('symbol array:', ', '.join(f'0x{x:02x}' for x in encoded))
    print('hex string:', binascii.hexlify(encoded).decode())

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    cmd_parser = parser.add_subparsers(required=True)
    test_parser = cmd_parser.add_parser('test', help='Test reed-solomon implementation')
    test_parser.add_argument('-m', '--message-length', type=int, default=6, help='Test message (plaintext) length in bytes')
    test_parser.add_argument('-e', '--errors', type=int, default=2, help='Number of byte errors to insert into simulation')
    test_parser.add_argument('-r', '--repeat', type=int, default=1000, help='Repeat experiment -r times')
    test_parser.add_argument('-b', '--bits', type=int, default=8, help='Symbol bit size')
    test_parser.add_argument('-s', '--seed', type=int, default=0, help='Random seed')
    test_parser.set_defaults(func=cmdline_func_test)
    enc_parser = cmd_parser.add_parser('encode', help='RS-Encode given hex string')
    enc_parser.set_defaults(func=cmdline_func_encode)
    enc_parser.add_argument('-b', '--bits', type=int, default=8, help='Symbol bit size')
    enc_parser.add_argument('hex_str', type=str, help='Input data as hex string')
    args = parser.parse_args()
    args.func(args)

