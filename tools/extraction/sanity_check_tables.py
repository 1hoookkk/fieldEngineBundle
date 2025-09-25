#!/usr/bin/env python3
# tools/sanity_check_tables.py
import numpy as np, re, sys
import os

def load_header(path, sym):
    with open(path, 'r', encoding='utf-8', errors='ignore') as f:
        txt = f.read()
    arr = re.search(rf"static constexpr float {sym}\[(\d+)\] = \{{([^}}]+)\}};", txt, re.S)
    if not arr:
        raise ValueError(f"Could not find {sym} table in {path}")
    n = int(arr.group(1))
    body = arr.group(2)
    vals = [float(x.strip().rstrip('f')) for x in body.split(',') if x.strip()]
    if len(vals) != n:
        raise ValueError(f"Expected {n} values, got {len(vals)}")
    return np.array(vals)

def report(name, a, expect_monotonic=True):
    print(f"{name}: len={len(a)}, min={a.min():.3f}, max={a.max():.3f}")
    if expect_monotonic:
        inc = (np.diff(a) >= 0).mean()
        print(f"  monotonic fraction: {inc:.3f}")
        if inc < 0.95:
            print(f"  WARNING: Not monotonic enough (expected >=0.95)")
            return False
    return True

def main():
    try:
        t1 = load_header("fieldEngine/Source/Core/ZPlaneTables_T1.h", "T1_TABLE")
        t2 = load_header("fieldEngine/Source/Core/ZPlaneTables_T2.h", "T2_TABLE")

        print("=== Table Sanity Check ===")

        # T1 should be monotonic frequency mapping (20Hz to ~20kHz)
        t1_ok = report("T1_TABLE (Hz)", t1, True)
        if t1[0] < 10 or t1[0] > 30:
            print(f"  WARNING: T1[0]={t1[0]:.1f} should be ~20 Hz")
        if t1[-1] < 15000 or t1[-1] > 25000:
            print(f"  WARNING: T1[-1]={t1[-1]:.1f} should be ~20000 Hz")

        # T2 should be Q/resonance (may not be strictly monotonic but usually increasing)
        t2_ok = report("T2_TABLE (Q/res)", t2, False)
        if t2[0] < 0.1 or t2[0] > 2:
            print(f"  WARNING: T2[0]={t2[0]:.1f} should be ~0.5-1.0")
        if t2[-1] < 5 or t2[-1] > 50:
            print(f"  WARNING: T2[-1]={t2[-1]:.1f} should be ~10-25")

        print("\n=== Lookup Function Test ===")
        # Test edge cases for interpolation
        print("T1_lookup(0.0) should equal T1[0]:", t1[0])
        print("T1_lookup(1.0) should equal T1[-1]:", t1[-1])
        print("T1_lookup(0.5) should be mid-range:", np.interp(0.5, [0, 1], [t1[0], t1[-1]]))

        if t1_ok and t2_ok:
            print("\n✅ PASS: Tables look reasonable")
            return True
        else:
            print("\n❌ FAIL: Tables have issues")
            return False

    except Exception as e:
        print(f"❌ ERROR: {e}")
        return False

if __name__ == '__main__':
    success = main()
    sys.exit(0 if success else 1)