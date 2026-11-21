#!/usr/bin/env python3
"""Inspect the two VF3tb assets published in cdmn1/vf3-naomi-assets.

Only text metadata is emitted. The binary assets are never copied into the
workflow artifact or committed to this repository.
"""
from __future__ import annotations

import hashlib
import struct
import sys
from pathlib import Path

EXPECTED = {
    "MOTHEAD.BIN": {
        "size": 168391,
        "sha1": "1bee8ac0fc843ef55a08b9e51a8314541ed789bd",
    },
    "MTJACLAU.BIN": {
        "size": 1493636,
        "sha1": "47a7bb98870764bfb76775f1c2d15812e45cf2d0",
    },
}
MT_SLOTS = 8880
MT_TABLE_BYTES = MT_SLOTS * 4
MT_PAYLOAD_FLOOR = 0x8AC0
TRACE_SLOTS = (5093, 5096, 1304, 1307, 1313, 572)


def digest(data: bytes, algorithm: str) -> str:
    return hashlib.new(algorithm, data).hexdigest()


def inspect_mt(data: bytes) -> list[str]:
    lines = [
        "",
        "MTJACLAU MT TABLE AUDIT",
        f"table_bytes={MT_TABLE_BYTES} (0x{MT_TABLE_BYTES:X})",
        f"payload_floor={MT_PAYLOAD_FLOOR:#x}",
    ]
    if len(data) < MT_TABLE_BYTES:
        lines.append("ERROR: file is shorter than the 8,880-entry slot table")
        return lines

    offsets = struct.unpack_from(f"<{MT_SLOTS}I", data, 0)
    populated = [(i, off) for i, off in enumerate(offsets) if off != 0]
    invalid = [(i, off) for i, off in populated
               if off < MT_PAYLOAD_FLOOR or off >= len(data)]
    valid_unique = sorted({off for _, off in populated
                           if MT_PAYLOAD_FLOOR <= off < len(data)})
    next_offset = {
        off: valid_unique[i + 1] if i + 1 < len(valid_unique) else len(data)
        for i, off in enumerate(valid_unique)
    }
    lines.extend([
        f"populated_slots={len(populated)}",
        f"invalid_nonzero_slots={len(invalid)}",
        f"unique_valid_payload_offsets={len(valid_unique)}",
    ])
    if invalid:
        lines.append("invalid_examples=" + repr(invalid[:12]))

    lines.append("trace_slot_records:")
    for slot in TRACE_SLOTS:
        off = offsets[slot]
        if off not in next_offset:
            lines.append(f"  slot={slot} offset={off:#x} INVALID")
            continue
        end = next_offset[off]
        record = data[off:end]
        lines.append(
            f"  slot={slot} offset={off:#08x} size={len(record)} "
            f"head32={record[:32].hex(' ')}"
        )
    return lines


def inspect_generic(name: str, data: bytes) -> list[str]:
    words = struct.unpack("<16I", data[:64])
    return [
        "",
        f"{name} GENERIC HEADER (FORMAT NOT ASSUMED)",
        f"first128={data[:128].hex(' ')}",
        "first16_u32_le=" + " ".join(f"{v:08x}" for v in words),
    ]


def main(root: Path) -> int:
    lines = ["VF3tb DREAMCAST ASSET AUDIT", "Source: cdmn1/vf3-naomi-assets tag assets"]
    loaded: dict[str, bytes] = {}
    failed = False

    for name, expected in EXPECTED.items():
        path = root / name
        if not path.is_file():
            lines.append(f"ERROR {name}: missing")
            failed = True
            continue
        data = path.read_bytes()
        loaded[name] = data
        sha1 = digest(data, "sha1")
        lines.extend([
            "",
            name,
            f"size={len(data)} expected_size={expected['size']}",
            f"sha1={sha1} expected_sha1={expected['sha1']}",
            f"sha256={digest(data, 'sha256')}",
            f"integrity={'PASS' if len(data) == expected['size'] and sha1 == expected['sha1'] else 'FAIL'}",
        ])
        if len(data) != expected["size"] or sha1 != expected["sha1"]:
            failed = True

    if "MOTHEAD.BIN" in loaded:
        lines.extend(inspect_generic("MOTHEAD.BIN", loaded["MOTHEAD.BIN"]))
    if "MTJACLAU.BIN" in loaded:
        lines.extend(inspect_mt(loaded["MTJACLAU.BIN"]))

    output = "\n".join(lines) + "\n"
    (root / "MOTION_ASSET_AUDIT.txt").write_text(output, encoding="utf-8")
    print(output, end="")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main(Path(sys.argv[1]) if len(sys.argv) > 1 else Path(".")))
