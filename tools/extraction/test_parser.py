#!/usr/bin/env python3
import sys
import glob
from pathlib import Path
import csv
from collections import defaultdict

# Add project root to path
ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.extraction.parse_emu_sysex import parse_sysex_file, merge_names_with_headers

# Get sample files
audity_files = glob.glob('ALL_SYSEX/A2K/AUDTY/*.syx')[:10]
xtreme_files = glob.glob('ALL_SYSEX/A2K/XTREM/*.syx')[:10]
xrom_files = glob.glob('ALL_SYSEX/CS/XROM/presets/*.syx')[:10]

all_files = audity_files + xtreme_files + xrom_files

print(f"Testing parser on {len(all_files)} files...")

all_rows = []
for file_path in all_files:
    path = Path(file_path)
    try:
        rows = parse_sysex_file(path)
        all_rows.extend(rows)
        print(f"[OK] {path.name}: {len(rows)} messages")
    except Exception as e:
        print(f"[ERROR] {path.name}: {e}")

# Merge names with headers
merged_rows = merge_names_with_headers(all_rows)

# Sort by ROM ID, then preset number
merged_rows.sort(key=lambda x: (x["rom_id"], x["preset_num"]))

# Write CSV
with open("test_all_presets.csv", "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=["filename", "preset_num", "rom_id", "preset_name"])
    w.writeheader()
    w.writerows(merged_rows)

print(f"\n[SUCCESS] Wrote {len(merged_rows)} presets → test_all_presets.csv")

# Show summary
rom_counts = defaultdict(int)
for row in merged_rows:
    rom_counts[row["rom_id"]] += 1

print("\nROM ID Summary:")
for rom_id, count in sorted(rom_counts.items()):
    print(f"  ROM {rom_id:04X}: {count} presets")
