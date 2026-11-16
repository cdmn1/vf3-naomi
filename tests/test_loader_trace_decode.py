#!/usr/bin/env python3
"""Unit tests for the mixed PC-probe / memory trace decoder."""
from __future__ import annotations

import struct
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from loader_trace_decode import decode  # noqa: E402


def qwords(*values: int) -> bytes:
    return struct.pack(f"<{len(values)}Q", *values)


class LoaderTraceDecodeTests(unittest.TestCase):
    def test_decodes_interleaved_pc_and_memory_groups(self) -> None:
        pc = 0x8C02DCEC
        words = [
            (pc << 16) | 0xFA10,
            0x13, 0x14, 0x4, 0x5, 0x6, 0x7,
            (pc << 16) | 0xFA11,
            (0x8C0360C8 << 16) | 0xFA20,
            (4 << 48) | (1 << 47),
            0x8C1069BC,
            0x12345678,
        ]
        with tempfile.TemporaryDirectory() as tmp:
            trace = Path(tmp) / "mixed.bin"
            trace.write_bytes(qwords(*words))
            probes, memory, warnings = decode(trace)

        self.assertEqual(warnings, [])
        self.assertEqual(len(probes), 1)
        self.assertEqual(probes[0]["pc"], "0x8C02DCEC")
        self.assertEqual(probes[0]["r13"], "0x0000000000000013")
        self.assertEqual(probes[0]["r7"], "0x0000000000000007")
        self.assertEqual(len(memory), 1)
        self.assertEqual(memory[0]["pc"], "0x8C0360C8")
        self.assertEqual(memory[0]["access"], "W")
        self.assertEqual(memory[0]["width"], "4")
        self.assertEqual(memory[0]["address"], "0x8C1069BC")
        self.assertEqual(memory[0]["value"], "0x12345678")

    def test_skips_unknown_words_and_reports_truncated_records(self) -> None:
        pc = 0x8C02DCEC
        words = [0x123456789ABCDEF0, (pc << 16) | 0xFA10, 1, 2]
        with tempfile.TemporaryDirectory() as tmp:
            trace = Path(tmp) / "truncated.bin"
            trace.write_bytes(qwords(*words))
            probes, memory, warnings = decode(trace)

        self.assertEqual(probes, [])
        self.assertEqual(memory, [])
        self.assertEqual(len(warnings), 1)
        self.assertIn("truncated PC probe", warnings[0])

    def test_rejects_non_u64_aligned_stream(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            trace = Path(tmp) / "bad.bin"
            trace.write_bytes(b"not-eight")
            with self.assertRaisesRegex(RuntimeError, "not divisible by 8"):
                decode(trace)


if __name__ == "__main__":
    unittest.main()
