#!/usr/bin/env python3
"""Find SH-4 PC-relative MOV.L references to source-path strings in a raw binary.

This is a static candidate finder, not a disassembler or proof that a referenced
path is on the normal gameplay path. It scans 16-bit-aligned words for
MOV.L @(disp,PC),Rn and resolves the associated literal-pool value.
"""
from __future__ import annotations

import argparse
import csv
import struct
from pathlib import Path

DEFAULT_BASE = 0x8C010000
DEFAULT_STRINGS = (
    b"/pub/data3/vf3/naomi/c/nlrom.c",
    b"/pub/data3/vf3/naomi/c/obj_load.c",
    b"/pub/data3/vf3/naomi/c/rob_load.c",
    b"/pub/data3/vf3/naomi/c/sound.c",
    b"/pub/data3/vf3/naomi/c/stage.c",
    b"/pub/data3/vf3/naomi/c/area_coli.c",
    b"/pub/data3/vf3/naomi/c/cpic_logic.c",
    # Alternate names recorded by the related decompilation project's module map.
    b"/pub/data3/vf3/naomi/c/lrom.c",
    b"/pub/data3/vf3/naomi/c/bj_load.c",
    b"/pub/data3/vf3/naomi/c/pic_logic.c",
    b"/pub/data3/vf3/naomi/c/vmsphys_.c",
)


def find_all(data: bytes, needle: bytes):
    pos = 0
    while True:
        pos = data.find(needle, pos)
        if pos < 0:
            return
        yield pos
        pos += 1


def pointer_aliases(address: int) -> set[int]:
    # SH-4 P1/P2 aliases share the low 29 physical-address bits.
    low29 = address & 0x1FFFFFFF
    return {
        address & 0xFFFFFFFF,
        low29,
        low29 | 0x80000000,
        low29 | 0xA0000000,
    }


def find_string_xrefs(data: bytes, base: int, needle: bytes):
    """Return (code_off, literal_off, value, register) for PC-relative loads."""
    results = []
    aliases_by_offset = {
        off: pointer_aliases(base + off) for off in find_all(data, needle)
    }
    if not aliases_by_offset:
        return results

    alias_to_string_offsets: dict[int, list[int]] = {}
    for string_off, aliases in aliases_by_offset.items():
        for alias in aliases:
            alias_to_string_offsets.setdefault(alias, []).append(string_off)

    for code_off in range(0, len(data) - 1, 2):
        opcode = struct.unpack_from("<H", data, code_off)[0]
        if (opcode & 0xF000) != 0xD000:  # MOV.L @(disp,PC),Rn
            continue
        pc = base + code_off
        literal_address = (pc & ~3) + 4 + (opcode & 0xFF) * 4
        literal_off = literal_address - base
        if literal_off < 0 or literal_off + 4 > len(data):
            continue
        value = struct.unpack_from("<I", data, literal_off)[0]
        if value in alias_to_string_offsets:
            reg = (opcode >> 8) & 0xF
            results.append((code_off, literal_off, value, reg))
    return results


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--base", type=lambda x: int(x, 0), default=DEFAULT_BASE)
    parser.add_argument(
        "--string", action="append", default=[],
        help="additional ASCII string to search (repeatable; defaults are always included)",
    )
    parser.add_argument("--csv", type=Path, help="optional CSV output for all matches")
    args = parser.parse_args()

    data = args.binary.read_bytes()
    needles = list(dict.fromkeys(DEFAULT_STRINGS + tuple(s.encode("ascii") for s in args.string)))
    print(f"File: {args.binary} ({len(data):,} bytes)")
    print(f"Base: 0x{args.base:08X}")
    print("Note: PC-relative MOV.L matches are candidate references, not proof of runtime reachability.")

    rows = []
    for needle in needles:
        label = needle.decode("ascii")
        string_offsets = list(find_all(data, needle))
        if not string_offsets:
            print(f"NOT FOUND: {label}")
            continue
        hits = find_string_xrefs(data, args.base, needle)
        print(f"\n{label}")
        for string_off in string_offsets:
            print(f"  string: file+0x{string_off:X} / 0x{args.base + string_off:08X}")
        if not hits:
            print("  no PC-relative MOV.L references found")
            continue
        for code_off, literal_off, value, reg in hits:
            print(
                f"  code file+0x{code_off:06X} (0x{args.base + code_off:08X}): "
                f"loads r{reg} from literal file+0x{literal_off:06X} "
                f"(value 0x{value:08X})"
            )
            rows.append({
                "string": label,
                "string_file_offset": next(
                    (off for off in string_offsets if value in pointer_aliases(args.base + off)),
                    "",
                ),
                "code_file_offset": code_off,
                "code_address": f"0x{args.base + code_off:08X}",
                "literal_file_offset": literal_off,
                "pointer_value": f"0x{value:08X}",
                "register": reg,
            })

    if args.csv:
        fields = ["string", "string_file_offset", "code_file_offset", "code_address",
                  "literal_file_offset", "pointer_value", "register"]
        args.csv.parent.mkdir(parents=True, exist_ok=True)
        with args.csv.open("w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=fields)
            writer.writeheader()
            writer.writerows(rows)
        print(f"\nWrote {len(rows)} xref rows to {args.csv}")


if __name__ == "__main__":
    main()
