#!/usr/bin/env python
# coding: utf-8

import binascii
import struct

import numpy as np
import pydub

from dsss_demod_test_waveform_gen import load_noise_gen, modulate as dsss_modulate

np.set_printoptions(linewidth=240)

def generate_noisy_signal(
        test_data=32,
        test_nbits=5,
        test_decimation=10,
        test_signal_amplitude=20e-3,
        noise_level=10e-3,
        noise_spec='synth://grid_freq_psd_spl_108pt.json',
        seed=0):

    #test_data = np.random.RandomState(seed=0).randint(0, 2 * (2**test_nbits), test_duration)
    #test_data = np.array([0, 1, 2, 3] * 50)
    if isinstance(test_data, int):
        test_data = np.array(range(test_data))


    signal = np.repeat(dsss_modulate(test_data, test_nbits) * 2.0 - 1, test_decimation)

    noise_gen, noise_params = load_noise_gen(noise_spec)
    noise = noise_gen(seed, len(signal), **noise_params)
    return np.absolute(noise + signal*test_signal_amplitude)

def write_raw_frequencies_bin(outfile, **kwargs):
    with open(outfile, 'wb') as f:
        for x in generate_noisy_signal(**kwargs):
            f.write(struct.pack('f', x))

def synthesize_sine(freqs, freqs_sampling_rate=10.0, output_sampling_rate=44100):
    duration = len(freqs) / freqs_sampling_rate # seconds
    afreq_out = np.interp(np.linspace(0, duration, int(duration*output_sampling_rate)), np.linspace(0, duration, len(freqs)), freqs)
    return np.sin(np.cumsum(2*np.pi * afreq_out / output_sampling_rate))

def write_flac(filename, signal, sampling_rate=44100):
    signal -= np.min(signal)
    signal /= np.max(signal)
    signal -= 0.5
    signal *= 2**16 - 1
    le_bytes = signal.astype(np.int16).tobytes()
    seg = pydub.AudioSegment(data=le_bytes, sample_width=2, frame_rate=sampling_rate, channels=1)
    seg.export(filename, format='flac')

def write_synthetic_hum_flac(filename, output_sampling_rate=44100, freqs_sampling_rate=10.0, **kwargs):
    signal = generate_noisy_signal(**kwargs)
    print(signal)
    write_flac(filename, synthesize_sine(signal, freqs_sampling_rate, output_sampling_rate),
            sampling_rate=output_sampling_rate)

def emulate_adc_signal(adc_bits=12, adc_offset=0.4, adc_amplitude=0.25, freq_sampling_rate=10.0, output_sampling_rate=1000, **kwargs):
    signal = synthesize_sine(generate_noisy_signal(), freq_sampling_rate, output_sampling_rate)
    signal = signal*adc_amplitude + adc_offset
    smin, smax = np.min(signal), np.max(signal)
    if smin < 0.0 or smax > 1.0:
        raise UserWarning('Amplitude or offset too large: Signal out of bounds with min/max [{smin}, {smax}] of ADC range')
    signal *= 2**adc_bits -1
    return signal

def save_adc_signal(fn, signal, dtype=np.uint16):
    with open(fn, 'wb') as f:
        f.write(signal.astype(dtype).tobytes())

def write_emulated_adc_signal_bin(filename, **kwargs):
    save_adc_signal(filename, emulate_adc_signal(**kwargs))

def hum_cmd(args):
    write_synthetic_hum_flac(args.out_flac,
            output_sampling_rate=args.audio_sampling_rate,
            freqs_sampling_rate=args.frequency_sampling_rate,
            test_data = np.array(list(binascii.unhexlify(args.data))),
            test_nbits = args.symbol_bits,
            test_decimation = args.decimation,
            test_signal_amplitude = args.signal_level/1e3,
            noise_level = args.noise_level/1e3,
            noise_spec=args.noise_spec,
            seed = args.random_seed)


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    cmd_parser = parser.add_subparsers(required=True)
    hum_parser = cmd_parser.add_parser('hum', help='Generated artificial modulated mains hum')
    # output parameters
    hum_parser.add_argument('-a', '--audio-sampling-rate', type=int, default=44100)

    # modulation parameters
    hum_parser.add_argument('-f', '--frequency-sampling-rate', type=float, default=10.0*100/128)
    hum_parser.add_argument('-b', '--symbol-bits', type=int, default=5, help='bits per symbol (excluding sign bit)')
    hum_parser.add_argument('-n', '--noise-level', type=float, default=1.0, help='Scale synthetic noise level')
    hum_parser.add_argument('-s', '--signal-level', type=float, default=20.0, help='Synthetic noise level in mHz')
    hum_parser.add_argument('-d', '--decimation', type=int, default=10, help='DSSS modulation decimation in frequency measurement cycles')
    hum_parser.add_argument('-r', '--random-seed', type=int, default=0)
    hum_parser.add_argument('--noise-spec', type=str, default='synth://grid_freq_psd_spl_108pt.json')
    hum_parser.add_argument('out_flac', metavar='out.flac', help='FLAC output file')
    hum_parser.add_argument('data', help='modulation data hex string')
    hum_parser.set_defaults(func=hum_cmd)

    args = parser.parse_args()
    args.func(args)
    
