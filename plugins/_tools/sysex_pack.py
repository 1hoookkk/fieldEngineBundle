#!/usr/bin/env python3
"""
Internal: Convert SysEx dumps to a compact pack for Z‑plane DSP.

Usage:
  Single file:
    python plugins/_tools/sysex_pack.py input.syx --out plugins/_packs/zplane_pack.bin
  Directory (multiple files):
    python plugins/_tools/sysex_pack.py --in-dir "C:/fieldEngineBundle/rich data" --out-dir plugins/_packs --spec plugins/_tools/specs/proteus_v22.json

Notes:
- Pure stdlib; vendor-specific decode hooks are TODO.
- Keep outputs under plugins/ to avoid touching legacy paths.
- Generates ZMF1 entries for 0x22 messages when models.json is available.
"""
import argparse
import json
import struct
from pathlib import Path
import sys
import math
import cmath

MAGIC = b'ZPK1'

def read_syx(path: Path) -> bytes:
    data = path.read_bytes()
    return data

def extract_frames(raw: bytes) -> list[bytes]:
    frames = []
    i = 0
    while i < len(raw):
        if raw[i] == 0xF0:
            j = i + 1
            while j < len(raw) and raw[j] != 0xF7:
                j += 1
            if j < len(raw) and raw[j] == 0xF7:
                frames.append(raw[i:j+1])
                i = j + 1
                continue
        i += 1
    return frames

def unpack7(block: bytes) -> bytes:
    out = bytearray()
    i = 0
    while i < len(block):
        chunk = block[i:i+8]
        if len(chunk) < 2:
            break
        msb = chunk[0]
        for k, b in enumerate(chunk[1:]):
            out.append(b | (((msb >> k) & 1) << 7))
        i += 8
    return bytes(out)

def _proteus_parse(payload: bytes, spec: dict) -> dict | None:
    # Expect payload starting with manufacturer, family, devId, editor, command
    mids = bytes(spec.get('manufacturerId', []))
    fam = spec.get('familyId')
    editor = spec.get('editor')
    if not mids or fam is None or editor is None:
        return None
    if len(payload) < len(mids) + 4:
        return None
    if not payload.startswith(mids):
        return None
    off = len(mids)
    family = payload[off]
    dev_id = payload[off+1]
    ed = payload[off+2]
    cmd = payload[off+3]
    data = payload[off+4:]
    sub = None
    if len(data) >= 1 and cmd in (0x10, 0x11, 0x18, 0x1A, 0x1B, 0x61):
        sub = data[0]
    if family != fam or ed != editor:
        return None
    # checksum check (optional)
    chkspec = spec.get('checksum', {})
    if chkspec.get('type') == 'ones_complement_7bit' and len(data) >= 1:
        exp = data[-1] & 0x7F
        if exp != 0x7F:  # 0x7F can mean ignore
            s = sum(b & 0x7F for b in data[:-1]) & 0x7F
            chk = (~s) & 0x7F
            if chk != exp:
                # Keep but flag invalid checksum
                return {
                    "family": family,
                    "dev": dev_id,
                    "cmd": cmd,
                    "sub": sub,
                    "validChecksum": False,
                    "data": list(data)
                }
    return {
        "family": family,
        "dev": dev_id,
        "cmd": cmd,
        "sub": sub,
        "validChecksum": True,
        "data": list(data)
    }

def canonize(frames: list[bytes], spec: dict | None = None) -> list[dict]:
    arr: list[dict] = []
    for idx, f in enumerate(frames):
        payload = f[1:-1] if len(f) >= 2 else b''
        if spec and spec.get('familyId') is not None:
            meta = _proteus_parse(payload, spec)
            if not meta:
                continue
            arr.append({
                "id": idx,
                "type": int(meta["cmd"]),
                "flags": 0,
                "meta": {
                    k: v for k, v in meta.items() if k != 'data'
                },
                "data": meta["data"]
            })
        else:
            arr.append({
                "id": idx,
                "type": 0,
                "flags": 0,
                "data": list(payload)
            })
    return arr

def assemble_preset(entries: list[dict]) -> list[dict]:
    """Reassemble Preset data packets (cmd 0x10, sub 0x02 or 0x04) into a single blob per preset.
    Assumes first two bytes of data encode a 14-bit seq number: seq = b0 | (b1<<7). Last byte is checksum.
    """
    from collections import defaultdict
    groups = defaultdict(dict)  # key: headerIndex or 0, value: {seq: bytes}
    headers = []
    for e in entries:
        t = int(e.get('type', 0))
        meta = e.get('meta') or {}
        sub = int(meta.get('sub') or 0)
        data = bytes(e.get('data', []))
        if t == 0x10 and sub in (0x02, 0x04) and len(data) >= 3:
            seq = data[0] | (data[1] << 7)
            payload = data[2:-1] if len(data) >= 3 else b''  # drop checksum
            groups[0][seq] = payload
        elif t == 0x10 and sub in (0x01, 0x03):
            headers.append(e)

    assembled = bytearray()
    if groups:
        parts = groups[0]
        for i in sorted(parts.keys()):
            assembled += parts[i]
        if assembled:
            entries.append({
                "id": max([e.get('id', 0) for e in entries] + [0]) + 1,
                "type": 0x10,
                "flags": 0,
                "meta": {"sub": 0xFE, "assembled": True},
                "data": list(assembled)
            })
    return entries

def _parse_preset_header_blob(blob: bytes) -> dict | None:
    # Expect sub at byte0, preset LSB/MSB at 1..2, dataBytes (u32) at 3..6, then 10 counts.
    if len(blob) < 1 + 2 + 4 + 10:
        return None
    sub = blob[0]
    preset = blob[1] | (blob[2] << 8)
    data_bytes = blob[3] | (blob[4] << 8) | (blob[5] << 16) | (blob[6] << 24)
    counts = list(blob[7:17])
    return {
        'sub': sub,
        'preset': preset,
        'data_bytes': data_bytes,
        'counts': {
            'PCGen': counts[0], 'Reserved': counts[1], 'PCEfx': counts[2], 'PCLinks': counts[3],
            'Layers': counts[4], 'LayGen': counts[5], 'LayFilt': counts[6], 'LayLFO': counts[7], 'LayEnv': counts[8], 'LayCords': counts[9]
        }
    }

# ============= ZMF1 Generation Functions =============

def load_zplane_models() -> dict | None:
    """Load Z-plane models from shared models.json if available."""
    models_path = Path(__file__).parent.parent / "_shared" / "zplane" / "models.json"
    if not models_path.exists():
        return None
    try:
        with models_path.open('r') as f:
            return json.load(f)
    except:
        return None

def poles_to_biquad_coeffs(r: float, theta: float, sample_rate: float = 48000.0) -> tuple:
    """Convert a pole pair (r, theta) to biquad coefficients (b0, b1, b2, a1, a2).

    Uses EMU Z-plane style bandpass characteristics with proper gain compensation.
    """
    # EMU-style pole to bandpass conversion (matches BiquadCascade.h)
    a1 = -2.0 * r * math.cos(theta)
    a2 = r * r

    # EMU bandpass: gain compensation and zeros at DC & Nyquist
    b0 = (1.0 - r) * 0.5
    b1 = 0.0
    b2 = -b0  # Creates zeros at DC and Nyquist for bandpass character

    # Clamp for stability (match EMU implementation)
    a1 = max(-1.999, min(1.999, a1))
    a2 = max(-0.999, min(0.999, a2))

    return (b0, b1, b2, a1, a2)

def interpolate_poles(poleA: dict, poleB: dict, t: float) -> dict:
    """Interpolate between two poles using shortest path on unit circle."""
    rA, thetaA = poleA['r'], poleA['theta']
    rB, thetaB = poleB['r'], poleB['theta']

    # Interpolate radius linearly
    r = rA + t * (rB - rA)

    # Interpolate angle via shortest path
    diff = thetaB - thetaA
    # Wrap to [-pi, pi]
    while diff > math.pi:
        diff -= 2 * math.pi
    while diff < -math.pi:
        diff += 2 * math.pi

    theta = thetaA + t * diff

    return {'r': r, 'theta': theta}

def generate_zmf1_entry(model_id: int, model_data: dict, num_frames: int = 11) -> bytes:
    """Generate a ZMF1 binary entry for a Z-plane model."""
    # ZMF1 Header (16 bytes)
    header = bytearray()
    header += b'ZMF1'                          # Magic (4 bytes)
    header += struct.pack('<H', 1)             # Version (2 bytes)
    header += struct.pack('<H', model_id)      # Model ID (2 bytes)
    header += struct.pack('B', num_frames)     # Number of frames (1 byte)
    header += struct.pack('B', 6)              # Number of sections (1 byte)
    header += struct.pack('<I', 48000)         # Sample rate reference (4 bytes)
    header += b'\x00\x00'                      # Reserved (2 bytes)

    # Generate coefficient frames
    frames = bytearray()
    frameA_poles = model_data['frameA']['poles']
    frameB_poles = model_data['frameB']['poles']

    for frame_idx in range(num_frames):
        t = frame_idx / (num_frames - 1) if num_frames > 1 else 0.0

        # Interpolate and generate coefficients for each section
        for section_idx in range(6):
            poleA = frameA_poles[section_idx]
            poleB = frameB_poles[section_idx]
            pole_interp = interpolate_poles(poleA, poleB, t)

            b0, b1, b2, a1, a2 = poles_to_biquad_coeffs(pole_interp['r'], pole_interp['theta'])

            # Pack as 5 floats (20 bytes per section)
            frames += struct.pack('<fffff', b0, b1, b2, a1, a2)

    return bytes(header + frames)

def augment_with_zmf1(entries: list[dict]) -> list[dict]:
    """Add ZMF1 entries for any 0x22 messages if models are available."""
    models = load_zplane_models()
    if not models:
        return entries

    # Map model names to IDs
    model_map = {
        'vowel': 0,
        'bell': 1,
        'low': 2
    }

    augmented = list(entries)
    next_id = max([e.get('id', 0) for e in entries] + [0]) + 1

    for entry in entries:
        if entry.get('type') == 0x10 and entry.get('meta', {}).get('sub') == 0x22:
            # Parse the 0x22 data to determine which model to use
            data = entry.get('data', [])
            if len(data) >= 14:
                # Simple heuristic: use first byte modulo 3 to select model
                # In production, you'd parse the actual EMU parameters
                model_idx = data[0] % 3
                model_name = ['vowel_pair', 'bell_pair', 'low_pair'][model_idx]

                if model_name in models['models']:
                    model_data = models['models'][model_name]
                    zmf1_data = generate_zmf1_entry(model_idx, model_data)

                    # Add ZMF1 entry
                    augmented.append({
                        'id': next_id,
                        'type': 0x5A4D,  # 'ZM' in hex
                        'flags': 0x4631,  # 'F1' in hex
                        'meta': {
                            'zmf1': True,
                            'model': model_name,
                            'source_id': entry.get('id')
                        },
                        'data': list(zmf1_data)
                    })
                    next_id += 1

    return augmented

def segment_assembled(entries: list[dict]) -> list[dict]:
    # Heuristic: if header present (type 0x10 sub 0x01) and assembled present (type 0x10 sub 0xFE),
    # carve tail as [LayFilt * 14][LayEnv * 92] from end of assembled blob and emit 0x22 entries.
    header = next((e for e in entries if int(e.get('type', 0)) == 0x10 and (e.get('meta') or {}).get('sub') == 0x01), None)
    assembled = next((e for e in entries if int(e.get('type', 0)) == 0x10 and (e.get('meta') or {}).get('sub') == 0xFE), None)
    if not header or not assembled:
        return entries
    hdr_data = bytes(header.get('data', []))
    asm_data = bytes(assembled.get('data', []))
    hdr = _parse_preset_header_blob(hdr_data)
    if not hdr:
        return entries
    lay_filt = int(hdr['counts'].get('LayFilt', 0))
    lay_env = int(hdr['counts'].get('LayEnv', 0))
    need = lay_filt * 14 + lay_env * 92
    if need == 0 or need > len(asm_data):
        return entries
    # Take from tail: [filters][envs]
    tail = asm_data[-need:]
    filt_blob = tail[: lay_filt * 14]
    # env_blob = tail[lay_filt * 14 : ]  # currently unused
    next_id = max([e.get('id', 0) for e in entries] + [0]) + 1
    for i in range(lay_filt):
        chunk = filt_blob[i*14:(i+1)*14]
        entries.append({
            'id': next_id,
            'type': 0x10,
            'flags': 0,
            'meta': {'sub': 0x22, 'heuristic': True, 'idx': i},
            'data': list(chunk)
        })
        next_id += 1
    return entries

def write_sidecar_json(sidecar_path: Path, entries: list[dict]) -> None:
    import json as _json
    out = []
    for e in entries:
        rec = {
            'id': int(e.get('id', 0)),
            'type': int(e.get('type', 0)),
            'sub': int((e.get('meta') or {}).get('sub') or 0),
            'flags': int(e.get('flags', 0)),
            'length': len(e.get('data', [])),
        }
        if rec['type'] == 0x10 and rec['sub'] == 0x22 and rec['length'] == 14:
            rec['bytes'] = list(e.get('data', []))
        out.append(rec)
    sidecar_path.write_text(_json.dumps({'count': len(out), 'entries': out}, indent=2))

def pack(entries: list[dict]) -> bytes:
    """
    ZPK1 v1 layout:
      header: magic(4) version(u16) count(u16)
      table[count]: id(u32) type(u16) sub(u16) flags(u16) offset(u32) length(u32)
      blobs: concatenated data
    """
    header = bytearray()
    header += MAGIC
    header += struct.pack('<H', 1)  # version
    header += struct.pack('<H', len(entries))
    # Build table
    table = bytearray()
    blobs = bytearray()
    entry_size = 4 + 2 + 2 + 2 + 4 + 4
    offset = 4 + 2 + 2 + len(entries) * entry_size
    for e in entries:
        data = bytes(e.get('data', b''))
        etype = int(e.get('type', 0))
        sub = int((e.get('meta', {}) or {}).get('sub', 0))
        flags = int(e.get('flags', 0))
        table += struct.pack('<IHHHII', int(e.get('id', 0)), etype, sub, flags, offset, len(data))
        blobs += data
        offset += len(data)
    return bytes(header + table + blobs)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('input', nargs='?', type=Path)
    ap.add_argument('--out', type=Path)
    ap.add_argument('--in-dir', type=Path)
    ap.add_argument('--out-dir', type=Path)
    ap.add_argument('--glob', type=str, default='**/*.syx')
    ap.add_argument('--spec', type=Path)
    args = ap.parse_args()

    spec = None
    if args.spec:
        spec = json.loads(args.spec.read_text())

    if args.in_dir:
        in_dir = args.in_dir
        out_dir = args.out_dir or Path('plugins/_packs')
        out_dir.mkdir(parents=True, exist_ok=True)
        count = 0
        for p in sorted(in_dir.rglob(args.glob)):
            if p.is_file():
                raw = read_syx(p)
                frames = extract_frames(raw)
                entries = canonize(frames, spec)
                # Assemble preset data blobs for easier downstream parsing
                entries = assemble_preset(entries)
                entries = segment_assembled(entries)
                entries = augment_with_zmf1(entries)
                packed = pack(entries)
                out_path = out_dir / (p.stem + '.bin')
                out_path.write_bytes(packed)
                # Sidecar JSON summary next to pack
                try:
                    write_sidecar_json(out_path.with_suffix('.json'), entries)
                except Exception as ex:
                    print(f"[warn] failed sidecar for {out_path}: {ex}")
                print(f"Packed {p} -> {out_path} ({len(packed)} bytes)")
                count += 1
        print(f"Done. {count} file(s) processed to {out_dir}")
        return 0

    if not args.input or not args.out:
        print('Specify input/out or use --in-dir/--out-dir', file=sys.stderr)
        return 2

    raw = read_syx(args.input)
    frames = extract_frames(raw)
    entries = canonize(frames, spec)
    entries = assemble_preset(entries)
    entries = segment_assembled(entries)
    entries = augment_with_zmf1(entries)
    out = pack(entries)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(out)
    try:
        write_sidecar_json(args.out.with_suffix('.json'), entries)
    except Exception as ex:
        print(f"[warn] failed sidecar for {args.out}: {ex}")
    print(f"Wrote {args.out} ({len(out)} bytes)")

if __name__ == '__main__':
    main()
