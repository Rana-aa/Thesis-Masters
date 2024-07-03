#!/usr/bin/env python3

import os
import sys
import textwrap
import uuid
import hmac
import binascii
import time
from datetime import datetime

LINKING_KEY_SIZE = 15
PRESIG_VERSION = '000.001'
DOMAINS = ['all', 'country', 'region', 'vendor', 'series']

def format_hex(data, indent=4, wrap=True):
    indent = ' '*indent
    par = ', '.join(f'0x{b:02x}' for b in data)
    par = textwrap.fill(par, width=120,
        initial_indent=indent, subsequent_indent=indent,
        replace_whitespace=False, drop_whitespace=False)
    if wrap:
        return f'{{\n{par}\n}}'
    return par

def domain_string(domain, value):
    return f'smart reset domain string v{PRESIG_VERSION}: domain:{domain}={value}'

def keygen_cmd(args):
    if os.path.exists(args.keyfile) and not args.force:
        print("Error: keyfile already exists. We won't overwrite it. Instead please remove it manually.",
                file=sys.stderr)
        return 1

    root_key = os.urandom(LINKING_KEY_SIZE)

    with open(args.keyfile, 'wb') as f:
        f.write(binascii.hexlify(root_key))
        f.write(b'\n')
    return 0

def gen_at_height(domain, value, height, key):
    # nanananananana BLOCKCHAIN!

    ds = domain_string(domain, value).encode('utf-8')

    for height in range(height+1):
        key = hmac.digest(key, ds, 'sha512')[:LINKING_KEY_SIZE]

    return key

def auth_cmd(args):
    with open(args.keyfile, 'r') as f:
        root_key = binascii.unhexlify(f.read().strip())

    vals = [ (domain, getattr(args, domain)) for domain in DOMAINS if getattr(args, domain) is not None ]
    if not vals:
        vals = [('all', 'all')]
    for domain, value in vals:
        auth = gen_at_height(domain, value, args.height, root_key)
        print(f'{domain}="{value}" @{args.height}: {binascii.hexlify(auth).decode()}')


def prekey_cmd(args):
    with open(args.keyfile, 'r') as f:
        root_key = binascii.unhexlify(f.read().strip())

    print('#include <stdint.h>')
    print('#include <assert.h>')
    print()
    print('#include "crypto.h"')
    print()

    bundle_id = uuid.uuid4().bytes
    print(f'/* bundle id {binascii.hexlify(bundle_id).decode()} */')
    print(f'uint8_t presig_bundle_id[16] = {format_hex(bundle_id)};')
    print()
    print(f'/* generated on {datetime.now()} */')
    print(f'uint64_t bundle_timestamp = {int(time.time())};')
    print()
    print(f'int presig_height = {args.max_height};')
    print()

    print('const char *presig_domain_strings[_TRIGGER_DOMAIN_COUNT] = {')
    for domain in DOMAINS:
        ds = domain_string(domain, getattr(args, domain))
        assert '"' not in ds
        print(f'    [TRIGGER_DOMAIN_{domain.upper()}] = "{ds}",')
    print('};')
    print()

    print('uint8_t presig_keys[_TRIGGER_DOMAIN_COUNT][PRESIG_MSG_LEN] = {')
    for domain in DOMAINS:
        key = gen_at_height(domain, getattr(args, domain), args.max_height, root_key)
        print(f'    [TRIGGER_DOMAIN_{domain.upper()}] = {{{format_hex(key, indent=0, wrap=False)}}},')
    print('};')

    print()
    print('static inline void __hack_asserts_only(void) {')
    print(f'    static_assert(_TRIGGER_DOMAIN_COUNT == {len(DOMAINS)});')
    print(f'    static_assert(PRESIG_MSG_LEN == {LINKING_KEY_SIZE});')
    print('}')
    print()



TEST_VENDOR = 'Darthenschmidt Cyberei und Verschleierungstechnik GmbH'
TEST_SERIES = 'Frobnicator v0.23.7'
TEST_REGION = 'Neuland'
TEST_COUNTRY = 'Germany'

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('keyfile', help='Key file to use')

    subparsers = parser.add_subparsers(title='subcommands')
    keygen_parser = subparsers.add_parser('keygen', help='Generate a new key')
    keygen_parser.add_argument('-f', '--force', action='store_true', help='Force overwriting existing keyfile')
    keygen_parser.set_defaults(func=keygen_cmd)

    auth_parser = subparsers.add_parser('auth', help='Generate one-time authentication string')
    auth_parser.add_argument('height', type=int, help='Authentication string height, counting from 0 (root key)')
    auth_parser.set_defaults(func=auth_cmd)
    auth_parser.add_argument('-a', '--all', action='store_const', const='all', help='Vendor name for vendor domain')
    auth_parser.add_argument('-v', '--vendor', type=str, nargs='?', const=TEST_VENDOR, help='Vendor name for vendor domain')
    auth_parser.add_argument('-s', '--series', type=str, nargs='?', const=TEST_SERIES, help='Series identifier for series domain')
    auth_parser.add_argument('-r', '--region', type=str, nargs='?', const=TEST_REGION, help='Region name for region domain')
    auth_parser.add_argument('-c', '--country', type=str, nargs='?', const=TEST_COUNTRY, help='Country name for country domain')

    prekey_parser = subparsers.add_parser('prekey', help='Generate prekey data .C source code file')
    prekey_parser.add_argument('-m', '--max-height', type=int, default=8, help='Height of generated prekey')
    prekey_parser.add_argument('-v', '--vendor', type=str, default=TEST_VENDOR, help='Vendor name for vendor domain')
    prekey_parser.add_argument('-s', '--series', type=str, default=TEST_SERIES, help='Series identifier for series domain')
    prekey_parser.add_argument('-r', '--region', type=str, default=TEST_REGION, help='Region name for region domain')
    prekey_parser.add_argument('-c', '--country', type=str, default=TEST_COUNTRY, help='Country name for country domain')
    prekey_parser.set_defaults(func=prekey_cmd, all='all')

    args = parser.parse_args()
    sys.exit(args.func(args))

