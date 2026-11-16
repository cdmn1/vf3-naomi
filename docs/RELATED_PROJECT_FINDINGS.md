# Related-project findings: Dreamcast VF3tb reverse engineering

Reviewed: 2026-11-16

This note records public research in [celsowm/vf3-decomp](https://github.com/celsowm/vf3-decomp) that is directly relevant to this Naomi investigation. It is a secondary source, not a substitute for validating findings against our own supplied executable or Naomi hardware.

## Executive summary

The related project is substantially more advanced than the earlier description in this repository implied. Its documentation describes two distinct tracks:

1. A native C reconstruction of VF3tb gameplay and asset readers. The README reports 2,398 baseline functions ported, an early SDL3 runtime shell, and a basic Akira-versus-Jacky match loop. It explicitly says this is not yet a minimum playable port.
2. An opt-in legacy research harness (`vf3sdl`) that models Dreamcast services and runs translated guest code. The research notes report ISO9660/GD-ROM loading of 367 files and a rendered VF3tb attract fight, while still identifying unimplemented code paths and missing AICA PCM output.

Neither track is a Naomi port. The work is nevertheless a valuable source of observed Dreamcast runtime behavior and asset-format knowledge.

## Verified findings from the related repository

### 1. Executable and resident module

Its `docs/executables.md` identifies:

| File | Size | Load/target | Documented role |
|---|---:|---|---|
| `1ST_READ.BIN` | 1,179,648 bytes | SH-4 at `0x8C010000` | US retail VF3tb V1.002 main executable |
| `VF3TBE3.BIN` | 1,114,112 bytes | SH-4 at `0x8C010000` | E3 demo build, useful for build-to-build comparison |
| `RELOAD.BIN` | 655,360 bytes | SH-4 at `0x8C010000` | Re-init/resident module; includes Shinobi, Naomi-library and NEC PowerVR identifiers |
| `SNDDRV.BIN` | 53,120 bytes | AICA RAM | ARM7 sound-driver program |

The same document identifies version banners for the Naomi library, GDFS, Shinobi system/peripheral/backup libraries, display driver and NEC PowerVR code.

Our earlier executable measurement is also 1,179,648 bytes and was recorded with SHA-256 `37f38dd9a47b785f526157c4a1674ca75322f0de1342f64bbf33c88d128e4b32`. The matching size and retail revision are consistent, but the external document does not publish a hash, so byte identity has not been independently established.

**Important scramble warning:** the related repository's `tools/dc_scramble.py` explicitly warns that retail VF3tb GD-ROM images are already plain. Applying the self-boot MIL-CD descrambler permutes the retail image incorrectly; a round-trip test alone will not detect that mistake. Do not descramble our retail `1ST_READ.BIN`.

### 2. Dreamcast boot and storage path

The related research harness documents:

- A real ISO9660/GD-ROM loader path.
- A GDFS resource-name table with 222 embedded `.BIN` filename records.
- Name-to-tag lookup and observed tag families for high-priority overlays, motion, BGM, stage tables and canvas packages.
- A GD-ROM BIOS service model that handles request, status polling, DMA-read setup and sector transfer.
- A boot run that loads 367 files from the disc.

This gives us a concrete adaptation boundary: retain the game's resource-name/tag semantics where needed, but replace the Dreamcast GD-ROM sector transport with a Naomi-side asset provider (ROMFS/NetDIMM-backed files). We should not assume that replacing GD-ROM calls alone is sufficient; the loader's GDFS lookup and resident-buffer expectations also matter.

### 3. REIOS system services and boot state

The related project's `docs/re/boot_memory_map.md` records that the game reads REIOS service pointers from the RAM system area at `0x8C0000B0..0x8C0000E0`, including the GD-ROM service pointer at `0x8C0000BC`.

It also records that `0x8C000000` is a cached RAM mirror, not a PVR2 register window. The PVR2 registers are in area 0 at `0x005F8000..0x005F9FFF`, mirrored at `0xA05F8000`. This is useful when interpreting memory accesses and when deciding which Dreamcast boot services must be emulated or replaced.

The harness starts at the game's entry point and models REIOS services; it does not yet execute the complete original BIOS boot chain. This distinction matters when reproducing its results.

### 4. Graphics, input and audio progress

The related project's research plan reports:

- A PVR2 Tile Accelerator decoder and software rasterizer, exercised with actual game command streams.
- A frame comparison against Flycast reference output; the boot-attract note reports an 87.28% pixel-within-tolerance match for a boot frame, not pixel-perfect equivalence.
- A Maple controller model and SDL3 gamepad/keyboard input path.
- SH-4-side AICA command queue and driver hand-off modelling.
- No ARM7 interpreter and no completed AICA PCM mixer in the documented research harness; therefore sound output remains incomplete.

These are Dreamcast-side observations. Naomi uses related hardware, but its boot, memory map, arcade I/O, ROM board/NetDIMM and device configuration still need their own tests.

### 5. What the native C port contributes

The related README describes a clean-room native C runtime that consumes extracted assets rather than booting the Dreamcast executable. Its current documented vertical slice includes POL/TEX parsing and rendering for selected fighters/stages and a basic match shell. It is not yet a minimum playable game: fighter motion, combat behavior, complete presentation and audio remain incomplete.

This track may eventually offer reusable, readable game-logic and asset-format insight. It is not a drop-in Naomi runtime and should not be confused with the legacy `vf3sdl` guest-code research harness.

## Consequences for this Naomi project

1. **Keep the Dreamcast executable as the initial reference.** It is the closest hardware/software lineage to Naomi, but direct binary execution is not established.
2. **Use the related project as a behavioral oracle.** Its GDFS, GD-ROM, Maple, PVR2 and AICA notes can help us classify Dreamcast dependencies and avoid rediscovering them.
3. **Prioritize storage and boot boundaries.** The documented 222-name resource table, GDFS tag decoding, BIOS service vectors and 367-file boot ladder give us concrete targets for a Naomi asset-provider design.
4. **Compare the resident module separately.** `RELOAD.BIN` is a distinct 655,360-byte image with relevant Naomi and PowerVR identifiers; it should be inventoried and analyzed separately if present in the user's own disc dump.
5. **Do not treat the native port as finished or Naomi-compatible.** Its current value is evidence and reverse-engineering leverage, not a completed game port.

## Immediate next technical tasks

- Reconcile the related project's documented executable identities against our own `1ST_READ.BIN` hash and disc inventory; do not rely on file size alone.
- Check whether the user's disc asset set contains `RELOAD.BIN` and compare its recorded size/name against the related project's inventory.
- Use the GDFS name/tag model to map the resource names already found in our own asset inventory to actual file groups and destination buffers.
- Once the original executable bytes are available in this working session, resume instruction-level tracing around the Naomi source-path references and distinguish active loader calls from diagnostics.
- Build Naomi-side probes independently for ROMFS reads, JVS input, PVR2 display and AICA startup before attempting game integration.

## Sources

- [celsowm/vf3-decomp README](https://github.com/celsowm/vf3-decomp)
- [Executable inventory and library banners](https://github.com/celsowm/vf3-decomp/blob/main/docs/executables.md)
- [Dreamcast boot/SDL3 research plan](https://github.com/celsowm/vf3-decomp/blob/main/docs/re/plan_sdl3_port.md)
- [REIOS memory map findings](https://github.com/celsowm/vf3-decomp/blob/main/docs/re/boot_memory_map.md)
- [Boot reaches the attract fight](https://github.com/celsowm/vf3-decomp/blob/main/docs/re/boot_attract.md)
- [Off-inventory code and Maple input findings](https://github.com/celsowm/vf3-decomp/blob/main/docs/re/boot_offinv_and_pad.md)
- [GDFS resource lookup implementation](https://github.com/celsowm/vf3-decomp/blob/main/src/sys/gdfs.c)
- [GD-ROM service model](https://github.com/celsowm/vf3-decomp/blob/main/src/sys/gdrom_hle.c)
- [Native C port plan](https://github.com/celsowm/vf3-decomp/blob/main/docs/re/native_decomp_port.md)
- [Retail scramble warning](https://github.com/celsowm/vf3-decomp/blob/main/tools/dc_scramble.py)
