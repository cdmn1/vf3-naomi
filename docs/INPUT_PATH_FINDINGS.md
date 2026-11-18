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


## Analysis of the 2026-11-18 02:19 fight capture

The user clarified that this capture ended during an actual fight, with Punch,
Kick, Defend and D-pad directions used in the final seconds. Re-decoding the
uploaded `vf3-loader-trace.bin` confirms 8,570 PC snapshots and 13,564
memory records, with no malformed or truncated records.

The controller lookup return snapshots resolve the two per-port record
pointers consistently across all 104 wrapper passes:

- Port 0: `0x0C2CF834` (104/104 returns)
- Port 1: `0x0C2CF848` (104/104 returns)

These addresses are 20 bytes apart, matching the reconstructed lookup's
per-port record stride. The related `controller_port.c` reads the record
status at offset `+0` and the controller condition/button word at offset
`+8`; the adjacent word at `+12` carries the packed analog axes in the
Maple HLE layout.

Important limitation: the previous watch profile did **not** watch those
resolved per-port record fields. It watched the guessed fixed merged-output
address `0x0C29BA50`, which produced 103 zero writes, one `0x20` write and
one `0x08` write. Those two non-zero values cannot safely be labelled as
Down and Start merely by mapping bit positions: the actual packet words,
polarity and transformation at the record boundary were not captured. In
particular, this trace does not yet show whether Punch, Kick, Defend or
directions were present in the guest's raw per-port records.

The updated `tools/VF3_WATCH_INPUT.txt` now watches the status, controller
word and analog word for both resolved records:
`0x0C2CF834`, `0x0C2CF83C`, `0x0C2CF840`, `0x0C2CF848`,
`0x0C2CF850` and `0x0C2CF854`. This should capture the actual data returned
by the two Maple port lookups on the next run. No new capture is required to
establish the record addresses; a new one is needed only to collect these
newly added memory fields and correlate their values with gameplay inputs.

## Analysis of the 2026-11-18 15:04 digital-input capture

The latest Flycast capture contains 56,358 decoded memory records and 10,350
PC probe groups, with no decoder warnings. The controller wrapper ran 82
times, the packet lookup 9,816 times, and the player-action update candidate
42 times. The selected-port reader entry at `0x8C09B006` still had zero
hits; the wrapper's indirect lookup calls and merge are the observed path.

For the digital-controls investigation, ignore the adjacent `+12` field.
The test used a conventional controller with analog sticks, and the duplicated
values at `+12` do not provide useful evidence about Naomi's required digital
controls. They are not used to infer any mapping.

The port-0 controller word at `0x0C2CF83C` contained these non-zero
one-hot values during the capture:

- `0x0004`, `0x0008`, `0x0010`, `0x0020`
- `0x0040`, `0x0080`, `0x0200`, `0x0400`

These bit positions match the standard KOS/Dreamcast controller masks:
A, Start, Up, Down, Left, Right, Y and X respectively. No B (`0x0002`),
C (`0x0001`) or Z (`0x0100`) value was observed. This is a useful
identification of the guest record's digital bit positions, not yet a
reliable mapping from each physical Flycast button to a VF3 action. In
particular, the observed capture does not contain the expected B/Kick bit,
so do not mark the provisional Punch/Kick/Guard/Escape mapping as validated.

Port 1's controller word remained zero throughout. The report therefore
supports concentrating on port 0 and the digital word at `+8`; analog
channels and port 1 can be left out of the next analysis.

The next useful investigation is to trace where the guest consumes the
merged word at shared-state offset `+0x1EC` and how the player-action update
uses it. Keep the mapping between physical controls and VF3 actions provisional
until those consumers and a clean, individually controlled button sample
agree.

## Finding the active digital-input consumer in the 15:04 capture

A second pass over the memory trace identifies a more relevant path than the
controller merge alone. Reads of the port-0 controller word at
`0x0C2CF83C` came from these guest instruction sites:

- `0x0C096280`: 1,434 reads
- `0x0C0962A0`: 1,434 reads
- `0x0C0962C8`: 1,434 reads
- `0x0C09648C`: 637 reads
- `0x0C09AF84`: 82 reads

The related reconstruction identifies `0x8C096258` as an indexed node-flag
merge routine. Its implementation reads a 20-byte-stride node record rooted
at `0x0C2CF834`, tests the record's fields against a mask table, and ORs
derived flags into three output words at `R9+0x138`, `R9+0x13C` and
`R9+0x140`. Within its loop, the record fields are read at offsets +16,
+8 and +12; the +8 field is the digital controller word. This makes the
`0x0C0962A0` read site a strong candidate for the actual digital-input
translation path.

This also explains why the earlier merge result was incomplete. The merge
routine ran 82 times early in the capture and its output at
`0x0C29BA50` only changed to `0x20` and `0x08` (with neutral writes
between them). Later digital values continued arriving in the port record,
while the merge routine was no longer observed. Meanwhile the indexed
node-flag routine continued reading the input record. Therefore the merge
output at +0x1EC is not a sufficient proxy for the game's active input path.

The routine at `0x8C0AD1B8` also executes during this period, but its
register snapshots do not establish that it consumes the raw button word.
Keep its "player action update" label provisional.

The next trace should watch entry `0x8C096258` and its key read sites
`0x8C096280`, `0x8C0962A0`, `0x8C0962C8` and `0x8C09648C`.
Capture the entry registers (especially R4, R6 and R9) and the routine's
writes to the three output words. That should reveal which node index and
mask table are active and how each digital input bit becomes a gameplay
flag. Continue to ignore the analog field at +12 for the Naomi target.

Reference reconstruction: `celsowm/vf3-decomp/src/fight/f096258.c` and
`docs/re/f096258.md`. This is supporting Dreamcast reconstruction evidence;
the Naomi target still needs its own runtime confirmation.


## Analysis of the 2026-11-18 17:37 digital-input capture

The 17:37 capture decoded successfully: 20,672,096 bytes, 64,876 PC
probe groups and 337,842 memory records, with no decoder warnings. The
controller wrapper ran 287 times, the packet lookup 27,686 times, and the
player-action update candidate 34 times. The selected-port reader entry
at `0x8C09B006` remained unobserved.

The port-0 record status was `0x000F06FE` during the connected portion of
the capture; port 1 remained disconnected with status `0xFFFFFFFE`. The
port-0 digital word at `0x0C2CF83C` contained these non-zero values:

- `0x0002`, `0x0004`, `0x0008`
- `0x0010`, `0x0020`, `0x0040`, `0x0050`, `0x0080`
- `0x0200`, `0x0202`, `0x0400`, `0x0404`, `0x0406`

These correspond to standard KOS/Dreamcast digital bit positions, including
B (`0x0002`), which was not observed in the 15:04 capture. Composite values
such as `0x0050`, `0x0202`, `0x0404` and `0x0406` show that more than
one bit was set in some samples. The trace establishes the guest-facing bit
values, but it does not establish which physical cabinet button or VF3
action produced each value. Do not mark the physical button mapping as
validated from this capture alone.

The indexed node-flag routine at `0x8C096258` was watched 8,192 times
(the trace diagnostic counter reports this value; treat it as a lower bound
rather than an exact total). Its key sites at `0x8C096280`,
`0x8C0962A0` and `0x8C0962C8` were also watched 8,192 times, while
`0x8C09648C` was watched 3,318 times. The digital-word read site
`0x0C0962A0` generated 4,096 memory reads from the P1 button word.

Entry snapshots show two node indices, each occurring 4,096 times:

- R4 = 0: R6 = `0x0C11F50C`
- R4 = 1: R6 = `0x0C11F530`

Both use R9 = `0x0C29B864`. The related Dreamcast reconstruction of
`0x8C096258` identifies R4 as the signed node index, R6 as the mask-table
pointer, and three OR-accumulated output words at R9 + `0x138`,
R9 + `0x13C` and R9 + `0x140`. This resolves the candidate output
addresses for the next capture:

- `0x0C29B99C`
- `0x0C29B9A0`
- `0x0C29B9A4`

The maintained watch profile now includes these three output words. The
next capture should reveal how the raw digital controller bits are converted
into the indexed node flags. This is the next useful boundary to inspect
before implementing the Naomi JVS-to-guest input adapter.

The 17:37 analysis report is available as the
[vf3-input-field-report-202611181737 workflow artifact](https://github.com/cdmn1/vf3-naomi/actions/runs/44364557681).


## Analysis of the 2026-11-18 18:44 indexed-input capture

The 18:44 capture decoded cleanly: 22,031,712 bytes, 59,996 PC probe groups and 403,510 memory records, with no decoder warnings. Port 0 was connected (0x000F06FE); port 1 remained disconnected (0xFFFFFFFE). The controller merge ran 319 times and the 0x8C0AD1B8 player-action candidate ran 34 times.

The indexed node-flag routine at 0x8C096258 was observed 7,598 times. Its three internal read sites at 0x8C096280, 0x8C0962A0 and 0x8C0962C8 each had 7,598 PC hits. The actual digital controller word at 0x0C2CF83C was read 3,799 times at 0x0C0962A0. Entry snapshots again show the routine processing node indices R4=0 and R4=1, 3,799 times each, with R9 fixed at 0x0C29B864.

### Direct digital-word conversion

This capture finally records writes to the routine's indexed output words. The reconstruction identifies the three source fields as node offsets +16, +8 and +12, respectively. Therefore:
- R9+0x138 (0x0C29B99C) is derived from node field +16.
- R9+0x13C (0x0C29B9A0) is the output derived from the digital controller word at node field +8.
- R9+0x140 (0x0C29B9A4) is derived from node field +12 (the analog-related field, which remains out of scope for the Naomi mapping).

The following values are directly correlated in trace order between the port-0 digital word and writes to the **digital-derived output** 0x0C29B9A0. The KOS names describe the standard Dreamcast bit positions; they are not claims about the physical cabinet button labels.

| Raw word at 0x0C2CF83C | KOS bit | Output at 0x0C29B9A0 |
|---:|---|---:|
| 0x0008 | START | 0x0010 |
| 0x0004 | A | 0x0400 |
| 0x0002 | B | 0x0800 |
| 0x0200 | Y | 0x0200 |
| 0x0400 | X | 0x0100 |
| 0x0010 | UP | 0x2000 |
| 0x0020 | DOWN | 0x1000 |
| 0x0040 | LEFT | 0x8000 |
| 0x0080 | RIGHT | 0x4000 |
| 0x0404 | A + X | 0x0500 |

The A+X sample is a useful cross-check: the resulting 0x0500 is the combination of the independently observed A-derived 0x0400 and X-derived 0x0100 flags. B's digital-derived output is 0x0800; other indexed words can contain additional bits from their own source fields, so do not use +0x138 or +0x140 as the direct button mapping.

This is the first capture that demonstrates the complete runtime conversion from the guest's standard digital controller bits to the game's indexed digital-input flags. It substantially narrows the remaining work: identify where the 0x0C29B9A0 flags are consumed and what each internal flag means to VF3's action logic. The capture does **not** by itself prove that the 0x8C0AD1B8 routine is that consumer, and it does not justify assigning Punch/Kick/Defend/Escape names to the raw bits without correlating those labels to controlled samples.

The analysis report is available as the [vf3-input-field-report-202611181844 workflow artifact](https://github.com/cdmn1/vf3-naomi/actions/runs/44373205735).


## Static consumer found in the Dreamcast reconstruction

A source-level cross-check in `celsowm/vf3-decomp` found a confirmed read
of the digital-derived word at `0x0C29B9A0`: callable
`0x8C0C9F54` (`src/fight/audio_input.c`, described there as the
sound-selection input caller). It loads the word from scene base
`0x0C29B864 + 0x13C` and tests several bits, including the direction flags
`0x2000` (up), `0x1000` (down), `0x8000` (left) and `0x4000` (right).
Those tests adjust and wrap a selection index before issuing audio requests.
This confirms that the converted word is consumed by real guest code, rather
than being an unused diagnostic/output buffer.

This is **not yet the missing fight-action consumer**. The identified caller
is specifically the sound-selection input path; its use of direction and
other flags cannot be used to label Punch, Kick, Defend or Escape. The related
fight audio transition at `0x8C099060` reads scene offset `+0x138`
(`0x0C29B99C`), not the digital-derived `+0x13C` word, so it is not evidence
that the latter drives fighter actions either.

The action-consumer investigation remains open. In particular, do not
promote `0x8C0AD1B8` as the consumer based only on its execution or register
variation. Next, continue tracing reads of `0x0C29B9A0` (including indirect
or table-driven accesses) in the fight/action update path and correlate those
reads with individually controlled cabinet-button samples. Source reference:
`celsowm/vf3-decomp/src/fight/audio_input.c` and
`src/fight/audio_fight.c`.


A second menu-side consumer is present in the reconstruction's
`vf3_advance_scene_selector` helper in `src/fight/advance_scene_commands.c`.
It reads the same scene word at `0x0C29B864 + 0x13C`; with its selector
counter at 32 or above, it tests `0x1000` to advance and `0x2000` to move
back, then updates the selector byte at scene offset +11. This is further
evidence that the converted direction flags are used by scene navigation.
It is still not evidence about the in-match fighter-action consumer.

So far, the statically identified consumers of this exact word are both
selection/navigation paths (`audio_input.c` and
`advance_scene_commands.c`). The fight-action consumer remains unidentified;
the next useful step is to inspect the fight-frame update's data flow around
the indexed mask table and follow any copied/shifted versions of the word,
rather than treating every reference to a nearby +0x13C structure offset as
the same global.


## Correction: 0x8C0AD1B8 is not established as an action-handler entry

The earlier trace tooling called `0x8C0AD1B8` "player action update". That
label was too strong. In the Dreamcast reconstruction's
`src/fight/advance_leaf_ranked_more_adapters.c`, this address is an
instruction inside a larger translated adapter block: it saves the return
address and continues at `0x8C0AD1BA`, where the code loads a pointer from
`0x0C0AD250`. The inspected sequence does not read the indexed digital flags
at `0x0C29B9A0`. Therefore hits at `0x8C0AD1B8` alone do not identify the
input consumer or justify calling it the player-action update entry.

The trace report now labels this PC as an unclassified fight-frame
instruction and reports read instruction sites for all three indexed output
words, not only the raw controller and merged words. The next analysis should
start with the actual read-site histogram for `0x0C29B9A0`, then follow the
relevant readers in the reconstructed fight path. The two statically
identified consumers documented above remain scene-navigation paths, not
fighter-action handlers.
