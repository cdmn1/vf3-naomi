# Flycast tracing toolchain — availability and reproducibility

Reviewed: 2026-11-17

## Current status

A reproducible first-pass tracer is maintained in `tools/flycast-trace/` and built against a pinned public upstream revision:

- Upstream: `flyinghead/flycast`
- Pinned commit: `3a5267a5e32438026e1c5b97d5de6cd77236f72c`
- Patch and Windows launcher: `tools/flycast-trace/`
- Build workflow: `.github/workflows/flycast-trace.yml`

The initial Windows build (run `43915068665`) failed during the Breakpad configure step. The workflow had not actually passed `USE_BREAKPAD=OFF` to CMake, so the optional Breakpad dependency was still built; its MinGW dependency-tracking bootstrap failed. The workflow now explicitly disables Breakpad and OpenMP for this tracing build. This is a build configuration correction, not a change to Flycast runtime behavior.

## What the maintained first-pass patch implements

- `VF3_INTERPRETER=1` forces the SH-4 interpreter.
- `VF3_WATCH` reads `pc 0xADDR` and `mem 0xADDR 0xLEN` entries.
- Watched PCs emit the eight-u64 `FA10/FA11` register probe group.
- Watched guest-memory reads and writes emit four-u64 `FA20` records understood by `tools/loader_trace_decode.py`.
- `run-trace.cmd` sets the environment and points output to `vf3-loader-trace.bin` beside the executable. The user does not need to create that file or type environment variables.

### First capture target: original guest input path

The focused default input watch list (`VF3_WATCH_INPUT.txt`) includes:

| Guest PC | Reconstructed role |
|---|---|
| `0x8C09B006` | Selected controller-port reader |
| `0x8C09AF6C` | Controller input merge |
| `0x8C0AD1B8` | Player action update |

These addresses originate in the related VF3 decompilation/research notes. They are hypotheses about the original executable's input path until this tracer records actual hits. A separate `VF3_WATCH_LOADER.txt` preserves the GDFS/MT loader watches and memory ranges. Keeping the first input capture PC-only avoids the potentially large memory trace and makes its result easier to inspect.

The user should load the **Dreamcast** VF3 image (not the Naomi probe ROM), use Start/confirm and gameplay controls to navigate as far as possible, ideally into a player-controlled match, and close Flycast normally. The launcher defaults to `VF3_WATCH_INPUT.txt`; the loader profile is retained separately for a later capture. If the game remains in attract/demo mode, the capture is still useful: absence of hits in the three controller routines confirms only that they were not executed during that capture.

### Deliberate limits

This does **not** recreate the private research build's `VF3_FULL` entry/exit oracle snapshots, frame-clock `vf3script` commands (`KEYF`, `SAVEF`, `EXITF`), or savestate staging (`VF3_STATE`). The current launcher expects manual navigation in the Flycast GUI. Do not claim those omitted features exist.

## Why the old executable could not be downloaded

The related research repository `celsowm/vf3-decomp` documents a locally modified Flycast checkout at `tools/emu/flycast`. Its root `.gitignore` excludes `tools/emu/`, so neither that source checkout nor its executables are included in the public repository.

The related documentation names local outputs such as:

- `tools/emu/flycast-build/flycast.exe` (headless tracing build)
- `tools/emu/flycast-build/flycast_v3.exe` (later headless build)
- `tools/emu/flycast-build-gui/flycast.exe` (GUI/navigation build)

These are paths on the original research machine, not downloadable assets. The public notes do not identify the exact source revision or publish the custom executable. The pinned upstream commit above is therefore our reproducible baseline, not a claim that it matches the old private fork.

## Verification gates before asking the user to run it

1. Workflow completes successfully.
2. ZIP exists and contains `flycast.exe`, required runtime files, `run-trace.cmd`, `VF3_WATCH_INPUT.txt`, and `VF3_WATCH_LOADER.txt`.
3. SHA-256 is recorded and the archive is inspected.
4. Confirm the executable launches and the trace file is created using a short smoke test.
5. Only then provide one ZIP and short Windows instructions. Keep trace decoding and analysis on the development side.

Until those steps are complete, the user's required action is **none**. In particular, `vf3-loader-trace.bin` is an output created by the launcher, not a file the user should already possess.

## Scope distinction

The existing `vf3-decomp` captures and savestates are evidence from the Dreamcast research environment. They are not captures from this Naomi project, and the documented fight savestate was already past the one-time character/stage loading boundary. The current tracer first targets the missing guest controller-path evidence. Resource loading is a separate follow-up capture using `VF3_WATCH_LOADER.txt`.
