# Dreamcast VF3tb runtime flow — user observation

Date: 2026-11-16

## Observed on the Dreamcast release

The user tested the retail Dreamcast game and reported this sequence:

1. On boot, the game enters an attract sequence. It is rendered 3D gameplay-style footage, not an FMV; it appears arcade-like.
2. Pressing Start opens a Dreamcast-specific mode-selection/options interface.
3. Entering a game mode appears to resume the arcade game flow.
4. FMVs are not part of the normal attract sequence. They are manually selected bonus material inside Dreamcast-specific option menus.

These are direct observations from running the Dreamcast game. They are not yet confirmed by executable tracing.

## Working interpretation

The runtime appears to have at least two layers:

- An arcade-style game layer, evidenced by the 3D attract sequence and the return to arcade gameplay after selecting a mode.
- A Dreamcast-specific front end, evidenced by the Start-triggered mode/options interface and the manually browsed bonus FMVs.

This makes the Dreamcast front end and bonus-video system lower priority for the first Naomi gameplay milestone. The first objective should be to understand the transition from the Dreamcast front end into the arcade-style game path, then determine which parts of the game runtime and services that path actually requires.

Do not assume yet that the attract sequence and playable arcade mode are the same executable scene/state, or that the Dreamcast mode-selection layer can simply be bypassed. Confirm this with runtime tracing.

## Porting implications and next investigation

1. Identify the front-end state reached after Start and the selected game-mode transition.
2. Trace the transition into the playable/arcade-style game path, distinguishing it from autonomous attract/demo behavior.
3. Correlate that transition with resource loads, especially fighter, stage, motion and audio assets.
4. Defer bonus FMV playback and Dreamcast-only bonus menus unless tracing shows they are prerequisites for gameplay.
5. Keep the Naomi test-mode work separate; it has established a Naomi-side diagnostic path, not the game's own mode-selection behavior.

## Evidence status

- User-observed: 3D attract sequence; Start opens Dreamcast-specific mode/options interface; selecting a game mode returns to arcade-style gameplay; bonus FMVs are manually triggered in menus.
- Not yet established: exact state IDs, code paths, whether the attract and match scenes share the same engine path, and which Dreamcast services are required after the mode transition.
