# Dreamcast VF3tb executable: Naomi code evidence

Date: 2026-11-16

## Source artifact

The analysis below is based on the user's supplied US Dreamcast `1ST_READ.BIN`:

- Size: 1,179,648 bytes
- SHA-256: `37f38dd9a47b785f526157c4a1674ca75322f0de1342f64bbf33c88d128e4b32`
- The file is a raw binary, not an ELF image.
- Offsets below are file offsets in this raw binary, not virtual addresses.
- For the SH-4 analysis, the Dreamcast executable load base used is `0x8c010000`.

## High-value evidence

The executable contains explicit Naomi runtime/library identifiers and Naomi source-tree paths:

| File offset | Embedded string |
|---:|---|
| `0x1b98` | `NAOMI&YUKA(G1) S.51` |
| `0x1bb0` | `NAOMI&YUKA(G2) S.51` |
| `0x1bd0` | `SEGA-AM NAOMI&YUKA(G1) S.51` |
| `0x1c00` | `SEGA-AM NAOMI&YUKA(G2) S.51` |
| `0xbc920` | `GDFS Version 0.53 1998/08/28` |
| `0xbc951` | `gdFs Build:Jan 26 1999 18:22:33` |
| `0xbcb91` | `pd Ver 1.07.VF3 Build:Jan 26 1999 18:22:58` |
| `0xbce75` | `syCbl Ver 1.10 Build:Jan 26 1999 18:48:26` |
| `0xbcf64` | `NAOMI LIBRARY Ver 0.8 AM R&D` |
| `0xbd671` | `kd Ver 1.20 Build:Jan 26 1999 18:22:44` |
| `0xfba4c` | `SEGA-GENKI VF3` |

The executable also contains these source paths:

- `/pub/data3/vf3/naomi/c/nlrom.c` at `0xd9ea8`
- `/pub/data3/vf3/naomi/c/obj_load.c` at `0xd9ec8`
- `/pub/data3/vf3/naomi/c/rob_load.c` at `0xd9ef0`
- `/pub/data3/vf3/naomi/c/sound.c` at `0xd9f18`
- `/pub/data3/vf3/naomi/c/stage.c` at `0xd9f38`

These strings are strong evidence that Naomi-targeted modules, diagnostics, or shared source-derived components are linked into the Dreamcast executable. The source paths specifically name object loading, robot/character loading, sound, stage and an `nlrom` module.

## SH-4 cross-reference findings

A first cross-reference pass was performed against the raw executable using the SH-4 PC-relative `MOV.L @(disp,PC),Rn` encoding. The effective literal-pool address was calculated from the instruction PC, then the loaded 32-bit value was compared with the executable's physical/uncached-style pointer representation of each embedded source path.

This found actual code instructions loading pointers to the Naomi source-path strings; they are not just unreferenced strings sitting in the binary.

| Naomi source path | Code file offsets containing PC-relative loads of its pointer | Nearest preceding `STS.L PR,@-R15` prologue candidates |
|---|---|---|
| `nlrom.c` | `0x92820`, `0x9288c`, `0x928b4` | `0x92804`, `0x9283c` |
| `obj_load.c` | `0x92fd2`, `0x92ff6` | `0x92f90` |
| `rob_load.c` | `0xa2578`, `0xa25ae`, `0xaeea0`, `0xaeec2`, `0xaefbe`, `0xaefe0` | `0xa2518`, `0xa259a`, `0xaed08`, `0xaef2e` |
| `sound.c` | `0xb5f68`, `0xb6120`, `0xb62e4`, `0xb6528` | `0xb5f52`, `0xb6104`, `0xb6298`, `0xb6482` |
| `stage.c` | `0xb794a`, `0xb7966`, `0xb7aa0` | `0xb78e0`, `0xb7a88` |

The prologue column is deliberately described as *candidate*: a nearby `STS.L PR,@-R15` is a useful SH-4 function-entry heuristic, not proof of a complete function boundary. The source-path pointers are passed through stack/register setup near these instructions, consistent with source-location or diagnostic arguments. The next step is to map each candidate's complete control-flow body and its callers before assigning semantic function names.

This is a meaningful step beyond string discovery: there are code-level references to all five Naomi source paths. It still does not prove that every referenced path is part of the normal gameplay path; some may be error/reporting paths or dormant platform-specific code.

## Evidence of both arcade and Dreamcast subsystems

The same binary contains arcade/operator-facing strings:

- Service/test-button workflows, sound-code selection and sound test.
- Coin chute and credit settings.
- CPU/video memory tests, input/output tests, sound test and CRT test.
- Bookkeeping, play-time histograms, 1P/VS statistics and stage select.
- The warning text and settings screens associated with an arcade cabinet.

It also contains clearly Dreamcast-oriented UI and device strings:

- Memory Card removal, formatting, saving/loading and controller port A.
- `mpdrv_...` Maple-port driver symbols and messages.
- Main Console File Menu Screen instructions.
- A GD-ROM filesystem identifier/build string (`GDFS` / `gdFs`).

This is consistent with a shared or multi-platform codebase containing both Naomi/arcade and Dreamcast-specific components. It is not yet proof that the Naomi paths are active in this particular Dreamcast build, nor that the executable can boot unchanged on Naomi.

## Revised working hypothesis

The user's observation — arcade-like fighting game with a console menu and intro FMVs attached — is now a technically plausible hypothesis worth testing. The executable contains both Naomi library/source identifiers and Dreamcast-specific memory-card/Maple/GD-ROM material.

A likely research direction is to separate:
1. game/engine modules that may be shared with Naomi;
2. Naomi-specific services and arcade I/O;
3. Dreamcast boot, GD-ROM, Maple and memory-card services;
4. movie playback and console front-end code.

This remains a hypothesis until control flow, register accesses and file-loading behaviour are mapped.

## Disassembler/tooling note

The public `sp00nznet/dcrecomp` project includes `tools/sh4_disasm.py` and `tools/static_recompile.py`, with a documented Dreamcast load base of `0x8c010000`. Its function finder uses direct BSR targets, stack-prologue patterns, literal-pool call targets and other heuristics. This is a useful starting point for this executable, but its own documentation/code makes clear that indirect control flow and mid-function entries require special handling. Its output must therefore be validated against instruction boundaries and control flow rather than treated as ground truth.

## Next reverse-engineering steps

1. Establish the real entry point and boot handoff from the supplied IP.BIN.
2. Trace callers and callees around the Naomi source-path reference sites.
3. Determine whether `nlrom.c` represents Naomi ROM-board access, and whether the Dreamcast build routes equivalent assets through GD-ROM/GDFS.
4. Trace object, character/robot and stage loading to determine whether the same data formats are shared.
5. Trace the `sound.c` references and the AICA driver startup sequence.
6. Identify Dreamcast-specific accesses to GD-ROM, Maple and VMU, and the corresponding Naomi/JVS/ROM-board paths.
7. Determine whether the executable contains conditional platform branches or separately linked platform modules.

No claim of direct Naomi boot or playable compatibility is made.

## Follow-up: AICA driver asset

The Dropbox `SNDDRV.BIN` was fetched through Dropbox's text-content endpoint for a limited string inspection. It contains the embedded identifiers:

- `AM2/AICAsoundDrv990708/Ver1.63a`
- `Sequencer 1.27 99/03/16 Y.Takeda`

This supports identifying the file as an AM2 AICA sound-driver asset, with the above internal version strings. It is not yet clear whether the binary is a complete driver image, a wrapped/padded form, or a modified copy. Its Dropbox size is 53,120 bytes, while `DATELIST.ASC` records 34,256 bytes for the original `AICADRV.BIN` path, so the discrepancy must be resolved before reusing it.

The executable's embedded `sound.c` path and the driver asset are mutually useful leads, but no sound initialization call sequence has been traced yet.
