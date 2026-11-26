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
- **Retail asset-map build:** CI generates a 411-entry table from the packed
  VF3tb USA manifest, excluding only `MOVIE_A.SFD`. The syscall handler then
  translates each requested FAD to an absolute NetDIMM cartridge offset and
  chains Holly DMA chunks across mapped asset boundaries. Missing FAD ranges
  return an error rather than reading an unrelated packed file.

The map-aware build currently receives a temporary aligned payload base for
compile validation. It is not yet a combined ROM with the packed payload and
has not yet been run in Flycast or on a Naomi cabinet.

No serial connection is required. No commercial game assets are committed
to this repository or uploaded as GitHub Actions artifacts.
