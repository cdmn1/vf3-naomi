#!/usr/bin/env python3
"""Summarize decoded VF3 loader trace CSVs for the first investigation pass."""
from __future__ import annotations

import argparse
import csv
from collections import Counter
from pathlib import Path

TABLE_START, TABLE_END = 0x8C1069BC, 0x8C1069BC + 0x210
MT_START, MT_END = 0x8C5F0000, 0x8C860000
WATCH_PCS = {
    0x8C02DCEC: "mt_loader",
    0x8C04BDA2: "load_mt_to_u_pai",
    0x8C08232E: "load_mt_aki_jef",
    0x8C0360C8: "_gdFsDaPlaySct",
    0x8C0CBC40: "bjload_run",
}
REGISTER_FIELDS = ("r13", "r14", "r4", "r5", "r6", "r7")


def read_rows(path: Path) -> list[dict[str, str]]:
    try:
        with path.open(newline="", encoding="utf-8") as handle:
            return list(csv.DictReader(handle))
    except OSError as exc:
        raise RuntimeError(f"cannot read {path}: {exc}") from exc


def number(value: str, field: str, source: Path) -> int:
    try:
        return int(value, 0)
    except (TypeError, ValueError) as exc:
        raise RuntimeError(f"invalid {field} value {value!r} in {source}") from exc


def summarize(pc_path: Path, memory_path: Path) -> str:
    probes = read_rows(pc_path)
    accesses = read_rows(memory_path)
    pc_counts = Counter(number(row["pc"], "pc", pc_path) for row in probes)
    first_probe: dict[int, dict[str, str]] = {}
    for row in probes:
        pc = number(row["pc"], "pc", pc_path)
        first_probe.setdefault(pc, row)

    by_range: dict[str, Counter] = {
        "runtime pointer table": Counter(),
        "resident MT window": Counter(),
        "outside watched ranges": Counter(),
    }
    mt_writers: Counter = Counter()
    table_events: list[tuple[int, str, str, str, str, str]] = []
    mt_write_events: list[tuple[int, str, str, str, str]] = []
    for row in accesses:
        addr = number(row["address"], "address", memory_path)
        pc = number(row["pc"], "pc", memory_path)
        access = row["access"]
        width = row.get("width", "?")
        value = row.get("value", "?")
        word = number(row["word_index"], "word_index", memory_path)
        if TABLE_START <= addr < TABLE_END:
            group = "runtime pointer table"
            table_events.append((word, row["pc"], access, width, row["address"], value))
        elif MT_START <= addr < MT_END:
            group = "resident MT window"
            if access == "W":
                mt_writers[pc] += 1
                mt_write_events.append((word, row["pc"], width, row["address"], value))
        else:
            group = "outside watched ranges"
        by_range[group][access] += 1

    lines = ["VF3 loader trace summary", "", "PC probe hits:"]
    for pc, label in WATCH_PCS.items():
        lines.append(f"  {label:20s} 0x{pc:08X}: {pc_counts.get(pc, 0)}")
    lines.append(f"  {'other watched PCs':20s} {sum(n for p, n in pc_counts.items() if p not in WATCH_PCS)}")

    lines.extend(["", "First register snapshot at each watched function:"])
    for pc, label in WATCH_PCS.items():
        row = first_probe.get(pc)
        if row is None:
            lines.append(f"  {label}: (no hit)")
            continue
        regs = " ".join(f"{name}={row[name]}" for name in REGISTER_FIELDS)
        lines.append(f"  {label}: word {row['word_index']} {regs}")

    lines.extend(["", "Memory accesses by watched range:"])
    for label, counts in by_range.items():
        lines.append(f"  {label}: R={counts['R']} W={counts['W']}")

    lines.append("\nMT-window write PCs:")
    if mt_writers:
        for pc, count in mt_writers.most_common(12):
            lines.append(f"  0x{pc:08X}: {count}")
    else:
        lines.append("  (none)")

    lines.append("\nRuntime pointer-table events (first 40, with values):")
    if table_events:
        for word, pc, access, width, addr, value in sorted(table_events)[:40]:
            lines.append(f"  word {word:>10}  {access}{width} {addr} = {value}  from {pc}")
    else:
        lines.append("  (none)")

    lines.append("\nResident MT writes (first 40, with values):")
    if mt_write_events:
        for word, pc, width, addr, value in sorted(mt_write_events)[:40]:
            lines.append(f"  word {word:>10}  W{width} {addr} = {value}  from {pc}")
    else:
        lines.append("  (none)")

    lines.append("\nInterpretation note: counts and samples are raw watched accesses, not proof of a complete load or pointer-chain meaning.")
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pc_csv", type=Path, help=".pc.csv output from loader_trace_decode.py")
    parser.add_argument("memory_csv", type=Path, help=".memory.csv output from loader_trace_decode.py")
    args = parser.parse_args(argv)
    try:
        print(summarize(args.pc_csv, args.memory_csv))
    except (RuntimeError, KeyError) as exc:
        parser.exit(1, f"loader_trace_summary: {exc}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
