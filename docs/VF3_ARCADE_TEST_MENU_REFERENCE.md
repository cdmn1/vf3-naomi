# VF3 arcade test menu reference and Naomi probe experiment

Date: 2026-11-16

## User-supplied Model 3 VF3 menu reference

The original VF3 arcade / Model 3 test menu is reported as:

Header: `TEST MENU`

1. CPU MEMORY TEST
2. VIDEO MEMORY TEST
3. BOUNDARY SCAN TEST
4. INPUT TEST
5. OUTPUT TEST
6. SOUND TEST
7. C.R.T. TEST
8. TIMER TEST
9. GAME ASSIGNMENT
10. COIN ASSIGNMENT
11. BOOKKEEPING
12. V3 DIAGRAM
13. BACKUP RAM CLEAR
14. ALL INITIALIZE
15. -> EXIT

Footer:

`SELECT BY SERVICE BUTTON`
`AND PRESS TEST BUTTON`

This is the title-specific VF3 reference menu, not the Naomi platform SYSTEM MENU. Preserve the original labels/order as a reference; determine which tests and settings can actually be implemented on Naomi before enabling each item.

## Existing Naomi MIE/JVS probe as the experiment target

The existing `tests/naomi_mie_probe/vf3_mie_probe.bin` is a known-booting Naomi ROM and has already verified the physical cabinet-to-MIE/JVS input path on the user's hardware.

Important correction: its current ROM test entry does not open a menu. It jumps into the same application as the normal entry, as does the current Daytona probe. It is therefore a hardware/input baseline, not evidence of a functioning game test-mode flow.

The probe is still the best low-risk experiment target because it already has:
- Naomi MIE/JVS initialization with the known KOS Retail-mode workaround;
- live JVS player switch words, system/panel switches, coin counters and analog channels;
- a working PVR text display;
- verified cabinet controls on the user's real Naomi.

Do not repeat basic wiring or MIE polling tests.

## Packaging finding

The current probe Makefile runs KallistiOS `naomibintool build ... -e vf3_mie_probe.elf`.

The inspected KOS `naomibintool` ELF packer assigns both `hdr->entry` and `hdr->test_entry` from the same ELF entry point, and duplicates the same segment descriptor into the normal and test segment tables. Therefore this build path cannot, by itself, provide a distinct test executable.

Naomi's ROM header does have separate main and test segment tables and entry-point fields. A dual-program experiment needs a packaging path that fills those fields with the correct independent load segments and entry points.

Do not simply set `test_entry` to the address of an ordinary C function in the existing ELF. A direct BIOS entry must have a valid startup environment (stack, runtime initialization, data/BSS setup, constructors as applicable). A separately linked test ELF with its own KOS startup is the safer first experiment unless inspection proves a shared-runtime handoff contract.

## First prototype scope

Build a distinct test program into the probe ROM while leaving the normal application entry unchanged.

The first test program should be deliberately small:
- draw a Naomi-style title test screen using the probe's proven PVR text rendering;
- show a short menu with an INPUT TEST item and an EXIT item;
- navigate with the already verified SERVICE switch and activate with the TEST switch;
- INPUT TEST should reuse the existing live MIE/JVS diagnostic display;
- record service/test switch states explicitly in the diagnostic output if their exact mapping is not yet certain;
- EXIT must be treated as incomplete until the real BIOS return-to-System-Menu protocol is identified and demonstrated.

Do not fake EXIT by rebooting into the normal game and call that a successful return. Do not implement destructive actions or persistent settings in this first prototype.

## Validation gates

1. Inspect the generated ROM header and verify that main/test entry points and segment tables are distinct and internally consistent.
2. Confirm normal boot still reaches the existing MIE/JVS probe.
3. Invoke GAME TEST MODE through the Naomi BIOS and confirm it enters the separate test program.
4. Confirm SERVICE navigates and TEST selects.
5. Confirm INPUT TEST displays live cabinet input using the existing, already validated MIE/JVS path.
6. Identify and verify the correct exit-to-System-Menu protocol on Flycast and real Naomi.
7. Only after the lifecycle works, grow the title menu and decide which Model 3 tests/settings have meaningful Naomi equivalents.

## Important scope boundary

This experiment establishes the standard Naomi title-test program lifecycle. It does not replace the platform Naomi SYSTEM MENU, and it does not yet implement VF3-specific assignments, coin settings, bookkeeping, backup clearing, ALL INITIALIZE, or the V3 diagram.
