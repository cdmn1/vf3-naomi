# VF3 Naomi Akira static model renderer

This target is the first PowerVR-native fighter geometry path for Naomi.

- Embeds the real MKAO_AKI.POL and ROB_AKI.POL packages.
- Parses the POL slot/model/mesh structure with the pinned upstream parser.
- Validates and selects the 18 Akira selector entries.
- Builds the current inferred Akira bind pose through the native pose builder.
- Transforms and submits the selected mesh triangles to the Naomi PowerVR.
- Auto-fits the selected geometry to the display.

This renderer deliberately draws untextured geometry. The camera fit is now
validated in Flycast and on retail Naomi. The current bind rig and most
pose-field-to-attachment assignments remain inferred, so this is a visual
integration milestone, not a claim of exact retail animation.

The next blocker is the native attachment-matrix producer, not camera framing.
The retail draw path consumes 18 same-index matrices at actor
`+0x180 + 0x40 * selector_index`; the standalone Naomi renderer currently
does not have the native fight actor state required to produce them. See
[`docs/AKIRA_ATTACHMENT_MATRIX_PORT.md`](../../docs/AKIRA_ATTACHMENT_MATRIX_PORT.md)
for the evidence, remaining gaps and implementation gates.
