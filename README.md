# 3D Mega Hub

## Download the latest Android APK

[**⬇ Download latest debug APK**](https://github.com/davealone69-gif/3d-mega-hub/releases/latest/download/3D-Mega-Hub-debug.apk)

This link downloads the APK directly. It is refreshed automatically after each successful Android build on `main`. No Actions/workflow navigation is needed. Debug builds are for testing and are not production-signed.


**Current handover: 2026-11-18.** Integration is active on review branches; `main` is intentionally still the stable baseline. This is not a release-ready or Samsung-verified build.

## Current handover — 2026-11-18

### Git state and active work

- Default branch: `main`. The current integration PR records its base at `aaee4443dbf53ef4707cce01d99f71a59e903ec7`; no evidence in this check shows the integration has been merged to `main`.
- **PR #9** — [Wire AvatarForge GLB engine to native GLES viewport](https://github.com/davealone69-gif/3d-mega-hub/pull/9). Canonical renderer integration branch: `integration/avatarforge-renderer-port`, head `fa3f6f810f1ad586f8e51567a97f5c3fd423ef58`. Open, 383 commits / 125 files, currently reported not mergeable. It contains the AvatarForge native GLES route for imported/generated self-contained GLBs, a separate Filament bundled-sample preview, WebP decoding/fallback, post-skin deformation checks, refreshed submesh bounds, EGL/PixelCopy diagnostics, scoped FileProvider sharing and launcher icon.
- **PR #14** — [Upgrade pass 2026-11-16: toolchain, CI blockers, and app feature work](https://github.com/davealone69-gif/3d-mega-hub/pull/14). Branch: `arena/25bd4008-3d-mega-hub`, head `c4a04108c3505138add43057a3ee85dbe61fe9b2`, based on the integration branch. Open, 37 commits / 34 files, reported mergeable. It adds the toolchain migration, renderer/CI fixes, animation diagnostics, appearance controls, bounded chat history and speech settings. It is a review/compile-gate branch, not a second product.
- GitHub currently lists **20 branches**. Treat `integration/avatarforge-renderer-port` as the canonical renderer line and PR #14 as a dependent upgrade/feature change set. Do not resume stale experimental branches or create another competing implementation branch. Consolidate PR #14 into the canonical line only after exact-head validation and conflict review. Keep `main` unchanged until the merge gates pass.
- The repository's latest commit search on `main` returned 15 November documentation commits. The README on `main` had not captured the later PR #9 / PR #14 work; this section corrects that handover gap.

### Evidence and honest capability status

| Area | Current status | Evidence boundary |
|---|---|---|
| Android Kotlin compile + JVM tests | **REAL + VERIFIED on intermediate PR #14 commit(s)** | Compile gate passed Kotlin main/test compilation and 46 JVM tests on the recorded gate commit(s). This is not proof for current PR #14 head `c4a0410`. |
| Native host regression | **REAL + VERIFIED on recorded upgrade workspace** | Six host CTest suites passed; 22/25 native translation units compiled there. Three required Android/system headers were unavailable in that host environment. |
| Emulator install / launch / interaction | **REAL + VERIFIED on recorded PR #14 evidence head** | 17 Nov evidence: API 30 x86_64 emulator installed and cold-launched the app (1353 ms), resumed MainActivity, captured a 1080×1920 non-black frame, survived 150 fixed-seed monkey events without crash/ANR; PSS 133,929 KB. This evidence is tied to the recorded earlier evidence run/head, not automatically to current PR #14 head. |
| AvatarForge generation → GLB → production native GLES SurfaceView → visible pixels | **REAL + VERIFIED on emulator, recorded evidence head only** | 17 Nov `GeneratedAvatarRuntimeTest`: 4/4 instrumentation tests passed in 473.665 s, including generated GLB visible-pixel capture through production `AvatarForgeSurfaceView`. Not a Samsung-phone result and not proof for a later branch head. |
| Imported GLB on physical Samsung | **PARTIAL / DEVICE-REPORTED** | Owner-supplied app diagnostics on 16 Nov recorded one 12.6 MiB imported GLB reaching `PIXELS_VERIFIED` in 3/3 runs on GLES 3.2, 144,073 triangles, 11 draw calls, ~401k visible pixels, zero GL/EGL errors. The reports contain no APK hash or source SHA, so this is not exact-head verification. |
| Large remote GLB | **PARTIAL** | A 98.8 MiB remote GLB reached `FRAME_PRESENTED`, but no terminal `PIXELS_VERIFIED` state or matching screenshot was recorded. Its load phase took about 13.9 s and needs bounded progress/diagnostics. |
| Filament gltfio preview / imported animation | **BLOCKED on recorded emulator path** | Filament 1.80.1 emulator preview aborts in `AssetLoader.createAsset` with `PreconditionPanic`: uniform `sheenColorIndex` not found (feature level 1). This blocks the Filament preview and imported skeletal animation controls on that path. Hardware behaviour is not established by the emulator failure. |
| Procedural hair quality work | **PARTIAL / SOURCE IMPLEMENTED** | Braid geometry, curved hairline, Afro coil variation, long-hair clump channels, root-to-tip pigment gradient and region-aware hair LOD are in source. Native logic/regression coverage exists in the work record; final-head CI and visual quality/performance validation remain separate gates. |
| Appearance controls | **PARTIAL** | Sex/build, ten hair styles, seven hair colours, nine clothed outfits, six cloth colours, melanin and height are implemented in PR #14 source with JVM tests. Do not claim device-tested controls on the current head. |
| Chat context | **PARTIAL** | Bounded local-Ollama transcript (12 turns / 24k characters / 8k per message), pending state and reset are implemented with JVM tests. This does not establish a unified chat/voice character controller or verified provider connectivity on the target phone. |
| Speech | **PARTIAL** | Android TTS rate/pitch and engine-reported voice selection are implemented with JVM tests. Microphone capture, speech recognition, viseme extraction and lip-sync are missing/unconnected. |
| Unified character experience | **MISSING / PARTIAL** | Chat and TTS remain separate; no shared conversation state machine, listening/input pipeline, blinking/gaze behaviours or conversation-driven facial expression/lip-sync. |
| Physical Samsung SM-S921B runtime | **UNVERIFIED for current PR heads** | Historical owner-supplied device diagnostics are useful but lack exact APK/source identity. No claim that current PR #9 or #14 head has passed phone installation, lifecycle, animation or thermal testing. |
| Production APK/AAB signing | **BLOCKED** | Requires the owner's release keystore and protected CI secrets. Ephemeral CI/debug signing is not production signing. |

### Exact-head and merge rules

1. The PR #14 compile gate passed on an earlier commit (the branch README records 46 JVM tests; the detailed upgrade record names gate commits `26911d8` / `2e53765`). The PR #14 README records the latest verified emulator artifact at `d3a1a25`; current PR #14 head is `c4a04108c3505138add43057a3ee85dbe61fe9b2`. **Do not transfer earlier green results to the current head.** Re-run compile, JVM tests, native tests, lint, R8, APK assembly and emulator runtime on the exact proposed merge SHA.
2. PR #9 head `fa3f6f810f1ad586f8e51567a97f5c3fd423ef58` has no successful combined status returned in this check; its PR description says fresh exact-head CI is running and makes no new pass claim. Treat its current head as **UNVERIFIED** until checks are read for that exact SHA.
3. Resolve the Filament loader abort without hiding it or claiming the GLES route fixes it. Keep the native AvatarForge GLES route and Filament preview distinct; they have different capabilities and evidence.
4. A successful emulator build/runtime is not Samsung verification. The phone gate must install the exact APK and verify the actual model pixels, import, animation where supported, repeated open/close, background/resume, memory and thermal behaviour.
5. Do not merge either PR to `main` until exact-head checks pass, branch overlap is reconciled, and the owner-facing phone gate is clearly reported. No force-push.
6. Do not label an APK/AAB production-signed until the user-owned keystore is configured and the resulting artifact's signature is inspected.

### Current next engineering objective

**Close the character-rendering path before expanding feature breadth.** First reconcile PR #14 into the canonical renderer line, establish a single exact candidate SHA, rerun the full build/test/emulator matrix, then address the Filament preview blocker and the imported-animation path. Preserve the native GLES path that has actual pixel evidence. Keep the Samsung install/runtime test as a separate owner-side gate. After renderer stability, unify chat and TTS behind one conversation/session controller, then implement speech input, viseme timing, facial expression, blinking and gaze with real runtime tests.

### Evidence documents on the current PR #14 work branch

- [PR #14](https://github.com/davealone69-gif/3d-mega-hub/pull/14)
- [Runtime evidence — 2026-11-17](https://github.com/davealone69-gif/3d-mega-hub/blob/arena/25bd4008-3d-mega-hub/docs/RUNTIME_EVIDENCE_2026-11-17.md)
- [Physical-device renderer diagnostics — 2026-11-16](https://github.com/davealone69-gif/3d-mega-hub/blob/arena/25bd4008-3d-mega-hub/docs/DEVICE_RENDERER_DIAGNOSTICS_2026-11-16.md)
- [Filament gltfio blocker](https://github.com/davealone69-gif/3d-mega-hub/blob/arena/25bd4008-3d-mega-hub/docs/BLOCKER_FILAMENT_PREVIEW_GLTFIO_PANIC.md)
- [Toolchain upgrade record](https://github.com/davealone69-gif/3d-mega-hub/blob/arena/25bd4008-3d-mega-hub/docs/UPGRADE_2026-11-16.md)
- [Android renderer build and verification procedure](https://github.com/davealone69-gif/3d-mega-hub/blob/arena/25bd4008-3d-mega-hub/BUILD_AND_VERIFY.md)

---

**Android-first avatar studio with an integrated AI assistant and a real 3D runtime.**

This repository is the new product host. Existing repositories remain independent donor/source projects until their code, dependencies, licences, tests, and build status have been checked. No donor is considered integrated merely because its files or documentation were copied.

- [Adult model, render and video integration](docs/ADULT_MODEL_RENDER_VIDEO_INTEGRATION.md): Grok-Girls visual/video port scope, Blender/MPFB2 anatomy pipeline, real 3D video requirements and acceptance gates.
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

## Initial repository baseline (2026-10-29; superseded)

The bullets below describe the repository at creation, not the current project state. Use the **Current handover — 2026-11-18** section above as the current source of truth.

### Historical baseline

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



## Interim architecture and adult-mode handoff (2026-11-15)

### Keep the product together for now

Do not split 3D-Mega-Hub into multiple installable apps or prematurely create a distributed service system while core avatar outputs are still below target quality. Keep one coherent product experience for now, but maintain clean internal boundaries, explicit interfaces, ownership and independent test seams. Revisit extraction only when measured codebase complexity, release cadence, security, scaling or device-resource constraints make a split beneficial. This is a deliberate interim architecture, not permission to tangle every subsystem together.

AvatarForge remains an independently buildable engine/SDK and test lab, and 3DDD remains a donor. Integrate proven capabilities into this product without destroying their source-project identity or standalone verification paths.

### Adult-content capability inventory

- **Separate source project found:** [Grok-Girls](https://github.com/davealone69-gif/Grok-Girls) contains an 18+ toggle, adult-mode settings, age-confirmation flow, adult-specific prompt/options and chat/video routing. Its README states the adult chat path is on-device and documents self-hosted Stable Diffusion/ComfyUI image/video backends. This is a donor candidate, not an already-integrated Hub feature.
- **AvatarForge engine-side work found:** `config/customization/adult-anatomy.inputs.json`, `requirements/custom-inputs/adult-anatomy.user.json`, and `sdk/src/test/java/com/avatarforge/sdk/AdultSafetyTest.kt` exist on `work/android-engine-dev`. The config explicitly marks detailed anatomy controls `BLOCKED_ENGINE_GEOMETRY`; it is a contract, not implemented geometry. SDK tests cover explicit adult confirmation and rejection of under-18/minor prompt cases.
- **Current status: PARTIAL.** Adult-mode UX/prompt logic exists in Grok-Girls; AvatarForge has an adult-only gate/requirements contract and safety tests; the complete adult anatomical mesh and verified end-to-end adult generation are not established. Do not claim it is already in 3D-Mega-Hub.
- The user confirms Grok-Girls was authored for this project by Grok and authorises reuse. Treat its relevant model/render/video source as user-authorised; preserve the original repository and history. Do not delay the port over an assumed third-party licence issue.
- Integration direction: one optional adult-mode capability within the existing product, not a separate app. Disabled by default; explicit 18+ confirmation; reject minors and ambiguous-age prompts; support only fictional adult subjects; require consent/ownership for reference assets; protect adult assets and metadata; enforce each provider's actual policy/capabilities; do not silently fall back to a provider that cannot fulfil the requested mode. Provider support remains UNVERIFIED until real generation is tested.
- Keep adult UI controls hidden when the engine capability is missing. Implement the real anatomy geometry and rendering path in AvatarForge behind its existing contract, validate it in the engine test lab, then expose it through the Hub's existing avatar workflow. Test on production GLES and the target Samsung device before calling it complete.
