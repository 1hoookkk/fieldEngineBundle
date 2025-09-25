#!/usr/bin/env python3
import struct, sys, json
from pathlib import Path

def read_pack(path: Path):
    b=path.read_bytes()
    if b[:4]!=b'ZPK1':
        raise SystemExit('Not a ZPK1 pack')
    ver,count=struct.unpack_from('<HH',b,4)
    esz=4+2+2+2+4+4
    entries=[]
    for i in range(count):
        id_,typ,sub,flags,offd,ln=struct.unpack_from('<IHHHII',b,8+i*esz)
        entries.append((id_,typ,sub,flags,offd,ln))
    return b,entries

def parse_header(data: bytes):
    # Heuristic parse: <preset# (2B?)> <dataBytes 32b> then 10 count bytes
    # The captured header entry we stored already excludes F0...F7 framing and includes sub=0x01 as first data byte.
    # Format seen: [sub][payload...]; We remove sub and last checksum if present in source syx. Here header len is ~29 from index.
    p=0
    sub=data[p]; p+=1
    # next bytes: unknown preset#, we skip 2 bytes
    if len(data)<p+2+4+10:
        return None
    preset_lsb=data[p]; preset_msb=data[p+1]; p+=2
    data_bytes=data[p] | (data[p+1]<<8) | (data[p+2]<<16) | (data[p+3]<<24); p+=4
    counts=list(data[p:p+10]); p+=10
    return {
        'sub': sub,
        'preset': preset_lsb | (preset_msb<<8),
        'data_bytes': data_bytes,
        'counts': {
            '#PCGen': counts[0], '#Reserved': counts[1], '#PCEfx': counts[2], '#PCLinks': counts[3],
            '#Layers': counts[4], '#LayGen': counts[5], '#LayFilt': counts[6], '#LayLFO': counts[7], '#LayEnv': counts[8], '#LayCords': counts[9]
        }
    }

def main():
    p=Path(sys.argv[1])
    b,entries=read_pack(p)
    hdr=[e for e in entries if e[1]==0x10 and e[2]==0x01]
    asm=[e for e in entries if e[1]==0x10 and e[2]==0xFE]
    info={}
    if hdr:
        off,ln=hdr[0][4],hdr[0][5]
        data=b[off:off+ln]
        info['header']=parse_header(data)
    if asm:
        off,ln=asm[0][4],asm[0][5]
        info['assembled_len']=ln
    print(json.dumps(info,indent=2))

if __name__=='__main__':
    main()

