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

The related Flycast fork's `VF3_WATCH` file supports `pc 0xADDR` lines. Each hit records `r13`, `r14`, `r4`, `r5`, `r6`, and `r7` in a probe group. The interpreter normalizes the watched PC for the game's P2 alias.

## Memory watches

Use the same `VF3_WATCH` file for memory ranges:

```text
mem 0x8C1069BC 0x210
mem 0x8C5F0000 0x270000
```

- `0x8C1069BC`, length `0x210`: the 132-pointer runtime table (132 × 4 bytes).
- `0x8C5F0000`, length `0x270000`: broad resident MT destination window. Earlier research found `MTJACLAU.BIN` resident from approximately `0x8C5F4100`; the broad range leaves room for the header and scattered records.

Memory records include the executing PC, access width, read/write flag, address and value. The watch is interpreter-only; dynarec memory accesses bypass these hooks.

## Capture sequence

1. Use the related project's pre-fight `vf3_6` savestate (or an equivalent fresh state immediately before character select). The state and the patched Flycast build are local research artifacts, not included in the public repository.
2. Create a `VF3_WATCH` file containing all five `pc` watches and both `mem` ranges above.
3. Run the patched Flycast interpreter with `VF3_INTERPRETER=1`, `VF3_FULL=1`, `VF3_WATCH=<watch-file>`, `VF3_TRACE=<trace-output>`, `VF3_TRACE_FRAMES=<sufficient-frame-count>`, and `VF3_STATE=<pre-fight-state>`.
4. Drive character select through round one using a reproducible `VF3_PLAY` script. Do not use an attract-only state; the target is the moment the selected fighter's MT pack is loaded.
5. Retain the trace and emulator log. Record the exact state, script, emulator revision and frame count so the result is reproducible.

The related project's `tools/mem_watch.py` shows the environment setup for memory-only traces, while `docs/re/probe_mode.md` documents the PC-probe record format. A combined PC+memory capture may require writing the watch file and setting the environment variables directly, because the helper script's normal CLI constructs memory-only watch files.

## Analysis order

1. **Runtime table pointer:** at the first hit in `mt_loader` / `load_mt_*`, inspect `r13` and `r14` and trace the pointer chain to `0x8C1069BC`. Confirm whether the selected table index is zero-based or one-based and whether it selects a string pointer, a file descriptor or a loader object.
2. **GDFS call contract:** correlate the `load_mt_*` hit with `_gdFsDaPlaySct`; capture `r4..r7`, any pointed-to parameter block and the returned status. Do not assume the BIOS `REQ_CMD`/`GET_CMD_STAT` ABI is the same as the GDFS library function ABI.
3. **Disc resource mapping:** use `GDFS_ASSET_INDEX.csv` to map the selected filename to its manifest LBA, byte size and SHA-1. Compare the actual sector range requested with the manifest entry; account for Mode1/2352 track sectors and the 150-sector FAD/LBA offset where applicable.
4. **MT scatter:** correlate reads from the selected MT file's 8,880-slot table with writes to `0x8C5F0000..0x8C810000`. The goal is to find the function that reads the three-offset row for a motion ID and constructs the runtime `ops[63]`, count, time and tuple streams.
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
- [GDFS / loader pointer investigation](https://github.com/celsowm/vf3-decomp/blob/main/docs/re/loader_m66.md)
- [MT resident scatter evidence](https://github.com/celsowm/vf3-decomp/blob/main/docs/re/mt_mount_m33.md)
- [MT asset layout](https://github.com/celsowm/vf3-decomp/blob/main/docs/formats/MT.md)
- [MT memory-access trace](https://github.com/celsowm/vf3-decomp/blob/main/docs/re/mt_fields.md)
- [This repository's candidate GDFS asset index](GDFS_ASSET_INDEX.csv)
