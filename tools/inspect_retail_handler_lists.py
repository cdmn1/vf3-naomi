#!/usr/bin/env python3
"""Inspect retail VF3 attachment handlers and their static dispatch tables.

Reads 1ST_READ.BIN from the existing shared Dropbox asset ZIP, if present.
Only filenames, hashes and derived address tables are emitted; game binaries
are never uploaded or committed.
"""
from __future__ import annotations
import argparse, csv, hashlib, struct, sys, zipfile
from pathlib import Path

IMAGE_BASE = 0x8C010000
HANDLER_TABLE = 0x0C10F7A8
SELECTORS = 18
MAX_HANDLERS = 64
TABLES = {
    "0x0C0B1C20 mode_table_ptr": (0x0C0B1C20, 1),
    "0x0C0B1C24 kind_table_ptr": (0x0C0B1C24, 1),
    "0x0C0B1CF8 callback_targets": (0x0C0B1CF8, 7),
    "0x0C0941CC..1E4 selector_targets": (0x0C0941CC, 7),
    "0x0C08D95C..96C descriptor_targets": (0x0C08D95C, 5),
}
HALFWORDS = {
    "0x0C0B1C16..1E": (0x0C0B1C16, 5),
    "0x0C0941CC..1D6": (0x0C0941CC, 6),
}

def canonical(addr: int) -> int:
    return (addr & 0x1FFFFFFF) | 0x8C000000

def image_offset(addr: int, size: int) -> int:
    off = (addr & 0x1FFFFFFF) - (IMAGE_BASE & 0x1FFFFFFF)
    if off < 0 or off + 4 > size:
        raise ValueError(f"address 0x{addr:08x} is outside 1ST_READ.BIN")
    return off

def u32(image: bytes, addr: int) -> int:
    return struct.unpack_from("<I", image, image_offset(addr, len(image)))[0]

def u16(image: bytes, addr: int) -> int:
    off = image_offset(addr, len(image))
    return struct.unpack_from("<H", image, off)[0]

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("zip", type=Path)
    ap.add_argument("--readable-status", type=Path, required=True)
    args = ap.parse_args()

    with zipfile.ZipFile(args.zip) as zf:
        entries = zf.infolist()
        print("SHARED_ARCHIVE_INVENTORY:")
        for info in sorted(entries, key=lambda x: x.filename.lower()):
            if Path(info.filename).suffix.lower() in (".bin", ".state", ".zip", ".7z"):
                print(f"  {info.file_size:>10}  {info.filename}")
        candidates = [n for n in zf.namelist()
                      if Path(n).name.upper() == "1ST_READ.BIN"]
        if not candidates:
            print("RETAIL_IMAGE_NOT_IN_SHARED_ASSET_ZIP")
            return 0
        selected = next((n for n in candidates
                         if zf.getinfo(n).file_size == 1_179_648), candidates[0])
        image = zf.read(selected)

    print(f"IMAGE={selected}")
    print(f"IMAGE_SIZE={len(image)}")
    digest = hashlib.sha256(image).hexdigest()
    print(f"IMAGE_SHA256={digest}")
    print(f"IMAGE_MATCHES_PREVIOUS_USER_COPY={'yes' if digest == '37f38dd9a47b785f526157c4a1674ca75322f0de1342f64bbf33c88d128e4b32' else 'no'}")
    print(f"HANDLER_TABLE=0x{HANDLER_TABLE:08x}")
    print(f"HANDLER_TABLE_FILE_OFFSET=0x{image_offset(HANDLER_TABLE, len(image)):x}")

    names: dict[int, str] = {}
    with args.readable_status.open(newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            try:
                names[int(row["entry"], 0)] = row["name"]
            except (KeyError, ValueError):
                continue

    print("STATIC_DISPATCH_TABLES:")
    for label, (base, count) in TABLES.items():
        print(f" {label}:")
        for i in range(count):
            addr = base + i * 4
            raw = u32(image, addr)
            canon = canonical(raw) if raw not in (0, 0xFFFFFFFF) else raw
            name = names.get(canon, "unmapped")
            print(f"  {addr:08x}: {raw:08x} -> {canon:08x} {name}")
    print("STATIC_HALFWORD_CONSTANTS:")
    for label, (base, count) in HALFWORDS.items():
        vals = [u16(image, base + i * 2) for i in range(count)]
        print(f"  {label}: " + " ".join(f"{v:04x}" for v in vals))

    total = 0
    print("ATTACHMENT_HANDLER_LISTS:")
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
            callbacks.append((callback, addr, names.get(addr, "unmapped")))
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
