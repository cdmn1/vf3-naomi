# VF3 Naomi GD-ROM provider probe

This probe exercises the Naomi BIOS GD-ROM service from a KallistiOS Naomi
application. It queries drive status and the high-density TOC, then issues a
single-sector BIOS `CD_CMD_DMAREAD` for the first sector of `MTJACLAU.BIN`.

The test sector address is derived from the checked-in asset index:
GD LBA 475142 + 150 = FAD 475292. The test does not embed or modify the
commercial asset. It reads one sector into an aligned RAM buffer and displays
the first eight bytes.

The important distinction is that KOS documents its ordinary
`cdrom_read_sectors()` interface as low-density-only. VF3's GDFS loader uses
the BIOS GD-ROM service directly (command 0x11 / DMAREAD), so this probe uses
the syscall interface rather than treating the NetDIMM as a mounted host
filesystem.

To test the data read, boot this ROM with the US VF3tb GD-ROM image mounted
in Flycast or available through the Naomi GD-ROM/NetDIMM path. A successful
build is not proof of a successful hardware read; the screen's status and
DMAREAD result are the runtime evidence.

No commercial game data is bundled.
