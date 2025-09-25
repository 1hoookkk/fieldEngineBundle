#!/usr/bin/env python3
"""
Quick analysis of Audity 2000 file structure
"""
import struct
import sys
import os

def analyze_file(filepath):
    """Analyze the structure of an EMU file"""
    if not os.path.exists(filepath):
        print(f"File not found: {filepath}")
        return

    filesize = os.path.getsize(filepath)
    print(f"File: {filepath}")
    print(f"Size: {filesize:,} bytes ({filesize/1024/1024:.1f} MB)")
    print()

    with open(filepath, 'rb') as f:
        # Read first 1024 bytes for header analysis
        header = f.read(1024)

        print("=== Header Analysis ===")

        # Look for common EMU signatures
        signatures = [
            b'FORM', b'CWAV', b'RIFF', b'EMU', b'EMU2',
            b'BANK', b'PRES', b'SAMP', b'TOC2', b'E5P1'
        ]

        for sig in signatures:
            pos = header.find(sig)
            if pos >= 0:
                print(f"Found '{sig.decode('ascii', errors='ignore')}' at offset {pos:04X}")

        print()

        # Show hex dump of first 256 bytes
        print("=== First 256 bytes ===")
        for i in range(0, min(256, len(header)), 16):
            hex_part = ' '.join(f'{b:02X}' for b in header[i:i+16])
            ascii_part = ''.join(chr(b) if 32 <= b < 127 else '.' for b in header[i:i+16])
            print(f"{i:04X}: {hex_part:<48} {ascii_part}")

        print()

        # Look for text strings (4+ printable chars)
        print("=== Text Strings (first 4096 bytes) ===")
        f.seek(0)
        chunk = f.read(4096)

        text = ""
        strings_found = []
        for b in chunk:
            if 32 <= b < 127:  # Printable ASCII
                text += chr(b)
            else:
                if len(text) >= 4:
                    strings_found.append(text)
                text = ""

        # Show unique strings
        unique_strings = list(set(strings_found))[:20]  # First 20 unique
        for s in sorted(unique_strings):
            print(f"  '{s}'")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python analyze_audity.py <file_path>")
        sys.exit(1)

    analyze_file(sys.argv[1])