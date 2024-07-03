
from os import path
import json
import functools

import numpy as np
import numbers
import math
from scipy import signal as sig
import scipy.fftpack

sampling_rate = 10 # sp/s

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

def modulate(data, nbits=5):
    # 0, 1 -> -1, 1
    mask = np.array(gold(nbits))*2 - 1
    
    sel = mask[data>>1]
    data_lsb_centered = ((data&1)*2 - 1)

    signal = (np.multiply(sel, np.tile(data_lsb_centered, (2**nbits-1, 1)).T).flatten() + 1) // 2
    return np.hstack([ np.zeros(len(mask)), signal, np.zeros(len(mask)) ])

def load_noise_meas_params(capture_file):
    with open(capture_file, 'rb') as f:
        meas_data = np.copy(np.frombuffer(f.read(), dtype='float32'))
        meas_data -= np.mean(meas_data)
        return (meas_data,)

def mains_noise_measured(seed, n, meas_data):
    last_valid = len(meas_data) - n
    st = np.random.RandomState(seed)
    start = st.randint(last_valid)
    return meas_data[start:start+n] + 50.00

def load_noise_synth_params(specfile):
    with open(specfile) as f:
        d = json.load(f)
        return {'spl_x': np.linspace(*d['x_spec']),
                'spl_N': d['x_spec'][2],
                'psd_spl': (d['t'], d['c'], d['k']) }

def mains_noise_synthetic(seed, n, psd_spl, spl_N, spl_x):
    st = np.random.RandomState(seed)
    noise = st.normal(size=spl_N) * 2
    spec = scipy.fftpack.fft(noise) **2

    spec *= np.exp(scipy.interpolate.splev(spl_x, psd_spl))

    spec **= 1/2
    
    renoise = scipy.fftpack.ifft(spec)
    return renoise[10000:][:n] + 50.00

@functools.lru_cache()
def load_noise_gen(url):
    schema, refpath = url.split('://')
    if not path.isabs(refpath):
        refpath = path.abspath(path.join(path.dirname(__file__), refpath))

    if schema == 'meas':
        return mains_noise_measured, load_noise_meas_params(refpath)
    elif schema == 'synth':
        return mains_noise_synthetic, load_noise_synth_params(refpath)
    else:
        raise ValueError('Invalid schema', schema)

