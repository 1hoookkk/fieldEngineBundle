#!/usr/bin/env python3
"""Compare morphEngine renders across sample rates.

Usage:
    python check_sr_morph.py --ref morph_48k.wav --compare morph_44k.wav morph_96k.wav

Each file should be a mono WAV render of the same preset/morph lane.
The script prints median/95th percentile spectral deltas in dB.
"""
import argparse
import numpy as np
import soundfile as sf
from scipy.signal import welch

EPS = 1e-12


def load_spectrum(path: str):
    audio, sr = sf.read(path, always_2d=True)
    if audio.shape[1] > 1:
        audio = audio[:, 0]
    audio = audio.astype(np.float64)
    freqs, psd = welch(audio, sr, nperseg=8192, noverlap=4096, scaling="spectrum")
    mag = 10.0 * np.log10(psd + EPS)
    mag -= np.mean(mag)
    return sr, freqs, mag


def interpolate_magnitude(src_freqs, src_mag, dst_freqs):
    return np.interp(dst_freqs, src_freqs, src_mag, left=src_mag[0], right=src_mag[-1])


def compare(reference_path: str, compare_paths: list[str]):
    ref_sr, ref_freqs, ref_mag = load_spectrum(reference_path)
    print(f"Reference: {reference_path} ({ref_sr} Hz)")
    for path in compare_paths:
        sr, freqs, mag = load_spectrum(path)
        interp = interpolate_magnitude(freqs, mag, ref_freqs)
        delta = np.abs(interp - ref_mag)
        median = float(np.median(delta))
        p95 = float(np.percentile(delta, 95.0))
        print(f"  {path}: median={median:.2f} dB, 95th={p95:.2f} dB")


def main():
    parser = argparse.ArgumentParser(description="Compare morph spectra across sample rates.")
    parser.add_argument("--ref", required=True, help="reference WAV (e.g. 48k render)")
    parser.add_argument("--compare", nargs="+", required=True, help="comparison WAVs (44.1k, 96k, etc.)")
    args = parser.parse_args()
    compare(args.ref, args.compare)


if __name__ == "__main__":
    main()
