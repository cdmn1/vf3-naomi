from __future__ import annotations

import csv
from pathlib import Path
import struct
import tempfile
import unittest

from tools.loader_trace_asset_map import (
    FA20, FA22, correlate, correlate_motion_memory, decode_trace, load_assets, group_mt_motion_rows, make_report, summarize_mt_slot_lookups, summarize_motion_stream_use,
)


class LoaderTraceAssetMapTests(unittest.TestCase):
    def test_decode_fa22_and_fa20_records(self) -> None:
        words = [
            FA22, 0x0C5F0000, 459609, 5, 2, 5, 1234, 0xAABBCCDD,
            (0x11223344 << 32) | 0x55667788,
            (0x8C001000 << 16) | FA20, (4 << 48), 0x8C5F0000, 0x12345678,
        ]
        transfers, memory, count = decode_trace(struct.pack(f"<{len(words)}Q", *words))
        self.assertEqual(count, 13)
        self.assertEqual(len(transfers), 1)
        self.assertEqual(transfers[0]["destination"], 0x0C5F0000)
        self.assertEqual(transfers[0]["fad"], 459609)
        self.assertEqual(transfers[0]["sectors"], 5)
        self.assertEqual(transfers[0]["first_word"], 0x11223344)
        self.assertEqual(transfers[0]["last_word"], 0x55667788)
        self.assertEqual(len(memory), 1)
        self.assertEqual(memory[0]["pc"], 0x8C001000)
        self.assertEqual(memory[0]["access"], "R")
        self.assertEqual(memory[0]["width"], 4)
        self.assertEqual(memory[0]["address"], 0x8C5F0000)

    def test_truncated_fa22_record_is_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "truncated FA22"):
            decode_trace(struct.pack("<2Q", FA22, 0x0C5F0000))

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
                FA22, destination, fad, 5, 2, 5, 1000, 0x12345678,
                (0x01020304 << 32) | 0x05060708,
            ])
        transfers, memory, word_count = decode_trace(struct.pack(f"<{len(words)}Q", *words))
        rows = correlate(transfers, assets)
        self.assertEqual([row["asset"] for row in rows], ["MTAKIPAI.BIN", "MTAKIWOL.BIN"])
        self.assertEqual([row["extent_match"] for row in rows], ["exact", "exact"])
        self.assertEqual([row["start_lba"] for row in rows], [459459, 463527])
        report = make_report(rows, version="v68")
        self.assertIn("# VF3 v68 REIOS HLE asset correlation", report)
        self.assertNotIn("VF3 v66", report)
        self.assertIn("MTAKIPAI.BIN", report)
        self.assertIn("0x0C5F0000", report)


    def test_maps_mt_slot_table_reads_to_slot_ids(self) -> None:
        rows = [
            {"asset": "MTAKIPAI.BIN", "destination": 0x0C5F4000,
             "sectors": 5, "word_index": 0, "asset_size_bytes": 1_572_408},
        ]
        memory = [
            {"pack": "MTAKIPAI.BIN", "pc": 0x0C09D5D2, "access": "R",
             "width": 4, "address": 0x0C5F41BC, "value": 0x98DD8},
            {"pack": "MTAKIPAI.BIN", "pc": 0x0C09DCE0, "access": "R",
             "width": 4, "address": 0x0C5F41BC, "value": 0x98DD8},
            {"pack": "MTAKIPAI.BIN", "pc": 0x0C09D5CE, "access": "R",
             "width": 4, "address": 0x0C5F41C0, "value": 0x98DF4},
        ]
        lookups = summarize_mt_slot_lookups(rows, memory)
        self.assertEqual(
            [(row["slot"], row["file_offset"], row["reads"]) for row in lookups],
            [(111, 0x98DD8, 1), (112, 0x98DF4, 1)],
        )

    def test_groups_three_mt_slots_into_one_based_motion_id(self) -> None:
        lookups = [
            {"pack": "MTAKIPAI.BIN", "slot": 111, "file_offset": 0x1A028, "reads": 2, "pcs": ""},
            {"pack": "MTAKIPAI.BIN", "slot": 112, "file_offset": 0x1A040, "reads": 2, "pcs": ""},
            {"pack": "MTAKIPAI.BIN", "slot": 113, "file_offset": 0x1A048, "reads": 2, "pcs": ""},
        ]
        rows = group_mt_motion_rows(lookups)
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["motion_id"], 38)
        self.assertEqual(rows[0]["slot_start"], 111)
        self.assertEqual(
            [rows[0][f"offset{i}"] for i in range(3)],
            [0x1A028, 0x1A040, 0x1A048],
        )
        self.assertEqual(rows[0]["complete"], 1)

    def test_matches_evaluator_reads_to_motion_stream_source_offsets(self) -> None:
        rows = [
            {"asset": "MTAKIPAI.BIN", "destination": 0x0C5F4000,
             "sectors": 5, "word_index": 0},
        ]
        motion_rows = [
            {"pack": "MTAKIPAI.BIN", "motion_id": 38,
             "offset0": 0x1A028, "offset1": 0x1A040, "offset2": 0x1A048,
             "complete": 1},
        ]
        base = 0x0C5F4000
        memory = [
            {"pack": "MTAKIPAI.BIN", "pc": 0x0C09D6F4, "access": "R",
             "address": base + 0x1A03A},
            {"pack": "MTAKIPAI.BIN", "pc": 0x0C09D70A, "access": "R",
             "address": base + 0x1A040},
            {"pack": "MTAKIPAI.BIN", "pc": 0x0C09D794, "access": "R",
             "address": base + 0x1A048},
            {"pack": "MTAKIPAI.BIN", "pc": 0x0C09D778, "access": "R",
             "address": base + 0x1A04C},
            {"pack": "MTAKIPAI.BIN", "pc": 0x0C09D77E, "access": "R",
             "address": base + 0x1A050},
            {"pack": "MTAKIPAI.BIN", "pc": 0x0C09D782, "access": "R",
             "address": base + 0x1A054},
        ]
        result = summarize_motion_stream_use(rows, memory, motion_rows)
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0]["count_reads"], 1)
        self.assertEqual(result[0]["time_reads"], 1)
        self.assertEqual(result[0]["tuple_prefix_reads"], 4)
        self.assertEqual(result[0]["tuple_lanes"], 4)

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

    def test_motion_memory_window_excludes_load_and_next_overwrite(self) -> None:
        transfers = [
            {"asset": "MTAKIWOL.BIN", "word_index": 10, "destination": 0x0C5F4000,
             "sectors": 5, "fad": 463677, "start_lba": 463527,
             "end_lba_exclusive": 463532, "flags": 2, "result": 5,
             "nonzero_bytes": 100, "fnv1a": 1, "first_word": 0, "last_word": 0,
             "extent_match": "exact"},
            {"asset": "MTAKIPAI.BIN", "word_index": 100, "destination": 0x0C5F4000,
             "sectors": 5, "fad": 459609, "start_lba": 459459,
             "end_lba_exclusive": 459464, "flags": 2, "result": 5,
             "nonzero_bytes": 100, "fnv1a": 1, "first_word": 0, "last_word": 0,
             "extent_match": "exact"},
        ]
        memory = [
            {"word_index": 20, "pc": 0x8C001000, "access": "R", "width": 4,
             "address": 0x0C5F4000, "value": 1},
            {"word_index": 30, "pc": 0x8C002000, "access": "W", "width": 4,
             "address": 0x8C5F4004, "value": 2},
            {"word_index": 110, "pc": 0x8C003000, "access": "R", "width": 2,
             "address": 0x8C5F4008, "value": 3},
        ]
        events = correlate_motion_memory(transfers, memory, 200)
        self.assertEqual([row["pack"] for row in events],
                         ["MTAKIWOL.BIN", "MTAKIWOL.BIN", "MTAKIPAI.BIN"])
        self.assertEqual([row["canonical_address"] for row in events],
                         [0x8C5F4000, 0x8C5F4004, 0x8C5F4008])


if __name__ == "__main__":
    unittest.main()
