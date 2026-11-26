# VF3 Dreamcast GD-ROM syscall bridge probe

This target installs a handler at BIOS service vector 0x8C0000BC and
exercises the r6=0, r7=0..13 syscall interface found in both retail
1ST_READ.BIN and RELOAD.BIN.

The handler translates DMAREAD (0x11) and PIOREAD (0x10) requests into
Naomi System Bus GD-DMA reads from the NetDIMM cartridge. It supports the
request, transfer, status and server calls used by the retail code.

## Two build configurations

- **Transport probe:** the checked-in default `vf3_asset_map.h` has zero
  entries. This preserves the v6 synthetic test mapping, FAD 45150 to image
  offset zero, and is used by the on-screen one-sector PIO/DMA comparison.
- **Retail asset-map build:** CI generates a 411-entry table from the retail
  VF3tb USA manifest, excluding only `MOVIE_A.SFD`. The runtime smoke test
  targets the actual `1ST_READ.BIN` FAD and packed offset. The syscall handler
  translates requested FADs to absolute cartridge offsets and chains Holly
  DMA chunks across packed-asset boundaries.

The map-aware base-ROM artifact includes the correct absolute payload offset,
but deliberately does not include commercial game assets. Its runtime test
requests all 576 sectors of 1ST_READ.BIN (1,179,648 bytes), forcing the
handler to complete two Holly DMA chunks at the 1 MiB boundary; the first
sector is compared with direct cartridge PIO. Append the separately packed
306 MiB payload with
`tools/append_vf3_assets_to_naomi_rom.py` before testing it in Flycast or on
a Naomi cabinet. The full combined image has not yet been run.

No serial connection is required. No commercial game assets are committed
to this repository or uploaded as GitHub Actions artifacts.


## Data-integrity diagnostic fixture

The map generator stores each entry's offset relative to the appended payload.
The syscall bridge adds `VF3_ASSET_PAYLOAD_BASE` exactly once when programming
the Naomi cartridge DMA offset. The generated `VF3_ASSET_TEST_OFFSET` is the
corresponding absolute cartridge address used by the direct PIO reference read.

CI also builds a self-contained diagnostic ROM with a deterministic 576-sector
synthetic fixture at the cartridge offset named by its generated map. It uses
the retail test FAD (548574) only to exercise the same lookup path; the fixture
is not the retail executable and contains no commercial game data. Unlike the
metadata-only asset-map base ROM, this diagnostic image physically contains
the bytes that PIO and DMA read, so its two data-comparison lines are meaningful
without the separate Dropbox payload. It is intended to isolate bridge address
translation and transfer integrity before the real packed assets are available.


## Full-manifest offset diagnostic

After the one-entry synthetic data-check passes, CI builds a second diagnostic
using the complete 411-entry VF3tb USA packed FAD map generated from
`docs/GDROM_DISC_FILE_MAP.csv`. The ROM is extended to the full sector-aligned
payload length (about 306.6 MiB), but remains sparse: only the deterministic
576-sector test fixture is written, at the actual packed payload offset for
`1ST_READ.BIN`. The test therefore exercises the real manifest FAD lookup and
the non-zero, high cartridge offset without distributing any retail game data.
This distinguishes a correct base-plus-relative-offset translation from a
test that happens to pass at payload offset zero.
