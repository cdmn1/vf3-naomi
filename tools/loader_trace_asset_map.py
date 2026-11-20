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
MT_SLOT_TABLE_BYTES = 0x8AC0
MT_SLOT_LOOKUP_PCS = {0x8C09D5CE, 0x8C09D5D2, 0x8C09D5DA}


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
            "size_bytes": size,
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



def summarize_memory_pcs(motion_memory):
    """Summarize watched addresses and read values by pack and guest PC."""
    groups = defaultdict(list)
    for event in motion_memory:
        groups[(str(event["pack"]), canonical(int(event["pc"])))].append(event)
    output = []
    for (pack, pc), events in sorted(groups.items()):
        reads = [e for e in events if e["access"] == "R"]
        writes = [e for e in events if e["access"] == "W"]
        addresses = [normalize(int(e["address"])) for e in events]
        values = Counter(int(e["value"]) for e in reads if int(e["value"]) != 0)
        addr_counts = Counter(addresses)
        output.append({
            "pack": pack, "pc": pc, "reads": len(reads), "writes": len(writes),
            "unique_addresses": len(set(addresses)),
            "min_address": min(addresses), "max_address": max(addresses),
            "top_addresses": ";".join(
                f"0x{addr:08X}:{count}" for addr, count in addr_counts.most_common(3)
            ),
            "top_nonzero_values": ";".join(
                f"0x{value:X}:{count}" for value, count in values.most_common(3)
            ),
        })
    return output


def summarize_mt_slot_lookups(rows, motion_memory):
    """Map observed reads of the MT 8880-entry slot table to slot IDs."""
    pack_info = {}
    for name in {str(r["asset"]) for r in rows
                 if str(r["asset"]).startswith("MT") and str(r["asset"]).endswith(".BIN")}:
        group = [r for r in rows if str(r["asset"]) == name]
        pack_info[name] = {
            "start": min(int(r["destination"]) for r in group),
            "size": max(int(r.get("asset_size_bytes", 0)) for r in group),
        }
    lookups = Counter()
    pcs = defaultdict(Counter)
    for event in motion_memory:
        pc = canonical(int(event["pc"]))
        if pc not in MT_SLOT_LOOKUP_PCS or event["access"] != "R" or int(event["width"]) != 4:
            continue
        pack = str(event["pack"])
        if pack not in pack_info:
            continue
        relative = normalize(int(event["address"])) - pack_info[pack]["start"]
        if relative < 0 or relative >= MT_SLOT_TABLE_BYTES or relative % 4:
            continue
        slot = relative // 4
        offset = int(event["value"]) & 0xFFFFFFFF
        if offset <= 0 or offset >= pack_info[pack]["size"]:
            continue
        key = (pack, slot, offset)
        lookups[key] += 1
        pcs[key][pc] += 1
    output = []
    for (pack, slot, offset), count in sorted(lookups.items()):
        output.append({
            "pack": pack, "slot": slot, "file_offset": offset, "reads": count,
            "pcs": ";".join(f"0x{pc:08X}:{n}" for pc, n in sorted(pcs[(pack, slot, offset)].items())),
        })
    return output


def make_report(
    rows: list[dict[str, int | str]],
    motion_memory: list[dict[str, int | str]] | None = None,
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

    slot_lookups = summarize_mt_slot_lookups(rows, motion_memory)
    lines.extend(["", "## MT slot-table lookups observed during motion setup", ""])
    if slot_lookups:
        lines.extend([
            "| Pack | Slot ID | File record offset | Reads | PCs |",
            "|---|---:|---:|---:|---|",
        ])
        for row in slot_lookups:
            lines.append(
                f"| {row['pack']} | {row['slot']} | 0x{int(row['file_offset']):X} | "
                f"{row['reads']} | {row['pcs']} |"
            )
        lines.append("")
        lines.append(
            "Slot IDs are computed from each 32-bit table-read address relative "
            "to the pack transfer destination (4-byte entries). The returned "
            "value is the file-relative record offset."
        )
    else:
        lines.append("No valid 32-bit reads from the MT slot-table range were identified at the setup PCs.")

    pc_summary = summarize_memory_pcs(motion_memory)
    lines.extend(["", "## Memory-access summary by PC", ""])
    if pc_summary:
        lines.extend([
            "| Pack | PC | Reads | Writes | Unique addresses | Address span | Common non-zero read values |",
            "|---|---:|---:|---:|---:|---|---|",
        ])
        for row in pc_summary:
            lines.append(
                f"| {row['pack']} | 0x{int(row['pc']):08X} | {row['reads']} | "
                f"{row['writes']} | {row['unique_addresses']} | "
                f"0x{int(row['min_address']):08X}–0x{int(row['max_address']):08X} | "
                f"{row['top_nonzero_values'] or '—'} |"
            )
    else:
        lines.append("No memory events were available for per-PC analysis.")

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

    slot_lookups = summarize_mt_slot_lookups(rows, motion_memory)
    with (output_dir / "mt_slot_lookups.csv").open("w", newline="", encoding="utf-8") as handle:
        fields = ["pack", "slot", "file_offset", "reads", "pcs"]
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(slot_lookups)
    pc_summary = summarize_memory_pcs(motion_memory)
    with (output_dir / "memory_pc_summary.csv").open("w", newline="", encoding="utf-8") as handle:
        fields = ["pack", "pc", "reads", "writes", "unique_addresses",
                  "min_address", "max_address", "top_addresses", "top_nonzero_values"]
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(pc_summary)
    (output_dir / "report.md").write_text(make_report(rows, motion_memory), encoding="utf-8")


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

    print(make_report(rows, motion_memory))
    print(f"\nWrote {len(rows)} transfer records and {len(motion_memory)} motion-memory events to {args.output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
