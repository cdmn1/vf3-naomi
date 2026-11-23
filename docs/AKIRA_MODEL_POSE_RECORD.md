# Fighter model pose record findings

Branch: `akira-native-row0-experiment`

## Recovered runtime resource object

The actor's `+0x54` pointer resolves to a runtime model-resource object. For the two retained actors:

| Actor | Runtime object | Model record | Motion table |
|---|---:|---:|---:|
| A | `0x0C03006C` | `0x0C11D4E8` | `0x0C10E548` |
| B | `0x0C030D6C` | `0x0C11D1B8` | `0x0C10EB60` |

The model-record pointers are also present in the retail image's static resource objects (for example, pointer fields at `0x8C030070` and `0x8C030C70`). This confirms that the actor's resource object is not an inferred pointer arrangement.

## Model-record layout

The two model records are separated by `0x330` bytes and each occupy a `0x330`-byte record in the retail image.

Observed layout:

- `+0x00..+0x0F`: four-word header.
- `+0x10..+0xCB`: 47 words copied into actor offsets `+0xC0..+0x178`.
- `+0xCC..+0x32F`: 153 words, exactly 17 groups of 9 floats (three float3 vectors per group).

The 17 groups are model-specific data. Their exact semantic labels and the mapping from each group to an attachment row are not yet proven; do not treat the grouping as a confirmed row-order mapping.

## Independent validation: attachment row 1

The first model-specific pose parameter is:

- Actor A: actor `+0xC0 = 0.37`.
- Actor B: actor `+0xC0 = 0.31`.

For both retained actors, the captured row-1 translation minus row-0 translation equals the row-0 first basis column multiplied by that parameter:

```
A: (row1.translation - row0.translation) ~= 0.37 * row0.column0
B: (row1.translation - row0.translation) ~= 0.31 * row0.column0
```

The Euclidean residuals from the captured float words are approximately:

- Actor A: `9.89e-7`
- Actor B: `5.23e-7`

This independently confirms that actor `+0xC0` is the local row-1 offset along the root matrix's first basis axis. It is not a guessed bind offset.

## 23-bone to attachment-row advance table

The retail byte table at `0x0C119670` contains:

```
40 80 00 40 00 40 80 00 40 00 40 80
40 00 40 00 40 40 00 40 00 40 40
```

The bone routine consumes the current joint record before applying each byte advance. Therefore the attachment-row index used by each bone is:

```
bone:  00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22
row:   00 01 03 03 04 04 05 07 07 08 08 09 11 12 12 13 13 14 15 15 16 16 17
```

This yields 15 distinct attachment rows used by the 23-bone routine. Rows 2, 6 and 10 are not referenced by this table; rows 5 and 9 are referenced even though their selector entries are empty in the retained Akira capture.

## Remaining gap

The model record and row-1 relationship are now grounded, but the remaining 17 attachment transforms still need to be traced through the actor's pose inputs and joint-chain setup. In particular, the two five-link pose records at actor offsets `+0x20C8` and `+0x217C`, the full attachment matrix table at `+0x180`, and the 23-bone output at `+0x7C0` were not present in the earlier 512-byte actor captures.

A targeted full-pose watch configuration is now available at `tools/VF3_WATCH_POSE_FULL.txt`. The matching report parser is `tools/inspect_actor_watch_trace.py`.
