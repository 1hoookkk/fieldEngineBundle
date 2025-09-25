#!/usr/bin/env python3
import argparse, json, math, struct, os
from pathlib import Path
from syx_tools import split_and_unpack, analyze_sysex_file
from probe_shapes import extract_candidate_shapes, shapes_to_compiled_json

def extract_priority_sounds(base_dir):
    """Extract Z-plane data from priority EMU sounds."""

    priority_files = [
        # AUDITY 2000 related
        "TECNO_v1/presets/65-0082-padZPlane2000_PRES.syx",
        "TECNO_v2/presets/65-0082-padZPlane2000_PRES.syx",
        "TECNO_v1/presets/65-0041-padSoft2000_PRES.syx",
        "TECNO_v2/presets/65-0041-padSoft2000_PRES.syx",

        # Freeze preset
        "CMPSR/presets/04-0284-bpmFreeze_PRES.syx",

        # BPM category samples (high priority)
        "BEAT/presets/67-0127-bpmMarsGarden_PRES.syx",
        "BEAT/presets/67-0209-bpmEveningVox_PRES.syx",
        "BEAT/presets/67-0108-pr1BPMLF01_PRES.syx",
        "BEAT/presets/67-0109-pr2BPMLFO1_PRES.syx",
        "BEAT/presets/67-0110-pr3BPMLFO1_PRES.syx",

        # Xtreme patterns
        "XLEAD/arps/07-0032-Xtreme_ARP.syx",
        "DRUM/arps/19-0032-Xtreme_ARP.syx",
    ]

    results = {}
    all_shapes = []

    for rel_path in priority_files:
        full_path = Path(base_dir) / rel_path
        if not full_path.exists():
            print(f"Warning: {rel_path} not found")
            continue

        print(f"Processing: {rel_path}")

        try:
            # Analyze the file structure
            info = analyze_sysex_file(str(full_path))

            # Extract candidate shapes
            shapes = extract_candidate_shapes(str(full_path))

            results[rel_path] = {
                "file_info": info,
                "shapes_found": len(shapes),
                "shapes": shapes[:5] if shapes else []  # Store first 5 for inspection
            }

            # Add to master list
            all_shapes.extend(shapes)

            print(f"  Found {len(shapes)} candidate shapes")

        except Exception as e:
            print(f"  Error processing {rel_path}: {e}")
            results[rel_path] = {"error": str(e)}

    return results, all_shapes

def main():
    parser = argparse.ArgumentParser(description="Extract Z-plane data from EMU SysEx ROMs")
    parser.add_argument("sysex_dir", help="Directory containing EMU SysEx files")
    parser.add_argument("--output", "-o", default="priority_shapes.json", help="Output JSON file")
    parser.add_argument("--analyze-only", action="store_true", help="Only analyze files, don't extract shapes")

    args = parser.parse_args()

    if not Path(args.sysex_dir).exists():
        print(f"Error: Directory {args.sysex_dir} not found")
        return 1

    print(f"Extracting priority EMU sounds from {args.sysex_dir}")
    print("=" * 60)

    # Extract from priority files
    results, all_shapes = extract_priority_sounds(args.sysex_dir)

    # Summary
    total_files = len([r for r in results if "error" not in results[r]])
    total_shapes = len(all_shapes)

    print("\n" + "=" * 60)
    print(f"SUMMARY:")
    print(f"  Files processed: {total_files}")
    print(f"  Total candidate shapes: {total_shapes}")

    if not args.analyze_only and all_shapes:
        # Convert to compiled format
        compiled = shapes_to_compiled_json(all_shapes)

        # Save results
        output_data = {
            "extraction_results": results,
            "compiled_shapes": compiled,
            "priority_sounds": {
                "audity_2000": [r for r in results if "2000" in r],
                "bpm_category": [r for r in results if "bpm" in r.lower()],
                "freeze": [r for r in results if "freeze" in r.lower()],
                "xtreme": [r for r in results if "xtreme" in r.lower()]
            }
        }

        with open(args.output, 'w') as f:
            json.dump(output_data, f, indent=2)

        print(f"  Unique shapes: {compiled['count']}")
        print(f"  Output saved to: {args.output}")

        # Print shape quality metrics
        if compiled['count'] > 0:
            print(f"\nSHAPE QUALITY METRICS:")
            for i, shape in enumerate(compiled['shapes'][:3]):  # Show first 3
                print(f"  Shape {i+1}: r_avg={sum(shape[::2])/6:.3f}, θ_spread={max(shape[1::2])-min(shape[1::2]):.3f}")

    return 0

if __name__ == "__main__":
    exit(main())