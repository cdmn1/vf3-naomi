#!/usr/bin/env python3
"""Pack extracted VF3tb files into a compact sector-aligned payload.

The sidecar map preserves original GD-ROM FAD ranges while assigning each
asset a compact NetDIMM payload offset. The packer intentionally does not
invent data for excluded files or for gaps in the original disc layout.
"""
from __future__ import annotations
import argparse, csv, hashlib, json, math
from pathlib import Path

SECTOR = 2048

def load_manifest(path: Path):
    with path.open(newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    required = {"asset_name", "gd_lba", "size_bytes", "manifest_sha1"}
    if not rows or not required.issubset(rows[0]):
        raise ValueError("manifest missing required columns or is empty")
    names = set()
    for r in rows:
        name = r["asset_name"].strip()
        if not name or name.upper() in names:
            raise ValueError(f"empty or duplicate manifest filename: {name!r}")
        names.add(name.upper())
        r["gd_lba"] = int(r["gd_lba"])
        r["size_bytes"] = int(r["size_bytes"])
        if r["gd_lba"] < 0 or r["size_bytes"] <= 0:
            raise ValueError(f"invalid LBA/size for {name}")
    rows.sort(key=lambda x: x["gd_lba"])
    previous_end = -1
    for r in rows:
        start = r["gd_lba"]
        end = start + math.ceil(r["size_bytes"] / SECTOR)
        if start < previous_end:
            raise ValueError(f"overlapping original disc extent at {r['asset_name']}")
        previous_end = end
    return rows

def find_asset(root: Path, name: str):
    matches = [p for p in root.rglob("*") if p.is_file() and p.name.upper() == name.upper()]
    if len(matches) != 1:
        raise ValueError(f"{name}: expected exactly one extracted file, found {len(matches)}")
    return matches[0]

def pack(assets: Path, manifest: Path, output: Path, map_path: Path, exclude: set[str]):
    rows = load_manifest(manifest)
    output.parent.mkdir(parents=True, exist_ok=True)
    mapping = []
    packed_offset = 0
    total_source = 0
    with output.open("wb") as out:
        for r in rows:
            name = r["asset_name"]
            if name.upper() in exclude:
                continue
            src = find_asset(assets, name)
            expected = r["size_bytes"]
            actual = src.stat().st_size
            if actual != expected:
                raise ValueError(f"{name}: extracted size {actual} != manifest size {expected}")
            digest = hashlib.sha1()
            with src.open("rb") as f:
                remaining = actual
                while remaining:
                    chunk = f.read(min(1024 * 1024, remaining))
                    if not chunk:
                        raise ValueError(f"{name}: unexpected EOF")
                    out.write(chunk); digest.update(chunk); remaining -= len(chunk)
            sha1 = digest.hexdigest()
            if r["manifest_sha1"] and sha1.lower() != r["manifest_sha1"].lower():
                raise ValueError(f"{name}: SHA-1 {sha1} != manifest {r['manifest_sha1']}")
            sectors = math.ceil(actual / SECTOR)
            padding = sectors * SECTOR - actual
            if padding:
                out.write(b"\0" * padding)
            mapping.append({
                "name": name, "fad": r["gd_lba"] + 150,
                "disc_lba": r["gd_lba"], "sectors": sectors,
                "size_bytes": actual, "payload_offset": packed_offset,
                "payload_sectors": sectors, "sha1": sha1,
            })
            packed_offset += sectors * SECTOR
            total_source += actual
    expected_names = {r["asset_name"].upper() for r in rows} - exclude
    actual_names = {p.name.upper() for p in assets.rglob("*") if p.is_file()}
    missing = sorted(expected_names - actual_names)
    if missing:
        raise ValueError("manifest assets missing from extraction: " + ", ".join(missing[:20]))
    map_doc = {
        "format": "VF3NAOMI_PACKED_ASSETS_V1",
        "sector_bytes": SECTOR,
        "fad_bias": 150,
        "excluded": sorted(exclude),
        "source_file_count": len(rows),
        "packed_file_count": len(mapping),
        "source_bytes": total_source,
        "payload_bytes": packed_offset,
        "entries": mapping,
    }
    map_path.parent.mkdir(parents=True, exist_ok=True)
    map_path.write_text(json.dumps(map_doc, indent=2) + "\n", encoding="utf-8")
    return map_doc

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--assets", type=Path, required=True)
    p.add_argument("--manifest", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--map", type=Path, required=True)
    p.add_argument("--exclude", action="append", default=["MOVIE_A.SFD"])
    a = p.parse_args()
    result = pack(a.assets, a.manifest, a.output, a.map, {x.upper() for x in a.exclude})
    print(json.dumps({k:v for k,v in result.items() if k != "entries"}, indent=2))

if __name__ == "__main__":
    main()
