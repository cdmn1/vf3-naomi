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

The related REIOS research gives us a concrete starting contract to verify against the retail executable (these are Dreamcast-side reference findings, not yet proven as the correct Naomi hook):

- BIOS service pointer: guest RAM word at 0x8C0000BC points to the REIOS entry at 0x8C001006; the modeled syscall uses r6 = 0.
- The retail loader's modeled stubs are at 0x8C0430A0 / 0x8C0430B4.
- r7 = 0 is REQ_CMD: r4 carries the command, r5 points to four parameter words, and r0 returns a request ID.
- Command 0x11 is DMAREAD. The current REIOS model interprets parameter words 0, 1 and 2 as FAD, sector count and guest destination address, respectively.
- r7 = 2 is EXEC_SERVER. r7 = 1 is GET_CMD_STAT: r4 is the request ID, r5 points to four result words, and r0 returns the status. The model defines idle/OK=0, BUSY=1, COMPLETE=2, CONTINUE=3 and ERR=-1.
- The GDFS sector-read endpoint at 0x8C0360C8 is a separate layer to distinguish from the BIOS service entry. The shim should hook the narrowest boundary that the actual executable uses, not blindly replace both.

- Audit the retail executable's actual GD-ROM syscall entry path and the GDFS functions it invokes.
- Identify the commands used during boot and gameplay, the request parameter layout, FAD/LBA convention, destination address, sector count, alignment, status polling and error behavior.
- Distinguish BIOS GD-ROM syscalls from GDFS library calls such as _gdFsDaPlaySct. Do not assume the ABI of one is the ABI of the other.
- Reuse the related REIOS syscall model as a checklist for expected semantics, but verify every field against the retail executable.

### Stage B — implement the NAOMI DMA backend

- Use the register sequence in the existing cartridge/DIMM DMA probe as the starting candidate; verify DMA completion, destination visibility and NetDIMM behavior before treating it as a working backend.
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
## Direct verification against the retail 1ST_READ.BIN

On 2026-11-26, GitHub Actions downloaded the already-shared `/Vf3-naomi/` folder, extracted `1ST_READ.BIN`, and verified the exact retail image:

- Size: `1,179,648` bytes
- SHA-256: `37f38dd9a47b785f526157c4a1674ca75322f0de1342f64bbf33c88d128e4b32`

A little-endian SH-4 disassembly of that exact image confirms a contiguous family of BIOS-service trampolines. Each loads the service-vector address `0x8C0000BC`, dereferences it, and jumps through the resulting pointer:

| Stub address | `r6` | `r7` | Related REIOS model operation |
|---|---:|---:|---|
| `0x8C04308C` | 0 | 0 | `REQ_CMD` |
| `0x8C0430A0` | 0 | 1 | `GET_CMD_STAT` |
| `0x8C0430B4` | 0 | 2 | `EXEC_SERVER` |
| `0x8C0430C8` | 0 | 3 | `INIT_SYSTEM` |
| `0x8C0430DC` | 0 | 4 | `GET_DRV_STAT` |
| `0x8C0430F0` | 0 | 5 | `G1_DMA_END` (model enum) |

The first three stubs are the important initial read path. In particular, the retail binary itself proves the `r6=0` selector and the `r7=0/1/2` call wrappers; the command names and `0x11 = DMAREAD` interpretation come from the related project's `bios_syscall.h` / `gdrom_hle.h` model and still need to be checked against the callers in this executable.

The executable contains repeated little-endian literals for `0x8C0000BC` at file offsets `0x3309C`, `0x330B0`, `0x330C4`, continuing in `0x14`-byte steps. The corresponding stubs set `r7` and dispatch through that service vector. There are also alternate stubs with `r6 = -1`; do not assume those belong to the GD-ROM path until their callers are identified.

The image also contains the strings `GDFS Version 0.53`, `gdFs Build: Jan 26 1999`, `DMA incompleted.`, `DMA completed.`, and `NAOMI LIBRARY Ver 0.8 AM R&D`, plus embedded Naomi source-path strings (`nlrom.c`, `obj_load.c`, `rob_load.c`, `sound.c`, `stage.c`). Their presence is a useful lead, but the first static pass did not establish direct PC-relative references to those strings or prove that the Naomi-specific modules are the active game path.

One caution: the related decomp notes label `0x8C0360C8` as `_gdFsDaPlaySct`; in the first disassembly window of this retail image, that address decodes as a `nop` within the surrounding code/data region. Do not use it as a proven hook point until the function boundary and call references are reconciled.

Inspection workflow and report:

- Workflow: `.github/workflows/inspect-dreamcast-vf3tb.yml`
- Analyzer: `tools/inspect_vf3tb_executable.py`
- Successful run: [GitHub Actions run 45846220320](https://github.com/cdmn1/vf3-naomi/actions/runs/45846220320)
- Report artifact: `vf3tb-dreamcast-executable-inspection-v1`

### Immediate next analysis

Resolve code references to the P0 aliases of the `r7=0`, `r7=1`, and `r7=2` stubs; inspect the caller register setup and parameter blocks; identify the real DMAREAD FAD/count/destination values and the status-poll sequence. Only then implement the Naomi-side replacement at the narrowest verified boundary.
## Retail read-mode selection and DMA callsites

A second pass through the same retail executable resolved the read-mode selector and proved that both transfer modes are present in the game code:

- At `0x8C0431DA`, the routine compares its incoming `r7` value with `1`.
- When `r7 == 1`, it sets the request command to `0x11` (`DMAREAD`) and proceeds to the shared `REQ_CMD` stub.
- When `r7 == 0`, it sets the request command to `0x10` (`PIOREAD`) and proceeds to the same `REQ_CMD` stub.
- Other `r7` values take the error path (`-18`).

This is direct evidence in the retail executable that the DMA and PIO paths are selectable. It closely matches the video notes: the game has a PIO mode and a DMA mode, and the mode selector is the first narrow place to preserve or force the DMA path. The caller that supplies this `r7` selector still needs to be traced before patching it.

Retail code references found by scanning SH-4 PC-relative literal loads (P0 aliases are used for indirect calls):

| Code address | Service stub | What is established |
|---|---|---|
| `0x8C043226` | `REQ_CMD` (`r7=0`) | Queues command `0x11` or `0x10`, with `r5` pointing at the parameter block |
| `0x8C04329A` | `REQ_DMA_TRANS` (`r7=6`) | Called by the transfer helper with a two-word parameter block |
| `0x8C0432E4` | `GET_CMD_STAT` (`r7=1`) | Status-poll helper is present and referenced |
| `0x8C04340E` | `CHECK_DMA_TRANS` (`r7=7`) | DMA transfer-check helper is present and referenced |
| `0x8C0434CA` | `EXEC_SERVER` (`r7=2`) | Server-execution service is called |
| `0x8C0433BC` | `READ_ABORT` (`r7=8`) | Abort service is called |
| `0x8C0433F2` | `GET_DRV_STAT` (`r7=4`) | Drive-status service is called |
| `0x8C0434D6` | `RESET` (`r7=9`) | Reset service is called |
| `0x8C034A1A` | `G1_DMA_END` (`r7=5`) | DMA completion service is referenced |

The helper at `0x8C0431DA` computes `r12 = r5 * 0x800` before building the request parameters. The DMA branch writes the command value `0x11`; the PIO branch writes `0x10` and forms a destination in the `0xA0000000` region. This suggests the DMA branch is the intended bulk-transfer route and the PIO branch is a fallback, but the full parameter semantics and caller-provided selector must be verified before implementation.

### Current implementation decision

Do not replace the game’s loader or invent a separate read protocol. Preserve the retail syscall sequence and implement the narrow BIOS GD-ROM service contract on NAOMI. First resolve the caller of `0x8C0431DA` and confirm the DMA destination/length fields; then route the existing `DMAREAD` / `REQ_DMA_TRANS` / `CHECK_DMA_TRANS` flow to the NAOMI DMA backend. Keep the PIO branch available as a diagnostic fallback, not as the main transfer path.