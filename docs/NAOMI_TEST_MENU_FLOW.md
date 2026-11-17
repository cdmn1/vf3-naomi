# Naomi system and game test-menu flow

Date: 2026-11-17

## User-provided reference

The repository folder `test menu flow/` contains four screenshots of the Naomi system/test-menu flow, captured in Flycast. These are the target reference for both the VF3 and Daytona Naomi projects.

The screenshots show this sequence:

1. The Naomi **SYSTEM MENU** is entered using the cabinet service/test controls.
2. The system menu offers RAM TEST, JVS TEST, SOUND TEST, C.R.T. TEST, SYSTEM ASSIGNMENTS, COIN ASSIGNMENTS, BOOKKEEPING, BACKUP DATA CLEAR, CLOCK SETTING, ROM BOARD TEST, GAME TEST MODE and EXIT.
3. Selecting **GAME TEST MODE** loads the game's test program.
4. The game's **GAME TEST MODE** menu offers INPUT TEST, OUTPUT TEST, SOUND TEST, GAME ASSIGNMENTS, BOOKKEEPING, BACKUP DATA CLEAR and EXIT.
5. Exiting the game test program returns to the Naomi system menu.

The system menu and game test program are distinct layers. Platform-level tests and assignments belong to Naomi/system services; game-specific tests and assignments belong to the title's test program.

## VF3-specific investigation

The supplied Dreamcast VF3tb `1ST_READ.BIN` contains strings associated with arcade/operator functionality, including service/test-button workflows, sound-code selection and sound test, coin/credit settings, CPU/video memory tests, input/output tests, CRT test, bookkeeping, play-time histograms, 1P/VS statistics and stage select.

The same executable also contains Dreamcast-facing menu/device strings (Maple/controller ports, memory-card operations and GD-ROM/GDFS). This is evidence of arcade-oriented code/data surviving in the Dreamcast executable, but does not prove that the original Naomi game-test program or its settings screens remain reachable in this build.

Do not infer that every original arcade option survived, is active, or can be reused unchanged. Trace references and control flow before deciding which settings can be preserved.

## Implementation requirements

- Reproduce the Naomi system-menu -> game-test-mode -> return-to-system-menu lifecycle for VF3 and Daytona.
- Keep platform-level menu/services separate from each game's own test program.
- Implement a real reachable test program with working navigation and exit handoff.
- For VF3, inspect the Dreamcast executable for surviving game-test and game-assignment routines, settings data, persistence, and platform assumptions.
- Expose cabinet button mapping if the original assignment semantics can be recovered. Punch, Kick, Guard and Escape must be assignable to available JVS buttons, persist, and affect gameplay input.
- Keep Naomi-wide settings at the platform layer unless evidence shows a title-specific setting is required.
- Implement functional test pages rather than labels alone. Destructive backup clearing requires explicit confirmation.
- Keep Daytona title-specific test items and settings independent.

## BIOS hand-off research and implementation

Naomi 1 [libnaomi](https://github.com/DragonMinded/libnaomi/blob/main/libnaomi/system.c) identifies the System Test Mode BIOS vector at `0xAC018000 + (0x11 * 4)`.

The VF3 probe now uses KOS's standard application shutdown path before calling that vector. It sets `ARCH_EXIT_MENU` and calls `exit(0)`; the Naomi KOS build workflow patches KOS's final `arch_menu()` destination to call the Naomi 1 vector. KOS's normal shutdown handles audio, Maple/MIE, hardware, timers, PVR, threads and interrupts first.

This replaces the failed dev.43 direct-call approach. dev.43 froze in Flycast on physical TEST and garbled the display/hung Flycast on GAME TEST MODE EXIT. It must not be retested.

Detailed source analysis: [NAOMI_BIOS_TEST_HANDOFF.md](NAOMI_BIOS_TEST_HANDOFF.md).

## Validation status

- MIE/JVS detection and input reporting have been tested separately.
- dev.43 BIOS hand-off failed in Flycast.
- The KOS shutdown-based hand-off compiled successfully in [GitHub Actions run 44175121844](https://github.com/cdmn1/vf3-naomi/actions/runs/44175121844); artifact dev.57 was produced.
- Flycast and real-hardware return behavior remain untested. No successful return to the Naomi System Test Menu is yet claimed.
- After the new build compiles, test both entry paths in Flycast before real hardware.
- The full VF3 game-test menu, settings persistence, and Daytona lifecycle remain separate unfinished work.
