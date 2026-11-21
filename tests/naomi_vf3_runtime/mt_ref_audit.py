#!/usr/bin/env python3
"""Audit MTAKIPAI motion-group component boundaries and descriptor references.

This deliberately reports raw component boundaries and pointer ownership. It
does not assign counts/times/tuples/opcode semantics to the components.
"""
from __future__ import annotations
import argparse
import hashlib
import math
import struct
from bisect import bisect_right
from pathlib import Path

SLOTS = 8880
PAYLOAD_FLOOR = 0x8AC0
MOTIONS = (1050, 2386)


def audit(path: Path) -> str:
    data = path.read_bytes()
    if len(data) < PAYLOAD_FLOOR or len(data) < SLOTS * 4:
        raise SystemExit(f"asset too small: {len(data)} bytes")
    offsets = list(struct.unpack_from(f"<{SLOTS}I", data, 0))
    unique = sorted({v for v in offsets if PAYLOAD_FLOOR <= v < len(data)})
    if not unique:
        raise SystemExit("no valid record offsets")
    next_offset = {
        off: (unique[i + 1] if i + 1 < len(unique) else len(data))
        for i, off in enumerate(unique)
    }
    lines = [
        "MTAKIPAI STATIC MOTION-REFERENCE AUDIT",
        f"ASSET BYTES: {len(data)}",
        f"SHA256: {hashlib.sha256(data).hexdigest()}",
        f"POPULATED SLOTS: {sum(v != 0 for v in offsets)}",
        f"UNIQUE VALID OFFSETS: {len(unique)}",
    ]
    for motion_id in MOTIONS:
        first = (motion_id - 1) * 3
        group = []
        lines.extend(["", f"MOTION {motion_id}  SLOTS {first}-{first + 2}"])
        for component in range(3):
            slot = first + component
            off = offsets[slot]
            if off not in next_offset:
                raise SystemExit(
                    f"motion {motion_id}: invalid slot {slot} offset {off:#x}"
                )
            end = next_offset[off]
            group.append((slot, off, end - off))
            lines.append(
                f"COMPONENT {component}: SLOT {slot} OFFSET {off:06X} SIZE {end-off}"
            )

        a_slot, a_off, a_len = group[0]
        if a_len < 12:
            raise SystemExit(f"motion {motion_id}: descriptor component too short")
        w0, w1, ref = struct.unpack_from("<III", data, a_off)
        lines.append(f"HEADER WORDS: {w0:08X} {w1:08X} {ref:08X}")

        pos = bisect_right(unique, ref) - 1
        if pos < 0 or ref >= next_offset[unique[pos]]:
            lines.append(f"REFERENCE {ref:06X}: NOT INSIDE A VALID RECORD")
            continue
        owner_off = unique[pos]
        delta = ref - owner_off
        owner_slots = [i for i, value in enumerate(offsets) if value == owner_off]
        owner_slot = owner_slots[0] if owner_slots else -1
        owner_motion = owner_slot // 3 + 1 if owner_slot >= 0 else -1
        owner_component = owner_slot % 3 if owner_slot >= 0 else -1
        owner_len = next_offset[owner_off] - owner_off
        lines.append(
            f"REFERENCE {ref:06X}: SLOT {owner_slot} "
            f"MOTION {owner_motion} COMPONENT {owner_component} "
            f"RECORD {owner_off:06X}+{delta:03X} "
            f"(RECORD SIZE {owner_len})"
        )
        if delta % 4 == 0:
            lines.append(f"REFERENCE FLOAT INDEX: {delta // 4}")
        if delta + 404 <= owner_len:
            vals = struct.unpack_from("<101f", data, ref)
            finite = [v for v in vals if math.isfinite(v)]
            lines.append(
                "REFERENCE 101-FLOAT WINDOW: "
                f"{len(finite)}/101 FINITE, "
                f"RANGE {min(finite):.5f} .. {max(finite):.5f}"
            )

        c_slot, c_off, c_len = group[2]
        if c_len % 4 == 0:
            vals = struct.unpack_from(f"<{c_len // 4}f", data, c_off)
            finite = [v for v in vals if math.isfinite(v)]
            lines.append(
                f"LOCAL COMPONENT 2 FLOATS: {len(finite)}/{len(vals)} FINITE, "
                f"RANGE {min(finite):.5f} .. {max(finite):.5f}"
            )
    lines += [
        "",
        "NOTE: Component meanings and evaluator-header construction remain unmapped.",
    ]
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("asset", type=Path)
    print(audit(parser.parse_args().asset), end="")
