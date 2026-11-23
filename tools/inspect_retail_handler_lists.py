#!/usr/bin/env python3
"""Inspect retail VF3 attachment handler lists without retaining game binaries.

Reads 1ST_READ.BIN from the existing shared Dropbox asset ZIP, if present,
then resolves the 18 selector handler-list pointers at 0x0C10F7A8. Only the
derived address report is emitted; the copyrighted executable is never
uploaded or committed.
"""
from __future__ import annotations

import argparse
import csv
import io
import struct
import sys
import zipfile
from pathlib import Path

IMAGE_BASE = 0x8C010000
HANDLER_TABLE = 0x0C10F7A8
SELECTORS = 18
MAX_HANDLERS = 64


def canonical(addr: int) -> int:
    return (addr & 0x1FFFFFFF) | 0x8C000000


def image_offset(addr: int, size: int) -> int:
    off = (addr & 0x1FFFFFFF) - (IMAGE_BASE & 0x1FFFFFFF)
    if off < 0 or off + 4 > size:
        raise ValueError(f"address 0x{addr:08x} is outside 1ST_READ.BIN")
    return off


def u32(image: bytes, addr: int) -> int:
    return struct.unpack_from("<I", image, image_offset(addr, len(image)))[0]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("zip", type=Path)
    ap.add_argument("--readable-status", type=Path, required=True)
    args = ap.parse_args()

    with zipfile.ZipFile(args.zip) as zf:
        candidates = [n for n in zf.namelist()
                      if Path(n).name.upper() == "1ST_READ.BIN"]
        if not candidates:
            bins = sorted(n for n in zf.namelist()
                          if Path(n).suffix.upper() == ".BIN")
            print("RETAIL_IMAGE_NOT_IN_SHARED_ASSET_ZIP")
            print("BIN_FILES_IN_ZIP:")
            for name in bins[:100]:
                print(name)
            return 0
        # Prefer the expected retail executable size if duplicate names exist.
        selected = next((n for n in candidates
                         if zf.getinfo(n).file_size == 1_179_648), candidates[0])
        image = zf.read(selected)

    print(f"IMAGE={selected}")
    print(f"IMAGE_SIZE={len(image)}")
    print(f"IMAGE_BASE=0x{IMAGE_BASE:08x}")
    print(f"HANDLER_TABLE=0x{HANDLER_TABLE:08x}")
    print(f"HANDLER_TABLE_FILE_OFFSET=0x{image_offset(HANDLER_TABLE, len(image)):x}")

    names: dict[int, str] = {}
    with args.readable_status.open(newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            try:
                names[int(row["entry"], 0)] = row["name"]
            except (KeyError, ValueError):
                continue

    total = 0
    for selector in range(SELECTORS):
        table_cell = HANDLER_TABLE + selector * 4
        list_ptr = u32(image, table_cell)
        print(f"SELECTOR {selector:02d}: list=0x{list_ptr:08x}", end="")
        if list_ptr in (0, 0xFFFFFFFF):
            print(" EMPTY")
            continue
        callbacks = []
        terminated = False
        for i in range(MAX_HANDLERS):
            callback = u32(image, list_ptr + i * 4)
            if callback == 0:
                terminated = True
                break
            addr = canonical(callback)
            name = names.get(addr, "unmapped")
            callbacks.append((callback, addr, name))
        print(f" count={len(callbacks)} terminated={terminated}")
        for index, (raw, addr, name) in enumerate(callbacks):
            print(f"  [{index:02d}] raw=0x{raw:08x} entry=0x{addr:08x} name={name}")
        total += len(callbacks)
        if not terminated:
            print("  WARNING: no zero terminator within 64 entries")
    print(f"TOTAL_CALLBACKS={total}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, zipfile.BadZipFile, ValueError, struct.error) as exc:
        print(f"HANDLER_TABLE_AUDIT_FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
