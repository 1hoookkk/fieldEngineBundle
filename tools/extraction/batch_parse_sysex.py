#!/usr/bin/env python3
"""
Batch EMU SysEx Parser - Process entire folders of SysEx files
"""

import sys
import argparse
import glob
from pathlib import Path
from collections import defaultdict

# Add project root to path
ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.extraction.parse_emu_sysex import parse_sysex_file, merge_names_with_headers

def batch_parse_sysex(input_path: Path, output_csv: Path, pattern: str = "*.syx"):
    """
    Batch parse SysEx files from a directory or file pattern.
    
    Args:
        input_path: Directory or file pattern to process
        output_csv: Output CSV file path
        pattern: File pattern to match (default: *.syx)
    """
    
    # Find all matching files
    if input_path.is_file():
        files = [input_path]
    elif input_path.is_dir():
        files = list(input_path.glob(pattern))
    else:
        # Treat as glob pattern
        files = [Path(f) for f in glob.glob(str(input_path))]
    
    if not files:
        print(f"[ERROR] No files found matching: {input_path}")
        return False
    
    print(f"Found {len(files)} SysEx files to process...")
    
    all_rows = []
    processed_count = 0
    error_count = 0
    
    for file_path in files:
        try:
            rows = parse_sysex_file(file_path)
            all_rows.extend(rows)
            processed_count += 1
            if rows:
                print(f"[OK] {file_path.name}: {len(rows)} messages")
            else:
                print(f"[SKIP] {file_path.name}: no valid messages")
        except Exception as e:
            print(f"[ERROR] {file_path.name}: {e}")
            error_count += 1
    
    if not all_rows:
        print("[ERROR] No valid SysEx messages found in any files")
        return False
    
    # Merge names with headers
    merged_rows = merge_names_with_headers(all_rows)
    
    # Sort by ROM ID, then preset number, then layer
    merged_rows.sort(key=lambda x: (x["rom_id"], x["preset_num"], x["layer_index"]))
    
    # Write CSV
    fieldnames = ["filename", "preset_num", "rom_id", "preset_name", "layer_index", "filter_type_id", "message_type"]
    with output_csv.open("w", newline="") as f:
        import csv
        w = csv.DictWriter(f, fieldnames=fieldnames)
        w.writeheader()
        w.writerows(merged_rows)
    
    # Generate summary
    print(f"\n[SUCCESS] Processed {processed_count} files, {error_count} errors")
    print(f"Extracted {len(merged_rows)} total messages → {output_csv}")
    
    # ROM ID summary
    rom_counts = defaultdict(int)
    filter_type_counts = defaultdict(int)
    
    for row in merged_rows:
        rom_counts[row["rom_id"]] += 1
        if row["filter_type_id"] != -1:
            filter_type_counts[row["filter_type_id"]] += 1
    
    print("\nROM ID Summary:")
    for rom_id, count in sorted(rom_counts.items()):
        print(f"  ROM {rom_id:04X}: {count} messages")
    
    if filter_type_counts:
        print("\nFilter Type ID Summary:")
        for filter_id, count in sorted(filter_type_counts.items()):
            print(f"  Filter Type {filter_id}: {count} occurrences")
    
    # Message type summary
    msg_type_counts = defaultdict(int)
    for row in merged_rows:
        msg_type_counts[row["message_type"]] += 1
    
    print("\nMessage Type Summary:")
    for msg_type, count in sorted(msg_type_counts.items()):
        print(f"  {msg_type}: {count} messages")
    
    return True

def main():
    parser = argparse.ArgumentParser(
        description="Batch parse EMU SysEx files and extract preset information.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Process all .syx files in a directory
  python batch_parse_sysex.py --in ./syx_files --out parsed_presets.csv
  
  # Process specific file pattern
  python batch_parse_sysex.py --in "ALL_SYSEX/A2K/*/*.syx" --out audity_presets.csv
  
  # Process single file
  python batch_parse_sysex.py --in single_preset.syx --out output.csv
        """
    )
    
    parser.add_argument("--in", dest="input_path", type=Path, required=True,
                       help="Input directory, file, or glob pattern")
    parser.add_argument("--out", dest="output_csv", type=Path, required=True,
                       help="Output CSV file path")
    parser.add_argument("--pattern", default="*.syx",
                       help="File pattern to match (default: *.syx)")
    
    args = parser.parse_args()
    
    success = batch_parse_sysex(args.input_path, args.output_csv, args.pattern)
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
