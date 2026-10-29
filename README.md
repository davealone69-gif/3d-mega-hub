# 3D Mega Hub

**Android-first avatar studio with an integrated AI assistant and a real 3D runtime.**

This repository is the new product host. Existing repositories remain independent donor/source projects until their code, dependencies, licences, tests, and build status have been checked. No donor is considered integrated merely because its files or documentation were copied.

## Product goals

- Render and manage customizable 3D avatars on Android.
- Integrate an AI assistant that can run on-device where a compatible model/runtime fits the target device; make network/provider backends optional rather than silently required.
- Support avatar voice, expressions, animation, and lip-sync through explicit interfaces.
- Provide avatar import/export, a persistent library, generation-job status, resource controls, and useful diagnostics.
- Prioritize the Samsung SM-S921B / Exynos 2400 target and a reliable OpenGL ES path. Vulkan remains optional until verified on the actual device.
- Ship a reproducible Android build, with an AAB as the primary release artifact and a signed APK for installation/testing. Never commit signing secrets.

## Blueprint and audit records

- [Master blueprint](docs/MASTER_BLUEPRINT.md): product architecture, renderer decision, integration phases and release gates.
- [Integration plan](docs/INTEGRATION_PLAN.md): staged work checklist.
- [Donor baseline](docs/DONOR_BASELINE.md): inspected branch commits, CI evidence, known blockers and licence questions.
- [Third-party notices](THIRD_PARTY_NOTICES.md): renderer dependency and bundled model provenance.

## Current implementation

- **Android shell:** Kotlin/Compose; CI builds and unit tests passed in [run 40924116626](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/40924116626).
- **Renderer branch:** Filament 1.71.5 with explicit OpenGL ES backend selection and a bundled CC0 base-rig GLB is under CI validation in [PR #2](https://github.com/davealone69-gif/3d-mega-hub/pull/2).
- **Avatar generation, embedded LLM, voice/lip-sync:** not integrated.
- **Physical Samsung runtime / visible-pixel evidence:** not yet verified.

## Engineering rules

1. Preserve all donor repositories and their branches.
2. Inspect the current default branch and recent commits before choosing source code.
3. Prefer tested, minimal integrations over wholesale merges.
4. Do not ship mock AI, fake controls, placeholder success states, or dummy services.
5. Show unavailable capabilities as unavailable, with actionable diagnostics.
6. Keep secrets out of source control. Do not weaken main history with force pushes.
7. A green CI build is not a green device build.

## Verification labels

- **REAL + VERIFIED**: implementation exists and the stated test was run successfully.
- **UNVERIFIED**: code or documentation exists, but the required test has not been run.
- **PARTIAL**: some parts work; named gaps remain.
- **MISSING**: not implemented.
- **BLOCKED**: a concrete dependency or access limitation prevents the next check.
