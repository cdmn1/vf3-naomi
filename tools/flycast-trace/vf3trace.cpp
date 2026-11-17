#include "types.h"

#include "../sh4_mem.h"
#include "vf3trace.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
struct MemoryRange
{
    u32 start;
    u32 length;
};

FILE *traceFile = nullptr;
bool initialized = false;
std::vector<u32> pcWatches;
std::vector<u64> pcWatchHits;
std::vector<MemoryRange> memoryWatches;
std::map<u32, u64> executedPcPages;
u64 instructionCount = 0;
std::string diagnosticsPath;
std::string initializationStatus = "not initialized";

u32 normalizeAddress(u32 address)
{
    return address & 0x1fffffff;
}

bool watchedPc(u32 pc)
{
    const u32 normalized = normalizeAddress(pc);
    for (u32 watched : pcWatches)
        if (normalizeAddress(watched) == normalized)
            return true;
    return false;
}

bool watchedMemory(u32 address)
{
    const u64 normalized = normalizeAddress(address);
    for (const MemoryRange &range : memoryWatches)
    {
        const u64 start = normalizeAddress(range.start);
        if (normalized >= start && normalized < start + range.length)
            return true;
    }
    return false;
}

void writeWord(u64 word)
{
    if (traceFile != nullptr)
        fwrite(&word, sizeof(word), 1, traceFile);
}

void writeDiagnostics()
{
    if (diagnosticsPath.empty())
        return;

    std::ofstream output(diagnosticsPath);
    if (!output)
        return;

    output << "VF3 Flycast trace diagnostics\n";
    output << "Initialization: " << initializationStatus << "\n";
    output << "Executed SH-4 instructions: " << instructionCount << "\n";
    output << "PC watchpoints:\n";
    for (size_t i = 0; i < pcWatches.size(); ++i)
        output << "  0x" << std::hex << pcWatches[i] << std::dec
               << " hits=" << pcWatchHits[i] << "\n";

    output << "Executed PC pages (4 KiB, normalized addresses):\n";
    for (const auto &entry : executedPcPages)
        output << "  0x" << std::hex << entry.first << std::dec
               << " instructions=" << entry.second << "\n";
}

struct DiagnosticsAtExit
{
    ~DiagnosticsAtExit() { writeDiagnostics(); }
};

ReadMem8Func baseRead8 = nullptr;
ReadMem16Func baseRead16 = nullptr;
ReadMem32Func baseRead32 = nullptr;
ReadMem64Func baseRead64 = nullptr;
WriteMem8Func baseWrite8 = nullptr;
WriteMem16Func baseWrite16 = nullptr;
WriteMem32Func baseWrite32 = nullptr;
WriteMem64Func baseWrite64 = nullptr;

u8 DYNACALL traceRead8(u32 address)
{
    const u8 value = baseRead8(address);
    vf3TraceMemory(Sh4cntx.pc - 2, address, 1, false, value);
    return value;
}

u16 DYNACALL traceRead16(u32 address)
{
    const u16 value = baseRead16(address);
    vf3TraceMemory(Sh4cntx.pc - 2, address, 2, false, value);
    return value;
}

u32 DYNACALL traceRead32(u32 address)
{
    const u32 value = baseRead32(address);
    vf3TraceMemory(Sh4cntx.pc - 2, address, 4, false, value);
    return value;
}

u64 DYNACALL traceRead64(u32 address)
{
    const u64 value = baseRead64(address);
    vf3TraceMemory(Sh4cntx.pc - 2, address, 8, false, value);
    return value;
}

void DYNACALL traceWrite8(u32 address, u8 value)
{
    baseWrite8(address, value);
    vf3TraceMemory(Sh4cntx.pc - 2, address, 1, true, value);
}

void DYNACALL traceWrite16(u32 address, u16 value)
{
    baseWrite16(address, value);
    vf3TraceMemory(Sh4cntx.pc - 2, address, 2, true, value);
}

void DYNACALL traceWrite32(u32 address, u32 value)
{
    baseWrite32(address, value);
    vf3TraceMemory(Sh4cntx.pc - 2, address, 4, true, value);
}

void DYNACALL traceWrite64(u32 address, u64 value)
{
    baseWrite64(address, value);
    vf3TraceMemory(Sh4cntx.pc - 2, address, 8, true, value);
}
}

bool vf3TraceEnabled()
{
    return traceFile != nullptr;
}

void vf3TraceInit()
{
    if (initialized)
        return;
    initialized = true;

    const char *tracePath = std::getenv("VF3_TRACE");
    if (tracePath == nullptr || tracePath[0] == '\0')
    {
        initializationStatus = "VF3_TRACE is not set";
        return;
    }

    diagnosticsPath = std::string(tracePath) + ".diag.txt";
    traceFile = fopen(tracePath, "wb");
    if (traceFile == nullptr)
    {
        initializationStatus = "could not open binary trace output";
        fprintf(stderr, "[vf3trace] cannot open trace output: %s\n", tracePath);
        return;
    }
    setvbuf(traceFile, nullptr, _IONBF, 0);

    const char *watchPath = std::getenv("VF3_WATCH");
    if (watchPath == nullptr || watchPath[0] == '\0')
    {
        initializationStatus = "VF3_WATCH is not set";
        fprintf(stderr, "[vf3trace] VF3_TRACE is set but VF3_WATCH is empty\n");
        return;
    }

    std::ifstream input(watchPath);
    if (!input)
    {
        initializationStatus = std::string("could not open watch file: ") + watchPath;
        fprintf(stderr, "[vf3trace] cannot open watch file: %s\n", watchPath);
        return;
    }

    std::string line;
    while (std::getline(input, line))
    {
        std::istringstream row(line);
        std::string kind, first, second;
        if (!(row >> kind) || kind[0] == '#')
            continue;
        if (kind == "pc" && row >> first)
        {
            if (pcWatches.size() < 128)
            {
                pcWatches.push_back(static_cast<u32>(std::stoul(first, nullptr, 0)));
                pcWatchHits.push_back(0);
            }
        }
        else if (kind == "mem" && row >> first >> second)
        {
            if (memoryWatches.size() < 128)
            {
                MemoryRange range;
                range.start = static_cast<u32>(std::stoul(first, nullptr, 0));
                range.length = static_cast<u32>(std::stoul(second, nullptr, 0));
                if (range.length != 0)
                    memoryWatches.push_back(range);
            }
        }
    }

    initializationStatus = "watch file loaded";
    fprintf(stderr, "[vf3trace] tracing to %s (%zu PC watches, %zu memory ranges)\n",
            tracePath, pcWatches.size(), memoryWatches.size());
}

void vf3TraceInstruction(Sh4Context *ctx)
{
    if (traceFile == nullptr || ctx == nullptr)
        return;

    const u32 pc = ctx->pc - 2;
    ++instructionCount;
    const u32 normalizedPc = normalizeAddress(pc);
    executedPcPages[normalizedPc & 0xfffff000]++;

    for (size_t i = 0; i < pcWatches.size(); ++i)
        if (normalizeAddress(pcWatches[i]) == normalizedPc)
            ++pcWatchHits[i];

    if (!watchedPc(pc))
        return;

    writeWord((static_cast<u64>(pc) << 16) | 0xFA10);
    writeWord(ctx->r[13]);
    writeWord(ctx->r[14]);
    writeWord(ctx->r[4]);
    writeWord(ctx->r[5]);
    writeWord(ctx->r[6]);
    writeWord(ctx->r[7]);
    writeWord((static_cast<u64>(pc) << 16) | 0xFA11);
}

void vf3TraceMemory(u32 pc, u32 address, u32 width, bool isWrite, u64 value)
{
    if (traceFile == nullptr || !watchedMemory(address))
        return;

    writeWord((static_cast<u64>(pc) << 16) | 0xFA20);
    writeWord((static_cast<u64>(width) << 48) | (isWrite ? (1ull << 47) : 0));
    writeWord(address);
    writeWord(value);
}

void vf3TraceInstallMemoryHooks()
{
    if (!vf3TraceEnabled())
        return;

    if (ReadMem8 != &traceRead8)  { baseRead8 = ReadMem8; ReadMem8 = &traceRead8; }
    if (ReadMem16 != &traceRead16) { baseRead16 = ReadMem16; ReadMem16 = &traceRead16; }
    if (ReadMem32 != &traceRead32) { baseRead32 = ReadMem32; ReadMem32 = &traceRead32; }
    if (ReadMem64 != &traceRead64) { baseRead64 = ReadMem64; ReadMem64 = &traceRead64; }
    if (WriteMem8 != &traceWrite8) { baseWrite8 = WriteMem8; WriteMem8 = &traceWrite8; }
    if (WriteMem16 != &traceWrite16) { baseWrite16 = WriteMem16; WriteMem16 = &traceWrite16; }
    if (WriteMem32 != &traceWrite32) { baseWrite32 = WriteMem32; WriteMem32 = &traceWrite32; }
    if (WriteMem64 != &traceWrite64) { baseWrite64 = WriteMem64; WriteMem64 = &traceWrite64; }
}

DiagnosticsAtExit diagnosticsAtExit;
