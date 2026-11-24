# VF3 Naomi Akira static model renderer

This target is the first PowerVR-native fighter geometry path for Naomi.

- Embeds the real MKAO_AKI.POL and ROB_AKI.POL packages.
- Parses the POL slot/model/mesh structure with the pinned upstream parser.
- Validates and selects the 18 Akira selector entries.
- Builds the current inferred Akira bind pose through the native pose builder.
- Transforms and submits the selected mesh triangles to the Naomi PowerVR.
- Auto-fits the selected geometry to the display.

This renderer deliberately draws untextured geometry. The camera fit is
validated in Flycast and on retail Naomi. The current bind rig and most
pose-field-to-attachment assignments remain inferred, so this is a visual
integration milestone, not a claim of exact retail animation.

The pinned upstream VF3 source already contains the complete plain-C native
pose-orchestration family and host differential tests. The v72 retail trace
now maps all 18 attachment rows to their exact producer groups: pose
orchestration, facing update, two aim-frame passes, aim midpoints, matrix basis
and two aim-frame-B passes. The trace findings are in
[`docs/VF3_POSE_TRACE_V72_FINDINGS.md`](../../docs/VF3_POSE_TRACE_V72_FINDINGS.md).

This Naomi renderer still calls the inferred motion-pose builder; it does not
yet invoke the full orchestration with live retail actor/work state. The next
blocker is recovering those inputs and connecting the already-proven native
source to the renderer, not inventing another skeleton hierarchy or repeating
generic matrix-write probes.

The retail draw path consumes 18 same-index matrices at actor
`+0x180 + 0x40 * selector_index`. The 23-row table at actor `+0x7c0` is
separate. See
[`docs/AKIRA_ATTACHMENT_MATRIX_PORT.md`](../../docs/AKIRA_ATTACHMENT_MATRIX_PORT.md)
for evidence, source references and implementation gates.
