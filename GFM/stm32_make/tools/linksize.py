#!/usr/bin/env python3

def parse_linker_script(data):
    pass

def link(groups):
    defined_symbols = {}
    undefined_symbols = set()
    for group, files in groups:
        while True:
            found_something = False

            for fn in files:
                symbols = load_symbols(fn)
                for symbol in symbols:
                    if symbol in defined_symbols:

            if not group or not found_something:
                break


if __name__ == '__main__':

    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('-T', '--script', type=str, help='Linker script to use')
    parser.add_argument('-o', '--output', type=str, help='Output file to produce')
    args, rest = parser.parse_known_intermixed_args()
    print(rest)

    addprefix = lambda *xs: [ prefix + opt for opt in xs for prefix in ('', '-Wl,') ]
    START_GROUP = addprefix('-(', '--start-group')
    END_GROUP = addprefix('-)', '--end-group')
    GROUP_OPTS = [*START_GROUP, *END_GROUP]
    input_files = [ arg for arg in rest if not arg.startswith('-') or arg in GROUP_OPTS ]

    def input_file_iter(input_files):
        group = False
        files = []
        for arg in input_files:
            if arg in START_GROUP:
                assert not group

                if files:
                    yield False, files # nested -Wl,--start-group
                group, files = True, []

            elif arg in END_GROUP:
                assert group # missing -Wl,--start-group
                if files:
                    yield True, files
                group, files = False, []

            else:
                files.append(arg)

        assert not group # missing -Wl,--end-group
        if files:
            yield False, files



