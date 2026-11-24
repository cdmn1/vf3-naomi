# Akira attachment-matrix port status

This note separates the verified retail draw contract, the already recovered
native pose producer, and the integration work still required by the Naomi
renderer. Source revision:
`celsowm/vf3-decomp`
`208e2e2402743123a4ba79c1733f479bec9f4906`.

Captured runtime attachment matrices are offline evidence only. They must not
be embedded as Naomi runtime pose data.

## Verified renderer contract

- The Akira selector map has 18 entries. Entries 5 and 9 are empty; the
  remaining 16 entries select the head/body POL models and meshes.
- The retail draw path in upstream `src/fight/fighter_refresh.c` and
  `src/fight/scene_link.c` uses the same-index 4x4 attachment matrix at
  `actor + 0x180 + 0x40 * selector_index` for every populated selector.
- The Naomi renderer's 18-element attachment array is therefore the correct
  interface. Its current draw path already passes `attachment[i]` to the
  meshes selected by `parts[i]`.
- The 23-row matrix table at `actor + 0x7c0` is separate. Existing basis
  comparisons do not establish a direct 1:1 mapping to the 18 POL rows.

## Recovered inputs and outputs

- The MT evaluator's 42 halfword pose-field conversion at actor `+0x1f50`
  is now validated against retail trace data: 65,814 channel-to-halfword pairs
  across 1,567 updates, with zero mismatches for truncation toward zero and
  signed 16-bit storage. The 42 float channels at `+0x1d58` map in order to
  the 42 halfwords at `+0x1f50`, forming 14 turn triples.
- The trace also confirms the sign-bit flip of the third component in each of
  the 14 angle triples: 21,938/21,938 pairs match the upstream `vf3_mt_flip`
  behavior exactly.
- The v72 retail trace now resolves the complete 18-row **producer-function
  grouping**: row 0 is copied by top-level pose orchestration; row 1 by
  facing_update; rows 2–5 and 6–9 by the two aim_frame calls; row 10 by
  aim_midpoints; row 11 by matrix_basis; and rows 12–14 and 15–17 by the two
  aim_frame_b calls. The exact direct writers and caller PRs are documented in
  [VF3_POSE_TRACE_V72_FINDINGS.md](VF3_POSE_TRACE_V72_FINDINGS.md).
- This resolves the motion-channel-to-pose-field conversion, but not the
  semantic parent/joint names or the complete pose-field-to-attachment
  transform mapping. The old multi-track relationships (rows 2/0, 5/4, 6/0
  and 9/8 following pose groups 3, 5, 6 and 8 in Z-Y-X order) remain useful
  partial evidence; most bind transforms in `src/native/motion_pose.c` are
  still inferred.
- The first 21 MT outputs are seven SEG_A vectors at actor `+0x1d04`; they
  feed `seven_pass` and produce SEG_B at `+0x1efc`. The v72 watch profile
  captured the 42 angle channels and the +`0x1f14` anchor, but omitted the
  first 21 vectors and the full SEG_B block. The existing 63-channel MT
  evaluator can supply the missing first 21 values once the live motion slot
  and phase are matched.
- The root attachment row has a recovered producer:
  `vf3_native_motion_row0()` / `vf3_native_motion_pose_actor()`. Differential
  replay matches four retained guest specimens within 2e-4. Inputs include
  actor yaw, position, mode/flags, the halfword at `+0x22e4`, three floats
  at `+0x208c`, and the first 21 motion channels.
- The pose orchestration selects a fighter-kind record from actor byte
  `+97`; the kind-0 aim-record selection is record 0 for the four retained
  orchestration specimens.
- The retail pose orchestration writes intermediate blocks at actor
  `+0x200`, `+0x300`, `+0x480` and `+0x540`, and copies its recovered
  root matrix into attachment row 0 at `+0x180`. The v72 trace now maps the
  remaining rows to the exact native callees listed above; the trace does not
  yet prove the semantic parent/joint meaning of every row.
- The per-frame `fighter_refresh` path (0x8C0B1550) walks all 18 slots,
  resolves each slot's scene link (0x8C0B1A46), and invokes the per-slot
  handler-list dispatcher (0x8C0B1B2E). A later pass loads the row matrix and
  dispatches its model drawing. The scene-link path consumes the row matrix
  as its base transform while traversing linked nodes; it must not be
  conflated with the separate 23-bone `bone_pose` pass.
- The handler-list table at image address `0x0C10F7A8` has now been read
  from the existing shared retail asset archive. Its non-empty lists are:
  selector 0: `0x8C0B1B6C`; selector 1:
  `0x8C08D898`, `0x8C0940C8`, `0x8C0B1B7C`, `0x8C0B1C92`;
  selector 14: `0x8C0869BC`; selector 17: `0x8C0B1C8E`.
  All other selector lists are empty. These are callback entry addresses,
  not yet proof of which callback writes which attachment row.

## Important upstream discovery: the full native pose pipeline exists

The pinned upstream revision already contains a plain-C native implementation
of the complete pose-orchestration driver. It is not necessary to re-port the
aim-frame routines from the readable guest implementation.

The relevant upstream source family is:

- `src/native/pose_orchestration.c/.h` — top-level orchestration
- `src/native/motion_aim.c/.h` and `src/native/aim_math.c/.h` — both
  aim-frame builders and shared matrix/aim helpers
- `src/native/aim_midpoints.c`, `matrix_basis.c`, `halfword_frame.c`,
  `facing_update.c`, `seven_pass.c` — orchestration callees
- `src/native/pose_callees.h`, `matrix.c/.h`, `pose_data.c/.h` — shared
  APIs, matrix math and fighter-kind data

The upstream `docs/re/native_pose_orchestration.md` reports 4/4 differential
replay scenarios passing (kinds 3, 12, 21 and 0), with 14/14 deliberate
mutations caught. The upstream `docs/re/native_pose_chain.md` also reports
five passing orchestration-plus-postprocessor cases. These are host-side
differential results against the readable guest authority; they are not yet
proof of the Naomi build's SH-4 numerical behavior or of a complete live game
actor state in the standalone Naomi renderer.

## Why the current Naomi image is still a mannequin

The current Naomi target is a standalone POL renderer. It parses the assets,
selects the correct parts and submits them to PowerVR. The current Akira build
uses the real MTAKIPAI motion 1050 data and the recovered native row-0 root
transform, so it is beyond the earlier zero-pose mannequin. However, rows 1–17
still use the inferred bind/attachment rig; the renderer does not yet invoke
the complete upstream pose-orchestration family for each live frame.

The actual remaining gap is **feeding real actor/work state into the full
native pose pipeline and replacing the inferred rows 1–17**. The v72 trace
maps all 18 retail row producers and validates the 42-channel-to-42-halfword
conversion, but the trace watch did not include the first 21 SEG_A vectors at
actor +`0x1d04` or the complete SEG_B block at +`0x1efc`. The existing MT
asset evaluator can produce those first 21 values; the remaining work is to
match the captured actor's motion slot/phase, reconstruct the required actor
and global inputs, and call the full native orchestration in the renderer.
The per-slot refresh/handler/scene-link path remains a separate draw/relink
task. Do not substitute guessed values or captured matrices as runtime pose
data.

## Next implementation gates

1. Integrate the pinned native pose-orchestration source family into a
   Naomi-compatible build, with its dependencies explicit and no SDL/guest
   emulator runtime dependency.
2. The Naomi SH-4 conformance probe using deterministic synthetic fixtures is
   already present; retain it as a separate proof of the native math, not as
   evidence of a live fighter.
3. Use the v72 caller map to wire the already-recovered native pose
   orchestration into the Naomi renderer; keep matrix_copy/matrix_store and
   the all-row translation pass distinct.
4. Recover the live actor/work state and motion inputs needed by that
   orchestration from the full v72 memory trace before requesting another
   Flycast capture. Keep the per-slot handler/scene-link/relink path separate
   from the 18-row matrix producer.
5. Reconstruct and compare all populated attachment rows against independent
   retail captures across multiple poses; only then replace the inferred bind
   rig in the visible renderer.
6. Keep texture upload and live motion playback as separate milestones after
   the geometry/attachment path is sound.

The 23-row table at actor `+0x7c0` must remain separate unless a proven
conversion to the 18 attachment rows is found.
