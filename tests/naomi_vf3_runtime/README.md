# VF3 Naomi motion lab

This target is the first visible Naomi runtime that executes a real gameplay
kernel from the pinned VF3 C reconstruction.

It calls upstream `src/fight/mt_play.c::vf3_mt_eval_frame()` every frame and
renders the resulting 63-channel pose as a simple animated stick figure using
the Naomi PowerVR2.

The fixture is synthetic and exists only to exercise the evaluator. It is not
an extracted fighter model, production animation, or playable game. No
commercial game data is included.

Build through the `VF3 Naomi bootstrap build` GitHub Actions workflow. The
artifact is named `vf3-naomi-motion-lab-vN` and contains a versioned Naomi
ROM plus `BUILD_INFO.txt`.

This is intentionally a stepping stone: next, replace the synthetic channel
fixture with a parsed motion record and then connect it to a real fighter
model/texture asset path.
