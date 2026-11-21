# VF3 Naomi Akira static model renderer

This target is the first PowerVR-native fighter geometry path for Naomi.

- Embeds the real MKAO_AKI.POL and ROB_AKI.POL packages.
- Parses the POL slot/model/mesh structure with the pinned upstream parser.
- Validates and selects the 18 Akira selector entries.
- Builds the current inferred Akira bind pose through the native pose builder.
- Transforms and submits the selected mesh triangles to the Naomi PowerVR.

This first renderer deliberately draws untextured geometry. The companion TEX
assets and decoder are validated by the host preflight; PowerVR texture upload
and live motion playback are the next steps. The bind rig and most pose-field
assignments remain inferred, so this is a visual integration milestone, not a
claim of exact retail animation.
