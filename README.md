# Virtua Fighter 3tb → Sega Naomi

A separate research and porting project investigating a path to run the Dreamcast version of **Virtua Fighter 3tb** on Sega Naomi hardware, with NetDIMM deployment as the eventual target.

This repository is independent of the Daytona Naomi port. Daytona's proven build, ROMFS, encryption and NetDIMM deployment workflow may be reused where applicable, but its game-specific code and controls are not part of this project.

## Current status

**Phase 1 — executable reconnaissance in progress. Naomi JVS input and the System Test Menu hand-off are verified on real hardware.**

The initial target is the Dreamcast VF3tb executable and assets, not the Model 3 arcade recompilation. Dreamcast and Naomi share the SH-4 CPU, PowerVR2 graphics family and AICA audio architecture, making this the most promising starting point. They are not binary-compatible by default: Naomi boot, memory layout, storage, peripherals and arcade I/O must be checked.

Our supplied US retail executable is recorded as 1,179,648 bytes with SHA-256 `37f38dd9a47b785f526157c4a1674ca75322f0de1342f64bbf33c88d128e4b32`. Initial analysis found Naomi library identifiers and code references to Naomi source modules, alongside Dreamcast-specific GDFS, Maple and memory-card material. These references do not yet prove normal gameplay reachability or direct Naomi compatibility.

A related project, [celsowm/vf3-decomp](https://github.com/celsowm/vf3-decomp), documents a Dreamcast research harness that loads 367 disc files and reaches a rendered attract fight, plus a separate incomplete native C reconstruction. We use its documented GDFS, GD-ROM, Maple, PVR2 and AICA findings as a behavioral reference—not as proof of Naomi compatibility. See [related-project findings](docs/RELATED_PROJECT_FINDINGS.md). The user's 412-file Dropbox set matches the related project's US GD-ROM manifest by filename and size; its corrected embedded filename-string inventory has 234 candidate rows, 232 of which align with extracted `.BIN` assets after normalizing leading underscore prefixes. This is distinct from the 132-pointer runtime table identified at `0x8C1069BC`. The loader and asset-boundary analysis is in [GDFS_RESOURCE_MAP.md](docs/GDFS_RESOURCE_MAP.md).

No playable Naomi port or hardware compatibility is claimed yet.

## Immediate next task: execute actual VF3 code on Naomi

The first standalone Naomi executable is now built and confirmed booting in Flycast. The known-good baseline is in `tests/naomi_bootstrap/`; it proves KallistiOS startup and visible PowerVR2 output.

The next target, `tests/naomi_vf3core_smoke/`, cross-compiles the real `mt_parse()` and `mt_each()` motion-table parser from the pinned `celsowm/vf3-decomp` revision. It runs those functions on a synthetic in-memory MT fixture and checks both a valid record and a short-input rejection. This deliberately avoids bundling commercial game data. The workflow uploads the already-working bootstrap separately before attempting the new VF3-code build, so a parser build failure cannot take away the known-good ROM.

After the parser passed, the storage investigation moved to the Naomi cartridge/DIMM path. The new versioned `tests/naomi_dma_probe/` compares cartridge PIO and Holly System Bus GD-DMA reads from the same image offset. It does not call Dreamcast BIOS GD-ROM syscalls. The withdrawn v19 probe remains invalid and should not be tested. A successful PIO/DMA comparison will establish raw image-offset access; GDFS filename/FAD mapping and VF3's loader scatter/re-site behavior remain separate tasks. ROM-derived asset checkpoints will be encrypted and content-addressed; plaintext commercial data will not be placed in public caches or artifacts.

Input-path findings are documented and frozen for now. Do not launch more input-trace builds unless the running game later gives us a concrete reason to revisit them.

## Resource-loader investigation

The 132-entry runtime name-pointer table, candidate MT loaders and GDFS sector-read endpoint are documented in [the loader trace plan](docs/GDFS_TRACE_PLAN.md). The trace decoder is [tools/loader_trace_decode.py](tools/loader_trace_decode.py), with the loader-specific report generator at [tools/loader_trace_summary.py](tools/loader_trace_summary.py). Synthetic regression tests are in [tests/](tests/) and run with `python -m unittest discover -s tests -v`.

## Chosen research sources

- [Dreamcast VF3tb native decompilation](https://github.com/celsowm/vf3-decomp) — asset and gameplay reconstruction plus a separate Dreamcast runtime research harness; useful evidence, but neither is a completed Naomi port.
- [Dreamcast/Naomi SH-4 recompilation framework](https://github.com/sp00nznet/dcrecomp) — useful architectural reference; early-stage.
- [Model 3 arcade VF3 recompilation](https://github.com/mr-finster/vf3-recomp) — arcade behavior/reference, but its PPC 603e and Real3D Model 3 platform are not a direct Naomi base.

## Naomi I/O bring-up

A standalone KallistiOS MIE/JVS probe is in
[`tests/naomi_mie_probe/`](tests/naomi_mie_probe/README.md). It reports native
JVS switch words, KOS-mapped controller state, coin meters and analog
channels. The API and firmware caveats are documented in
[`NAOMI_IO_RESEARCH.md`](docs/NAOMI_IO_RESEARCH.md). Its Naomi ROM packaging,
JVS input path, and System Test Menu hand-off have been exercised in Flycast
and on real Naomi hardware. This remains a hardware-validation tool, not a
VF3 game port.

## Reusable Naomi System Test Menu hand-off

The VF3 probe established and hardware-validated a reusable KallistiOS
pattern for returning to the Naomi BIOS System Test Menu: request
`ARCH_EXIT_MENU`, call `exit(0)`, let KOS complete its normal shutdown, then
invoke the Naomi 1 BIOS vector from the Naomi-specific `arch_menu()` exit
destination. Directly calling the BIOS vector from a live KOS application
was tested and failed.

The complete integration recipe, KOS patch, source references, validation
record and reuse checklist are in
[`docs/KOS_NAOMI_SYSTEM_TEST_HANDOFF.md`](docs/KOS_NAOMI_SYSTEM_TEST_HANDOFF.md).
The flow is confirmed in Flycast and on real Naomi 1 hardware, including
both physical TEST entry and GAME TEST MODE EXIT. This is a platform-level
handoff, not a complete title-specific test menu.

## Proposed milestones

1. **Executable reconnaissance** — identify binary layout, imports/calls, memory map and platform dependencies.
2. **Naomi compatibility proof** — boot a minimal program, initialize video, read inputs and establish audio/device strategy.
3. **Runtime adaptation** — implement only the Dreamcast services the game actually uses, replacing disc access with Naomi ROMFS/NetDIMM-backed assets where feasible.
4. **First game scene** — bring up a title/menu or other controlled scene, then a playable match.
5. **Deployment** — validate in Flycast, then on Naomi hardware; package through the established NetDIMM pipeline.

## Inputs and controls

The target is a fighting-game layout, not Daytona's racing controls. The intended mapping is directions plus punch, kick, guard and any additional required buttons, using Naomi/JVS input where available. Exact mapping will be established from the executable and hardware tests.

## Required input

The Dreamcast VF3tb `1ST_READ.BIN` has already been inventoried. Do not commit copyrighted game binaries, ROMs or extracted commercial assets to this repository. Keep such files local and use them only for analysis/builds as appropriate.

## Build versioning

Every Naomi build artifact, including diagnostic probes and parser smoke tests, is versioned with the GitHub Actions run number (`vN`). The version is included in the ROM filename, Naomi disc title, artifact name and `BUILD_INFO.txt`. Previous run artifacts are retained independently; no new build overwrites an earlier ROM. Object files and toolchain outputs may still be cached, but ROM packaging is performed for each version.

## Working rules

- Keep this project separate from the Daytona repository.
- Record verified facts separately from hypotheses and TODOs.
- Do not claim compatibility until demonstrated in an emulator or on hardware.
- Prefer small, testable Naomi probes over large speculative rewrites.
