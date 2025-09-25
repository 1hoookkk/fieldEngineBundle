#!/usr/bin/env python3
from __future__ import annotations
import sys
import re
from pathlib import Path

KEY_TERMS = [
    r"Preset Dump Header",
    r"Generic Name",
    r"Generic Name Request",
    r"ROM ID",
    r"0x0B",
    r"0x0C",
    r"0Bh",
    r"0Ch",
    r"Command\s+0x10",
    r"Command\s+10[hH]?",
    r"Preset Layer",
    r"Filter Params",
    r"0x22",
    r"0x21",
]

CONTEXT = 240


def extract_text(pdf_path: Path) -> str:
    from pdfminer.high_level import extract_text as _extract_text
    return _extract_text(str(pdf_path))


def print_context(text: str, pattern: str):
    print(f"\n=== Matches for /{pattern}/ ===")
    try:
        rgx = re.compile(pattern)
    except re.error as e:
        print(f"[bad regex] {e}")
        return
    found = False
    for m in rgx.finditer(text):
        found = True
        s = max(0, m.start() - CONTEXT)
        e = min(len(text), m.end() + CONTEXT)
        snippet = text[s:e].replace('\r', '').replace('\t', ' ')
        print("..." + snippet + "...")
    if not found:
        print("(no matches)")


def main():
    if len(sys.argv) != 2:
        print("Usage: spec_scan.py 'Proteus Family Sysex 2.2.pdf'")
        sys.exit(2)
    pdf_path = Path(sys.argv[1])
    if not pdf_path.exists():
        print(f"No such file: {pdf_path}")
        sys.exit(1)
    text = extract_text(pdf_path)
    print(f"[info] Extracted {len(text)} characters from {pdf_path.name}")
    for term in KEY_TERMS:
        print_context(text, term)

if __name__ == '__main__':
    main()
