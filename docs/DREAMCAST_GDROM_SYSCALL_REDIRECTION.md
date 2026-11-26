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

A second pass through the same retail executable found a function-shaped read-mode routine beginning at `0x8C0431D8`. Its instruction sequence contains both transfer-mode branches:

- At `0x8C0431D8`, the routine compares its incoming `r7` value with `1`.
- When `r7 == 1`, it sets the request command to `0x11` (`DMAREAD`) and proceeds to the shared `REQ_CMD` stub.
- When `r7 == 0`, it sets the request command to `0x10` (`PIOREAD`) and proceeds to the same `REQ_CMD` stub.
- Other `r7` values take the error path (`-18`).

This is direct evidence that the executable contains a DMA/PIO selector: `r7 == 1` builds a DMAREAD request and `r7 == 0` builds a PIOREAD request. It closely matches the video notes. However, the current static scan found no direct PC-relative or raw pointer reference to `0x8C0431D8` inside `1ST_READ.BIN`. The routine may be reached through a runtime-loaded module/function table, or it may be unused code. Do not assume it is the active retail caller until that linkage is resolved.

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

The routine at `0x8C0431D8` computes `r12 = r5 * 0x800` before building the request parameters. The DMA branch writes the command value `0x11`; the PIO branch writes `0x10` and forms a destination in the `0xA0000000` region. In the DMA branch it calls `0x0C050E8A` or `0x0C050C60` with the destination (`r14`) and byte count (`r12`). Those helpers contain 32-byte cache-line loops using SH-4 `ocbp` and `ocbi` instructions respectively, confirming that the DMA destination is explicitly cache-maintained.

### Current implementation decision

Do not replace the game’s loader or invent a separate read protocol. Preserve the retail syscall sequence and implement the narrow BIOS GD-ROM service contract on NAOMI. First resolve the caller of `0x8C0431DA` and confirm the DMA destination/length fields; then route the existing `DMAREAD` / `REQ_DMA_TRANS` / `CHECK_DMA_TRANS` flow to the NAOMI DMA backend. Keep the PIO branch available as a diagnostic fallback, not as the main transfer path.
## 512 MiB NetDIMM packaging constraint

The existing Dropbox folder inventory was re-summed directly from its file metadata (413 entries including one unrelated ZIP archive):

- All 412 retail disc files total `686,357,488` bytes (`654.6 MiB`).
- `MOVIE_A.SFD` is `356,409,344` bytes (`339.9 MiB`).
- `MOVIE_B.SFD` is `69,398,528` bytes (`66.2 MiB`).
- The two SFD movie files total `425,807,872` bytes (`406.1 MiB`).
- The remaining retail files, excluding both movies, total `260,549,616` bytes (`248.5 MiB`).
- Excluding the unrelated `n1-la_keyboard_xyu-[GDS-0017].zip` archive as well leaves `251,578,523` bytes (`239.9 MiB`) of game data.

Therefore the full retail disc-file set does not fit in a 512 MiB NetDIMM image, but the executable and non-movie gameplay assets do, with substantial room for the loader, index and alignment. The first playable package should retain all non-SFD retail assets and omit both movie streams. The movie playback/read path must be handled gracefully (skip/no-op or a later optional video package); do not silently pretend the missing movie data is present. Whether either movie is required for attract mode or endings remains to be checked.

One unrelated ZIP archive is present in the shared Dropbox folder; it is not part of the 412-file retail asset inventory and should not be packed.
## Second-stage RELOAD.BIN confirmation

The same workflow extracted `RELOAD.BIN` from the shared folder and verified its size (`655,360` bytes) and SHA-256 (`b8eb49a134230bcd195b5abb03adfd03edfe7ba936b261362bacf2c9e48cd2d1`). Its disassembly contains the GDFS 0.53 and NAOMI LIBRARY 0.8 banners, plus a second complete family of service-vector trampolines:

- `0x8C012BC0 + 0x14*n`, for `n = 0..13`: each sets `r6=0`, sets `r7=n`, loads `0x8C0000BC`, dereferences it and jumps through the service pointer.
- `0x8C012CD8`: alternate selector `r6=-1, r7=0`.
- `0x8C012CEC`: alternate selector `r6=-1, r7=1`.

This independently confirms that both `1ST_READ.BIN` and `RELOAD.BIN` expect the same BIOS service-vector address and the same `r6/r7` dispatch convention. The narrowest replacement remains a handler installed at `0x8C0000BC`; it must handle the `r6=0` GD-ROM family and preserve or explicitly support the `r6=-1` calls if the game reaches them. The service pointer must remain valid when the second-stage module is loaded.
## First BIOS-vector bridge build

`tests/naomi_gdrom_syscall_probe/` now contains a reusable SH-4 service-vector handler and a Naomi on-screen transport probe. GitHub Actions successfully built `vf3_naomi_gdrom_syscall_probe-v6.bin` (225,724 bytes; SHA-256 `9f15f161e26e4a874472569dfc54e8cd8f726c3cf51dab3c494ef6ec34488c3a`).

The probe installs its handler at `0x8C0000BC`, implements `r6=0` operations `r7=0..13`, and routes both command `0x11` (`DMAREAD`) and command `0x10` (`PIOREAD`) through the Naomi Holly GD-DMA registers. It checks request IDs, transfer polling, command completion, and compares both syscall-mediated transfers against a direct 2 KiB cartridge PIO read. It requires no serial connection.

Important limits of this first build:

- It transfers one 2 KiB sector only.
- Its temporary mapping is `FAD 45150 -> NetDIMM image offset 0`; this is a transport test, not the retail disc FAD/file map.
- The `r6=-1` alternate service calls are harmless no-ops in this probe; the production loader must preserve or implement them if the retail game uses them.
- It does not yet load `1ST_READ.BIN` or `RELOAD.BIN`, and it is not a playable game build.

The next validation is a Flycast run of this ROM. If both read paths and the byte comparisons pass, extend the same handler to the retail FAD-to-packed-asset index and multi-sector transfers, then use it in the Dreamcast executable bootstrap.
Clean rebuild `vf3_naomi_gdrom_syscall_probe-v7.bin` also succeeded after removing temporary linker diagnostics from the Makefile. It is 225,724 bytes with SHA-256 `1e7613c4545bce8cf7e103df3545558c375dd08c1405aaee3aa424e03ef913d3`.

## Retail DMA wrapper and bridge contract audit (2026-11-26)

A closer read of the retail disassembly identifies the function-shaped transfer
wrapper beginning at `0x8C0431DA` (the instruction at `0x8C0431D8` belongs
to the preceding code boundary). Its observed register contract is:

- `r4`: starting FAD;
- `r5`: sector count;
- `r6`: destination address;
- `r7=1`: DMA path, command `0x11` (`DMAREAD`);
- `r7=0`: PIO path, command `0x10` (`PIOREAD`).

The routine computes the transfer byte count as `r5 * 2048`, creates the
four-word request block `{FAD, sector_count, destination, 0}`, and submits
it through the `REQ_CMD` trampoline at `0x8C043226`. The DMA branch calls
the cache-preparation helper at `0x8C050E8A`; the alternate aligned-buffer
path can call `0x8C050C60`. This establishes the wrapper's request layout
directly from the retail executable.

The subsequent helper callsites are also identified:

| PC | Operation | Evidence at callsite |
|---|---|---|
| `0x8C04329A` | `REQ_DMA_TRANS` | request ID in `r4`, two-word transfer parameter block in `r5` |
| `0x8C0432E4` | `GET_CMD_STAT` | request ID in `r4`, four-word result block on stack in `r5` |
| `0x8C04340E` | `CHECK_DMA_TRANS` | transfer-check wrapper |
| `0x8C0434CA` | `EXEC_SERVER` | server-execution wrapper |

The static scan still finds no direct literal pointer to the wrapper entry
`0x8C0431DA`. That is consistent with the related project's finding that
this executable uses extensive object/table-indirect dispatch, but it does
not prove this particular routine is reached during the game. The v73 Flycast
loader trace therefore adds register snapshots at the wrapper, request,
transfer, status and server callsites, plus the GDFS dispatch boundary
`0x8C0360C8` and the service-vector word `0x8C0000BC`.

### Bridge state-machine correction

A source audit found two issues in the first Naomi bridge implementation:

1. `GET_CMD_STAT` took its result-byte-count snapshot before polling the DMA.
   If that poll observed the final DMA completion, the status could be
   COMPLETE while `ext[2]` still contained the previous byte count.
2. `REQ_CMD` could replace a terminal-but-unconsumed request. The REIOS model
   accepts a new request only while the device is idle.

Both are corrected. The bridge now polls before filling the four-word result
block, returns terminal status only for the matching request ID, and accepts
a new request only from the idle state. A host-side regression test exercises
the final-byte-count case, request-ID ownership and request gating; it passed
as step 3 of GitHub Actions run
[45879107100](https://github.com/cdmn1/vf3-naomi/actions/runs/45879107100).
This is a state-machine unit test, not a NetDIMM hardware or retail-game test.

### Resident-shim memory placement lead

The pinned KallistiOS architecture header defines `HW_MEM_16` as 16 MiB and
`HW_MEM_32` as 32 MiB for NAOMI / 32-MiB systems. Its 32-MiB memory top is
`0x8E000000`; the 16-MiB boundary is `0x8D000000`. This makes the second
16 MiB (`0x8D000000–0x8E000000`) a candidate location for a resident GD-ROM
shim that survives loading the Dreamcast executable at `0x8C010000`.

It is not yet safe to assume that range is unused: the Naomi-specific game
code may allocate from the extra RAM. The bootloader must reserve the shim
region or constrain the game's allocator before relying on it. KOS source
reference: [KallistiOS arch.h, 32-MiB memory definitions](https://github.com/KallistiOS/KallistiOS/blob/9926b70aeafc8398675caef98813f10d15d8a329/kernel/arch/dreamcast/include/arch/arch.h).

### Existing v66 trace: prioritize the BIOS wrapper over the GDFS endpoint

The checked-in v66 Flycast diagnostic provides useful runtime context without
requiring another capture. It executed 1,055,753,246 SH-4 instructions and
recorded 2,139 GD-ROM sector-read calls, including 2,136 REIOS HLE transfers
covering 10,491 sectors. Its normalized executed-PC histogram records
36,263,675 instructions in page `0x0C043000`, which contains the retail
`0x8C0431DA` read wrapper and the nearby BIOS-service helpers. In that same
capture, the `0x8C0360C8` GDFS endpoint watchpoint had zero hits.

This does not prove that `0x8C0431DA` itself was executed—the trace did not
watch that exact PC—but it makes the BIOS-wrapper callsites the higher-priority
runtime target. The new v73 profile watches those exact PCs. The GDFS endpoint
watch remains as a cross-check rather than the assumed active path.
