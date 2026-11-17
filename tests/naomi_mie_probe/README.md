# Naomi MIE/JVS input probe

A small standalone KallistiOS application to verify Naomi Port A's MIE/JVS
input path before VF3 integration. It does not boot VF3 or modify its assets.

## Build

Use a current KallistiOS toolchain configured for Naomi.

From this directory:

```sh
make clean rom
```

This produces `vf3_mie_probe.bin`, a Naomi ROM image packaged by
KallistiOS's `naomibintool`.

## What it reports

The probe reports Port A mode, MIE enumeration, KOS-mapped player-one buttons,
native JVS player 1/player 2 switch words, system and panel switches, coin
counts, and raw analog channels.

It uses KOS's `INIT_MIE`, `MAPLE_FUNC_MIE`, and `mie_state_t` APIs.

## Naomi System Test Menu hand-off

Both return paths now use KOS's standard shutdown lifecycle:

- Physical JVS system TEST in the running probe calls `exit(0)` with
  `ARCH_EXIT_MENU` selected.
- EXIT in the separate GAME TEST MODE executable does the same.
- The Naomi KOS build's `arch_menu()` is patched to call Naomi 1 BIOS System
  Test Mode vector `0x11` at `0xAC018000 + 0x44`, through the uncached P2
  alias. KOS performs its normal shutdown before reaching that function.

This approach is based on Naomi 1 [libnaomi's system.c](https://github.com/DragonMinded/libnaomi/blob/main/libnaomi/system.c)
and KOS's own shutdown path. See
[NAOMI_BIOS_TEST_HANDOFF.md](../../docs/NAOMI_BIOS_TEST_HANDOFF.md) for the
source analysis and build patch details.

The previous dev.43 implementation called the BIOS helper directly while KOS
was still running. It froze in Flycast and must not be retested.

## Reference

- [KallistiOS MIE driver](https://github.com/KallistiOS/KallistiOS/blob/master/kernel/arch/dreamcast/include/dc/maple/mie.h)
- [KallistiOS MIE example](https://github.com/KallistiOS/KallistiOS/tree/master/examples/dreamcast/mie/basic)
- [DragonMinded libnaomi system.c](https://github.com/DragonMinded/libnaomi/blob/main/libnaomi/system.c)
