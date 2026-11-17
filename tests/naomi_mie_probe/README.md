# Naomi MIE/JVS input probe

A tiny standalone KallistiOS application to verify Naomi Port A's MIE/JVS
input path before VF3 integration. It does not boot VF3 and does not touch
the game executable or assets.

## Build

Use a current KallistiOS toolchain configured for Naomi (KOS 2.1.x or later
is the documented baseline for current Naomi/NetDIMM support).

From this directory:

```sh
make clean rom
```

This produces `vf3_mie_probe.bin`, a Naomi ROM image packaged by KallistiOS's
`naomibintool`.

## What it reports

The probe reports Port A mode, MIE enumeration, KOS-mapped player-one buttons,
native JVS player 1/player 2 switch words, system and panel switches, coin
counts, and raw analog channels.

It uses KOS's `INIT_MIE`, `MAPLE_FUNC_MIE`, and `mie_state_t` APIs.

## Naomi System Test Menu hand-off status

The BIOS hand-off is deliberately **disabled** in the probe after dev.43
froze in Flycast on both the running-game TEST path and the GAME TEST MODE
EXIT path. Do not use dev.43 to test the hand-off.

Research in DragonMinded's Naomi-specific libnaomi project confirms that the
Naomi System Test Mode vector is at `0xAC018000 + 0x11 * 4` (byte offset
`0x44`). More importantly, libnaomi does not call it while its runtime is
active: it unwinds to a top-level exit trampoline, shuts down audio, video,
interrupts, Maple, threads, timers and other services, and only then calls
the BIOS vector.

That shutdown sequence is part of libnaomi's own runtime; it is not a KOS
shutdown API and cannot simply be copied into this KOS application. The probe
therefore reports inputs but no longer attempts the unsafe direct call.

See [NAOMI_BIOS_TEST_HANDOFF.md](../../docs/NAOMI_BIOS_TEST_HANDOFF.md) for
the source analysis and the requirements for a safe KOS implementation.

## Reference

- [KallistiOS MIE driver](https://github.com/KallistiOS/KallistiOS/blob/master/kernel/arch/dreamcast/include/dc/maple/mie.h)
- [KallistiOS MIE example](https://github.com/KallistiOS/KallistiOS/tree/master/examples/dreamcast/mie/basic)
- [DragonMinded libnaomi system.c](https://github.com/DragonMinded/libnaomi/blob/main/libnaomi/system.c)
