# Dreamcast GD-ROM syscall redirection: new primary porting path

Date: 2026-11-26

## Decision

The primary target remains the **Dreamcast retail VF3tb executable** (1ST_READ.BIN) and its existing assets. Do not replace this target with the Model 3 arcade build or continue the Model 3 PowerPC/Real3D reconstruction experiments.

The YouTube video's accompanying comments describe a direct Dreamcast-to-NAOMI technique:

1. Rewrite/intercept the low-level GD-ROM syscall path so reads are redirected to the NAOMI cartridge/NetDIMM.
2. Use DMA for the data transfer rather than having the SH-4 copy data through PIO.
3. Preserve the original game and its resource-loading behavior as much as possible.

This is a **working implementation lead from the video's comments**, not independently verified source code. We should test the same architecture against our own retail executable and NAOMI hardware.

## What our repository already tells us

- The retail Dreamcast 1ST_READ.BIN is the intended executable. Its inventory and hash are recorded in the main README.
- The executable contains Dreamcast GDFS/GD-ROM code and also embedded references to Naomi source modules. The references alone do not prove those Naomi routines are callable or active.
- The related celsowm/vf3-decomp research contains a REIOS GD-ROM syscall model. Its bios_syscall.c handles REQ_CMD, GET_CMD_STAT, EXEC_SERVER, REQ_DMA_TRANS, CHECK_DMA_TRANS, READ_ABORT and related services. Its gdrom_hle.c models DMAREAD request parameters and the transfer state machine. These are useful behavioral references, not a drop-in Naomi driver.
- Our tests/naomi_dma_probe/ compares a Naomi cartridge PIO read with a Holly System Bus GD-DMA read at the same image offset. It deliberately bypasses Dreamcast GD-ROM syscalls. It is therefore a low-level hardware transport probe, **not** the syscall redirection implementation described by the video.
- The existing GDFS/resource-loader analysis identifies filename lookup, FAD/LBA mapping and guest destination-buffer behavior. That work may help validate that the shim supplies the exact sectors and byte counts the game expects.

## New implementation target

Build a **VF3 Dreamcast GD-ROM compatibility shim backed by NAOMI NetDIMM DMA**.

The intended flow is:

VF3 GD-ROM/GDFS request → compatibility shim → NetDIMM cartridge DMA → original guest RAM destination → VF3 completion/status path

The first implementation should preserve the original request/completion contract. It must not merely read a raw image offset and display a PASS marker.

### Stage A — establish the exact guest interface

- Audit the retail executable's actual GD-ROM syscall entry path and the GDFS functions it invokes.
- Identify the commands used during boot and gameplay, the request parameter layout, FAD/LBA convention, destination address, sector count, alignment, status polling and error behavior.
- Distinguish BIOS GD-ROM syscalls from GDFS library calls such as _gdFsDaPlaySct. Do not assume the ABI of one is the ABI of the other.
- Reuse the related REIOS syscall model as a checklist for expected semantics, but verify every field against the retail executable.

### Stage B — implement the NAOMI DMA backend

- Reuse the known-good register setup and bounds checks from the existing cartridge/DIMM DMA probe.
- Transfer NetDIMM image sectors directly into the guest destination buffer using the NAOMI/Holly DMA path.
- Handle FAD-to-image-offset conversion, track/data-sector offsets, sector rounding, destination alignment and transfer limits.
- Do not route bulk data through a CPU PIO copy loop.
- Keep a small PIO read only as an optional diagnostic/reference, not the production data path.

### Stage C — connect the compatibility layer

- Intercept or replace the guest GD-ROM service at the narrowest proven boundary.
- Preserve request IDs, BUSY/COMPLETE/error states, result words and the game's expected polling cadence.
- Support the commands actually observed in VF3; do not implement a speculative full GD-ROM device unless evidence requires it.
- Confirm that sectors land at the exact addresses and byte counts expected by the original VF3 loader.

### Stage D — boot the original game and validate real resources

- Start with the original retail 1ST_READ.BIN and matching disc assets.
- Reach the first real resource loads; validate filenames/FAD ranges and destination buffers against the existing asset index.
- Progress through boot, title/menu, character selection and a match.
- Only investigate a loader or asset format further when the syscall/DMA path demonstrates a concrete mismatch.

## Explicitly paused

Until the syscall/DMA path is implemented and exercised against the original game, do not spend more time on:

- Model 3 PPC interpreter, CROM/VROM or Real3D reconstruction;
- repeated MT evaluator/pointer-watch Flycast captures;
- further synthetic triangle, scene-builder or synthetic-ROM integration milestones;
- standalone input traces without an actual game boot requiring them.

Keep the old code, captures, reports and successful probes in the repository as historical research. Do not delete them or treat them as the current critical path.

## Reusable work

Retain and reuse where relevant:

- Naomi NetDIMM image packaging and versioned GitHub Actions builds;
- the existing cartridge PIO / Holly GD-DMA register probe as a hardware reference;
- verified JVS input and Naomi System Test Menu hand-off;
- retail executable identity, GDFS asset index and loader/resource analysis;
- Dreamcast syscall semantics from the related REIOS research.

## Definition of the next meaningful milestone

A versioned NAOMI ROM boots the **original Dreamcast VF3tb executable**, submits a real VF3 GD-ROM/GDFS read request, services it by DMA from the NetDIMM image into the requested guest RAM destination, and returns the completion/status expected by the game. A raw DMA probe pass, synthetic data transfer, or successful KallistiOS startup alone does not meet this milestone.