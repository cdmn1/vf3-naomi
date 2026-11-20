#!/usr/bin/env python3
"""Decode the mixed VF3 Flycast PC-probe, guest-memory and GD-ROM DMA trace stream.

Recognized record groups:
  Legacy PC probe: (pc<<16)|0xFA10, r13, r14, r4, r5, r6, r7, (pc<<16)|0xFA11
  expanded PC:     (pc<<16)|0xFA12, PR, R0..R15, (pc<<16)|0xFA13
  memory:    (pc<<16)|0xFA20, (size<<48)|(isWrite<<47), address, value
  GD-ROM DMA: 0xFA21, destination, byte count, starting FAD, sector size, remaining sectors
"""
from __future__ import annotations

import argparse
import csv
from pathlib import Path
import struct
import sys

PC_OPEN = 0xFA10
PC_CLOSE = 0xFA11
PC_EXPANDED_OPEN = 0xFA12
PC_EXPANDED_CLOSE = 0xFA13
MEMORY = 0xFA20
GDROM_DMA = 0xFA21


def decode(path: Path, include_transfers: bool = False):
    try:
        raw = path.read_bytes()
    except OSError as exc:
        raise RuntimeError(f"cannot read {path}: {exc}") from exc
    if len(raw) % 8:
        raise RuntimeError(f"trace size {len(raw)} is not divisible by 8 bytes")

    words = struct.unpack(f"<{len(raw) // 8}Q", raw) if raw else ()
    probes, memory, transfers, warnings = [], [], [], []
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
                "word_index": str(i), "pc": f"0x{pc:08X}",
                "r13": f"0x{regs[0]:016X}", "r14": f"0x{regs[1]:016X}",
                "r4": f"0x{regs[2]:016X}", "r5": f"0x{regs[3]:016X}",
                "r6": f"0x{regs[4]:016X}", "r7": f"0x{regs[5]:016X}",
            })
            i += 8
            continue
        if marker == PC_EXPANDED_OPEN:
            if i + 18 >= len(words):
                warnings.append(f"truncated expanded PC probe at u64 word {i}")
                break
            close = words[i + 18]
            if (close & 0xFFFF) != PC_EXPANDED_CLOSE or (close >> 16) != pc:
                warnings.append(f"invalid expanded PC probe close marker at u64 word {i}")
                i += 1
                continue
            regs = words[i + 2:i + 18]
            row = {"word_index": str(i), "pc": f"0x{pc:08X}", "pr": f"0x{words[i + 1]:08X}"}
            row.update({f"r{n}": f"0x{value:08X}" for n, value in enumerate(regs)})
            probes.append(row)
            i += 19
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
                "word_index": str(i), "pc": f"0x{pc:08X}",
                "access": "W" if is_write else "R", "width": str(size),
                "address": f"0x{address:08X}",
                "value": f"0x{value:0{value_width}X}" if size == 8 else f"0x{value & 0xFFFFFFFF:08X}",
            })
            i += 4
            continue
        if marker == GDROM_DMA:
            if i + 5 >= len(words):
                warnings.append(f"truncated GD-ROM DMA record at u64 word {i}")
                break
            destination, size, fad, sector_size, remaining = words[i + 1:i + 6]
            transfers.append({
                "word_index": str(i), "destination": f"0x{destination:08X}",
                "size": str(size), "start_fad": str(fad),
                "sector_size": str(sector_size), "remaining_sectors": str(remaining),
            })
            i += 6
            continue
        i += 1
    if include_transfers:
        return probes, memory, transfers, warnings
    return probes, memory, warnings


def write_csv(path: Path, rows: list[dict[str, str]], fields: list[str]) -> None:
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path, help="combined Flycast trace file")
    parser.add_argument("--prefix", type=Path, help="output prefix (default: trace path without suffix)")
    args = parser.parse_args(argv)
    prefix = args.prefix or args.trace.with_suffix("")
    try:
        probes, memory, transfers, warnings = decode(args.trace, include_transfers=True)
        prefix.parent.mkdir(parents=True, exist_ok=True)
        write_csv(Path(str(prefix) + ".pc.csv"), probes,
                  ["word_index", "pc", "pr"] + [f"r{n}" for n in range(16)])
        write_csv(Path(str(prefix) + ".memory.csv"), memory,
                  ["word_index", "pc", "access", "width", "address", "value"])
        write_csv(Path(str(prefix) + ".gdrom_dma.csv"), transfers,
                  ["word_index", "destination", "size", "start_fad", "sector_size", "remaining_sectors"])
    except (RuntimeError, OSError) as exc:
        print(f"loader_trace_decode: {exc}", file=sys.stderr)
        return 1
    print(f"PC probe groups: {len(probes)}")
    print(f"Memory accesses: {len(memory)}")
    print(f"GD-ROM DMA transfers: {len(transfers)}")
    for warning in warnings:
        print(f"warning: {warning}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
