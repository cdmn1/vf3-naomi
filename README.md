# Virtua Fighter 3tb → Sega Naomi

A separate research and porting project investigating a path to run the Dreamcast version of **Virtua Fighter 3tb** on Sega Naomi hardware, with NetDIMM deployment as the eventual target.

This repository is independent of the Daytona Naomi port. Daytona's proven build, ROMFS, encryption and NetDIMM deployment workflow may be reused where applicable, but its game-specific code and controls are not part of this project.

## Current status

**Phase 1 — executable reconnaissance in progress.**

The initial target is the Dreamcast VF3tb executable and assets, not the Model 3 arcade recompilation. Dreamcast and Naomi share the SH-4 CPU, PowerVR2 graphics family and AICA audio architecture, making this the most promising starting point. They are not binary-compatible by default: Naomi boot, memory layout, storage, peripherals and arcade I/O must be checked.

Our supplied US retail executable is recorded as 1,179,648 bytes with SHA-256 `37f38dd9a47b785f526157c4a1674ca75322f0de1342f64bbf33c88d128e4b32`. Initial analysis found Naomi library identifiers and code references to Naomi source modules, alongside Dreamcast-specific GDFS, Maple and memory-card material. These references do not yet prove normal gameplay reachability or direct Naomi compatibility.

A related project, [celsowm/vf3-decomp](https://github.com/celsowm/vf3-decomp), now documents a Dreamcast research harness that loads 367 disc files and reaches a rendered attract fight, plus a separate incomplete native C reconstruction. We will use its documented GDFS, GD-ROM, Maple, PVR2 and AICA findings as a behavioral reference—not as proof of Naomi compatibility. See [related-project findings](docs/RELATED_PROJECT_FINDINGS.md). The user's 412-file Dropbox set matches the related project's US GD-ROM manifest by filename and size; its corrected embedded filename-string inventory has 234 candidate rows, 232 of which align with extracted `.BIN` assets after normalizing leading underscore prefixes. This is distinct from the 132-pointer runtime table identified at `0x8C1069BC`. The loader and asset-boundary analysis is in [GDFS_RESOURCE_MAP.md](docs/GDFS_RESOURCE_MAP.md).

No playable Naomi port or hardware compatibility is claimed yet.

## Current loader investigation

The current blocking reverse-engineering task is to capture the character-select-to-round-one resource load. The 132-entry runtime name-pointer table, candidate MT loaders and GDFS sector-read endpoint are documented in [the loader trace plan](docs/GDFS_TRACE_PLAN.md). A combined watch list is provided at [tools/VF3_WATCH_LOADER.txt](tools/VF3_WATCH_LOADER.txt), with a CSV decoder at [tools/loader_trace_decode.py](tools/loader_trace_decode.py) and a first-pass report generator at [tools/loader_trace_summary.py](tools/loader_trace_summary.py). Synthetic regression tests are in [tests/](tests/) and run with `python -m unittest discover -s tests -v`. A pre-fight savestate is an optional convenience, not a required user-provided artifact: the intended route is to start the game in a research Flycast build and drive it to the load boundary once the input sequence is reliable. Current Maple/input findings and the distinction between the related project's native harness and patched Flycast captures are recorded in [INPUT_PATH_FINDINGS.md](docs/INPUT_PATH_FINDINGS.md). A real capture still requires the patched interpreter hooks.

## Chosen research sources

- [Dreamcast VF3tb native decompilation](https://github.com/celsowm/vf3-decomp) — asset and gameplay reconstruction plus a separate Dreamcast runtime research harness; useful evidence, but neither is a completed Naomi port.
- [Dreamcast/Naomi SH-4 recompilation framework](https://github.com/sp00nznet/dcrecomp) — useful architectural reference; early-stage.
- [Model 3 arcade VF3 recompilation](https://github.com/mr-finster/vf3-recomp) — arcade behavior/reference, but its PPC 603e and Real3D Model 3 platform are not a direct Naomi base.

## Naomi I/O bring-up

A standalone KallistiOS MIE/JVS probe is now in
[`tests/naomi_mie_probe/`](tests/naomi_mie_probe/README.md). It reports the
native JVS switch words, KOS-mapped controller state, coin meters and analog
channels. The API and firmware caveats are documented in
[`NAOMI_IO_RESEARCH.md`](docs/NAOMI_IO_RESEARCH.md). The repository now has a GitHub Actions workflow to build KallistiOS for
Naomi and compile the probe. The first run is underway; until it completes,
no successful build or runtime test is claimed. This is a hardware-validation
tool, not a VF3 port.

## First milestone

Inspect a legally obtained Dreamcast VF3tb executable (`1ST_READ.BIN`) and map:

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

The first required game artifact is the Dreamcast VF3tb `1ST_READ.BIN` extracted from the user's own disc dump. Do not commit copyrighted game binaries, ROMs or extracted commercial assets to this repository. Keep such files local and use them only for analysis/builds as appropriate.

## Working rules

- Keep this project separate from the Daytona repository.
- Record verified facts separately from hypotheses and TODOs.
- Do not claim compatibility until demonstrated in an emulator or on hardware.
- Prefer small, testable Naomi probes over large speculative rewrites.
