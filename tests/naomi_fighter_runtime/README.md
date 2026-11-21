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
pose-orchestration family and host differential tests. This Naomi target does
not yet compile or invoke that family, and it does not yet supply the live
fighter actor state the family consumes. The next blocker is therefore
integrating that proven source and obtaining its real runtime inputs, not
reimplementing the aim-frame mathematics.

The retail draw path consumes 18 same-index matrices at actor
`+0x180 + 0x40 * selector_index`. The 23-row table at actor `+0x7c0` is
separate. See
[`docs/AKIRA_ATTACHMENT_MATRIX_PORT.md`](../../docs/AKIRA_ATTACHMENT_MATRIX_PORT.md)
for evidence, source references and implementation gates.
