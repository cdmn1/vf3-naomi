#!/usr/bin/env python3
"""Extract final 63-channel MT evaluator targets from a correlated v77 trace.

The call CSV must be produced by analyze_pose_trace_correlation.py. The memory
CSV must retain both actor output ranges and the evaluator/sign-flip PCs.
Writes one row per complete evaluator call, with channel values as IEEE-754
32-bit hex words. Values are taken after the guest's sign-flip stores.
"""
from __future__ import annotations

import bisect
import csv
import gzip
import sys
from collections import Counter, defaultdict
from pathlib import Path

ACTORS = {
    0x0C1FEFE4: "0x0C1FEFE4",
    0x0C20138C: "0x0C20138C",
}
OUTPUT_FIRST = 0x1D04
OUTPUT_END = 0x1E00
CHANNEL_COUNT = 63
RELEVANT_PCS = {0x0C09D7FC, 0x0C09D412, 0x0C09D422}


def open_csv(path: Path):
    if path.suffix == ".gz":
        return gzip.open(path, "rt", newline="", encoding="utf-8")
    return path.open("r", newline="", encoding="utf-8")


def main() -> int:
    if len(sys.argv) != 4:
        print("usage: extract_v77_mt_targets.py CALLS.csv.gz MEMORY.csv.gz OUTPUT.csv",
              file=sys.stderr)
        return 2
    calls_path, memory_path, output_path = map(Path, sys.argv[1:])

    calls_by_actor: dict[int, list[dict[str, str]]] = {
        actor: [] for actor in ACTORS
    }
    with open_csv(calls_path) as f:
        for row in csv.DictReader(f):
            actor = int(row["actor"], 16)
            if actor in calls_by_actor:
                row["_event"] = str(int(row["event_index"]))
                calls_by_actor[actor].append(row)
    for rows in calls_by_actor.values():
        rows.sort(key=lambda row: int(row["_event"]))
    indexes = {
        actor: [int(row["_event"]) for row in rows]
        for actor, rows in calls_by_actor.items()
    }

    # The main evaluator writes all 63 floats at 0x0C09D7FC. Its two later
    # sign-flip passes overwrite 7 + 14 selected floats. Retain the last write
    # to each address within the same actor/call interval.
    values: dict[tuple[int, int], dict[int, tuple[int, int, int]]] = defaultdict(dict)
    with open_csv(memory_path) as f:
        for row in csv.DictReader(f):
            if row["access"] != "W" or int(row["width"]) != 4:
                continue
            pc = int(row["pc"], 16)
            if pc not in RELEVANT_PCS:
                continue
            address = int(row["address"], 16)
            actor = next((base for base in ACTORS
                          if base + OUTPUT_FIRST <= address < base + OUTPUT_END), None)
            if actor is None:
                continue
            event = int(row["event_index"])
            call_index = bisect.bisect_right(indexes[actor], event) - 1
            if call_index < 0:
                continue
            value = int(row["value"], 16) & 0xFFFFFFFF
            previous = values[(actor, call_index)].get(address)
            if previous is None or event > previous[0]:
                values[(actor, call_index)][address] = (event, pc, value)

    fieldnames = ["event_index", "actor", "motion_id", "cycle", "phase_q8",
                  "out_pp_pointer_r5", "channel_write_count",
                  "post_signflip_writes"]
    fieldnames += [f"ch{i:02d}_bits" for i in range(CHANNEL_COUNT)]
    complete = Counter()
    incomplete = Counter()
    with output_path.open("w", newline="", encoding="utf-8") as out:
        writer = csv.DictWriter(out, fieldnames=fieldnames)
        writer.writeheader()
        for actor, rows in calls_by_actor.items():
            base = actor
            for call_index, call in enumerate(rows):
                by_address = values.get((actor, call_index), {})
                if len(by_address) != CHANNEL_COUNT:
                    incomplete[actor] += 1
                    continue
                complete[actor] += 1
                row = {
                    "event_index": call["event_index"],
                    "actor": ACTORS[actor],
                    "motion_id": call["motion_id"],
                    "cycle": call["cycle_field"],
                    "phase_q8": call["phase_r6"],
                    "out_pp_pointer_r5": call["out_pp_pointer_r5"],
                    "channel_write_count": len(by_address),
                    "post_signflip_writes": sum(
                        1 for channel in range(CHANNEL_COUNT)
                        if by_address[base + OUTPUT_FIRST + channel * 4][1]
                        in (0x0C09D412, 0x0C09D422)
                    ),
                }
                for channel in range(CHANNEL_COUNT):
                    address = base + OUTPUT_FIRST + channel * 4
                    row[f"ch{channel:02d}_bits"] = f"0x{by_address[address][2]:08X}"
                writer.writerow(row)

    print("VF3 v77 FINAL MT TARGET EXTRACTION")
    for actor in ACTORS:
        print(f"{ACTORS[actor]} complete_calls={complete[actor]} "
              f"incomplete_calls={incomplete[actor]}")
    print(f"output={output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
