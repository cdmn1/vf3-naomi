# Naomi GD-ROM / NetDIMM asset access investigation

## Status

The earlier vf3_naomi_gdrom_probe-v19.bin is withdrawn. It called the Dreamcast BIOS GD-ROM syscall family from a Naomi build without establishing that Naomi BIOS implements that service. It must not be treated as a valid Naomi storage probe, and its results would not validate the Naomi NetDIMM path.

## Keep the two storage interfaces separate

### Dreamcast reference

The related Dreamcast VF3 source models the Dreamcast BIOS service vector at 0x8C0000BC. The GDFS loader queues command 0x11 (DMAREAD), advances the request through EXEC_SERVER, and polls GET_CMD_STAT. This is useful for understanding the original VF3 resource semantics (FAD, sector count, destination address and completion), but it is not evidence that Naomi provides the same BIOS vector or service ABI.

KallistiOS labels dc/syscalls.h as the Dreamcast ROM system-call API. Its Dreamcast hardware makefile also excludes the ordinary Dreamcast cdrom.c and g1ata.c drivers when KOS_SUBARCH=naomi. Compiling a translation unit that references syscall_gdrom_* under the Naomi target only proves those C symbols linked; it does not prove the Naomi BIOS implements the referenced Dreamcast vector.

### Naomi GD-ROM / DIMM model

Naomi GD-ROM is an arcade option-board and DIMM architecture. The disc is loaded into DIMM RAM; the game-side access path must be investigated in terms of the Naomi GD cartridge/NetDIMM interface and Naomi System Bus DMA, not by assuming the Dreamcast BIOS command queue is present.

Flycast's Naomi implementation is a useful inspectable reference:
- core/hw/naomi/naomi.cpp starts GD DMA using the Naomi System Bus registers (SB_GDSTAR, SB_GDLEN, SB_GDEN).
- The DMA scheduler obtains the data pointer from the current Naomi cartridge's GetDmaPtr().
- core/hw/naomi/gdcartridge.cpp models the GD cartridge's data in DIMM memory and resolves DMA offsets into that memory.
- The GD cartridge source describes the GD-ROM/CF IDE path and DIMM board architecture, and models loading sectors from the GD-ROM image into its DIMM data store.

This is strong evidence for the Naomi hardware/emulator architecture. It does not, by itself, prove the exact SH-4 register sequence or API that a Naomi game uses for every file read. That guest-side sequence must be traced from a real Naomi title or the relevant Naomi BIOS/NetDIMM code.

## Correct next steps

1. Inspect Naomi BIOS and GD-ROM/NetDIMM initialization and the game-facing access routines; identify whether VF3 reads through System Bus DMA, cartridge PIO, DIMM commands, or a combination.
2. Trace an existing Naomi GD-ROM title's SH-4 accesses to the GD DMA registers and DIMM board registers, using Flycast instrumentation or disassembly.
3. Compare that trace with the Dreamcast VF3 GDFS read boundary. Preserve VF3's resource/FAD semantics where applicable, but replace the transport layer with the verified Naomi interface.
4. Only then create a Naomi probe. It should read from the already-loaded DIMM image through the actual Naomi interface and verify known bytes against the local asset manifest.
5. Test on Flycast, then on real Naomi/NetDIMM hardware.

## Safety / build policy

Do not use Dreamcast-only BIOS syscall probes as proof of Naomi storage support. Every diagnostic ROM remains uniquely versioned (vN). Keep commercial asset data out of public source, build artifacts and plaintext caches; any derived persistent cache must be encrypted and content-addressed.

## References

- celsowm/vf3-decomp/src/sys/gdrom_hle.h and gdrom_hle.c: Dreamcast BIOS GD-ROM syscall model only.
- KallistiOS kernel/arch/dreamcast/include/dc/syscalls.h: Dreamcast BIOS syscall declarations.
- KallistiOS kernel/arch/dreamcast/hardware/Makefile: excludes cdrom.o and g1ata.o for the Naomi subarchitecture.
- Flycast core/hw/naomi/naomi.cpp: Naomi System Bus GD DMA path.
- Flycast core/hw/naomi/gdcartridge.cpp: Naomi GD cartridge and DIMM memory model.
