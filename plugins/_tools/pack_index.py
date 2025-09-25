#!/usr/bin/env python3
import json, struct
from pathlib import Path
import argparse

def scan_pack(path: Path):
    b = path.read_bytes()
    if b[:4] != b'ZPK1':
        return None
    ver, count = struct.unpack_from('<HH', b, 4)
    if ver != 1:
        return None
    esz = 4+2+2+2+4+4
    entries = []
    for i in range(count):
        id_, typ, sub, flags, offd, ln = struct.unpack_from('<IHHHII', b, 8 + i*esz)
        entries.append({"id": id_, "type": typ, "sub": sub, "flags": flags, "offset": offd, "length": ln})
    return entries

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dir', type=Path, required=True)
    ap.add_argument('--out', type=Path, default=Path('plugins/_packs/index.json'))
    args = ap.parse_args()
    out = {}
    total=0
    with_sub22=0
    with_assembled=0
    for p in sorted(args.dir.rglob('*.bin')):
        e = scan_pack(p)
        if e is None:
            continue
        total += 1
        has22 = any(x['type']==0x10 and x['sub']==0x22 for x in e)
        hasFE = any(x['type']==0x10 and x['sub']==0xFE for x in e)
        with_sub22 += 1 if has22 else 0
        with_assembled += 1 if hasFE else 0
        out[str(p)] = {
            "entries": e,
            "has_layer_filter_0x22": has22,
            "has_assembled_preset": hasFE,
        }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps({
        "summary": {
            "total_packs": total,
            "with_layer_filter_0x22": with_sub22,
            "with_assembled_preset": with_assembled
        },
        "packs": out
    }, indent=2))
    print(f"Indexed {total} pack(s). Layer filter 0x22 present in {with_sub22}; assembled in {with_assembled}.")

if __name__ == '__main__':
    main()

