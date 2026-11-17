# KallistiOS on Naomi: return to the System Test Menu

**Status: proven on Flycast and real Naomi 1 hardware**  
**Validated:** 2026-11-17  
**Reference implementation:** [VF3 Naomi probe](https://github.com/cdmn1/vf3-naomi/tree/main/tests/naomi_mie_probe)  
**Validated build:** [dev.57, commit 8be15a81d418fc848f3e5eb515527c40fc4bbdf4](https://github.com/cdmn1/vf3-naomi/actions/runs/44175121844)

This document records a reusable KallistiOS (KOS) integration pattern for Naomi applications that need to return to the Naomi BIOS **System Test Menu**. It was implemented in the VF3 Naomi MIE/JVS probe and confirmed working in both Flycast and on the user's real Naomi hardware.

The same platform-level technique can be reused by Daytona and other KOS-based Naomi projects. It is specifically documented and validated for **original Naomi 1**. Do not assume the vector or behavior is valid for Naomi 2 without separate verification.

## The essential rule

**Do not call the Naomi BIOS System Test Mode vector directly while a KOS application is still running.**

The failed VF3 dev.43 experiment called the BIOS helper from the live application:
- Pressing physical TEST froze the application in Flycast.
- Selecting EXIT from GAME TEST MODE garbled the display and hung Flycast.

The working implementation first lets KOS complete its normal shutdown, then makes the BIOS call from KOS's final menu-exit destination.

## Naomi 1 BIOS vector

The Naomi 1 reference implementation is [DragonMinded/libnaomi, `libnaomi/system.c`](https://github.com/DragonMinded/libnaomi/blob/main/libnaomi/system.c).

| Item | Value |
| --- | --- |
| BIOS vector base | `0xAC018000` |
| System Test Mode vector index | `0x11` |
| Byte offset | `0x11 * 4 = 0x44` |
| Vector entry | `((volatile uint32_t *)0xAC018000)[0x11]` |
| Call address | `entry | 0xA0000000` (uncached P2 alias) |

The vector contains a function address. The reference code reads the vector entry and calls it through the uncached P2 alias.

## Why the shutdown sequence matters

libnaomi does not call the vector from inside its normal managed runtime. Its `enter_test_mode()` requests an unmanaged BIOS call, unwinds to the top-level runtime trampoline, performs `_shutdown()`, and only then calls the BIOS function.

KOS has its own normal exit lifecycle, so a KOS application can use that rather than recreating libnaomi's teardown:

1. Application selects `ARCH_EXIT_MENU`.
2. Application calls the standard C `exit(0)`.
3. Newlib/KOS enters the registered KOS shutdown path (unless the application deliberately disabled it with `INIT_NO_SHUTDOWN`).
4. KOS runs `arch_exit_handler()` and `arch_shutdown()`.
5. KOS tears down runtime services, including audio, hardware, Maple/MIE, timers, PVR, threads and interrupts.
6. KOS reaches `arch_menu()`, which is the final destination for `ARCH_EXIT_MENU`.
7. On Naomi builds only, the patched `arch_menu()` calls the Naomi System Test Mode BIOS vector.

This is the key architectural separation: **the application requests the standard KOS menu exit; the Naomi-specific BIOS hand-off happens only after KOS has shut itself down.**

## Application-side integration

Use the public KOS exit-path API and the standard C exit function at each point where the title should return to the Naomi System Test Menu:

```c
#include <stdlib.h>
#include <arch/arch.h>

/* ... when TEST/EXIT should return to the Naomi System Test Menu ... */
arch_set_exit_path(ARCH_EXIT_MENU);
exit(0);
```

In the VF3 probe this is used in two places:
- Rising edge of the physical JVS system TEST switch in the running application.
- EXIT selection in the separate GAME TEST MODE application.

For a physical TEST switch, detect a **rising edge**, not just a held state. The VF3 probe also samples the initial switch state so a button already held during startup does not trigger an immediate exit.

Do not set `INIT_NO_SHUTDOWN` for this path. The normal KOS shutdown must remain enabled.

## KOS-side integration

Patch KOS's `kernel/arch/dreamcast/kernel/init.c` implementation of `arch_menu()` in the Naomi build. Keep the existing Dreamcast behavior unchanged.

The proven VF3 workflow applies this Naomi-only destination:

```c
void arch_menu(void) {
    dbglog(DBG_CRITICAL, "arch: exiting the system to the BIOS menu\\n");
#if KOS_PLATFORM_IS_NAOMI
    typedef void (*naomi_syscall_t)(void);
    volatile uint32_t *syscall_vector = (volatile uint32_t *)0xAC018000u;
    uint32_t entry = syscall_vector[0x11];

    if(entry == 0 || entry == 0xFFFFFFFFu) {
        dbglog(DBG_CRITICAL, "arch: invalid Naomi System Test Mode vector\\n");
        for(;;) { }
    }

    ((naomi_syscall_t)(uintptr_t)(entry | 0xA0000000u))();
    __builtin_unreachable();
#else
    syscall_system_bios_menu();
#endif
}
```

The exact build-time patch is in [`.github/workflows/naomi-mie-probe.yml`](https://github.com/cdmn1/vf3-naomi/blob/main/.github/workflows/naomi-mie-probe.yml). It is applied to the KOS source used by the Naomi build, not to the game/application source. The workflow checks for the already-patched implementation so a restored KOS cache does not apply the patch twice.

Important:
- Keep the `#if KOS_PLATFORM_IS_NAOMI` guard. Dreamcast builds must retain `syscall_system_bios_menu()`.
- Apply the patch to the KOS revision actually used by the project and fail the build if the expected original `arch_menu()` cannot be found.
- Make the patch idempotent when using cached KOS source/build trees.
- Validate the resulting binary; a successful compile alone does not prove the BIOS hand-off works.

## Validation record

The VF3 dev.57 build (source commit `8be15a81d418fc848f3e5eb515527c40fc4bbdf4`) passed both build and runtime validation.

<badge>Confirmed on Flycast</badge> <badge>Confirmed on real Naomi 1 hardware</badge>

Both return paths were tested:

1. Start the running probe and press the cabinet TEST switch → Naomi System Test Menu appears.
2. Select GAME TEST MODE → the separate title test application starts.
3. Select EXIT in GAME TEST MODE → Naomi System Test Menu appears again.

The user confirmed the complete sequence works correctly on both Flycast and real hardware.

The validated GitHub Actions build is [run 44175121844](https://github.com/cdmn1/vf3-naomi/actions/runs/44175121844). Its source commit is `8be15a81d418fc848f3e5eb515527c40fc4bbdf4`.

## Source references

- [DragonMinded/libnaomi `system.c`](https://github.com/DragonMinded/libnaomi/blob/main/libnaomi/system.c) — Naomi BIOS vector and shutdown-before-unmanaged-call pattern.
- [KOS `newlib_exit.c`](https://github.com/KallistiOS/KallistiOS/blob/master/kernel/libc/newlib/newlib_exit.c) — standard C exit into KOS shutdown.
- [KOS `init.c`](https://github.com/KallistiOS/KallistiOS/blob/master/kernel/arch/dreamcast/kernel/init.c) — exit handler and final `arch_menu()` destination.
- [KOS `hardware.c`](https://github.com/KallistiOS/KallistiOS/blob/master/kernel/arch/dreamcast/hardware/hardware.c) — hardware shutdown.
- [KOS Maple shutdown](https://github.com/KallistiOS/KallistiOS/blob/master/kernel/arch/dreamcast/hardware/maple/maple_init_shutdown.c) and [MIE shutdown](https://github.com/KallistiOS/KallistiOS/blob/master/kernel/arch/dreamcast/hardware/maple/mie/mie.c) — Maple/MIE teardown.
- [VF3 probe application](https://github.com/cdmn1/vf3-naomi/tree/main/tests/naomi_mie_probe) — working application-side example.
- [VF3 probe build workflow](https://github.com/cdmn1/vf3-naomi/blob/main/.github/workflows/naomi-mie-probe.yml) — Naomi KOS patch and build integration.

## Reuse checklist

When bringing this into another KOS-based Naomi project:

- [ ] Confirm the target is Naomi 1, or independently verify the BIOS vector for the target hardware.
- [ ] Identify the KOS revision and the exact `arch_menu()` implementation being built.
- [ ] Add the Naomi-only `arch_menu()` destination while preserving the non-Naomi branch.
- [ ] Ensure the KOS shutdown path is enabled; do not use `INIT_NO_SHUTDOWN`.
- [ ] Call `arch_set_exit_path(ARCH_EXIT_MENU)` followed by `exit(0)` from every relevant TEST/EXIT path.
- [ ] Check physical TEST as an edge if it is polled.
- [ ] Build, then test both entry and return paths in Flycast.
- [ ] Repeat the same tests on real Naomi hardware.

This document records a proven platform hand-off mechanism. It does **not** implement a game's full test menu, input assignments, bookkeeping, persistence, or any title-specific operator functionality.
