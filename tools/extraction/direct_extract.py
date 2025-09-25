#!/usr/bin/env python3
import struct
from pathlib import Path
from syx_tools import split_and_unpack

def extract_exact_values():
    """Extract the exact values we saw in debug output."""

    # The file where we saw potential radius values
    target_file = "C:/fieldEngineBundle/ALL_SYSEX/CS/BEAT/presets/67-0127-bpmMarsGarden_PRES.syx"

    print(f"Extracting from: {target_file}")

    unpacked = split_and_unpack(target_file)

    for msg_idx, msg in enumerate(unpacked):
        print(f"\n=== Message {msg_idx} ({len(msg)} bytes) ===")

        # Try little-endian 16-bit words (this showed results in debug)
        try:
            words = struct.unpack('<' + 'H' * (len(msg) // 2), msg[:len(msg) // 2 * 2])

            # Look for the specific values we saw:
            # 28672 -> 0.8750, 24909 -> 0.7602, etc.
            target_words = [28672, 24909, 62450, 24903, 25714, 61029]

            found_positions = []
            for target in target_words:
                for pos, word in enumerate(words):
                    if word == target:
                        r_q115 = word / 32767.0
                        r_q016 = word / 65535.0
                        found_positions.append((pos, word, r_q115, r_q016))
                        print(f"  Found {word} at position {pos}: Q1.15={r_q115:.4f}, Q0.16={r_q016:.4f}")

            if found_positions:
                print(f"  Total matches: {len(found_positions)}")

                # Try to extract adjacent values as potential angles
                print(f"  Extracting 6 pole pairs:")
                for i in range(min(6, len(found_positions))):
                    pos, r_word, r_q115, r_q016 = found_positions[i]

                    # Look for angle in adjacent positions
                    angle_candidates = []
                    for offset in [-1, 1, -2, 2]:
                        angle_pos = pos + offset
                        if 0 <= angle_pos < len(words):
                            angle_word = words[angle_pos]
                            # Convert to angle assuming 16-bit turn
                            angle_rad = (angle_word / 65536.0) * 2 * 3.14159
                            angle_rad = ((angle_rad + 3.14159) % (2 * 3.14159)) - 3.14159
                            angle_candidates.append((offset, angle_word, angle_rad))

                    print(f"    Pole {i+1}: r={r_q115:.4f} (word={r_word})")
                    for offset, a_word, a_rad in angle_candidates[:2]:
                        print(f"      θ candidate @{offset:+2d}: {a_word:5d} -> {a_rad:+.4f} rad")

            else:
                print("  No target values found in this message")

        except struct.error as e:
            print(f"  Struct error: {e}")

def scan_beat_directory():
    """Scan all BEAT files for similar patterns."""

    beat_dir = Path("C:/fieldEngineBundle/ALL_SYSEX/CS/BEAT/presets")
    files = list(beat_dir.glob("*.syx"))

    print(f"\nScanning {len(files)} BEAT preset files...")

    candidates = []

    for syx_file in files[:10]:  # First 10 files
        try:
            unpacked = split_and_unpack(str(syx_file))

            radius_count = 0
            for msg in unpacked:
                if len(msg) >= 24:
                    words = struct.unpack('<' + 'H' * (len(msg) // 2), msg[:len(msg) // 2 * 2])

                    # Count values in radius range
                    for word in words:
                        r = word / 32767.0
                        if 0.70 <= r <= 0.999:
                            radius_count += 1

            if radius_count >= 6:
                rel_path = syx_file.relative_to(beat_dir.parent.parent)
                candidates.append((str(rel_path), radius_count))
                print(f"  {rel_path}: {radius_count} potential radius values")

        except Exception:
            continue

    return candidates

def main():
    # First, extract from the known good file
    extract_exact_values()

    # Then scan for similar files
    print("\n" + "="*60)
    candidates = scan_beat_directory()

    print(f"\nFound {len(candidates)} BEAT files with potential Z-plane data")

if __name__ == "__main__":
    main()