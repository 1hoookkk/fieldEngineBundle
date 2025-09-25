#!/usr/bin/env python3
"""
EMU Proteus Family SysEx Parser
Follows Proteus Family SysEx 2.2 spec to extract preset information.
"""

import sys
import csv
from pathlib import Path
from collections import defaultdict

def parse_sysex_file(path: Path):
    """Parse a single EMU SysEx .syx file and extract preset info"""
    data = path.read_bytes()
    results = []

    # SysEx starts with F0, ends with F7
    if not (data[0] == 0xF0 and data[-1] == 0xF7):
        print(f"[WARN] {path} not valid SysEx")
        return results

    # Manufacturer check: E-MU = 0x18 0x0? (depending on family)
    manu_id = data[1]
    if manu_id != 0x18:
        print(f"[WARN] {path} not E-MU? Manufacturer={manu_id:02X}")

    # EMU SysEx structure: F0 18 0F dd 55 CC SS payload... F7
    # Command is at position 5, subcommand at position 6
    if len(data) < 7:
        print(f"[WARN] {path} too short")
        return results
        
    cmd = data[5]
    subcmd = data[6]

    # --- Preset Dump Header (0x10 0x01 or 0x10 0x03) ---
    if cmd == 0x10 and subcmd in (0x01, 0x03):
        payload = data[7:-1]  # strip F0 header + F7
        # bytes: nn,nn | xxxx xxxx | aa..jj | kk,kk
        if len(payload) < 30:
            print(f"[WARN] {path} payload too short: {len(payload)} bytes")
            return results

        preset_num = payload[0] + (payload[1] << 7)  # 14-bit LSB/MSB
        # skip to kk,kk at end
        rom_id = payload[-2] + (payload[-1] << 7)

        results.append({
            "filename": path.name,
            "preset_num": preset_num,
            "rom_id": rom_id,
            "preset_name": "",  # may be filled by Generic Name
            "layer_index": -1,  # header doesn't have layer info
            "filter_type_id": -1,  # header doesn't have filter info
            "message_type": "header"
        })

    # --- Generic Name (0x0B) ---
    if cmd == 0x0B:
        payload = data[7:-1]
        if len(payload) >= 20:
            obj_type = payload[0]
            if obj_type == 1:  # 1 = preset
                preset_num = payload[1] + (payload[2] << 7)
                rom_id = payload[3] + (payload[4] << 7)
                name_bytes = payload[5:21]
                try:
                    name = bytes(name_bytes).decode("ascii").rstrip("\x00 ")
                except UnicodeDecodeError:
                    name = ""
                results.append({
                    "filename": path.name,
                    "preset_num": preset_num,
                    "rom_id": rom_id,
                    "preset_name": name,
                    "layer_index": -1,  # name doesn't have layer info
                    "filter_type_id": -1,  # name doesn't have filter info
                    "message_type": "name"
                })

    # --- Filter Parameters Dump (0x10 0x22) ---
    if cmd == 0x10 and subcmd == 0x22:
        payload = data[7:-1]  # strip F0 header + F7
        # Expected: 14 bytes = 7×14-bit values (LSB first pairs)
        if len(payload) == 14:
            # Decode 7×14-bit values
            filter_params = []
            for i in range(0, 14, 2):
                lsb = payload[i]
                msb = payload[i + 1]
                value = lsb + (msb << 7)  # 14-bit value
                filter_params.append(value)
            
            # First parameter is filter_type_id
            filter_type_id = filter_params[0]
            
            # For filter dumps, we need to extract preset/layer info from context
            # This would typically come from the request that generated this response
            # For now, we'll mark it as unknown and let the user correlate
            results.append({
                "filename": path.name,
                "preset_num": -1,  # unknown from filter dump alone
                "rom_id": -1,  # unknown from filter dump alone
                "preset_name": "",
                "layer_index": -1,  # unknown from filter dump alone
                "filter_type_id": filter_type_id,
                "message_type": "filter_params",
                "filter_cutoff": filter_params[1] if len(filter_params) > 1 else -1,
                "filter_resonance": filter_params[2] if len(filter_params) > 2 else -1,
                "filter_params": filter_params
            })
        else:
            print(f"[WARN] {path} filter params payload wrong length: {len(payload)} bytes (expected 14)")

    return results


def merge_names_with_headers(rows):
    """
    Merge preset names from 0x0B messages into header rows.
    Returns a list where each preset has its name if available.
    """
    # Group by (preset_num, rom_id) to merge names
    preset_dict = defaultdict(lambda: {
        "filename": "", 
        "preset_num": 0, 
        "rom_id": 0, 
        "preset_name": "",
        "layer_index": -1,
        "filter_type_id": -1,
        "message_type": "header"
    })
    
    for row in rows:
        key = (row["preset_num"], row["rom_id"])
        
        # If this row has a name, use it
        if row["preset_name"]:
            preset_dict[key]["preset_name"] = row["preset_name"]
        
        # Always update the other fields
        preset_dict[key]["filename"] = row["filename"]
        preset_dict[key]["preset_num"] = row["preset_num"]
        preset_dict[key]["rom_id"] = row["rom_id"]
        preset_dict[key]["layer_index"] = row.get("layer_index", -1)
        preset_dict[key]["filter_type_id"] = row.get("filter_type_id", -1)
        preset_dict[key]["message_type"] = row.get("message_type", "header")
    
    # Convert back to list
    return list(preset_dict.values())


def main():
    if len(sys.argv) < 3:
        print("Usage: python parse_emu_sysex.py <out.csv> <files.syx...>")
        print("Example: python parse_emu_sysex.py emu_presets.csv *.syx")
        sys.exit(1)

    out_csv = Path(sys.argv[1])
    input_files = sys.argv[2:]
    
    print(f"Parsing {len(input_files)} SysEx files...")
    
    all_rows = []
    for file_path in input_files:
        path = Path(file_path)
        if not path.exists():
            print(f"[WARN] File not found: {path}")
            continue
            
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
    fieldnames = ["filename", "preset_num", "rom_id", "preset_name", "layer_index", "filter_type_id", "message_type"]
    with out_csv.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fieldnames)
        w.writeheader()
        w.writerows(merged_rows)

    print(f"\n[SUCCESS] Wrote {len(merged_rows)} presets → {out_csv}")
    
    # Show summary
    rom_counts = defaultdict(int)
    for row in merged_rows:
        rom_counts[row["rom_id"]] += 1
    
    print("\nROM ID Summary:")
    for rom_id, count in sorted(rom_counts.items()):
        print(f"  ROM {rom_id:04X}: {count} presets")


if __name__ == "__main__":
    main()
