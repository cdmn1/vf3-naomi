# VF3 NAOMI retail chainload test

This package contains a small NAOMI base ROM, the complete 411-entry FAD map,
and Windows packing helpers. It does not contain the commercial game payload.

## Build the complete image on Windows

1. Extract the original VF3tb USA asset archive to a folder. The files may be
   in subfolders; the packer searches recursively.
2. Keep the extracted artifact folder together and open PowerShell in it.
3. Run:

   ```powershell
   Set-ExecutionPolicy -Scope Process Bypass
   .\build_vf3_netdimm_image.ps1 -AssetsPath "D:\VF3\ExtractedAssets"
   ```

The script verifies every asset against the checked-in manifest, packs all 411
included files (excluding only `MOVIE_A.SFD`), and appends the payload to the
map-aware base ROM. `MOVIE_B.SFD` remains included.

The output filename includes the build version, for example
`vf3-naomi-chainload-v69.bin`. The complete image is approximately 306.73 MiB,
within the 512 MiB NetDIMM capacity.

## Flycast test

Load the resulting combined image as a NAOMI NetDIMM image. The diagnostic
screen should first show the existing DMA/PIO checks passing, followed by:

- `RETAIL 1ST_READ LOAD: PASS`
- `1ST_READ SHA256: MATCH`

After displaying the successful result for approximately three seconds, the
resident high-RAM chainloader:

1. Invalidates cached lines covering the destination.
2. Copies the verified 1,179,648-byte retail executable from staging RAM to
   its Dreamcast load address, `0x8C010000`.
3. Invalidates the SH-4 instruction cache for the copied code.
4. Sets a reserved high-RAM stack and jumps to the retail entry point.

The resident GD-ROM bridge remains in `0x8D000000–0x8D030000` so subsequent
retail GD-ROM requests can still be serviced. This is the first actual
retail boot attempt, not proof that all later GDFS, audio, or game-runtime
operations are compatible.

No serial connection is required. No commercial game assets are committed to
the repository or included in GitHub Actions artifacts.
