#!/usr/bin/env python3
# validate_mapping.py <reference_csv> <generated_header_csv> (the header CSV is the exported table used in C++)
import sys, numpy as np
import matplotlib.pyplot as plt

def validate_mapping(ref_path, gen_path):
    ref = np.loadtxt(ref_path, delimiter=',')
    gen = np.loadtxt(gen_path, delimiter=',')
    # resample both to 512 points
    x = np.linspace(0,1,512)
    ref_i = np.interp(x, np.linspace(0,1,ref.size), ref)
    gen_i = np.interp(x, np.linspace(0,1,gen.size), gen)
    rmse = np.sqrt(np.mean((ref_i - gen_i)**2))
    print("RMSE:", rmse)
    plt.figure(figsize=(8,3))
    plt.plot(x, ref_i, label='ref')
    plt.plot(x, gen_i, label='gen', alpha=0.8)
    plt.legend(); plt.grid(True)
    plt.title("Mapping validation (RMSE={:.6g})".format(rmse))
    plt.savefig('results/mapping_validation.png', dpi=150)
    print("Saved results/mapping_validation.png")
    return rmse

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: python validate_mapping.py <reference_csv> <generated_header_csv>")
        sys.exit(1)
    validate_mapping(sys.argv[1], sys.argv[2])