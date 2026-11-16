# Dreamcast VF3tb executable: Naomi code evidence

Date: 2026-11-16

## Source artifact

The analysis below is based on the user's supplied US Dreamcast `1ST_READ.BIN`:

- Size: 1,179,648 bytes
- SHA-256: `37f38dd9a47b785f526157c4a1674ca75322f0de1342f64bbf33c88d128e4b32`
- The file is a raw binary, not an ELF image.
- Offsets below are file offsets in this raw binary, not virtual addresses.

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

These strings are strong evidence that Naomi-targeted modules, diagnostics, or shared source-derived components are linked into the Dreamcast executable. The source paths specifically name object loading, robot/character loading, sound, stage and an `nlrom` module. Their precise runtime role still needs to be established.

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

This remains a hypothesis until code references, control flow, register accesses and file-loading behaviour are mapped.

## Next reverse-engineering steps

1. Disassemble the raw SH-4 image and establish load/entry addresses from the Dreamcast boot process.
2. Determine whether Naomi library/source strings are referenced by live code or are merely retained diagnostics/debug strings.
3. Trace the `obj_load.c`, `rob_load.c`, `sound.c`, `stage.c` and `nlrom.c` modules via references and call sites.
4. Identify Dreamcast-specific accesses to GD-ROM, Maple and VMU, and the corresponding Naomi/JVS/ROM-board paths.
5. Determine whether the executable contains conditional platform branches or separately linked platform modules.

No claim of direct Naomi boot or playable compatibility is made.
