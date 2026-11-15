# Virtua Fighter 3tb → Sega Naomi

A separate research and porting project investigating a path to run the Dreamcast version of **Virtua Fighter 3tb** on Sega Naomi hardware, with NetDIMM deployment as the eventual target.

This repository is independent of the Daytona Naomi port. Daytona's proven build, ROMFS, encryption and NetDIMM deployment workflow may be reused where applicable, but its game-specific code and controls are not part of this project.

## Current status

**Phase 0 — repository and technical investigation.**

The initial target is the Dreamcast VF3tb executable and assets, not the Model 3 arcade recompilation. Dreamcast and Naomi share the SH-4 CPU, PowerVR2 graphics family and AICA audio architecture, making this the most promising starting point. They are not binary-compatible by default: Naomi boot, memory layout, storage, peripherals and arcade I/O must be checked.

No playable port or hardware compatibility is claimed yet.

## Chosen research sources

- [Dreamcast VF3tb native decompilation](https://github.com/celsowm/vf3-decomp) — game/asset research; currently incomplete.
- [Dreamcast/Naomi SH-4 recompilation framework](https://github.com/sp00nznet/dcrecomp) — useful architectural reference; currently early-stage.
- [Model 3 arcade VF3 recompilation](https://github.com/mr-finster/vf3-recomp) — arcade behavior/reference, but its PPC 603e and Real3D Model 3 platform are not a direct Naomi base.

## First milestone

Inspect a legally obtained Dreamcast VF3tb executable (1ST_READ.BIN) and map:

1. executable format, entry point and SH-4 code characteristics;
2. memory addresses and hardware-register access;
3. boot and runtime dependencies on Dreamcast services;
4. file, disc and asset loading;
5. graphics, sound and controller initialization.

Then build a minimal Naomi-side boot/runtime test before attempting game integration.

## Proposed milestones

1. **Executable reconnaissance** — identify binary layout, imports/calls, memory map and platform dependencies.
2. **Naomi compatibility proof** — boot a minimal program, initialize video, read inputs and establish audio/device strategy.
3. **Runtime adaptation** — implement only the Dreamcast services the game actually uses, replacing disc access with Naomi ROMFS/NetDIMM-backed assets where feasible.
4. **First game scene** — bring up a title/menu or other controlled scene, then a playable match.
5. **Deployment** — validate in Flycast, then on Naomi hardware; package through the established NetDIMM pipeline.

## Inputs and controls

The target is a fighting-game layout, not Daytona's racing controls. The intended mapping is directions plus punch, kick, guard and any additional required buttons, using Naomi/JVS input where available. Exact mapping will be established from the executable and hardware tests.

## Required input

The first required game artifact is the Dreamcast VF3tb 1ST_READ.BIN extracted from the user's own disc dump. Do not commit copyrighted game binaries, ROMs or extracted commercial assets to this repository. Keep such files local and use them only for analysis/builds as appropriate.

## Working rules

- Keep this project separate from the Daytona repository.
- Record verified facts separately from hypotheses and TODOs.
- Do not claim compatibility until demonstrated in an emulator or on hardware.
- Prefer small, testable Naomi probes over large speculative rewrites.
