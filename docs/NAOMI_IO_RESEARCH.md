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

KOS's automatic `INIT_MIE` path registers the driver and probes Port A. It
will use an already active MIE Z80/JVS bridge if available. The public
`mie_init_fw(fw_path)` API can upload `mie_z80.bin` when the bridge needs
firmware; its documentation says this is intended for early Naomi startup
before the normal Maple scan completes JVS setup. The firmware file is not
included in the KOS repository and is not bundled in this probe.

Therefore, a probe reporting Port A as JVS but no usable MIE input state may
indicate that the bridge firmware has not been activated; it does not
automatically prove the physical JVS wiring is wrong. Do not add an unknown
firmware blob or invent a path. Establish whether the BIOS/emulator already
initializes the bridge before deciding whether a separately sourced firmware
file is needed.

## Probe added to this repository

`tests/naomi_mie_probe/` contains a small KOS application that reports every
250 ms:

- Port A mode and MIE enumeration;
- mapped P1 controller buttons;
- raw P1/P2 JVS switch words and system/panel switches;
- coin meter values, insert pulses and fault flags;
- all eight analog channels.

The source is checked against the current KOS MIE public header and official
MIE example. It has not yet been compiled or run in this environment. Its
first success criterion is a real runtime report of `MIE_PORT0_JVS` and a
changing raw P1 switch word when a cabinet control is pressed.

## Sources

- [KallistiOS MIE public header](https://github.com/KallistiOS/KallistiOS/blob/master/kernel/arch/dreamcast/include/dc/maple/mie.h)
- [KallistiOS MIE driver](https://github.com/KallistiOS/KallistiOS/blob/master/kernel/arch/dreamcast/hardware/maple/mie/mie.c)
- [KallistiOS official MIE example](https://github.com/KallistiOS/KallistiOS/tree/master/examples/dreamcast/mie/basic)
- [KallistiOS 2.1 release notes](https://github.com/KallistiOS/KallistiOS/releases)
- [dcrecomp early Naomi I/O implementation](https://github.com/sp00nznet/dcrecomp/blob/main/src/hal/naomi_io.c)
