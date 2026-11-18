# Build checkpoints and asset-cache policy

Date: 2026-11-16

## Daytona lessons carried into VF3

The VF3 port must report and validate these as separate outcomes:

1. Source/toolchain build completed.
2. Expected Naomi image was packaged and has the correct format/signature.
3. Image boots in Flycast.
4. Gameplay and player input work.
5. Physical Naomi/JVS/NetDIMM hardware works.

A successful compile or uploaded artifact is not proof of boot or gameplay. Every workflow must verify the exact output path, filename, nonzero size, expected format and archive contents before uploading. Keep artifact names/version metadata explicit; never assume an artifact exists because a job reached its final step.

Other confirmed Daytona lessons:
- Check workflow name, default branch, trigger and dispatch inputs before asking the user to run it.
- Check artifact paths, names, sizes and ZIP contents; missing paths and 404s wasted iterations.
- Respect GitHub Actions spending limits and avoid unrelated/full rebuilds.
- Fix the failing stage before rerunning the whole pipeline.
- Naomi images must use the KallistiOS Naomi packaging path (including `naomibintool` where required), not an unverified raw `objcopy` flat binary.
- Keep emulator boot, game behavior, input and physical hardware validation as separate test gates.
- The Daytona MIE Retail guard workaround and JVS-to-Maple findings are reusable, but do not imply that VF3's original player-input path is solved.

## Current workflow checkpoints

`.github/workflows/naomi-mie-probe.yml` now has:
- SH-4 toolchain cache, saved immediately after the toolchain stage;
- a KallistiOS source/build-tree cache keyed by the checked-out KOS commit and workflow contents;
- the KOS cache saved immediately after the KOS build, before the VF3 probe compile/package stage.

This explicit save timing is intentional: the normal cache post-job save would be skipped when a later step fails. A subsequent run can therefore restore completed expensive stages rather than repeating them.

The current cache applies to the MIE/JVS probe workflow. It is groundwork, not yet the complete VF3 game-image asset pipeline.

## Required design for the first full VF3 image workflow

Split the pipeline into independently verifiable stages with stable checkpoint boundaries:

1. Validate inputs and tool versions.
2. Restore/build SH-4 toolchain.
3. Restore/build KallistiOS Naomi libraries.
4. Decode/validate source ROM inputs.
5. Generate converted game assets.
6. Compile/link VF3 runtime.
7. Package with the correct Naomi image tool.
8. Validate image structure, signature, size and alignment.
9. Upload the final deliverable and a build manifest.
10. Run emulator tests separately; hardware validation remains a separate gate.

Each expensive stage must save its cache/checkpoint immediately after it succeeds, not at the end of the entire job. Later stages should restore the preceding checkpoint and run only when their required inputs or configuration changed.

### Cache keys

Use content-addressed keys, not just branch names:

- Toolchain: runner OS/architecture + compiler/toolchain version + build options.
- KOS: KOS commit SHA + Naomi configuration + local patch-set hash + toolchain identity.
- Converted assets: source ROM SHA-256 + asset-converter scripts SHA-256 + converter/tool versions + asset configuration.
- Runtime objects: source revision + compiler/toolchain identity + build flags + generated headers/assets manifest.
- Final image: never treat as an incremental cache; package from verified stage outputs and attach a manifest.

Use a cache version suffix when the cache layout or build procedure changes. Record cache hit/miss and the exact key in the build summary.

### ROM-derived asset confidentiality

The VF3 repository is public. Never place plaintext source ROMs or plaintext ROM-derived assets in public workflow artifacts or public-repository caches.

For resumable asset caching, package the generated asset tree, encrypt it with a repository secret, and cache/upload only the encrypted archive. Decrypt it only inside trusted workflow jobs that need it. Do not put the encryption key in the repository, workflow file, build logs, artifact metadata or cache key. Keep plaintext only in the ephemeral runner workspace and remove it after use.

A cache hit must still validate the encrypted archive, its manifest and the decrypted asset manifest before reuse. If validation fails, discard that checkpoint and regenerate it.

### Checkpoint manifest

Every saved stage should include a machine-readable manifest with:
- stage name and schema version;
- source commit;
- source ROM SHA-256 identifiers (hashes only, never ROM contents);
- toolchain and dependency commit/version identifiers;
- build flags and Naomi target configuration;
- converter/build-script hashes;
- output filenames, byte sizes and SHA-256 hashes;
- whether the output is encrypted;
- producing workflow run ID and timestamp.

A restored checkpoint is reusable only when all required manifest inputs match. Never silently reuse assets produced by different source inputs or tool versions.

## Resume behavior

The eventual full build should expose an explicit stage selector (for example, `all`, `assets`, `runtime`, `package`, `validate`) or separate reusable workflows. A stage must fail clearly when its prerequisite checkpoint is absent or incompatible; it must not silently start a costly full rebuild. The default should reuse valid checkpoints and build only missing/invalid stages.

Do not rely on rerunning a failed job to preserve its filesystem: GitHub-hosted runners are ephemeral. Persist reusable outputs explicitly as caches or artifacts, and save them immediately after each completed stage.

## Evidence / current status

| Capability | Status |
|---|---|
| SH-4 toolchain cache in VF3 probe workflow | Implemented; explicit early save |
| KOS source/build cache in VF3 probe workflow | Implemented; keyed by KOS commit and workflow |
| ROM-derived VF3 asset cache | Design specified; not implemented yet |
| Encrypted asset checkpoint/restore | Design specified; not implemented yet |
| Separate full-image build stages | Not implemented; no full VF3 image workflow yet |
| Build manifest and stage selector | Required for full image workflow; not implemented yet |



## Bootstrap build implementation (2026-11-18)

The first independent Naomi executable target is now implemented in
`tests/naomi_bootstrap/`, with a dedicated
`.github/workflows/naomi-bootstrap.yml` workflow.

The workflow reuses the SH-4 toolchain cache and restores the existing
Naomi KOS source/build checkpoint when its KOS commit matches. It also has a
separate object/ELF cache keyed by the KOS revision, Naomi target and complete
bootstrap source/Makefile content. The toolchain, KOS tree and object cache
are persisted independently; the final `.bin` is always packaged afresh
through `naomibintool` and receives a build manifest containing its size and
SHA-256. This prevents a failed later stage from throwing away earlier
expensive work.

The asset stage is not yet implemented because this repository does not yet
have a Naomi asset-conversion pipeline. When it is added, ROM-derived
checkpoints must follow the encrypted-cache policy above. Do not cache or
publish plaintext commercial assets in this public repository.
