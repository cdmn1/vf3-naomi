#!/usr/bin/env python3
"""Inspect the retail VF3tb Dreamcast executable without redistributing it."""
from pathlib import Path
import hashlib
import re
import struct
import subprocess

path = Path("1ST_READ.BIN")
data = path.read_bytes()
out = [
    "VF3tb RETAIL DREAMCAST EXECUTABLE INSPECTION",
    f"SIZE: {len(data)}",
    f"SHA256: {hashlib.sha256(data).hexdigest()}",
    "LOAD BASE REFERENCE: 0x8C010000 (from existing project research)",
    "",
    "FIRST 64 BYTES:",
    data[:64].hex(" "),
    "",
    "KNOWN ADDRESS LITERALS (big-endian 32-bit occurrences):",
]
addresses = [
    (0x8C0000BC, "BIOS service pointer"),
    (0x8C001006, "REIOS entry reference"),
    (0x8C0360C8, "_gdFsDaPlaySct candidate"),
    (0x8C0430A0, "modeled syscall stub A"),
    (0x8C0430B4, "modeled syscall stub B"),
]
for value, label in addresses:
    offsets = [m.start() for m in re.finditer(re.escape(struct.pack(">I", value)), data)]
    found = ", ".join(f"file+0x{x:X}" for x in offsets[:32]) if offsets else "not found"
    out.append(f"{label} 0x{value:08X}: {found}")

out.extend(["", "ASCII STRINGS (4+ chars) matching GD/GDFS/ROM/SYSCALL/DMA terms:"])
for match in re.finditer(rb"[ -~]{4,}", data):
    value = match.group().decode("ascii", "replace")
    if re.search(r"gd|gdfs|rom|syscall|dma|disc|sector|naomi", value, re.I):
        out.append(f"file+0x{match.start():X}: {value[:180]}")

disassembler = "sh4-linux-gnu-objdump"
if subprocess.call(["sh", "-c", f"command -v {disassembler} >/dev/null 2>&1"]) == 0:
    result = subprocess.run(
        [disassembler, "-D", "-b", "binary", "-m", "sh4",
         "--adjust-vma=0x8c010000", str(path)],
        check=True, text=True, capture_output=True,
    )
    Path("vf3tb-sh4-disassembly.txt").write_text(result.stdout)
    out.extend(["", "SH-4 DISASSEMBLY REFERENCES (address constants / GDFS terms):"])
    hits = []
    for line_number, line in enumerate(result.stdout.splitlines(), 1):
        if re.search(r"8c0000bc|8c001006|8c0360c8|8c0430a0|8c0430b4|gdFs|GDFS|DMAREAD", line, re.I):
            hits.append(f"{line_number}: {line}")
    out.extend(hits[:160] if hits else ["No matching literal text in disassembly output."])
else:
    out.extend(["", "SH-4 objdump unavailable; string/literal scan completed."])

Path("executable-inspection.txt").write_text("\n".join(out) + "\n")
print(Path("executable-inspection.txt").read_text())
