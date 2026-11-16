#!/usr/bin/env python3
"""Validate the checked-in VF3 GDFS asset index and its inventory invariants."""
from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path

EXPECTED_ROWS = 232
EXPECTED_MT_FILES = 91
EXPECTED_MT_BYTES = 154_676_388
EXPECTED_CORE_ROWS = 225
SECTOR_BYTES = 2048
REQUIRED_COLUMNS = {
    "table_row_0based", "core_order_position", "asset_name", "embedded_name",
    "tag_candidate", "tag_status", "order_class", "gd_lba", "size_bytes",
    "manifest_sha1",
}


def validate_rows(rows: list[dict[str, str]]) -> list[str]:
    errors: list[str] = []
    if not rows:
        return ["asset index is empty"]

    missing = REQUIRED_COLUMNS - set(rows[0])
    if missing:
        return [f"missing required columns: {', '.join(sorted(missing))}"]

    if len(rows) != EXPECTED_ROWS:
        errors.append(f"expected {EXPECTED_ROWS} rows, found {len(rows)}")

    names = [row["asset_name"] for row in rows]
    if len(set(names)) != len(names):
        errors.append("asset_name values are not unique")

    for row_no, row in enumerate(rows, start=2):
        if not row["asset_name"]:
            errors.append(f"row {row_no}: empty asset_name")
        if not row["embedded_name"]:
            errors.append(f"row {row_no}: empty embedded_name")
        if not row["manifest_sha1"] or len(row["manifest_sha1"]) != 40 or any(
            c not in "0123456789abcdefABCDEF" for c in row["manifest_sha1"]
        ):
            errors.append(f"row {row_no}: invalid manifest_sha1")

        for field in ("table_row_0based", "gd_lba", "size_bytes"):
            try:
                value = int(row[field], 10)
            except (TypeError, ValueError):
                errors.append(f"row {row_no}: invalid {field}")
                continue
            if value < 0 or (field == "size_bytes" and value == 0):
                errors.append(f"row {row_no}: out-of-range {field}")

        if row["core_order_position"]:
            try:
                if int(row["core_order_position"], 10) < 1:
                    raise ValueError
            except ValueError:
                errors.append(f"row {row_no}: invalid core_order_position")

    mt_rows = [row for row in rows if row["asset_name"].upper().startswith("MT")
               and row["asset_name"].upper().endswith(".BIN")]
    mt_bytes = 0
    for row in mt_rows:
        try:
            mt_bytes += int(row["size_bytes"], 10)
        except (TypeError, ValueError):
            pass
    if len(mt_rows) != EXPECTED_MT_FILES:
        errors.append(f"expected {EXPECTED_MT_FILES} MT packs, found {len(mt_rows)}")
    if len({row["asset_name"] for row in mt_rows}) != len(mt_rows):
        errors.append("MT pack filenames are not unique")
    if mt_bytes != EXPECTED_MT_BYTES:
        errors.append(f"expected {EXPECTED_MT_BYTES} MT bytes, found {mt_bytes}")

    core = [row for row in rows if row["order_class"] == "core-lba-order"]
    if len(core) != EXPECTED_CORE_ROWS:
        errors.append(f"expected {EXPECTED_CORE_ROWS} core-order rows, found {len(core)}")

    previous_lba = None
    previous_end = None
    for row in core:
        try:
            lba = int(row["gd_lba"], 10)
            size = int(row["size_bytes"], 10)
        except (TypeError, ValueError):
            continue
        if previous_lba is not None and lba <= previous_lba:
            errors.append(f"core-order LBAs are not strictly ascending at {row['asset_name']}")
        if previous_end is not None and lba < previous_end:
            errors.append(f"core-order assets overlap at {row['asset_name']}")
        previous_lba = lba
        previous_end = lba + math.ceil(size / SECTOR_BYTES)

    return errors


def validate_file(path: Path) -> tuple[list[str], int]:
    try:
        with path.open(newline="", encoding="utf-8") as handle:
            rows = list(csv.DictReader(handle))
    except OSError as exc:
        return [f"cannot read {path}: {exc}"], 0
    return validate_rows(rows), len(rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv_path", type=Path, nargs="?",
                        default=Path("docs/GDFS_ASSET_INDEX.csv"))
    args = parser.parse_args()
    errors, count = validate_file(args.csv_path)
    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        return 1
    print(f"OK: {count} asset rows; {EXPECTED_MT_FILES} MT packs "
          f"({EXPECTED_MT_BYTES:,} bytes); {EXPECTED_CORE_ROWS} ordered, non-overlapping core assets.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
