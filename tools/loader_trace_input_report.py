#!/usr/bin/env python3
"""Summarize the six resolved VF3 controller-record fields in a Flycast trace."""
from __future__ import annotations

import argparse
from collections import Counter
from pathlib import Path

from loader_trace_decode import decode

FIELDS = {
    0x0C2CF834: "port 0 status (+0)",
    0x0C2CF83C: "port 0 controller/button word (+8)",
    0x0C2CF848: "port 1 status (+0)",
    0x0C2CF850: "port 1 controller/button word (+8)",
}
PC_LABELS = {
    "0x8C09AF6C": "controller merge",
    "0x8C0AD1B8": "player action update",
    "0x8C0A1282": "packet-record lookup",
    "0x8C09AF82": "merge return",
    "0x8C09B068": "port 0 lookup return",
    "0x8C09B070": "port 1 lookup return",
    "0x8C09B074": "port 0 wrapper check",
    "0x8C09B07C": "port 1 wrapper check",
}
DIGITAL_BITS = {
    0x0001: "C", 0x0002: "B", 0x0004: "A", 0x0008: "START",
    0x0010: "UP", 0x0020: "DOWN", 0x0040: "LEFT", 0x0080: "RIGHT",
    0x0100: "Z", 0x0200: "Y", 0x0400: "X",
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path)
    parser.add_argument("--output", type=Path, default=Path("input-field-report.md"))
    args = parser.parse_args()

    probes, memory, warnings = decode(args.trace)
    lines = [
        "# VF3 controller input trace report",
        "",
        f"- Trace: `{args.trace}`",
        f"- Size: {args.trace.stat().st_size:,} bytes",
        f"- PC probe groups decoded: {len(probes):,}",
        f"- Memory records decoded: {len(memory):,}",
        f"- Decoder warnings: {len(warnings)}",
        "",
        "## Controller-path probe counts",
        "",
        "| Guest PC | Routine | Hits |",
        "|---|---|---:|",
    ]
    pc_counts = Counter(f"0x{int(row['pc'], 16) & 0x1FFFFFFF:08X}" for row in probes)
    for pc, label in PC_LABELS.items():
        normalized_pc = f"0x{int(pc, 16) & 0x1FFFFFFF:08X}"
        lines.append(f"| `{pc}` | {label} | {pc_counts[normalized_pc]:,} |")

    lines += ["", "## Digital button-word interpretation", "",
              "KOS/Dreamcast controller bit positions are a reference only; they do not prove the physical button assignment in this capture.", "",
              "| Value | KOS button bit |", "|---:|---|"]
    observed = sorted({int(row["value"], 16) for row in memory
                       if int(row["address"], 16) == 0x0C2CF83C
                       and int(row["value"], 16) != 0})
    for value in observed:
        names = [name for bit, name in DIGITAL_BITS.items() if value & bit]
        lines.append(f"| 0x{value:04X} | {", ".join(names) if names else "none"} |")
    lines += ["", "## Per-port digital record fields", ""]
    for address, label in FIELDS.items():
        rows = [row for row in memory if int(row["address"], 16) == address]
        counts = Counter((row["access"], row["value"]) for row in rows)
        values = []
        for row in rows:
            value = row["value"]
            if not values or values[-1] != value:
                values.append(value)
        lines += [
            f"### {label} — `0x{address:08X}`",
            "",
            f"- Records: {len(rows):,} (reads {sum(r['access'] == 'R' for r in rows):,}, writes {sum(r['access'] == 'W' for r in rows):,})",
            f"- Distinct values: {len(set(r['value'] for r in rows))}",
            f"- Consecutive-value changes (including initial observation): {max(0, len(values) - 1)}",
            "",
        ]
        if not rows:
            lines.append("No matching memory records were found.")
        else:
            lines += ["| Access | Value | Count |", "|---|---:|---:|"]
            for (access, value), count in sorted(counts.items()):
                lines.append(f"| {access} | `{value}` | {count:,} |")
            writes = [row for row in rows if row["access"] == "W"]
            write_values = []
            for row in writes:
                if not write_values or write_values[-1] != row["value"]:
                    write_values.append(row["value"])
            lines += ["", "Write-value sequence (consecutive duplicates collapsed; trace order):", ""]
            lines.append(" ` → ` ".join(f"`{v}`" for v in write_values[:160]))
            if len(write_values) > 160:
                lines.append(f"… {len(write_values) - 160:,} additional write transitions omitted.")
        lines.append("")

    if warnings:
        lines += ["## Decoder warnings", ""]
        lines.extend(f"- {warning}" for warning in warnings)
        lines.append("")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Wrote {args.output}; decoded {len(memory)} memory records and {len(probes)} PC probes.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
