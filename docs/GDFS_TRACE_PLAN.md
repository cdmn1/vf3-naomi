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
The user confirmed both gameplay contexts against the actual run:
the first batch is the attract-mode characters shown after the SEGA logo
(Akira and Wolf); the second is the first match after selecting Akira,
against Pai. This independently validates the asset-selection interpretation
and ties the later batch to the character-select → Round 1 transition.
It still does not prove that the filename batches completed their disc reads
or reveal the read destination/scatter routine.

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

## v38 object-loader follow-up (2026-11-19)

The v38 capture is now decoded and checked in under
[VF3-Flycast-Trace-v38](../VF3-Flycast-Trace-v38/). Its analysis workflow
completed successfully:
[run #44566831828](https://github.com/cdmn1/vf3-naomi/actions/runs/44566831828).

- Trace size: 10,107,088 bytes; 1,087,188,287 SH-4 instructions executed.
- 198 decoded PC-probe groups and 314,906 watched memory-access records.
- All five primary loader probes still had zero hits:
  `mt_loader` (`0x8C02DCEC`), `load_mt_to_u_pai` (`0x8C04BDA2`),
  `load_mt_aki_jef` (`0x8C08232E`), `_gdFsDaPlaySct` (`0x8C0360C8`)
  and `bjload_run` (`0x8C0CBC40`).
- The runtime-table reader at `0x8C0A7F84` hit 60 times. The
  `obj_load.c` literal-reference PCs `0x8C0A2FD2` and `0x8C0A2FF6`
  each hit once; the candidate object-loader prologue `0x8C0A2F90` hit
  twice.
- The indirect helper `0x8C0A81EE` ran three times. Its observed calls
  received `R5=0x0C364000` and `R5=0x0CA2D800`. The first address is
  within the watched runtime object table; the second watched 4 KiB window
  (`0x0CA2D800..0x0CA2E800`) recorded no accesses. Neither R5 value is
  therefore established as a file-read destination.
- The helper's decompiled implementation performs metadata/index processing
  and calls the matrix-family code. The trace does not show a GD-ROM/GDFS
  request or a completed asset transfer. Buffer initialization and pointer
  relocation must not be mistaken for file-data movement.

### Rejected lead: 0x0C0A2B90 / 0x0C0A2B94

The indirect calls at `0x0C0A2B48` and `0x0C0A2B5E` read callback
pointers from `0x0C0A2B90` and `0x0C0A2B94`, respectively. These
instructions belong to the separate generated fight adapter
`src/fight/vf3_away_batch20_adapter_41.c` in the related decompilation;
they are not the call at `0x0C0A2FDE` in the observed `obj_load.c)
path. There is no v38 trace hit at either `0x0C0A2B48` or
`0x0C0A2B5E`. Do not treat these callback slots as the next asset-I/O
target.

### Next capture

No new PC or memory watchpoints are required yet: the existing
`tools/VF3_WATCH_LOADER.txt` already contains the five primary loader
probes, the runtime pointer table and the resident MT window. The next
capture must begin before the one-shot load and span the actual
character-select confirmation into Round 1. A trace taken only after the
game has entered an already-running fight can miss `mt_loader`,
`coli_run` and the GDFS transfer entirely. If the five primary probes
remain at zero in a verified transition capture, revisit the candidate
entrypoints and their aliases before expanding instrumentation.

## v66 REIOS HLE transfer-to-asset correlation (2026-11-20)

The v66 diagnostic's FAD-page histogram can be mapped against
[the checked-in GDFS asset index](GDFS_ASSET_INDEX.csv). The index uses GDFS
LBA values; for this Dreamcast data track the matching FAD is LBA + 150.
Asset extents are calculated in 2048-byte sectors, rounding file size up.

The trace hook increments its FAD histogram using the **starting FAD** of
each FA22 transfer, divided by 256. This is confirmed by
`tools/flycast-trace/vf3trace.cpp` at `gdromHleFadPages[startFad / 256]`.
The first 96 diagnostic samples therefore were not representative of all
reads: the raw FA22 stream contains all 2,136 transfer records.

### Per-transfer result: motion packs are copied into the resident MT window

I added `tools/loader_trace_asset_map.py` to decode every FA22 record and
correlate its complete sector interval with the GDFS asset extents. The
analysis ran in GitHub Actions
[successful run #44790842795](https://github.com/cdmn1/vf3-naomi/actions/runs/44790842795).
It decoded 2,136 records, 10,491 sectors and 17,733,028 non-zero bytes,
matching the v66 diagnostic totals.

| Asset | Calls | Sectors | Non-zero bytes | Guest destination span (end exclusive) |
|---|---:|---:|---:|---|
| `MTAKIWOL.BIN` | 166 | 829 | 1,185,084 | `0x0C5F4000–0x0C791000` |
| `MTAKIPAI.BIN` | 154 | 768 | 1,094,417 | `0x0C5F4000–0x0C774000` |

For **every** transfer in both packs:
- the requested FAD/LBA interval is fully contained in that file's indexed
  extent (`extent_match=exact`);
- the HLE hook flags are `2` (`virtual=0, direct=1`);
- the returned sector count equals the requested sector count;
- the destination buffer contains non-zero bytes;
- the FAD and guest destination addresses advance monotonically in matching
  5-sector batches (the final batch is 4 sectors for Wolf and 3 for Pai).

Each pack starts at the same guest address, `0x0C5F4000`, and uses 154 or
166 consecutive destination buffers. Both complete spans lie inside the
previously watched resident-MT window `0x0C5F0000–0x0C860000`. This is now
direct evidence that the game reads the named matchup motion packs from the
disc and copies their non-zero contents into the resident-MT memory region.
The two packs reuse the same destination base rather than occupying separate
permanent ranges.

### Post-load CPU reads confirm the packs are consumed by the motion evaluator

I extended the offline decoder to parse FA20 interpreter memory-access records
as well as FA22 transfer records. For each pack, it counts accesses to the
loaded destination span only after that pack's final transfer, and stops the
Wolf interval immediately before the same buffer is reused for Pai.

| Pack lifetime | CPU reads | CPU writes | Distinct read/write PCs |
|---|---:|---:|---:|
| `MTAKIWOL.BIN` | 32,073 | 18 | 20 |
| `MTAKIPAI.BIN` | 26,717 | 0 | 19 |

The dominant read PCs are the same motion-evaluation sites identified in
the earlier MT analysis. The FA20 hook records Sh4cntx.pc - 2, so this table
uses the tracer's logged PC values:

| PC | Wolf reads | Pai reads |
|---|---:|---:|
| `0x8C09D6C8` | 7,474 | 7,525 |
| `0x8C09D70A` | 5,892 | 4,309 |
| `0x8C09D6F4` | 3,738 | 2,915 |
| `0x8C09D778` | 3,643 | 2,834 |
| `0x8C09D77E` | 3,643 | 2,834 |
| `0x8C09D782` | 3,643 | 2,834 |
| `0x8C09D794` | 3,643 | 2,833 |

These are CPU accesses whose normalized guest addresses fall inside the
corresponding pack's loaded destination span, and whose trace positions are
after the pack's load. This directly connects the named disc asset to the
motion-evaluation code reading its resident bytes. It closes the earlier gap
between seeing a file copied into RAM and seeing the game use that RAM as
motion data.

The 18 Wolf-lifetime writes are zero-clears from `0x8C0A304E` (12) and
`0x8C0A2EC8` (6) at six buffer-start addresses. They are ordinary CPU
initialization writes, separate from the direct REIOS HLE sector copies.
No corresponding payload-range writes were observed during the Pai lifetime.

The artifact also contains `motion_memory.csv`, with each watched access
tagged to the pack lifetime, guest PC, width, access type and guest address.
The successful workflow run completed the Python tests, v66 asset/memory
correlation and the existing input-capture report jobs.

### v66 slot-table reads identify the selected MT stream triplets

The setup path also reads the pair pack's 8,880-entry slot table directly.
The v66 FA20 records at PCs `0x8C09D5D2`, `0x8C09D5CE` and
`0x8C09D5DA` are 32-bit reads from the first `0x8AC0` bytes of the
currently loaded MT pack. For each read:

`slot_id = (guest_read_address - 0x0C5F4000) / 4`

The returned dword is the file-relative record offset. The three setup PCs
read three consecutive slot IDs per selected group. This is independently
cross-checked against the corresponding slot-table entries in the extracted
pair-pack files. (The text extractor replaces offsets above Unicode's maximum;
the FA20 values preserve those high offsets.)

**MTAKIPAI.BIN — 10 observed triplets**

| Slot IDs | File offsets |
|---|---|
| 111–113 | `0x1A028, 0x1A040, 0x1A048` |
| 324–326 | `0x23D68, 0x23D80, 0x23D88` |
| 402–404 | `0x26C1C, 0x26C34, 0x26C3C` |
| 414–416 | `0x2725C, 0x27274, 0x2727C` |
| 1293–1295 | `0x542CC, 0x542FC, 0x54358` |
| 1581–1583 | `0x5B154, 0x5B16C, 0x5B174` |
| 1596–1598 | `0x5B924, 0x5B93C, 0x5B944` |
| 1623–1625 | `0x5D59C, 0x5D5CC, 0x5D5E0` |
| 3147–3149 | `0x98DD8, 0x98DF4, 0x98E04` |
| 7155–7157 | `0x14BC30, 0x14BC4C, 0x14BC5C` |

**MTAKIWOL.BIN — 6 observed triplets**

| Slot IDs | File offsets |
|---|---|
| 111–113 | `0x1A028, 0x1A040, 0x1A048` |
| 324–326 | `0x25D78, 0x25D90, 0x25D98` |
| 402–404 | `0x2A4D0, 0x2A4E8, 0x2A4F0` |
| 414–416 | `0x2AB10, 0x2AB28, 0x2AB30` |
| 3147–3149 | `0x9D724, 0x9D740, 0x9D750` |
| 4437–4439 | `0xDADD0, 0xDADF2, 0xDAE0C` |

### Converting the triplets to runtime motion IDs

The 8,880 entries are not unrelated slots: they are 2,960 consecutive
three-offset rows. The runtime motion ID is one-based, with
`first_slot = (motion_id - 1) * 3`. Therefore each observed triplet maps
directly to a motion ID:

| Motion ID | Hex | MTAKIPAI | MTAKIWOL |
|---:|---:|---|---|
| 38 | `0x26` | yes | yes |
| 109 | `0x6D` | yes | yes |
| 135 | `0x87` | yes | yes |
| 139 | `0x8B` | yes | yes |
| 432 | `0x1B0` | yes | — |
| 528 | `0x210` | yes | — |
| 533 | `0x215` | yes | — |
| 542 | `0x21E` | yes | — |
| 1050 | `0x41A` | yes | yes |
| 1480 | `0x5C8` | — | yes |
| 2386 | `0x952` | yes | — |

So v66 has identified **10 motion IDs selected from the Pai pack and 6 from
the Wolf pack**, with five IDs common to both. These are source-row IDs,
not yet semantic action names or fighter ownership labels. The first common
row, `0x26`, is the one for which the evaluator's count/time/tuple reads
already line up with the three source offsets. The remaining IDs are proven
to have their three table entries read during setup; their evaluator
consumption and any linked-record relationships still need to be mapped.

The analysis report now groups the three table reads into a one-based motion
ID and emits `mt_motion_rows.csv` in addition to the per-slot
`mt_slot_lookups.csv`.

The first triplet gives us a direct bridge from setup to evaluator streams.
Relative to the pack destination, the evaluator's first observed reads are:

- `0x8C09D6F4` (count cursor): `0x1A03A`, inside slot 111's
  `0x1A028` block;
- `0x8C09D70A` (key-time cursor): `0x1A040`, exactly slot 112's
  start;
- `0x8C09D794` (tuple cursor): `0x1A048`, exactly slot 113's
  start, followed by tuple-lane reads at `0x1A04C`, `0x1A050` and
  `0x1A054`.

This strongly supports the interpretation that each consecutive triplet
supplies the evaluator's count, key-time and tuple streams for a motion
record group. The setup PCs and the first evaluator accesses line up exactly
for slots 111–113 in both packs. The remaining triplets are confirmed table
lookups during setup; mapping each to a named move or proving its exact
linkage within the active chain is still open.

The full lookup list is generated as `mt_slot_lookups.csv` by
`tools/loader_trace_asset_map.py`; the per-PC address/value summary is
`memory_pc_summary.csv`. The corresponding workflow completed successfully:
[run #44799078608](https://github.com/cdmn1/vf3-naomi/actions/runs/44799078608).

### Next capture: register-level motion ID and runtime pointer mapping

The v68 trace profile adds expanded FA12/FA13 register snapshots at
`0x8C09D5AE` / `0x8C09D5B0` (motion-ID row selection), the three slot
lookup instructions, evaluator entry `0x8C09D690`, linked-record helper
`0x8C09D69A`, and the count/time/tuple cursor sites. The aim is to capture:

- the actor pointer and its `+0x3C` one-based motion ID;
- the selected 12-byte three-offset row and the registers used to fetch it;
- the evaluator task/header pointer;
- the actual count, time and tuple cursor registers at runtime.

This should tell us which motion IDs belong to which actor and whether the
guest loader rewrites/scatters the source offsets before evaluation. It
requires one fresh capture from a cold game start; v66 remains sufficient
for the disc-to-RAM and slot-row findings above.

### MOTHEAD.BIN is loaded immediately before each motion pack

The trace also contains two separate, identical `MOTHEAD.BIN` read
sequences:
- 17 calls / 83 sectors at trace word 5,642, immediately followed by the
  `MTAKIWOL.BIN` sequence beginning at trace word 5,818;
- 17 calls / 83 sectors at trace word 1,174,133, immediately followed by
  the `MTAKIPAI.BIN` sequence beginning at trace word 1,174,309.

Both header reads use the same destination span,
`0x0C5CA800–0x0C5F4000` (end exclusive). That span ends exactly where each
motion-pack payload begins, at `0x0C5F4000`. The file is therefore loaded
twice into the header area immediately preceding the reused pack buffer,
once for the Wolf sequence and once for the Pai sequence.

### Why the first 96 transfer samples were misleading

The diagnostic prints only the first 96 FA22 samples, which are dominated
by other assets. Mapping representative samples with FAD - 150 gives:

| Sample FAD | GDFS LBA | Indexed asset |
|---:|---:|---|
| 428441 | 428291 | `CP_ASC.BIN` |
| 533101 | 532951 | `COIN.BIN` |
| 533122 | 532972 | `ANAUNCE.BIN` |
| 545440 | 545290 | `PLAYER.BIN` |
| 547602 | 547452 | `SNDDRV.BIN` |

These examples also independently support the +150 FAD/LBA conversion.
They are not the motion-pack reads.

### What this establishes — and what remains

The v66 trace now proves all of the following:
1. the Dreamcast game requests sectors from the indexed `MTAKIWOL.BIN`
   and `MTAKIPAI.BIN` file extents;
2. the returned data is non-zero and the HLE read reports the full requested
   sector count;
3. the data is copied directly into guest RAM at `0x0C5F4000` onward,
   inside the previously identified resident-MT window;
4. `MOTHEAD.BIN` is loaded into the contiguous preceding header area before
   each matchup pack;
5. after each load, the motion-evaluation CPU sites read from addresses
   inside that pack's payload span (32,073 Wolf reads; 26,717 Pai reads).

This does **not yet** identify the internal MT header/offset-table parsing
routine, map the evaluator's reads to specific slot IDs / motion records, or
establish whether the original game performs an additional per-record
scatter/relocation after the initial file copy (no non-zero CPU writes to
this watched payload range were recorded). Those are now the next
reverse-engineering targets. No new capture is needed to establish the
initial disc-to-RAM transfer and evaluator-read path.

The workflow uploads `report.md` and the full per-transfer `transfers.csv`
as artifact `vf3-v66-hle-asset-correlation` for repeatable inspection.

## Limits

This plan is based on the related project's published M33/M66 notes and the user's v32/v38 captures. The runtime table's pointer chase, GDFS call contract and MT scatter routine remain unresolved.

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
