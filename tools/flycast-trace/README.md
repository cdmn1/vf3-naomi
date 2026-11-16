# VF3 Flycast trace build

This directory contains the maintained patch applied to the pinned upstream Flycast revision:

- Repository: https://github.com/flyinghead/flycast
- Commit: `3a5267a5e32438026e1c5b97d5de6cd77236f72c`

The patch implements the project's current loader-trace requirements:

- `VF3_INTERPRETER=1` forces the SH-4 interpreter.
- `VF3_WATCH` reads `pc 0xADDR` and `mem 0xADDR 0xLEN` entries.
- Watched PCs emit the eight-u64 `FA10/FA11` register probe group.
- Watched memory reads/writes emit the four-u64 `FA20` records understood by `tools/loader_trace_decode.py`.

It deliberately does not claim to implement the separate `VF3_FULL` oracle snapshots, frame-clock `vf3script` commands, or savestate staging from the private research build. Those are not required for the first loader capture and are not silently emulated here.

The Windows package includes `run-trace.cmd` and `VF3_WATCH_LOADER.txt`. Run the command file, load the Dreamcast VF3 image in Flycast, navigate from character select into Round 1, and close Flycast normally. The trace is written beside the executable as `vf3-loader-trace.bin`.

The binary is not ready for user use until the GitHub Actions build succeeds and the artifact has been inspected.
