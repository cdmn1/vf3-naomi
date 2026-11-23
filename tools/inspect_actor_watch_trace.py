#!/usr/bin/env python3
"""Recover the previously watched fighter actor regions and full pose tables from trace CSVs.

This does not assume a savestate exists. It collects only memory events already
present in the retained v71/v72 Flycast traces and reports observed actor words.
"""
from __future__ import annotations
import argparse, csv, struct
from collections import Counter, defaultdict
from pathlib import Path

ACTORS = (0x0C1FEFE4, 0x0C20138C)
ACTOR_BYTES = 0x600
SELECTOR_OFFSET = 0x6C
SELECTOR_COUNT = 18
EXTRA_WINDOWS = {
    "watch-A aim blocks +0x640/+0x740": (0x0C1FF624, 0x200),
    "watch-B aim blocks +0x640/+0x740": (0x0C2019CC, 0x200),
    "watch-A 23-bone matrices +0x7C0": (0x0C1FF7A4, 0x5C0),
    "watch-B 23-bone matrices +0x7C0": (0x0C201B4C, 0x5C0),
    "watch-A MT channels +0x1D58": (0x0C200D3C, 0xFC),
    "watch-B MT channels +0x1D58": (0x0C2030E4, 0xFC),
    "watch-A pose fields +0x1F50": (0x0C200F34, 0x54),
    "watch-B pose fields +0x1F50": (0x0C2032DC, 0x54),
    "watch-A pose inputs and joint chains +0x208C": (0x0C201070, 0x26C),
    "watch-B pose inputs and joint chains +0x208C": (0x0C203418, 0x26C),
    "watch-A aim selector +0x420B": (0x0C2031EF, 0x10),
    "watch-B aim selector +0x420B": (0x0C205597, 0x10),
    "global pose flags 0x0C29BCCC": (0x0C29BCCC, 0x4),
    "watch-A model resource object": (0x0C03006C, 0x100),
    "watch-B model resource object": (0x0C030D6C, 0x100),
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

    print(f"=== {args.version} RETAINED ACTOR WATCHES (0x{ACTOR_BYTES:X}-byte windows) ===")
    for base in bases:
        by_offset = events[base]
        print(f"ACTOR 0x{bases[base]:08X}: events={sum(len(v) for v in by_offset.values())} "
              f"unique_addresses={len(by_offset)}")
        if not by_offset:
            print("  NO_EVENTS_IN_0x600_BYTE_ACTOR_WINDOW")
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
    def dump_matrix(label: str, base: int, rows_n: int, stride: int,
                    data: dict[int, list[dict]]) -> None:
        print(f"=== {label} ===")
        for row in range(rows_n):
            values = []
            indices = []
            missing = 0
            for col in range(16):
                off = row * stride + col * 4
                samples = [e for e in data.get(off, []) if e["width"] == 4]
                if not samples:
                    values.append("????????")
                    missing += 1
                    continue
                latest = max(samples, key=lambda e: e["word"])
                raw = latest["value"] & 0xFFFFFFFF
                values.append(f"{raw:08X}")
                indices.append(latest["word"])
            span = max(indices) - min(indices) if indices else -1
            print(f"  row {row:02d} base=0x{base + row * stride:08X} "
                  f"missing={missing} last_event_span={span}: " + " ".join(values))

    for actor_base in ACTORS:
        dump_matrix(f"ACTOR 0x{actor_base:08X} ATTACHMENT MATRICES +0x180",
                    actor_base + 0x180, 18, 0x40, events[norm(actor_base)])
    for name, (base, size, data) in extra.items():
        if "23-bone matrices" in name:
            dump_matrix(name, base, 23, 0x40, data)

    print("=== JOINT-CHAIN POSE RECORDS ===")
    for actor_base in ACTORS:
        base = norm(actor_base + 0x208C)
        data = extra["watch-A pose inputs and joint chains +0x208C"][2] if actor_base == ACTORS[0] else extra["watch-B pose inputs and joint chains +0x208C"][2]
        for rec_name, off in (("chain-A", 0x20C8 - 0x208C), ("chain-B", 0x217C - 0x208C)):
            print(f"  actor=0x{actor_base:08X} {rec_name} at +0x{off+0x208C:04X}")
            for pos in range(off, off + 0xAC, 4):
                samples = [e for e in data.get(pos, []) if e["width"] == 4]
                if samples:
                    latest = max(samples, key=lambda e: e["word"])
                    print(f"    +0x{pos+0x208C:04X}: 0x{latest['value'] & 0xFFFFFFFF:08X} "
                          f"({latest['access']} @ {latest['word']})")

    print("=== DEREFERENCED +0x54 / +0x58 WINDOWS ===")
    for name, (base, size, words) in extra.items():
        count = sum(len(v) for v in words.values())
        print(f"{name}: base=0x{base:08X} bytes=0x{size:X} "
              f"events={count} unique_addresses={len(words)}")
        for off in sorted(words):
            by_width = defaultdict(list)
            for row in words[off]:
                by_width[row["width"]].append(row)
            for width, rows in sorted(by_width.items()):
                latest = max(rows, key=lambda r: r["word"])
                reads = sum(r["access"] == "R" for r in rows)
                writes = sum(r["access"] == "W" for r in rows)
                mask = (1 << (width * 8)) - 1
                vals = Counter(r["value"] & mask for r in rows)
                digits = width * 2
                common = ",".join(f"0x{v:0{digits}X}({n})" for v,n in vals.most_common(3))
                print(f"  +0x{off:03X} w{width}: last=0x{latest['value'] & mask:0{digits}X} "
                      f"R{reads}/W{writes} values={common}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
