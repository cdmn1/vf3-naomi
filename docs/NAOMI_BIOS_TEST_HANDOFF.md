# Naomi retail BIOS test-menu hand-off

## Confirmed commercial-game implementation

The Virtua Fighter 4 Final Tuned (VF4FT) decompilation contains both sides of
the Naomi test-menu lifecycle, including the separate test.bin program.

Its SDK routine bios_helpers_copy():

1. Reads the Naomi BIOS helper table at 0xA0060000.
2. Checks that its first eight entries point into the BIOS helper region
   0x0C01xxxx.
3. Copies 0x7000 bytes (0x1c00 words) to 0xAC018000.
4. Sets the game's helper-table pointer to that copied block.

The BIOS helper table entry at offset 0x44 is the system-test hand-off.
VF4FT's bios_helper_44() waits until G1 DMA is idle, acknowledges the
cartridge bus by writing 0xC000 to 0xA05F700C, and calls the helper
through its uncached P2 alias. In the observed VF4FT build, the table entry
resolves to cached address 0x0C018188, called as 0xAC018188. The call
does not return.

## Both transitions use the same service

### Running game -> Naomi System Test Menu

VF4FT's test_switch_exit_to_bios() polls the merged JVS TEST input using
mask 0x10000004; bit 2 is the physical JVS TEST switch. Once pressed, the
game waits for card-unit operations to finish, then shuts down networking,
clears hardware breakpoints, stops sound, holds the G1 bus, quiesces Naomi 2
ELAN registers where applicable, and calls bios_helper_44().

### Game Test Menu -> Naomi System Test Menu

The separate VF4FT test.bin program's test_menu_exit_to_bios() handles
the EXIT item. It shuts down networking, holds the G1 bus, quiesces Naomi 2
ELAN registers where applicable, and calls the same bios_helper_44().

This is direct evidence for the intended flow:

GAME RUNNING -- physical JVS TEST --> NAOMI SYSTEM TEST MENU
GAME TEST MENU -- select EXIT -----> NAOMI SYSTEM TEST MENU

The KOS arch_menu() / syscall_system_bios_menu() path is not this
service. Nor is KOS ARCH_EXIT_RETURN. DreamShell's HOLLYSH_TEST_ADDR
is a separate entry point in its custom HollySH BIOS and must not be used as
a retail-BIOS address.

## Shared implementation in this repository

platform/naomi/naomi_bios.c provides:

- naomi_bios_helpers_init(): validates and copies the retail BIOS helper
  block to the reserved RAM area.
- naomi_bios_test_switch_update(): reports a rising edge of the physical
  TEST input.
- naomi_bios_enter_system_test(): invokes helper entry 0x44; it does
  not return.

The caller must stop title-specific activity before invoking the hand-off.
VF4FT's shutdown list is the reference; networking, card units, audio,
pending G1/DIMM transfers and Naomi 2 ELAN activity should be handled only
when the title actually uses them.

The standalone VF3 MIE/JVS probe now exercises both paths: the game-side
probe exits on a physical TEST press, and the separate test executable's
EXIT item calls helper 0x44.

## Validation still required

The code path is now grounded in a commercial title and has been integrated
into the probe build, but it is not yet hardware-validated in this project.
The next gate is a build followed by these checks in Flycast and on the
user's Naomi:

1. Boot the probe game executable.
2. Press the physical TEST button; confirm the Naomi System Test Menu appears.
3. Select GAME TEST MODE; confirm the VF3 probe test menu loads.
4. Select EXIT; confirm the Naomi System Test Menu reappears without reset.

Do not treat a successful compile as proof of the BIOS hand-off.

## Source references

The recovered VF4 Final Tuned Ver. B source used for this finding is
available here:

- [Game-side TEST handler](https://github.com/djanice1980/VF4-Final-Tuned-Rev-B-Decomp-Source/blob/5646898aca5eedd45ae7495ad52bc7bd479604d4/src/vf4lib/libbase/src/system.c) — test_switch_exit_to_bios().
- [Test-program EXIT handler](https://github.com/djanice1980/VF4-Final-Tuned-Rev-B-Decomp-Source/blob/5646898aca5eedd45ae7495ad52bc7bd479604d4/src_test/testmode/testmode.c) — test_menu_exit_to_bios().
- [BIOS helper copy and helper 0x44](https://github.com/djanice1980/VF4-Final-Tuned-Rev-B-Decomp-Source/blob/5646898aca5eedd45ae7495ad52bc7bd479604d4/src_test/sdk/libam/nlasrvdt.c) — bios_helpers_copy() and bios_helper_44().
