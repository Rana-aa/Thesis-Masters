
import tempfile
import os
from os import path
import sys
import re
import subprocess
from contextlib import contextmanager
from collections import defaultdict
import colorsys

import cxxfilt
from elftools.elf.elffile import ELFFile
from elftools.elf.enums import ENUM_ST_SHNDX
from elftools.elf.descriptions import describe_symbol_type, describe_sh_type
import libarchive
import matplotlib.cm

@contextmanager
def chdir(newdir):
    old_cwd = os.getcwd()
    try:
        os.chdir(newdir)
        yield
    finally:
        os.chdir(old_cwd)

def keep_last(it, first=None):
    last = first
    for elem in it:
        yield last, elem
        last = elem

def delim(start, end, it, first_only=True):
    found = False
    for elem in it:
        if end(elem):
            if first_only:
                return
            found = False
        elif start(elem):
            found = True
        elif found:
            yield elem

def delim_prefix(start, end, it):
    yield from delim(lambda l: l.startswith(start), lambda l: end is not None and l.startswith(end), it)

def trace_source_files(linker, cmdline, trace_sections=[], total_sections=['.text', '.data', '.rodata']):
    with tempfile.TemporaryDirectory() as tempdir:
        out_path = path.join(tempdir, 'output.elf')
        output = subprocess.check_output([linker, '-o', out_path, f'-Wl,--print-map', *cmdline])
        lines = [ line.strip() for line in output.decode().splitlines() ]
        # FIXME also find isr vector table references

        defs = {}
        objs = defaultdict(lambda: 0)
        aliases = {}
        sec_name = None
        last_loc = None
        last_sym = None
        line_cont = None
        for last_line, line in keep_last(delim_prefix('Linker script and memory map', 'OUTPUT', lines), first=''):
            if not line or line.startswith('LOAD '):
                sec_name = None
                continue

            # first part of continuation line
            if m := re.match('^(\.[0-9a-zA-Z-_.]+)$', line):
                line_cont = line
                sec_name = None
                continue

            if line_cont:
                line = line_cont + ' ' + line
                line_cont = None

            # -ffunction-sections/-fdata-sections section
            if m := re.match('^(\.[0-9a-zA-Z-_.]+)\.([0-9a-zA-Z-_.]+)\s+(0x[0-9a-f]+)\s+(0x[0-9a-f]+)\s+(\S+)$', line):
                sec, sym, loc, size, obj = m.groups()
                *_, sym = sym.rpartition('.')
                sym = cxxfilt.demangle(sym)
                size = int(size, 16)
                obj = path.abspath(obj)

                if sec not in total_sections:
                    size = 0

                objs[obj] += size
                defs[sym] = (sec, size, obj)

                sec_name, last_loc, last_sym = sec, loc, sym
                continue
            
            # regular (no -ffunction-sections/-fdata-sections) section
            if m := re.match('^(\.[0-9a-zA-Z-_]+)\s+(0x[0-9a-f]+)\s+(0x[0-9a-f]+)\s+(\S+)$', line):
                sec, _loc, size, obj = m.groups()
                size = int(size, 16)
                obj = path.abspath(obj)

                if sec in total_sections:
                    objs[obj] += size

                sec_name = sec
                last_loc, last_sym = None, None
                continue
            
            # symbol def
            if m := re.match('^(0x[0-9a-f]+)\s+(\S+)$', line):
                loc, sym = m.groups()
                sym = cxxfilt.demangle(sym)
                loc = int(loc, 16)
                if sym in defs:
                    continue

                if loc == last_loc:
                    assert last_sym is not None
                    aliases[sym] = last_sym
                else:
                    assert sec_name
                    defs[sym] = (sec_name, None, obj)
                    last_loc, last_sym = loc, sym

                continue

        refs = defaultdict(lambda: set())
        for sym, (sec, size, obj) in defs.items():
            fn, _, member = re.match('^([^()]+)(\((.+)\))?$', obj).groups()
            fn = path.abspath(fn)

            if member:
                subprocess.check_call(['ar', 'x', '--output', tempdir, fn, member])
                fn = path.join(tempdir, member)

            with open(fn, 'rb') as f:
                elf = ELFFile(f)

                symtab = elf.get_section_by_name('.symtab')
                
                symtab_demangled = { cxxfilt.demangle(nsym.name).replace(' ', ''): i
                        for i, nsym in enumerate(symtab.iter_symbols()) }

                s = set()
                sec_map = { sec.name: i for i, sec in enumerate(elf.iter_sections()) }
                matches = [ i for name, i in sec_map.items() if re.match(f'\.rel\..*\.{sym}', name) ]
                if matches:
                    sec = elf.get_section(matches[0])
                    for reloc in sec.iter_relocations():
                        refsym = symtab.get_symbol(reloc['r_info_sym'])
                        name = refsym.name if refsym.name else elf.get_section(refsym['st_shndx']).name.split('.')[-1]
                        s.add(name)
                refs[sym] = s

                for tsec in trace_sections:
                    matches = [ i for name, i in sec_map.items() if name == f'.rel{tsec}' ]
                    s = set()
                    if matches:
                        sec = elf.get_section(matches[0])
                        for reloc in sec.iter_relocations():
                            refsym = symtab.get_symbol(reloc['r_info_sym'])
                            s.add(refsym.name)
                    refs[tsec.replace('.', '_')] |= s

        return objs, aliases, defs, refs

@contextmanager
def wrap(leader='', print=print, left='{', right='}'):
    print(leader, left)
    yield lambda *args, **kwargs: print('   ', *args, **kwargs)
    print(right)

def mangle(name):
    return re.sub('[^a-zA-Z0-9_]', '_', name)

hexcolor = lambda r, g, b, *_a: f'#{int(r*255):02x}{int(g*255):02x}{int(b*255):02x}'
def vhex(val):
    r,g,b,_a = matplotlib.cm.viridis(1.0-val)
    fc = hexcolor(r, g, b)  
    h,s,v = colorsys.rgb_to_hsv(r,g,b)
    cc = '#000000' if v > 0.8 else '#ffffff'
    return fc, cc

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--trace-sections', type=str, action='append', default=[])
    parser.add_argument('--trim-stubs', type=str, action='append', default=[])
    parser.add_argument('--highlight-subdirs', type=str, default=None)
    parser.add_argument('linker_binary')
    parser.add_argument('linker_args', nargs=argparse.REMAINDER)
    args = parser.parse_args()

    trace_sections = args.trace_sections
    trace_sections_mangled = { sec.replace('.', '_') for sec in trace_sections }
    objs, aliases, syms, refs = trace_source_files(args.linker_binary, args.linker_args, trace_sections)

    clusters = defaultdict(lambda: [])
    for sym, (sec, size, obj) in syms.items():
        clusters[obj].append((sym, sec, size))

    max_ssize = max(size or 0 for _sec, size, _obj in syms.values())
    max_osize = max(objs.values())

    subdir_prefix = path.abspath(args.highlight_subdirs) + '/' if args.highlight_subdirs else '### NO HIGHLIGHT ###'
    first_comp = lambda le_path: path.dirname(le_path).partition(os.sep)[0]
    subdir_colors = sorted({ first_comp(obj[len(subdir_prefix):]) for obj in objs if obj.startswith(subdir_prefix) })
    subdir_colors = { path: hexcolor(*matplotlib.cm.Pastel1(i/len(subdir_colors))) for i, path in enumerate(subdir_colors) }

    subdir_sizes = defaultdict(lambda: 0)
    for obj, size in objs.items():
        if not isinstance(size, int):
            continue
        if obj.startswith(subdir_prefix):
            subdir_sizes[first_comp(obj[len(subdir_prefix):])] += size
        else:
            subdir_sizes['<others>'] += size

    print('Subdir sizes:', file=sys.stderr)
    for subdir, size in sorted(subdir_sizes.items(), key=lambda x: x[1]):
        print(f'{subdir:>20}: {size:>6,d} B', file=sys.stderr)

    def lookup_highlight(path):
        if args.highlight_subdirs:
            if obj.startswith(subdir_prefix):
                highlight_head = first_comp(path[len(subdir_prefix):])
                return subdir_colors[highlight_head], highlight_head
            else:
                return '#e0e0e0', None
        else:
            return '#ddf7f4', None

    with wrap('digraph G', print) as lvl1print:
        print('size="23.4,16.5!";')
        print('graph [fontsize=40];')
        print('node [fontsize=40];')
        #print('ratio="fill";')

        print('rankdir=LR;')
        print('ranksep=5;')
        print('nodesep=0.2;')
        print()

        for i, (obj, obj_syms) in enumerate(clusters.items()):
            with wrap(f'subgraph cluster_{i}', lvl1print) as lvl2print:
                print('style = "filled";')
                highlight_color, highlight_head = lookup_highlight(obj)
                print(f'bgcolor = "{highlight_color}";')
                print('pencolor = none;')
                fc, cc = vhex(objs[obj]/max_osize)
                highlight_subdir_part = f'<font face="carlito" color="{cc}" point-size="40">{highlight_head} / </font>' if highlight_head else ''
                lvl2print(f'label = <<table border="0"><tr><td border="0" cellpadding="5" bgcolor="{fc}">'
                          f'{highlight_subdir_part}'
                          f'<font face="carlito" color="{cc}"><b>{path.basename(obj)} ({objs[obj]}B)</b></font>'
                          f'</td></tr></table>>;')
                lvl2print()
                for sym, sec, size in obj_syms:
                    has_size = isinstance(size, int) and size > 0
                    size_s = f' ({size}B)' if has_size else ''
                    fc, cc = vhex(size/max_ssize) if has_size else ('#ffffff', '#000000')
                    shape = 'box' if sec == '.text' else 'oval'
                    lvl2print(f'{mangle(sym)}[label = "{sym}{size_s}", style="rounded,filled", shape="{shape}", fillcolor="{fc}", fontname="carlito", fontcolor="{cc}" color=none];')
            lvl1print()

        edges = set()
        for start, ends in refs.items():
            for end in ends:
                end = aliases.get(end, end)
                if (start in syms or start in trace_sections_mangled) and end in syms:
                    edges.add((start, end))

        for start, end in edges:
            lvl1print(f'{mangle(start)} -> {mangle(end)} [style="bold", color="#333333"];')

        for sec in trace_sections:
            lvl1print(f'{sec.replace(".", "_")} [label = "section {sec}", shape="box", style="filled,bold"];')

