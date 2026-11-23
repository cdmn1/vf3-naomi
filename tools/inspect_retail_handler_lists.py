#!/usr/bin/env python3
"""Inspect retail VF3 attachment handlers and their static dispatch tables.

Reads 1ST_READ.BIN from the existing shared Dropbox asset ZIP, if present.
Only filenames, hashes and derived address tables are emitted; game binaries
are never uploaded or committed.
"""
from __future__ import annotations
import argparse, csv, hashlib, struct, subprocess, sys, zipfile
from pathlib import Path

IMAGE_BASE = 0x8C010000
HANDLER_TABLE = 0x0C10F7A8
SELECTORS = 18
MAX_HANDLERS = 64
TABLES = {
    "0x0C10F940 mode_dispatch_table": (0x0C10F940, 32),
    "0x0C10F6D0 mode_selector_table": (0x0C10F6D0, 32),
    "0x0C10E548 actor-A motion record": (0x0C10E548, 32),
    "0x0C10EB60 actor-B motion record": (0x0C10EB60, 32),
    "0x0C11D4E8 actor-A model record": (0x0C11D4E8, 204),
    "0x0C11D1B8 actor-B model record": (0x0C11D1B8, 204),
    "0x0C03006C actor-A descriptor": (0x0C03006C, 16),
    "0x0C030D6C actor-B descriptor": (0x0C030D6C, 16),
    "0x0C03186C actor-B alternate descriptor": (0x0C03186C, 16),
    "0x0C11D8D0 actor descriptor pointer table": (0x0C11D8D0, 32),
    "0x0C0AB9A0 node handler table": (0x0C0AB9A0, 64),
    "0x0C0B1C20 mode_table_ptr": (0x0C0B1C20, 1),
    "0x0C0B1C24 kind_table_ptr": (0x0C0B1C24, 1),
    "0x0C0B1CF0..1D14 callback control/helpers": (0x0C0B1CF0, 10),
    "0x0C0A9C10 actor-init lookup table": (0x0C0A9C10, 8),
    "0x0C0941CC..1E4 selector_targets": (0x0C0941CC, 7),
    "0x0C08D95C..96C descriptor_targets": (0x0C08D95C, 5),
}
HALFWORDS = {
    "0x0C0B1C16..1E": (0x0C0B1C16, 5),
    "0x0C0941CC..1D6": (0x0C0941CC, 6),
    "0x0C0B1CEA..1CF2 callback constants": (0x0C0B1CEA, 5),
    "0x0C0A9906..9912 actor-init offsets": (0x0C0A9906, 7),
    "0x0C0A99CC..99D2 actor-init offsets": (0x0C0A99CC, 4),
    "0x0C0A9B04..9B16 actor-init offsets": (0x0C0A9B04, 10),
    "0x0C0A9C04..9C16 actor-init offsets": (0x0C0A9C04, 10),
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
    ap.add_argument("--sh4-tools", type=Path, required=True)
    args = ap.parse_args()
    sys.path.insert(0, str(args.sh4_tools))
    import sh4

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
            is_ptr = (raw & 0xFF000000) in (0x0C000000, 0x8C000000)
            canon = canonical(raw) if is_ptr else raw
            name = names.get(canon, "unmapped") if is_ptr else "value"
            print(f"  {addr:08x}: {raw:08x}" +
                  (f" -> {canon:08x} {name}" if is_ptr else f" ({name})"))
    print("MODE_DISPATCH_RECORD_PREFIXES:")
    mode_ptrs = sorted({u32(image, 0x0C10F940 + i * 4)
                        for i in range(26)
                        if (u32(image, 0x0C10F940 + i * 4) & 0xFF000000)
                        in (0x0C000000, 0x8C000000)})
    for ptr in mode_ptrs:
        print(f"  record 0x{ptr:08x}:")
        for i in range(16):
            addr = ptr + i * 4
            raw = u32(image, addr)
            is_ptr = (raw & 0xFF000000) in (0x0C000000, 0x8C000000)
            canon = canonical(raw) if is_ptr else raw
            print(f"    +0x{i*4:02x}: {raw:08x}" +
                  (f" -> {canon:08x}" if is_ptr else ""))
    print("ACTOR_RECORD_POINTER_REFERENCES:")
    actor_record_targets = (0x0C11D4E8, 0x0C11D1B8, 0x0C10E548, 0x0C10EB60,
                           0x0C03006C, 0x0C030D6C, 0x0C03186C, 0x0C11D8E8)
    pointer_literals: dict[int, list[int]] = {}
    for target in actor_record_targets:
        needle = struct.pack("<I", target)
        refs = []
        start = 0
        while True:
            found = image.find(needle, start)
            if found < 0:
                break
            refs.append(0x8C010000 + found)
            start = found + 1
        pointer_literals[target] = refs
        print(f"  0x{target:08x}: " +
              (", ".join(f"0x{x:08x}" for x in refs[:32]) if refs else "no literal references") +
              (f" (+{len(refs)-32} more)" if len(refs) > 32 else ""))

    # Resolve the SH-4 PC-relative MOV.L instructions that load those literal
    # pool words. This identifies the actual code sites which consume the
    # model/motion records, rather than merely listing where their pointers sit.
    literal_to_value = {
        literal: target
        for target, literals in pointer_literals.items()
        for literal in literals
    }
    code_refs = []
    for off in range(0, len(image) - 1, 2):
        opcode = struct.unpack_from("<H", image, off)[0]
        if (opcode & 0xF000) != 0xD000:  # MOV.L @(disp,PC),Rn
            continue
        pc = IMAGE_BASE + off
        literal = ((pc + 4) & ~3) + ((opcode & 0xFF) * 4)
        if literal not in literal_to_value:
            continue
        reg = (opcode >> 8) & 0x0F
        decoded = sh4.decode(opcode, pc)
        mnemonic = f"{decoded[0]} {decoded[1]}".strip() if decoded else "unknown"
        value = u32(image, literal)
        code_refs.append((pc, opcode, reg, literal, value, mnemonic))
    print("ACTOR_RESOURCE_PC_RELATIVE_POINTER_LOADS:")
    for pc, opcode, reg, literal, value, mnemonic in code_refs:
        print(f"  pc=0x{pc:08x} op=0x{opcode:04x} {mnemonic}; "
              f"literal=0x{literal:08x} -> value=0x{value:08x}")
    print("ACTOR_RESOURCE_POINTER_REFERENCE_CODE:")
    adapter_dump = args.sh4_tools / "adapter_dump.py"
    for pc, opcode, reg, literal, value, mnemonic in code_refs:
        print(f"  ---- reference PC 0x{pc:08x} -> record 0x{value:08x}")
        try:
            result = subprocess.run(
                [sys.executable, str(adapter_dump), f"0x{pc:08x}", "--max", "96"],
                check=False, capture_output=True, text=True, timeout=30)
            body = result.stdout.strip() or result.stderr.strip()
            print(body if body else f"  <adapter dump exit {result.returncode}>")
        except (OSError, subprocess.SubprocessError) as exc:
            print(f"  <adapter dump unavailable: {exc}>")

    # The two actor model records are each 204 words. Show a neutral 17 x
    # 12-float chunking for comparison with the 23-bone/18-joint path; this
    # grouping is descriptive only and does not assign bone semantics.
    print("MODEL_RECORD_17X12_FLOAT_CHUNKS (four float3 values per chunk):")
    for label, base in (("Akira", 0x0C11D4E8), ("Jacky", 0x0C11D1B8)):
        print(f"  {label}:")
        for record in range(17):
            vals = [struct.unpack_from(
                "<f", image, image_offset(base + record * 48 + word * 4, len(image)))[0]
                for word in range(12)]
            vecs = [vals[i:i + 3] for i in range(0, 12, 3)]
            fmt = " ".join("(" + ",".join(f"{v:+.4f}" for v in vec) + ")" for vec in vecs)
            print(f"    {record:02d}: {fmt}")
    bone_table = 0x0C119670
    bone_off = image_offset(bone_table, len(image))
    advances = image[bone_off:bone_off + 23]
    cumulative = 0
    print(f"BONE_POSE_ADVANCE_TABLE=0x{bone_table:08x} bytes={advances.hex()}")
    print("BONE_TO_ATTACHMENT_ROW_MAP (current record is used before advance):")
    for i, advance in enumerate(advances):
        current_offset = 0x180 + cumulative
        current_row = ((current_offset - 0x180) // 0x40
                       if (current_offset - 0x180) % 0x40 == 0 else "non-64-byte")
        cumulative += advance
        next_offset = 0x180 + cumulative
        print(f"  bone {i:02d}: current_joint=0x{current_offset:04x} attachment_row={current_row} "
              f"advance=0x{advance:02x} next_joint=0x{next_offset:04x}")
    print("STATIC_HALFWORD_CONSTANTS:")
    for label, (base, count) in HALFWORDS.items():
        vals = [u16(image, base + i * 2) for i in range(count)]
        print(f"  {label}: " + " ".join(f"{v:04x}" for v in vals))

    total = 0
    callbacks_seen: list[int] = []
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
            callbacks_seen.append(addr)
        print(f" count={len(callbacks)} terminated={terminated}")
        for index, (raw, addr, name) in enumerate(callbacks):
            print(f"  [{index:02d}] raw=0x{raw:08x} entry=0x{addr:08x} name={name}")
        total += len(callbacks)
        if not terminated:
            print("  WARNING: no zero terminator within 64 entries")
    print(f"TOTAL_CALLBACKS={total}")
    print("CALLBACK_RAW_DISASSEMBLY (linear sweep, up to 256 instructions; literal loads annotated):")
    for addr in sorted(set(callbacks_seen)):
        print(f"  0x{addr:08x}:")
        count = 0
        terminal = False
        for rec in sh4.disasm(image, IMAGE_BASE, addr, 256):
            literal = ""
            if "lit_addr" in rec:
                literal = f" ; literal 0x{rec['lit_addr']:08x}=0x{rec['lit_val']:08x}"
            print(f"    0x{rec['addr']:08x}: {rec['word']:04x} {rec['text']}{literal}")
            count += 1
            # Include the delay-slot instruction after a return or tail jump.
            if terminal and rec.get("delay"):
                break
            mnemonic = rec["text"].split(" ", 1)[0]
            if mnemonic in ("rts", "rte", "jmp"):
                terminal = True
        if count == 0:
            print("    <unreadable>")

    print("CALLBACK_TRANSLATED_BODIES:")
    adapter_dump = args.sh4_tools / "adapter_dump.py"
    for addr in sorted(set(callbacks_seen)):
        print(f"  ---- callback 0x{addr:08x}")
        try:
            result = subprocess.run(
                [sys.executable, str(adapter_dump), f"0x{addr:08x}", "--max", "128"],
                check=False, capture_output=True, text=True, timeout=45)
            body = result.stdout.strip() or result.stderr.strip()
            print(body if body else f"  <adapter dump exit {result.returncode}>")
        except (OSError, subprocess.SubprocessError) as exc:
            print(f"  <adapter dump unavailable: {exc}>")
    return 0

if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, zipfile.BadZipFile, ValueError, struct.error) as exc:
        print(f"HANDLER_TABLE_AUDIT_FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
