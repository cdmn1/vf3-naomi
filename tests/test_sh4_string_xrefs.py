#!/usr/bin/env python3
"""Synthetic tests for SH-4 source-path literal xref scanning."""
from __future__ import annotations

import struct
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from sh4_string_xrefs import find_string_xrefs, pointer_aliases  # noqa: E402


class Sh4StringXrefTests(unittest.TestCase):
    def test_finds_pc_relative_movl_using_p1_pointer(self) -> None:
        base = 0x8C010000
        data = bytearray(0x300)
        code_off = 0x100
        literal_off = 0x184
        target_off = 0x220
        opcode = 0xD300 | 0x20  # MOV.L @(0x20,PC),R3
        struct.pack_into("<H", data, code_off, opcode)
        struct.pack_into("<I", data, literal_off, base + target_off)
        data[target_off:target_off + 5] = b"abc.c"

        hits = find_string_xrefs(bytes(data), base, b"abc.c")
        self.assertEqual(hits, [(code_off, literal_off, base + target_off, 3)])

    def test_accepts_uncached_physical_alias(self) -> None:
        base = 0x8C010000
        data = bytearray(0x300)
        code_off = 0x100
        literal_off = 0x184
        target_off = 0x220
        opcode = 0xD300 | 0x20
        struct.pack_into("<H", data, code_off, opcode)
        struct.pack_into("<I", data, literal_off, (base + target_off) & 0x1FFFFFFF)
        data[target_off:target_off + 5] = b"abc.c"

        hits = find_string_xrefs(bytes(data), base, b"abc.c")
        self.assertEqual(len(hits), 1)
        self.assertEqual(hits[0][0], code_off)

    def test_no_match_for_unreferenced_string(self) -> None:
        data = bytearray(0x300)
        data[0x220:0x225] = b"abc.c"
        self.assertEqual(find_string_xrefs(bytes(data), 0x8C010000, b"abc.c"), [])

    def test_pointer_aliases_cover_cached_and_uncached_forms(self) -> None:
        aliases = pointer_aliases(0x8C0D9EA8)
        self.assertIn(0x0C0D9EA8, aliases)
        self.assertIn(0x8C0D9EA8, aliases)
        self.assertIn(0xAC0D9EA8, aliases)


if __name__ == "__main__":
    unittest.main()
