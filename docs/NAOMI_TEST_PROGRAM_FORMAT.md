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

## Existing toolchain examples and current probe

DragonMinded/libnaomi's `examples/eepromtest/Makefile` invokes its ROM maker with:
- `--main-binary-includes-test-binary`
- `--test-entrypoint ${TEST_ADDR}`

The corresponding `main.c` defines both `main()` and `test()`. In that example the test function is a stub loop, demonstrating the dual-entrypoint build arrangement, not a complete production menu or return sequence.

KallistiOS `naomibintool` currently initializes the test segment and test entry point to match the main executable in its simple `build` path. Its ELF packer explicitly assigns `hdr->test_entry = ehdr->e_entry` and duplicates the main segment descriptor into the test segment table. It does not provide a separate test-ELF option in the version inspected.

That is exactly how the existing VF3 MIE/JVS probe is packaged: `tests/naomi_mie_probe/Makefile` invokes `naomibintool build ... -e vf3_mie_probe.elf`. Therefore its current ROM header has the same main and test entry points and segment descriptors. It is an ideal known-booting hardware/input baseline, but it does not yet demonstrate a distinct test program.

The existing probe already contains useful working pieces for a test-mode prototype:
- Naomi MIE/JVS initialization and live cabinet input polling;
- raw player switch words, system/panel switches, coin counters and analog channels;
- a working PVR text display.

We should extend this probe rather than create a new unrelated test ROM. The initial menu prototype can retain the diagnostic screen as an INPUT TEST page and add a small menu/navigation layer around it.

## What remains to be verified

The ROM-header fields and toolchain examples establish how a test entry point is represented, but do not by themselves prove the full BIOS handoff/return protocol. Before implementing the game menu, inspect the BIOS-visible launch behaviour and establish:

1. Whether the BIOS loads the test segments into their declared RAM addresses and jumps directly to `test_entry`.
2. What exact operation returns from the test program to the Naomi SYSTEM MENU (BIOS call, reset path, or another documented convention).
3. Whether the title test program is expected to share the main program's runtime/data, or can be an independently linked binary with its own initialization.
4. How the test-mode request is entered and how the title detects it during boot versus normal startup.
5. Which test/service controls are provided through the BIOS-initialized Maple/JVS path, and how their states are represented.
6. Which persistent storage area and APIs are appropriate for title settings, and when writes must be committed.

The user-provided screenshot's `NOW LOADING TEST (31K)` message is consistent with a separately loaded test component, but is not sufficient by itself to prove the loader details above.

## Experiment plan using the existing probe

1. Keep the known-good probe binary/source as the baseline and preserve its current input diagnostics.
2. Add a visible distinction between normal entry and test entry, without changing the existing JVS/MIE polling path.
3. Extend ROM packaging so the test entry can be distinct and points at a correctly initialized test-program startup path. Do not simply point the header at an ordinary C function until its stack/runtime requirements are handled.
4. In test mode, display a simple standard-style menu and use the already verified Service/Test and JVS inputs for navigation/selection.
5. Determine and implement the actual BIOS return mechanism; verify the exit returns to the Naomi SYSTEM MENU rather than merely restarting the game.
6. Validate through Flycast and then the real Naomi board.

## Implementation direction

After the entry/exit contract is proven, integrate the reusable title-side menu shell into VF3 and Daytona. Keep each title's settings and tests separate. Do not treat a static overlay or a reboot to the normal game as a successful test-mode implementation.

## Sources inspected

- Sega, Naomi GD-ROM System Service Manual, section 5 SYSTEM TEST MODE and section 5-11 GAME TEST MODE.
- KallistiOS: `utils/naomibintool/naomibintool.c`.
- DragonMinded/libnaomi: `libnaomi/cart.c`, `examples/eepromtest/Makefile`, and `examples/eepromtest/main.c`.
- VF3 Naomi: `tests/naomi_mie_probe/Makefile` and `tests/naomi_mie_probe/main.c`.
- User-provided Naomi menu flow screenshots in `cdmn1/vf3-naomi/test menu flow/`.


## Prototype implementation status (2026-11-16)

A first separate title-test executable has now been added under `tests/naomi_mie_probe/test_main.c`. The build packages it as an independent Naomi test segment/entry point while retaining the existing MIE/JVS probe as the normal application entry.

The packer is `tests/naomi_mie_probe/combine_naomi_test_rom.py`. It:
- builds the existing probe ELF as a normal Naomi ROM;
- builds the test-menu ELF separately with KOS startup;
- appends the test ELF payload at a 0x1000-aligned ROM offset;
- copies its load address and size into the ROM header's test segment table;
- writes its entry point to the test-entry field.

The first screen is intentionally only a two-item proof of concept:
- INPUT TEST — reuses the live MIE/JVS state, including player switch words, panel test/service switches, coin meters and analog channels;
- EXIT — currently reports that BIOS return is not implemented and does not reboot or fake a return.

Navigation uses the KOS MIE definitions:
- panel PSW2 (`jvs.panel.psw.service`) advances the selection;
- panel PSW1 (`jvs.panel.psw.test`) selects/returns.

These are the public MIE driver definitions in `dc/maple/mie.h`; they are not inferred from player-button bits.

A build has completed successfully for the initial source revision (Actions run 17). A later revision also initializes PVR explicitly in the separate test startup. The latest CI run must pass before distributing its artifact.

### New research clue: BIOS boot header

Flycast's `RomBootID` mirrors the Naomi boot header and explicitly names the fields `gameLoad[8]`, `testLoad[8]`, `gamePC`, and `testPC`. Its source comment says the structure is copied by the BIOS to system RAM at `0x0c01f400`. This independently confirms that the test load descriptors and test PC are first-class BIOS boot data. It does not reveal the BIOS's exit-to-System-Menu call or convention.

The Sega GD-ROM system manual documents the user-facing lifecycle: select GAME TEST MODE from the system menu, use SERVICE to move and TEST to select, then choose EXIT in the title's test menu. It describes returning to the game mode; the supplied Naomi menu-flow screenshots show the System Menu reappearing. We should verify the exact return destination on the target BIOS/loader rather than hard-code a reset or guessed BIOS address.

### Next validation

1. Confirm the latest CI package's header has distinct main/test entry points and distinct load descriptors.
2. Boot the package normally and confirm the existing probe still works.
3. Enter GAME TEST MODE from the actual Naomi System Menu and confirm the separate test executable starts.
4. Confirm PSW2/PSW1 navigation and live INPUT TEST.
5. Establish the actual EXIT path by tracing BIOS/game behaviour or comparing a known-working commercial title; only then implement EXIT.


### BIOS return path candidate found in KallistiOS

KallistiOS exposes a standard BIOS-menu exit path:
- `arch_set_exit_path(ARCH_EXIT_MENU)` selects the menu exit path;
- the normal KOS `exit()` shutdown flow runs `arch_shutdown()`;
- `arch_exit_handler()` dispatches to `arch_menu()`;
- `arch_menu()` calls `syscall_system_bios_menu()`;
- that syscall invokes the system BIOS vector with `FUNC_SYSTEM_BIOS_MENU`.

The test program now uses this KOS path when EXIT is selected. This is materially better grounded than a reset or guessed address. The KOS API documentation calls it the BIOS menu exit path, but it has not yet been verified on this Naomi BIOS. The next build is a test candidate, not proof that the screen returns to the exact Naomi System Menu shown in the supplied screenshots.
