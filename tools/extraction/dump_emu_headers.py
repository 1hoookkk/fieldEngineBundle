#!/usr/bin/env python3
from __future__ import annotations
import sys
from pathlib import Path

# Ensure project root is importable when running as a script
ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.extraction.syx_tools import split_sysex_file, is_emu_proteus, emu_fields

CMD_PRESET_DUMP = 0x10
SUB_HDR_CLOSED  = 0x01
SUB_HDR_OPEN    = 0x03

def u16(lsb: int, msb: int) -> int:
    return (lsb & 0x7F) | ((msb & 0x7F) << 7)

def dump_headers(path: Path):
    try:
        msgs = split_sysex_file(str(path))
    except Exception as e:
        print(f"[err] {path}: {e}")
        return
    print(f"\n== {path} ==")
    found = False
    for idx, m in enumerate(msgs):
        if not is_emu_proteus(m):
            continue
        dev, cmd, sub, payload = emu_fields(m)
        if cmd == CMD_PRESET_DUMP and sub in (SUB_HDR_CLOSED, SUB_HDR_OPEN):
            found = True
            off = 2 + 4 + (2*10)
            romL = payload[off] if len(payload) > off else None
            romH = payload[off+1] if len(payload) > off+1 else None
            rom  = u16(romL, romH) if romL is not None and romH is not None else None
            hex_preview = ' '.join(f"{b:02X}" for b in payload[:min(len(payload), 64)])
            rom_bytes = (f"{romL:02X}" if romL is not None else "??", f"{romH:02X}" if romH is not None else "??")
            rom_str = (f"0x{rom:04X}" if rom is not None else "None")
            print(f"[msg {idx}] sub=0x{sub:02X} payload_len={len(payload)} off={off} rom_bytes={rom_bytes[0]} {rom_bytes[1]} rom={rom_str}")
            print(f"        payload[0:64]: {hex_preview}")
    if not found:
        print("[warn] No Preset Dump Header found.")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: dump_emu_headers.py file1.syx [file2.syx ...]")
        sys.exit(2)
    for arg in sys.argv[1:]:
        dump_headers(Path(arg))
