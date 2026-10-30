# 3D Mega Hub — Master Blueprint

**Purpose:** Build one reliable, Android-first avatar studio by selectively integrating proven components from the existing repositories. This is a design and audit plan, not a claim that code has already been integrated or device-verified.

## Product target

- Primary device: Samsung Galaxy S24 SM-S921B, Exynos 2400 / Xclipse 940.
- Android-native UI and lifecycle; phone performance and memory are first-class constraints.
- Support real avatar import, generation, rigging, rendering, animation, voice, AI conversation, asset management and diagnostics.
- Prefer offline operation. Remote services and Termux/Ollama may be optional adapters, never an undocumented requirement for core app operation.
- No mock AI, placeholder generation, inert buttons, fake progress, or telemetry that reports missing measurements as zero.

## GitHub repository map

| Repository | Candidate role | Rule |
|---|---|---|
| [3D Mega Hub](https://github.com/davealone69-gif/3d-mega-hub) | Canonical host app, architecture, integration records and release evidence | Keep `main` stable; work through controlled branches |
| [3DDD](https://github.com/davealone69-gif/3DDD) | Filament viewport, glTF/GLB reader, materials, skinning, morphs, animation and export | Source looks substantial; latest observed `main` CI failed. License/build/runtime gates still required |
| [AvatarForge](https://github.com/davealone69-gif/AvatarForge) | Selected renderer-independent generation kernel and Kotlin spec/resource pieces, pinned to `work/android-engine-dev` at `9e36197ace3b18faa6c566d93f8fce89b185c45d` | Keep the C ABI/version boundary; custom renderer is not part of this transplant; host and device checks still required |
| [Aaa](https://github.com/davealone69-gif/Aaa) | SDK/provider contracts, jobs, model lifecycle, persistence and resource governance | Reuse only after source/build/licence inspection |
| [Grok-Girls](https://github.com/davealone69-gif/Grok-Girls) | Potential studio/persona workflows and provider adapters | Web/Capacitor UI is not proof of native rendering or embedded inference |
| [Aura-Studio-Avatar](https://github.com/davealone69-gif/Aura-Studio-Avatar) | Potential avatar library, settings and diagnostic patterns | Verify implementation and runtime before adoption |

## Generation engine decision

AvatarForge `afcore` is the selected generation-kernel source for the host app. The integration lives in `:avatarforge-engine`, uses the pinned ABI-v1 C interface, Kotlin avatar specification/resource checks, and exports generated GLB files to app-private storage. This selection does **not** select AvatarForge's custom renderer; rendering remains a separate adapter decision so only one renderer is activated after target-device tests.

## Renderer decision

**First path to evaluate: Filament on OpenGL ES. Vulkan is optional, not a requirement.**

The 3DDD source declares Filament Android dependencies and contains a dedicated renderer, model loader, material factory, camera, environment, offscreen capture and instrumented runtime tests. Its test suite includes checks for a real graphics backend and visible pixels, not just a nonzero FPS value. This is promising source evidence, not a successful build or proof it works on the target phone.

Before accepting the renderer, run a fresh build and its current CI tests; then on the target phone verify:
1. A known textured GLB loads with nonzero triangles and sensible bounds.
2. A captured frame contains visible model pixels, not only the clear colour.
3. Camera controls, materials, skeletal animation and morph targets work.
4. Repeated viewer open/close and background/foreground transitions remain stable.
5. Memory and thermal behaviour remain acceptable during a sustained run.
6. Failures report useful diagnostics and do not strand the UI in an endless loading state.

Do not run two renderer stacks in parallel without an explicit, tested need. AvatarForge's custom renderer remains a separate candidate until it passes the same acceptance tests.

## Proposed module boundaries

1. **Android shell:** Kotlin, Jetpack Compose, navigation, permissions and lifecycle.
2. **Rendering adapter:** one chosen renderer, isolated behind a small viewport/model API.
3. **Avatar engine adapter:** versioned native C ABI and Kotlin SDK, if compatible and licensed.
4. **Avatar data:** stable model IDs, source URI, rig/animation metadata, materials and provenance.
5. **Studio controls:** appearance, camera, lighting, animation and export.
6. **AI provider API:** explicit local/remote provider state, cancellation, streaming and persistence.
7. **Generation jobs:** durable queue, progress tied to real stages, cancellation and actionable failure reports.
8. **Voice pipeline:** speech input/output, audio focus, voice selection and timed viseme events.
9. **Library and diagnostics:** local assets, resource governor, logs and privacy-safe diagnostic export.

Chat LLM inference, image-to-3D generation, mesh/rig processing, rendering and speech are distinct subsystems. A working chat model does not prove image-to-3D generation works.

## Integration sequence

### Phase 0 — source truth and legal gate
- Record exact branch and commit for each donor.
- Inspect repository and asset licences, Gradle files, manifests, native build settings, dependencies, test suites and latest workflow results.
- No copying until the applicable licence is clear. A dependency's licence does not automatically license the host app or generated assets.
- Choose components by tests and measured behaviour, not README claims alone.

### Phase 1 — minimal host
- Kotlin/Compose Android shell, Gradle Kotlin DSL, version catalog, stable package/module boundaries.
- Diagnostics screen reports actual device capabilities, renderer backend, memory/thermal state, model and provider readiness.
- Build/test CI; CI success must remain distinct from device verification.

### Phase 2 — viewer
- Establish one renderer, one known fixture GLB and visible-frame regression tests.
- Test lifecycle, recovery, camera, materials, animation, morphs, memory and thermal limits.

### Phase 3 — avatar engine
- Integrate a small, versioned interface to generation/rigging components only after ABI compatibility, licence and Android build checks.
- Keep the renderer and generation kernel independently replaceable.

### Phase 4 — voice and facial animation
- Connect speech output to timed phoneme/viseme events and avatar morph targets.
- Test interruption, cancellation, audio focus and renderer lifecycle together.

### Phase 5 — AI and studio workflows
- Evaluate a genuinely embedded Android LLM against model licence, ABI, RAM, storage and latency.
- Keep Ollama/Termux optional. Test offline with networking disabled.
- Implement real model management, job queue, import/export, persistence and failure recovery.

### Phase 6 — release gates
- Clean checkout build, unit tests, Android instrumentation, dependency/licence review and native ABI checks.
- Produce AAB and APK with release signing configured; never commit signing secrets or silently fall back to debug signing for release.
- Install and test on the target Samsung phone. If installation/runtime access is unavailable, mark device verification **BLOCKED**, not passed.
- Record exact commit SHA, workflow run, artifact link, test outcomes and unresolved gaps.

## Status vocabulary

- **REAL + VERIFIED:** behaviour demonstrated by the named test/evidence.
- **UNVERIFIED:** code or claims exist, but the required test has not been run.
- **PARTIAL:** only a defined subset works.
- **MISSING:** no implementation found.
- **BLOCKED:** a required test/action could not be performed, with the reason stated.

## Current evidence at blueprint creation

- Host repository documentation exists; this file does not itself create an Android app.
- 3DDD has substantial Filament-related source and instrumentation tests, but the latest observed `main` workflow runs failed. It is not green yet.
- AvatarForge's default `main` and `work/android-engine-dev` are materially different branches. The intended engine branch must be audited by exact commit; default-branch README claims are not sufficient.
- No donor has been integrated into this repository by this blueprint.
- No build or physical-device verification is claimed.
