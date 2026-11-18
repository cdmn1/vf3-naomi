# VF3 Naomi bootstrap

This is the first game-project executable target, intentionally separate from
the MIE/JVS hardware probe. It does not read controller input, load commercial
assets, or claim to run VF3 gameplay.

It initializes KallistiOS/PowerVR2 and continuously renders a visible status
screen. A successful Flycast boot proves the Naomi executable packaging,
startup and basic video path independently of the input investigation.

Build with a Naomi-configured KallistiOS environment:

```sh
make rom
```

The output is `vf3_naomi_bootstrap.bin`, packaged with KOS
`naomibintool`. The GitHub Actions workflow
`.github/workflows/naomi-bootstrap.yml` caches the SH-4 toolchain, the
compiled KOS tree and the bootstrap object files. The final ROM image is
always freshly packaged and validated; it is not reused from cache.
