# Engine integration candidate

Date: 2026-10-31

Candidate branch: `work/engine-integration-candidate`

## Integrated source

- Renderer base: `work/filament-renderer-live-diagnostics` at `2a1352c5994a42a586a929d995a33fcb4f6ebd67`.
- AvatarForge host module: copied from `work/avatarforge-core-integration` at `731bd31bf431cf391cb052d1c06d49e0c7255ba9`.
- AvatarForge donor pin recorded by the source integration: `work/android-engine-dev` at `9e36197ace3b18faa6c566d93f8fce89b185c45d`.
- Filament Android and gltfio: `1.71.5`, Apache 2.0.
- Bundled viewer fixture: `app/src/main/assets/models/female.glb`, documented in `THIRD_PARTY_NOTICES.md`.

## Scope

- Adds the isolated `:avatarforge-engine` Android library module with its JNI bridge and C++ generation kernel.
- Adds an explicit on-device prompt generation action with cancellation and the engine's existing RAM/storage/thermal gates.
- Adds a Filament gltfio path for the bundled sample and self-contained GLB files.
- Plays the first glTF animation when present.
- Prefers OpenGL ES; Vulkan is an optional fallback.
- Does not claim photo-to-identity reconstruction, an embedded LLM, external-resource glTF support, or lip-sync.

## Verification

- Original renderer branch CI: passed on `2a1352c5994a42a586a929d995a33fcb4f6ebd67`.
- Original core branch CI: passed on `731bd31bf431cf391cb052d1c06d49e0c7255ba9`.
- Combined candidate CI: PENDING.
- Fresh APK build and SHA-256: PENDING.
- Samsung SM-S921B model visibility, generation, animation, rotation, lifecycle, RAM and thermal behavior: UNVERIFIED.
- Signed release APK/AAB: BLOCKED until valid signing credentials are configured.

The two source-branch CI passes are not evidence that the combined candidate builds. Re-run the full matrix against the exact candidate HEAD.
