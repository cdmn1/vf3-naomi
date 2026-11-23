#!/usr/bin/env python3
"""Recover the previously watched 512-byte fighter actor regions from trace CSVs.

This does not assume a savestate exists. It collects only memory events already
present in the retained v71/v72 Flycast traces and reports observed actor words.
"""
from __future__ import annotations
import argparse, csv
from collections import Counter, defaultdict
from pathlib import Path

ACTORS = (0x0C1FEFE4, 0x0C20138C)
ACTOR_BYTES = 0x200
SELECTOR_OFFSET = 0x6C
SELECTOR_COUNT = 18
EXTRA_WINDOWS = {
    "watch-A +0x54 pointer 0x0C03006C": (0x0C03006C, 0x100),
    "watch-B +0x54 pointer 0x0C030D6C": (0x0C030D6C, 0x100),
    "watch-A +0x58 pointer 0x0C10E548": (0x0C10E548, 0x100),
    "watch-B +0x58 pointer 0x0C10EB60": (0x0C10EB60, 0x100),
}

def norm(x: int) -> int:
    return x & 0x1FFFFFFF

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("memory_csv", type=Path)
    ap.add_argument("--version", required=True)
    args = ap.parse_args()

    bases = {norm(x): x for x in ACTORS}
    events = {base: defaultdict(list) for base in bases}
    extra = {name: (norm(base), size, defaultdict(list))
             for name, (base, size) in EXTRA_WINDOWS.items()}
    with args.memory_csv.open(newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            addr = norm(int(row["address"], 0))
            event = {
                "word": int(row["word_index"], 0),
                "access": row["access"],
                "width": int(row["width"], 0),
                "value": int(row["value"], 0),
            }
            matched = False
            for base in bases:
                if base <= addr < base + ACTOR_BYTES:
                    events[base][addr - base].append(event)
                    matched = True
                    break
            if not matched:
                for _name, (base, size, words) in extra.items():
                    if base <= addr < base + size:
                        words[addr - base].append(event)
                        break

    print(f"=== {args.version} RETAINED ACTOR WATCHES ===")
    for base in bases:
        by_offset = events[base]
        print(f"ACTOR 0x{bases[base]:08X}: events={sum(len(v) for v in by_offset.values())} "
              f"unique_addresses={len(by_offset)}")
        if not by_offset:
            print("  NO_EVENTS_IN_512_BYTE_WINDOW")
            continue
        print("  HEADER:")
        for off in (0x00, 0x04, 0x54, 0x58):
            rows = by_offset.get(off, [])
            vals = Counter((r["width"], r["value"]) for r in rows)
            rendered = ", ".join(
                f"w{width}=0x{value:08X} x{count}"
                for (width, value), count in vals.most_common(6)
            )
            print(f"    +0x{off:03X}: {rendered or 'not observed'}")
        print("  SELECTORS +0x6C (18 x 32-bit):")
        for i in range(SELECTOR_COUNT):
            off = SELECTOR_OFFSET + i * 4
            rows = [r for r in by_offset.get(off, []) if r["width"] == 4]
            vals = Counter(r["value"] & 0xFFFFFFFF for r in rows)
            if vals:
                latest = max(rows, key=lambda r: r["word"])
                choices = ",".join(f"0x{v:08X}({n})" for v,n in vals.most_common(4))
                print(f"    [{i:02d}] +0x{off:03X}: last=0x{latest['value'] & 0xFFFFFFFF:08X} "
                      f"values={choices} reads={sum(r['access']=='R' for r in rows)} "
                      f"writes={sum(r['access']=='W' for r in rows)}")
            else:
                print(f"    [{i:02d}] +0x{off:03X}: not observed as 32-bit")
        print("  OBSERVED 32-BIT WORDS (offset: last value / read-write counts):")
        for off in sorted(by_offset):
            rows = [r for r in by_offset[off] if r["width"] == 4]
            if not rows:
                continue
            latest = max(rows, key=lambda r: r["word"])
            reads = sum(r["access"] == "R" for r in rows)
            writes = sum(r["access"] == "W" for r in rows)
            print(f"    +0x{off:03X}: 0x{latest['value'] & 0xFFFFFFFF:08X} "
                  f"(R{reads}/W{writes}, n={len(rows)})")
    print("=== DEREFERENCED +0x54 / +0x58 WINDOWS ===")
    for name, (base, size, words) in extra.items():
        count = sum(len(v) for v in words.values())
        print(f"{name}: base=0x{base:08X} bytes=0x{size:X} "
              f"events={count} unique_addresses={len(words)}")
        for off in sorted(words):
            rows = [r for r in words[off] if r["width"] == 4]
            if not rows:
                continue
            latest = max(rows, key=lambda r: r["word"])
            reads = sum(r["access"] == "R" for r in rows)
            writes = sum(r["access"] == "W" for r in rows)
            vals = Counter(r["value"] & 0xFFFFFFFF for r in rows)
            common = ",".join(f"0x{v:08X}({n})" for v,n in vals.most_common(3))
            print(f"  +0x{off:03X}: last=0x{latest['value'] & 0xFFFFFFFF:08X} "
                  f"R{reads}/W{writes} values={common}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
