# tools/extraction/probe_shapes.py
import math, struct, json
from syx_tools import split_and_unpack

def try_decode_pairs(words, mode='le_q0_16'):
    """Try to decode 12 words as 6 pole pairs (r,θ)."""
    # words: list[int] 16-bit
    pairs = []
    for i in range(0, 12, 2):
        if i+1 >= len(words):
            return None
        r_w  = words[i]
        th_w = words[i+1]

        if mode == 'le_q0_16':
            r  = r_w / 65535.0
            th = (th_w / 65536.0) * (2.0 * math.pi)
        elif mode == 'le_q1_15':
            r  = r_w / 32767.0
            th = (th_w / 65536.0) * (2.0 * math.pi)
        elif mode == 'be_q0_16':
            # Try big-endian
            r  = r_w / 65535.0
            th = (th_w / 65536.0) * (2.0 * math.pi)
        else:
            return None

        # fold angle to [-π, π)
        th = (th + math.pi) % (2*math.pi) - math.pi

        # Sanity check: radius should be in plausible range for Z-plane
        if not (0.70 < r < 0.999999):
            return None

        pairs.append((r, th))
    return pairs

def u16_stream_le(b: bytes):
    """Convert bytes to little-endian 16-bit words."""
    if len(b) % 2:
        b = b[:-1]
    return list(struct.unpack('<' + 'H'*(len(b)//2), b))

def u16_stream_be(b: bytes):
    """Convert bytes to big-endian 16-bit words."""
    if len(b) % 2:
        b = b[:-1]
    return list(struct.unpack('>' + 'H'*(len(b)//2), b))

def extract_candidate_shapes(syx_path):
    """Extract candidate Z-plane shapes from SysEx file."""
    unpacked = split_and_unpack(syx_path)
    shapes = []

    for msg_idx, msg in enumerate(unpacked):
        # Try both endianness
        for endian, u16_func in [('le', u16_stream_le), ('be', u16_stream_be)]:
            words = u16_func(msg)

            # Slide window looking for 12-word patterns
            for start_idx in range(0, max(0, len(words)-12), 2):  # step by 2 (pairs)
                candidate_words = words[start_idx:start_idx+12]
                if len(candidate_words) != 12:
                    continue

                # Try different decoding modes
                for mode in ['le_q0_16', 'le_q1_15']:
                    pairs = try_decode_pairs(candidate_words, mode)
                    if pairs:
                        # Additional heuristics for Z-plane shapes
                        angles = [abs(th) for r, th in pairs]
                        radii = [r for r, th in pairs]

                        # Check if angles are reasonably distributed (not all same)
                        angle_spread = max(angles) - min(angles)
                        radius_spread = max(radii) - min(radii)

                        if angle_spread > 0.1 and radius_spread > 0.05:  # Some variation
                            shape_info = {
                                'msg_index': msg_idx,
                                'word_offset': start_idx,
                                'endian': endian,
                                'mode': mode,
                                'pairs': pairs,
                                'angle_spread': angle_spread,
                                'radius_spread': radius_spread,
                                'avg_radius': sum(radii) / len(radii)
                            }
                            shapes.append(shape_info)

    return shapes

def shapes_to_compiled_json(shapes, sample_rate_ref=48000):
    """Convert extracted shapes to engine's compiled JSON format."""
    # Flatten pairs to match engine format [r0,θ0,r1,θ1,...]
    def pack_shape(pairs):
        v = []
        for r, th in pairs:
            v += [float(r), float(th)]
        return v

    # Deduplicate similar shapes
    unique_shapes = []
    tolerance = 1e-4

    for shape_info in shapes:
        flat = pack_shape(shape_info['pairs'])

        # Check if this shape is similar to any existing one
        is_duplicate = False
        for existing in unique_shapes:
            existing_flat = pack_shape(existing['pairs'])
            if len(flat) == len(existing_flat):
                # Calculate L2 distance
                diff = sum((a-b)**2 for a,b in zip(flat, existing_flat))
                rms_diff = math.sqrt(diff / len(flat))
                if rms_diff < tolerance:
                    is_duplicate = True
                    break

        if not is_duplicate:
            unique_shapes.append(shape_info)

    # Convert to engine format
    compiled_shapes = []
    for shape_info in unique_shapes:
        compiled_shapes.append(pack_shape(shape_info['pairs']))

    return {
        "sampleRateRef": sample_rate_ref,
        "count": len(compiled_shapes),
        "shapes": compiled_shapes,
        "extraction_meta": {
            "total_candidates": len(shapes),
            "unique_shapes": len(unique_shapes),
            "dedup_tolerance": tolerance
        }
    }

if __name__ == "__main__":
    import sys
    if len(sys.argv) < 2:
        print("Usage: python probe_shapes.py <sysex_file> [output.json]")
        sys.exit(1)

    syx_path = sys.argv[1]
    output_path = sys.argv[2] if len(sys.argv) > 2 else "extracted_shapes.json"

    print(f"Extracting shapes from {syx_path}...")
    shapes = extract_candidate_shapes(syx_path)
    print(f"Found {len(shapes)} candidate shapes")

    if shapes:
        compiled = shapes_to_compiled_json(shapes)
        with open(output_path, 'w') as f:
            json.dump(compiled, f, indent=2)
        print(f"Wrote {output_path} with {compiled['count']} unique shapes")
    else:
        print("No valid Z-plane shapes found")