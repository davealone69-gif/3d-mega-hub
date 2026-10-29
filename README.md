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

## Initial donor map (candidates, not yet integrated)

| Source repository | Candidate contribution | Required before adoption |
|---|---|---|
| [AvatarForge](https://github.com/davealone69-gif/AvatarForge) | Native rendering engine, C ABI, Kotlin SDK, scene/runtime and diagnostics | Inspect current commit and ABI; compile native and Android targets; verify GLES rendering on device |
| [3DDD](https://github.com/davealone69-gif/3DDD) | glTF/GLB, materials, skinning, animation and viewport lifecycle | Inspect current implementation, dependency/licence compatibility, and regression coverage |
| [Aaa](https://github.com/davealone69-gif/Aaa) | SDK/provider boundaries, model management, jobs, resource governance and persistence patterns | Verify working implementations and API contracts |
| [Grok-Girls](https://github.com/davealone69-gif/Grok-Girls) | Renderer regression cases and optional provider/voice adapters | Compare branch commits; keep external server integration optional |
| [Aura-Studio-Avatar](https://github.com/davealone69-gif/Aura-Studio-Avatar) | Avatar library/designer and management patterns | Verify source, build, tests, and licence |

This is a starting map from prior audits, not a claim that every repository or branch has been fully re-audited. Heavy 3D generation models are not assumed to run locally on the phone; their requirements and any remote-provider boundary must be explicit.

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

## Current baseline

- Repository created and writable: **verified**.
- Product scope and donor shortlist: **documented; source integration not yet verified**.
- Android project compilation: **not yet verified in this repository**.
- On-device renderer, embedded LLM, voice/lip-sync: **not yet verified in this repository**.
- Release signing and installable artifacts: **not yet configured or verified**.

See docs/INTEGRATION_PLAN.md for the staged integration plan.