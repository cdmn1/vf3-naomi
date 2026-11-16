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

1. Run `run-trace.cmd` from the extracted package. It sets `VF3_INTERPRETER=1`, `VF3_WATCH`, and `VF3_TRACE` automatically.
2. In the Flycast GUI, load the **Dreamcast** VF3 game image used by the decomp research (not the Naomi ROM image).
3. Manually navigate to the actual character-select confirmation and continue into Round 1. A savestate at that boundary is not required for this first pass.
4. Close Flycast normally. The output `vf3-loader-trace.bin` will be beside `flycast.exe` in the extracted package.
5. Retain the trace and emulator log. Record the exact game image, emulator revision and whether the capture reached Round 1.

Important: if the run reaches only an autonomous attract/demo fight, the one-time loaders may already have finished. That capture will not answer this plan's question. The existing `vf3_play_frontend_ladder.txt` is not a verified route to the boundary; continue solving the input/navigation path rather than assuming it works.

## Decoding a retained capture

The combined `VF3_TRACE` stream contains the PC-probe marker groups (`0xFA10`/`0xFA11`) and memory-access records (`0xFA20`) described by the related project's trace notes. After a capture, decode those records with:

```text
python tools/loader_trace_decode.py path/to/vf3-loader-trace.bin --prefix extract/analysis/loader_trace
```

This writes `loader_trace.pc.csv` (PC plus `r13`, `r14`, `r4`–`r7`) and `loader_trace.memory.csv` (PC, access type, width, address and value). Eight-byte memory values are retained at their full 64-bit width. Unknown trace words are skipped; malformed or truncated recognized groups are reported as warnings. The decoder is for these documented probe/memory records, not a general trace disassembler.

For a first-pass report, run:

```text
python tools/loader_trace_summary.py extract/analysis/loader_trace.pc.csv extract/analysis/loader_trace.memory.csv
```

The report includes the first register snapshot for each watched function, pointer-table read/write values, and the first resident-MT writes. These samples help narrow the pointer chain and scatter routine; they are not by themselves proof that a resource load completed.

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
