# Naomi system and game test-menu flow

Date: 2026-11-17

## User-provided reference

The repository folder `test menu flow/` contains four screenshots of the Naomi system/test-menu flow, captured in Flycast. These are the target reference for both the VF3 and Daytona Naomi projects.

The screenshots show this sequence:

1. The Naomi **SYSTEM MENU** is entered using the cabinet service/test controls.
2. The system menu offers platform-level items:
   - RAM TEST
   - JVS TEST
   - SOUND TEST
   - C.R.T. TEST
   - SYSTEM ASSIGNMENTS
   - COIN ASSIGNMENTS
   - BOOKKEEPING
   - BACKUP DATA CLEAR
   - CLOCK SETTING
   - ROM BOARD TEST
   - GAME TEST MODE
   - EXIT
3. Selecting **GAME TEST MODE** loads the game's test program (the screenshot says `NOW LOADING TEST (31K)`).
4. The game's **GAME TEST MODE** menu offers:
   - INPUT TEST
   - OUTPUT TEST
   - SOUND TEST
   - GAME ASSIGNMENTS
   - BOOKKEEPING
   - BACKUP DATA CLEAR
   - EXIT
5. Exiting the game test program returns to the Naomi system menu.

The system menu and the game test program are distinct layers. The platform-level tests and assignments belong to Naomi/system services; game-specific tests and assignments belong to the title's game test program. A port must not conflate these layers or replace the whole flow with a standalone in-game settings overlay.

## VF3-specific investigation

The supplied Dreamcast VF3tb `1ST_READ.BIN` contains strings associated with arcade/operator functionality, including service/test-button workflows, sound-code selection and sound test, coin/credit settings, CPU/video memory tests, input/output tests, CRT test, bookkeeping, play-time histograms, 1P/VS statistics and stage select.

The same executable also contains Dreamcast-facing menu/device strings (Maple/controller ports, memory-card operations and GD-ROM/GDFS). This is evidence of arcade-oriented code/data surviving in the Dreamcast executable, but it does **not** yet prove that the original Naomi game-test program or its settings screens remain reachable in this Dreamcast build.

The Dreamcast release's arcade-related strings and the Naomi system screenshots are separate evidence sources. Do not infer that every original arcade option survived, is active, or can be reused unchanged. Trace references and control flow before deciding which settings can be preserved.

## Implementation requirements for the Naomi ports

- Reproduce the Naomi **system-menu -> game-test-mode -> return-to-system-menu** lifecycle for both VF3 and Daytona.
- Keep platform-level menu/services separate from each game's own test program.
- Implement the game test program as a real reachable program/state with working navigation and exit handoff, not just a static screen or an inaccessible debug overlay.
- For VF3, inspect the Dreamcast executable for surviving game-test and game-assignment routines, settings data, persistence, and platform assumptions. Reuse only what can be demonstrated to be valid on Naomi; otherwise implement the missing title-side functionality.
- The VF3 **Game Assignments** area should expose cabinet button mapping if the original title's assignment semantics can be recovered. The four logical actions (Punch, Kick, Guard, Escape) must be assignable to the available JVS buttons rather than relying on a fixed physical layout. Persist assignments and make gameplay consume them.
- Investigate which other VF3 game settings survived in the Dreamcast binary and expose supported ones through the title's game-test program where appropriate. Do not invent options or claim parity without evidence.
- Keep Naomi-wide settings such as system assignments and coin assignments at the platform layer unless evidence shows a title-specific setting is required.
- Treat input/output/sound tests and bookkeeping/backup-clear entries as functional test items, not merely labels. Do not implement destructive backup clearing without an explicit confirmation step.
- Use the same overall menu lifecycle for Daytona, but keep title-specific test items and settings independent.

## Validation gates

A test-menu implementation is not complete merely because a menu renders. Verify:

1. Entry from the Naomi system menu into the correct title's game-test program.
2. Service-button navigation and test-button selection match the reference flow.
3. Each listed item opens a working test/settings page or is explicitly marked unsupported until implemented.
4. Exit returns to the Naomi system menu without resetting or hanging the game/system.
5. Game assignments persist across a reboot and affect actual gameplay input.
6. Tests exercise the real Naomi JVS input path, not just a host-side mock or diagnostic printout.
7. VF3 and Daytona both pass the lifecycle checks independently.

## Current status

- The four reference screenshots are present in `test menu flow/` and have been inspected.
- The standalone VF3 JVS/MIE probe proves the physical cabinet input path on the user's Naomi, but it is not the full VF3 game.
- Commercial VF4 Final Tuned game and test-program code identifies the retail Naomi BIOS helper entry 0x44 as the hand-off to the Naomi System Test Menu. Both the running-game TEST handler and the separate test-program EXIT handler call it.
- A shared helper implementation is now in platform/naomi/naomi_bios.c. The probe's game executable calls it on a physical TEST edge, and its separate test executable calls it on EXIT.
- The new helper path has compiled successfully in GitHub Actions and produced probe ROM dev.43. Flycast and real-Naomi hand-off validation are still pending. No working VF3 game-test menu is yet considered validated.
- No successful Daytona game-test menu implementation is assumed. The Daytona port must be evaluated against the same lifecycle and validation gates.
- The VF3 porting work must still establish which surviving Dreamcast arcade test routines are callable and how they depend on Naomi-specific services versus Dreamcast-specific devices. The BIOS test-menu hand-off itself is no longer an unknown.
