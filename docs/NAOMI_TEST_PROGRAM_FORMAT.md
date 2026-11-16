# Naomi title test-program format: initial findings

Date: 2026-11-16

## Confirmed platform behaviour

The Naomi GD-ROM System Service Manual describes a platform-level SYSTEM MENU. Selecting GAME TEST MODE enters the test mode specific to the connected title. The title-side menu normally includes input testing, game assignments and bookkeeping, with options varying by game. Settings are committed when test mode is exited; power-off before exiting can discard changes.

The title test mode is distinct from the Naomi SYSTEM MENU. A game port should provide its title-side test program and must not attempt to replace the platform menu.

## Naomi ROM header support

KallistiOS `utils/naomibintool/naomibintool.c` defines the Naomi ROM header with two separate executable descriptions:

- `segment[8]` and `entry` for the normal game program;
- `test_segment[8]` and `test_entry` for the test program.

The header parser in DragonMinded/libnaomi `libnaomi/cart.c` independently reads both sets:
- main segment table begins at header offset `0x360`;
- test segment table begins at `0x3C0`;
- main entry point is at `0x420`;
- test entry point is at `0x424`.

The libnaomi `executable_t` structure therefore exposes separate main and test section counts, load addresses, lengths, and entry points.

This is direct evidence that the Naomi ROM format has first-class support for a test executable/entry point. It is not necessary to invent an unrelated menu-launch mechanism.

## Existing toolchain examples

DragonMinded/libnaomi's `examples/eepromtest/Makefile` invokes its ROM maker with:
- `--main-binary-includes-test-binary`
- `--test-entrypoint ${TEST_ADDR}`

The corresponding `main.c` defines both `main()` and `test()`. In that example the test function is a stub loop, demonstrating the dual-entrypoint build arrangement, not a complete production menu or return sequence.

KallistiOS `naomibintool` currently initializes the test segment and test entry point to match the main executable in its simple `build` path. Its header reader nevertheless parses and prints distinct test segments and a distinct test entry point. Thus the ROM format supports the distinction even though that simple builder does not expose a separate test-binary option.

## What remains to be verified

The ROM-header fields and toolchain examples establish how a test entry point is represented, but do not by themselves prove the full BIOS handoff/return protocol. Before implementing the game menu, inspect the BIOS-visible launch behaviour and establish:

1. Whether the BIOS loads the test segments into their declared RAM addresses and jumps directly to `test_entry`.
2. What exact operation returns from the test program to the Naomi SYSTEM MENU (BIOS call, reset path, or another documented convention).
3. Whether the title test program is expected to share the main program's runtime/data, or can be an independently linked binary with its own initialization.
4. How the test-mode request is entered and how the title detects it during boot versus normal startup.
5. Which test/service controls are provided through the BIOS-initialized Maple/JVS path, and how their states are represented.
6. Which persistent storage area and APIs are appropriate for title settings, and when writes must be committed.

The user-provided screenshot's `NOW LOADING TEST (31K)` message is consistent with a separately loaded test component, but is not sufficient by itself to prove the loader details above.

## Implementation direction

First produce a minimal Naomi ROM with:
- a normal entry point that displays a simple game/attract placeholder;
- a distinct test entry point (preferably a separately linked test segment for the first experiment);
- a minimal service/test navigable test screen;
- a verified, clean return to the Naomi SYSTEM MENU.

Validate the same image through the intended ROM/netboot path on Flycast and real Naomi hardware. Only after the entry/exit contract is proven should the common title-side menu shell be integrated into VF3 and Daytona. Keep each title's settings and tests separate.

## Sources inspected

- Sega, Naomi GD-ROM System Service Manual, section 5 SYSTEM TEST MODE and section 5-11 GAME TEST MODE.
- KallistiOS: `utils/naomibintool/naomibintool.c`.
- DragonMinded/libnaomi: `libnaomi/cart.c`, `examples/eepromtest/Makefile`, and `examples/eepromtest/main.c`.
- User-provided Naomi menu flow screenshots in `cdmn1/vf3-naomi/test menu flow/`.
