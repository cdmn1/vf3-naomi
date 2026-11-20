#pragma once

#include "../sh4_if.h"

bool vf3TraceEnabled();
void vf3TraceInit();
void vf3TraceInstruction(Sh4Context *ctx);
void vf3TraceMemory(u32 pc, u32 address, u32 width, bool isWrite, u64 value);
void vf3TraceGdromTransfer(u32 destination, u32 size, u32 startFad, u32 sectorSize, u32 remainingSectors);
void vf3TraceGdromSectorRead(const u8 *buffer, u32 startFad, u32 sectorCount, u32 sectorSize, bool stopOnMiss);
void vf3TraceGdromReadPath(const char *path, u32 startFad, u32 sectorCount, u32 sectorSize);
void vf3TraceGdromPioWord(u16 value);
void vf3TraceInstallMemoryHooks();
