#!/usr/bin/env python3
"""Build the packed FAD map from the checked-in retail GD-ROM file manifest.

This metadata-only step does not read or replace the extracted asset files.
The full packer separately verifies every extracted file's size and SHA-1.
"""
from __future__ import annotations
import argparse, csv, json, math
from pathlib import Path

SECTOR=2048

def generate(manifest:Path, exclude:set[str])->dict:
    with manifest.open(newline="",encoding="utf-8") as f:
        rows=list(csv.DictReader(f))
    required={"asset_name","gd_lba","size_bytes","manifest_sha1"}
    if not rows or not required.issubset(rows[0]):
        raise ValueError("manifest missing required columns")
    names=set()
    parsed=[]
    for r in rows:
        name=r["asset_name"].strip()
        if not name or name.upper() in names:
            raise ValueError(f"duplicate or empty asset name: {name!r}")
        names.add(name.upper())
        lba=int(r["gd_lba"]); size=int(r["size_bytes"])
        if lba<0 or size<=0: raise ValueError(f"invalid LBA/size: {name}")
        parsed.append({"name":name,"fad":lba+150,"disc_lba":lba,
                       "sectors":math.ceil(size/SECTOR),"size_bytes":size,
                       "sha1":r["manifest_sha1"].strip()})
    parsed.sort(key=lambda e:e["disc_lba"])
    previous=-1
    for e in parsed:
        if e["disc_lba"]<previous:
            raise ValueError(f"overlapping disc extent at {e['name']}")
        previous=e["disc_lba"]+e["sectors"]
    packed=[]; offset=0; source_bytes=0
    for e in parsed:
        if e["name"].upper() in exclude: continue
        item=dict(e,payload_offset=offset,payload_sectors=e["sectors"])
        packed.append(item)
        offset+=e["sectors"]*SECTOR
        source_bytes+=e["size_bytes"]
    return {"format":"VF3NAOMI_PACKED_ASSETS_V1","sector_bytes":SECTOR,
            "fad_bias":150,"excluded":sorted(exclude),
            "source_file_count":len(rows),"packed_file_count":len(packed),
            "source_bytes":source_bytes,"payload_bytes":offset,"entries":packed}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--manifest",type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    p.add_argument("--exclude",action="append",default=["MOVIE_A.SFD"])
    a=p.parse_args()
    d=generate(a.manifest,{x.upper() for x in a.exclude})
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(d,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({k:v for k,v in d.items() if k!="entries"},indent=2))

if __name__=="__main__": main()
