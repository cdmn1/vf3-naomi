# Naomi I/O research: use the MIE/JVS bridge

Reviewed: 2026-11-16

## Finding

For a normal Naomi Port A JVS setup, the correct software abstraction is not a
Dreamcast controller packet and not a generic "JVS DMA" response buffer.
KallistiOS documents the Naomi MIE (Maple I/O Emulator) as a Z80-based bridge
that connects the Maple bus to a JVS I/O board. It normally occupies Maple
Port A, unit 0. Port A can instead be configured as a normal Maple port, so
the program must check which mode was detected.

The KallistiOS MIE driver is a much stronger starting point than the Naomi I/O
stub in the early-stage `sp00nznet/dcrecomp` framework. That framework's
`naomi_io.c` describes its own DMA response as provisional and says the
exact format depends on BIOS version; it should not be copied as a proven
Naomi JVS implementation.

## KallistiOS API path

The current KallistiOS MIE header and official `examples/dreamcast/mie/basic`
example establish this usage pattern:

1. Compile for the Naomi subarchitecture with a current KallistiOS toolchain.
2. Enable `INIT_MIE` alongside `INIT_DEFAULT`.
3. Wait for the initial Maple scan with `maple_wait_scan()`.
4. Check `mie_port0_mode()`:
   - `MIE_PORT0_JVS`: Port A is connected to the MIE/JVS bridge.
   - `MIE_PORT0_MAPLE`: Port A is operating as a normal Maple port.
   - `MIE_PORT0_UNKNOWN`: probing has not completed.
5. Find the MIE device with `maple_enum_type(0, MAPLE_FUNC_MIE)`.
6. Cast `maple_dev_status(dev)` to `mie_state_t *`.

The state contains both:
- `cont`: KOS's mapped Dreamcast-style controller state;
- `jvs`: native JVS inputs, including P1/P2 switches, system/panel
  switches, eight analog channels and two coin meters.

This is useful because the native JVS values can be recorded before we choose
how VF3's fighting-game controls should be mapped.

## Relevant switch mapping

KOS documents the first player switch word as active-high, with these bits:

| JVS input | Bit |
|---|---:|
| Start | 15 |
| Service | 14 |
| Up | 13 |
| Down | 12 |
| Left | 11 |
| Right | 10 |
| Push switch 2 | 9 |
| Push switch 1 | 8 |
| Push switches 3–10 | 0–7 |

The KOS default mapper maps JVS SW16 to `CONT_A`, SW2 to `CONT_B`, SW1 to
`CONT_X`, SW14 to `CONT_Y`, and player service to `CONT_Z`. These are
KOS defaults, not a verified VF3 button assignment. We should inspect the
raw JVS words first and then set a VF3-specific mapping deliberately.

## MIE firmware caveat

The known-good Daytona `naomi_input_probe_v5.bin` established an additional
critical requirement: on the tested Naomi setup, `hardware_sys_mode()` reports
`HW_TYPE_RETAIL`, and unmodified KallistiOS consequently returns early from
both `mie_init()` and `mie_init_fw()` because of its Retail-Dreamcast guards.
The Daytona v5 workflow removes those two guards from KOS's
`kernel/arch/dreamcast/hardware/maple/mie/mie.c` before building KOS. This is
a Naomi-only build workaround, not a change to Naomi hardware mode and not a
request to switch Port A into Dreamcast controller mode.

The VF3 probe must use the same KOS source patch. Merely compiling the
application with `INIT_DEFAULT | INIT_MIE` is insufficient when those guards
remain in the KOS library. Do not revisit firmware upload, generic Maple
mode, or physical wiring as the first diagnosis: first reproduce the exact
v5 KOS build treatment and then compare runtime enumeration and native JVS
state.

## Probe added to this repository

`tests/naomi_mie_probe/` contains a small KOS application that reports every
250 ms. Its Naomi ROM image must be packaged with KOS `naomibintool`; a raw
ELF or flat `objcopy` binary is not an acceptable substitute. This follows the
known-good Daytona `naomi_cart_probe_v1.bin` build recipe. The first VF3
hardware attempt was rejected by the main board with error 01 because the
workflow had uploaded a flat binary; the workflow and Makefile have now been
corrected to emit a `naomibintool` image. The corrected image is awaiting
hardware validation.

- Port A mode and MIE enumeration;
- mapped P1 controller buttons;
- raw P1/P2 JVS switch words and system/panel switches;
- coin meter values, insert pulses and fault flags;
- all eight analog channels.

The probe uses the same `INIT_DEFAULT | INIT_MIE` startup flags and
`maple_enum_type(..., MAPLE_FUNC_MIE)` / `maple_dev_status()` native-state path
as Daytona v5. The important difference was in the KOS library build: Daytona
v5 removed the two Retail guards from `mie_init()` and `mie_init_fw()`. The VF3
workflow now applies that exact Naomi-only patch before building KOS. The first packaged image was not
accepted by the Naomi main board; that failure occurred before the program
could execute and therefore says nothing about MIE/JVS behavior. The corrected
Naomi ROM image must first boot, then report `MIE_PORT0_JVS` and a changing raw
P1 switch word when a cabinet control is pressed.

## Flycast validation path

The current Flycast source contains a Naomi MIE/JVS implementation in
`core/hw/maple/maple_jvs.cpp`; its Maple device factory creates the MIE
device for `MDT_NaomiJamma`. For an unrecognized game ID, the MIE defaults
to the Sega `837-13551` JVS I/O board model. That model advertises two player
switch banks, 13 digital inputs, two coin inputs, eight analog channels and
six digital outputs. The input reader consumes Flycast's configured Naomi
input state.

This gives us a concrete emulator-side test target for the probe: the MIE
should enumerate, the two native player switch words should react to the
configured player controls, and the eight analog values should be readable.
This is source-level evidence that Flycast implements the required device,
not a claim that this particular probe has already run successfully in
Flycast. Actual runtime validation remains pending.

Reference: [Flycast MIE/JVS implementation](https://github.com/flyinghead/flycast/blob/master/core/hw/maple/maple_jvs.cpp)
and [Maple device factory](https://github.com/flyinghead/flycast/blob/master/core/hw/maple/maple_devs.cpp).

## Sources

- [KallistiOS MIE public header](https://github.com/KallistiOS/KallistiOS/blob/master/kernel/arch/dreamcast/include/dc/maple/mie.h)
- [KallistiOS MIE driver](https://github.com/KallistiOS/KallistiOS/blob/master/kernel/arch/dreamcast/hardware/maple/mie/mie.c)
- [KallistiOS official MIE example](https://github.com/KallistiOS/KallistiOS/tree/master/examples/dreamcast/mie/basic)
- [KallistiOS 2.1 release notes](https://github.com/KallistiOS/KallistiOS/releases)
- [dcrecomp early Naomi I/O implementation](https://github.com/sp00nznet/dcrecomp/blob/main/src/hal/naomi_io.c)
