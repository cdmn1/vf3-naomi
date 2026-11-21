# VF3 Naomi real MT motion-component probe

This Naomi ROM embeds the verified `MTAKIPAI.BIN` asset and inspects the
three slot components selected by the two motion IDs observed in the actor
contexts (1050 and 2386).

For each motion it derives component boundaries from the next greater unique
file offset, validates component sizes (28, 16 and 404 bytes), checks the
descriptor's in-file reference, and verifies all 101 floats in the third
component are finite and within the probe's sanity bound.

The ROM displays the actual component offsets, descriptor words, referenced
file offset, and float range on the Naomi PowerVR2.

This is real game data, not a synthetic fixture. The component field meanings,
mount-time relocation, evaluator header construction, and real pose playback
are still open. The probe deliberately does not label the components as
counts, times, opcodes, bones, or transforms without evidence.

Build through the `VF3 Naomi bootstrap build` GitHub Actions workflow. The
artifact is named `vf3-naomi-mt-asset-probe-vN` and contains a versioned
Naomi ROM plus `BUILD_INFO.txt`.
