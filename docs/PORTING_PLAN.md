# Porting plan and technical questions

## Strategy

Investigate the Dreamcast VF3tb executable first. Shared SH-4 / PowerVR2 / AICA architecture is a substantial advantage over the Model 3 arcade build, but does not establish direct compatibility.

The arcade Model 3 recompilation remains a reference for game behavior. Its PowerPC 603e guest code and Real3D Pro-1000 board implementation are fundamentally different from Naomi. A review of the public [Model 3 VF3 recompilation](MODEL3_RECOMP_REFERENCE.md) confirms that its useful contribution here is its measurement, lockstep-validation and subsystem-isolation methodology—not reusable Naomi code.

## Phase 1 — executable reconnaissance

Input: user-supplied 1ST_READ.BIN from a Dreamcast VF3tb dump.

Deliverables:
- file format, load address and entry point;
- SH-4 instruction/runtime profile;
- memory regions and hardware register references;
- startup sequence and required Dreamcast SDK/runtime services;
- GD-ROM/disc access patterns and asset inventory;
- PVR2 and AICA initialization/use;
- Maple/controller calls and expected controls.

Do not infer a service is unused merely because it is not obvious in static analysis; validate with call tracing or emulator instrumentation where possible.

## Phase 2 — Naomi proof of compatibility

Build independent test programs for:
- Naomi boot/entry and memory sanity;
- PowerVR2 video initialization and a basic rendered frame;
- JVS input detection/polling;
- audio/AICA initialization;
- ROMFS-backed asset reads.

Use Flycast for rapid iteration, then verify on real Naomi hardware. Keep tests small and isolate failures by subsystem.

## Phase 3 — adaptation

Only after Phase 1 identifies concrete dependencies:
- implement required Dreamcast runtime/API shims;
- replace GD-ROM file access with a Naomi-appropriate asset provider;
- resolve Dreamcast-specific startup and peripheral assumptions;
- adapt Maple controller input to Naomi JVS only after tracing the original
  guest-facing controller read/merge boundary; the physical JVS source is now
  verified on hardware;
- preserve or adapt PVR2/AICA usage where the Naomi hardware interface is compatible.

Avoid broad API emulation until evidence shows it is needed.

## Phase 4 — game integration

Bring up a deterministic scene first, then menus, fighter selection, match logic, animation, effects and sound. Track missing functions/assets explicitly.

## Phase 5 — deployment

Reuse the proven Daytona NetDIMM packaging/encryption/upload workflow where applicable, without sharing game-specific source or merging the repositories.

## Current blockers and next work

- [x] Obtain and inspect Dreamcast VF3tb `1ST_READ.BIN`.
- [x] Build the standalone Naomi bootstrap and verify its visible screen in Flycast.
- [x] Cross-compile and execute the real VF3 MT motion-table parser on Naomi using a synthetic fixture (user confirmed PASS in Flycast).
- [ ] Establish the first reproducible VF3 game-runtime target on Naomi; the bootstrap and parser smoke test are not VF3 gameplay.
- [ ] Determine which portions of the Dreamcast runtime or native reconstruction can be compiled/adapted without importing SDL/host-only dependencies.
- [ ] Define the Naomi ROMFS/NetDIMM asset-provider boundary and add encrypted, content-addressed asset checkpoints before any large conversion stage.
- [ ] Verify Naomi PVR2 video, AICA audio and ROMFS-backed asset reads in isolated probes before attempting full game integration.

The original VF3 input path is now documented in `docs/INPUT_PATH_FINDINGS.md` and is not the current build priority.
- [x] Identify the supplied executable as US retail VF3tb `MK-51001`, V1.002; recorded SHA-256:
  `37f38dd9a47b785f526157c4a1674ca75322f0de1342f64bbf33c88d128e4b32`.
- [x] Build and package the standalone KOS MIE/JVS probe as a versioned Naomi
  ROM image using GitHub Actions and `naomibintool`.
- [x] Verify MIE/JVS input on real Naomi hardware. The user confirmed that all
  cabinet buttons and analog inputs respond. The native JVS switch words and
  `jvs.analog[]` channels are the intended input source; dedicated racing-style
  `wheel`/`accel`/`brake` fields are not required.
- [x] Naomi MIE/JVS input path is confirmed on real hardware.
- [x] Original VF3 digital input conversion has been traced in Flycast; the
  current findings are documented but are not yet integrated into a Naomi
  game runtime.
- [x] Boot the Naomi bootstrap in Flycast; KallistiOS startup and PowerVR2
  output are confirmed.
- [x] Boot the VF3 MT parser smoke test in Flycast and confirm its PASS result (user confirmed).
- [ ] Establish the first actual game-runtime executable target.
- [ ] Verify Naomi PVR2 video, AICA audio and ROMFS-backed asset reads in
  isolated probes before attempting full game integration.

## Build artifact policy

All future Naomi ROM builds, including short-lived probes and parser tests, must have unique `vN` build versions. Include the version in the ROM filename, Naomi title string, GitHub Actions artifact name and `BUILD_INFO.txt`. Keep previous artifacts available. Reuse cached compiler/toolchain/object outputs where safe, but package a fresh versioned ROM for every run.

## Risks

- Dreamcast and Naomi share major chips but differ in platform boot, memory configuration, peripherals and storage.
- Decompiled game source is not yet a complete playable reconstruction.
- SH-4 recompilation frameworks are experimental; do not assume they provide a ready Naomi runtime.
- PVR2 and AICA behavior must be validated on actual Naomi hardware, not inferred from shared architecture alone.
