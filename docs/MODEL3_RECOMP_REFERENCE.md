# Model 3 VF3 recompilation: relevance to the Naomi port

Reviewed: 2026-11-16

Reference: [mr-finster/vf3-recomp](https://github.com/mr-finster/vf3-recomp), a public native recompilation of Virtua Fighter 3 for the Sega Model 3 arcade board.

## Finding

This is a valuable engineering reference, but it is **not a Naomi port and its executable/game core is not directly reusable**.

The reference project runs the Model 3 game's PowerPC 603e program. It recompiles that program to x86-64/ARM64 and implements the surrounding Model 3 board: memory map, interrupts, 53C810 SCSI DMA, JTAG, EEPROM, inputs, tile generator, Real3D Pro-1000 graphics and the Model 3 sound board. Its documented target is the Model 3 VF3 Revision D ROM set.

Our current Naomi investigation is based on the Dreamcast-derived VF3tb SH-4 executable and Naomi hardware. The CPU architecture, executable, memory map, graphics/audio integration, I/O and storage path differ. Similar game title and shared franchise do not establish binary or source compatibility.

## What is useful

1. **Measurement before implementation.** The project instrumented the reference emulator across attract mode, all twelve fighters, versus play and the test menu. It measured executed instructions, indirect branch destinations, writes to executable RAM, interrupt behavior and code coverage before deciding how to recompile.
2. **A reference interpreter.** It implemented a PowerPC interpreter and compared CPU registers and RAM against the reference after every frame across scripted runs.
3. **A narrow native-code layer.** Its recompiler uses an address map derived from coverage, keeps an interpreter fallback, and validates interpreted and native execution in lockstep.
4. **Board behavior isolated from game code.** The project separates CPU execution, board devices, graphics, sound and the host application. This is a useful architecture for planning our own Naomi bring-up and debugging boundaries.
5. **Evidence-driven peripheral work.** Its renderer and sound implementation are checked against reference output rather than being accepted because they merely boot.

These are methods and test-design ideas we can adopt. The reference's specific register behavior, instruction handlers, device models, address map, rendering code and sound code describe Model 3 and must not be copied into the Naomi implementation as if they described Naomi.

## What is not directly reusable

| Reference component | Why it does not transfer directly |
|---|---|
| PowerPC 603e interpreter and recompiler map | Naomi game executable is SH-4, not PowerPC |
| Model 3 memory map, interrupts and SCSI DMA | Naomi has a different board configuration and storage path |
| Real3D Pro-1000 and tile-generator implementation | Naomi uses PowerVR2-based graphics hardware |
| Model 3 68000/SCSP sound-board integration | Naomi audio is based on AICA and a different system interface |
| Model 3 ROM set and asset layout | It is not the Dreamcast/Naomi VF3tb executable or its GD-ROM resource set |

The reference repository's own licence is MIT for its project-written code, but explicitly excludes the game itself and separately licensed third-party components. That licence does not make Sega game code or assets redistributable. Any future reuse would require checking each file's provenance and licence; this investigation does not copy code from that repository.

## Could the complete Model 3 emulator be ported to Naomi?

In principle, yes: one could run the Model 3 PowerPC program under an interpreter or a new SH-4 code generator, then implement the Model 3 devices and graphics/audio behavior on Naomi. But the reviewed implementation does not provide the key pieces needed to make that practical.

### CPU execution

The reference recompiler uses SLJIT to emit **x86-64 or ARM64** code. Its bundled [`sljitConfigCPU.h`](https://github.com/mr-finster/vf3-recomp/blob/main/third_party/sljit/sljitConfigCPU.h) has architecture selections for x86, ARM, PPC, MIPS, RISC-V, S390X, LoongArch and Alpha; there is no SH-4 backend. A Naomi port would therefore need either:
- a new SH-4 SLJIT backend and the associated ABI/register/branch handling; or
- a hand-written PowerPC-to-SH-4 translator; or
- the PowerPC interpreter only.

The project's own Step 3 measurements are on an x86-64 host: 89 frames/s for the interpreter and 456–478 frames/s for its optimized recompiler in the measured scripted run. Those numbers do not predict SH-4 performance. They do show that the optimized native path is a major part of its performance; there is no evidence that the interpreter alone would meet Naomi's frame budget.

### Graphics

The reference's CPU renderer is a software rasterizer, while its GPU renderer is implemented through SDL_GPU and shaders for desktop APIs. Neither is a Naomi PowerVR2 backend. A port would need either to make the software rasterizer fast enough on SH-4, or to translate Model 3 Real3D scene data and rendering state into PowerVR2 polygon/texture submissions. The latter is a substantial renderer project, not a build-system change.

### Sound and board devices

The reference runs the Model 3 sound board's 68000 and two SCSPs using MAME-derived code. Naomi's AICA path is different. Likewise, the Model 3 SCSI controller/DMA, board memory map, IRQs, JTAG and EEPROM do not describe Naomi's platform. Each device would need an implementation or compatibility layer, and the Model 3's timing assumptions would need validation.

### Practical assessment

| Route | Main work | Assessment |
|---|---|---|
| Reuse the Model 3 native recompiler unchanged | Run x86-64/ARM64 code on SH-4 | Not possible as-is |
| Use its PowerPC interpreter on Naomi | Port interpreter and Model 3 board; measure SH-4 speed | Technically possible, performance unproven and likely the first major risk |
| Add an SH-4 JIT backend | New SLJIT backend plus Model 3 board emulation | Large CPU-engineering effort before graphics/audio |
| Rebuild the game around Naomi hardware | PPC execution, Real3D-to-PVR2 renderer, SCSP-to-AICA sound, board I/O/storage adaptation | Effectively a new Model 3 emulator port |

This does not make a Model 3-to-Naomi port impossible in theory. It means the existing repository is not a shortcut to a usable Naomi game image. For the current goal, the Dreamcast SH-4 executable remains the much lower-risk starting point because its CPU family and major graphics/audio chips align with Naomi, while its remaining platform differences can be isolated and measured.

## Recommendation for this project

Keep the Dreamcast-derived SH-4 executable as the primary software lineage for the Naomi port. Continue using the Model 3 recompilation as a behavioral and engineering reference only.

The most transferable improvement is to apply its measurement discipline to our current target:

- record actual execution and device interactions before implementing a subsystem;
- keep probes small and isolate boot, graphics, input, audio and storage;
- compare against a known-good reference at deterministic frame or event boundaries;
- distinguish a successful boot from verified gameplay behavior;
- retain clear evidence for every hardware assumption.

For our immediate work, this reinforces the current order: establish Naomi boot and ROMFS asset access, prove PowerVR2 and AICA independently, then use the pending SH-4 loader trace to identify the game's real resource and runtime dependencies. Do not start a Model 3 PowerPC recompiler or attempt to transplant its renderer.

## Sources reviewed

- [Project README](https://github.com/mr-finster/vf3-recomp/blob/main/README.md)
- [How it was made](https://github.com/mr-finster/vf3-recomp/blob/main/docs/HOW-IT-WAS-MADE.md)
- [Step 1: execution measurement](https://github.com/mr-finster/vf3-recomp/blob/main/docs/step1-measure.md)
- [Step 2: interpreter and board core](https://github.com/mr-finster/vf3-recomp/blob/main/docs/step2-core.md)
- [Step 3: PowerPC recompiler](https://github.com/mr-finster/vf3-recomp/blob/main/docs/step3-recompiler.md)
- [SLJIT CPU architecture configuration](https://github.com/mr-finster/vf3-recomp/blob/main/third_party/sljit/sljitConfigCPU.h)
- [PowerPC recompiler implementation](https://github.com/mr-finster/vf3-recomp/blob/main/src/rec/ppc_rec.cpp)
- [Step 4: graphics, sound and host integration](https://github.com/mr-finster/vf3-recomp/blob/main/docs/step4-game.md)
- [Project licence](https://github.com/mr-finster/vf3-recomp/blob/main/LICENSE)
- [This project's porting plan](PORTING_PLAN.md)
