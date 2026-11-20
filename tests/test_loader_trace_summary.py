#!/usr/bin/env python3
"""Tests for the VF3 decoded loader-trace summarizer."""
from __future__ import annotations

import csv
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from loader_trace_summary import summarize  # noqa: E402


class LoaderTraceSummaryTests(unittest.TestCase):
    def test_groups_pc_hits_and_memory_ranges(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            pc = Path(tmp) / "trace.pc.csv"
            mem = Path(tmp) / "trace.memory.csv"
            with pc.open("w", newline="") as f:
                w = csv.DictWriter(f, fieldnames=["word_index", "pc", "r13", "r14", "r4", "r5", "r6", "r7"])
                w.writeheader()
                w.writerow(dict(word_index="0", pc="0x8C02DCEC", r13="0x111", r14="0x112", r4="0x114", r5="0x115", r6="0x116", r7="0x117"))
                w.writerow(dict(word_index="8", pc="0x8C0360C8", r13="0", r14="0", r4="0", r5="0", r6="0", r7="0"))
            with mem.open("w", newline="") as f:
                w = csv.DictWriter(f, fieldnames=["word_index", "pc", "access", "width", "address", "value"])
                w.writeheader()
                w.writerow(dict(word_index="16", pc="0x8C02DCEC", access="R", width="4", address="0x8C1069BC", value="0x8C5F4100"))
                w.writerow(dict(word_index="20", pc="0x8C08232E", access="W", width="4", address="0x8C5F4100", value="0x12345678"))
                w.writerow(dict(word_index="24", pc="0x8C0360C8", access="R", width="4", address="0x8C100000", value="0"))
            report = summarize(pc, mem)
        self.assertIn("mt_loader            0x8C02DCEC: 1", report)
        self.assertIn("runtime pointer table: R=1 W=0", report)
        self.assertIn("resident MT window: R=0 W=1", report)
        self.assertIn("0x8C08232E: 1", report)
        self.assertIn("outside watched ranges: R=1 W=0", report)
        self.assertIn("r13=0x111", report)
        self.assertIn("R4 0x8C1069BC = 0x8C5F4100", report)
        self.assertIn("W4 0x8C5F4100 = 0x12345678", report)

    def test_reports_new_object_loader_probes_and_indirect_call_registers(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            pc = Path(tmp) / "trace.pc.csv"
            mem = Path(tmp) / "trace.memory.csv"
            fields = ["word_index", "pc", "pr"] + [f"r{i}" for i in range(16)]
            with pc.open("w", newline="") as f:
                w = csv.DictWriter(f, fieldnames=fields)
                w.writeheader()
                row = {key: "0" for key in fields}
                row.update(word_index="0", pc="0x0C0A2FDE", pr="0x8C0A3100",
                           r1="0x8C0360C8", r4="0x8C1069BC", r5="0x00000014")
                w.writerow(row)
                row2 = {key: "0" for key in fields}
                row2.update(word_index="19", pc="0xAC0A7F84", pr="0x8C0A8000",
                            r1="0x8C1069BC", r4="0x00000014", r5="0x8C0D65D4")
                w.writerow(row2)
            with mem.open("w", newline="") as f:
                w = csv.DictWriter(f, fieldnames=["word_index", "pc", "access", "width", "address", "value"])
                w.writeheader()
            report = summarize(pc, mem)
        self.assertIn("obj_load_indirect_call 0x8C0A2FDE: 1", report)
        self.assertRegex(report, r"table_entry_load +0x8C0A7F84: 1")
        self.assertIn("r1=0x8C0360C8", report)
        self.assertIn("r4=0x8C1069BC", report)
        self.assertIn("r5=0x00000014", report)

    def test_reports_r9_context_pointer_candidates_without_actor_claim(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            pc = Path(tmp) / "trace.pc.csv"
            mem = Path(tmp) / "trace.memory.csv"
            fields = ["word_index", "pc", "r9"]
            with pc.open("w", newline="") as f:
                w = csv.DictWriter(f, fieldnames=fields)
                w.writeheader()
                w.writerow(dict(word_index="0", pc="0x8C09D5AE", r9="0x0C29BCC4"))
                w.writerow(dict(word_index="8", pc="0x8C09D690", r9="0x0C29BCC4"))
                w.writerow(dict(word_index="16", pc="0x8C09D690", r9="0x0C29B864"))
            with mem.open("w", newline="") as f:
                w = csv.DictWriter(f, fieldnames=["word_index", "pc", "access", "width", "address", "value"])
                w.writeheader()
            report = summarize(pc, mem)
        self.assertIn("R9 context-pointer candidates (not actor identities)", report)
        self.assertIn("0x0C29BCC4 (1)", report)
        self.assertIn("0x0C29BCC4 (1), 0x0C29B864 (1)", report)
        self.assertIn("do not establish actor ownership", report)

    def test_normalizes_interpreter_and_p2_aliases(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            pc = Path(tmp) / "trace.pc.csv"
            mem = Path(tmp) / "trace.memory.csv"
            with pc.open("w", newline="") as f:
                w = csv.DictWriter(f, fieldnames=["word_index", "pc", "r13", "r14", "r4", "r5", "r6", "r7"])
                w.writeheader()
                w.writerow(dict(word_index="0", pc="0x0C02DCEC", r13="0x111", r14="0x112", r4="0x114", r5="0x115", r6="0x116", r7="0x117"))
            with mem.open("w", newline="") as f:
                w = csv.DictWriter(f, fieldnames=["word_index", "pc", "access", "width", "address", "value"])
                w.writeheader()
                w.writerow(dict(word_index="16", pc="0x0C02DCEC", access="R", width="4", address="0x0C1069BC", value="0x0C5F4100"))
                w.writerow(dict(word_index="20", pc="0xAC04BDA2", access="W", width="4", address="0xAC5F4100", value="0x12345678"))
            report = summarize(pc, mem)
        self.assertIn("mt_loader            0x8C02DCEC: 1", report)
        self.assertIn("runtime pointer table: R=1 W=0", report)
        self.assertIn("resident MT window: R=0 W=1", report)
        self.assertIn("R4 0x8C1069BC = 0x0C5F4100", report)
        self.assertIn("W4 0x8C5F4100 = 0x12345678  from 0x8C04BDA2", report)


if __name__ == "__main__":
    unittest.main()
