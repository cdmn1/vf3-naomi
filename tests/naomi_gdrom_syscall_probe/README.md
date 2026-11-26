# VF3 Dreamcast GD-ROM syscall bridge probe

This is the first game-facing storage bridge test. It installs a handler at BIOS service vector 0x8C0000BC and exercises the same r6=0, r7=0..13 syscall interface found in both retail 1ST_READ.BIN and RELOAD.BIN.

It deliberately does not use the Dreamcast BIOS implementation. The handler translates both DMAREAD (0x11) and PIOREAD (0x10) requests into Naomi System Bus GD-DMA reads from the NetDIMM cartridge. It supports the request, transfer, status and server calls used by the retail code.

For this probe only, FAD 45150 maps to NetDIMM image byte offset zero. This is a transport/ABI check, not yet the retail GDFS file/FAD map. The production handler will replace this linear mapping with the checked-in asset index and a packed NetDIMM payload map.

The on-screen test compares a direct Naomi cartridge PIO read of 2048 bytes at image offset zero with a DMAREAD request through the BIOS service vector and a PIOREAD request translated internally to the same DMA backend. It also checks transfer and command completion status.

No serial connection is required. No commercial game assets are included.
