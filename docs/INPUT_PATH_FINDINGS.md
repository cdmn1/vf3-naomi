# VF3 input-path findings

Reviewed: 2026-11-17

## Naomi hardware input validation

The standalone KallistiOS MIE/JVS probe has now booted on the user's real Naomi. The user tested all cabinet buttons and analog inputs and confirmed that they respond. This verifies the physical cabinet → JVS I/O board → Naomi MIE → KOS `mie_state_t` path on this setup.

This result is deliberately separate from the Dreamcast guest input findings below. It does not establish that the original VF3 executable's controller reader or merge routine has run, nor that the Dreamcast Maple API can be replaced by simply reading KOS's mapped `cont` state. The dedicated `wheel`/`accel`/`brake` fields are not needed: use native JVS switch words and `jvs.analog[]` for this fighting-game input adapter.

A first conversion layer now exists in `tests/naomi_mie_probe/vf3_input_map.c`. It converts each player's active-high JVS switch word into a standard Dreamcast controller button word, active-low, including the standard-pad released-bit mask. The provisional mapping is Push 1 = Punch/A, Push 2 = Kick/B, Push 3 = Guard/X, Push 4 = Escape/Y, plus digital directions and Start. The pure conversion tests passed in GitHub Actions run 15. The probe displays the resulting P1/P2 Maple words so the cabinet's actual switch assignment can be checked. This helper is not yet wired into the original guest runtime.

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
offset `+0x1EC`. The player action update candidate is `0x8C0AD1B8`.
These are reconstructed C implementations from the related project, not
direct proof of runtime execution in our Naomi target.

## Two different harness findings — do not conflate them

### Native `vf3sdl` harness

The related project's `docs/re/boot_offinv_and_pad.md` reports an old host
bug: the harness published pad state only when a scripted press was active.
With no script, the Maple model's static zero button word meant every
active-low button appeared pressed. Publishing the neutral pad every frame
fixed that. The note reports that holding Start with
`VF3_PRESS=8,0,900,1000` passed its press-start screen and reached menu code.

That is a finding about the native `vf3sdl` harness and its host-side Maple
model. It is not evidence that the patched Flycast cold-boot ladder reaches
character select or a live round.

### Patched Flycast captures of the original executable

The related project's `docs/re/front_to_fight.md` records that corrected
Start/A masks reached Maple (the expected active-low words were logged), but
the tested cold-boot scripts and checkpoint replays remained on
autonomous attract/demo paths. In those scans:

- Fight workers and the MT evaluator sometimes executed.
- Controller merge `0x0C09AF6C` was not reached.
- Selected-port reader `0x0C09B006` was not reached.
- Player action update `0x0C0AD1B8` was not reached.

The documented `vf3_play_frontend_ladder.txt` is therefore only an input /
navigation probe, not a verified route to the boundary. The separate
`vf3_play_start_hold.txt` is also explicitly described as a navigation
probe; its recorded Flycast scans did not establish player control.

## Consequence for this Naomi project

1. Keep the confirmed Dreamcast Maple active-low representation and Start/A
   masks (`0x8` and `0x4`) unchanged.
2. Treat the Naomi JVS hardware input source as proven on the tested cabinet;
   do not spend more time troubleshooting its wiring or basic MIE polling.
3. Do not infer a game-level input failure from successful JVS polling alone.
   The missing evidence is execution of the original guest's selected-port
   reader and controller-merge path.
4. Do not reuse the native harness's `VF3_PRESS` result as proof of a
   successful Flycast menu route.
5. The maintained Flycast watch list now probes the packet lookup at
   `0x8C0A1282` as well as the selected-port reader
   (`0x8C09B006`), controller merge (`0x8C09AF6C`) and player action update
   (`0x8C0AD1B8`) on every interpreter execution. The first capture should
   determine which of these routines are reached during normal navigation.
   Existing GDFS/MT watches remain enabled.
6. Once the guest input seam is observed, add a small adapter that translates
   JVS active-high switches and the required analog channels into the exact
   guest-facing Maple/controller representation. Do not invent the mapping
   before identifying the buttons the game actually consumes.

## Sources

- [Related project's Maple / pad and harness findings](https://github.com/celsowm/vf3-decomp/blob/main/docs/re/boot_offinv_and_pad.md)
- [Flycast front-end capture results and limits](https://github.com/celsowm/vf3-decomp/blob/main/docs/re/front_to_fight.md)
- [Selected-port reader reconstruction](https://github.com/celsowm/vf3-decomp/blob/main/src/fight/controller_port.c)
- [Controller merge reconstruction](https://github.com/celsowm/vf3-decomp/blob/main/src/fight/controller_merge.c)
- [Maple HLE implementation](https://github.com/celsowm/vf3-decomp/blob/main/src/sys/maple_hle.c)


## Follow-up trace instrumentation (2026-11-18)

The first expanded capture reached controller merge `0x0C09AF6C` 138 times,
player action update `0x0C0AD1B8` 17 times, and packet lookup
`0x0C0A1282` 6,654 times. The selected-port reader entry
`0x0C09B006` remained at zero. The lookup's PR distribution included
`0x0C09AF82`, `0x0C09B068` and `0x0C09B070`, each exactly 138 times.

Inspection of the related project's instruction-level reconstruction
`src/fight/vf3_09b_adapter.c` resolves those return sites:

- `0x0C09B058` calls controller merge `0x0C09AF6C`; its return
  continuation is `0x0C09B05C`.
- The same wrapper then loads a function pointer from
  `0x0C09B130` and calls it indirectly twice, first with `R4=0`
  (return continuation `0x0C09B068`) and then with `R4=1`
  (return continuation `0x0C09B070`).
- At both lookup-entry snapshots, `R3=0x0C0A1282`. These are therefore
  confirmed indirect calls to the same packet-record lookup, for port 0 and
  port 1—not unrelated callers.
- The lookup at `0x0C0A1282` computes a record address from the base pointer
  stored at `0x0C0A1300`, using a 20-byte stride per port. The wrapper then
  checks the status word at each returned record against the disconnected
  sentinel `0xFFFFFFFE`.
- The merge's own lookup return site `0x0C09AF82` also occurred 138 times.
  The other two conditional merge lookup return sites
  (`0x0C09AF94` and `0x0C09AFA8`) were not observed in this capture.

This is the first direct Flycast evidence that the original guest executes a
controller-management sequence: the wrapper calls the merge routine and then
queries both port records. It does not yet prove that a player-controlled
round is active, that the queried record contains live Maple button data, or
that `0x0C0AD1B8` consumes the merged word. The Naomi JVS-to-Maple adapter
therefore remains provisional.

The input watch profile has been expanded to capture the return continuations
(`0x0C09AF82`, `0x0C09B068`, `0x0C09B070`), the two status-check sites
(`0x0C09B074`, `0x0C09B07C`), and reads/writes of the fixed packet-base
pointer (`0x0C0A1300`), merge selector (`0x0C29BCC4`) and merged output
(`0x0C29BA50`). The next capture should reveal the computed per-port record
addresses, connection status values and merged controller word.
