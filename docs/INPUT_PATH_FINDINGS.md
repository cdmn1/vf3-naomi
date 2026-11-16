# VF3 input-path findings

Reviewed: 2026-11-16

## Scope and source boundary

This note separates observations from the Dreamcast research harness in
[celsowm/vf3-decomp](https://github.com/celsowm/vf3-decomp) from observations
made by its patched Flycast capture runs. Neither is a Naomi test. The source
project is a behavioral reference only; these findings do not establish that
the original executable's Naomi path is active.

## Confirmed input representation

The related project's Maple implementation and input notes document the
Dreamcast controller wire representation:

- Button bits are active-low on the Maple reply: zero means pressed and
  `0xFFFF` means no buttons pressed.
- The KOS/Flycast pressed-button masks used by its frame scripts are Start
  `0x8` and A `0x4`.
- The standard controller model forces unsupported controls to released.
- The guest requests the pad through Maple GET_CONDITION; the host input is
  not read directly by game code.

The related project's `src/fight/controller_port.c` reconstructs the
selected-port reader at `0x8C09B006`. It calls the packet lookup at
`0x0C0A1282`, checks the disconnected status sentinel
`0xFFFFFFFE`, and returns the selected packet word. Its
`src/fight/controller_merge.c` reconstructs `0x8C09AF6C`: it reads the
packet lookup, uses selector state at `0x0C29BCCC`, chooses packet zero,
packet one, or their bitwise OR, then writes the merged word at shared-state
offset `+0x1EC`. These are reconstructed C implementations from the
related project, not direct proof of runtime execution in our Naomi target.

## Two different harness findings — do not conflate them

### Native `vf3sdl` harness

The related project's `docs/re/boot_offinv_and_pad.md` reports an old host
bug: the harness published pad state only when a scripted press was active.
With no script, the Maple model's static zero button word meant every
active-low button appeared pressed. Publishing the neutral pad every frame
fixed that. The note reports that holding Start with
`VF3_PRESS=8,0,900,1000` passed the press-start screen and reached menu code.

That is a finding about the native `vf3sdl` harness and its host-side Maple
model. It is not evidence that the patched Flycast cold-boot ladder reaches
character select or a live round.

### Patched Flycast captures of the original executable

The related project's `docs/re/front_to_fight.md` records that corrected
Start/A masks reached Maple (the expected active-low words were logged), but
the tested cold-boot scripts and checkpoint replays remained on autonomous
attract/demo paths. In those scans:

- Fight workers and the MT evaluator sometimes executed.
- Controller merge `0x0C09AF6C` was not reached.
- Selected-port reader `0x0C09B006` was not reached.
- Player action update `0x0C0AD1B8` was not reached.

The documented `vf3_play_frontend_ladder.txt` is therefore only an input /
navigation probe, not a verified route to a player-controlled round. The
separate `vf3_play_start_hold.txt` is also explicitly described as a
navigation probe; its recorded Flycast scans did not establish player
control.

## Consequence for this Naomi project

1. Do not change the Start/A bit values: `0x8` and `0x4` are already
   confirmed for the Dreamcast/KOS input model.
2. Do not infer a game-level input failure from Maple delivery alone. The
   missing evidence is execution of the game's selected-port and merge path.
3. Do not reuse the native harness's `VF3_PRESS` result as proof of a
   successful Flycast menu route.
4. The next useful original-executable capture must distinguish menu states
   and record the first transition into the selected-port reader /
   controller-merge path. Once that boundary is found, the existing GDFS/MT
   watches can be combined with it to capture character-load resources.

## Sources

- [Related project's Maple / pad and harness findings](https://github.com/celsowm/vf3-decomp/blob/main/docs/re/boot_offinv_and_pad.md)
- [Flycast front-end capture results and limits](https://github.com/celsowm/vf3-decomp/blob/main/docs/re/front_to_fight.md)
- [Selected-port reader reconstruction](https://github.com/celsowm/vf3-decomp/blob/main/src/fight/controller_port.c)
- [Controller merge reconstruction](https://github.com/celsowm/vf3-decomp/blob/main/src/fight/controller_merge.c)
- [Maple HLE implementation](https://github.com/celsowm/vf3-decomp/blob/main/src/sys/maple_hle.c)
