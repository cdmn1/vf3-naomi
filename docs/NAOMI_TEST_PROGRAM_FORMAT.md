# Naomi title test-program format: initial findings

Date: 2026-11-17

## Confirmed platform behaviour

The Naomi GD-ROM System Service Manual describes a platform-level SYSTEM MENU. Selecting GAME TEST MODE enters the test mode specific to the connected title. The title-side menu normally includes input testing, game assignments and bookkeeping, with options varying by game. Settings are committed when test mode is exited; power-off before exiting can discard changes.

The title test mode is distinct from the Naomi SYSTEM MENU. A game port should provide its title-side test program and must not attempt to replace the platform menu.

The manual describes selecting EXIT in a title's test mode as returning to **game mode**. That is not the same as invoking a BIOS System Menu operation.

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

The VF3 MIE/JVS probe uses `tests/naomi_mie_probe/combine_naomi_test_rom.py` to package a separate test ELF into the Naomi test segment. It retains the known-good normal entry and appends the test ELF payload at a 0x1000-aligned ROM offset, writing the test load descriptor and entry point into the header.

The test program contains:
- Naomi MIE/JVS initialization and live cabinet input polling;
- raw player switch words, system/panel switches, coin counters and analog channels;
- a working PVR text display;
- a small INPUT TEST / EXIT menu.

## What the exit experiment established

The first implementation selected KallistiOS `ARCH_EXIT_MENU`. Source inspection of KOS shows that this runs the normal KOS shutdown sequence and then calls `arch_menu()`, which invokes `syscall_system_bios_menu()` through the Dreamcast system-call vector.

That is a BIOS-menu operation, not the title-test return-to-game-mode operation described by the Naomi service manual. On the user's tested setup, selecting EXIT with this path resets the Naomi rather than returning to game mode.

The revised implementation selects `ARCH_EXIT_RETURN`. KOS's `arch_real_exit` restores the stack and return address saved by its startup code and returns through that saved address. For a BIOS-launched test executable, this is the candidate hand-off back to the Naomi loader, which should resume normal game mode. This is an evidence-based candidate, but it must still be verified on the real board.

## Experiment plan

1. Keep the known-good MIE/JVS and PVR code unchanged.
2. Build the test ELF with `ARCH_EXIT_RETURN` selected on EXIT.
3. Test from the actual Naomi SYSTEM MENU's GAME TEST MODE entry.
4. Confirm EXIT resumes normal game mode rather than resetting or returning to the system menu.
5. If it does not, inspect the BIOS test-entry calling convention and saved PR/stack state; do not substitute a guessed BIOS address.

## Implementation direction

After the entry/exit contract is proven, integrate the reusable title-side menu shell into VF3 and Daytona. Keep each title's settings and tests separate. Do not treat a reset or an unverified transition as a successful test-mode implementation.

## Sources inspected

- Sega, Naomi GD-ROM System Service Manual, section 5 SYSTEM TEST MODE and section 5-11 GAME TEST MODE.
- KallistiOS: `utils/naomibintool/naomibintool.c`.
- KallistiOS: `kernel/arch/dreamcast/kernel/init.c` and `kernel/arch/dreamcast/kernel/startup.S`.
- KallistiOS: `kernel/arch/dreamcast/hardware/syscalls.c`.
- DragonMinded/libnaomi: `libnaomi/cart.c`, `examples/eepromtest/Makefile`, and `examples/eepromtest/main.c`.
- VF3 Naomi: `tests/naomi_mie_probe/test_main.c` and `tests/naomi_mie_probe/combine_naomi_test_rom.py`.
- User-provided Naomi menu flow screenshots in `cdmn1/vf3-naomi/test menu flow/`.
