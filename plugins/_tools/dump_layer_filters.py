#!/usr/bin/env python3
import argparse, json, struct
from pathlib import Path

def scan_pack(path: Path):
    b = path.read_bytes()
    if b[:4] != b'ZPK1':
        return []
    ver, count = struct.unpack_from('<HH', b, 4)
    if ver != 1:
        return []
    esz = 4+2+2+2+4+4
    out = []
    for i in range(count):
        id_, typ, sub, flags, offd, ln = struct.unpack_from('<IHHHII', b, 8 + i*esz)
        if typ == 0x10 and sub == 0x22 and ln == 14:
            data = b[offd:offd+ln]
            out.append({
                "pack": str(path),
                "id": id_,
                "type": typ,
                "sub": sub,
                "bytes": list(data),
                "decode": {
                    "filterType": data[0],
                    "cutoff": data[1],
                    "q": data[2],
                    "morphIndex": data[3],
                    "morphDepth": data[4],
                    "tilt": (data[5] if data[5] < 128 else data[5]-256),
                    "reserved": list(data[6:14])
                }
            })
    return out

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dir', type=Path, required=True)
    ap.add_argument('--out', type=Path, default=Path('plugins/_packs/layer_filters_sample.json'))
    ap.add_argument('--limit', type=int, default=20)
    args = ap.parse_args()

    samples = []
    for p in sorted(args.dir.rglob('*.bin')):
        samples.extend(scan_pack(p))
        if len(samples) >= args.limit:
            break

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps({
        "count": len(samples),
        "items": samples
    }, indent=2))
    print(f"Wrote {args.out} with {len(samples)} 0x22 entries")

if __name__ == '__main__':
    main()

