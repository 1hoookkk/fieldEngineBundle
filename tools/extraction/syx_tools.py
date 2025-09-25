# tools/extraction/syx_tools.py
from __future__ import annotations
from pathlib import Path
from typing import Iterable, Tuple, List

F0, F7 = 0xF0, 0xF7

def iter_sysex_messages(blob: bytes) -> Iterable[bytes]:
    i, n = 0, len(blob)
    while i < n:
        while i < n and blob[i] != F0:
            i += 1
        if i >= n: break
        j = i + 1
        while j < n and blob[j] != F7:
            j += 1
        if j >= n: break
        yield blob[i:j+1]
        i = j + 1

def split_sysex_file(path: str | Path) -> List[bytes]:
    return list(iter_sysex_messages(Path(path).read_bytes()))

def is_emu_proteus(msg: bytes) -> bool:
    # F0 18 0F dd 55 ...
    return (
        len(msg) >= 6 and
        msg[0] == 0xF0 and msg[1] == 0x18 and msg[2] == 0x0F
    )

def emu_fields(msg: bytes) -> Tuple[int,int,int,bytes]:
    """
    Return (deviceId, command, subcommand_or_first, payload) for EMU messages.
    Layout: F0 18 0F dd 55 CC [SS] payload... F7
    """
    dev   = msg[3]
    cmd   = msg[5]
    sub   = msg[6] if len(msg) > 6 else -1
    payload = msg[7:-1] if len(msg) > 8 else b""
    return dev, cmd, sub, payload

def midi_u14(lsb: int, msb: int) -> int:
    return (lsb & 0x7F) | ((msb & 0x7F) << 7)

# Legacy functions for backward compatibility
def iter_sysex_messages_legacy(data: bytes):
    """Extract individual SysEx messages from binary data."""
    return iter_sysex_messages(data)

def unpack_7bit(payload: bytes) -> bytes:
    """Unpack 7-bit groups where first byte contains MSBs for next 7 bytes."""
    out = bytearray()
    i = 0
    while i < len(payload):
        if i >= len(payload): break
        msb = payload[i]; i += 1
        for k in range(7):
            if i >= len(payload): break
            b = payload[i] | (((msb >> k) & 1) << 7)
            out.append(b)
            i += 1
    return bytes(out)

def split_and_unpack(path: str):
    """Split SysEx file into messages and unpack 7-bit encoding."""
    raw = Path(path).read_bytes()
    msgs = list(iter_sysex_messages(raw))
    out = []
    for m in msgs:
        # strip F0 ... F7
        core = m[1:-1]
        # If top bit is set in any core bytes, it's likely already unpacked
        if any(b & 0x80 for b in core):
            # likely already unpacked; keep as-is
            out.append(core)
        else:
            # Apply 7-bit unpacking
            out.append(unpack_7bit(core))
    return out

def analyze_sysex_file(path: str) -> dict:
    """Analyze a SysEx file and return basic info."""
    try:
        raw = Path(path).read_bytes()
        msgs = list(iter_sysex_messages(raw))

        info = {
            "file_size": len(raw),
            "num_messages": len(msgs),
            "messages": []
        }

        for i, msg in enumerate(msgs):
            if len(msg) < 3:
                continue

            core = msg[1:-1]  # Strip F0/F7
            msg_info = {
                "index": i,
                "total_length": len(msg),
                "core_length": len(core),
                "manufacturer_id": None,
                "device_id": None,
                "has_7bit_data": not any(b & 0x80 for b in core)
            }

            # Extract manufacturer and device IDs if present
            if len(core) >= 3:
                msg_info["manufacturer_id"] = f"0x{core[0]:02X}"
                msg_info["device_id"] = f"0x{core[1]:02X}"
                msg_info["model_id"] = f"0x{core[2]:02X}"

            info["messages"].append(msg_info)

        return info
    except Exception as e:
        return {"error": str(e)}