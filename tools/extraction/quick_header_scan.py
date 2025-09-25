#!/usr/bin/env python3
# quick_header_scan.py - Fast initial scan of EMU file header
import sys
import struct

def scan_header(filepath, read_size=32768):
    """Quick scan of file header for signatures and structure"""
    with open(filepath, 'rb') as f:
        header = f.read(read_size)
        filesize = f.seek(0, 2)
    print(f"File: {filepath}")
    print(f"Size: {filesize:,} bytes ({filesize/1024/1024:.1f} MB)")

    # Look for common EMU signatures
    signatures = [
        b'FORM', b'CWAV', b'RIFF', b'EMU', b'EMU2',
        b'BANK', b'PRES', b'SAMP', b'TOC2', b'E5P1', b'E4P1'
    ]

    print("\n=== Signatures Found ===")
    for sig in signatures:
        pos = header.find(sig)
        if pos >= 0:
            # Look for more occurrences
            positions = []
            start = 0
            while True:
                pos = header.find(sig, start)
                if pos == -1:
                    break
                positions.append(pos)
                start = pos + 1
            print(f"'{sig.decode('ascii', errors='ignore')}': {positions[:10]}")

    print("\n=== First 128 bytes (hex) ===")
    for i in range(0, min(128, len(header)), 16):
        hex_part = ' '.join(f'{b:02X}' for b in header[i:i+16])
        ascii_part = ''.join(chr(b) if 32 <= b < 127 else '.' for b in header[i:i+16])
        print(f"{i:04X}: {hex_part:<48} {ascii_part}")

    print("\n=== Text Strings ===")
    text = ""
    strings_found = []
    for b in header:
        if 32 <= b < 127:
            text += chr(b)
        else:
            if len(text) >= 4:
                strings_found.append(text)
            text = ""

    unique_strings = sorted(set(strings_found))[:30]
    for s in unique_strings:
        print(f"  '{s}'")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python quick_header_scan.py <file>")
        sys.exit(1)
    scan_header(sys.argv[1])