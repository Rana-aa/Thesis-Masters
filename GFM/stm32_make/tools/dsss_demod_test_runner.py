#!/usr/bin/env python3

import os
import sys
from os import path
import subprocess
import json
from collections import namedtuple, defaultdict
from tqdm import tqdm
import uuid
import multiprocessing
import sqlite3 
import time
from urllib.parse import urlparse
import tempfile
import itertools

import numpy as np
np.set_printoptions(linewidth=240)

from dsss_demod_test_waveform_gen import load_noise_gen, modulate as dsss_modulate


def build_test_binary(nbits, thf, decimation, symbols, cachedir):
    build_id = str(uuid.uuid4())
    builddir = path.join(cachedir, build_id)
    os.mkdir(builddir)

    cwd = path.join(path.dirname(__file__), '..')

    env = os.environ.copy()
    env['BUILDDIR'] = path.abspath(builddir)
    env['DSSS_GOLD_CODE_NBITS'] = str(nbits)
    env['DSSS_DECIMATION'] = str(decimation)
    env['DSSS_THRESHOLD_FACTOR'] = str(thf)
    env['DSSS_WAVELET_WIDTH'] = str(0.73 * decimation)
    env['DSSS_WAVELET_LUT_SIZE'] = str(10 * decimation)
    env['TRANSMISSION_SYMBOLS'] = str(symbols)

    with open(path.join(builddir, 'make_stdout.txt'), 'w') as stdout,\
         open(path.join(builddir, 'make_stderr.txt'), 'w') as stderr:
        subprocess.run(['make', 'clean', os.path.abspath(path.join(builddir, 'tools/dsss_demod_test'))],
                env=env, cwd=cwd, check=True, stdout=stdout, stderr=stderr)

    return build_id

def sequence_matcher(test_data, decoded, max_shift=3):
    match_result = []
    for shift in range(-max_shift, max_shift):
        failures = -shift if shift < 0 else 0 # we're skipping the first $shift symbols
        a = test_data if shift > 0 else test_data[-shift:]
        b = decoded if shift < 0 else decoded[shift:]
        for i, (ref, found) in enumerate(itertools.zip_longest(a, b)):
            if ref is None: # end of signal
                break
            if ref != found:
                failures += 1
        match_result.append(failures)
    failures = min(match_result)
    return failures/len(test_data)

ResultParams = namedtuple('ResultParams', ['nbits', 'thf', 'decimation', 'symbols', 'seed', 'amplitude', 'background'])

def run_test(seed, amplitude_spec, background, nbits, decimation, symbols, thfs, lookup_binary, cachedir):
    noise_gen, noise_params = load_noise_gen(background)

    test_data = np.random.RandomState(seed=seed).randint(0, 2 * (2**nbits), symbols)
    
    signal = np.repeat(dsss_modulate(test_data, nbits) * 2.0 - 1, decimation)
    # We're re-using the seed here. This is not a problem.
    noise = noise_gen(seed, len(signal), *noise_params)
    amplitudes = amplitude_spec[0] * 10 ** np.linspace(0, amplitude_spec[1], amplitude_spec[2])
    # DEBUG
    my_pid = multiprocessing.current_process().pid
    wql = len(amplitudes) * len(thfs)
    print(f'[{my_pid}] starting, got workqueue of length {wql}')
    i = 0
    # Map lsb to sign to match test program
    # test_data = (test_data>>1) * (2*(test_data&1) - 1)
    # END DEBUG

    output = []
    for amp in amplitudes:
        with tempfile.NamedTemporaryFile(dir=cachedir) as f:
            waveform = signal*amp + noise
            f.write(waveform.astype('float32').tobytes())
            f.flush()
            # DEBUG
            fcopy = f'/tmp/test-{path.basename(f.name)}'
            import shutil
            shutil.copy(f.name, fcopy)
            # END DEBUG

            for thf in thfs:
                rpars = ResultParams(nbits, thf, decimation, symbols, seed, amp, background)
                cmdline = [lookup_binary(nbits, thf, decimation, symbols), f.name]
                # DEBUG
                starttime = time.time()
                # END DEBUG
                try:
                    proc = subprocess.run(cmdline, stdout=subprocess.PIPE, encoding='utf-8', check=True, timeout=300)

                    lines = proc.stdout.splitlines()
                    matched = [ l.partition('[')[2].partition(']')[0]
                            for l in lines if l.strip().startswith('data sequence received:') ]
                    matched = [ [ int(elem) for elem in l.split(',') ] for l in matched ]

                    ser = min(sequence_matcher(test_data, match) for match in matched) if matched else None
                    output.append((rpars, ser))
                    # DEBUG
                    #print(f'[{my_pid}] ran {i}/{wql}: time={time.time() - starttime}\n    {ser=}\n    {rpars}\n    {" ".join(cmdline)}\n    {fcopy}', flush=True)
                    i += 1
                    # END DEBUG

                except subprocess.TimeoutExpired:
                    output.append((rpars, None))
                    # DEBUG
                    print(f'[{my_pid}] ran {i}/{wql}: Timeout!\n    {rpars}\n    {" ".join(cmdline)}\n    {fcopy}', flush=True)
                    i += 1
                    # END DEBUG
    print(f'[{my_pid}] finished.')
    return output

def parallel_generator(db, table, columns, builder, param_list, desc, context={}, params_mapper=lambda *args: args,
        disable_cache=False):
    with multiprocessing.Pool(multiprocessing.cpu_count()) as pool:
        with db as conn:
            jobs = []
            for params in param_list:
                found_res = conn.execute(
                            f'SELECT result FROM {table} WHERE ({",".join(columns)}) = ({",".join("?"*len(columns))})',
                        params_mapper(*params)).fetchone()

                if found_res and not disable_cache:
                    yield params, json.loads(*found_res)

                else:
                    jobs.append((params, pool.apply_async(builder, params, context)))

        pool.close()
        print('Using', len(param_list) - len(jobs), 'cached jobs', flush=True)
        with tqdm(total=len(jobs), desc=desc) as tq:
            for i, (params, res) in enumerate(jobs):
                # DEBUG
                print('Got result', i, params, res)
                # END DEBUG
                tq.update(1)
                result = res.get()
                with db as conn:
                    conn.execute(f'INSERT INTO {table} VALUES ({"?,"*len(params)}?,?)',
                            (*params_mapper(*params), json.dumps(result), timestamp()))
                yield params, result
        pool.join()

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--dump', help='Write results to JSON file')
    parser.add_argument('-c', '--cachedir', default='dsss_test_cache', help='Directory to store build output and data in')
    parser.add_argument('-n', '--no-cache', action='store_true', help='Disable result cache')
    parser.add_argument('-b', '--batches', type=int, default=1, help='Number of batches to split the computation into')
    parser.add_argument('-i', '--index', type=int, default=0, help='Batch index to compute')
    parser.add_argument('-p', '--prepare', action='store_true', help='Prepare mode: compile runners, then exit.')
    args = parser.parse_args()

    DecoderParams = namedtuple('DecoderParams', ['nbits', 'thf', 'decimation', 'symbols'])
#    dec_paramses = [ DecoderParams(nbits=nbits, thf=thf, decimation=decimation, symbols=20)
#            for nbits in [5, 6]
#            for thf in [4.5, 4.0, 5.0]
#            for decimation in [10, 5, 22] ]
    dec_paramses = [ DecoderParams(nbits=nbits, thf=thf, decimation=decimation, symbols=100)
            for nbits in [5, 6]
            for thf in [3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 6.0, 6.5, 7.0, 7.5, 8.0, 8.5, 9.0, 9.5, 10.0]
            for decimation in [1, 2, 3, 4, 5, 6, 9, 10, 11, 12, 16, 22, 30, 40, 50] ]
#    dec_paramses = [ DecoderParams(nbits=nbits, thf=thf, decimation=decimation, symbols=100)
#            for nbits in [5, 6, 7, 8]
#            for thf in [1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 6.0, 6.5, 7.0, 7.5, 8.0, 8.5, 9.0, 9.5, 10.0]
#            for decimation in [1, 2, 3, 4, 5, 6, 9, 10, 11, 12, 16, 22, 30, 40, 50] ]

    build_cache_dir = path.join(args.cachedir, 'builds')
    data_cache_dir = path.join(args.cachedir, 'data')
    os.makedirs(build_cache_dir, exist_ok=True)
    os.makedirs(data_cache_dir, exist_ok=True)

    build_db = sqlite3.connect(path.join(args.cachedir, 'build_db.sqlite3'))
    build_db.execute('CREATE TABLE IF NOT EXISTS builds (nbits, thf, decimation, symbols, result, timestamp)')
    timestamp = lambda: int(time.time()*1000)

    builds = dict(parallel_generator(build_db, table='builds', columns=['nbits', 'thf', 'decimation', 'symbols'],
            builder=build_test_binary, param_list=dec_paramses, desc='Building decoders',
            context=dict(cachedir=build_cache_dir)))
    print('Done building decoders.')
    if args.prepare:
        sys.exit(0)

    GeneratorParams = namedtuple('GeneratorParams', ['seed', 'amplitude_spec', 'background'])
    gen_params = [ GeneratorParams(rep, (5e-3, 1, 5), background)
            #GeneratorParams(rep, (0.05e-3, 3.5, 50), background)
            for rep in range(50)
            for background in ['meas://fmeas_export_ocxo_2day.bin', 'synth://grid_freq_psd_spl_108pt.json'] ]
#    gen_params = [ GeneratorParams(rep, (5e-3, 1, 5), background)
#            for rep in range(1)
#            for background in ['meas://fmeas_export_ocxo_2day.bin'] ]

    data_db = sqlite3.connect(path.join(args.cachedir, 'data_db.sqlite3'))
    data_db.execute('CREATE TABLE IF NOT EXISTS waveforms'
                    '(seed, amplitude_spec, background, nbits, decimation, symbols, thresholds, result, timestamp)')

    'SELECT FROM waveforms GROUP BY (amplitude_spec, background, nbits, decimation, symbols, thresholds, result)'

    dec_param_groups = defaultdict(lambda: [])
    for nbits, thf, decimation, symbols in dec_paramses:
        dec_param_groups[(nbits, decimation, symbols)].append(thf)
    waveform_params = [ (*gp, *dp, thfs) for gp in gen_params for dp, thfs in dec_param_groups.items() ]
    print(f'Generated {len(waveform_params)} parameter sets')

    # Separate out our batch
    waveform_params = waveform_params[args.index::args.batches]

    def lookup_binary(*params):
        return path.join(build_cache_dir, builds[tuple(params)], 'tools/dsss_demod_test')

    def params_mapper(seed, amplitude_spec, background, nbits, decimation, symbols, thresholds):
        amplitude_spec = ','.join(str(x) for x in amplitude_spec)
        thresholds = ','.join(str(x) for x in thresholds)
        return seed, amplitude_spec, background, nbits, decimation, symbols, thresholds

    results = []
    for _params, chunk in parallel_generator(data_db, 'waveforms',
            ['seed', 'amplitude_spec', 'background', 'nbits', 'decimation', 'symbols', 'thresholds'],
            params_mapper=params_mapper,
            builder=run_test,
            param_list=waveform_params, desc='Simulating demodulation',
            context=dict(cachedir=data_cache_dir, lookup_binary=lookup_binary),
            disable_cache=args.no_cache):
        results += chunk

    if args.dump:
        with open(args.dump, 'w') as f:
            json.dump(results, f)

