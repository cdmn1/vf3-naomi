#!/usr/bin/env python3
"""Byte-exact inventory for the two uploaded VF3 motion assets.

This deliberately reports raw structure without assigning undocumented meanings
to fields. Expected sizes and hashes pin the files to the supplied originals.
"""
from pathlib import Path
import hashlib
import collections
import string
import struct
import sys

EXPECTED = {
    "MOTHEAD.BIN": (168391, "015248c733ee7fb55e13f77cfe39ccab20496c6e9b6017e1d75ff1ce2b7983b6"),
    "MTJACLAU.BIN": (1493636, "e59afe305a056c5473b1ce5a304e56ca386fb4617f242c60e755d26ad8a77bfc"),
}

def printable_runs(data, minimum=6, limit=80):
    out, run = [], bytearray()
    for value in data + b"\0":
        if 32 <= value < 127:
            run.append(value)
        else:
            if len(run) >= minimum:
                out.append(run.decode("ascii", "replace"))
            run.clear()
    return out[:limit]

def inspect(path):
    data = path.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    expected_size, expected_hash = EXPECTED[path.name]
    if len(data) != expected_size:
        raise SystemExit(f"{path.name}: size mismatch {len(data)} != {expected_size}")
    if digest != expected_hash:
        raise SystemExit(f"{path.name}: SHA256 mismatch {digest} != {expected_hash}")

    lines = [
        f"FILE: {path.name}",
        f"SIZE: {len(data)} bytes (verified)",
        f"SHA256: {digest} (verified)",
        f"UNIQUE BYTE VALUES: {len(set(data))}/256",
        f"ZERO BYTES: {data.count(0)}",
        f"FIRST 256 BYTES: {data[:256].hex(' ')}",
        "PRINTABLE ASCII RUNS (first 80, minimum 6 chars):",
    ]
    lines.extend(f"  {s}" for s in printable_runs(data))
    for width, fmt in ((2, "H"), (4, "I")):
        count = len(data) // width
        vals = struct.unpack("<" + fmt * count, data[:count * width])
        common = collections.Counter(vals).most_common(16)
        lines.append(f"COMMON LE U{width*8} WORDS: " + ", ".join(f"0x{v:0{width*2}X}:{n}" for v,n in common))
    return lines

def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1 else "motion-assets")
    report = ["VF3 NAOMI MOTION ASSET BYTE AUDIT", "Integrity is checked against the original asset manifest.", "No undocumented field semantics are inferred.", ""]
    for name in EXPECTED:
        report.extend(inspect(root / name))
        report.append("")
    text = "\n".join(report) + "\n"
    (root / "MOTION_ASSET_AUDIT.txt").write_text(text, encoding="utf-8")
    print(text, end="")

if __name__ == "__main__":
    main()
