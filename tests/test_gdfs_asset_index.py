#!/usr/bin/env python3
"""Tests for the checked-in GDFS asset-index validator."""
from __future__ import annotations

import csv
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from validate_gdfs_asset_index import validate_file, validate_rows  # noqa: E402


class GdfsAssetIndexTests(unittest.TestCase):
    def test_repository_asset_index_passes(self) -> None:
        errors, count = validate_file(ROOT / "docs" / "GDFS_ASSET_INDEX.csv")
        self.assertEqual(count, 232)
        self.assertEqual(errors, [])

    def test_detects_overlapping_core_assets(self) -> None:
        with (ROOT / "docs" / "GDFS_ASSET_INDEX.csv").open(newline="", encoding="utf-8") as f:
            rows = list(csv.DictReader(f))
        core = [row for row in rows if row["order_class"] == "core-lba-order"]
        first_lba = int(core[0]["gd_lba"])
        core[1]["gd_lba"] = str(first_lba + 100)
        errors = validate_rows(rows)
        self.assertTrue(any("overlap" in error for error in errors), errors)

    def test_detects_malformed_manifest_hash(self) -> None:
        with (ROOT / "docs" / "GDFS_ASSET_INDEX.csv").open(newline="", encoding="utf-8") as f:
            rows = list(csv.DictReader(f))
        rows[0]["manifest_sha1"] = "not-a-sha1"
        errors = validate_rows(rows)
        self.assertTrue(any("invalid manifest_sha1" in error for error in errors), errors)


if __name__ == "__main__":
    unittest.main()
