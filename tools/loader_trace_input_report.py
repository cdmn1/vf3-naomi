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
    0x0C29BA50: "merged controller word (+0x1EC)",
    0x0C29BCCC: "controller merge selector (+8)",
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
        lines.append("| 0x{:04X} | {} |".format(value, ", ".join(names) if names else "none"))
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
        if address in (0x0C2CF83C, 0x0C29BA50):
            reads = [row for row in rows if row["access"] == "R"]
            if reads:
                read_sites = Counter(row["pc"] for row in reads)
                lines += ["", "Read instruction sites:", "",
                          "| Guest PC | Reads |", "|---|---:|"]
                for pc, count in read_sites.most_common(20):
                    lines.append("| {} | {:,} |".format(pc, count))
        lines.append("")

    # The indexed node-flag routine is the current candidate consumer of
    # the digital controller word. Capture its calling convention so the
    # next pass can resolve the three dynamic output addresses from R9.
    candidate_pcs = {
        0x0C096258: "indexed node-flag routine entry",
        0x0C096280: "node status read",
        0x0C0962A0: "digital controller-word read",
        0x0C0962C8: "third node-field read",
        0x0C09648C: "later node-field read",
    }
    lines += ["", "## Indexed node-flag input routine snapshots", "",
              "The reconstruction uses R4 as the signed node index, R6 as the mask-table pointer, and R9 as the base for output words at R9+0x138, +0x13C and +0x140.", "",
              "| Guest PC | Site | Hits |", "|---|---|---:|"]
    candidate_counts = Counter(
        int(row["pc"], 16) & 0x1FFFFFFF for row in probes
        if (int(row["pc"], 16) & 0x1FFFFFFF) in candidate_pcs
    )
    for pc, label in candidate_pcs.items():
        lines.append(f"| 0x{pc:08X} | {label} | {candidate_counts[pc]:,} |")
    entry_rows = [row for row in probes
                  if (int(row["pc"], 16) & 0x1FFFFFFF) == 0x0C096258]
    tuples = Counter((row.get("r4", ""), row.get("r6", ""), row.get("r9", ""))
                     for row in entry_rows)
    lines += ["", "### Entry register combinations", "",
              "| R4 (node index) | R6 (mask table) | R9 (output base) | Hits |",
              "|---:|---:|---:|---:|"]
    for (r4, r6, r9), count in tuples.most_common(30):
        lines.append(f"| {r4} | {r6} | {r9} | {count:,} |")
    if len(tuples) > 30:
        lines.append(f"| … | … | … | {len(tuples) - 30:,} additional combinations omitted |")
    output_bases = Counter()
    for row in entry_rows:
        try:
            base = int(row["r9"], 16) & 0xFFFFFFFF
            output_bases[base] += 1
        except (KeyError, ValueError):
            continue
    lines += ["", "### Candidate output addresses", ""]
    if output_bases:
        for base, count in output_bases.most_common(20):
            lines.append(f"- R9 `{base:#010x}` ({count:,} entry snapshots): "
                         f"`{base + 0x138:#010x}`, `{base + 0x13C:#010x}`, "
                         f"`{base + 0x140:#010x}`")
    else:
        lines.append("No usable R9 values were present in the entry snapshots.")
    lines.append("")
    # Align transitions in the raw P1 guest record with the merged word.
    timeline_addresses = {
        0x0C2CF83C: "P1 record +8",
        0x0C29BA50: "merged +0x1EC",
    }
    last_value = {}
    timeline = []
    for row in memory:
        address = int(row["address"], 16)
        if address not in timeline_addresses or row["access"] != "W":
            continue
        value = int(row["value"], 16)
        if last_value.get(address) != value:
            timeline.append((int(row["word_index"]), timeline_addresses[address], row["value"], row["pc"]))
            last_value[address] = value
    for row in probes:
        pc = int(row["pc"], 16) & 0x1FFFFFFF
        if pc == 0x0C09AF6C:
            timeline.append((int(row["word_index"]), "PC controller merge", "", row["pc"]))
        elif pc == 0x0C0AD1B8:
            timeline.append((int(row["word_index"]), "PC player-action update", "", row["pc"]))
    lines += ["", "## P1 digital input / merge / action timeline", "",
              "| Trace word index | Event | Value | PC |", "|---:|---|---:|---|"]
    for index, field, value, pc in sorted(timeline):
        lines.append("| {:,} | {} | {} | {} |".format(index, field, value, pc))

    action_rows = [row for row in probes
                   if (int(row["pc"], 16) & 0x1FFFFFFF) == 0x0C0AD1B8]
    lines += ["", "## Player-action update register snapshots", "",
              "Selected registers at each watched execution of 0x8C0AD1B8.", "",
              "| Trace word index | PR | R0 | R1 | R2 | R3 | R4 | R5 | R6 | R7 | R13 | R14 | R15 |",
              "|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|"]
    for row in action_rows:
        regs = [row.get("r" + str(n), "") for n in [0,1,2,3,4,5,6,7,13,14,15]]
        lines.append("| " + " | ".join([row["word_index"], row.get("pr", "")] + regs) + " |")

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
