#!/usr/bin/env python3
"""Decode the mixed VF3 Flycast PC-probe and guest-memory trace stream.

The patched Flycast trace is a little-endian stream of u64 words. Recognized
record groups are:
  PC probe:  (pc<<16)|0xFA10, r13, r14, r4, r5, r6, r7, (pc<<16)|0xFA11
  memory:    (pc<<16)|0xFA20, (size<<48)|(isWrite<<47), address, value

Unknown trace words are skipped. This is deliberately a decoder for the
published probe/memory record formats, not a general instruction-trace parser.

Usage:
  python tools/loader_trace_decode.py loader_trace.bin
  python tools/loader_trace_decode.py loader_trace.bin --prefix out/loader
"""
from __future__ import annotations

import argparse
import csv
from pathlib import Path
import struct
import sys

PC_OPEN = 0xFA10
PC_CLOSE = 0xFA11
MEMORY = 0xFA20


def decode(path: Path) -> tuple[list[dict[str, str]], list[dict[str, str]], list[str]]:
    try:
        raw = path.read_bytes()
    except OSError as exc:
        raise RuntimeError(f"cannot read {path}: {exc}") from exc
    if len(raw) % 8:
        raise RuntimeError(f"trace size {len(raw)} is not divisible by 8 bytes")

    words = struct.unpack(f"<{len(raw) // 8}Q", raw) if raw else ()
    probes: list[dict[str, str]] = []
    memory: list[dict[str, str]] = []
    warnings: list[str] = []
    i = 0
    while i < len(words):
        marker = words[i] & 0xFFFF
        pc = words[i] >> 16
        if marker == PC_OPEN:
            if i + 7 >= len(words):
                warnings.append(f"truncated PC probe at u64 word {i}")
                break
            close = words[i + 7]
            if (close & 0xFFFF) != PC_CLOSE or (close >> 16) != pc:
                warnings.append(f"invalid PC probe close marker at u64 word {i}")
                i += 1
                continue
            regs = words[i + 1:i + 7]
            probes.append({
                "word_index": str(i),
                "pc": f"0x{pc:08X}",
                "r13": f"0x{regs[0]:016X}",
                "r14": f"0x{regs[1]:016X}",
                "r4": f"0x{regs[2]:016X}",
                "r5": f"0x{regs[3]:016X}",
                "r6": f"0x{regs[4]:016X}",
                "r7": f"0x{regs[5]:016X}",
            })
            i += 8
            continue
        if marker == MEMORY:
            if i + 3 >= len(words):
                warnings.append(f"truncated memory record at u64 word {i}")
                break
            meta, address, value = words[i + 1:i + 4]
            size = meta >> 48
            is_write = (meta >> 47) & 1
            value_width = 16 if size == 8 else 8
            memory.append({
                "word_index": str(i),
                "pc": f"0x{pc:08X}",
                "access": "W" if is_write else "R",
                "width": str(size),
                "address": f"0x{address:08X}",
                "value": f"0x{value:0{value_width}X}" if size == 8 else f"0x{value & 0xFFFFFFFF:08X}",
            })
            i += 4
            continue
        i += 1
    return probes, memory, warnings


def write_csv(path: Path, rows: list[dict[str, str]], fields: list[str]) -> None:
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path, help="combined Flycast trace file")
    parser.add_argument("--prefix", type=Path,
                        help="output prefix (default: trace path without suffix)")
    args = parser.parse_args(argv)
    prefix = args.prefix or args.trace.with_suffix("")
    try:
        probes, memory, warnings = decode(args.trace)
        prefix.parent.mkdir(parents=True, exist_ok=True)
        pc_path = Path(str(prefix) + ".pc.csv")
        mem_path = Path(str(prefix) + ".memory.csv")
        write_csv(pc_path, probes,
                  ["word_index", "pc", "r13", "r14", "r4", "r5", "r6", "r7"])
        write_csv(mem_path, memory,
                  ["word_index", "pc", "access", "width", "address", "value"])
    except (RuntimeError, OSError) as exc:
        print(f"loader_trace_decode: {exc}", file=sys.stderr)
        return 1
    print(f"PC probe groups: {len(probes)} -> {pc_path}")
    print(f"Memory accesses: {len(memory)} -> {mem_path}")
    for warning in warnings:
        print(f"warning: {warning}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
