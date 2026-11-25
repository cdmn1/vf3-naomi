#!/usr/bin/env python3
"""Correlate MT evaluator entry snapshots with preceding actor motion/cycle reads."""
import csv
import gzip
import sys
from collections import Counter, defaultdict
from pathlib import Path

pc_path = Path(sys.argv[1])
mem_path = Path(sys.argv[2])
out_dir = Path(sys.argv[3])
out_dir.mkdir(parents=True, exist_ok=True)

actors = {
    0x0C1FEFE4: {"motion_id": None, "cycle": None, "calls": 0,
                 "pairs": Counter(), "phases": [], "r5": Counter(),
                 "cycle_mismatches": 0},
    0x0C20138C: {"motion_id": None, "cycle": None, "calls": 0,
                 "pairs": Counter(), "phases": [], "r5": Counter(),
                 "cycle_mismatches": 0},
}
motion_addr = {base + 0x3C: base for base in actors}
cycle_addr = {base + 0x1A00: base for base in actors}

def rows(path):
    with gzip.open(path, "rt", newline="", encoding="utf-8") as f:
        yield from csv.DictReader(f)

mem_iter = iter(rows(mem_path))
mem = next(mem_iter, None)
calls_path = out_dir / "v77-mt-play-calls.csv.gz"
with gzip.open(calls_path, "wt", newline="", encoding="utf-8") as f:
    fields = ["event_index", "actor", "motion_id", "cycle_field",
              "entry_r3", "phase_r6", "pr", "r1", "out_pp_pointer_r5"]
    writer = csv.DictWriter(f, fieldnames=fields)
    writer.writeheader()
    for pc in rows(pc_path):
        event = int(pc["event_index"])
        while mem is not None and int(mem["event_index"]) <= event:
            addr = int(mem["address"], 16)
            if mem["width"] == "2" and mem["access"] == "R" and addr in motion_addr:
                actors[motion_addr[addr]]["motion_id"] = int(mem["value"], 16) & 0xFFFF
            if mem["width"] == "2" and mem["access"] == "W" and addr in cycle_addr:
                actors[cycle_addr[addr]]["cycle"] = int(mem["value"], 16) & 0xFFFF
            mem = next(mem_iter, None)
        if pc["pc"] != "0x0C09D690":
            continue
        base = int(pc["r4"], 16)
        if base not in actors:
            continue
        state = actors[base]
        motion_id = state["motion_id"]
        cycle = state["cycle"]
        entry_cycle = int(pc["r3"], 16)
        phase = int(pc["r6"], 16)
        state["calls"] += 1
        state["pairs"][(motion_id, cycle)] += 1
        state["phases"].append(phase)
        state["r5"][pc["r5"]] += 1
        if cycle is not None and entry_cycle != cycle:
            state["cycle_mismatches"] += 1
        writer.writerow({
            "event_index": event, "actor": f"0x{base:08X}",
            "motion_id": f"0x{motion_id:03X}" if motion_id is not None else "",
            "cycle_field": cycle if cycle is not None else "",
            "entry_r3": entry_cycle, "phase_r6": phase,
            "pr": pc["pr"], "r1": pc["r1"], "out_pp_pointer_r5": pc["r5"],
        })

summary_path = out_dir / "MT_CALL_SUMMARY.txt"
with summary_path.open("w", encoding="utf-8") as f:
    f.write("VF3 v77 MT-PLAY CALL CORRELATION\n")
    for base, state in actors.items():
        f.write(f"\nACTOR 0x{base:08X}\n")
        f.write(f"entry_calls={state['calls']}\n")
        f.write(f"cycle_register_mismatches={state['cycle_mismatches']}\n")
        if state["phases"]:
            f.write(f"phase_r6_min=0x{min(state['phases']):X}\n")
            f.write(f"phase_r6_max=0x{max(state['phases']):X}\n")
        f.write("MOTION_ID / CYCLE / ENTRY_COUNT\n")
        for (motion_id, cycle), count in state["pairs"].most_common():
            mid = "unknown" if motion_id is None else f"0x{motion_id:03X}"
            f.write(f"{mid} / {cycle if cycle is not None else 'unknown'} / {count}\n")
        f.write("OUT_PP_POINTER_R5\n")
        for pointer, count in state["r5"].most_common():
            f.write(f"{pointer} {count}\n")
print(summary_path.read_text())
