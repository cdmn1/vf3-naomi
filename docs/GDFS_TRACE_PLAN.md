# GDFS / MT loader trace plan

Date: 2026-11-16

## Objective

Resolve the live path from a selected motion resource to the bytes mounted in guest RAM:

1. identify the runtime-populated pointer that reaches the 132-entry name-pointer table at `0x8C1069BC..0x8C106BCC`;
2. capture the caller's resource index and the selected pointer/name;
3. observe the GDFS sector request at `_gdFsDaPlaySct` (`0x8C0360C8`);
4. identify the MT pack loader that reads the selected MT file's internal 8,880-slot offset table and scatters/re-sites individual motion records into resident RAM.

This trace is specifically for the character-select-to-round-one transition. A post-initialization fight-only state is not enough: the related research reports `mt_loader` (`0x8C02DCEC`) had zero hits in the post-init fight trace.

## Watch points

The related project's M66 recipe identifies these guest PCs:

| PC | Name / role | What to capture |
|---|---|---|
| `0x8C02DCEC` | `mt_loader` | candidate top-level motion-pack loader |
| `0x8C04BDA2` | `load_mt_to_u_pai` | per-pair loader; known to reference `MTTOUPAI.BIN` |
| `0x8C08232E` | `load_mt_aki_jef` | another per-pair loader |
| `0x8C0360C8` | `_gdFsDaPlaySct` | fingerprint-verified GDFS sector-read endpoint |
| `0x8C0CBC40` | `bjload_run` | fighter-object loader/runner; likely part of the character load path |

The maintained Flycast tracer in `tools/flycast-trace/` reads `pc 0xADDR` lines. Each hit records `r13`, `r14`, `r4`, `r5`, `r6`, and `r7` in a probe group. The interpreter normalizes the watched PC for the game's P2 alias.

## Memory watches

Use the same `VF3_WATCH` file for memory ranges:

```text
mem 0x8C1069BC 0x210
mem 0x8C5F0000 0x270000
```

- `0x8C1069BC`, length `0x210`: the 132-pointer runtime table (132 × 4 bytes).
- `0x8C5F0000`, length `0x270000` (end-exclusive `0x8C860000`): broad resident MT destination window. Earlier research found `MTJACLAU.BIN` resident from approximately `0x8C5F4100`; the broad range leaves room for the header and scattered records.

Memory records include the executing PC, access width, read/write flag, address and value. The watch is interpreter-only; dynarec memory accesses bypass these hooks.

## Input-path evidence and harness distinction

See [INPUT_PATH_FINDINGS.md](INPUT_PATH_FINDINGS.md) for the detailed cross-check. The related project's native `vf3sdl` harness once published a zero-valued Maple pad when no script was active; because the wire is active-low, that looked like every button pressed. Its later per-frame neutral publication fixed that host bug, and a Start hold passed its press-start screen. This is **not** a result from patched Flycast running the original executable.

The separate Flycast cold-boot captures logged the correct Start/A Maple words (`0x8` / `0x4` pressed masks), yet their scanned checkpoints did not execute selected-port reader `0x0C09B006`, controller merge `0x0C09AF6C`, or action update `0x0C0AD1B8`. Therefore, button delivery is verified at Maple, but the game-level route to a player-controlled round is not. Do not treat either harness's navigation sequence as a proven character-select-to-round-one script.

## Capture sequence

The first maintained tracer is a minimum PC/memory-watch build. It does not yet implement the private research fork's `VF3_FULL` snapshots, `VF3_TRACE_FRAMES` frame limit, `VF3_STATE` savestate staging, or `VF3_PLAY` / frame-clock `vf3script` commands. Do not set those variables or assume they work.

After the Windows package has been built and smoke-tested:

1. Run `run-trace-loader.cmd` from the extracted package. It selects `VF3_WATCH_LOADER.txt`, forces the interpreter, and writes a separate `vf3-loader-trace-loader.bin` plus diagnostics so it cannot overwrite the compact controller-path capture.
2. In the Flycast GUI, load the **Dreamcast** VF3 game image used by the decomp research (not the Naomi ROM image).
3. Manually navigate to the actual character-select confirmation and continue into Round 1. A savestate at that boundary is not required for this first pass.
4. Close Flycast normally. The output `vf3-loader-trace-loader.bin` and its `.diag.txt` companion will be beside `flycast.exe` in the extracted package.
5. Retain the trace and emulator log. Record the exact game image, emulator revision and whether the capture reached Round 1.

Important: if the run reaches only an autonomous attract/demo fight, the one-time loaders may already have finished. That capture will not answer this plan's question. The existing `vf3_play_frontend_ladder.txt` is not a verified route to the boundary; continue solving the input/navigation path rather than assuming it works.

## Decoding a retained capture

The combined `VF3_TRACE` stream contains the PC-probe marker groups (`0xFA10`/`0xFA11`) and memory-access records (`0xFA20`) described by the related project's trace notes. After a capture, decode those records with:

```text
python tools/loader_trace_decode.py path/to/vf3-loader-trace-loader.bin --prefix extract/analysis/loader_trace
```

This writes `loader_trace.pc.csv` (PC plus `r13`, `r14`, `r4`–`r7`) and `loader_trace.memory.csv` (PC, access type, width, address and value). Eight-byte memory values are retained at their full 64-bit width. Unknown trace words are skipped; malformed or truncated recognized groups are reported as warnings. The decoder is for these documented probe/memory records, not a general trace disassembler.

For a first-pass report, run:

```text
python tools/loader_trace_summary.py extract/analysis/loader_trace.pc.csv extract/analysis/loader_trace.memory.csv
```

The report includes the first register snapshot for each watched function, pointer-table read/write values, and the first resident-MT writes. These samples help narrow the pointer chain and scatter routine; they are not by themselves proof that a resource load completed.

## v32 capture findings (2026-11-18)

The first maintained loader capture is now decoded from
[VF3-Flycast-Trace-v32](https://github.com/cdmn1/vf3-naomi/tree/main/VF3-Flycast-Trace-v32).
The binary is 2,398,672 bytes (299,834 little-endian u64 words), SHA-256
'b2fd9f1352e28f7e1c4e73ef6e5cfb4c19868ebe91bfadc5580400909fc07b07'.
The decode produced 2 PC-probe groups and 74,949 memory records.

### What the capture did show

- The only watched PC probes hit were the two obj_load.c source-path
  literal-load instructions: '0x8C0A2FD2' and '0x8C0A2FF6', once each.
- The five primary loader targets all had zero hits:
  mt_loader ('0x8C02DCEC'), load_mt_to_u_pai ('0x8C04BDA2'),
  load_mt_aki_jef ('0x8C08232E'), _gdFsDaPlaySct ('0x8C0360C8'),
  and bjload_run ('0x8C0CBC40').
- The runtime pointer table range had 12 reads and no writes. The reads touched
  entries 0, 20, 25, 92, 93, 118, 119, 122 and 123. The read instruction PC
  was '0x8C0A7F84'; this is now an additional register-probe target so the
  next capture can record the table base, scaled index and loaded pointer.

The pointer values were checked against the user's original Dreamcast
1ST_READ.BIN (load base 0x8C010000). They resolve to filenames embedded in
the executable:

| Runtime table index | String pointer | Resolved filename | Manifest row | GDFS LBA | Candidate FAD (LBA+150) | Size |
|---:|---:|---|---:|---:|---:|---:|
| 0 | 0x8C0D6498 | MOTHEAD.BIN | 65 | 442686 | 442836 | 168,391 B |
| 20 | 0x8C0D65D4 | MTAKIPAI.BIN | 85 | 459459 | 459609 | 1,572,408 B |
| 25 | 0x8C0D6624 | MTAKIWOL.BIN | 90 | 463527 | 463677 | 1,696,108 B |
| 92 | 0x8C0D6A54 | ROB_AKI.POL | — | — | — |
| 93 | 0x8C0D6A60 | ROB_AKI.TEX | — | — | — |
| 118 | 0x8C0D6B8C | ROB_PA2.POL | — | — | — |
| 119 | 0x8C0D6B98 | ROB_PA2.TEX | — | — | — |
| 122 | 0x8C0D6BBC | ROB_WO2.POL | — | — | — |
| 123 | 0x8C0D6BC8 | ROB_WO2.TEX | — | — | — |

The GDFS LBA/size columns are cross-references to
[docs/GDFS_ASSET_INDEX.csv](GDFS_ASSET_INDEX.csv). Its manifest row numbers
are a separate index space from the 132-entry runtime pointer table; for
example, runtime table index 20 resolves to MTAKIPAI.BIN, whose manifest
table_row_0based is 85. The ROB_* names were resolved from the executable,
but are not present in that GDFS asset index, so no LBA/size is assigned here.
The current GDFS asset index contains 232 entries and all are '.BIN'
files. The runtime 132-pointer table is broader: it also exposes '.POL' and
'.TEX' names. Do not assume those ROB_* entries use the same GDFS lookup/read
path as the MTAKI*.BIN motion packs; the next call-target probe is intended to
separate those loader paths.

The reads occurred in two six-entry bursts at the same instruction PC:

| Trace words | Runtime table indices, in read order | Filenames |
|---:|---|---|
| 0–20 | 92, 93, 122, 123, 0, 25 | ROB_AKI.POL, ROB_AKI.TEX, ROB_WO2.POL, ROB_WO2.TEX, MOTHEAD.BIN, MTAKIWOL.BIN |
| 126874–126894 | 92, 93, 118, 119, 0, 20 | ROB_AKI.POL, ROB_AKI.TEX, ROB_PA2.POL, ROB_PA2.TEX, MOTHEAD.BIN, MTAKIPAI.BIN |

The second burst follows the observed object-loader source-path hits and the
zeroing writes to nine destination-buffer starts. This is consistent with
two different character-pair asset selections (Akira/Wolf and Akira/Pai),
but the trace alone does not prove that either filename batch completed a
disc read or identify which side of the match was selected. Treat the pairing
interpretation as a lead, not a confirmed gameplay-state identification.

This establishes that the observed table reads are filename-pointer lookups.
It does not yet establish which lookup causes a file transfer, how the
resource index is selected, or which loader consumes the returned filename.
- The resident MT window had 74,937 accesses: 74,910 reads and 27 writes.
  All 27 writes stored zero at nine buffer-start addresses, from
  '0x8C0A2EC8' and '0x8C0A304E'. No nonzero resident-MT writes were captured,
  so this is not evidence of the per-record scatter/re-site loader.
  The nine distinct zeroed starts were '0x8C62D000', '0x8C67C800',
  '0x8C6C1000', '0x8C6C3000', '0x8C70B800', '0x8C757000', '0x8C7A7800',
  '0x8C7EF000' and '0x8C844000'. The first PC wrote through a P2 alias and
  the second through a P0 alias; after SH-4 alias normalization, they are the
  same nine destinations. Their relationship to individual asset files is
  not established yet.
- Most reads came from '0x8C09D6C8', '0x8C09D6F4', '0x8C09D70A',
  '0x8C09D778', '0x8C09D77E', '0x8C09D782' and '0x8C09D794'.
  The related MT evaluator analysis identifies these as motion-frame
  evaluation reads: count bytes, key-time bytes and tuple words. This confirms
  that the capture reached code consuming resident motion data; it does not
  identify how that data was originally loaded.

### Interpretation and next capture

The capture is useful: it proves the memory hooks work, shows the runtime table
being read, and shows active motion data being consumed. It does **not** show
the one-shot MT pack loader or the GDFS sector-read endpoint. Therefore the
pointer chain and scatter/rewrite algorithm remain unresolved. This does not
by itself prove whether the capture missed the character-select transition or
whether the game reached a path that reused already-resident motion data.

The v32 decoder output exposed an address-alias issue in the first-pass
summarizer: interpreter records use normalized 0x0C... PCs/addresses and
some memory accesses use the 0xAC... alias, while the report compared only
against 0x8C... . 'tools/loader_trace_summary.py' now normalizes P0/P1/P2
aliases before counting probes and classifying the two watched memory ranges.

The loader watch profile now adds register probes at '0x8C0A7F84'
(table-entry load), '0x8C0A2F90' (candidate obj_load.c prologue),
'0x8C0A2FDE' (indirect helper target in that code region), and '0x8C0A2EC8'
and '0x8C0A304E' (observed zeroing writes). The '0x8C0A2FDE' snapshot is
intended to expose the target held in r1 and the associated argument registers. The next capture
should still span the actual character-select confirmation into Round 1.
These added probes are deliberately limited; the high-frequency MT evaluator
PCs are left as memory-event-only watches to avoid a large PC-probe trace.

## Naomi source-path reference probes

The checked-in `VF3_WATCH_LOADER.txt` now also watches the exact SH-4 PCs that load pointers to the embedded Naomi source-path strings for `nlrom.c`, `obj_load.c`, `rob_load.c`, `sound.c` and `stage.c`. These are literal-reference instructions, not confirmed function entries.

Use their hits as a separate line of evidence:
- no hit during a capture means only that these specific instructions were not executed in that captured interval;
- a hit proves the instruction executed and records `r13`, `r14`, and `r4`–`r7` at that point;
- a hit does not prove the named source module is the current caller, that its loader path completed, or that it is part of normal gameplay.

This should help determine whether the Naomi source-path references are reached during boot, character selection or Round 1, while the memory watches continue to identify the actual resource reads and destination writes. The new probes do not replace the original loader/GDFS/memory watches.

## Analysis order

1. **Runtime table pointer:** at the first hit in `mt_loader` / `load_mt_*`, inspect `r13` and `r14` and trace the pointer chain to `0x8C1069BC`. Confirm whether the selected table index is zero-based or one-based and whether it selects a string pointer, a file descriptor or a loader object.
2. **GDFS call contract:** correlate the `load_mt_*` hit with `_gdFsDaPlaySct`; capture `r4..r7`, any pointed-to parameter block and the returned status. Do not assume the BIOS `REQ_CMD`/`GET_CMD_STAT` ABI is the same as the GDFS library function ABI.
3. **Disc resource mapping:** use `GDFS_ASSET_INDEX.csv` to map the selected filename to its manifest LBA, byte size and SHA-1. Compare the actual sector range requested with the manifest entry; account for Mode1/2352 track sectors and the 150-sector FAD/LBA offset where applicable.
4. **MT scatter:** correlate reads from the selected MT file's 8,880-slot table with writes to `0x8C5F0000..0x8C860000`. The goal is to find the function that reads the three-offset row for a motion ID and constructs the runtime `ops[63]`, count, time and tuple streams.
5. **Validate:** compare at least two different pair packs and two motion IDs. A single `MTJACLAU` capture is not enough to generalize the loader.

## Expected deliverables

- pointer-chain note for the 132-entry runtime table;
- captured argument/return contract for `_gdFsDaPlaySct`;
- named pack-loader function and its source/destination address ranges;
- per-record MT scatter algorithm, including any pointer fixups or rewritten headers;
- test vectors that compare selected runtime records against the extracted MT asset rows.

## Limits

This plan is based on the related project's published M33/M66 notes and the previously recorded analysis of the user's executable. It is not a trace result. We have not run this capture in the current working session, and the runtime table's pointer chase and MT scatter routine remain unresolved.

## Sources

- [Probe-mode trace format](https://github.com/celsowm/vf3-decomp/blob/main/docs/re/probe_mode.md)
- [Front-end / character-select capture attempts and their limits](https://github.com/celsowm/vf3-decomp/blob/main/docs/re/front_to_fight.md)
- [Checked-in combined PC + memory watch list](../tools/VF3_WATCH_LOADER.txt)
- [Trace marker decoder](../tools/loader_trace_decode.py)
- [GDFS / loader pointer investigation](https://github.com/celsowm/vf3-decomp/blob/main/docs/re/loader_m66.md)
- [MT resident scatter evidence](https://github.com/celsowm/vf3-decomp/blob/main/docs/re/mt_mount_m33.md)
- [MT asset layout](https://github.com/celsowm/vf3-decomp/blob/main/docs/formats/MT.md)
- [MT memory-access trace](https://github.com/celsowm/vf3-decomp/blob/main/docs/re/mt_fields.md)
- [Flycast tracer implementation and build status](FLYCAST_TRACE_TOOLING.md)
