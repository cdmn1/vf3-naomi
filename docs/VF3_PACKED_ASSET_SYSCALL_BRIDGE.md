# VF3tb packed-asset GD-ROM bridge

## Current target profile

The first NetDIMM profile retains every extracted VF3tb USA asset except
`MOVIE_A.SFD`. `MOVIE_B.SFD` remains included. The current checked extraction
contains 412 manifest assets; the packed profile contains 411 files.

The sector-aligned payload is 321,267,712 bytes (306.38 MiB). It is intended
to fit within a 512 MiB NetDIMM when combined with the Naomi executable and
runtime overhead.

## Runtime mapping

`tools/pack_vf3_assets.py` produces a compact payload and JSON sidecar. Each
entry preserves the original GD-ROM FAD range while assigning a compact
payload offset. `tools/generate_vf3_asset_map_header.py` converts that map
into the SH-4 compile-time table in
`tests/naomi_gdrom_syscall_probe/vf3_asset_map.h`.

The syscall handler now resolves the requested FAD to an asset range and
translates it to the corresponding cartridge byte offset. A request is split
into separate Holly DMA operations at packed-asset boundaries. If a request
continues into a FAD range that is not present in the map (including the
intentionally omitted MOVIE_A.SFD range), the bridge reports an error instead
of reading unrelated packed data.

Each DMA chunk is capped at 1 MiB. A single request is capped at 32 MiB.
DMA completion updates the transferred-byte count, destination pointer, FAD
and remaining-sector count before the next chunk is started.

## Important build detail

The runtime table stores absolute offsets in the final Naomi ROM image.
Therefore the final image builder must:
1. Build the Naomi executable once to determine its final size.
2. Align the appended asset payload start to 2048 bytes.
3. Generate the header with that aligned payload base.
4. Rebuild and verify that the executable segment size did not change.
5. Append the packed payload at exactly that offset.

The current CI job compiles the map-aware bridge against a temporary aligned
base as a compile check. It does not yet create or publish a combined
commercial-game asset image, and it does not claim that VF3tb boots.

## Current validation

- The existing v6 syscall probe established a one-sector DMAREAD/PIOREAD
  transport path at the synthetic FAD 45150 / image offset zero.
- Packer tests check sector alignment, FAD mapping, and SHA-1 mismatch failure.
- Header-generator tests check absolute offsets, sorted ranges, overlap
  rejection, and payload-base alignment.
- The map-aware SH-4 bridge is compiled by GitHub Actions against the complete
  411-entry profile.
- Real-game boot, requests to the omitted movie range, and non-zero-offset
  cartridge DMA still require runtime validation in Flycast/NAOMI.

## Safety constraints

The 306 MiB profile is the first target. Do not silently fall back to the
239 MiB profile (which also removes MOVIE_B.SFD). Do not map the excluded
MOVIE_A.SFD range to another file. Do not distribute the commercial game
payload as a public GitHub Actions artifact.
