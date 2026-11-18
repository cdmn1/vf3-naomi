# Naomi BIOS GD-ROM asset-provider investigation

## Goal

Provide the VF3 runtime with random-access reads from the disc image loaded in
the Naomi NetDIMM. The NetDIMM is treated as a GD-ROM emulator: the game should
request sectors through the Naomi BIOS GD-ROM service, not assume that the
NetDIMM exposes a host filesystem or a Dreamcast-style streamed file API.

## Source findings

### VF3's original request path

The related Dreamcast VF3 source documents its GD-ROM BIOS service at
`0x8C0000BC`. Its GDFS loader queues BIOS command `0x11` (`DMAREAD`) with
the start FAD, sector count and destination address, then advances the command
through `EXEC_SERVER` and polls `GET_CMD_STAT`. The reference HLE models
the transfer as 2048-byte user sectors and subtracts 150 from FAD when mapping
the request to the GDI's LBA-based track files.

This is the request shape the Naomi asset provider needs to preserve:
resource metadata resolves a file to a start FAD and length, and the provider
reads the requested sector range into Naomi RAM.

### KallistiOS interfaces

KOS exposes the BIOS GD-ROM syscall family in `dc/syscalls.h`:
`syscall_gdrom_send_command()`, `syscall_gdrom_exec_server()` and
`syscall_gdrom_check_command()`. The command enum includes
`CD_CMD_DMAREAD` (0x11), and `cd_read_params_t` carries the start sector,
sector count, destination buffer and test flag.

KOS's higher-level `dc/cdrom.h` API can query the high-density TOC through
`cdrom_read_toc(..., true)`. However, its documentation explicitly says
that the ordinary `cdrom_read_sectors()` path is not for reading the
high-density GD-ROM area. Therefore the asset provider should not be built
around ordinary file APIs or assume that the standard low-density sector
helper is sufficient. The new probe uses the BIOS command interface directly.

## Probe

`tests/naomi_gdrom_probe/` performs these steps:

1. Initializes the KOS GD-ROM service.
2. Reads drive status and the high-density TOC.
3. Issues one BIOS `CD_CMD_DMAREAD` for the first sector of
   `MTJACLAU.BIN`.
4. Displays the command result and the first eight bytes read.

The sector location comes from `docs/GDFS_ASSET_INDEX.csv`: the asset's
manifest LBA is 475142, and the GDI/BIOS address conversion documented by the
reference implementation gives FAD 475292 (LBA + 150). No game asset is
embedded in the probe.

A successful build only proves that the probe compiled and packaged. A
successful runtime read must still be confirmed in Flycast with the VF3tb
GD-ROM image mounted, and then on the real Naomi/NetDIMM.

## Provider boundary after the probe

The intended C interface is a small sector reader, conceptually:

```c
int vf3_naomi_read_fad(uint32_t fad, uint32_t sector_count,
                       void *destination);
```

The GDFS/resource layer remains responsible for resolving a resource name
and its start FAD/length. The Naomi provider is responsible only for issuing
the BIOS read, handling alignment/cache/DMA completion, and reporting errors.
This keeps VF3's asset semantics separate from the storage transport.

Before large-scale asset conversion or caching, validate:
- a sector read at a known FAD;
- a multi-sector read crossing a sector boundary;
- a complete asset read whose SHA-1 matches the local manifest;
- repeated reads and cache reuse without re-reading the NetDIMM image.

Commercial asset bytes must remain local. Any persistent cache derived from
them must be encrypted and content-addressed; do not upload plaintext assets
to public GitHub artifacts or Actions caches.

## References

- `celsowm/vf3-decomp/src/sys/gdrom_hle.h` — BIOS vector and command model.
- `celsowm/vf3-decomp/src/sys/gdrom_hle.c` — FAD-to-LBA and sector transfer.
- KallistiOS `kernel/arch/dreamcast/include/dc/syscalls.h` — GD-ROM BIOS syscall API.
- KallistiOS `kernel/arch/dreamcast/include/dc/cdrom.h` — sector and TOC API caveats.
