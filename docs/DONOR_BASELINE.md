# Donor Baseline Snapshot

Checked against the repositories' current default branches on 2026-10-30. This is a README/metadata pass, not a full source audit or build.

## Repositories

### AvatarForge
- Default branch observed: `main`.
- Candidate role: primary native engine, C ABI, Kotlin SDK, renderer and diagnostics.
- Its current README describes a fully offline Android engine and reports a host test harness passing 386 checks. This differs from an earlier project note mentioning 405 checks. Do not reuse either count as current verification until the exact commit and test command are run.
- Known target-device concern: Vulkan has not been verified on the user's Samsung SM-S921B; prioritize GLES and visible-render verification.
- Status for 3D Mega Hub: **candidate only; not imported or compiled here**.

### 3DDD
- Default branch observed: `main`.
- Candidate role: compare its Filament viewport lifecycle, glTF/GLB loading, materials, morph targets, skinning, animation and export behavior.
- Do not mix its Filament stack with AvatarForge's native renderer without a deliberate boundary and measured reason. Choose the smallest working path first.
- Status for 3D Mega Hub: **candidate only; not imported or compiled here**.

### Aaa
- Default branch observed: `main`.
- Its current README says the SDK, sample APK/AAB and tests build in its reported sandbox, while physical-device rendering and generation remain unverified and multiple avatar/generation features remain incomplete.
- Candidate role: SDK/provider contracts, model lifecycle, jobs, persistence and truthful unavailable states.
- Status for 3D Mega Hub: **candidate only; not imported or compiled here**.

### Grok-Girls
- Default branch observed: `main`.
- Its README describes a React/Vite client-side app with Capacitor packaging and persona/studio workflows. This is not automatically a native Android engine or embedded LLM runtime.
- Candidate role: inspect specific useful UI flows, regression cases and optional provider adapters; compare commits and dependencies before adoption.
- Status for 3D Mega Hub: **candidate only; not imported or compiled here**.

### Aura-Studio-Avatar
- Default branch observed: `main`.
- Its README describes an Android local-first avatar lab with model selection, gallery, settings and diagnostics. Those descriptions require source/build verification.
- Candidate role: inspect avatar management and diagnostics patterns; avoid adopting heavyweight model assumptions without device measurements.
- Status for 3D Mega Hub: **candidate only; not imported or compiled here**.

## Next evidence required

1. Record exact default-branch commit SHAs for every donor.
2. Inspect Gradle settings/build files, manifests, native build configuration, licences, model licences, tests, and dependency graphs.
3. Run build/test workflows or inspect their exact current workflow runs and artifacts.
4. Compare duplicate renderer stacks and choose one primary path; avoid merging two engines just because both exist.
5. Evaluate an embedded Android LLM runtime with actual target-device memory, model-size, latency, and offline tests. Ollama/Termux remains optional.
6. Do not label a feature REAL + VERIFIED until its specific test evidence exists. Physical-device tests remain separate from CI.

## Important limitation

This snapshot uses repository metadata and README content only. It is not a line-by-line audit, licence clearance, successful build, integration, or phone test.