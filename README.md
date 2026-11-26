# 3D Mega Hub

**Status: integration hardening in progress. Not ready to merge or call fixed.**

## Current working branch

- Branch: [`integration/avatarforge-renderer-port`](https://github.com/davealone69-gif/3d-mega-hub/tree/integration/avatarforge-renderer-port)
- Review PR: [#9](https://github.com/davealone69-gif/3d-mega-hub/pull/9)
- Consolidation source: `work/samsung-s24-photoreal-patch`, three commits beyond the integration base, including the WebP/GLES renderer fixes. The final source is being fast-forwarded into this single working branch.
- `main` remains unchanged. PR #8 is an older, superseded Filament-only candidate and is not the integration target.

## What is wired in source

- **AvatarForge generation:** isolated `:avatarforge-engine` Android library, JNI bridge and C++ generation kernel. Avatar Studio exposes prompt generation, cancellation, resource/thermal checks, native-result validation and atomic publication of validated GLBs.
- **3D viewer:** the bundled sample uses the separate Filament preview. Imported and generated self-contained GLBs use `AvatarForgeSurfaceView` and the integrated native AvatarForge renderer.
- **Graphics backend:** the imported/generated avatar path explicitly forces native GLES. Skinned GLBs apply the initial rest pose and refresh bounds before camera framing; Vulkan code is not selected for this phone-first route.
- **Debug verification:** debuggable builds include a seed-1337 generated-avatar diagnostic that validates the deformed mesh, imports the exported GLB through the real GLES route, waits for successful EGL swaps and verifies background-subtracted PixelCopy geometry; it saves a PNG result. A scoped AndroidX FileProvider exposes only app-private external-files captures for sharing, including `failed_render.png`.
- **Avatar imports:** selected GLBs are copied into app-private storage, size-limited and structurally validated before use. The viewer rejects external-resource `.gltf` files.
- **Local AI:** Ollama remains an external service, not an embedded LLM. Plain HTTP is restricted to loopback; remote endpoints require HTTPS.
- **Voice:** Android TextToSpeech output is connected. Viseme extraction and avatar lip-sync are not connected.

## Important sample-model limitation

The bundled `female.glb` is a renderer fixture, not proof of a complete rigged avatar. Inspection of the exact bundled file shows one mesh, one node, seven morph targets, zero skins, zero animations and zero embedded images. It can exercise GLB loading/rendering, but it does not prove skeletal animation, facial animation, texture quality or generated-avatar quality.

The fixture is byte-identical to the file at 3DDD revision `be5bf17d8afe46247646465941fa1b7317d8e415` (Git blob `a1c00c6b31f6ab0db3b2e68243526cb2d0e15967`). The donor README at that revision states the generated base rigs are original work released under CC0. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Known boundaries and missing features

- The AvatarForge donor pin is `9e36197ace3b18faa6c566d93f8fce89b185c45d` from `work/android-engine-dev`. It is **divergent**, not a fast-forward ancestor, of the standalone AvatarForge RB-01 line (`0e782166…` / `fc30efd0…`). Several generation-core files differ in both directions. No wholesale replacement was attempted; any core-only port needs a file-by-file API review and native tests. AvatarForge remains a separate project.
- Prompt generation is deterministic native parsing, not photo-to-identity reconstruction and not an embedded LLM.
- External-resource `.gltf` is unsupported; the current viewer path is self-contained GLB.
- Voice lip-sync/visemes are missing.
- The bundled fixture is not a rigged/animated model.
- Samsung SM-S921B runtime, visible pixels, real-device generation, import lifecycle, memory and thermal behaviour remain **UNVERIFIED**.

## Verification

- Parent integration commit `f45c15a73c42b9240b6b6f325fe18d4df61fb630`: unit tests, lint, native sampling regression, debug APK build and R8/minification passed. The GLES emulator smoke timed out while booting the `swangle` emulator; the generated-runtime workflow stopped before tests because it attempted to install the invalid SDK package `tools`.
- The current integration update restores Android WebP decoding, resolves the actual `EXT_texture_webp` image source with core-source fallback, checks/retries GLES geometry uploads, and registers host GLB-extension regression tests.
- CI for the consolidated source is pending. Samsung SM-S921B visible-pixel/runtime verification remains **UNVERIFIED**. FileProvider sharing and the launcher icon are source changes, not yet build-verified.
- Production release signing is **BLOCKED** until a user-owned production keystore is configured through protected CI secrets. Never substitute the debug key or a one-run preview key.

## Existing older debug build

[Download the previously published debug APK](https://github.com/davealone69-gif/3d-mega-hub/releases/download/android-build-46/3D-Mega-Hub-debug.apk). This is an older troubleshooting build, not the current integration candidate and not a production release.
