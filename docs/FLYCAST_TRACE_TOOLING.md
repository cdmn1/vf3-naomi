# Flycast tracing toolchain — availability and reproducibility

Reviewed: 2026-11-16

## Current status

The Flycast executable required by `docs/GDFS_TRACE_PLAN.md` is **not a published download**.

The related research repository `celsowm/vf3-decomp` documents a locally modified Flycast checkout at `tools/emu/flycast`. Its root `.gitignore` excludes `tools/emu/`, so neither that source checkout nor the built executables are included in the public repository.

The related documentation names local outputs such as:

- `tools/emu/flycast-build/flycast.exe` (headless tracing build)
- `tools/emu/flycast-build/flycast_v3.exe` (later headless build)
- `tools/emu/flycast-build-gui/flycast.exe` (GUI/navigation build)

These are paths on the original research machine, not downloadable assets. The public notes do not identify a Flycast release number, upstream commit, fork URL, or a published binary artifact for the exact instrumented build. Therefore we cannot truthfully give the user an exact Flycast version or a download link yet.

## What the research notes establish

The required hooks are custom changes, not standard Flycast features:

- `VF3_WATCH` PC and memory watch parsing, plus register/memory trace records.
- `VF3_INTERPRETER` / interpreter-only watch execution.
- `VF3_FULL` trace mode.
- `VF3_TRACE` and `VF3_TRACE_FRAMES` output controls.
- `vf3script` frame-clock input commands, including `KEYF`, `SAVEF`, and `EXITF`.

The documented Flycast fork was built locally with MinGW/MSYS2 UCRT64. The public documentation gives a local CMake build command, but because the actual fork checkout and its exact source revision are ignored, that command alone is not a reproducible build recipe.

The existing `vf3-decomp` captures and savestates are evidence from the Dreamcast research environment. They are not captures from this Naomi project, and the documented fight savestate was already past the one-time character/stage loading boundary.

## Required action before asking the user to run anything

Do not ask the user to set environment variables, create a trace file, install Python, or navigate Flycast until we have supplied a verifiable executable.

First make the toolchain reproducible on the project side:

1. Identify and pin the exact upstream Flycast base revision.
2. Recreate only the required trace/watch and frame-script hooks as a maintained patch set (or a small public fork).
3. Add a Windows build workflow that compiles the instrumented headless executable and publishes it as a workflow artifact, with source revision and SHA-256 recorded.
4. Verify the artifact actually starts and accepts the documented environment variables.
5. Provide the user with one ZIP containing the executable and the project's watch file, plus a short launch procedure. Keep trace decoding and analysis on the development side.

Until those steps are complete, the user's required action is **none**. In particular, `C:\\vf3-loader-trace.bin` is a future output path, not a file the user should already possess.
