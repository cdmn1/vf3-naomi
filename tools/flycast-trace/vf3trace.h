#pragma once

#include "sh4_if.h"

bool vf3TraceEnabled();
void vf3TraceInit();
void vf3TraceInstruction(Sh4Context *ctx);
void vf3TraceMemory(u32 pc, u32 address, u32 width, bool isWrite, u64 value);
void vf3TraceInstallMemoryHooks();
