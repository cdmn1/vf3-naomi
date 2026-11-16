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
                w.writerow(dict(word_index="0", pc="0x8C02DCEC", r13="0", r14="0", r4="0", r5="0", r6="0", r7="0"))
                w.writerow(dict(word_index="8", pc="0x8C0360C8", r13="0", r14="0", r4="0", r5="0", r6="0", r7="0"))
            with mem.open("w", newline="") as f:
                w = csv.DictWriter(f, fieldnames=["word_index", "pc", "access", "width", "address", "value"])
                w.writeheader()
                w.writerow(dict(word_index="16", pc="0x8C02DCEC", access="R", width="4", address="0x8C1069BC", value="0"))
                w.writerow(dict(word_index="20", pc="0x8C08232E", access="W", width="4", address="0x8C5F4100", value="0"))
                w.writerow(dict(word_index="24", pc="0x8C0360C8", access="R", width="4", address="0x8C100000", value="0"))
            report = summarize(pc, mem)
        self.assertIn("mt_loader            0x8C02DCEC: 1", report)
        self.assertIn("runtime pointer table: R=1 W=0", report)
        self.assertIn("resident MT window: R=0 W=1", report)
        self.assertIn("0x8C08232E: 1", report)
        self.assertIn("outside watched ranges: R=1 W=0", report)


if __name__ == "__main__":
    unittest.main()
