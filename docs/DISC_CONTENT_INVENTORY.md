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

## Interpretation

This is a substantial game-data set, not merely a menu/intro package. The presence of character motion match-up banks, character model/texture assets, stage geometry, collision data, and sound banks gives us a clear target for identifying the runtime's asset-loading interfaces.

The two very large `.SFD` files and the movie directory corroborate the user's observation that the Dreamcast release includes a substantial video component. This alone does not prove that the fighting engine is an arcade binary wrapped by a console front end; that requires executable and runtime analysis.

The sound driver path explicitly points to an AM2/SEGA library asset named `AICADRV.BIN`, a useful lead for the AICA initialization/runtime investigation. It does not establish the exact driver API or whether the driver is called directly by game code.

## Important size discrepancy

The Dropbox folder's current file metadata does not always match the sizes recorded in `DATELIST.ASC`. Examples:
- `MOVIE.BIN`: inventory 327,680 bytes; current Dropbox file metadata 3,932,160 bytes.
- `SNDDRV.BIN`: inventory 34,256 bytes; current Dropbox file metadata 53,120 bytes.
- `ROB_AKI.POL`: inventory 132,848 bytes; current Dropbox file metadata 132,416 bytes.

The Dropbox folder is therefore not yet proven to be a byte-for-byte faithful extraction matching this inventory. This may reflect extraction/processing differences or a changed copy, but the cause is unknown. Before relying on individual asset bytes, compare their actual hashes and formats against a known-good disc extraction.

The Dropbox copy of `1ST_READ.BIN` is 1,179,648 bytes, matching the size of the executable uploaded directly in chat.

## Next analysis priorities

1. Reconcile the Dropbox asset set against the DATELIST byte sizes and establish whether the files are raw, padded, transformed, or otherwise modified.
2. Analyze the small driver and motion-header files locally if byte-level access is available; avoid downloading the two large FMVs unless a specific video-format question requires them.
3. Use asset filenames and path mappings to search the executable for file-open/load references and determine whether it uses a filesystem abstraction or hard-coded disc paths.
4. Identify the SFD movie playback path separately from the main game loop.
5. Continue SH-4 startup and hardware dependency analysis using the confirmed US executable and boot-sector metadata.

Commercial game binaries and assets remain outside this Git repository; only analysis notes are committed.
