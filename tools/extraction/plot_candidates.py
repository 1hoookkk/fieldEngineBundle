#!/usr/bin/env python3
# plot_candidates.py
# Plots CSV float tables exported by bin_inspector.py
import sys, glob, os
import numpy as np
import matplotlib.pyplot as plt

def plot_all(folder='results'):
    files = sorted(glob.glob(os.path.join(folder, '*float_table_*.csv')))
    if not files:
        print("No float_table CSVs found in", folder)
        return
    for f in files:
        arr = np.loadtxt(f, delimiter=',')
        plt.figure(figsize=(10,3))
        plt.plot(arr, '-', linewidth=1)
        plt.grid(True)
        plt.title(os.path.basename(f))
        out = f.replace('.csv', '.png')
        plt.tight_layout()
        plt.savefig(out, dpi=150)
        plt.close()
        print("Saved", out)

if __name__ == '__main__':
    folder = sys.argv[1] if len(sys.argv) > 1 else 'results'
    plot_all(folder)