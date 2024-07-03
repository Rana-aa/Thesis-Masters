#!/usr/bin/env python3
import subprocess
from os import path
import binascii
import re

import presig_gen

def do_test(domain, value, height, root_key, binary, expect_fail=False):
    auth = presig_gen.gen_at_height(domain, value, height, root_key)
    auth = binascii.hexlify(auth).decode()

    output = subprocess.check_output([binary, auth])
    *lines, rc_line = output.decode().splitlines()
    rc = int(re.match('^rc=(\d+)$', rc_line).group(1))
    assert expect_fail == (rc == 0)

def run_tests(root_key, max_height, binary):
    for domain, value in {
            'all': 'all',
            'vendor': presig_gen.TEST_VENDOR,
            'series': presig_gen.TEST_SERIES,
            'country': presig_gen.TEST_COUNTRY,
            'region': presig_gen.TEST_REGION,
            }.items():
        for height in range(max_height):
            do_test(domain, value, height, root_key, binary)
            do_test(domain, 'fail', height, root_key, binary, expect_fail=True)
            do_test('fail', 'fail', height, root_key, binary, expect_fail=True)
            do_test('', '', height, root_key, binary, expect_fail=True)
        do_test(domain, value, max_height, root_key, binary, expect_fail=True)
        do_test(domain, value, max_height+1, root_key, binary, expect_fail=True)

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('keyfile', help='Root key file')
    parser.add_argument('max_height', type=int, default=8, nargs='?', help='Height of generated prekeys')
    default_binary = path.abspath(path.join(path.dirname(__file__), '../build/tools/crypto_test'))
    parser.add_argument('binary', default=default_binary, nargs='?', help='crypto_test binary to use')
    args = parser.parse_args()

    with open(args.keyfile, 'r') as f:
        root_key = binascii.unhexlify(f.read().strip())

    run_tests(root_key, args.max_height, args.binary)
