#!/usr/bin/env python3
"""Append a packed VF3tb asset payload to a Naomi ROM at its mapped offset."""
from __future__ import annotations
import argparse, json, re
from pathlib import Path

SECTOR = 2048
NETDIMM_LIMIT = 512 * 1024 * 1024

def combine(rom: Path, payload: Path, map_json: Path, header: Path,
            output: Path, payload_base: int, limit: int = NETDIMM_LIMIT) -> dict:
    doc=json.loads(map_json.read_text(encoding="utf-8"))
    if doc.get("format") != "VF3NAOMI_PACKED_ASSETS_V1":
        raise ValueError("unsupported asset map format")
    if payload_base % SECTOR:
        raise ValueError("payload base must be sector aligned")
    rom_size=rom.stat().st_size
    if payload_base < rom_size:
        raise ValueError("payload base overlaps the Naomi ROM")
    payload_size=payload.stat().st_size
    if payload_size != int(doc["payload_bytes"]):
        raise ValueError(f"payload size {payload_size} != map size {doc['payload_bytes']}")
    h=header.read_text(encoding="utf-8")
    match=re.search(r"^#define VF3_ASSET_PAYLOAD_BASE (\d+)u$",h,re.M)
    if not match or int(match.group(1)) != payload_base:
        raise ValueError("generated runtime header does not use the requested payload base")
    if len(doc.get("entries",[])) != int(doc["packed_file_count"]):
        raise ValueError("asset map entry count mismatch")
    for e in doc["entries"]:
        start=int(e["payload_offset"])
        length=int(e["sectors"])*SECTOR
        if start < 0 or start+length > payload_size:
            raise ValueError(f"asset mapping exceeds packed payload: {e['name']}")
    total=payload_base+payload_size
    if total > limit:
        raise ValueError(f"combined image {total} bytes exceeds NetDIMM limit {limit}")
    output.parent.mkdir(parents=True,exist_ok=True)
    with output.open("wb") as dst, rom.open("rb") as src:
        while True:
            block=src.read(1024*1024)
            if not block: break
            dst.write(block)
        dst.write(b"\0"*(payload_base-rom_size))
        with payload.open("rb") as src:
            while True:
                block=src.read(1024*1024)
                if not block: break
                dst.write(block)
    return {"rom_bytes":rom_size,"payload_base":payload_base,
            "payload_bytes":payload_size,"combined_bytes":total,
            "combined_mib":round(total/(1024*1024),2),
            "entry_count":len(doc["entries"])}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--rom",type=Path,required=True)
    p.add_argument("--payload",type=Path,required=True)
    p.add_argument("--map-json",type=Path,required=True)
    p.add_argument("--header",type=Path,required=True)
    p.add_argument("--payload-base",type=lambda x:int(x,0),required=True)
    p.add_argument("--output",type=Path,required=True)
    a=p.parse_args()
    print(json.dumps(combine(a.rom,a.payload,a.map_json,a.header,a.output,a.payload_base),indent=2))

if __name__=="__main__": main()
