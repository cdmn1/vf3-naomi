# VF3tb GDFS resource map

Date: 2026-11-16

## Scope and evidence

This note combines:

- the user's Dropbox folder `Vf3-naomi` (412 files);
- the related project's public US GD-ROM manifest, `celsowm/vf3-decomp/docs/files.md`;
- its extracted GDFS name table, `src/sys/gdfs_table.h`;
- its extraction script, `tools/bin_names.py`, and reverse-engineering notes in `docs/re/architecture.md` and `docs/re/ANALYSIS_STATE.md`.

The source project reports that the retail executable is the verbatim, non-descrambled image loaded at `0x8C010000`. Its post-identity-fix name scan produced 234 table rows (an earlier 222-row result was superseded). We have not independently rerun that scan against the user's raw executable in this working session.

## Dropbox folder inventory

The Dropbox folder contains 412 files:

| Extension | Count | Likely role based on names / existing inventory |
|---|---:|---|
| `.BIN` | 235 | executable, motion, canvas, sound/voice and other packed resources |
| `.POL` | 80 | polygon/model/stage geometry |
| `.TEX` | 80 | texture data |
| `.CLI` | 13 | collision data |
| `.ASC` | 2 | text/inventory metadata |
| `.SFD` | 2 | Sofdec movie files |

The 412 Dropbox filenames and sizes match all 412 rows of the related project's US GD-ROM high-density manifest. That manifest identifies track 3 as Mode1/2352 with base LBA 45000. This establishes filename/size agreement, not byte-for-byte identity.

## Embedded filename-string inventory (not the live pointer table)

The related project's generated `src/sys/gdfs_table.h` is a host-side mirror of 234 candidate filename/tag records extracted from strings embedded in `1ST_READ.BIN`. It is **not** the game's live runtime GDFS pointer table. Comparing those candidate names with the 235 Dropbox `.BIN` files gives:

- 221 exact filename matches;
- 232 matches after removing a leading underscore from the table name;
- 2 table entries that still do not correspond to a Dropbox filename;
- 3 Dropbox `.BIN` files not represented by a normalized table name.

The 13 leading-underscore names are therefore consistent with a prefix/flag being included in the extractor's backward name scan rather than being literal filenames. This is an observed normalization that makes the extracted string list line up with the asset folder; it is not yet a fully decoded record format.

### Two unresolved table entries

| Table entry | Extracted tag text | Why it is unresolved |
|---|---|---|
| `Y15.BIN` | `%s%s %d/%d  ` | Looks like a format string, not a matching asset filename |
| `_00CP_SKY1F.BIN` | `_paROB_BALL.` | Does not normalize to `CP_SKY1F.BIN`; likely the name/tag boundary was parsed incorrectly |

The table also contains two suspicious tag strings that appear to have swallowed the beginning of the following record: `MTWOLWOL.BIN` has tag text `_akROB_AKI.P`, and `GEN_DMY5.BIN` has `a_lDATE.ASC`. Do not treat these tag values as cleanly decoded without checking the raw bytes.

### Three Dropbox BINs outside the normalized table

| File | Interpretation |
|---|---|
| `VF3TBE3.BIN` | Separate E3 demo executable documented by the related project; not a normal retail resource-table entry |
| `RELOAD.BIN` | Separate 655,360-byte SH-4 re-init/resident module; not in the retail GDFS resource list |
| `CP_SKY1F.BIN` | Real Dropbox asset whose corresponding table row appears malformed as `_00CP_SKY1F.BIN` |

This accounts for all 235 `.BIN` files: 232 ordinary resource-name matches plus these three separate/unresolved files.

## Tag families in the generated table

The related project's `src/sys/gdfs.c` groups selected tags for scheduling:

| Tag | Rows | Related project's interpretation |
|---|---:|---|
| `_ak` | 29 | AIK motion |
| `_du` | 11 | dump / BGM staging |
| `_te` | 9 | stage table data |
| `_ka` | 4 | canvas package |
| `_hi` | 4 | high-priority overlay |
| `_sh` | 10 | present in table; not mapped to a dedicated kind in `gdfs.c` |
| `_yu` | 8 | present in table; not mapped to a dedicated kind |
| `_su` | 2 | present in table; not mapped to a dedicated kind |
| `_mo` | 2 | present in table; not mapped to a dedicated kind |
| `_ey` | 2 | present in table; not mapped to a dedicated kind |

These descriptions are the related project's current interpretation, not independently verified semantics for every tag. Most rows have other short tags or an empty tag, and `vf3_gdfs_kind()` classifies those as OTHER.

Examples of the five tags that the current helper recognizes:

- `_ak`: `MTAKIAKI.BIN`, `MTAKIDUR.BIN`, `MTAKIJAC.BIN`, `MTDURDUR.BIN`, `MTRIKJAC.BIN`, `MTTOUPAI.BIN`
- `_du`: `BGM_FUNK.BIN`, `BGM_ITO3.BIN`, `BGM_JACK.BIN`, `BGM_JEFF.BIN`, `VOFSARFX.BIN`, `GEN_DMY2.BIN`
- `_te`: `MTJACWOL.BIN`, `MTJEFJEF.BIN`, `BGM_ITO6.BIN`, `BGM_KAGE.BIN`, `VOFTAKFX.BIN`
- `_ka`: `CP_SKY0F.BIN`, `CP_SKY11.BIN`, `CP_SKY17.BIN`, `MTDURLAU.BIN`
- `_hi`: `VOFJACFX.BIN`, `VOFJEFFX.BIN`, `VOFKAGFX.BIN`, `VOFLAUFX.BIN`

The categories are useful for organizing the investigation, but not sufficient to choose a loader or destination buffer by themselves.

## Separate runtime pointer table and loader state

The related project's later [loader investigation](https://github.com/celsowm/vf3-decomp/blob/main/docs/re/loader_m66.md) identifies a distinct runtime table at `0x8C1069BC..0x8C106BCC`: 132 pointers, with the `MTJACLAU` entry at index 39 (`0x8C106A58`). The note says this table has no static pool/mova references and is reached through a runtime-populated pointer (possibly init relocation or a manager object). It explicitly leaves the runtime pointer chase unresolved. This 132-pointer table must not be conflated with the 234 candidate filename strings in `gdfs_table.h`.

The 8,880-slot table is **inside each MT motion asset**, not another GDFS name table. The related project's `docs/formats/MT.md` describes it as 8,880 32-bit offsets (`0x0000..0x8ABF`), interpreted as 2,960 rows of three offsets for packed channel descriptors, key-time bytes and float tuples. The loader then scatters/re-sites records into resident RAM; the exact runtime pack-loader function remains unresolved. The related project's recommended capture watches the MT resident pack at `0x8C5F0000+`, the 132-pointer runtime table at `0x8C1069BC`, and loader candidates `0x8C02DCEC`, `0x8C04BDA2`, `0x8C08232E`, `_gdFsDaPlaySct` at `0x8C0360C8`, and `bjload_run` at `0x8C0CBC40`.

## What the related reverse engineering establishes about loading

The related project's `docs/re/architecture.md` reports:

- GDFS is the GD-ROM access layer; `_gdFsDaPlaySct` is fingerprint-verified at `0x8C0360C8`.
- Per-asset loader functions reference resource names directly. Examples include:
  - `load_mt_to_u_pai` at `0x8C04BDA2` for `MTTOUPAI.BIN`;
  - `load_mt_aki_jef` at `0x8C08232E`;
  - `load_fx_akfx` at `0x8C05C0FA`;
  - `load_cp_st0f` at `0x8C059E60`.
- The loader is described as positional: the file table is resolved by index at runtime, rather than by direct literal pointers to the name table.
- Approximately 370 name strings were identified by the related project's string-reader analysis.

The related project's `src/sys/gdfs.c` is a small host-side NAME-to-tag lookup helper generated from the embedded filename-string inventory. It is not the runtime 132-pointer table, and it is not a complete implementation of the original GDFS 0.53 library. Likewise, `src/sys/gdrom_hle.c` and `src/sys/bios_syscall.c` model the Dreamcast/REIOS BIOS GD-ROM DMA-read service: the DMA request is advanced in chunks of up to five sectors, with the guest polling GET_CMD_STAT. That is a lower-level disc transport model, not the complete GDFS name/resource loader.

The retail executable's banner identifies GDFS 0.53 (1998/08/28) and NAOMI library 0.8. The related project's analysis notes that the game uses this older/custom GDFS build rather than simply matching a later SDK library. Therefore we should not assume a generic modern GDFS implementation reproduces its exact behavior.

## Consequences for the Naomi port

The most promising boundary is to preserve the game's resource identity, lookup/index semantics, and destination-buffer expectations while replacing the Dreamcast-specific sector transport with a Naomi-side asset provider.

Before implementing that boundary, the port needs to establish:

1. how the runtime-populated pointer reaches the 132-entry table at `0x8C1069BC`, and which caller supplies the index;
2. how that runtime index relates to the 234 embedded filename/tag candidates and the disc's LBA/length metadata;
3. which function reads each MT asset's internal 8,880-slot table and performs the per-record scatter/rewrite into resident RAM;
4. which loader functions request each asset family and what destination address/relocation rules they apply;
5. whether Naomi-side ROMFS/NetDIMM access can supply equivalent file contents and timing, or whether a compatibility shim is needed;
6. which resources are loaded during boot, menu, character select, stage setup and match runtime.

Do not yet hard-code the related project's tag labels as a Naomi loader specification. The table extractor has visible boundary ambiguities, and the runtime's destination-buffer and relocation behavior is more important than the category labels.

## Sources

- [Related project: GDFS helper](https://github.com/celsowm/vf3-decomp/blob/main/src/sys/gdfs.c)
- [Related project: generated GDFS table](https://github.com/celsowm/vf3-decomp/blob/main/src/sys/gdfs_table.h)
- [Related project: table extraction script](https://github.com/celsowm/vf3-decomp/blob/main/tools/bin_names.py)
- [Related project: architecture and loader chain](https://github.com/celsowm/vf3-decomp/blob/main/docs/re/architecture.md)
- [Related project: later runtime loader-table investigation](https://github.com/celsowm/vf3-decomp/blob/main/docs/re/loader_m66.md)
- [Related project: MT asset internal 8,880-slot format](https://github.com/celsowm/vf3-decomp/blob/main/docs/formats/MT.md)
- [Related project: MT resident scatter/rewrite evidence](https://github.com/celsowm/vf3-decomp/blob/main/docs/re/mt_mount_m33.md)
- [Related project: analysis state and GDFS 0.53 identification](https://github.com/celsowm/vf3-decomp/blob/main/docs/re/ANALYSIS_STATE.md)
- [Related project: retail GD-ROM manifest](https://github.com/celsowm/vf3-decomp/blob/main/docs/files.md)
- [Related project: GD-ROM BIOS model](https://github.com/celsowm/vf3-decomp/blob/main/src/sys/gdrom_hle.c)
- [Related project: BIOS syscall model](https://github.com/celsowm/vf3-decomp/blob/main/src/sys/bios_syscall.c)
