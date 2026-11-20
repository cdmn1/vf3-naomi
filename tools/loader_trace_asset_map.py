#!/usr/bin/env python3
"""Correlate REIOS HLE FA22 transfers and FA20 memory accesses with VF3 MT assets."""
from __future__ import annotations

import argparse
import csv
import math
from collections import Counter, defaultdict
from pathlib import Path
import struct

FA20 = 0xFA20
FA22 = 0xFA22
WORD_BYTES = 8
SECTOR_BYTES = 2048
FAD_LBA_OFFSET = 150
FA22_WORDS = 9
FA20_WORDS = 4
MT_WINDOW_START = 0x0C5F0000
MT_WINDOW_END = 0x0C860000


def normalize(address: int) -> int:
    """Normalize SH-4 P0/P1/P2 aliases to the 29-bit physical address."""
    return address & 0x1FFFFFFF


def canonical(address: int) -> int:
    """Display a normalized RAM address through the game's P1 alias."""
    return 0x80000000 | normalize(address)


def decode_trace(data: bytes) -> tuple[list[dict[str, int]], list[dict[str, int]], int]:
    """Decode FA22 transfer and FA20 memory records from a mixed u64 trace."""
    if len(data) % WORD_BYTES:
        raise ValueError(f"trace size {len(data)} is not divisible by 8 bytes")
    words = struct.unpack(f"<{len(data) // WORD_BYTES}Q", data) if data else ()
    transfers: list[dict[str, int]] = []
    memory: list[dict[str, int]] = []
    i = 0
    while i < len(words):
        word = words[i]
        if word == FA22:
            if i + FA22_WORDS > len(words):
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
            i += FA22_WORDS
            continue
        if (word & 0xFFFF) == FA20:
            if i + FA20_WORDS > len(words):
                raise ValueError(f"truncated FA20 record at u64 word {i}")
            meta, address, value = words[i + 1:i + 4]
            width = (meta >> 48) & 0xFFFF
            memory.append({
                "word_index": i,
                "pc": (word >> 16) & 0xFFFFFFFF,
                "access": "W" if ((meta >> 47) & 1) else "R",
                "width": width,
                "address": address & 0xFFFFFFFF,
                "value": value,
            })
            i += FA20_WORDS
            continue
        i += 1
    return transfers, memory, len(words)


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


def correlate_motion_memory(
    rows: list[dict[str, int | str]],
    memory: list[dict[str, int]],
    trace_word_count: int,
) -> list[dict[str, int | str]]:
    """Attach watched CPU accesses made after each MT pack load, before reuse."""
    groups: dict[str, list[dict[str, int | str]]] = defaultdict(list)
    for row in rows:
        name = str(row["asset"])
        if name.startswith("MT") and name.endswith(".BIN"):
            groups[name].append(row)

    pack_starts = {
        name: min(int(row["word_index"]) for row in group)
        for name, group in groups.items()
    }
    ordered = sorted(pack_starts, key=pack_starts.get)
    output: list[dict[str, int | str]] = []
    for name in ordered:
        group = groups[name]
        load_end = max(int(row["word_index"]) for row in group) + FA22_WORDS
        later_starts = [pack_starts[other] for other in ordered if pack_starts[other] > pack_starts[name]]
        window_end = min(later_starts) if later_starts else trace_word_count
        start = min(int(row["destination"]) for row in group)
        end = max(int(row["destination"]) + int(row["sectors"]) * SECTOR_BYTES for row in group)
        for event in memory:
            if not (load_end <= event["word_index"] < window_end):
                continue
            address = normalize(event["address"])
            if address < end and address + max(1, event["width"]) > start:
                row: dict[str, int | str] = dict(event)
                row["pack"] = name
                row["canonical_pc"] = canonical(event["pc"])
                row["canonical_address"] = canonical(event["address"])
                output.append(row)
    return output



# Fixed record-relative byte streams used by the SH-4 motion evaluator.
PRIMARY_RECORD_READS = {
    0x8C09D6F4: ("counts", 0x736),
    0x8C09D70A: ("times", 0x75B),
    0x8C09D778: ("tuple0", 0x7A4),
    0x8C09D77E: ("tuple1", 0x7A8),
    0x8C09D782: ("tuple2", 0x7AC),
    0x8C09D794: ("tuple3", 0x7B0),
}


def infer_primary_records(rows, motion_memory):
    """Infer primary MT record bases from fixed evaluator cursor offsets."""
    pack_starts = {}
    for name in {str(r["asset"]) for r in rows
                 if str(r["asset"]).startswith("MT") and str(r["asset"]).endswith(".BIN")}:
        group = [r for r in rows if str(r["asset"]) == name]
        pack_starts[name] = min(int(r["destination"]) for r in group)

    counts = defaultdict(Counter)
    for event in motion_memory:
        spec = PRIMARY_RECORD_READS.get(int(event["pc"]))
        if not spec or event["access"] != "R":
            continue
        stream, delta = spec
        pack = str(event["pack"])
        base = normalize(int(event["address"])) - delta
        counts[(pack, base)][stream] += 1

    output = []
    for (pack, base), streams in sorted(counts.items()):
        start = pack_starts.get(pack, 0)
        output.append({
            "pack": pack, "record_base": base,
            "canonical_record_base": canonical(base),
            "pack_relative_offset": base - start,
            "counts_reads": streams["counts"], "times_reads": streams["times"],
            "both_streams_seen": int(streams["counts"] > 0 and streams["times"] > 0),
        })
    return output


def make_report(
    rows: list[dict[str, int | str]],
    motion_memory: list[dict[str, int | str]] | None = None,
    record_candidates: list[dict[str, int | str]] | None = None,
) -> str:
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

    motion_memory = motion_memory or []
    lines.extend(["", "## CPU accesses to loaded motion-pack payloads", ""])
    if motion_memory:
        by_pack: dict[str, list[dict[str, int | str]]] = defaultdict(list)
        for row in motion_memory:
            by_pack[str(row["pack"])].append(row)
        lines.extend([
            "| Pack lifetime | Reads | Writes | Distinct PCs |",
            "|---|---:|---:|---:|",
        ])
        for name, group in by_pack.items():
            reads = sum(row["access"] == "R" for row in group)
            writes = sum(row["access"] == "W" for row in group)
            pcs = {int(row["pc"]) for row in group}
            lines.append(f"| {name} | {reads} | {writes} | {len(pcs)} |")
            pc_counts = Counter(int(row["pc"]) for row in group if row["access"] == "R")
            if pc_counts:
                lines.append(f"  Top read PCs for {name}: " + ", ".join(
                    f"0x{canonical(pc):08X} ({count})" for pc, count in pc_counts.most_common(8)
                ))
    else:
        lines.append("No watched CPU accesses overlapped the loaded motion-pack payloads after each load.")

    record_candidates = record_candidates or []
    lines.extend(["", "## Primary record-base candidates from evaluator cursors", ""])
    if record_candidates:
        lines.extend([
            "| Pack | Inferred record base | Pack-relative offset | Counts reads | Time reads |",
            "|---|---:|---:|---:|---:|",
        ])
        for row in record_candidates:
            lines.append(
                f"| {row['pack']} | 0x{int(row['canonical_record_base']):08X} | "
                f"0x{int(row['pack_relative_offset']):X} | "
                f"{row['counts_reads']} | {row['times_reads']} |"
            )
        lines.append("")
        lines.append("Bases are inferred from fixed evaluator PCs and documented offsets: counts +0x736, key times +0x75B, and tuple lanes +0x7A4/+0x7A8/+0x7AC/+0x7B0. Tuple lanes can also be reached through linked records, so these are candidate bases, not yet proven slot IDs.")
    else:
        lines.append("No fixed count/time cursor reads mapped to a motion-pack range.")

    lines.extend([
        "",
        "## Interpretation limits",
        "",
        "- exact means the complete requested sector interval is contained in one indexed file extent.",
        "- overlap means a transfer crosses an indexed extent boundary or overlaps multiple extents.",
        "- FA20 records are interpreter-observed CPU accesses from configured memory watches; they do not include the direct REIOS HLE copy itself.",
        "- The index maps disc sectors to filenames; it does not by itself prove how the game interprets the bytes.",
        "- Non-zero-byte counts are content fingerprints, not proof that every byte is meaningful or consumed.",
        "- Guest destinations are the addresses recorded by the REIOS HLE hook, not hardware GD-ROM DMA destinations.",
        "",
    ])
    return "\n".join(lines)


def write_outputs(
    rows: list[dict[str, int | str]],
    motion_memory: list[dict[str, int | str]],
    output_dir: Path,
) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    transfer_fields = [
        "word_index", "asset", "extent_match", "fad", "start_lba", "end_lba_exclusive",
        "sectors", "destination", "flags", "result", "nonzero_bytes", "fnv1a",
        "first_word", "last_word",
    ]
    with (output_dir / "transfers.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=transfer_fields)
        writer.writeheader()
        writer.writerows(rows)

    memory_fields = [
        "pack", "word_index", "pc", "canonical_pc", "access", "width",
        "address", "canonical_address", "value",
    ]
    with (output_dir / "motion_memory.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=memory_fields)
        writer.writeheader()
        writer.writerows(motion_memory)

    record_candidates = infer_primary_records(rows, motion_memory)
    with (output_dir / "record_candidates.csv").open("w", newline="", encoding="utf-8") as handle:
        fields = ["pack", "record_base", "canonical_record_base", "pack_relative_offset",
                  "counts_reads", "times_reads", "both_streams_seen"]
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(record_candidates)
    (output_dir / "report.md").write_text(
        make_report(rows, motion_memory, record_candidates), encoding="utf-8"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path, help="combined vf3-loader-trace-loader.bin file")
    parser.add_argument("--asset-index", type=Path, default=Path("docs/GDFS_ASSET_INDEX.csv"))
    parser.add_argument("--output-dir", type=Path, default=Path("loader-trace-asset-map"))
    args = parser.parse_args()

    try:
        transfers, memory, trace_word_count = decode_trace(args.trace.read_bytes())
        assets = load_assets(args.asset_index)
        rows = correlate(transfers, assets)
        motion_memory = correlate_motion_memory(rows, memory, trace_word_count)
        write_outputs(rows, motion_memory, args.output_dir)
    except (OSError, ValueError, csv.Error, struct.error) as exc:
        parser.exit(1, f"loader_trace_asset_map: {exc}\n")

    record_candidates = infer_primary_records(rows, motion_memory)
    print(make_report(rows, motion_memory, record_candidates))
    print(f"\nWrote {len(rows)} transfer records, {len(motion_memory)} motion-memory events and {len(record_candidates)} record-base candidates to {args.output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
