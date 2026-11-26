# VF3tb packed-asset GD-ROM bridge

## Current target profile

The first NetDIMM profile retains every extracted VF3tb USA asset except
`MOVIE_A.SFD`. `MOVIE_B.SFD` remains included. The checked manifest contains
412 assets; the target packed profile contains 411 files.

The raw retained data is 320,977,051 bytes. After 2048-byte sector alignment,
the payload is 321,267,712 bytes (306.38 MiB). The estimated combined image
fits within a 512 MiB NetDIMM.

## Runtime mapping and DMA

`tools/pack_vf3_assets.py` verifies each extracted file's size and SHA-1,
writes the compact payload, and emits a JSON sidecar. The metadata-only
`tools/generate_vf3_asset_map_from_manifest.py` can recreate the same FAD
layout without downloading the extracted files. The
`tools/generate_vf3_asset_map_header.py` converts the JSON map into the
SH-4 compile-time table.

The syscall handler resolves each requested FAD to an asset range and
translates it to the corresponding absolute cartridge byte offset. A request
is split into separate Holly DMA operations at packed-asset boundaries. If a
request continues into a FAD range that is not present in the map (including
the intentionally omitted MOVIE_A.SFD range), the bridge reports an error
instead of reading unrelated packed data.

Each DMA chunk is capped at 1 MiB. A single request is capped at 32 MiB.
DMA completion updates the transferred-byte count, destination pointer, FAD,
and remaining-sector count before the next chunk is started.

## Build arrangement

The runtime table stores absolute offsets in the final Naomi ROM image.
The build therefore performs a preliminary link to determine the executable
size, aligns the payload base to 2048 bytes, regenerates the table with that
absolute base, and links the base ROM again. The final image combiner
`tools/append_vf3_assets_to_naomi_rom.py` verifies the header, map, payload
size, and 512 MiB limit before appending the packed payload.

The map-aware base-ROM CI build includes a runtime smoke test for the actual
`1ST_READ.BIN` FAD and payload offset. Its on-screen test compares a
2048-byte cartridge PIO read with the syscall bridge's Holly DMA read. The
base-ROM artifact intentionally excludes the commercial payload; the payload
is built separately and is not published as a GitHub Actions artifact.

## Current validation

- The earlier v6 syscall probe established a one-sector DMAREAD/PIOREAD
  transport path at synthetic FAD 45150 / image offset zero.
- The packed payload was successfully generated once from the extracted
  412-file folder: 411 files retained, 321,267,712 sector-aligned bytes.
- Packer tests check sector alignment, FAD mapping, and SHA-1 mismatch failure.
- Map-header tests check absolute offsets, sorted ranges, overlap rejection,
  payload-base alignment, and the 1ST_READ smoke-test entry.
- ROM-combiner tests check exact append offset, padding, and header/base
  consistency.
- The map-aware SH-4 bridge compiled successfully against the full 411-entry
  table in GitHub Actions.
- Real-game boot and Flycast/NAOMI validation of non-zero-offset and
  multi-chunk DMA are still outstanding.

## Safety constraints

The 306 MiB profile is the first target. Do not silently fall back to the
239 MiB profile (which also removes MOVIE_B.SFD). Do not map the excluded
MOVIE_A.SFD range to another file. Do not publish the commercial game payload
as a public GitHub Actions artifact.
