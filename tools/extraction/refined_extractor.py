#!/usr/bin/env python3
import struct, math, json
from pathlib import Path
from syx_tools import split_and_unpack

def extract_scattered_zplane_data(msg_data):
    """Extract Z-plane data from scattered locations in SysEx message."""
    candidates = []

    # Try both endianness and different scales
    for endian in ['<', '>']:
        for word_size in [2]:  # Focus on 16-bit words
            try:
                fmt = endian + 'H'
                words = struct.unpack(fmt * (len(msg_data) // 2),
                                    msg_data[:len(msg_data) // 2 * 2])

                # Find all potential radius values
                radius_candidates = []
                for i, w in enumerate(words):
                    for scale in [32767, 65535]:  # Q1.15, Q0.16
                        r = w / scale
                        if 0.70 <= r <= 0.999:
                            radius_candidates.append((i, w, r, scale))

                if len(radius_candidates) >= 6:  # Need at least 6 radii
                    # Try to find corresponding angle values
                    # Angles might be adjacent or at fixed offsets
                    for stride in [1, 2, 8, 16]:  # Common data layouts
                        shape_candidates = []

                        for pos, word, radius, r_scale in radius_candidates[:6]:
                            # Look for angle near this radius
                            angle_found = False
                            for offset in [-1, 1, stride, -stride]:
                                angle_pos = pos + offset
                                if 0 <= angle_pos < len(words):
                                    angle_word = words[angle_pos]
                                    # Try different angle encodings
                                    for a_scale in [32768, 65536]:
                                        angle = (angle_word / a_scale) * (2 * math.pi)
                                        angle = (angle + math.pi) % (2*math.pi) - math.pi

                                        # Check if angle is reasonable
                                        if -math.pi <= angle <= math.pi:
                                            shape_candidates.append((radius, angle))
                                            angle_found = True
                                            break
                                if angle_found:
                                    break

                        if len(shape_candidates) >= 6:
                            candidates.append({
                                'endian': endian,
                                'stride': stride,
                                'r_scale': r_scale,
                                'pairs': shape_candidates[:6],
                                'quality': len(shape_candidates)
                            })

            except struct.error:
                continue

    return candidates

def scan_for_zplane_files(base_dir, max_files=50):
    """Scan for files containing potential Z-plane data."""
    print(f"Scanning {base_dir} for Z-plane data...")

    results = {}
    zplane_files = []

    # Get all .syx files
    syx_files = list(Path(base_dir).rglob("*.syx"))
    print(f"Found {len(syx_files)} total SysEx files")

    # Sample files to test
    test_files = syx_files[:max_files] if len(syx_files) > max_files else syx_files

    for i, syx_file in enumerate(test_files):
        if i % 10 == 0:
            print(f"  Progress: {i}/{len(test_files)}")

        try:
            unpacked = split_and_unpack(str(syx_file))
            total_candidates = 0

            for msg in unpacked:
                if len(msg) >= 24:  # Need at least 12 words
                    candidates = extract_scattered_zplane_data(msg)
                    total_candidates += len(candidates)

            if total_candidates > 0:
                rel_path = syx_file.relative_to(base_dir)
                zplane_files.append((str(rel_path), total_candidates))
                print(f"    Z-plane data found: {rel_path} ({total_candidates} candidates)")

        except Exception as e:
            continue

    return sorted(zplane_files, key=lambda x: x[1], reverse=True)

def extract_best_shapes(syx_path):
    """Extract the best Z-plane shapes from a specific file."""
    print(f"\nExtracting shapes from: {syx_path}")

    unpacked = split_and_unpack(syx_path)
    all_candidates = []

    for msg_idx, msg in enumerate(unpacked):
        if len(msg) >= 24:
            candidates = extract_scattered_zplane_data(msg)
            for candidate in candidates:
                candidate['msg_index'] = msg_idx
                all_candidates.append(candidate)

    # Sort by quality (number of pairs found)
    all_candidates.sort(key=lambda x: x['quality'], reverse=True)

    # Convert best candidates to engine format
    shapes = []
    for candidate in all_candidates[:3]:  # Top 3 candidates
        flat_shape = []
        for r, theta in candidate['pairs']:
            flat_shape.extend([float(r), float(theta)])
        shapes.append(flat_shape)

    return shapes

def main():
    base_dir = "C:/fieldEngineBundle/ALL_SYSEX/CS"

    # First, scan for files with Z-plane data
    zplane_files = scan_for_zplane_files(base_dir, 100)

    print(f"\n=== FOUND {len(zplane_files)} FILES WITH Z-PLANE DATA ===")
    for file_path, count in zplane_files[:10]:
        print(f"  {file_path}: {count} candidates")

    if zplane_files:
        # Extract shapes from the best file
        best_file = Path(base_dir) / zplane_files[0][0]
        shapes = extract_best_shapes(str(best_file))

        # Save results
        output = {
            "source_file": str(zplane_files[0][0]),
            "sampleRateRef": 48000,
            "count": len(shapes),
            "shapes": shapes,
            "all_zplane_files": zplane_files[:20]  # Top 20
        }

        with open("refined_extraction.json", 'w') as f:
            json.dump(output, f, indent=2)

        print(f"\nExtracted {len(shapes)} shapes from best file")
        print(f"Results saved to: refined_extraction.json")

if __name__ == "__main__":
    main()