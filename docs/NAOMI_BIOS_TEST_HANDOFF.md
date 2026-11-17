# Naomi System Test Menu hand-off

## Current implementation

The probe now uses KallistiOS's normal application exit path before entering the Naomi System Test Menu.

The flow is:

1. The running probe detects a rising edge on the physical JVS system TEST switch, or the title-test program selects EXIT.
2. The application calls `arch_set_exit_path(ARCH_EXIT_MENU)`, then `exit(0)`.
3. KOS runs its normal exit handler and `arch_shutdown()`.
4. KOS tears down the runtime, including audio, Maple/MIE, hardware services, timers, PVR, threads and interrupts.
5. The Naomi-specific `arch_menu()` destination reads the Naomi BIOS System Test Mode vector at `0xAC018000 + 0x11 * 4`, converts the cached function address to its uncached P2 alias with `| 0xA0000000`, and calls it.

This replaces the failed direct call from inside a live KOS application.

## Naomi 1 vector reference: DragonMinded/libnaomi

The independent Naomi homebrew project [libnaomi](https://github.com/DragonMinded/libnaomi) provides the relevant Naomi 1 implementation in [libnaomi/system.c](https://github.com/DragonMinded/libnaomi/blob/main/libnaomi/system.c).

Its constants identify:

- System-call vector base: `0xAC018000`
- Enter-test-mode vector index: `0x11`
- Byte offset: `0x11 * 4 = 0x44`
- Uncached mirror: `0xA0000000`

libnaomi's `enter_test_mode()` does not call the vector immediately. It calls `call_unmanaged()`, which records the BIOS call and `longjmp`s to the top-level runtime entry. The runtime then runs `_shutdown()` before invoking the BIOS function.

The libnaomi shutdown sequence runs global destructors, frees audio and video, and tears down ROMFS, POSIX, IRQ, Maple, audio, thread and timer subsystems.

## KOS shutdown evidence

KallistiOS already provides the equivalent lifecycle:

- `exit(0)` enters Newlib's `_exit()`, which calls KOS's registered `kos_shutdown()` unless `INIT_NO_SHUTDOWN` is set.
- `arch_exit_handler()` calls `arch_shutdown()` before selecting the configured exit destination.
- `arch_shutdown()` runs destructors and `arch_auto_shutdown()`, then shuts down IRQs.
- `arch_auto_shutdown()` shuts down sound and hardware, disables interrupts, shuts down timers and PVR, and finally shuts down threads and remaining runtime services.
- On Naomi, `hardware_shutdown()` calls `maple_shutdown()`. The Maple shutdown calls `mie_shutdown()`; the MIE shutdown removes callbacks and unregisters the MIE driver.

Sources:
- [KOS newlib_exit.c](https://github.com/KallistiOS/KallistiOS/blob/master/kernel/libc/newlib/newlib_exit.c)
- [KOS kernel init.c](https://github.com/KallistiOS/KallistiOS/blob/master/kernel/arch/dreamcast/kernel/init.c)
- [KOS hardware.c](https://github.com/KallistiOS/KallistiOS/blob/master/kernel/arch/dreamcast/hardware/hardware.c)
- [KOS Maple shutdown](https://github.com/KallistiOS/KallistiOS/blob/master/kernel/arch/dreamcast/hardware/maple/maple_init_shutdown.c)
- [KOS MIE shutdown](https://github.com/KallistiOS/KallistiOS/blob/master/kernel/arch/dreamcast/hardware/maple/mie/mie.c)
- [libnaomi system.c](https://github.com/DragonMinded/libnaomi/blob/main/libnaomi/system.c)

## KOS Naomi-specific destination patch

The probe build workflow checks out KallistiOS and patches its `arch_menu()` implementation:

- Naomi builds call the Naomi BIOS vector described above.
- Non-Naomi builds retain the original `syscall_system_bios_menu()` behavior.

The application itself uses the public KOS `arch_set_exit_path(ARCH_EXIT_MENU)` and standard `exit(0)` APIs. The workflow patch is idempotent so a restored KOS build cache does not apply it twice.

The old VF4-derived helper-table copy at `0xAC018000` has been removed from the VF3 probe. It was not part of the libnaomi Naomi 1 call path and must not be used here.

## Naomi 2 VF4 evidence

The recovered Virtua Fighter 4 Final Tuned Ver. B source remains useful for title-side TEST/EXIT behavior, but VF4 is a Naomi 2 title. It is not the authority for the Naomi 1 BIOS vector or for KOS shutdown behavior.

References:
- [VF4FT game-side TEST handler](https://github.com/djanice1980/VF4-Final-Tuned-Rev-B-Decomp-Source/blob/5646898aca5eedd45ae7495ad52bc7bd479604d4/src/vf4lib/libbase/src/system.c)
- [VF4FT test-program EXIT handler](https://github.com/djanice1980/VF4-Final-Tuned-Rev-B-Decomp-Source/blob/5646898aca5eedd45ae7495ad52bc7bd479604d4/src_test/testmode/testmode.c)
- [VF4FT helper implementation](https://github.com/djanice1980/VF4-Final-Tuned-Rev-B-Decomp-Source/blob/5646898aca5eedd45ae7495ad52bc7bd479604d4/src_test/sdk/libam/nlasrvdt.c)

## Validation status — confirmed

The KOS shutdown-based hand-off is now **confirmed working in both Flycast and on real Naomi 1 hardware**.

- GitHub Actions build: [run 44175121844](https://github.com/cdmn1/vf3-naomi/actions/runs/44175121844).
- Source commit: `8be15a81d418fc848f3e5eb515527c40fc4bbdf4`.
- Built ROM: `vf3_mie_probe_v0.1.1-dev.57+8be15a8.bin`.
- Both the running probe and separate title-test program were included in the build.

Verified sequence on both environments:
1. Physical cabinet TEST from the running probe enters the Naomi System Test Menu.
2. Selecting GAME TEST MODE starts the separate probe test program.
3. Selecting EXIT returns to the Naomi System Test Menu.

The earlier dev.43 direct-call implementation remains a known-bad experiment: it froze in Flycast on physical TEST and garbled the display/hung Flycast on GAME TEST MODE EXIT. Do not reuse that implementation.

For the reusable integration recipe, including the application-side calls, KOS build patch, source references and porting checklist, see [KOS_NAOMI_SYSTEM_TEST_HANDOFF.md](KOS_NAOMI_SYSTEM_TEST_HANDOFF.md).
