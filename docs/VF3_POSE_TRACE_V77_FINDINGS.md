# VF3 pose trace v77 findings

Capture source: `VF3-Flycast-Trace/VF3-Flycast-Trace-v77` on `main`.

Trace SHA-256: `fcc053849927fde1edb1d424f2ec10859f32953dd87ea33faf0ae6bcc60126a7`

## Intake and decoder validation

The 7-Zip archive expands to a 539,694,824-byte trace (67,461,853 little-endian 64-bit words). The streaming decoder reports:

- 1,137,227 PC snapshot groups
- 11,458,829 watched-memory records
- 2,136 opaque `FA22` records, each retained as a nine-word record boundary
- zero malformed PC/memory markers
- zero GD-ROM DMA records

The diagnostics report 1,326 hits at `0x8C09D690`, the MT frame-evaluator entry.

The decoder preserves the global event index. The PC and memory streams were merged by that index, so each evaluator-entry snapshot can be associated with the most recent preceding actor motion-ID read and cycle-field write.

## MT evaluator call inputs recovered

The pinned upstream implementation at `208e2e2402743123a4ba79c1733f479bec9f4906` documents the evaluator inputs:

- R4: task/actor pointer
- R5: `float **out_pp`
- R6: motion phase in 1/256-frame units
- `task + 0x1A00`: 16-bit cycle length
- `task + 0x1D00`: pointer to the runtime motion header

The v77 snapshots at `0x0C09D690` agree with that calling convention. R4 identifies the two actor bases, R6 contains the phase, and the R3 value at entry matches the cycle value written to `actor + 0x1A00` in **all 1,326 calls** (zero mismatches). R5 is usually `0x0C31F9D8` (624 entries for each actor), with a small number of alternate output-pointer contexts.

| Actor base | Entry snapshots | Motion ID / cycle / entry count |
|---|---:|---|
| `0x0C1FEFE4` | 659 | `0x41A / 60`: 572; `0x09C / 31`: 38; `0x0A6 / 31`: 33; `0x0A0 / 31`: 8; `0x0A2 / 31`: 8 |
| `0x0C20138C` | 667 | `0x952 / 80`: 295; `0x5C8 / 48`: 192; `0x1AC / 61`: 63; `0x8ED / 36`: 38; `0x421 / 33`: 35; `0x0EE / 32`: 10; `0x966 / 29`: 10; `0x968 / 35`: 9; `0x969 / 31`: 7; `0x6FC / 28`: 2; `0x6F1 / 28`: 2; `0x216 / 20`: 2; `0x1B6 / 19`: 2 |

These motion IDs were obtained by correlating each evaluator entry with the latest preceding 16-bit read of `actor + 0x3C`; they are not guesses from pose similarity. The cycle values were independently correlated with writes to `actor + 0x1A00`.

Actor A's dominant motion is therefore **ID `0x41A`, cycle 60**. The upstream pinned `docs/re/mt_fields.md` identifies row `0x41A` in `MTAKIJAC.BIN`, with source stream offsets `0x997F4`, `0x99810`, and `0x99820` (9 count bytes, 13 key-time bytes, 404 tuple bytes). This establishes that the row exists in that pack; it does **not**, by itself, prove that MTAKIJAC was the pack mounted by this particular runtime.

Actor B uses several motion IDs during the capture. Its pack/character mapping is not established by this trace alone.

The phase register R6 ranges from `0x100` to `0x3C00` for actor A and `0x100` to `0x5000` for actor B. These are Q8 phase values (1/256-frame units), not whole-frame counters.

## 63-channel evaluator output verified

The memory writes at the two actor output blocks match the upstream 63-channel evaluator structure exactly.

At PC `0x0C09D7FC`, the trace contains:

| Actor | First 21 floats | Next 42 floats | Complete 63-float output calls |
|---|---:|---:|---:|
| `0x0C1FEFE4` | 13,314 = 21 × 634 | 26,628 = 42 × 634 | 634 |
| `0x0C20138C` | 13,503 = 21 × 643 | 27,006 = 42 × 643 | 643 |

The two post-evaluation sign-flip passes also match the decompiled routine exactly:

- PC `0x0C09D412`: 7 writes per output call (4,438 actor A; 4,501 actor B)
- PC `0x0C09D422`: 14 writes per output call (8,876 actor A; 9,002 actor B)

The downstream 42-halfword pose conversion writes are also present: PC `0x0C09D5A4` accounts for 42 fields per complete output call (26,628 actor A; 27,006 actor B), with 21 additional initialization writes in each actor's watched range.

This is a strong runtime confirmation of the 63-channel evaluator and its 7+14 sign-flip post-pass, not merely a successful synthetic test.

## Exact per-call output targets recovered

The successful final correlation artifact was inspected independently. A new extractor, `tools/extract_v77_mt_targets.py`, assigns every watched output write to the preceding evaluator call for the same actor, retains the final value after the sign-flip stores, and exports all 63 channels as IEEE-754 bit patterns.

Results:

- Actor A: 634 complete 63-channel outputs; 25 evaluator entries had no writes to this actor output block.
- Actor B: 643 complete 63-channel outputs; 24 evaluator entries had no writes to this actor output block.
- Every complete call has exactly 63 distinct channel words.
- Every complete call has exactly 21 final post-sign-flip writes (7 + 14).
- Total: **1,277 complete per-call target rows**, each carrying actor, motion ID, cycle, exact Q8 phase, R5 context, and all 63 channel bit patterns.

This gives us the guest's actual final evaluator outputs at exact phases. It is a much stronger comparison target than a handful of manually selected screenshots or guessed motion samples.

## First native-versus-runtime comparison: MTAKIPAI

A host comparator, `tools/mt_v77_compare.c`, now runs the pinned upstream MT parser/evaluator against every captured target for a selected actor/motion/cycle. It compares all 63 IEEE-754 channel words at the exact captured Q8 phase, including the guest's final sign-flip output.

The first candidate tested was the already available `MTAKIPAI.BIN` (1,572,408 bytes). It contains motion ID `0x41A` and loads successfully for all **549 complete Actor A calls** with ID `0x41A`, cycle 60. However:

- exact full-call matches: **0 / 549**
- exact channel words: **15,519 / 34,587**
- overall RMSE: **11,161.8685**
- maximum absolute channel difference: **178,676.625**

Therefore MTAKIPAI is **not an exact match** for the live Actor A output in this capture. This does not prove the native evaluator is wrong: the runtime may be using a different paired MT pack, and the same motion ID can exist in more than one pack. The result makes pack identity the immediate question, rather than another generic evaluator probe.

Comparison workflow: `.github/workflows/compare-v77-mt-outputs.yml`. Successful run: [45614466600](https://github.com/cdmn1/vf3-naomi/actions/runs/45614466600). Artifact: `vf3-v77-mt-output-comparison`.

## Downstream seven-vector output

The 21-float SEG_B blocks at actor offsets `+0x1EFC` each contain 23,436 writes. Three component-writer PCs (`0x0C03B45C`, `0x0C03B45E`, and `0x0C03B460`) each contribute 7,805 writes per actor, i.e. seven values per component writer across 1,115 updates, plus 21 initialization clears. This gives us a concrete output stream to correlate with the seven-vector transform stage already identified in v72.

## Remaining gap and next input

The exact candidate to test next is `MTAKIJAC.BIN`, because the pinned upstream notes identify row `0x41A` there. I located that file in Dropbox at `/Vf3-naomi/MTAKIJAC.BIN` (1,491,008 bytes). Its current Dropbox link has audience `no_one` (private), so the GitHub Actions runner cannot download it. The Dropbox connector can identify the file, but the private file content is not available as raw bytes to the comparison runner.

To finish the comparison, I need an accessible copy of that one file: either a temporary Dropbox anyone-with-the-link download URL, or the file placed in a private repository/storage location the runner can read. I will not make the file public or change its sharing settings without your approval. Once the bytes are available, the existing comparator can test the 549 captured Actor A calls directly.

The v77 watch profile did not include the four-byte runtime header pointer at actor `+0x1D00` (Actor A: `0x0C200CE4`; Actor B: `0x0C20308C`). That pointer is **not needed to start the source-row output comparison** because the trace already gives the motion ID, cycle, phase, and final channel values. Only add those two addresses to a tiny follow-up watch if a successful asset comparison still leaves the mounted-header identity unresolved. Do not repeat the broad v77 capture.

## Reproducible analysis outputs

The v77 streaming analysis is generated by `tools/analyze_pose_trace_stream.py`, `tools/analyze_pose_trace_correlation.py`, and `tools/extract_v77_mt_targets.py` through `.github/workflows/analyze-pose-trace-v77.yml`. The workflow artifact includes compressed PC snapshots, watched-memory records, correlated MT calls, and the exact 63-channel target CSV. The trace itself is not copied into the repository.
