# Project Status

Last updated: 2026-10-31.

## Repository

- Repository: [davealone69-gif/3d-mega-hub](https://github.com/davealone69-gif/3d-mega-hub)
- Default branch: `main`, unchanged by this integration work.
- Candidate branch: `work/engine-integration-candidate`.
- Candidate PR: [#7](https://github.com/davealone69-gif/3d-mega-hub/pull/7), draft.
- Previous source PRs #4 (Filament startup) and #5 (AvatarForge core) remain separate and unmerged.

## Candidate source

| Component | Source |
|---|---|
| Filament host/viewer base | `work/filament-renderer-live-diagnostics` at `2a1352c5994a42a586a929d995a33fcb4f6ebd67` |
| AvatarForge host module | `work/avatarforge-core-integration` at `731bd31bf431cf391cb052d1c06d49e0c7255ba9` |
| AvatarForge donor pin | `work/android-engine-dev` at `9e36197ace3b18faa6c566d93f8fce89b185c45d` |
| Filament Android / gltfio | `1.71.5`, Apache 2.0 |
| Bundled GLB fixture | `app/src/main/assets/models/female.glb`, provenance in `THIRD_PARTY_NOTICES.md` |

The AvatarForge donor pin is older than the current donor fixes in progress. Refresh the module after the updated AvatarForge source is pushed and reviewed.

## Evidence-based status

| Area | Status | Evidence / remaining gate |
|---|---|---|
| Native generation module source | **INTEGRATED IN CANDIDATE** | JNI/C++ core, Kotlin facade, spec, resource governor and module tests are present. |
| Generation UI | **IMPLEMENTED IN SOURCE, UNVERIFIED** | User-triggered prompt generation, cancellation, and status display are wired to `AvatarForgeEngine.generate`. No phone run yet. |
| Filament GLB viewer | **IMPLEMENTED IN SOURCE, UNVERIFIED** | gltfio asset/resource loading, bundled sample, self-contained GLB loading, bounds-based camera framing and first-animation playback are wired. Visible pixels are not proven by CI. |
| Backend selection | **IMPLEMENTED IN SOURCE, UNVERIFIED** | GLES is primary; Vulkan is optional fallback. Actual backend is only known after successful session initialization. |
| Combined unit tests | **PENDING** | Must run against the exact candidate HEAD. |
| Combined Android lint | **PENDING** | Must run against the exact candidate HEAD. |
| Combined debug APK | **PENDING** | Must assemble on the exact candidate HEAD and publish artifact plus SHA-256. |
| Release R8/minification | **PENDING** | Must pass against the exact candidate HEAD. |
| Samsung SM-S921B runtime | **UNVERIFIED** | Requires install and in-app verification. No USB/ADB assumption. |
| Release signing | **BLOCKED** | Release keystore file and valid signing credentials are not available in the repository. Do not substitute a debug key. |
| Embedded LLM | **MISSING** | Ollama remains an external endpoint. |
| Photo-to-identity reconstruction | **MISSING** | The deterministic prompt parser is not image reconstruction. |
| Voice lip-sync / visemes | **MISSING** | Android TTS exists; viseme and avatar synchronization do not. |
| External-resource `.gltf` | **MISSING** | First viewer pass accepts self-contained GLB only. |

## Current gate

The first combined candidate has been pushed. Do not merge PR #7 or call the app fixed until combined CI passes, a fresh APK is built and hashed, and the installed app is exercised on the Samsung phone. A passing CI run does not prove visible model pixels or real-device generation.
