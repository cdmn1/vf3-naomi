# VF3 v72 pose-trace findings — 2026-11-24

## Executive result

The v72 trace identifies the **two retail SH-4 matrix routines that directly populate the 18-row attachment table**, and it maps the recurring caller return PCs to specific rows. It also identifies a separate pass that rewrites the translation vector in all 18 rows.

This is the first trace-backed map of the direct attachment-table write path. It is not yet a complete map of the mathematical producer for every row: the two matrix routines either copy an already-built matrix or store the current XMTRX. The caller-side code that prepares those matrices is the remaining target.

The report and filtered CSVs were imported from the existing GitHub Release into a temporary Actions artifact. All three published SHA-256 hashes verified. The temporary importer workflow was removed after the artifact was retrieved; no raw trace or large CSV was committed to this branch.

## Input validation and scope

The source report records:

- 69,235,794 little-endian 64-bit words, with no malformed, truncated, or unconsumed words.
- 1,448,938 expanded PC snapshots, 10,421,687 FA20 memory records, and 2,136 opaque nine-word FA22 records.
- 1,175,350 retained snapshots at the three requested PCs.
- 985,556 four-byte writes overlapping the two direct attachment tables.
- No timestamp field in the documented records. `event_index`, `word_index`, and `byte_offset` preserve event order; they are not timestamps.
- The FA22 records were skipped as opaque nine-word records. No timestamp semantics were inferred for them.

The filtered range is exactly the 18 × 64-byte matrix table at actor +0x180:

| Actor | Actor base | Attachment table | End (exclusive) |
|---|---:|---:|---:|
| A | `0x0C1FEFE4` | `0x0C1FF164` | `0x0C1FF5E4` |
| B | `0x0C20138C` | `0x0C20150C` | `0x0C20198C` |

The separate 23-row table at actor +0x7C0 starts at `0x0C1FF7A4` / `0x0C201B4C` and is not part of these results.

## 1. Direct writer A: matrix_copy at 0x8C03B530

Raw memory-write PC: `0x0C03B56A` (the 0x0C mirror of the guest code address).

- 358,144 writes total: 179,072 per actor.
- Writes cover rows **0, 1, 2, 6, 10, 11, 14, 17**.
- Each covered row receives a complete 16-word / 64-byte matrix, written in ascending offsets 0, 4, …, 60.
- Each row has 1,399 complete copies per actor in the recurring path.

The requested snapshot at `0x8C03B56C` pairs one event after each of these writes: **358,144/358,144 exact pairs**. In every pair, register R5 equals the FA20 destination address. R4 counts down from 15 to 0 over the 16 words, while R6 advances by four bytes per word. This is the register shape of the source-to-destination copy path.

That matches the pinned upstream implementation of `matrix_copy`: when both destination and source are non-null, it copies 16 words from source to destination. It does not calculate the matrix itself.

### Dominant caller return PCs for matrix_copy

The PR value in the immediately following PC snapshot groups the recurring copies by row:

| Attachment rows | PR (return PC) |
|---|---:|
| 0 | `0x0C09EBAC` |
| 1 | `0x0C09E026` |
| 2, 6 | `0x0C09E0F8` |
| 10 | `0x0C09E49E` |
| 11 | `0x0C09E5A2` |
| 14, 17 | `0x0C09E9A8` |

The row-0 PR is especially useful: `0x0C09EBAC` is the `CONT_COPY` continuation in the pinned upstream `pose_orchestration.c`. So row 0's copy is directly tied to the already-recovered top-level pose-orchestration path, not an inferred skeleton parent.

The PRs resolve against the pinned upstream caller sources, so the row map can now be stated at the function level:

| Attachment row(s) | Retail caller / output block |
|---|---|
| 0 | top-level `pose_orchestration` |
| 1 | `facing_update` |
| 2, 3, 4, 5 | first `aim_frame` call, output block actor +0x200 |
| 6, 7, 8, 9 | second `aim_frame` call, output block actor +0x300 |
| 10 | `aim_midpoints` |
| 11 | `matrix_basis` |
| 12, 13, 14 | first `aim_frame_b` call, output block actor +0x480 |
| 15, 16, 17 | second `aim_frame_b` call, output block actor +0x540 |

This is a complete 18-row caller map. It follows directly from the trace PR groups and the call continuations in the pinned upstream sources: `facing_update.c`, `aim_frame.c`, `aim_midpoints.c`, `matrix_basis.c`, `aim_frame_b.c`, and `pose_orchestration.c`. In particular, the two `aim_frame` calls account for rows 2–9, and the two `aim_frame_b` calls account for rows 12–17. No parent-bone fitting is needed to establish this producer grouping.

The extra PR `0x0C09D50E` on rows 5 and 9 is the internal return from `matrix_store` inside `store_keep_translation` (`0x8C09D4E6`). The three subsequent stores at `0x0C09D514/51E/526` restore those rows' translation components. This explains why those rows have an additional translation-only writer without implying another attachment producer.


## 2. Direct writer B: matrix_store at 0x8C03B620

Raw write PCs: `0x0C03B62C` through `0x0C03B64A`, every two bytes.

- 541,872 writes total.
- The recurring full-matrix path covers rows **3, 4, 5, 7, 8, 9, 12, 13, 15, 16**.
- The 16 instruction PCs write one word each in descending offsets: PC `0x0C03B62C` writes offset 60, `0x0C03B62E` offset 56, continuing down to PC `0x0C03B64A` at offset 0.
- The routine stores the current XMTRX/XF matrix to the supplied destination. It is the other null-arm behavior documented by upstream `matrix_copy`, and is separately exposed as `matrix_store` at `0x8C03B620`.

The `0x8C03B62E` snapshots pair one event before the matching FA20 write. For **all 33,867 paired events**, R4 equals the write destination plus four bytes. That exact relationship confirms the descending/pre-decrement destination walk. The writes are stores of the current matrix bank, not a copy from a captured GPR value.

### Dominant caller return PCs for matrix_store

The recurring 1,399-call-per-actor row groups resolve as follows:

| Attachment rows | PR (return PC) |
|---|---:|
| 3, 7 | `0x0C09E322` |
| 4, 8 | `0x0C09E3A6` |
| 5, 9 | `0x0C09D50E` and `0x0C09E3E0` |
| 12, 15 | `0x0C09E8BA` |
| 13, 16 | `0x0C09E94E` |

Rows 5 and 9 each have two recurring caller contexts, so each receives two complete matrix-store passes per actor in the capture. The small non-recurring PC groups are not included in this caller map.

**Important distinction:** the Akira selector map has empty model/mesh entries at selector indices 5 and 9, but the corresponding attachment-matrix rows are not empty. The trace shows both rows receiving full matrices and later translation writes.

## 3. Separate translation writers

The matrix format in the pinned upstream `matrix_lib.c` is column-major. The translation vector is the final matrix column, at byte offsets 48, 52, and 56 (float indices 12, 13, and 14).

### All-row translation pass

| Raw write PC | Matrix byte offset | Total writes | Per actor |
|---|---:|---:|---:|
| `0x0C07488C` | 48 | 22,716 | 11,358 |
| `0x0C074894` | 52 | 22,716 | 11,358 |
| `0x0C07489C` | 56 | 22,716 | 11,358 |

Each PC writes the corresponding translation component for all 18 rows, for both actors. The `0x8C074854` snapshot count is 1,262: 631 with R4 equal to actor A's base and 631 with R4 equal to actor B's base. R5 is the opposing actor base. The write counts are exactly 631 × 18 rows for each component and actor.

This is a separate all-row translation pass; it is not either matrix_copy or matrix_store.

### Row-5/row-9 translation follow-up

| Raw write PC | Matrix byte offset | Total writes |
|---|---:|---:|
| `0x0C09D514` | 48 | 5,596 |
| `0x0C09D51E` | 52 | 5,596 |
| `0x0C09D526` | 56 | 5,596 |

These stores touch only rows 5 and 9, on both actors. The relevant `0x8C03B62E` snapshots have PR `0x0C09D50E`; in the ordered trace, a row-5 matrix_store sequence is followed immediately by the three writes at `0x0C09D514`, `0x0C09D51E`, and `0x0C09D526` to that same row's translation slots. This is a caller-side follow-up, not a third matrix writer.

## Actor-kind values in this capture

The watched actor header includes the retail kind byte at actor +`0x61`:

- Actor A (`0x0C1FEFE4`): kind `0`, identified as **Akira** by the pinned native `pose_data.c` record.
- Actor B (`0x0C20138C`): kind `6`. The current native pose-data port has verified records for kinds 0 (Akira) and 7 (Jacky), so kind 6 is not yet covered by that port.

The MTAKIPAI comparison below should therefore be judged primarily against actor A, not treated as a universal match for both fighters.

## 4. Motion channels to the 42 pose halfwords

The same full trace also validates the retail motion-channel conversion:

- PC `0x0C09D7FC` writes the 42-float angle block at actor +`0x1D58` through +`0x1E00` (exclusive): 32,760 writes for actor A and 33,054 for actor B. Those are exactly 42 channels × 780 and 787 updates respectively.
- PC `0x0C09D41C` reads and PC `0x0C09D422` rewrites 14 values per update. Every paired output is exactly the input float bit pattern XOR `0x80000000`: **21,938/21,938 exact sign-flip pairs**. These are the third components of the 14 angle triples, matching the second loop of upstream `vf3_mt_flip`.
- PC `0x0C09D59A` reads the 42 float channels in order; PC `0x0C09D5A4` writes the 42 halfwords at actor +`0x1F50` through +`0x1FA4` (exclusive). Across both actors this gives **65,814 float-to-halfword pairs** (32,760 + 33,054).
- Every one of those 65,814 pairs matches truncation toward zero followed by signed 16-bit storage: **zero mismatches**.

This confirms the exact channel-to-pose-field relationship already implemented by the pinned upstream native motion evaluator: the 42 angle channels at actor +`0x1D58` map in order to the 42 halfword pose fields at actor +`0x1F50`. No channel-to-field permutation or skeleton guess is needed for this stage.

The 21 preceding floats at actor +`0x1D04` through +`0x1D58` are the seven SEG_A vectors consumed by `seven_pass`. They were **outside the v72 memory-watch ranges**, so their values are not present in this decoded trace. The same MT evaluator produces the full 63-channel block; its first 21 values can supply those seven vectors. `seven_pass` transforms them into SEG_B at actor +`0x1EFC`; the v72 watch captured the +`0x1F14` anchor output, but not the complete SEG_B block.

The captured stream does not expose the `mt_play` task header/phase directly. Matching the 42-float sample to a specific MTAKIPAI motion slot and Q8 phase remains a separate asset-timeline task.

## 4. Initialization and residual writes

- `0x0C02002C` clears all 18 × 16 words for both actors: 288 zero writes per actor.
- The remaining 28 filtered writes are isolated events at other PCs. They do not change the recurring writer/caller map above and are not assigned semantics here.

## What this resolves — and what it does not

**Resolved from the trace:**

1. Both exact attachment-table base addresses and the full 18-row ranges.
2. The two retail direct matrix writers: `matrix_copy` (`0x8C03B530`) and `matrix_store` (`0x8C03B620`).
3. Which recurring caller return PCs populate each row group.
4. A separate translation pass across every row, plus the row-5/row-9-specific translation follow-up.
5. Row 0's matrix_copy call is the known top-level pose-orchestration continuation `CONT_COPY`.

**Not yet resolved:**

- The actual math and source data prepared by each caller before it enters matrix_copy or matrix_store.
- The FPU/XF register contents at the store instructions; the retained snapshots contain GPRs, PR, and PC, not the complete FPU register bank.
- A proven semantic parent/joint name for every attachment row. The caller map is not itself a skeleton hierarchy.

## Next reverse-engineering step

Do not add another generic matrix-write probe. Use the mapped PR return PCs as the targets for caller-side disassembly and input recovery:

- matrix_copy callers: `0x0C09EBAC`, `0x0C09E026`, `0x0C09E0F8`, `0x0C09E49E`, `0x0C09E5A2`, `0x0C09E9A8`;
- matrix_store callers: `0x0C09E322`, `0x0C09E3A6`, `0x0C09D50E`, `0x0C09E3E0`, `0x0C09E8BA`, `0x0C09E94E`.

For each caller, identify the call instruction and its delay slot, determine how the current XMTRX or source matrix is constructed, then connect that caller to the pinned native pose/aim routines. Keep the 18 attachment rows separate from the 23-row table at actor +0x7C0.

## References

- [v72 pose-trace analysis release](https://github.com/cdmn1/vf3-naomi/releases/tag/pose-trace-analysis-2026-11-24)
- [Pinned upstream matrix helpers](https://github.com/celsowm/vf3-decomp/blob/208e2e2402743123a4ba79c1733f479bec9f4906/src/fight/matrix_lib.c)
- [Pinned upstream native matrix_copy](https://github.com/celsowm/vf3-decomp/blob/208e2e2402743123a4ba79c1733f479bec9f4906/src/native/matrix_copy.c)
- [Pinned upstream matrix_copy proof and contract](https://github.com/celsowm/vf3-decomp/blob/208e2e2402743123a4ba79c1733f479bec9f4906/docs/re/native_matrix_copy.md)
- [Pinned upstream native pose orchestration](https://github.com/celsowm/vf3-decomp/blob/208e2e2402743123a4ba79c1733f479bec9f4906/src/native/pose_orchestration.c)
