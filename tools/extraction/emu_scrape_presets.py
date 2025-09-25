#!/usr/bin/env python3
from __future__ import annotations
import csv, argparse, re
from collections import defaultdict
from pathlib import Path
from syx_tools import split_sysex_file, is_emu_proteus, emu_fields, midi_u14

# Commands/subcommands we care about (from the spec):
CMD_PRESET_DUMP = 0x10         # Preset Dump family
SUB_HDR_CLOSED  = 0x01         # Preset Dump Header (closed)
SUB_HDR_OPEN    = 0x03         # Preset Dump Header (open)
SUB_LAYER_GEN   = 0x21         # Preset Layer General Params Dump
SUB_LAYER_FILT  = 0x22         # Preset Layer Filter Params Dump
SUB_PRESET_ARP  = 0x12         # Preset Common Arp Params Dump
CMD_GENERIC_NAME= 0x0B         # Generic Name (object naming)

# Simple keyword filter for categories
DEFAULT_NAME_FILTER = re.compile(r"\b(pad|bpm|arp|arpe?g|synth)\b", re.IGNORECASE)

def u16(lsb, msb):
    return (lsb & 0x7F) | ((msb & 0x7F) << 7)

def decode_preset_header(payload: bytes):
    """
    Preset Dump Header payload layout per spec:
    <nn,nn> preset number
    <xxxx>  data byte count (4 bytes)
    <aa,aa> # preset common general
    <bb,bb> # reserved
    <cc,cc> # preset common FX
    <dd,dd> # preset common links
    <ee,ee> # number of layers
    <ff,ff> # # layer general params
    <gg,gg> # # layer filter params
    <hh,hh> # # layer LFO
    <ii,ii> # # layer env
    <jj,jj> # # layer cords
    <kk,kk> Preset ROM ID
    We only need preset number and ROM ID here.
    """
    # 2 (nn) + 4 (xxxx) + 2*10 (aa..jj) + 2 (kk) = 28 bytes minimum
    if len(payload) < 2 + 4 + (2*10) + 2:
        return None
    p = memoryview(payload)
    pn   = u16(p[0], p[1])
    # skip 4-byte data count + TEN pairs (aa..jj per spec)
    off  = 2 + 4 + (2*10)  # per spec: aa..jj = TEN pairs; kk,kk at end
    romL = p[off]; romH = p[off+1]
    rom  = u16(romL, romH)
    return pn, int(rom)

def decode_filter_params(payload: bytes):
    # Exactly 14 data bytes => seven 14-bit values (unsigned).
    # Per practice: vals[0] = filter_type_id; others are scalar params.
    if len(payload) != 14: return None
    vals=[]
    for i in range(0,14,2):
        vals.append(midi_u14(payload[i], payload[i+1]))
    return vals

def decode_generic_name(payload: bytes):
    """
    Generic Name: 0x0B tt, <xx,xx> object#, <yy,yy> ROM id, <16 chars>
    We care about tt=1 (Preset). Returns (obj_type, obj_num, rom_id, name).
    """
    if len(payload) < 1 + 2 + 2 + 1:
        return None
    tt = payload[0]
    obj  = u16(payload[1], payload[2])
    rom  = u16(payload[3], payload[4])
    name = bytes(payload[5:5+16]).decode('ascii', errors='ignore').rstrip('\x00 ').strip()
    return tt, obj, rom, name

def main():
    ap = argparse.ArgumentParser(description="Scrape EMU Preset/Layer/Filter from SysEx")
    ap.add_argument("syx", help="Path to .syx file")
    ap.add_argument("--out-csv", default="emu_presets_scraped.csv")
    ap.add_argument("--allow-rom", type=lambda s:int(s,0), nargs="*", default=[],
                    help="ROM IDs to include (e.g. --allow-rom 0x0002 0x0005)")
    ap.add_argument("--only-audity-xl1", action="store_true",
                    help="Shortcut: include only Audity 2000 and Xtreme Lead-1 ROMs (you can also pass explicit --allow-rom)")
    ap.add_argument("--name-filter", default=None, help="Regex to include preset names (default: pad|bpm|arp|synth)")
    args = ap.parse_args()

    # If you know the ROM IDs for Audity2000 and Xtreme Lead-1, place them here:
    # NOTE: These IDs come from the Preset Dump Header's <kk,kk> and Generic Name's <yy,yy>.
    # If unknown, the script will still work; you can log observed ROM IDs and re-run.
    AUDITY_ROM_IDS = { }        # fill once observed (e.g., {0x0002})
    XLEAD1_ROM_IDS = { }        # fill once observed

    allowed_roms = set(args.allow_rom)
    if args.only_audity_xl1:
        allowed_roms |= AUDITY_ROM_IDS | XLEAD1_ROM_IDS

    name_re = re.compile(args.name_filter, re.IGNORECASE) if args.name_filter else DEFAULT_NAME_FILTER

    msgs = split_sysex_file(args.syx)

    # State keyed by (preset_num, rom_id)
    presets = defaultdict(lambda: {
        "name": None,
        "rom_id": None,
        "arp_block": False,
        "layers": []  # each: {"layer_index": i, "filter_type_id": ftid, "filter_params": [..]}
    })

    current_preset = None

    layer_seq = 0  # fall-back layer sequencing if layer index isn't explicit

    for m in msgs:
        if not is_emu_proteus(m):
            continue
        dev, cmd, sub, payload = emu_fields(m)

        # Track preset context
        if cmd == CMD_PRESET_DUMP and sub in (SUB_HDR_CLOSED, SUB_HDR_OPEN):
            decoded = decode_preset_header(payload)
            if decoded:
                # If we were in the middle of another preset, we implicitly finish it
                # by just switching contexts; defaultdict has already accumulated it.
                pn, rom = decoded
                current_preset = (pn, rom)
                # ensure record
                presets[current_preset]["rom_id"] = rom
                layer_seq = 0
                print(f"[hdr] preset={pn} rom=0x{rom:04X}")

        elif cmd == CMD_PRESET_DUMP and sub == SUB_PRESET_ARP:
            if current_preset:
                presets[current_preset]["arp_block"] = True

        elif cmd == CMD_PRESET_DUMP and sub == SUB_LAYER_GEN:
            # We don't need to decode all general params yet; keep layer index sequence
            # A Layer General block indicates a new layer context
            if current_preset:
                # Start a new layer record (filter gets filled if/when 0x22 arrives)
                presets[current_preset]["layers"].append({
                    "layer_index": layer_seq,
                    "filter_type_id": None,
                    "filter_params": None
                })
                layer_seq += 1

        elif cmd == CMD_PRESET_DUMP and sub == SUB_LAYER_FILT:
            vals = decode_filter_params(payload)
            if vals and current_preset:
                # Ensure a layer exists; if not, create one with running index
                if not presets[current_preset]["layers"]:
                    presets[current_preset]["layers"].append({
                        "layer_index": layer_seq,
                        "filter_type_id": None,
                        "filter_params": None
                    })
                    layer_seq += 1
                # attach to the most recent (or create a new one if you prefer 1:1)
                L = presets[current_preset]["layers"][-1]
                L["filter_type_id"] = int(vals[0])
                L["filter_params"]  = list(map(int, vals[1:]))

        elif cmd == CMD_GENERIC_NAME:
            g = decode_generic_name(payload)
            if g and g[0] == 1:  # tt==1 => Preset
                _, obj_num, rom, name = g
                key = (obj_num, rom)
                presets[key]["name"] = name
                presets[key]["rom_id"] = rom

    # Flatten & filter
    rows=[]
    for (pn, rom), rec in presets.items():
        name = rec["name"] or ""
        # Filter by ROM if user asked
        if allowed_roms and (rom not in allowed_roms):
            continue
        # Filter by name keywords
        if name and not name_re.search(name):
            continue
        # Emit one row per layer (so we don't lose multi-layer info)
        if not rec["layers"]:
            rows.append({
                "preset_num": pn,
                "rom_id": rom,
                "preset_name": name,
                "arp_present": int(rec["arp_block"]),
                "layer_index": -1,
                "filter_type_id": -1
            })
        else:
            for L in rec["layers"]:
                rows.append({
                    "preset_num": pn,
                    "rom_id": rom,
                    "preset_name": name,
                    "arp_present": int(rec["arp_block"]),
                    "layer_index": L["layer_index"],
                    "filter_type_id": (L["filter_type_id"] if L["filter_type_id"] is not None else -1)
                })

    outp = Path(args.out_csv)
    outp.parent.mkdir(parents=True, exist_ok=True)
    with outp.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["preset_num","rom_id","preset_name","arp_present","layer_index","filter_type_id"])
        w.writeheader()
        for r in sorted(rows, key=lambda r:(r["rom_id"], r["preset_num"], r["layer_index"])):
            w.writerow(r)

    # ROM ID histogram for analysis
    from collections import Counter
    rom_histogram = Counter(r["rom_id"] for r in rows)
    print(f"[+] Parsed {len(msgs)} SysEx messages; collected {len(presets)} presets; wrote {outp}")
    print(f"[+] ROM ID histogram: {dict(rom_histogram)}")

    # Filter type histogram if available
    filter_types = [r["filter_type_id"] for r in rows if r["filter_type_id"] != -1]
    if filter_types:
        filter_histogram = Counter(filter_types)
        print(f"[+] Filter type IDs found: {sorted(set(filter_types))}")
        print(f"[+] Filter histogram: {dict(filter_histogram)}")

if __name__ == "__main__":
    main()