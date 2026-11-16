# Naomi MIE/JVS input probe

A tiny standalone KallistiOS application to verify Naomi Port A's MIE/JVS
input path before VF3 integration. It does not boot VF3 and does not touch
the game executable or assets.

## Build

Use a current KallistiOS toolchain configured for Naomi (KOS 2.1.x or later
is the documented baseline for current Naomi/NetDIMM support).

From this directory:

```sh
make
make dist
```

This produces `vf3_mie_probe.elf` and `vf3_mie_probe.elf.bin`.

## What it reports

Every 250 ms the probe refreshes an on-screen status page showing:

- Port A mode: MIE/JVS, standard Maple, or unknown;
- whether the MIE device enumerated;
- KOS-mapped player-one controller buttons;
- native JVS player 1/player 2 switch words, system switches and cabinet
  panel test/service switches;
- both coin meter counts, insert pulses and fault flags;
- all eight raw JVS analog channels.

The program uses KOS's `INIT_MIE` startup flag and the public
`MAPLE_FUNC_MIE` / `mie_state_t` API. KOS documents the MIE as the Naomi
Maple-port-A bridge to the JVS I/O board; it is not a direct Dreamcast
controller packet and should not be replaced with the simplified JVS DMA
stub found in unrelated early recompilation frameworks.

## Validation

This source has been checked against the current KallistiOS MIE header and
its `examples/dreamcast/mie/basic` example. It has **not** been compiled or
run in this environment. A successful build is not a hardware test. The
first runtime check is whether Port A reports MIE/JVS and the native P1
switch word changes when a control-panel switch is pressed.

## Reference

- KallistiOS MIE driver: https://github.com/KallistiOS/KallistiOS/blob/master/kernel/arch/dreamcast/include/dc/maple/mie.h
- KallistiOS official MIE example: https://github.com/KallistiOS/KallistiOS/tree/master/examples/dreamcast/mie/basic
- KallistiOS 2.1 release notes: https://github.com/KallistiOS/KallistiOS/releases
