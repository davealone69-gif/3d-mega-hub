# 3D Mega Hub

**Status: integration candidate. Android CI and Samsung runtime verification are still required.**

## Current candidate

- Branch: `work/engine-integration-candidate`
- Draft PR: [#7](https://github.com/davealone69-gif/3d-mega-hub/pull/7)
- `main` is unchanged.
- The candidate combines the Filament renderer work with the AvatarForge native generation module.

## What is now wired in source

- **AvatarForge generation:** isolated `:avatarforge-engine` Android library, JNI bridge, and C++ generation kernel. The Avatar Studio exposes a user-triggered prompt generation action, cancellation, and the engine's existing RAM/storage/thermal gates.
- **3D viewer:** Filament gltfio loads the bundled sample GLB and self-contained imported/generated GLB files. It plays the first glTF animation when present and frames the camera from the asset bounds.
- **Graphics backend:** OpenGL ES is primary; Vulkan is an optional fallback if the complete GLES session fails.
- **Avatar library:** imported and generated file URIs are persisted. A bundled sample is loaded when no user avatar is selected.
- **Local AI:** existing chat sends requests to the configured Ollama `/api/tags` and `/api/chat` endpoints. This is an external local service, not an embedded LLM.
- **Voice:** existing Android TextToSpeech output is connected. Viseme extraction and avatar lip-sync are not connected.

## Known boundaries

- The AvatarForge module is copied from `work/avatarforge-core-integration` at `731bd31bf431cf391cb052d1c06d49e0c7255ba9`, with donor source pinned to AvatarForge `work/android-engine-dev` at `9e36197ace3b18faa6c566d93f8fce89b185c45d`. This is older than the current AvatarForge fixes in progress and must be refreshed after those changes are pushed and reviewed.
- The first viewer pass supports self-contained GLB files. External-resource `.gltf` files are rejected with an explicit status.
- Prompt generation is deterministic native parsing, not photo-to-identity reconstruction and not an embedded LLM.
- A submitted frame or successful gltfio upload is not proof of visible pixels on the phone.

## Verification

- Renderer source branch CI passed at `2a1352c5994a42a586a929d995a33fcb4f6ebd67`.
- AvatarForge core source branch CI passed at `731bd31bf431cf391cb052d1c06d49e0c7255ba9`.
- Combined candidate CI must pass on the exact current candidate commit.
- Fresh candidate APK, artifact link, and SHA-256 are pending.
- Samsung SM-S921B install, model visibility, generation, animation, lifecycle, RAM and thermal behavior remain **UNVERIFIED**.
- Signed release APK/AAB remains **BLOCKED** until the release keystore file and credentials are available.

## Existing debug build

[Download the previously published debug APK](https://github.com/davealone69-gif/3d-mega-hub/releases/download/android-build-46/3D-Mega-Hub-debug.apk). This is an older troubleshooting build, not the current integration candidate and not a production release.
