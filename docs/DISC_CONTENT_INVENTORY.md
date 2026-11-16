# Dreamcast VF3tb disc content inventory

Date: 2026-11-16

## Source

The user granted access to the Dropbox shared folder `Vf3-naomi`. The folder metadata reports approximately 677,386,395 bytes total. The shared folder is accessible through the connected Dropbox account.

The key inventory file, `DATELIST.ASC`, was read directly from Dropbox. It contains a file-by-file listing with short extracted filenames, original source paths, timestamps, and byte sizes. `DATE.ASC` contains:

```
1998/10/29 21:49
```

The inventory therefore appears to describe a development/build or extraction snapshot dated 1998-10-29, not the later 1999 retail pressing date printed in the IP.BIN. These are different kinds of dates and should not be conflated.

## Notable inventory findings

| Extracted file | Original path / role from DATELIST.ASC | Listed size |
|---|---|---:|
| `MOVIE_A.SFD` | `/PUB/DC/FAT_DATA/MOVIE/MOVIE_A.SFD` | 352,061,440 bytes |
| `MOVIE_B.SFD` | `/PUB/DC/FAT_DATA/MOVIE/MOVIE_B.SFD` | 69,023,744 bytes |
| `MOVIE.BIN` | `/PUB/DC/FAT_DATA/MOVIE/MOVIE.BIN` | 327,680 bytes |
| `SNDDRV.BIN` | `AM2/SET5/981007/LIBSET5/LIB.SOUND.981026/LIB/AICADRV.BIN` | 34,256 bytes |
| `MOTHEAD.BIN` | `MOTHEAD/MOTHEAD.BIN` | 168,391 bytes |
| `DATELIST.ASC` | `TMP/DATELIST` | 32,768 bytes |
| `DATE.ASC` | `TMP/DATE` | 17 bytes |

The inventory also lists:
- a large set of character-vs-character motion data files (`MOTION/CHAR_VS`), with names such as `MTAIKJAC.BIN` and corresponding match-up combinations;
- character model polygon and texture data under `MODEL/ROB_IB` and `MODEL/KAO_IB`;
- stage polygon and texture data under `MODEL/STG_IB`;
- stage collision data under `COLI_S`;
- large scroll/canvas assets under `SCROLL/CNVS`;
- sound and voice assets under `SOUND/BIN.981013`, including music, voice, and sound-effect banks.

## Cross-check against the related project's retail manifest

The Dropbox folder contains 412 files. I compared every immediate Dropbox filename and byte size against the 412 rows in [celsowm/vf3-decomp's US GD-ROM high-density manifest](https://github.com/celsowm/vf3-decomp/blob/main/docs/files.md), which identifies track 3 as Mode1/2352 with base LBA 45000. **All 412 filenames are present and all 412 byte sizes match exactly.** This is consistent with both folders representing the same extracted file set. It is not a byte-for-byte verification: the upstream manifest publishes SHA-1 hashes, but the Dropbox raw bytes have not yet been obtained here for hash comparison.

The manifest lists `1ST_READ.BIN` as 1,179,648 bytes with SHA-1 `bd327fc8ffb0411883abeb9b925b4660973f160e`. Our earlier local analysis recorded SHA-256 `37f38dd9a47b785f526157c4a1674ca75322f0de1342f64bbf33c88d128e4b32`; different hash algorithms mean these values cannot be compared directly. A raw-byte SHA-1 calculation is still needed to establish identity.

## Additional root-level executable: RELOAD.BIN

A separate root-level file, `RELOAD.BIN`, is present in the Dropbox folder at `/Vf3-naomi/RELOAD.BIN` (655,360 bytes; modified 2026-11-16). This exactly matches the size and filename/role documented independently by [celsowm/vf3-decomp's executable inventory](https://github.com/celsowm/vf3-decomp/blob/main/docs/executables.md), which describes a 655,360-byte SH-4 re-init/resident module containing Shinobi, Naomi-library and NEC PowerVR identifiers.

`RELOAD.BIN` does **not** appear in the 418-line `DATELIST.ASC` listing. The related project's [file inventory](https://github.com/celsowm/vf3-decomp/blob/main/docs/files.md) independently lists a 655,360-byte `RELOAD.BIN` with SHA-1 `d6b40a9f2bccac3e961cbcaf1867f91c82670cb8`, and its [format inventory](https://github.com/celsowm/vf3-decomp/blob/main/docs/formats/INVENTORY.md) records leading bytes `22 4f dc 7f 48 1f 57 1f 66 1f 75 1f 15 d3 16 d2`. These are useful fingerprints to compare, but we have not verified them against the Dropbox file's bytes. Keep it separate from the retail `1ST_READ.BIN` until its provenance and byte identity are established.

## Interpretation

This is a substantial game-data set, not merely a menu/intro package. The presence of character motion match-up banks, character model/texture assets, stage geometry, collision data, and sound banks gives us a clear target for identifying the runtime's asset-loading interfaces.

The two very large `.SFD` files and the movie directory corroborate the user's observation that the Dreamcast release includes a substantial video component. This alone does not prove that the fighting engine is an arcade binary wrapped by a console front end; that requires executable and runtime analysis.

The sound driver path explicitly points to an AM2/SEGA library asset named `AICADRV.BIN`, a useful lead for the AICA initialization/runtime investigation. It does not establish the exact driver API or whether the driver is called directly by game code.

## Size reconciliation

The inventory and Dropbox metadata were compared across the 412 files in the folder. **321 files have matching sizes; 91 differ.** So the Dropbox folder is not yet proven to be a byte-for-byte faithful copy of the file set represented by `DATELIST.ASC`.

Examples:

| File | DATELIST size | Dropbox size | Difference |
|---|---:|---:|---:|
| `MOVIE_A.SFD` | 352,061,440 | 356,409,344 | +4,347,904 |
| `MOVIE_B.SFD` | 69,023,744 | 69,398,528 | +374,784 |
| `MOVIE.BIN` | 327,680 | 3,932,160 | +3,604,480 |
| `SNDDRV.BIN` | 34,256 | 53,120 | +18,864 |
| `ROB_AKI.POL` | 132,848 | 132,416 | -432 |
| `ST01.CLI` | 201,696 | 201,696 | 0 |

Some differences are small; others are substantial. The pattern is not explained by a single simple padding rule from the metadata alone. Possible causes include conversion/packing, alternate versions, or modified copies, but none is established yet. Before relying on individual asset bytes, inspect actual file contents and compare hashes/formats against a known-good disc extraction.

The Dropbox copy of `1ST_READ.BIN` is 1,179,648 bytes, matching the executable uploaded directly in chat.

## Next analysis priorities

1. Reconcile the Dropbox asset set against the DATELIST byte sizes and establish whether the files are raw, padded, transformed, or otherwise modified.
2. Analyze the small driver and motion-header files locally if byte-level access is available; avoid downloading the two large FMVs unless a specific video-format question requires them.
4. Use asset filenames and path mappings to search the executable for file-open/load references and determine whether it uses a filesystem abstraction or hard-coded disc paths.
5. Identify the SFD movie playback path separately from the main game loop.
6. Continue SH-4 startup and hardware dependency analysis using the confirmed US executable and boot-sector metadata.

Commercial game binaries and assets remain outside this Git repository; only analysis notes are committed.
