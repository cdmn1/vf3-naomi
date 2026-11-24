# VF3 Flycast trace build

This directory contains the maintained patch applied to the pinned upstream Flycast revision:

- Repository: https://github.com/flyinghead/flycast
- Commit: `3a5267a5e32438026e1c5b97d5de6cd77236f72c`

The patch implements the project's current trace requirements:

- `VF3_INTERPRETER=1` forces the SH-4 interpreter.
- `VF3_WATCH` reads `pc 0xADDR` and `mem 0xADDR 0xLEN` entries.
- Watched PCs emit an expanded `FA12/FA13` probe containing PR and R0-R15 (including the stack pointer in R15). The decoder remains backward-compatible with older `FA10/FA11` records.
- Watched memory reads/writes emit the four-u64 `FA20` records understood by `tools/loader_trace_decode.py`.

The default input watch profile prioritizes three original guest input routines:

- `0x8C09B006` — selected-port reader.
- `0x8C09AF6C` — controller merge.
- `0x8C0AD1B8` — player action update.
- `0x8C0A1282` — packet lookup called by the controller routines.

The input profile is intentionally PC-only and compact. The separate `VF3_WATCH_LOADER.txt` retains the GDFS/MT resource and memory watches. Use `run-trace.cmd` for the controller-path capture; use `run-trace-loader.cmd` for the loader-focused capture needed to resolve the runtime resource table, GDFS sector-read contract and MT resident scatter path.

It deliberately does not claim to implement the separate `VF3_FULL` oracle snapshots, frame-clock `vf3script` commands, or savestate staging from the private research build. Those features are not required for this first controller-path capture and are not silently emulated here.

The Windows package includes both launchers and both watch profiles. For the next storage investigation, run `run-trace-loader.cmd`, load the **Dreamcast** VF3 image (not the Naomi probe ROM), navigate through character selection and confirm into Round 1, then close Flycast normally. The loader trace is written beside the executable as `vf3-loader-trace-loader.bin`; its `.diag.txt` companion records watched-PC hit counts. This profile includes broad memory watches, so a large trace file is expected. The separate `run-trace.cmd` remains available for the compact controller-path capture.

The binary is not ready for user use until the GitHub Actions build succeeds, the ZIP has been inspected, and a short launch/trace smoke test passes.


## Pose capture profile v73

The full pose watch profile now includes the MT-player entry snapshot at `0x8C09D690`, candidate MT task cycle/header fields, the complete SEG_A and SEG_B vector blocks, and both caller argument blocks identified from the v72 R5 snapshots. Use the same two-character selection and matchup as the v72 capture.
