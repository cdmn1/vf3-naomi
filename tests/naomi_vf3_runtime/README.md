# VF3 Naomi real MT source-row and evaluator probe

This ROM embeds the verified MTAKIPAI.BIN asset and uses the pinned
celsowm/vf3-decomp MT reader and native motion evaluator.

For motion IDs 1050 and 2386 it:
- parses the real 8,880-slot table;
- expands the 16 packed bytes at source component +2..+17 into 63 channel
  opcodes (the 64th decoded 2-bit value is ignored);
- reads the count bytes beginning at source component +18;
- bounds the key-time and tuple streams using the opcode/count layout; and
- evaluates all 63 channels at four Q8 phases using the traced cycle test
  input of 60.

The workflow also compiles and runs the exact same upstream parser/evaluator
on the downloaded binary with a host-side validator. Its
MOTION_EVAL_AUDIT.txt report records the source offsets, opcode counts,
stream lengths, and finite output ranges. The Naomi screen reports the same
motion rows and evaluator result.

This is a substantial step beyond component-boundary validation: the real
source streams are decoded and the 63-channel evaluator is exercised. It
still does not map those channels to POL model bones/parts, prove the
runtime cycle for every action, or render a moving fighter. The old
mt_mount.c helper remains a synthetic range fixture and is not used as the
live loader.

Build through the VF3 Naomi bootstrap build GitHub Actions workflow. The
artifact is named vf3-naomi-mt-asset-probe-vN and contains a versioned
Naomi ROM, BUILD_INFO.txt, MT_REFERENCE_AUDIT.txt, and
MOTION_EVAL_AUDIT.txt.
