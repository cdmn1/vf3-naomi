# VF3 Naomi MT parser smoke test

This target builds and executes the real `mt_parse()` and `mt_each()`
implementation from the pinned `celsowm/vf3-decomp` source revision (`208e2e2402743123a4ba79c1733f479bec9f4906`).

It constructs a small synthetic MT file in RAM with one valid slot and one
four-byte record, calls the upstream parser, iterates the record, and checks
the parser's short-input rejection path. No commercial ROM or game asset is
included or needed.

A successful Flycast boot with `MT MOTION PARSER: PASS` proves that this
specific VF3 asset-parser code was cross-compiled for Naomi and executed
there. It does not yet prove real asset loading, motion playback, or gameplay.

Build with:
- a Naomi-configured KallistiOS environment;
- `VF3_DECOMP_DIR` pointing at the exact upstream source checkout.

The CI workflow pins the upstream commit and includes that commit in the
object-cache key. The final Naomi ROM is packaged afresh with
`naomibintool`.
