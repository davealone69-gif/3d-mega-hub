# 3D Mega Hub

**Android-first avatar studio foundation with a selected native avatar-generation core.**

This repository is the canonical product host. Donor repositories remain independent source projects unless a component, its dependencies, licence, tests, and build status have been checked. Copying files alone is not considered integration.

## Product goals

The planned studio will render and manage customizable avatars, provide an optional AI assistant, support voice, expressions, animation and lip-sync, and include import/export, a persistent library, job status and resource controls. These are goals, not all current features. AvatarForge's generation kernel is the first integrated engine component; there is not yet a generation screen or selected model viewer. Prioritize the Samsung SM-S921B / Exynos 2400 and a tested OpenGL ES path. Do not treat host or CI builds as phone verification.

## Blueprint and audit records

- [Master blueprint](docs/MASTER_BLUEPRINT.md): architecture, renderer decision, phases and release gates.
- [Integration plan](docs/INTEGRATION_PLAN.md): staged implementation checklist.
- [Donor baseline](docs/DONOR_BASELINE.md): branch, commit, CI, blockers and licence evidence.
- [AvatarForge integration](docs/AVATARFORGE_INTEGRATION.md): source pin, module boundary, build evidence and remaining device gates.

## Selected donor map

| Source | Candidate contribution | Current decision / next gate |
|---|---|---|
| [AvatarForge](https://github.com/davealone69-gif/AvatarForge) | Renderer-independent C++ generation kernel; Kotlin spec and resource controls | Integrated from `work/android-engine-dev` at `9e36197ace3b18faa6c566d93f8fce89b185c45d`; clean Android/NDK debug and minified release builds pass locally. On-phone generation remains unverified; no root donor `LICENSE` was found. |
| [3DDD](https://github.com/davealone69-gif/3DDD) | Filament/glTF viewport, materials, skinning, animation and export | Keep as a separate viewer candidate. The inspected Android CI run failed; resolve build, lifecycle and licensing gates before importing it. |
| [Aaa](https://github.com/davealone69-gif/Aaa) | SDK/provider boundaries, model management, jobs, resource governance and persistence patterns | Re-audit specific implementations and API contracts before adoption. |
| [Grok-Girls](https://github.com/davealone69-gif/Grok-Girls) | Renderer regression cases and optional provider/voice adapters | Compare branches and keep external service integration optional. |
| [Aura-Studio-Avatar](https://github.com/davealone69-gif/Aura-Studio-Avatar) | Avatar library/designer and management patterns | Verify source, build, tests and licence before adoption. |

The prior repository scan enumerated 99 accessible repositories and identified 17 likely first-party 3D/avatar projects. That shortlist is not a claim that every branch and dependency has been fully re-audited. Heavy generation is not assumed to run locally on the phone; compute and provider requirements must be explicit.

## Build and verification

With Android SDK, NDK `26.1.10909125`, and CMake `3.22.1` installed, run `./gradlew clean testDebugUnitTest lintDebug assembleDebug assembleRelease`. The current local clean build passed with eight Kotlin unit tests, lint, debug APK assembly, and minified release assembly for `arm64-v8a` and `x86_64`. Release signing/AAB output is not configured. The sandbox had no attached Android device or emulator, so generation, cancellation under load, and rendering on the Samsung target remain unverified.

## Engineering rules

1. Preserve donor repositories and their branches; prefer minimal, pinned integrations.
2. Do not ship mock AI, fake controls, placeholder success states or dummy services.
3. Report unavailable capabilities accurately and provide actionable diagnostics.
4. Keep secrets out of source control. Do not force-push shared branches.
5. A green build is not proof of correct device behavior.

## Verification labels

- **REAL + VERIFIED**: implementation exists and the stated test passed.
- **PARTIAL**: some components work; named gaps remain.
- **UNVERIFIED**: code exists, but the required test has not been run.
- **BLOCKED**: a concrete access or dependency limit prevents the next check.
- **MISSING**: not implemented.

## Current baseline

- Android shell and capability diagnostics: present.
- AvatarForge native generation core: integrated; clean local build and unit/lint checks pass; no generation UI or target-phone run yet.
- Renderer/viewer, embedded LLM, voice and lip-sync: not integrated.
- Release signing and AAB: not configured.
