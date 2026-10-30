# Integration Plan

## Phase 0: source and provenance

- [x] Establish the Android host and record donor provenance.
- [x] Keep the AvatarForge generation core isolated in `:avatarforge-engine`.
- [x] Keep the Filament renderer as a separate replaceable viewer.
- [ ] Refresh the AvatarForge donor pin after the current donor fixes are pushed and reviewed.

## Phase 1: Android host and engine module

- [x] Kotlin/Jetpack Compose host app and Gradle Kotlin DSL.
- [x] Isolated AvatarForge C++/JNI generation module.
- [x] User-triggered prompt generation with cancellation and resource/thermal gates.
- [ ] Run combined unit tests, lint, native compilation, and APK packaging on the exact integration candidate.
- [ ] Verify a bounded generation on the Samsung SM-S921B and record resource snapshot, GLB size, triangles, timing and failures.

## Phase 2: real model viewer

- [x] Filament startup with OpenGL ES primary and Vulkan fallback.
- [x] gltfio loader for bundled sample and self-contained GLB assets.
- [x] Bounds-based camera framing and first glTF animation playback.
- [ ] Combined CI and R8/minification pass.
- [ ] On Samsung SM-S921B, verify visible model pixels, materials, animation, camera controls, attach/detach, repeated open/close, memory pressure and sustained thermal behavior.
- [ ] Add viewer lifecycle and malformed-file regression tests.
- [ ] Add camera orbit/zoom controls after stable rendering is confirmed.

A submitted frame or successful gltfio resource upload is not proof that model pixels are visible.

## Phase 3: integrated AI

- [ ] Evaluate embedded Android LLM runtimes and models against target RAM, storage, ABI, licence and latency.
- [ ] Keep the provider interface honest: unloaded or unavailable models are not shown as ready.
- [ ] Keep Ollama optional; the app must not secretly require a local server.
- [ ] Add cancellation, streaming, conversation persistence, model lifecycle and memory-pressure handling.
- [ ] Test offline operation.

## Phase 4: voice and avatar behaviour

- [ ] Define a timed event interface connecting assistant output, speech playback, visemes, facial expressions and animation.
- [x] Android TextToSpeech output.
- [ ] Viseme extraction and avatar lip-sync.
- [ ] Test interruption, cancellation, audio focus and renderer/voice lifecycle together.

## Phase 5: studio features

- [x] Import and persist self-contained GLB assets.
- [ ] Durable generation-job queue with real progress, cancellation and actionable failure details.
- [ ] Model/provider management and resource governor UI.
- [ ] Diagnostic export with sensitive data excluded by default.
- [ ] Audit every button, route, setting and dialog for a real action or clearly disabled state.

## Phase 6: release gates

- [ ] Clean Gradle build from a fresh checkout.
- [ ] Unit, native, integration, dependency and licence checks pass.
- [ ] Debug APK assembled and SHA-256 recorded.
- [ ] Signed AAB/APK built only when a valid release keystore is configured.
- [ ] Install and runtime-test on the Samsung phone. If unavailable, mark BLOCKED/UNVERIFIED and provide exact user-side steps.
- [ ] Confirm visible geometry, on-device generation, animation, offline AI inference and synchronized voice/lip-sync before calling those features verified.
- [ ] Publish exact commit, workflow run, artifact link, test outcomes and remaining gaps.

## Donor rules

No wholesale repository merges. For each adopted component, record source repository, branch and commit SHA, licence, files/dependencies adopted, local modifications, tests run and reason for inclusion. If a component cannot be compiled or verified, keep it isolated and label it UNVERIFIED.
