# Project Status

Last updated: 2026-11-19.

## Active work

- Repository: `davealone69-gif/3d-mega-hub`
- Active review: [PR #14](https://github.com/davealone69-gif/3d-mega-hub/pull/14)
- Head branch: `arena/25bd4008-3d-mega-hub`
- Base: `integration/avatarforge-renderer-port`
- `main` remains unchanged. PR #14 is not merged.

## Changes committed to PR #14

- **FIXED IN SOURCE, CI UNVERIFIED:** Android build workflows now install API 37 to match `compileSdk = 37` in both Android modules. Four workflows previously installed API 36 only.
- **IMPLEMENTED, RUNTIME UNVERIFIED:** Filament now queries the selected backend's supported feature level and activates that supported level before constructing gltfio loaders. Filament otherwise defaults to feature level 1. This does not force unsupported capabilities and is not proven to fix the historical imported-GLB native abort.
- **IMPLEMENTED:** Filament dependency pin is 1.79.0 as a controlled rollback experiment from the previously crashing 1.80.1 version. No runtime result yet.
- **DOCS CORRECTED:** the Filament blocker document now labels the native abort as observed but its root cause unproven; feature-level/material mismatch remains a hypothesis.

## Evidence ledger

- **REAL + VERIFIED on prior exact CI emulator commit only:** `GeneratedAvatarRuntimeTest` recorded 4 tests, 0 failures on `392ea220c6b5d8fc25a989be58f78148e514e40a`. The production AvatarForge GLES SurfaceView test presented 3 frames, 29 draw calls and 1,642,936 triangles. This is not evidence for later commits and does not cover Filament.
- **UNVERIFIED:** latest PR head has no associated Actions runs or commit status checks. No exact-head compile, unit-test, lint, R8, APK/AAB, emulator, Filament, or runtime pass is established.
- **BLOCKED / UNVERIFIED:** Filament imported-GLB preview previously aborted in native gltfio on 1.80.1 with missing `sheenColorIndex`. Current 1.79.0 pin and supported-feature-level selection have not been tested together. Dedicated imported-GLB Filament instrumentation is still required.
- **UNVERIFIED:** Samsung SM-S921B runtime for the current PR head. Historical owner/device reports are not tied to this exact source SHA.
- **BLOCKED:** production APK/AAB signing until the owner configures a production keystore and credentials in protected CI secrets.
- **NOT READY:** PR #14 remains open and GitHub reports it not mergeable. Do not merge until exact-head CI, both renderer gates, artifact inspection and review pass.

## Architecture guardrails

- Native AvatarForge OpenGL ES SurfaceView is the primary renderer for imported and generated GLBs.
- Filament remains a separate preview path for imported glTF animation and PBR.
- Vulkan is not the phone-first renderer target.
- Do not add a third renderer or merge competing app architectures.

## Next required work

1. Trigger GitHub Actions on the exact PR head and fix the real compile/build/test failures shown by those runs.
2. Run the generated-GLB-to-production-GLES SurfaceView instrumentation gate on that same SHA.
3. Run a dedicated imported-GLB test through `FilamentPreviewView` on the current 1.79.0 pin; capture logs and prove the process survives asset creation, resource upload and visible frames.
4. Download APK/AAB artifacts from the same passing SHA and inspect package ID, ABI contents, signatures/certificate fingerprints and SHA-256.
5. Install that exact APK on Samsung SM-S921B and test launch, generation, import, appearance controls, animation and sustained rendering.
6. Configure the owner-controlled production signing identity before production release.

Update this file and README after each meaningful verified milestone. Never promote an unverified change to a fixed or green status.
