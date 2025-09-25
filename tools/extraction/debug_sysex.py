#!/usr/bin/env python3
import struct
from pathlib import Path
from syx_tools import split_and_unpack, analyze_sysex_file

def hex_dump(data, max_bytes=256):
    """Create a hex dump of binary data."""
    lines = []
    for i in range(0, min(len(data), max_bytes), 16):
        chunk = data[i:i+16]
        hex_part = ' '.join(f'{b:02x}' for b in chunk)
        ascii_part = ''.join(chr(b) if 32 <= b <= 126 else '.' for b in chunk)
        lines.append(f'{i:04x}: {hex_part:<48} {ascii_part}')
    if len(data) > max_bytes:
        lines.append(f'... ({len(data)} total bytes)')
    return '\n'.join(lines)

def analyze_emu_structure(syx_path):
    """Deep analysis of EMU SysEx structure."""
    print(f"\n=== ANALYZING: {syx_path} ===")

    if not Path(syx_path).exists():
        print(f"File not found: {syx_path}")
        return

    # Basic file info
    file_size = Path(syx_path).stat().st_size
    print(f"File size: {file_size} bytes")

    # Analyze SysEx messages
    info = analyze_sysex_file(syx_path)
    print(f"SysEx messages: {info.get('num_messages', 0)}")

    # Look at raw structure
    unpacked = split_and_unpack(syx_path)
    for i, msg in enumerate(unpacked[:3]):  # Only first 3 messages
        print(f"\n--- Message {i} ({len(msg)} bytes) ---")
        print(hex_dump(msg, 128))

        # Look for repeating patterns (potential shape data)
        if len(msg) >= 24:  # At least 12 words
            # Try different word sizes and endianness
            for word_size, fmt in [(2, 'H'), (4, 'I')]:
                for endian in ['<', '>']:
                    try:
                        words = struct.unpack(endian + fmt * (len(msg) // word_size),
                                            msg[:len(msg) // word_size * word_size])

                        # Look for potential radius values (0.7-0.999 as fixed point)
                        if word_size == 2:  # 16-bit
                            potential_radii = []
                            for w in words[:24]:  # Check first 24 words
                                for scale in [32767, 65535]:  # Q1.15, Q0.16
                                    r = w / scale
                                    if 0.7 <= r <= 0.999:
                                        potential_radii.append((w, r, scale))

                            if len(potential_radii) >= 6:  # At least 6 radii (for 6 poles)
                                print(f"  Potential radii found ({endian}{fmt}):")
                                for w, r, scale in potential_radii[:6]:
                                    print(f"    {w:5d} -> {r:.4f} (scale={scale})")

                    except struct.error:
                        continue

def main():
    # Test files from different categories
    test_files = [
        "C:/fieldEngineBundle/ALL_SYSEX/CS/TECNO_v1/presets/65-0082-padZPlane2000_PRES.syx",
        "C:/fieldEngineBundle/ALL_SYSEX/CS/BEAT/presets/67-0127-bpmMarsGarden_PRES.syx",
        "C:/fieldEngineBundle/ALL_SYSEX/CS/CMPSR/presets/04-0284-bpmFreeze_PRES.syx"
    ]

    for test_file in test_files:
        analyze_emu_structure(test_file)

if __name__ == "__main__":
    main()