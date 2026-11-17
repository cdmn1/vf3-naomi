# Naomi System Test Menu hand-off

## Current status: direct hand-off is unsafe in the KOS probe

Probe build `vf3-naomi-mie-probe-v0.1.1-dev.43+d49eebe` was tested in Flycast:

- Selecting EXIT in the in-app GAME TEST MODE produced a garbled display and Flycast stopped responding.
- Pressing the in-app/physical TEST path froze the application.

Treat both paths as failed. Do not use dev.43 to test BIOS hand-off again.

The previous implementation called BIOS vector entry `0x11` / byte offset `0x44` directly from a running KallistiOS application. That is not a safe hand-off because KOS's runtime, drivers, interrupts, threads, and video/audio services are still active.

## Naomi 1 reference: DragonMinded/libnaomi

The independent Naomi homebrew project [libnaomi](https://github.com/DragonMinded/libnaomi) provides a directly relevant Naomi 1 implementation in [libnaomi/system.c](https://github.com/DragonMinded/libnaomi/blob/main/libnaomi/system.c).

Its constants identify:

- System-call vector base: `0xAC018000`
- Enter-test-mode vector index: `0x11`
- Therefore the vector's byte offset is `0x11 * 4 = 0x44`

This reconciles the notation used by the two sources: libnaomi calls it vector index `0x11`; the VF4 decomp refers to byte offset `0x44`. They identify the same table slot. This is independent Naomi 1 evidence for the vector location, not a reason to assume VF4's Naomi 2 title cleanup is applicable to VF3.

The important part is the call sequence. libnaomi does not invoke the vector from inside the running application. Its `enter_test_mode()` calls `call_unmanaged()`. That records the requested BIOS call and performs a `longjmp` to the runtime's top-level entry point. The runtime then runs `_shutdown()` before invoking the requested BIOS function.

The observed libnaomi shutdown sequence runs global destructors, frees audio and video, then tears down ROMFS, POSIX, IRQ, Maple, audio, thread, and timer subsystems. The vector call happens only after this teardown. This is the key missing piece in the KOS probe.

Sources:
- [libnaomi system.c](https://github.com/DragonMinded/libnaomi/blob/main/libnaomi/system.c) — vector index, `enter_test_mode()`, `call_unmanaged()`, and runtime `_shutdown()`.
- [libnaomi README](https://github.com/DragonMinded/libnaomi) — describes libnaomi as a Naomi-specific bare-metal system library and documents its hardware/runtime coverage.

## What this does and does not establish

Established by the Naomi 1 libnaomi source:
- The Naomi System Test Mode vector is read from `0xAC018000 + 0x11 * 4`.
- The runtime must be shut down before the BIOS call in that implementation.
- libnaomi's shutdown is part of its own runtime and is not a KOS API.

Not established:
- That directly calling the vector from a live KOS application is safe. dev.43 demonstrates the opposite.
- That libnaomi's internal shutdown functions can be called from KOS. They belong to a different runtime and cannot simply be linked into this project.
- That a particular KOS shutdown sequence is sufficient to return to the Naomi System Test Menu.
- That Naomi 2-specific VF4 cleanup is needed or appropriate for VF3 on original Naomi.

## Required next implementation step

Do not call the BIOS vector from either probe executable while KOS is active.

First determine a supported KOS shutdown/exit path for a Naomi application, including how to stop the PVR/TA, audio, Maple/MIE, interrupts, timers, and threads without leaving KOS callbacks or background work running. Then arrange for the BIOS vector call to occur only after that shutdown has completed, analogous to libnaomi's `call_unmanaged()` design.

If KOS has no safe way to resume at a top-level exit trampoline after shutting down its subsystems, a small standalone libnaomi-based Naomi test program may be the better way to validate the BIOS vector independently. That would be a separate test target, not a drop-in replacement for the existing KOS probe.

## VF4 decompilation: useful but Naomi 2 evidence

The recovered Virtua Fighter 4 Final Tuned Ver. B source remains useful for understanding how a commercial title handles its own TEST and test-program EXIT events. However, VF4 is a Naomi 2 title. Its title-specific shutdown sequence must not be copied wholesale into VF3 or treated as proof of Naomi 1 BIOS internals.

References:
- [VF4FT game-side TEST handler](https://github.com/djanice1980/VF4-Final-Tuned-Rev-B-Decomp-Source/blob/5646898aca5eedd45ae7495ad52bc7bd479604d4/src/vf4lib/libbase/src/system.c)
- [VF4FT test-program EXIT handler](https://github.com/djanice1980/VF4-Final-Tuned-Rev-B-Decomp-Source/blob/5646898aca5eedd45ae7495ad52bc7bd479604d4/src_test/testmode/testmode.c)
- [VF4FT BIOS helper implementation](https://github.com/djanice1980/VF4-Final-Tuned-Rev-B-Decomp-Source/blob/5646898aca5eedd45ae7495ad52bc7bd479604d4/src_test/sdk/libam/nlasrvdt.c)

## Validation

No BIOS hand-off is currently validated in Flycast or on real Naomi hardware. A successful compile is not evidence that the hand-off works. The next ROM must not be sent for user testing until the KOS shutdown/trampoline problem has been addressed.
