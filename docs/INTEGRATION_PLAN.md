# Integration Plan

## Phase 0: trustworthy baseline

- [x] Confirm repository exists and is writable.
- [x] Record product scope, donor candidates, and verification rules.
- [ ] Complete exact-SHA inventory of donor branches, build files, licences, native binaries, tests, and current workflow results before adopting code.
- [ ] Record exact donor commit SHAs and licences for every adopted source file and asset.

## Phase 1: minimum viable Android shell

- [x] Kotlin/Jetpack Compose Android application, Gradle Kotlin DSL, and version catalog.
- [x] Initial `:app` boundary before importing donor code.
- [x] Capability-status screen that does not claim disconnected backends are ready.
- [x] Android CI runs unit tests, assembles a debug APK, and uploads the APK artifact.
- [ ] Extend diagnostics with measured memory pressure, thermal state, model/provider readiness, and renderer health.

Evidence: Android shell CI and unit tests are documented as passing in [workflow run 40924116626](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/40924116626). CI is not physical-device verification.

## Phase 2: renderer proof

- [x] Select Filament with explicit OpenGL ES backend selection for the first renderer proof. Vulkan is not required to start.
- [x] Add a bundled CC0 base-rig GLB fixture, touch-orbit handling, render loop, and visible status/error reporting.
- [ ] Confirm the renderer PR's latest CI build and tests pass.
- [ ] On Samsung SM-S921B, verify visible model pixels, geometry/bounds, materials, camera controls, lifecycle recovery, memory pressure, and sustained thermal behaviour.
- [ ] Add automated regression coverage for viewer attach/detach, repeated open/close, and renderer initialization failure.
- [ ] Evaluate skinning, animation, and morph-target behaviour with a known fixture before accepting the renderer as baseline.

A submitted frame is only a renderer signal, not proof that the model produced visible pixels.

## Phase 3: avatar engine

- [ ] Inspect AvatarForge's intended native C ABI, Kotlin SDK, lifecycle, licences, build settings, and tests at exact commit SHAs.
- [ ] Inspect 3DDD for glTF/GLB, materials, skinning, morph targets, animation, and lifecycle improvements. Port only compatible components with tests.
- [ ] Integrate a versioned avatar-engine adapter only after ABI, licence, Android build, and runtime checks pass.

## Phase 4: integrated AI

- [ ] Evaluate an embedded Android LLM runtime and models against target RAM, storage, ABI, licence, and latency limits.
- [ ] Keep AI behind a provider interface; an unavailable or unloaded model must be reported honestly.
- [ ] Keep Ollama/Termux optional; the app must not secretly require a local server.
- [ ] Add cancellation, streaming, conversation persistence, model lifecycle, and memory-pressure handling.
- [ ] Test offline operation. Distinguish LLM chat from image-to-3D generation, which has separate model and compute requirements.

## Phase 5: voice and avatar behaviour

- [ ] Define an event/state interface connecting assistant output, speech playback, viseme/lip-sync, facial expressions, and animation.
- [ ] Add Android speech output and an explicit voice-provider interface.
- [ ] Implement lip-sync from timed phoneme/viseme data where available; audio playback alone is not lip-sync.
- [ ] Test interruption, cancellation, audio focus, and renderer/voice lifecycle together.

## Phase 6: studio features

- [ ] Add a persistent avatar library and import/export.
- [ ] Add a durable generation-job queue with real progress, cancellation, and actionable failure details.
- [ ] Add model/provider management and a resource governor.
- [ ] Add diagnostic export with sensitive data excluded by default.
- [ ] Audit every button, route, setting, and dialog for a real action or a clearly disabled state.

## Phase 7: release gates

- [ ] Clean Gradle build from a fresh checkout.
- [ ] Unit, native, integration, dependency, and licence checks pass.
- [ ] AAB build succeeds; APK is signed using a configured key without committing secrets.
- [ ] Install and runtime-test on the target Samsung phone. If device testing cannot be performed, mark it BLOCKED and give exact user-side steps.
- [ ] Confirm visible geometry, offline AI inference, and synchronized voice/lip-sync before calling those features verified.
- [ ] Publish exact commit, workflow run, artifact link, test outcomes, and remaining gaps.

## Donor rules

No wholesale repository merges. For each adopted component, record source repository, branch and commit SHA, licence, files/dependencies adopted, local modifications, tests run, and reason for inclusion. If a component cannot be compiled or verified, keep it isolated and label it UNVERIFIED.
