# Flycast tracing toolchain — availability and reproducibility

Reviewed: 2026-11-16

## Current status

A reproducible first-pass tracer is now in the Naomi repository under `tools/flycast-trace/`. It is being built against a pinned public upstream revision:

- Upstream: `flyinghead/flycast`
- Pinned commit: `3a5267a5e32438026e1c5b97d5de6cd77236f72c`
- Patch and Windows launcher: `tools/flycast-trace/`
- Build workflow: `.github/workflows/flycast-trace.yml`

The first Windows workflow run is `43915068665`:
https://github.com/cdmn1/vf3-naomi/actions/runs/43915068665

At the time of this update, it is still checking out upstream Flycast. No executable artifact has been published or verified yet. Do not give the user a download link until the build completes and the ZIP has been inspected.

## What the maintained first-pass patch implements

- `VF3_INTERPRETER=1` forces the SH-4 interpreter.
- `VF3_WATCH` reads `pc 0xADDR` and `mem 0xADDR 0xLEN` entries.
- Watched PCs emit the eight-u64 `FA10/FA11` register probe group.
- Watched guest-memory reads and writes emit four-u64 `FA20` records understood by `tools/loader_trace_decode.py`.
- `run-trace.cmd` sets the environment and points output to `vf3-loader-trace.bin` beside the executable. The user will not need to create that file or type environment variables.

This is a deliberate minimum for the loader-transition capture. It does **not** yet recreate the private research build's `VF3_FULL` entry/exit oracle snapshots, frame-clock `vf3script` commands (`KEYF`, `SAVEF`, `EXITF`), or savestate staging (`VF3_STATE`). The current launcher expects manual navigation in the Flycast GUI. Do not claim those omitted features exist.

## Why the old executable could not be downloaded

The related research repository `celsowm/vf3-decomp` documents a locally modified Flycast checkout at `tools/emu/flycast`. Its root `.gitignore` excludes `tools/emu/`, so neither that source checkout nor its executables are included in the public repository.

The related documentation names local outputs such as:

- `tools/emu/flycast-build/flycast.exe` (headless tracing build)
- `tools/emu/flycast-build/flycast_v3.exe` (later headless build)
- `tools/emu/flycast-build-gui/flycast.exe` (GUI/navigation build)

These are paths on the original research machine, not downloadable assets. The public notes do not identify the exact source revision or publish the custom executable. The pinned upstream commit above is therefore our new reproducible baseline, not a claim that it matches the old private fork.

## Verification gates before asking the user to run it

1. Workflow completes successfully.
2. ZIP exists and contains `flycast.exe`, required runtime files, `run-trace.cmd`, and `VF3_WATCH_LOADER.txt`.
3. SHA-256 is recorded and the archive is inspected.
4. Confirm the executable launches and the trace file is created using a short smoke test.
5. Only then provide one ZIP and short Windows instructions. Keep trace decoding and analysis on the development side.

Until those steps are complete, the user's required action is **none**. In particular, `C:\\vf3-loader-trace.bin` is a future output path, not a file the user should already possess.

## Scope distinction

The existing `vf3-decomp` captures and savestates are evidence from the Dreamcast research environment. They are not captures from this Naomi project, and the documented fight savestate was already past the one-time character/stage loading boundary. The new tracer is intended to capture the missing transition from character select into Round 1.
