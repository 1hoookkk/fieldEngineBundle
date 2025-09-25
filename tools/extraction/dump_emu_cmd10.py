#!/usr/bin/env python3
from __future__ import annotations
import sys
from pathlib import Path
import string

# Ensure project root import
ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.extraction.syx_tools import split_sysex_file, is_emu_proteus, emu_fields

CMD_PRESET_DUMP = 0x10

SUB_NAMES = {
    0x01: "HDR_CLOSED",
    0x03: "HDR_OPEN",
    0x12: "PRESET_ARP",
    0x21: "LAYER_GEN",
    0x22: "LAYER_FILT",
}

PRINTABLE = set(bytes(string.printable, 'ascii'))

def ascii_preview(b: bytes, n: int = 80) -> str:
    s = bytearray()
    for ch in b[:n]:
        s.append(ch if ch in PRINTABLE and ch not in (0x0B, 0x0C) else 0x2E)
    return s.decode('ascii', errors='ignore')

def dump_cmd10(path: Path):
    msgs = split_sysex_file(str(path))
    print(f"\n== {path} ==")
    for i, m in enumerate(msgs):
        if not is_emu_proteus(m):
            continue
        dev, cmd, sub, payload = emu_fields(m)
        if cmd != CMD_PRESET_DUMP:
            continue
        name = SUB_NAMES.get(sub, f"SUB_0x{sub:02X}")
        page = payload[0] if payload else None
        subidx = payload[1] if len(payload) > 1 else None
        print(f"[#{i}] sub={name:<11} len={len(payload):3d} page={page!s:>3} subidx={subidx!s:>3}  first24={' '.join(f'{b:02X}' for b in payload[:24])}")
        if sub not in (0x01, 0x03) and payload:
            print(f"      ascii: {ascii_preview(payload[2:])}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: dump_emu_cmd10.py file.syx [more.syx]")
        sys.exit(2)
    for arg in sys.argv[1:]:
        dump_cmd10(Path(arg))
