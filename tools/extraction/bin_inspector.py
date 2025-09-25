#!/usr/bin/env python3
# bin_inspector.py
# Usage: python bin_inspector.py <file> [--min-table 64] [--outdir results]
# Scans a binary for ASCII strings, float32 arrays, and PCM-like int16 regions.
# Requires: Python 3.8+, numpy

import sys, os, argparse
import numpy as np

def find_ascii_strings(data, minlen=6):
    out, cur = [], []
    for b in data:
        if 32 <= b < 127:
            cur.append(chr(b))
        else:
            if len(cur) >= minlen:
                out.append(''.join(cur))
            cur = []
    if len(cur) >= minlen:
        out.append(''.join(cur))
    return out

def export_csv(arr, path):
    np.savetxt(path, arr, fmt='%.9g', delimiter=',')

def scan_float32_tables(data_bytes, min_len=64, outdir='results', basename='file'):
    floats_le = np.frombuffer(data_bytes, dtype='<f4')  # little-endian float32
    N = floats_le.size
    candidates = []
    i = 0
    while i < N - min_len:
        chunk = floats_le[i:i+min_len]
        if not np.all(np.isfinite(chunk)):
            i += 1
            continue
        mx = float(np.max(np.abs(chunk)))
        std = float(np.std(chunk))
        if mx < 1e6 and std > 1e-6:
            # expand forward while finite
            j = i + min_len
            while j < N and np.isfinite(floats_le[j]) and abs(float(floats_le[j])) < 1e6:
                j += 1
            length = j - i
            if length >= min_len:
                arr = floats_le[i:j]
                # heuristic: monotonic or sinusoidal or envelope-like
                diffs = np.diff(arr)
                frac_pos = np.mean(diffs > 0)
                zc = np.sum(np.abs(np.diff(np.sign(arr))))/2
                if frac_pos > 0.85 or frac_pos < 0.15 or zc >= 3 or (np.ptp(arr) > 1e-3 and length <= 4096):
                    idx = len(candidates)
                    path = os.path.join(outdir, f'{basename}_float_table_{idx:03d}.csv')
                    if not os.path.exists(outdir): os.makedirs(outdir)
                    export_csv(arr, path)
                    candidates.append((i, j, path))
                    i = j
                    continue
        i += 1
    return candidates

def scan_int16_audio_like(data_bytes, min_samples=48000, outdir='results', basename='file'):
    int16 = np.frombuffer(data_bytes, dtype='<i2').astype(np.int32)
    N = int16.size
    candidates = []
    if N < min_samples:
        return candidates
    # sliding RMS
    win = 4096
    rms = np.sqrt(np.convolve(int16.astype(np.float64)**2, np.ones(win)/win, mode='valid'))
    thresh = max(np.mean(rms) * 2.0, 1.0)
    above = rms > thresh
    i = 0; L = above.size
    while i < L:
        if above[i]:
            j = i
            while j < L and above[j]:
                j += 1
            s = max(0, i)
            e = min(N, j + win)
            if (e - s) >= min_samples:
                candid = (s, e)
                candidates.append(candid)
                # export raw int16
                if not os.path.exists(outdir): os.makedirs(outdir)
                rawpath = os.path.join(outdir, f'{basename}_pcm_int16_{len(candidates)-1:03d}.raw')
                int16[s:e].astype('<i2').tofile(rawpath)
            i = j
        else:
            i += 1
    return candidates

def main():
    p = argparse.ArgumentParser()
    p.add_argument('file')
    p.add_argument('--min-table', type=int, default=64)
    p.add_argument('--outdir', default='results')
    args = p.parse_args()
    fname = args.file
    with open(fname, 'rb') as f:
        data = f.read()
    data_b = bytearray(data)
    print("File size:", len(data_b))
    print("Extracting ASCII strings (first 60):")
    strings = find_ascii_strings(data_b, minlen=6)
    for s in strings[:60]:
        print("  ", s)
    print("Scanning float32 arrays...")
    fl = scan_float32_tables(data, min_len=args.min_table, outdir=args.outdir, basename=os.path.basename(fname))
    print("Found float candidates:", len(fl))
    print("Scanning int16 audio-like regions...")
    pcm = scan_int16_audio_like(data, min_samples=48000, outdir=args.outdir, basename=os.path.basename(fname))
    print("Found PCM candidates:", len(pcm))
    print("Results saved in", args.outdir)

if __name__ == '__main__':
    main()