# VF3 Naomi retail 1ST_READ load-check image

This artifact contains a small NAOMI base ROM, the complete 411-entry FAD map,
and the pack/append helper scripts. It does not contain the commercial game
payload.

## Build the complete image on Windows

1. Extract the original VF3tb USA asset archive to a folder. The files may be
   in subfolders; the packer searches recursively.
2. Keep the extracted artifact folder together and open PowerShell in it.
3. Run:

   ```powershell
   .\build_vf3_netdimm_image.ps1 -AssetsPath "D:\VF3\ExtractedAssets"
   ```

The script verifies every asset against the checked-in manifest, packs all 411
included files (excluding only `MOVIE_A.SFD`), and appends the payload to the
map-aware base ROM. `MOVIE_B.SFD` remains included.

The resulting image is expected to be about 306.73 MiB, below the 512 MiB
NetDIMM capacity. No commercial assets are uploaded or redistributed by this
build artifact.

## Flycast test

Load the resulting combined image as a NAOMI NetDIMM image. The on-screen
diagnostic should show the existing DMA/PIO checks passing, followed by:

- `RETAIL 1ST_READ LOAD: PASS`
- `1ST_READ SHA256: MATCH`

The test reads all 576 sectors of the actual retail `1ST_READ.BIN` through
the resident GD-ROM bridge into reserved Naomi upper RAM at `0x8D100000`.
It then compares the complete 1,179,648-byte image against the known retail
SHA-256. It does not yet execute the game. A passing result clears the actual
asset-map, full-file transfer and integrity gates before chainloading.
