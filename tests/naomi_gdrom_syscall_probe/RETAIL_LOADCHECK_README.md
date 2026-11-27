# VF3 NAOMI retail chainload test

This package contains a small NAOMI base ROM, the complete 411-entry FAD map,
and Windows packing helpers. It does not contain the commercial game payload.

## Build the complete image on Windows

1. Extract the compact-map-base ZIP from the GitHub Actions artifact.
2. Open PowerShell in the extracted package folder.
3. Pass either your local original `.7z` archive or the folder containing its
   extracted files:

   ```powershell
   Set-ExecutionPolicy -Scope Process Bypass
   # Option A: point directly to the local 7z archive (requires 7-Zip)
   .\build_vf3_netdimm_image.ps1 -AssetsPath "D:\VF3\dc-vf3tb-usa.7z"

   # Option B: use an already-extracted local assets folder
   .\build_vf3_netdimm_image.ps1 -AssetsPath "D:\VF3\ExtractedAssets"
   ```

The script uses only files already on your PC. For a 7z archive it extracts
locally using an installed 7-Zip; for a folder it searches recursively. It
verifies every asset against the checked-in manifest, packs all 411 included
files (excluding only `MOVIE_A.SFD`), derives the payload offset from the base
ROM's actual sector-aligned end, and appends the payload. `MOVIE_B.SFD` remains
included.

Python 3 is required. The completed image is written to the current PowerShell
folder as `vf3-naomi-chainload-vNN.bin` (unless `-OutputPath` is supplied).
The complete image is approximately 306.76 MiB, within the 512 MiB NetDIMM
capacity. Temporary extraction and payload files are removed automatically.

## Flycast test

Load the resulting combined image as a NAOMI NetDIMM image. The diagnostic
screen should first show the existing DMA/PIO checks passing, followed by:

- `RETAIL 1ST_READ LOAD: PASS`
- `1ST_READ SHA256: MATCH`

After displaying the successful result for approximately three seconds, the
resident high-RAM chainloader:

1. Installs and verifies a BSD-licensed replacement Dreamcast IP.BIN template
   at `0x8C008000`, including the reserved boot-area range expected by retail
   startup code.
2. Invalidates cached lines covering the destination.
3. Copies the verified 1,179,648-byte retail executable from staging RAM to
   its Dreamcast load address, `0x8C010000`.
4. Invalidates the SH-4 instruction cache for the copied code.
5. Sets the Dreamcast startup CPU context and calls the retail entry point,
   matching the established replacement-IP bootstrap handoff convention.

The resident GD-ROM bridge remains in `0x8D000000–0x8D030000` so subsequent
retail GD-ROM requests can still be serviced. This is the first actual
retail boot attempt, not proof that all later GDFS, audio, or game-runtime
operations are compatible.

No serial connection is required. No commercial game assets are committed to
the repository or included in GitHub Actions artifacts.

The embedded replacement IP template is from Dreamcast-Projects/makeip and is distributed under the BSD 2-Clause License. See `THIRD_PARTY_NOTICES.txt` in the package.
