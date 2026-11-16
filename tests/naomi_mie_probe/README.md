# Naomi MIE/JVS input probe

A tiny standalone KallistiOS application to verify Naomi Port A's MIE/JVS
input path before VF3 integration. It does not boot VF3 and does not touch
the game executable or assets.

## Build

Use a current KallistiOS toolchain configured for Naomi (KOS 2.1.x or later
is the documented baseline for current Naomi/NetDIMM support).

From this directory:

```sh
make clean rom
```

This produces `vf3_mie_probe.bin`, a Naomi ROM image packaged by KallistiOS's
`naomibintool`. Do not upload the ELF or an `objcopy -O binary` output: those
are not substitutes for the Naomi ROM image header/container. The packaging
recipe follows the known-good Daytona `naomi_cart_probe_v1.bin` build path.

## What it reports

Every 250 ms the probe refreshes an on-screen status page showing:

- Port A mode: MIE/JVS, standard Maple, or unknown;
- whether the MIE device enumerated;
- KOS-mapped player-one controller buttons;
- native JVS player 1/player 2 switch words, system switches and cabinet
  panel test/service switches;
- both coin meter counts, insert pulses and fault flags;
- all eight raw JVS analog channels;
- the JVS-to-Dreamcast-Maple button word for each player, in active-low wire
  form.

The program uses KOS's `INIT_MIE` startup flag and the public
`MAPLE_FUNC_MIE` / `mie_state_t` API. KOS documents the MIE as the Naomi
Maple-port-A bridge to the JVS I/O board; it is not a direct Dreamcast
controller packet and should not be replaced with the simplified JVS DMA
stub found in unrelated early recompilation frameworks.

## Validation

The source has compiled, but the first packaged build was rejected by the
Naomi main board with error 01 ("game not acceptable by main board"). That
build uploaded a raw flat binary rather than a Naomi ROM image. The packaging
has now been corrected to use `naomibintool`, matching the known-good Daytona
cart-probe recipe. The corrected image still requires a fresh hardware test.

The probe now also exercises a small, host-unit-tested input adapter. Its
provisional default layout maps JVS push switches 1–4 to VF3 Punch, Kick,
Guard and Escape, respectively, then encodes the buttons and directions as a
standard Dreamcast Maple controller word (active-low). The mapping is visible
on screen and can be adjusted if the cabinet's switch wiring differs.

This proves the JVS-to-Maple conversion independently; it does not yet mean
the original VF3 guest executable is consuming the generated word. That
requires the Maple HLE/input seam to be integrated into the game runtime.

The automatic `INIT_MIE` path expects the MIE bridge firmware to already
be active (for example, initialized by the Naomi BIOS). KOS also exposes
`mie_init_fw(fw_path)` for uploading `mie_z80.bin` when required, but that
firmware is not bundled here. Do not interpret an empty input page as proof
of bad wiring until bridge firmware state is known.

## Reference

- KallistiOS MIE driver: https://github.com/KallistiOS/KallistiOS/blob/master/kernel/arch/dreamcast/include/dc/maple/mie.h
- KallistiOS official MIE example: https://github.com/KallistiOS/KallistiOS/tree/master/examples/dreamcast/mie/basic
- KallistiOS Naomi ROM packaging tool: https://github.com/KallistiOS/KallistiOS/tree/master/utils/naomibintool
- KallistiOS 2.1 release notes: https://github.com/KallistiOS/KallistiOS/releases
