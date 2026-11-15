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

## Initial donor map (candidates, not yet integrated)

| Source repository | Candidate contribution | Required before adoption |
|---|---|---|
| [AvatarForge](https://github.com/davealone69-gif/AvatarForge) | Native avatar kernel, C ABI, Kotlin SDK, rigging/blendshapes and diagnostics | Select intended branch by exact SHA; compile native and Android targets; verify renderer on device; clarify source licence |
| [3DDD](https://github.com/davealone69-gif/3DDD) | Filament viewport, glTF/GLB, materials, skinning, animation and export | Resolve current CI failures; inspect dependency/licence compatibility and regression coverage |
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


## Non-monolithic product architecture and preserved AvatarForge baseline (2026-11-15)

**The goal is one coherent product experience, not one giant app/module/process containing everything.**

- **3D-Mega-Hub** is the end-user host/product. It owns the integrated workflow, navigation, avatar library and user-facing orchestration.
- **AvatarForge** remains a separately maintained, independently buildable engine/SDK and test lab. The Hub may consume its engine through a narrow SDK/native boundary. Do not copy its standalone app UI wholesale into the Hub or create a second competing avatar engine.
- **3DDD** and other donors remain independent source projects. Reuse selected proven components behind explicit interfaces; do not erase donor boundaries or assume copied files mean successful integration.
- Keep engine/runtime, host UI, asset pipeline, AI/provider adapters, voice, persistence and optional heavyweight services independently testable and separately packaged/process-isolated where lifecycle, dependency, permission, licensing or resource constraints require it. A single APK/AAB may package multiple modules; that does not require a monolithic source tree or single process.
- Choose packaging based on the actual user experience and measured Android constraints. Do not force every subsystem into the Hub merely to call it integrated. Do not split functionality into multiple user-facing apps without a concrete product reason either.

### Existing user-supplied AvatarForge baseline (do not request it again)

The user previously supplied:
1. A full AvatarForge source snapshot at Dropbox path `/Update/AvatarForge-fixed-3b98600/AvatarForge/`, containing the app, engine, native code, SDK, Rust implementation, ABI, tools, docs, requirements, prebuilt libraries, artifacts and build/status handover files.
2. A known-working pre-integration APK: `/AvatarForge/3d engine/AvatarForge-pre-integration-debug-f12a35acefd1 (1).apk` (**11,026,696 bytes**).

Historical handoff evidence for that APK: **5/5 app tests and 43/43 SDK tests passed**, including JobHandle retention; the handoff reported the Samsung engine visible. Preserve this binary as an immutable recovery/comparison baseline. This historical result is not a claim about today's build or current phone compatibility.

Before asking the user for a skeleton, rig, source archive or test APK, inspect the already-supplied Dropbox source snapshot and APK. Do not make the user repeat this handoff. Verify exact source and artifact identities before porting; the Dropbox snapshot is not presumed identical to current GitHub HEAD.

References: [AvatarForge source checkpoint](https://www.dropbox.com/scl/fi/bwbd3qrr432poip5epnpv) · [AvatarForge source README](https://www.dropbox.com/scl/fi/lgkron1i7uco8yhiylr4r) · [AvatarForge repository](https://github.com/davealone69-gif/AvatarForge).

