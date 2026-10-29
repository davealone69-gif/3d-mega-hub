# Integration Plan

## Phase 0: establish a trustworthy baseline

- [x] Confirm this repository exists and is writable.
- [x] Record the product scope, donor candidates, and verification rules.
- [ ] Inventory each donor's default branch, recent commits, build files, licenses, native binaries, tests, and working-tree assumptions.
- [ ] Record exact donor commit SHAs before copying code.

## Phase 1: choose the minimum viable Android shell

- [ ] Create a Kotlin/Jetpack Compose Android application using Gradle Kotlin DSL and a version catalog.
- [ ] Establish package/module boundaries before importing native code.
- [ ] Add a basic diagnostic screen that reports actual runtime capability, memory pressure, renderer selection, and model/provider state.
- [ ] Configure CI to compile and test; do not label CI success as phone verification.

## Phase 2: integrate the 3D engine

- [ ] Inspect AvatarForge's current native C ABI, Kotlin SDK, render lifecycle, licensing, and tests.
- [ ] Select a working GLES path first for the Samsung SM-S921B target. Vulkan is optional until real device evidence exists.
- [ ] Inspect 3DDD for specific glTF/GLB, material, skinning, morph-target, animation, and lifecycle improvements. Port only compatible components with tests.
- [ ] Add a deterministic sample scene and a render-health test. A nonzero FPS counter alone is not proof that a model is visible.
- [ ] Verify model import, repeated viewer open/close, background/foreground, context loss, memory pressure, and thermal fallback.

## Phase 3: integrated AI

- [ ] Evaluate an embedded Android LLM runtime and models against the target phone's RAM, storage, ABI, license, and latency limits.
- [ ] Keep the AI provider behind an interface; an unavailable or unloaded model must be reported honestly.
- [ ] Make Ollama/Termux an optional adapter only. The default in-app experience must not secretly require a local server.
- [ ] Add cancellation, streaming, conversation persistence, model lifecycle, and memory-pressure handling.
- [ ] Test offline operation with network disabled. Distinguish LLM chat from image-to-3D generation, which has separate model and compute requirements.

## Phase 4: voice and avatar behavior

- [ ] Define a common event/state interface connecting assistant output, speech playback, viseme/lip-sync, facial expressions, and animation.
- [ ] Add Android speech output and an explicit voice-provider interface.
- [ ] Implement lip-sync from timed phoneme/viseme data where available; do not claim lip-sync merely because audio plays.
- [ ] Test interruption, cancellation, audio focus, and renderer/voice lifecycle together.

## Phase 5: studio features

- [ ] Add persistent avatar library and import/export.
- [ ] Add job queue with real progress, cancellation, and durable failure details.
- [ ] Add model/provider management and resource governor.
- [ ] Add diagnostic export with sensitive data excluded by default.
- [ ] Audit every button, route, setting, and dialog for a real action or a clearly disabled state.

## Phase 6: release gate

- [ ] Clean Gradle build from a fresh checkout.
- [ ] Unit, native, integration, and dependency/licence checks pass.
- [ ] AAB build succeeds; APK is signed using a configured key without committing secrets.
- [ ] Install and runtime-test on the actual target Samsung phone. If installation cannot be performed through available tools, mark device verification BLOCKED and give exact user-side steps.
- [ ] Confirm renderer produces visible geometry, AI inference completes offline, and voice/lip-sync is synchronized before calling those features verified.
- [ ] Publish exact commit, workflow run, artifact link, test results, and remaining gaps.

## Donor rules

No wholesale repository merges. For each donor component, record source repository, source branch and commit SHA, licence, files/dependencies adopted, local modifications, tests run, and reason for inclusion. If a component cannot be compiled or verified, label it UNVERIFIED and keep it isolated rather than presenting it as integrated.