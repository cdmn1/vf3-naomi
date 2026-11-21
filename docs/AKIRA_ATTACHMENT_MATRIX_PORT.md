# Akira attachment-matrix port status

This note separates the renderer's verified attachment contract from the still
incomplete native pose producer. It is based on the pinned
`celsowm/vf3-decomp` revision
`208e2e2402743123a4ba79c1733f479bec9f4906`; it does not promote captured
runtime matrices into shipped data.

## Verified renderer contract

- The Akira selector map has 18 entries. Entries 5 and 9 are empty; the
  remaining 16 entries select the head/body POL models and meshes.
- The retail draw path in upstream `src/fight/fighter_refresh.c` and
  `src/fight/scene_link.c` uses the same-index 4x4 attachment matrix at
  `actor + 0x180 + 0x40 * selector_index` for every populated selector.
- Therefore the Naomi renderer's 18-element attachment array is the correct
  interface. The current renderer already passes `attachment[i]` to the
  meshes selected by `parts[i]`.
- The 23-row matrix table at `actor + 0x7c0` is a different table. Basis
  comparisons in the upstream research do not establish a direct 1:1 mapping
  from those rows to the 18 POL attachment rows.

## What has been recovered

- The MT evaluator and conversion to the 42 halfword pose fields at actor
  `+0x1f50` are validated independently. The 42 fields form 14 turn triples.
- Four row/group relationships are supported by multi-track comparison:
  attachment rows 2/0, 5/4, 6/0 and 9/8 follow pose groups 3, 5, 6 and 8 in
  Z-Y-X order. The remaining row/group assignments and most bind transforms
  in `src/native/motion_pose.c` are explicitly described upstream as inferred.
- The root attachment row (row 0) has a recovered producer:
  `vf3_native_motion_row0()` and `vf3_native_motion_pose_actor()`. Its
  differential replay matches four retained guest specimens within 2e-4.
  This function requires actor yaw, position, mode/flags, the halfword at
  `+0x22e4`, three floats at `+0x208c`, and the first 21 motion channels.
- The kind-0 aim-record selection is recovered as record 0 for the four
  retained orchestration specimens. Its twelve leading floats are embedded
  in upstream `src/native/motion_pose.c` and checked against the image data.
- The two `aim_frame` calls write matrix blocks beginning at actor
  `+0x200` and `+0x300`; the two `aim_frame_b` calls operate on blocks
  beginning at `+0x480` and `+0x540`. These routines are readable guest
  implementations, but are not yet a complete native attachment builder.

## Why the current Naomi image is still a mannequin

The current Naomi target is a standalone POL renderer. It parses the assets,
selects the correct parts and submits them to PowerVR, but it does not run the
native fight-state update that would populate the retail actor structure.
Consequently it has no live actor fields to feed the recovered row-0 builder
or the aim-frame routines. It currently calls the inferred
`vf3_native_motion_pose_fields()` with zero fields, which creates the
approximate bind mannequin shown in v55.

The remaining gap is not a camera issue and not a failure to parse the POL
meshes. It is the absence of a native actor-state producer and a verified
implementation of the full attachment-matrix orchestration.

## Next implementation gates

1. Bring the native actor state needed by the pose chain into the Naomi
   runtime. Do not substitute guessed constants for missing per-frame state.
2. Port and replay the `aim_frame`, `aim_frame_b`, midpoint and basis
   builders as pure native matrix routines, preserving their matrix/FPU
   conventions. Use guest differential fixtures where available.
3. Reconstruct the complete 18-row attachment output from those builders and
   prove all populated rows against independent guest captures across more
   than one pose.
4. Only then replace the inferred bind rig in the renderer. Keep the separate
   23-bone table out of this path unless a proven conversion is found.

Captured attachment matrices remain useful for offline visual comparisons
only. They must not be baked into the Naomi runtime.
