# Dreamcast VF3tb executable: initial reconnaissance

Date: 2026-11-16

## Supplied artifact

The user supplied the US Dreamcast release's `1ST_READ.BIN` for analysis. The executable itself is retained outside this repository; commercial game binaries and assets must not be committed.

| Property | Result |
|---|---|
| File | `1ST_READ.BIN` |
| Size | 1,179,648 bytes (1.125 MiB) |
| SHA-256 | `37f38dd9a47b785f526157c4a1674ca75322f0de1342f64bbf33c88d128e4b32` |
| Container | Raw binary; no ELF header/signature at offset zero |
| Initial interpretation | Opening words are plausible little-endian SH-4 instructions; this is preliminary, not yet a disassembly |

## Initial observations

- The file is not an ELF executable. Its load address and entry-point convention must be established from the Dreamcast boot format / disc boot metadata and corroborated against code references.
- The first bytes decode plausibly as SH-4 instruction words in little-endian order. This supports (but does not by itself prove) that the supplied file is an unscrambled executable image.
- Embedded strings include:
  - `NAOMI`
  - `NAOMI&YUKA(G1) S.51`
  - `NAOMI&YUKA(G2) S.51`
  - `SEGA-AM NAOMI&YUKA(G1)  S.51`
  - `SEGA-AM NAOMI&YUKA(G2)  S.51`
  - `PORTDEV`
  - `INFOENBLSTATRADRTOUTDRQCFUNCEND`
- The Naomi/Yuka strings are interesting leads, but are not proof that this Dreamcast game binary contains a Naomi game build or a reusable Naomi runtime. They may belong to shared Sega SDK, driver, diagnostic, or other embedded code/data.
- The `PORTDEV` and adjacent token-like string also need contextual analysis before assigning them to a particular device driver or GD-ROM subsystem.

## What this does and does not establish

Established: the supplied artifact's size and hash, its raw-binary form, and the presence of the listed embedded strings.

Not yet established:
- exact disc pressing/revision from the executable alone;
- load address and entry point;
- whether the image is scrambled or otherwise transformed;
- code/data boundaries and function map;
- which Naomi-related strings are live code, unused library code, diagnostics, or data;
- the extent to which game logic and rendering are independent of Dreamcast services;
- the feasibility of reusing this executable directly on Naomi.

## Next analysis steps

1. Obtain/inspect the matching disc boot metadata (IP.BIN / boot sector) to establish the executable load address and boot entry details; record disc serial/revision separately.
2. Validate SH-4 decoding at the entry point and map early startup code.
3. Locate references to the Naomi/Yuka and PORTDEV strings and determine whether they are referenced by executable code.
4. Identify Dreamcast-specific hardware register access, GD-ROM/asset-loading paths, PVR2 setup, AICA/audio, and Maple input dependencies.
5. Compare with the Japanese executable only if it helps isolate regional changes or shared engine code.

No claim of a successful Naomi boot or direct binary compatibility is made at this stage.
