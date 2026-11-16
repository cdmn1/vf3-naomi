# Naomi port scope — arcade restoration

Date: 2026-11-16

## Project objective

Restore as much as possible of the original VF3 arcade game on Naomi. The Dreamcast release is a source/reference for the arcade game, but Dreamcast-specific presentation and peripheral features are not part of the target.

## Preserve / restore

- Arcade-style 3D attract sequence.
- Character selection and playable game flow.
- Arcade gameplay, game logic, presentation, assets, sound and other original arcade behavior wherever feasible.
- Relevant game configuration such as time and difficulty.

## Rehome configuration

Gameplay settings exposed by the Dreamcast menus (for example, time and difficulty) are not to be discarded. They should eventually be exposed through the Naomi Game Test mode instead of recreating the Dreamcast-specific menus.

The exact setting list, defaults, ranges and persistence behavior are deferred until the core game is running.

## Exclude from the Naomi project

- Dreamcast-specific mode-selection/options front end.
- Bonus FMV playback and its menu paths.
- VMU features and menus.

## Architectural direction

Treat Dreamcast-specific front-end, FMV and VMU code as optional/non-target functionality unless reverse-engineering proves a dependency on it for the arcade game path.

Prefer preserving/reusing the original arcade game code and assets over rewriting equivalent behavior. Identify the transition from the Dreamcast wrapper into the arcade game path, then reproduce the required entry/setup on Naomi.

## Immediate technical priorities

1. Trace the path from boot/attract into the Dreamcast mode menu and the selected game mode, separating arcade game initialization from Dreamcast wrapper logic.
2. Identify the minimum initialization sequence and runtime services needed to enter the arcade game path on Naomi.
3. Confirm which attract, character-select, stage, fighter, motion, sound and HUD resources are required by that path.
4. Reuse the existing Naomi test-mode work as the future configuration interface; defer individual settings until core gameplay is restored.
5. Establish staged milestones: arcade attract → character select → playable round → complete match flow.

## Evidence discipline

User-confirmed behavior: the Dreamcast release starts with a 3D arcade-style attract sequence; Start opens Dreamcast-specific mode/options menus; selecting a game mode returns to arcade-style gameplay; bonus FMVs are manually launched from menus.

Do not assume the arcade path is a single function or can be entered by simply skipping a menu. Trace and verify the actual initialization and dependencies.
