# Project Status

Last updated: 2026-11-19.

## Active work

- Repository: `davealone69-gif/3d-mega-hub`
- Active review: [PR #14](https://github.com/davealone69-gif/3d-mega-hub/pull/14)
- PR head branch: `arena/25bd4008-3d-mega-hub`
- Integration base: `integration/avatarforge-renderer-port`
- Latest known head before this status update: `69c95c6e40b4bb7517ba223628c52a8c57f3a0a4`; fetch the PR again for the authoritative current head.
- `main` has not been modified or merged.

## Current fixes and evidence

- **FIXED IN SOURCE, CI UNVERIFIED:** four workflows now install Android platform API 37, matching `compileSdk = 37` in both Android modules. The previous fixed package was API 36 and could not satisfy the declared compile SDK.
- **REAL + VERIFIED on prior exact emulator commit only:** `GeneratedAvatarRuntimeTest` reported 4 tests, 0 failures on `392ea220c6b5d8fc25a989be58f78148e514e40a`. The production AvatarForge GLES SurfaceView test presented 3 frames, 29 draw calls and 1,642,936 triangles. This evidence does not transfer to later commits and does not cover Filament.
- **UNVERIFIED:** the current PR head has no associated Actions runs or commit status checks. No exact-head compile, APK, AAB, lint, R8, emulator or runtime pass is established.
- **BLOCKED / UNVERIFIED:** Filament imported-GLB preview previously aborted in native gltfio on Filament 1.80.1 with missing `sheenColorIndex`. Current PR pins 1.79.0 as an experiment. No exact-head imported-GLB test proves the rollback works. Root cause remains unproven; see `docs/BLOCKER_FILAMENT_PREVIEW_GLTFIO_PANIC.md`.
- **UNVERIFIED:** Samsung SM-S921B runtime for the current PR APK. Prior owner/device reports are not tied to this exact source SHA.
- **BLOCKED:** production APK/AAB signing until the owner configures a production keystore and credentials in protected CI secrets. Do not use ephemeral preview keys as production signing.
- **NOT READY:** PR #14 is open and GitHub reports it is not mergeable. Do not merge until exact-head CI, renderer tests, artifact inspection and review pass.

## Renderer architecture

- Native AvatarForge OpenGL ES SurfaceView remains the primary renderer for imported and generated GLBs.
- Filament remains a separate preview path for imported glTF animation and PBR.
- Vulkan is not the phone-first renderer target.
- Do not replace these paths with a third renderer or merge competing app architectures.

## Next required work

1. Get GitHub Actions to execute on the exact PR head; inspect every job result and fix actual compiler/build/test failures.
2. Add or run a dedicated instrumentation test that opens the bundled/imported GLB through `FilamentPreviewView` on the current Filament 1.79.0 pin. Capture logcat and prove the process survives asset creation, resource upload and visible rendering.
3. Keep the generated-GLB-to-production-GLES SurfaceView test as a separate required gate.
4. Download and inspect APK/AAB artifacts from the same passing SHA: package ID, ABI contents, signature/certificate and SHA-256.
5. Install that exact APK on Samsung SM-S921B and verify launch, generation, import, appearance controls, animation and sustained rendering. Until this phone test passes, device status remains UNVERIFIED.
6. Configure the owner-controlled production signing identity before any production release.

Update this file and README after each meaningful verified milestone. Never promote an unverified change to a fixed or green status.
