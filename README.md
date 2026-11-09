# 3D Mega Hub

**Status: integration hardening in progress. Not ready to merge or call fixed.**

## Current working branch

- Branch: [`fix/mega-hub-candidate-hardening`](https://github.com/davealone69-gif/3d-mega-hub/tree/fix/mega-hub-candidate-hardening)
- Review PR: [#8](https://github.com/davealone69-gif/3d-mega-hub/pull/8)
- Current hardening base: `7ba8526850ef85a3288c4984a82775ac08c77a65`
- `main` remains unchanged. Earlier candidate PR #7 is left open and untouched.

## What is wired in source

- **AvatarForge generation:** isolated `:avatarforge-engine` Android library, JNI bridge and C++ generation kernel. Avatar Studio exposes prompt generation, cancellation, resource/thermal checks, native-result validation and atomic publication of validated GLBs.
- **3D viewer:** Filament gltfio loads the bundled GLB fixture and self-contained imported/generated GLBs. It frames the camera from asset bounds and plays the first animation when present.
- **Graphics backend:** OpenGL ES is primary. Vulkan remains an optional fallback only after a complete GLES session fails.
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

- Earlier exact revision `34201cb16154958ae2379be6ce485856806d4bbc`: Android unit tests, lint, debug APK build and R8/minification all passed.
- Earlier exact revision `877f73d10756cd9a61a99724aaa37b1e63a6df0f`: signed preview APK and AAB built; signature verification and SHA-256 provenance passed. The ephemeral preview key is not a production signing identity.
- The earlier emulator smoke failed in its shell harness before APK installation. It did **not** establish an app crash. The harness has been replaced with a single Bash script that checks Avatar Studio, GLB load and an OpenGL ES frame.
- Current hardening HEAD includes the generated-avatar native AABB refresh, JNI post-skinning mesh validation, Filament camera-frustum check, and debug PixelCopy diagnostic dialog. Exact source commit: `7012c684c160a96dc9c88a1a58fc196f9d2f8374`.
- CI for that exact commit, fresh APK artifact, generated-avatar pixel capture and Samsung runtime verification are still pending/unverified.
- Production release signing is **BLOCKED** until a user-owned production keystore is configured through protected CI secrets. Never substitute the debug key or a one-run preview key.

## Generated-avatar diagnostic implementation

The debug Avatar Studio includes **Run generated-avatar diagnostic**. It generates seed 1337 with the fixed adult prompt, validates the CPU-deformed native mesh (positions, skin weights and joint indices), loads the generated GLB into the Hub's Filament viewer, checks the camera frustum and captures a completed frame using Android PixelCopy. If fewer than 5,000 qualifying pixels are counted, it writes `failed_render.png` to app-specific external storage and prints the exact path in the diagnostic log.

The Hub viewer remains Filament gltfio. AvatarForge's standalone custom GLES `af_gl.cpp` is not linked into this Hub module. The Hub applies its animation pose before camera framing and rendering; it does not ship two competing renderer backends.

## Existing older debug build

[Download the previously published debug APK](https://github.com/davealone69-gif/3d-mega-hub/releases/download/android-build-46/3D-Mega-Hub-debug.apk). This is an older troubleshooting build, not the current integration candidate and not a production release.
