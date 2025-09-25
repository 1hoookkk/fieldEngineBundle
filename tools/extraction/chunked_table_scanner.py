#!/usr/bin/env python3
# tools/chunked_table_scanner.py
# Usage:
#   python tools/chunked_table_scanner.py <file> [--outdir results] [--chunk-mb 64]
#       [--min-table 64] [--stride 1] [--no-pcm]
#
# Finds candidate lookup tables (float32 LE/BE, uint16 monotonic) in large binaries
# without loading entire file in RAM. Exports CSV snippets around each candidate.

import os, sys, argparse, math, mmap
import numpy as np

def nice_size(n):
    for unit in ('B','KB','MB','GB','TB'):
        if n < 1024.0:
            return f"{n:,.1f} {unit}"
        n /= 1024.0
    return f"{n:.1f} PB"

def ensure_dir(p):
    if not os.path.exists(p):
        os.makedirs(p)

def scan_chunk_for_float_tables(buf: memoryview, base_off: int, outdir: str,
                                min_len=64, stride=1, endian='<', namehint='le'):
    """Heuristic: run-lengths of finite float32 with sane magnitude; monotonic or wavy."""
    arr = np.frombuffer(buf, dtype=(endian+'f4'))
    N = arr.size
    i = 0
    found = 0
    while i + min_len <= N:
        # sample a small window
        win = arr[i:i+min_len:stride]
        if not np.all(np.isfinite(win)):
            i += 1; continue
        mx = float(np.max(np.abs(win)))
        std = float(np.std(win))
        if mx < 1e6 and std > 1e-9:
            # try to extend forward while sane
            j = i + min_len
            while j < N:
                v = arr[j]
                if not np.isfinite(v) or abs(float(v)) >= 1e6:
                    break
                j += 1
            length = j - i
            if length >= min_len:
                # heuristic filters
                cand = arr[i:j]
                diffs = np.diff(cand)
                frac_pos = np.mean(diffs > 0)
                zc = np.sum(np.abs(np.diff(np.sign(cand))))/2
                monotonicish = (frac_pos > 0.85 or frac_pos < 0.15)
                sinusoidalish = (zc >= 4 and np.std(cand) > 1e-4)
                if monotonicish or sinusoidalish or (np.ptp(cand) > 1e-3 and length <= 4096):
                    # limit export size to keep artifacts small
                    clip_len = min(length, 2048)
                    out = cand[:clip_len]
                    ensure_dir(outdir)
                    idx = len([f for f in os.listdir(outdir) if f.endswith('.csv') and namehint in f])
                    path = os.path.join(outdir, f"tables_{namehint}_{idx:03d}_off_{base_off + i*4}.csv")
                    np.savetxt(path, out, delimiter=',', fmt='%.9g')
                    found += 1
                    i = j
                    continue
        i += 1
    return found

def scan_chunk_for_u16_tables(buf: memoryview, base_off: int, outdir: str, min_len=64, stride=1, endian='<'):
    arr = np.frombuffer(buf, dtype=(endian+'u2'))
    N = arr.size
    i = 0
    found = 0
    while i + min_len <= N:
        win = arr[i:i+min_len:stride]
        # monotonic nontrivial range
        diffs = np.diff(win.astype(np.int64))
        if (np.all(diffs >= 0) or np.all(diffs <= 0)) and (int(np.ptp(win)) > 8):
            # extend
            j = i + min_len
            last = win[-1]
            while j < N:
                v = arr[j]
                if (v < last and diffs[0] >= 0) or (v > last and diffs[0] <= 0):
                    break
                last = v
                j += 1
            length = j - i
            if length >= min_len:
                clip_len = min(length, 4096)
                out = arr[i:i+clip_len].astype(np.float32)  # save as float for convenience
                ensure_dir(outdir)
                idx = len([f for f in os.listdir(outdir) if f.endswith('.csv') and 'u16' in f])
                path = os.path.join(outdir, f"tables_u16_{idx:03d}_off_{base_off + i*2}.csv")
                np.savetxt(path, out, delimiter=',', fmt='%.9g')
                found += 1
                i = j
                continue
        i += 1
    return found

def scan_chunk_for_pcm(buf: memoryview, base_off: int, outdir: str):
    # Lightweight int16 RMS-based detector; exports small raw slices index only (no WAV)
    arr = np.frombuffer(buf, dtype='<i2')
    if arr.size < 8192: return 0
    x = arr.astype(np.float32)
    win = 4096
    # avoid heavy conv for huge chunks by downsampling envelope
    step = 512
    rms = np.sqrt(np.convolve(x[::step]**2, np.ones(8)/8.0, mode='valid'))
    if rms.size == 0: return 0
    thr = max(rms.mean()*2.0, 100.0)
    hits = int(np.sum(rms > thr))
    if hits > 0:
        # mark a tiny tag file with the region bounds (no large exports)
        ensure_dir(outdir)
        tag = os.path.join(outdir, f"pcm_marker_off_{base_off}.txt")
        with open(tag, 'w') as f:
            f.write(f"PCM-like energy detected near 0x{base_off:X} (chunk)\n")
        return 1
    return 0

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('file')
    ap.add_argument('--outdir', default='results')
    ap.add_argument('--chunk-mb', type=int, default=64)
    ap.add_argument('--min-table', type=int, default=64)
    ap.add_argument('--stride', type=int, default=1)
    ap.add_argument('--no-pcm', action='store_true')
    args = ap.parse_args()

    path = args.file
    size = os.path.getsize(path)
    print(f"File: {path}")
    print(f"Size: {size:,} bytes ({nice_size(size)})")
    ensure_dir(args.outdir)

    chunk = args.chunk_mb * 1024 * 1024
    overlap = 4096  # keep small overlap so we don't split tables on boundaries
    scanned = 0
    tot_f = tot_fb = tot_u16 = tot_pcm = 0

    with open(path, 'rb') as f, mmap.mmap(f.fileno(), length=0, access=mmap.ACCESS_READ) as mm:
        off = 0
        while off < size:
            end = min(size, off + chunk)
            # include overlap from previous chunk
            start = max(0, off - overlap if off else off)
            view = memoryview(mm)[start:end]
            print(f"  scanning {start:,}..{end:,} ({nice_size(end-start)})")

            # floats LE/BE
            tot_f  += scan_chunk_for_float_tables(view, start, args.outdir, min_len=args.min_table, stride=args.stride, endian='<', namehint='f32le')
            tot_fb += scan_chunk_for_float_tables(view, start, args.outdir, min_len=args.min_table, stride=args.stride, endian='>', namehint='f32be')
            # u16 monotonic (LE/BE)
            tot_u16 += scan_chunk_for_u16_tables(view, start, args.outdir, min_len=args.min_table, stride=args.stride, endian='<')
            tot_u16 += scan_chunk_for_u16_tables(view, start, args.outdir, min_len=args.min_table, stride=args.stride, endian='>')

            # PCM marker (optional)
            if not args.no_pcm:
                tot_pcm += scan_chunk_for_pcm(view, start, args.outdir)

            off = end
            scanned += 1

    print("\n=== summary ===")
    print("float32 LE tables:", tot_f)
    print("float32 BE tables:", tot_fb)
    print("uint16 monotonic :", tot_u16)
    print("PCM-like markers :", tot_pcm)
    print("CSV + markers in:", args.outdir)

if __name__ == '__main__':
    # keep numpy quiet about harmless warnings
    np.seterr(all='ignore')
    main()