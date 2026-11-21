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

- The MT evaluator and conversion to the 42 halfword pose fields at actor
  `+0x1f50` are independently validated. The 42 fields form 14 turn triples.
- Four row/group relationships are supported by multi-track comparison:
  attachment rows 2/0, 5/4, 6/0 and 9/8 follow pose groups 3, 5, 6 and 8 in
  Z-Y-X order. Other row/group assignments and most bind transforms in
  `src/native/motion_pose.c` remain explicitly inferred.
- The root attachment row has a recovered producer:
  `vf3_native_motion_row0()` / `vf3_native_motion_pose_actor()`. Differential
  replay matches four retained guest specimens within 2e-4. Inputs include
  actor yaw, position, mode/flags, the halfword at `+0x22e4`, three floats
  at `+0x208c`, and the first 21 motion channels.
- The pose orchestration selects a fighter-kind record from actor byte
  `+97`; the kind-0 aim-record selection is record 0 for the four retained
  orchestration specimens.
- The retail orchestration writes intermediate blocks at actor
  `+0x200`, `+0x300`, `+0x480` and `+0x540`, then produces the
  attachment matrix table consumed by the draw path.

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
selects the correct parts and submits them to PowerVR, but it does not run the
native fight-state update that would populate the retail actor structure.
It currently calls the inferred `vf3_native_motion_pose_fields()` with zero
fields, which creates the approximate bind mannequin shown in v55.

The actual remaining gap is integration, not inventing the aim-frame math:
the Naomi target does not yet compile or invoke the upstream native
pose-orchestration family, and it does not yet have the per-frame actor state
that family consumes. The orchestration expects a populated actor/work region,
the fighter-kind record table, the kind-code table, and its matrix-stack
context. Supplying guessed values would create another synthetic pose, not a
retail pose.

## Next implementation gates

1. Integrate the pinned native pose-orchestration source family into a
   Naomi-compatible build, with its dependencies explicit and no SDL/guest
   emulator runtime dependency.
2. Add a Naomi SH-4 conformance probe using deterministic synthetic fixtures
   from the upstream differential tests. Compare its caller-visible outputs
   against known reference values. This validates the native code on Naomi
   without pretending the fixture is a live fighter.
3. Recover and supply the required live actor state and motion channels from
   the actual VF3 runtime path. Do not substitute guessed constants.
4. Reconstruct and compare all populated attachment rows against independent
   retail captures across multiple poses; only then replace the inferred bind
   rig in the visible renderer.
5. Keep texture upload and live motion playback as separate milestones after
   the geometry/attachment path is sound.

The 23-row table at actor `+0x7c0` must remain separate unless a proven
conversion to the 18 attachment rows is found.
