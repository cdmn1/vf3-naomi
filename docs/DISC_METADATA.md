# Dreamcast VF3tb disc metadata

Date: 2026-11-16

The user supplied a 32,768-byte file named `IP0000.BIN`. Its SHA-256 is:

`0d68ad96f978996e1739e701c191ace218eb4d0077e84d61d863210814d47ecd`

Its header is consistent with a Dreamcast IP.BIN boot-sector image. The metadata fields read:

| Field | Value |
|---|---|
| Hardware ID | `SEGA SEGAKATANA` |
| Maker | `SEGA ENTERPRISES` |
| Device info | `A053 GD-ROM1/1` |
| Area | `U` |
| Product number | `MK-51001` |
| Version | `V1.002` |
| Release date field | `19990820` |
| Boot filename | `1ST_READ.BIN` |
| Publisher | `SEGA ENTERPRISES` |
| Disc title | `VF3TB` |

This matches the US Dreamcast VF3tb release and the user's supplied `1ST_READ.BIN` as the expected boot filename. The IP.BIN itself does not directly provide the executable's load address or entry point; those still need to be established from the Dreamcast boot process and executable startup code.

The filename `IP0000.BIN` is not the conventional `IP.BIN` name, but the file contents and exact 32 KiB size are consistent with the boot-sector data. The name alone is not a problem.
