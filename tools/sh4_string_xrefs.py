#!/usr/bin/env python3
"""Find SH-4 PC-relative MOV.L references to selected strings in a raw binary.

Default base is the Dreamcast 1ST_READ.BIN load address. No game data is
written; this tool only reads the supplied executable and prints file offsets.
"""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

DEFAULT_BASE = 0x8C010000
DEFAULT_STRINGS = (
    b"/pub/data3/vf3/naomi/c/nlrom.c",
    b"/pub/data3/vf3/naomi/c/obj_load.c",
    b"/pub/data3/vf3/naomi/c/rob_load.c",
    b"/pub/data3/vf3/naomi/c/sound.c",
    b"/pub/data3/vf3/naomi/c/stage.c",
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
    # SH-4 P1/P2 aliases share the low 29 physical-address bits. Some
    # Dreamcast/Naomi binaries store these source-data pointers in the
    # 0x0cxxxxxx form rather than as a full 0x8c/0xac virtual address.
    low29 = address & 0x1FFFFFFF
    return {
        address & 0xFFFFFFFF,
        low29,
        low29 | 0x80000000,
        low29 | 0xA0000000,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--base", type=lambda x: int(x, 0), default=DEFAULT_BASE)
    args = parser.parse_args()

    data = args.binary.read_bytes()
    print(f"File: {args.binary} ({len(data):,} bytes)")
    print(f"Base: 0x{args.base:08X}")

    strings = []
    for needle in DEFAULT_STRINGS:
        offsets = list(find_all(data, needle))
        if offsets:
            for off in offsets:
                strings.append((needle.decode("ascii"), off,
                                pointer_aliases(args.base + off)))
        else:
            print(f"NOT FOUND: {needle.decode('ascii')}")

    for label, string_off, aliases in strings:
        hits = []
        for off in range(0, len(data) - 1, 2):
            opcode = struct.unpack_from("<H", data, off)[0]
            if (opcode & 0xF000) != 0xD000:  # MOV.L @(disp,PC),Rn
                continue
            pc = args.base + off
            literal_address = (pc & ~3) + 4 + (opcode & 0xFF) * 4
            literal_off = literal_address - args.base
            if literal_off < 0 or literal_off + 4 > len(data):
                continue
            value = struct.unpack_from("<I", data, literal_off)[0]
            if value in aliases:
                reg = (opcode >> 8) & 0xF
                hits.append((off, literal_off, value, reg))

        print(f"\n{label}")
        print(f"  string: file+0x{string_off:X} / 0x{args.base + string_off:08X}")
        if not hits:
            print("  no PC-relative MOV.L references found")
            continue
        for off, literal_off, value, reg in hits:
            print(
                f"  code file+0x{off:06X} (0x{args.base + off:08X}): "
                f"loads r{reg} from literal file+0x{literal_off:06X} "
                f"(value 0x{value:08X})"
            )


if __name__ == "__main__":
    main()
