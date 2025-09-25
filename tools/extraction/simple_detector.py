#!/usr/bin/env python3
import struct
from pathlib import Path
from syx_tools import split_and_unpack

def find_radius_clusters(msg_data):
    """Find clusters of potential radius values in binary data."""
    radius_counts = 0

    # Try both endianness for 16-bit words
    for endian in ['<', '>']:
        try:
            fmt = endian + 'H'
            words = struct.unpack(fmt * (len(msg_data) // 2),
                                msg_data[:len(msg_data) // 2 * 2])

            # Count potential radius values
            for w in words:
                for scale in [32767, 65535]:  # Q1.15, Q0.16
                    r = w / scale
                    if 0.70 <= r <= 0.999:
                        radius_counts += 1
                        break  # Don't double-count same word

        except struct.error:
            continue

    return radius_counts

def quick_scan(base_dir, min_radius_count=6):
    """Quick scan for files with potential Z-plane data."""
    print(f"Quick scanning for files with Z-plane potential...")

    candidates = []
    syx_files = list(Path(base_dir).rglob("*.syx"))

    # Sample files from different categories
    test_files = []
    categories = ['BEAT', 'TECNO', 'CMPSR', 'VROM', 'QROM', 'PHATT', 'XLEAD']

    for category in categories:
        cat_files = [f for f in syx_files if category in str(f)]
        test_files.extend(cat_files[:5])  # 5 files per category

    print(f"Testing {len(test_files)} files from {len(categories)} categories")

    for syx_file in test_files:
        try:
            unpacked = split_and_unpack(str(syx_file))
            max_radius_count = 0

            for msg in unpacked:
                if len(msg) >= 24:  # Need meaningful data
                    count = find_radius_clusters(msg)
                    max_radius_count = max(max_radius_count, count)

            if max_radius_count >= min_radius_count:
                rel_path = syx_file.relative_to(base_dir)
                candidates.append((str(rel_path), max_radius_count))
                print(f"  FOUND: {rel_path} ({max_radius_count} radius values)")

        except Exception as e:
            continue

    return sorted(candidates, key=lambda x: x[1], reverse=True)

def detailed_analysis(syx_path):
    """Detailed analysis of a promising file."""
    print(f"\n=== DETAILED ANALYSIS: {syx_path} ===")

    unpacked = split_and_unpack(syx_path)

    for msg_idx, msg in enumerate(unpacked):
        if len(msg) < 24:
            continue

        print(f"\nMessage {msg_idx} ({len(msg)} bytes):")

        # Try both endianness
        for endian in ['<', '>']:
            try:
                fmt = endian + 'H'
                words = struct.unpack(fmt * (len(msg) // 2),
                                    msg[:len(msg) // 2 * 2])

                radius_values = []
                for i, w in enumerate(words):
                    for scale in [32767, 65535]:
                        r = w / scale
                        if 0.70 <= r <= 0.999:
                            radius_values.append((i, w, r, scale))
                            break

                if len(radius_values) >= 6:
                    print(f"  {endian}-endian: {len(radius_values)} radius values")
                    for i, (pos, word, r, scale) in enumerate(radius_values[:12]):
                        print(f"    [{pos:3d}] {word:5d} -> {r:.4f} (Q-scale={scale})")

                    # Look for patterns in positions
                    positions = [rv[0] for rv in radius_values[:12]]
                    if len(positions) >= 6:
                        diffs = [positions[i+1] - positions[i] for i in range(len(positions)-1)]
                        print(f"    Position deltas: {diffs[:10]}")

            except struct.error:
                continue

def main():
    base_dir = "C:/fieldEngineBundle/ALL_SYSEX/CS"

    # Quick scan first
    candidates = quick_scan(base_dir, min_radius_count=6)

    print(f"\n=== SUMMARY ===")
    print(f"Found {len(candidates)} files with potential Z-plane data:")
    for file_path, count in candidates:
        print(f"  {file_path}: {count} radius values")

    # Detailed analysis of top candidates
    if candidates:
        for file_path, count in candidates[:3]:  # Top 3
            full_path = Path(base_dir) / file_path
            detailed_analysis(str(full_path))

if __name__ == "__main__":
    main()