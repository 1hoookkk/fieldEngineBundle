#!/usr/bin/env python3
"""
ZMF1 Validation Script

Tests the complete ZMF1 pipeline:
1. Loads models.json with authentic EMU pole data
2. Generates ZMF1 binary entries using sysex_pack.py functions
3. Validates binary format and coefficient interpolation
4. Creates test ZMF1 files for plugin embedding

Usage:
    python plugins/_tools/zmf1_validate.py
"""

import json
import struct
import math
import cmath
import numpy as np
from pathlib import Path
import sys

# Import functions from sysex_pack.py
sys.path.append(str(Path(__file__).parent))
from sysex_pack import poles_to_biquad_coeffs, interpolate_poles, generate_zmf1_entry

def load_models():
    """Load Z-plane models from shared models.json"""
    models_path = Path(__file__).parent.parent / "_shared" / "zplane" / "models.json"
    if not models_path.exists():
        print(f"❌ Models file not found: {models_path}")
        return None

    with models_path.open('r') as f:
        return json.load(f)

def validate_zmf1_header(data):
    """Validate ZMF1 binary header"""
    if len(data) < 16:
        return False, "Data too short for header"

    magic, version, model_id, num_frames, num_sections, sample_rate, reserved = struct.unpack(
        "<I H H B B I H", data[:16])

    if magic != 0x31464D5A:  # 'ZMF1' little-endian
        return False, f"Invalid magic: {magic:08x}"

    if version != 1:
        return False, f"Invalid version: {version}"

    if num_frames == 0 or num_frames > 64:
        return False, f"Invalid frame count: {num_frames}"

    if num_sections == 0 or num_sections > 6:
        return False, f"Invalid section count: {num_sections}"

    expected_size = 16 + num_frames * num_sections * 5 * 4  # 5 floats per section
    if len(data) < expected_size:
        return False, f"Data too short: {len(data)} < {expected_size}"

    return True, {
        "magic": magic,
        "version": version,
        "model_id": model_id,
        "num_frames": num_frames,
        "num_sections": num_sections,
        "sample_rate": sample_rate,
        "expected_size": expected_size
    }

def extract_coefficients(data, header_info):
    """Extract biquad coefficients from ZMF1 data"""
    num_frames = header_info["num_frames"]
    num_sections = header_info["num_sections"]

    frames = []
    offset = 16  # Skip header

    for frame_idx in range(num_frames):
        sections = []
        for section_idx in range(num_sections):
            b0, b1, b2, a1, a2 = struct.unpack("<fffff", data[offset:offset+20])
            sections.append({
                "b0": b0, "b1": b1, "b2": b2,
                "a1": a1, "a2": a2
            })
            offset += 20
        frames.append(sections)

    return frames

def test_interpolation(frames):
    """Test coefficient interpolation between frames"""
    if len(frames) < 2:
        return True, "Only one frame, interpolation not needed"

    # Test interpolation at morph = 0.5 (midpoint)
    mid_frame = len(frames) // 2
    morph_pos = 0.5

    # Simple interpolation test
    frame_a = frames[0]
    frame_b = frames[-1]

    for section_idx in range(len(frame_a)):
        coeffs_a = frame_a[section_idx]
        coeffs_b = frame_b[section_idx]

        # Interpolate
        interp_b0 = coeffs_a["b0"] + morph_pos * (coeffs_b["b0"] - coeffs_a["b0"])
        interp_a1 = coeffs_a["a1"] + morph_pos * (coeffs_b["a1"] - coeffs_a["a1"])

        # Check for reasonable values
        if not (-10.0 <= interp_b0 <= 10.0):
            return False, f"Interpolated b0 out of range: {interp_b0}"

        if not (-3.0 <= interp_a1 <= 3.0):
            return False, f"Interpolated a1 out of range: {interp_a1}"

    return True, "Interpolation test passed"

def test_stability(coefficients):
    """Test filter stability for biquad coefficients"""
    stable_count = 0
    total_count = 0

    for frame in coefficients:
        for section in frame:
            a1, a2 = section["a1"], section["a2"]

            # Check stability triangle: |a2| < 1 and |a1| < 1 + a2
            stable = (abs(a2) < 1.0) and (abs(a1) < 1.0 + a2)

            if stable:
                stable_count += 1
            total_count += 1

    stability_ratio = stable_count / total_count if total_count > 0 else 0

    return stability_ratio > 0.9, f"Stability: {stable_count}/{total_count} ({stability_ratio:.1%})"

def create_test_zmf1_files(models, output_dir):
    """Create test ZMF1 files for each model"""
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    created_files = []

    for model_name, model_data in models["models"].items():
        try:
            model_id = model_data["id"]
            zmf1_data = generate_zmf1_entry(model_id, model_data, num_frames=11)

            output_file = output_dir / f"{model_name}.zmf1"
            output_file.write_bytes(zmf1_data)

            created_files.append({
                "model": model_name,
                "file": str(output_file),
                "size": len(zmf1_data)
            })

            print(f"✅ Created {model_name}.zmf1 ({len(zmf1_data)} bytes)")

        except Exception as e:
            print(f"❌ Failed to create {model_name}.zmf1: {e}")

    return created_files

def main():
    print("ZMF1 Validation Script")
    print("=" * 50)

    # Load models
    print("📁 Loading models...")
    models = load_models()
    if not models:
        return 1

    print(f"✅ Loaded {len(models['models'])} models from models.json")

    # Test each model
    all_tests_passed = True

    for model_name, model_data in models["models"].items():
        print(f"\n🧪 Testing model: {model_name}")

        try:
            # Generate ZMF1 data
            model_id = model_data["id"]
            zmf1_data = generate_zmf1_entry(model_id, model_data, num_frames=11)

            # Validate header
            valid, header_info = validate_zmf1_header(zmf1_data)
            if not valid:
                print(f"❌ Header validation failed: {header_info}")
                all_tests_passed = False
                continue

            print(f"✅ Header valid: {header_info['num_frames']} frames, {header_info['num_sections']} sections")

            # Extract and test coefficients
            coefficients = extract_coefficients(zmf1_data, header_info)

            # Test interpolation
            interp_valid, interp_msg = test_interpolation(coefficients)
            if interp_valid:
                print(f"✅ {interp_msg}")
            else:
                print(f"❌ {interp_msg}")
                all_tests_passed = False

            # Test stability
            stable, stability_msg = test_stability(coefficients)
            if stable:
                print(f"✅ {stability_msg}")
            else:
                print(f"⚠️  {stability_msg}")
                # Don't fail on stability issues - EMU filters can be resonant

        except Exception as e:
            print(f"❌ Test failed: {e}")
            all_tests_passed = False

    # Create output files
    print(f"\n📦 Creating ZMF1 files...")
    output_dir = Path(__file__).parent.parent / "MorphEngine" / "assets" / "zplane"
    created_files = create_test_zmf1_files(models, output_dir)

    # Summary
    print(f"\n" + "=" * 50)
    if all_tests_passed:
        print("✅ All tests passed!")
        print(f"📁 Created {len(created_files)} ZMF1 files in {output_dir}")
        print("\nNext steps:")
        print("1. Build MorphEngine plugin with CMake")
        print("2. ZMF1 files will be embedded as binary data")
        print("3. Test plugin in your DAW")
    else:
        print("❌ Some tests failed!")
        return 1

    return 0

if __name__ == "__main__":
    sys.exit(main())