#!/usr/bin/env python3
"""Summarize decoded VF3 loader-trace CSVs, normalizing SH-4 address aliases."""
from __future__ import annotations

import argparse
import csv
from collections import Counter
from pathlib import Path

def normalize(address: int) -> int:
    """Return the 29-bit SH-4 physical address used by P0/P1/P2 aliases."""
    return address & 0x1FFFFFFF

def canonical(address: int) -> int:
    """Format a normalized RAM address using the game's 0x8C P1 alias."""
    return 0x80000000 | normalize(address)

TABLE_START = normalize(0x8C1069BC)
TABLE_END = TABLE_START + 0x210
MT_START = normalize(0x8C5F0000)
MT_END = MT_START + 0x270000
WATCH_PCS = {
    normalize(0x8C02DCEC): ("mt_loader", 0x8C02DCEC),
    normalize(0x8C04BDA2): ("load_mt_to_u_pai", 0x8C04BDA2),
    normalize(0x8C08232E): ("load_mt_aki_jef", 0x8C08232E),
    normalize(0x8C0360C8): ("_gdFsDaPlaySct", 0x8C0360C8),
    normalize(0x8C0CBC40): ("bjload_run", 0x8C0CBC40),
    normalize(0x8C0A7F84): ("table_entry_load", 0x8C0A7F84),
    normalize(0x8C0A2F90): ("obj_load_prologue", 0x8C0A2F90),
    normalize(0x8C0A2FDE): ("obj_load_indirect_call", 0x8C0A2FDE),
    normalize(0x8C0A2EC8): ("obj_buffer_zero_A", 0x8C0A2EC8),
    normalize(0x8C0A304E): ("obj_buffer_zero_B", 0x8C0A304E),
    normalize(0x8C09D5AE): ("motion_id_row_lookup", 0x8C09D5AE),
    normalize(0x8C09D5B0): ("motion_row_pointer", 0x8C09D5B0),
    normalize(0x8C09D5CE): ("mt_slot_lookup_B", 0x8C09D5CE),
    normalize(0x8C09D5D2): ("mt_slot_lookup_A", 0x8C09D5D2),
    normalize(0x8C09D5DA): ("mt_slot_lookup_C", 0x8C09D5DA),
    normalize(0x8C09D690): ("mt_evaluator_entry", 0x8C09D690),
    normalize(0x8C09D69A): ("mt_linked_record_helper", 0x8C09D69A),
    normalize(0x8C09D6F4): ("mt_counts_cursor", 0x8C09D6F4),
    normalize(0x8C09D70A): ("mt_times_cursor", 0x8C09D70A),
    normalize(0x8C09D778): ("mt_tuple_lane_0", 0x8C09D778),
    normalize(0x8C09D77E): ("mt_tuple_lane_1", 0x8C09D77E),
    normalize(0x8C09D782): ("mt_tuple_lane_2", 0x8C09D782),
    normalize(0x8C09D794): ("mt_tuple_lane_3", 0x8C09D794),
}
# Expanded FA12/FA13 records carry PR and R0-R15. Include the call target
# register (r1) and likely argument/manager registers in the compact report.
REGISTER_FIELDS = ("pr",) + tuple(f"r{n}" for n in range(16))

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
    pc_counts: Counter[int] = Counter()
    first_probe: dict[int, dict[str, str]] = {}
    for row in probes:
        pc = normalize(number(row["pc"], "pc", pc_path))
        pc_counts[pc] += 1
        first_probe.setdefault(pc, row)

    by_range: dict[str, Counter] = {
        "runtime pointer table": Counter(),
        "resident MT window": Counter(),
        "outside watched ranges": Counter(),
    }
    mt_writers: Counter[int] = Counter()
    table_events: list[tuple[int, int, str, str, int, str]] = []
    mt_write_events: list[tuple[int, int, str, int, str]] = []
    for row in accesses:
        raw_addr = number(row["address"], "address", memory_path)
        addr = normalize(raw_addr)
        pc = normalize(number(row["pc"], "pc", memory_path))
        access = row["access"]
        width = row.get("width", "?")
        value = row.get("value", "?")
        word = number(row["word_index"], "word_index", memory_path)
        if TABLE_START <= addr < TABLE_END:
            group = "runtime pointer table"
            table_events.append((word, pc, access, width, addr, value))
        elif MT_START <= addr < MT_END:
            group = "resident MT window"
            if access == "W":
                mt_writers[pc] += 1
                mt_write_events.append((word, pc, width, addr, value))
        else:
            group = "outside watched ranges"
        by_range[group][access] += 1

    lines = ["VF3 loader trace summary", "", "PC probe hits:"]
    for normalized_pc, (label, display_pc) in WATCH_PCS.items():
        lines.append(f"  {label:20s} 0x{display_pc:08X}: {pc_counts.get(normalized_pc, 0)}")
    known = set(WATCH_PCS)
    lines.append(f"  {'other watched PCs':20s} {sum(n for p, n in pc_counts.items() if p not in known)}")

    lines.extend(["", "First register snapshot at each watched function:"])
    for normalized_pc, (label, _display_pc) in WATCH_PCS.items():
        row = first_probe.get(normalized_pc)
        if row is None:
            lines.append(f"  {label}: (no hit)")
            continue
        regs = " ".join(f"{name}={row.get(name, '?')}" for name in REGISTER_FIELDS)
        lines.append(f"  {label}: word {row['word_index']} {regs}")

    mt_probe_pcs = {
        normalize(pc) for pc in (
            0x8C09D5AE, 0x8C09D5B0, 0x8C09D5CE, 0x8C09D5D2, 0x8C09D5DA,
            0x8C09D690, 0x8C09D69A, 0x8C09D6F4, 0x8C09D70A,
            0x8C09D778, 0x8C09D77E, 0x8C09D782, 0x8C09D794,
        )
    }
    lines.extend(["", "MT motion-ID / slot / evaluator register snapshots (first 12 per PC):"])
    for pc in sorted(mt_probe_pcs):
        hits = [row for row in probes if normalize(number(row["pc"], "pc", pc_path)) == pc]
        if not hits:
            continue
        label = WATCH_PCS.get(pc, (f"0x{canonical(pc):08X}", pc))[0]
        lines.append(f"  {label}: {len(hits)} hits")
        for row in hits[:12]:
            regs = " ".join(f"{name}={row.get(name, '?')}" for name in REGISTER_FIELDS)
            lines.append(f"    word {row['word_index']}: {regs}")

    context_pcs = {
        normalize(0x8C09D5AE): "motion_id_row_lookup",
        normalize(0x8C09D5CE): "mt_slot_lookup_B",
        normalize(0x8C09D5D2): "mt_slot_lookup_A",
        normalize(0x8C09D5DA): "mt_slot_lookup_C",
        normalize(0x8C09D690): "mt_evaluator_entry",
        normalize(0x8C09D69A): "mt_linked_record_helper",
    }
    lines.extend(["", "R9 context-pointer candidates (not actor identities):"])
    for pc, label in context_pcs.items():
        hits = [row for row in probes if normalize(number(row["pc"], "pc", pc_path)) == pc]
        if not hits:
            continue
        by_r9 = Counter(row.get("r9", "?") for row in hits)
        rendered = ", ".join(f"{pointer} ({count})" for pointer, count in by_r9.most_common())
        lines.append(f"  {label}: {rendered}")
    lines.append(
        "  Note: R9 is reported as an opaque register value; these counts do not establish actor ownership."
    )

    lines.extend(["", "Memory accesses by watched range:"])
    for label, counts in by_range.items():
        lines.append(f"  {label}: R={counts['R']} W={counts['W']}")

    lines.extend(["", "MT-window write PCs:"])
    if mt_writers:
        for pc, count in mt_writers.most_common(12):
            lines.append(f"  0x{canonical(pc):08X}: {count}")
    else:
        lines.append("  (none)")

    lines.extend(["", "Runtime pointer-table events (first 40, with values):"])
    if table_events:
        for word, pc, access, width, addr, value in sorted(table_events)[:40]:
            lines.append(f"  word {word:>10}  {access}{width} 0x{canonical(addr):08X} = {value}  from 0x{canonical(pc):08X}")
    else:
        lines.append("  (none)")

    lines.extend(["", "Resident MT writes (first 40, with values):"])
    if mt_write_events:
        for word, pc, width, addr, value in sorted(mt_write_events)[:40]:
            lines.append(f"  word {word:>10}  W{width} 0x{canonical(addr):08X} = {value}  from 0x{canonical(pc):08X}")
    else:
        lines.append("  (none)")

    lines.append("")
    lines.append("Interpretation note: counts and samples are raw watched accesses, not proof of a complete load or pointer-chain meaning.")
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
