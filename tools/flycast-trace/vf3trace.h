#pragma once

#include "../sh4_if.h"
#include <cstdint>

bool vf3TraceEnabled();
void vf3TraceInit();
void vf3TraceInstruction(Sh4Context *ctx);
void vf3TraceMemory(u32 pc, u32 address, u32 width, bool isWrite, u64 value);
void vf3TraceGdromTransfer(u32 destination, u32 size, u32 startFad, u32 sectorSize, u32 remainingSectors);
void vf3TraceGdromSectorRead(const u8 *buffer, u32 startFad, u32 sectorCount, u32 sectorSize, bool stopOnMiss, uintptr_t caller);
void vf3TraceGdromHleRead(u32 guestDestination, u32 startFad, u32 sectorCount, bool virtualAddress, bool directMemory, const u8 *buffer, u32 readResult);
void vf3TraceGdromPioWord(u16 value);
void vf3TraceInstallMemoryHooks();
