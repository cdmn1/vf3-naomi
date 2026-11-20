#!/usr/bin/env python3
"""Correlate REIOS HLE FA22 sector-transfer records with the VF3 GDFS asset index."""
from __future__ import annotations

import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path
import struct

MARKER = 0xFA22
WORD_BYTES = 8
RECORD_WORDS = 9
SECTOR_BYTES = 2048
FAD_LBA_OFFSET = 150


def decode_transfers(data: bytes) -> list[dict[str, int]]:
    """Decode FA22 records from the mixed little-endian u64 trace stream."""
    if len(data) % WORD_BYTES:
        raise ValueError(f"trace size {len(data)} is not divisible by 8 bytes")
    words = struct.unpack(f"<{len(data) // WORD_BYTES}Q", data) if data else ()
    transfers: list[dict[str, int]] = []
    i = 0
    while i < len(words):
        if words[i] != MARKER:
            i += 1
            continue
        if i + RECORD_WORDS > len(words):
            raise ValueError(f"truncated FA22 record at u64 word {i}")
        destination, fad, sectors, flags, result, nonzero, fnv1a, endpoints = words[i + 1:i + 9]
        transfers.append({
            "word_index": i,
            "destination": destination & 0xFFFFFFFF,
            "fad": fad & 0xFFFFFFFF,
            "sectors": sectors & 0xFFFFFFFF,
            "flags": flags & 0xFFFFFFFF,
            "result": result & 0xFFFFFFFF,
            "nonzero_bytes": nonzero,
            "fnv1a": fnv1a & 0xFFFFFFFF,
            "first_word": (endpoints >> 32) & 0xFFFFFFFF,
            "last_word": endpoints & 0xFFFFFFFF,
        })
        i += RECORD_WORDS
    return transfers


def load_assets(path: Path) -> list[dict[str, int | str]]:
    with path.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    required = {"asset_name", "gd_lba", "size_bytes"}
    if not rows or required - set(rows[0]):
        raise ValueError(f"asset index must contain columns: {', '.join(sorted(required))}")
    assets = []
    for row in rows:
        lba = int(row["gd_lba"])
        size = int(row["size_bytes"])
        assets.append({
            "name": row["asset_name"],
            "start_lba": lba,
            "end_lba": lba + math.ceil(size / SECTOR_BYTES),
        })
    return assets


def correlate(transfers: list[dict[str, int]],
              assets: list[dict[str, int | str]]) -> list[dict[str, int | str]]:
    """Map each transfer's FAD range to overlapping indexed asset extents."""
    result = []
    for transfer in transfers:
        start_lba = transfer["fad"] - FAD_LBA_OFFSET
        end_lba = start_lba + transfer["sectors"]
        matches = [
            asset for asset in assets
            if int(asset["start_lba"]) < end_lba and int(asset["end_lba"]) > start_lba
        ]
        exact = [
            asset for asset in matches
            if int(asset["start_lba"]) <= start_lba and end_lba <= int(asset["end_lba"])
        ]
        names = [str(asset["name"]) for asset in (exact if len(exact) == 1 else matches)]
        row: dict[str, int | str] = dict(transfer)
        row["start_lba"] = start_lba
        row["end_lba_exclusive"] = end_lba
        row["asset"] = names[0] if len(names) == 1 else (";".join(names) if names else "UNMAPPED")
        row["extent_match"] = "exact" if len(exact) == 1 else ("overlap" if matches else "unmapped")
        result.append(row)
    return result


def make_report(rows: list[dict[str, int | str]]) -> str:
    by_asset: dict[str, list[dict[str, int | str]]] = defaultdict(list)
    for row in rows:
        by_asset[str(row["asset"])].append(row)

    lines = [
        "# VF3 v66 REIOS HLE asset correlation",
        "",
        f"- FA22 transfer records: **{len(rows):,}**",
        f"- Sectors reported: **{sum(int(r['sectors']) for r in rows):,}**",
        f"- Non-zero bytes observed: **{sum(int(r['nonzero_bytes']) for r in rows):,}**",
        f"- FAD-to-GDFS LBA conversion: LBA = FAD - {FAD_LBA_OFFSET}",
        "- Asset extents use 2048-byte sectors, rounded up from indexed file size.",
        "",
        "## Transfers grouped by indexed asset",
        "",
        "| Asset | Calls | Sectors | Non-zero bytes | Destination range | Match |",
        "|---|---:|---:|---:|---|---|",
    ]
    for name, group in sorted(by_asset.items(), key=lambda item: (not item[0].startswith("MT"), item[0])):
        destinations = [int(r["destination"]) for r in group]
        matches = sorted({str(r["extent_match"]) for r in group})
        lines.append(
            f"| {name} | {len(group)} | "
            f"{sum(int(r['sectors']) for r in group)} | "
            f"{sum(int(r['nonzero_bytes']) for r in group):,} | "
            f"0x{min(destinations):08X}–0x{max(destinations):08X} | "
            f"{', '.join(matches)} |"
        )

    motion = [r for r in rows if str(r["asset"]).startswith("MT") and str(r["asset"]).endswith(".BIN")]
    lines.extend(["", "## Motion-pack transfer records", ""])
    if motion:
        lines.extend([
            "| Asset | FAD | LBA | Sectors | Guest destination | Non-zero bytes | Result |",
            "|---|---:|---:|---:|---:|---:|---:|",
        ])
        for row in motion:
            lines.append(
                f"| {row['asset']} | {row['fad']} | {row['start_lba']} | "
                f"{row['sectors']} | 0x{int(row['destination']):08X} | "
                f"{int(row['nonzero_bytes']):,} | {row['result']} |"
            )
    else:
        lines.append("No transfer records mapped to indexed MT*.BIN assets.")

    lines.extend([
        "",
        "## Interpretation limits",
        "",
        "- exact means the complete requested sector interval is contained in one indexed file extent.",
        "- overlap means a transfer crosses an indexed extent boundary or overlaps multiple extents.",
        "- The index maps disc sectors to filenames; it does not by itself prove how the game interprets the bytes.",
        "- Non-zero-byte counts are content fingerprints, not proof that every byte is meaningful or consumed.",
        "- Guest destinations are the addresses recorded by the REIOS HLE hook, not hardware GD-ROM DMA destinations.",
        "",
    ])
    return "\n".join(lines)


def write_outputs(rows: list[dict[str, int | str]], output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    fields = [
        "word_index", "asset", "extent_match", "fad", "start_lba", "end_lba_exclusive",
        "sectors", "destination", "flags", "result", "nonzero_bytes", "fnv1a",
        "first_word", "last_word",
    ]
    with (output_dir / "transfers.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)
    (output_dir / "report.md").write_text(make_report(rows), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path, help="combined vf3-loader-trace-loader.bin file")
    parser.add_argument("--asset-index", type=Path, default=Path("docs/GDFS_ASSET_INDEX.csv"))
    parser.add_argument("--output-dir", type=Path, default=Path("loader-trace-asset-map"))
    args = parser.parse_args()

    try:
        transfers = decode_transfers(args.trace.read_bytes())
        assets = load_assets(args.asset_index)
        rows = correlate(transfers, assets)
        write_outputs(rows, args.output_dir)
    except (OSError, ValueError, csv.Error) as exc:
        parser.exit(1, f"loader_trace_asset_map: {exc}\n")

    print(make_report(rows))
    print(f"\nWrote {len(rows)} transfer records to {args.output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
