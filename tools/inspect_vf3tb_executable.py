#!/usr/bin/env python3
"""Inspect the retail VF3tb Dreamcast executable without redistributing it."""
from pathlib import Path
import hashlib
import re
import struct
import subprocess

BASE = 0x8C010000
path = Path("1ST_READ.BIN")
data = path.read_bytes()
out = [
    "VF3tb RETAIL DREAMCAST EXECUTABLE INSPECTION",
    f"SIZE: {len(data)}",
    f"SHA256: {hashlib.sha256(data).hexdigest()}",
    f"LOAD BASE REFERENCE: 0x{BASE:08X} (existing project research; verify against runtime)",
    "",
    "FIRST 64 BYTES:",
    data[:64].hex(" "),
    "",
    "KNOWN ADDRESS LITERALS (both byte orders):",
]
known = [
    (0x8C0000BC, "BIOS service pointer"),
    (0x8C001006, "REIOS entry reference"),
    (0x8C0360C8, "_gdFsDaPlaySct candidate"),
    (0x8C0430A0, "modeled syscall stub A"),
    (0x8C0430B4, "modeled syscall stub B"),
]
for value, label in known:
    be = [m.start() for m in re.finditer(re.escape(struct.pack(">I", value)), data)]
    le = [m.start() for m in re.finditer(re.escape(struct.pack("<I", value)), data)]
    out.append(f"{label} 0x{value:08X}: BE={[hex(x) for x in be[:16]]}; LE={[hex(x) for x in le[:16]]}")

string_patterns = [
    rb"GDFS Version", rb"gdFs Build", rb"DMA incompleted", rb"DMA completed",
    rb"NAOMI LIBRARY", rb"/pub/data3/vf3/naomi/c/nlrom.c",
    rb"/pub/data3/vf3/naomi/c/obj_load.c", rb"/pub/data3/vf3/naomi/c/rob_load.c",
    rb"/pub/data3/vf3/naomi/c/sound.c", rb"/pub/data3/vf3/naomi/c/stage.c",
]
strings = []
for pattern in string_patterns:
    match = re.search(re.escape(pattern), data)
    if match:
        strings.append((match.start(), match.group().decode("ascii", "replace")))
out.extend(["", "NAOMI/GDFS STRINGS AND POINTER REFERENCES:"])
for offset, value in strings:
    va = BASE + offset
    be = [m.start() for m in re.finditer(re.escape(struct.pack(">I", va)), data)]
    le = [m.start() for m in re.finditer(re.escape(struct.pack("<I", va)), data)]
    out.append(f"file+0x{offset:X} VA=0x{va:08X} {value}")
    out.append(f"  pointer literals: BE={[hex(x) for x in be[:16]]}; LE={[hex(x) for x in le[:16]]}")

out.extend(["", "SH-4 PC-RELATIVE LITERAL REFERENCES TO THOSE STRINGS:"])
string_vas = {BASE + offset: value for offset, value in strings}
stub_vas = {
    0x8C04308C: "REQ_CMD (r7=0)", 0x8C0430A0: "GET_CMD_STAT (r7=1)",
    0x8C0430B4: "EXEC_SERVER (r7=2)", 0x8C0430C8: "r7=3",
    0x8C0430DC: "r7=4", 0x8C0430F0: "r7=5",
}
stub_aliases = {value: label for value, label in stub_vas.items()}
stub_aliases.update({value & 0x0FFFFFFF: label + " (P0 alias)" for value, label in stub_vas.items()})
pc_refs = []
stub_refs = []
for offset in range(0, len(data) - 1, 2):
    insn = struct.unpack_from("<H", data, offset)[0]
    if (insn & 0xF000) != 0xD000:  # mov.l @(disp,PC),Rn
        continue
    insn_va = BASE + offset
    literal_va = ((insn_va + 4) & ~3) + (insn & 0xFF) * 4
    literal_off = literal_va - BASE
    if 0 <= literal_off <= len(data) - 4:
        literal_value = struct.unpack_from("<I", data, literal_off)[0]
        if literal_value in string_vas:
            pc_refs.append((offset, insn_va, literal_off, literal_value, string_vas[literal_value], (insn >> 8) & 0xF))
        if literal_value in stub_aliases:
            stub_refs.append((offset, insn_va, literal_off, literal_value, stub_aliases[literal_value], (insn >> 8) & 0xF))
for ref in pc_refs[:100]:
    off, va, lit_off, value, label, reg = ref
    out.append(f"code file+0x{off:X} VA=0x{va:08X}: mov.l @(PC literal file+0x{lit_off:X}),r{reg} -> 0x{value:08X} [{label}]")
if not pc_refs:
    out.append("No matches under the SH-4 mov.l @(disp,PC),Rn decoding assumption.")out.extend(["", "PC-RELATIVE CALL REFERENCES TO BIOS SYSCALL STUBS (including P0 aliases):"])
for ref in stub_refs[:160]:
    off, va, lit_off, value, label, reg = ref
    out.append(f"code file+0x{off:X} VA=0x{va:08X}: mov.l literal file+0x{lit_off:X},r{reg} -> 0x{value:08X} [{label}]")
if not stub_refs:
    out.append("No direct PC-relative references to the known stub addresses/aliases.")

out.extend(["", "ASCII STRINGS MATCHING GD/GDFS/ROM/SYSCALL/DMA TERMS:"])
for match in re.finditer(rb"[ -~]{4,}", data):
    value = match.group().decode("ascii", "replace")
    if re.search(r"gd|gdfs|rom|syscall|dma|disc|sector|naomi", value, re.I):
        out.append(f"file+0x{match.start():X}: {value[:180]}")

objdump = "sh4-linux-gnu-objdump"
if subprocess.call(["sh", "-c", f"command -v {objdump} >/dev/null 2>&1"]) == 0:
    result = subprocess.run(
        [objdump, "-EL", "-D", "-b", "binary", "-m", "sh4", f"--adjust-vma=0x{BASE:08x}", str(path)],
        check=True, text=True, capture_output=True,
    )
    disassembly = result.stdout.splitlines()
    Path("vf3tb-sh4-disassembly.txt").write_text(result.stdout)
    address_to_line = {}
    for index, line in enumerate(disassembly):
        match = re.match(r"^([0-9a-fA-F]+):", line)
        if match:
            address_to_line[int(match.group(1), 16)] = index
    targets = [0x8C036080, 0x8C043080, 0x8C043090, 0x8C0430A0, 0x8C0430B4, 0x8C0430C8, 0x8C0430DC, 0x8C0430F0] + [BASE + x[0] for x in strings]
    out.extend(["", "SYSCALL STUB REGION (0x8C043000-0x8C043200):"])
    for index, line in enumerate(disassembly):
        match = re.match(r"^([0-9a-fA-F]+):", line)
        if match and 0x8C043000 <= int(match.group(1), 16) < 0x8C043200:
            out.append(line)
    out.extend(["", "ALL INSTRUCTIONS REFERENCING THE BIOS SERVICE POINTER OR KNOWN STUB ADDRESSES:"])
    for line in disassembly:
        if re.search(r"8c0000bc|8c043090|8c0430a0|8c0430b4|8c0430c8|8c0430dc|8c0430f0", line, re.I):
            out.append(line)
    out.extend(["", "SH-4 DISASSEMBLY WINDOWS AROUND CANDIDATE FUNCTIONS / STRING DATA:"])
    for target in targets:
        if target not in address_to_line:
            continue
        index = address_to_line[target]
        out.append(f"--- around VA 0x{target:08X} ---")
        out.extend(disassembly[max(0, index - 8):min(len(disassembly), index + 16)])
    out.extend(["", "SH-4 DISASSEMBLY WINDOWS AROUND CODE REFERENCES TO NAOMI STRINGS:"])
    for ref in pc_refs[:30]:
        index = address_to_line.get(ref[1])
        if index is not None:
            out.append(f"--- {ref[5]}: {ref[4]} at VA 0x{ref[1]:08X} ---")
            out.extend(disassembly[max(0, index - 5):min(len(disassembly), index + 9)])    out.extend(["", "SH-4 CALLSITE WINDOWS FOR BIOS SYSCALL STUBS:"])
    for ref in stub_refs[:60]:
        index = address_to_line.get(ref[1])
        if index is not None:
            out.append(f"--- {ref[4]} at VA 0x{ref[1]:08X} (literal 0x{ref[3]:08X}) ---")
            out.extend(disassembly[max(0, index - 8):min(len(disassembly), index + 12)])
else:
    out.extend(["", "SH-4 objdump unavailable; disassembly scan skipped."])

Path("executable-inspection.txt").write_text("\n".join(out) + "\n")
print(Path("executable-inspection.txt").read_text())
