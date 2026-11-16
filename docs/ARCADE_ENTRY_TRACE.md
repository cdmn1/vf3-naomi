# Arcade entry path — current trace findings

Date: 2026-11-16

## Objective

Find the original VF3tb transition from the Dreamcast-specific front end into a player-controlled arcade round, then identify the initialization and resource-loading work that must be reproduced or replaced on Naomi.

The target is not to copy the Dreamcast mode/options UI. The target is to preserve the arcade game layer and enter it through a Naomi-compatible setup path.

## What is established

### User-observed runtime flow

The retail Dreamcast game boots into a 3D arcade-style attract sequence. Pressing Start opens a Dreamcast-specific mode/options interface. Selecting a game mode returns to arcade-style gameplay. Bonus FMVs are selected manually from the Dreamcast menus and are not part of the normal attract sequence.

These are user observations, not executable-trace conclusions.

### Attract/demo is not a playable round

The related reverse-engineering project's corrected Flycast input captures delivered Start (mask `0x8`) and A (mask `0x4`) to the Maple path. However, the tested cold-boot scripts remained in autonomous attract/demo behavior.

The scans did not reach:
- selected-port reader: `0x8C09B006`;
- controller packet merge: `0x8C09AF6C`;
- player action update: `0x8C0AD1B8`.

Fight workers and the MT evaluator did run in some checkpoints. This proves that the attract sequence exercises substantial fight-engine code; it does not prove player control.

### Fight-scene dispatcher

The related trace identifies the scene-0x0A predicate and walker:
- `0x8C0B24FA`: scene frame walker;
- `0x8C0B1AC0`: predicate sub-entry;
- fight scene ID: `0x0A`, read from scene-state byte `[3]`.

This is the per-frame fight-scene gate. It is not, by itself, the front-end-to-match transition or a safe entry point to call directly.

### One-time load boundary

The related fight-scene investigation reports:
- `mt_loader` at `0x8C02DCEC` and `coli_run` at `0x8C09AAE2` had zero hits in the post-initialization mid-round trace.
- These are interpreted as one-shot load functions: motion-pack loading and per-stage collision-data construction.
- The notes explicitly state that these functions must be captured across character/stage selection into Round 1; an already-running attract/demo fight state is too late.
- `bjload_run` at `0x8C0CBC40` is observed in the fight trace, but its dispatch context means the address is a dispatch waypoint, not necessarily a normal function entry.

Other useful anchors:
- `0x8C04BDA2`: per-pair loader referencing `MTTOUPAI.BIN`;
- `0x8C08232E`: another per-pair loader;
- `0x8C0360C8`: fingerprint-verified GDFS sector-read endpoint;
- runtime 132-pointer table: `0x8C1069BC..0x8C106BCC`;
- candidate resident MT destination window: `0x8C5F0000..0x8C860000`.

The 132-pointer runtime table is distinct from the 234-row embedded filename-string inventory. Its pointer chain and the MT loader's per-record scatter/rewrite remain unresolved.

## Required capture

Do not keep repeating the existing blind Start/A ladder: its corrected input values are known, but it has not reached a player-controlled round.

The useful capture must span the actual character-select confirmation and Round 1 load. The related project's fight-scene notes recommend a manual savestate from character select immediately before confirmation, followed by a trace through Round 1. A savestate is a convenience; a verified cold-boot route is also valid.

Use the checked-in combined watch list:
- `tools/VF3_WATCH_LOADER.txt`

It watches the MT loaders, GDFS sector-read endpoint, fighter loader, runtime name-pointer table and resident MT memory range. The capture must use the patched Flycast interpreter because the memory watches are interpreter-only.

For reproducibility, retain:
- the pre-confirmation state (if used);
- the exact input sequence or script;
- patched Flycast revision and interpreter setting;
- trace and emulator log;
- frame count and the first Round 1 frame/state.

Decode the resulting combined trace with:

```text
python tools/loader_trace_decode.py path/to/loader_trace.bin --prefix extract/analysis/loader_trace
```

Then correlate the first loader hits with the runtime table pointer, resource index/name, GDFS request and destination writes. Compare at least two fighter-pair packs and two motion IDs before generalizing the loader.

## Naomi port implications

1. Do not jump directly to the scene-0x0A predicate. Its scene root, task graph, assets, audio kit and fighter/stage setup are not proven to be initialized by such a jump.
2. Preserve the original game-side resource identity and buffer expectations where feasible; replace Dreamcast GD-ROM transport with a Naomi-side asset provider only after the loader contract is captured.
3. Treat Dreamcast menu code, FMV paths and VMU features as non-target unless a trace proves a dependency on them.
4. Keep the existing Naomi JVS test path separate. JVS input working on Naomi proves hardware input delivery, not that the original guest's player-control path has been entered.
5. Rehome gameplay settings such as time and difficulty into Naomi Game Test after the core round path works.

## Evidence status

| Item | Status |
|---|---|
| Dreamcast 3D attract and Start-triggered front end | User-observed |
| Attract/demo executes fight-engine workers | Trace-observed |
| Player-controlled Round 1 input path | Not yet captured |
| Character-select-to-Round-1 transition | Not yet captured |
| MT pack loader and collision initialization during transition | Identified as capture targets; transition calls unobserved |
| 132-pointer runtime table pointer chain | Unresolved |
| Naomi asset-provider compatibility contract | Not yet established |

## Sources

- Related project: [front-to-fight capture results](https://github.com/celsowm/vf3-decomp/blob/main/docs/re/front_to_fight.md)
- Related project: [fight-scene and transition investigation](https://github.com/celsowm/vf3-decomp/blob/main/docs/re/fight_scene.md)
- Related project: [fight dispatch chain](https://github.com/celsowm/vf3-decomp/blob/main/docs/re/fight_dispatch_chain.md)
- Related project: [controller merge](https://github.com/celsowm/vf3-decomp/blob/main/docs/re/controller_merge.md)
- This repository: [GDFS / MT loader trace plan](GDFS_TRACE_PLAN.md)
- This repository: [combined loader watch list](../tools/VF3_WATCH_LOADER.txt)
