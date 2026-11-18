# Naomi cartridge / NetDIMM DMA probe

This diagnostic deliberately avoids the Dreamcast BIOS GD-ROM syscall API.

It compares:
1. a short Naomi cartridge PIO read from image offset 0 via 0x5f7000/04/08;
2. a System Bus GD-DMA transfer from the same image offset, using the Naomi DMA
   offset registers (0x5f700c/10) and Holly registers (0x5f7404..0x5f7418);
3. the resulting 16-bit words.

The register model is cross-checked against Flycast's Naomi implementation:
- `naomi_cart.cpp`: cartridge PIO and DMA-offset register behavior;
- `naomi.cpp`: System Bus GD-DMA transfer and cartridge GetDmaPtr path;
- `gdcartridge.cpp`: DIMM-backed data returned by GetDmaPtr.

This is a raw image-offset transport test, not a filesystem/GDFS implementation.
A PASS demonstrates that the same image bytes are visible through Naomi PIO
and System Bus GD-DMA. It does not yet prove VF3's filename/FAD lookup or its
loader's resident-buffer/scatter behavior.

Run this ROM as a NetDIMM-loaded title on Naomi hardware (or in a Naomi emulator
configuration that exposes the same cartridge/DIMM interface). Do not use a
Dreamcast GD-ROM image as a substitute for a Naomi NetDIMM image.

No commercial game assets are included.
