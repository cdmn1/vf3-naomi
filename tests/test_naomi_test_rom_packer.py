#!/usr/bin/env python3
"""Regression tests for the Naomi dual-entry ROM packer."""
from __future__ import annotations

import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PACKER = ROOT / "tests" / "naomi_mie_probe" / "combine_naomi_test_rom.py"
HEADER_SIZE = 0x1000


def make_rom(*, segment_offset: int, load_address: int, segment_size: int,
             entry: int, fill: int) -> bytes:
    rom = bytearray(HEADER_SIZE + max(segment_offset + segment_size - HEADER_SIZE, 0))
    struct.pack_into("<III", rom, 0x360, segment_offset, load_address, segment_size)
    struct.pack_into("<I", rom, 0x420, entry)
    if segment_offset + segment_size > len(rom):
        rom.extend(b"\0" * (segment_offset + segment_size - len(rom)))
    rom[segment_offset:segment_offset + segment_size] = bytes([fill]) * segment_size
    return bytes(rom)


class NaomiTestRomPackerTests(unittest.TestCase):
    def run_packer(self, main_rom: bytes, test_rom: bytes):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            main_path, test_path, out_path = root / "main.bin", root / "test.bin", root / "out.bin"
            main_path.write_bytes(main_rom)
            test_path.write_bytes(test_rom)
            result = subprocess.run(
                [sys.executable, str(PACKER), str(main_path), str(test_path), str(out_path)],
                capture_output=True, text=True,
            )
            output = out_path.read_bytes() if out_path.exists() else None
            return result, output

    def test_packs_distinct_test_segment_and_entry(self) -> None:
        main = make_rom(segment_offset=0x1000, load_address=0x8C020000,
                        segment_size=0x20, entry=0x8C020000, fill=0x11)
        test = make_rom(segment_offset=0x1000, load_address=0x8C030000,
                        segment_size=0x30, entry=0x8C030010, fill=0xA5)
        result, output = self.run_packer(main, test)
        self.assertEqual(result.returncode, 0, result.stderr)
        assert output is not None

        test_file_offset, test_load_address, test_size = struct.unpack_from("<III", output, 0x3C0)
        test_entry = struct.unpack_from("<I", output, 0x424)[0]
        self.assertEqual(test_file_offset % 0x1000, 0)
        self.assertEqual(test_load_address, 0x8C030000)
        self.assertEqual(test_size, 0x30)
        self.assertEqual(test_entry, 0x8C030010)
        self.assertEqual(output[test_file_offset:test_file_offset + test_size], bytes([0xA5]) * 0x30)
        for index in range(1, 8):
            self.assertEqual(struct.unpack_from("<III", output, 0x3C0 + index * 12),
                             (0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF))

    def test_rejects_missing_test_segment(self) -> None:
        main = make_rom(segment_offset=0x1000, load_address=0x8C020000,
                        segment_size=0x20, entry=0x8C020000, fill=0x11)
        test = bytearray(make_rom(segment_offset=0x1000, load_address=0x8C030000,
                                  segment_size=0x30, entry=0x8C030010, fill=0xA5))
        struct.pack_into("<I", test, 0x360, 0xFFFFFFFF)
        result, output = self.run_packer(main, bytes(test))
        self.assertNotEqual(result.returncode, 0)
        self.assertIsNone(output)
        self.assertIn("missing its first load segment", result.stderr)

    def test_rejects_truncated_test_payload(self) -> None:
        main = make_rom(segment_offset=0x1000, load_address=0x8C020000,
                        segment_size=0x20, entry=0x8C020000, fill=0x11)
        test = bytearray(HEADER_SIZE)
        struct.pack_into("<III", test, 0x360, 0x2000, 0x8C030000, 0x30)
        struct.pack_into("<I", test, 0x420, 0x8C030010)
        result, output = self.run_packer(main, bytes(test))
        self.assertNotEqual(result.returncode, 0)
        self.assertIsNone(output)
        self.assertIn("payload is shorter", result.stderr)


if __name__ == "__main__":
    unittest.main()
