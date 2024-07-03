#!/usr/bin/env python3

import re
import subprocess
import tempfile
import pprint

ARCHIVE_RE = r'([^(]*)(\([^)]*\))?'

def trace_source_files(linker, cmdline):
    with tempfile.NamedTemporaryFile() as mapfile:
        output = subprocess.check_output([linker, f'-Wl,--Map={mapfile.name}', *cmdline])

        # intentionally use generator here
        idx = 0 
        lines = [ line.rstrip() for line in mapfile.read().decode().splitlines() if line.strip() ]
        
        for idx, line in enumerate(lines[idx:], start=idx):
            #print('Dropping', line)
            if line == 'Linker script and memory map':
                break

        idx += 1
        objects = []
        symbols = {}
        sections = {}
        current_object = None
        last_offset = None
        last_symbol = None
        cont_sec = None
        cont_ind = None
        current_section = None
        for idx, line in enumerate(lines[idx:], start=idx):
            print(f'Processing >{line}')
            if line.startswith('LOAD'):
                _load, obj = line.split()
                objects.append(obj)
                continue

            if line.startswith('OUTPUT'):
                break

            m = re.match(r'^( ?)([^ ]+)? +(0x[0-9a-z]+) +(0x[0-9a-z]+)?(.*)?$', line)
            if m is None:
                m = re.match(r'^( ?)([^ ]+)?$', line)
                if m:
                    cont_ind, cont_sec = m.groups()
                else:
                    cont_ind, cont_sec = None, None
                last_offset, last_symbol = None, None
                continue
            indent, sec, offx, size, sym_or_src = m.groups()
            if sec is None:
                sec = cont_sec
                ind = cont_ind
            cont_sec = None
            cont_ind = None
            print(f'vals: indent={indent} sec={sec} offx={offx} size={size} sym_or_src={sym_or_src}')
            if not re.match('^[a-zA-Z_0-9<>():*]+$', sym_or_src):
                continue

            if indent == '':
                print(f'Section: {sec} 0x{size:x}')
                current_section = sec
                sections[sec] = size
                last_offset = None
                last_symbol = None
                continue

            if offx is not None:
                offx = int(offx, 16)
            if size is not None:
                size = int(size, 16)

            if size is not None and sym_or_src is not None:
                # archive/object line
                archive, _member = re.match(ARCHIVE_RE, sym_or_src).groups()
                current_object = archive
                last_offset = offx
            else:
                if sym_or_src is not None:
                    assert size is None
                    if last_offset is not None:
                        last_size = offx - last_offset
                        symbols[last_symbol] = (last_size, current_section)
                        print(f'Symbol: {last_symbol} 0x{last_size:x} @{current_section}')
                    last_offset = offx
                    last_symbol = sym_or_src

        idx += 1

        for idx, line in enumerate(lines[idx:], start=idx):
            if line == 'Cross Reference Table':
                break

        idx += 1

        # map which symbol was pulled from which object in the end
        used_defs = {}
        for line in lines:
            *left, right = line.split()

            archive, _member = re.match(ARCHIVE_RE, right).groups()
            if left:
                used_defs[''.join(left)] = archive

        #pprint.pprint(symbols)


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('linker_binary')
    parser.add_argument('linker_args', nargs=argparse.REMAINDER)
    args = parser.parse_args()

    source_files = trace_source_files(args.linker_binary, args.linker_args)

