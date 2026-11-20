from __future__ import annotations

import csv
from pathlib import Path
import struct
import tempfile
import unittest

from tools.loader_trace_asset_map import (
    MARKER, correlate, decode_transfers, load_assets, make_report,
)


class LoaderTraceAssetMapTests(unittest.TestCase):
    def test_decode_fa22_record(self) -> None:
        words = [
            MARKER, 0x0C5F0000, 459609, 5, 2, 5, 1234, 0xAABBCCDD,
            (0x11223344 << 32) | 0x55667788,
        ]
        rows = decode_transfers(struct.pack("<9Q", *words))
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["destination"], 0x0C5F0000)
        self.assertEqual(rows[0]["fad"], 459609)
        self.assertEqual(rows[0]["sectors"], 5)
        self.assertEqual(rows[0]["first_word"], 0x11223344)
        self.assertEqual(rows[0]["last_word"], 0x55667788)

    def test_truncated_fa22_record_is_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "truncated FA22"):
            decode_transfers(struct.pack("<2Q", MARKER, 0x0C5F0000))

    def test_correlates_motion_pack_extents_and_destinations(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            index = Path(temp) / "assets.csv"
            with index.open("w", newline="", encoding="utf-8") as handle:
                writer = csv.DictWriter(handle, fieldnames=["asset_name", "gd_lba", "size_bytes"])
                writer.writeheader()
                writer.writerow({"asset_name": "MTAKIPAI.BIN", "gd_lba": 459459, "size_bytes": 1572408})
                writer.writerow({"asset_name": "MTAKIWOL.BIN", "gd_lba": 463527, "size_bytes": 1696108})
            assets = load_assets(index)

        words = []
        for destination, fad in ((0x0C5F0000, 459609), (0x0C828000, 463677)):
            words.extend([
                MARKER, destination, fad, 5, 2, 5, 1000, 0x12345678,
                (0x01020304 << 32) | 0x05060708,
            ])
        rows = correlate(decode_transfers(struct.pack(f"<{len(words)}Q", *words)), assets)
        self.assertEqual([row["asset"] for row in rows], ["MTAKIPAI.BIN", "MTAKIWOL.BIN"])
        self.assertEqual([row["extent_match"] for row in rows], ["exact", "exact"])
        self.assertEqual([row["start_lba"] for row in rows], [459459, 463527])
        report = make_report(rows)
        self.assertIn("MTAKIPAI.BIN", report)
        self.assertIn("0x0C5F0000", report)

    def test_transfer_crossing_asset_boundary_is_marked_overlap(self) -> None:
        assets = [
            {"name": "A.BIN", "start_lba": 100, "end_lba": 102},
            {"name": "B.BIN", "start_lba": 102, "end_lba": 104},
        ]
        row = {"fad": 250, "sectors": 4, "destination": 0x0C100000,
               "flags": 2, "result": 4, "nonzero_bytes": 0, "fnv1a": 0,
               "first_word": 0, "last_word": 0, "word_index": 0}
        mapped = correlate([row], assets)
        self.assertEqual(mapped[0]["asset"], "A.BIN;B.BIN")
        self.assertEqual(mapped[0]["extent_match"], "overlap")


if __name__ == "__main__":
    unittest.main()
