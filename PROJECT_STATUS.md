# Project Status

Last updated: 2026-11-06.

## Repository and protected work

- Repository: [davealone69-gif/3d-mega-hub](https://github.com/davealone69-gif/3d-mega-hub)
- Default branch: `main`, last confirmed at `aaee4443dbf53ef4707cce01d99f71a59e903ec7`.
- Active branch: [`fix/mega-hub-candidate-hardening`](https://github.com/davealone69-gif/3d-mega-hub/tree/fix/mega-hub-candidate-hardening).
- Active PR: [#8](https://github.com/davealone69-gif/3d-mega-hub/pull/8).
- Hardening started from confirmed candidate `7ba8526850ef85a3288c4984a82775ac08c77a65`.
- Previous candidate PR #7 remains open and untouched.
- The separate dirty renderer checkout was not touched. No force-push or merge into `main` was performed.

## Integrated source

| Component | Source / evidence |
|---|---|
| Filament host/viewer | Candidate source, Filament Android/gltfio 1.71.5 |
| AvatarForge Android library | `:avatarforge-engine`, JNI/C++ kernel, Kotlin facade and resource governor |
| Bundled GLB fixture | `app/src/main/assets/models/female.glb` |
| Fixture provenance | 3DDD revision `be5bf17d8afe46247646465941fa1b7317d8e415`; matching Git blob `a1c00c6b31f6ab0db3b2e68243526cb2d0e15967` |
| Filament licence | Apache License 2.0 |
| Fixture licence statement | 3DDD README at the pinned revision states generated base rigs are original work released under CC0 |

The AvatarForge donor pin is `9e36197ace3b18faa6c566d93f8fce89b185c45d` from `work/android-engine-dev`. It diverges from the standalone AvatarForge RB-01 line (`0e78216643d1a305be7d278011d6732e56d62d62` / merge `fc30efd0db3d7892cfee388a783508ea18f32471`): the comparison reports 48 commits unique to RB-01 and 138 unique to the donor side. Multiple generation-core files differ in both directions. Do not wholesale copy either tree. Any update must be a controlled core-only port with API review and native tests; AvatarForge remains standalone.

## Hardening changes

- Ollama endpoint validation: HTTP limited to loopback; remote endpoints require HTTPS; credentials, paths, query strings and fragments rejected.
- Ollama requests: redirects disabled, connection/read timeouts, model-name and prompt bounds, response-size limits.
- Android network security: cleartext denied by default, loopback exception only.
- GLB validation: GLB v2 magic/version/length, chunk alignment and bounds, JSON chunk and glTF asset version checks; maximum 128 MiB.
- Generated output: native output is staged, validated and atomically moved into app-private avatar storage; native statistics are checked against geometry and memory-budget invariants.
- Imported output: content-provider input is copied to app-private storage, size-limited, validated and atomically published. It no longer depends on a possibly non-persistable document URI.
- UI: prompt bounded to 500 characters, parser warnings surfaced, and conflicting import/remove/generate actions disabled.
- Renderer: status logging added for diagnosable GLB-load and frame checkpoints.
- Emulator workflow: standalone Bash script, GLES-specific frame assertion, diagnostic screenshot/log capture, concurrency cancellation and bounded job runtime.

## Evidence-based status

| Gate | Status | Evidence |
|---|---|---|
| Ollama endpoint utility | **REAL + VERIFIED** | Kotlin compiler smoke passed; Android unit tests passed on revision `34201cb`. Actual phone-to-Ollama connection remains unverified. |
| GLB validator | **REAL + VERIFIED** | Kotlin compile/synthetic GLB checks passed; Android unit tests passed on revision `34201cb`; actual bundled `female.glb` (418,556 bytes) passes the validator. |
| Native GLB atomic publication | **IMPLEMENTED, BUILD VERIFIED ON EARLIER REVISION** | Release preview build at `877f73d` compiled the engine and produced APK/AAB. Latest head still requires CI. |
| Durable GLB import path | **IMPLEMENTED, UNVERIFIED ON LATEST HEAD** | Source copied to private storage, bounded, validated and atomically moved; latest-head Android build pending. |
| Android unit tests | **PARTIAL** | Passed on `34201cb`; exact current PR head still pending. |
| Android lint | **PARTIAL** | Passed on `34201cb`; exact current PR head still pending. |
| Debug APK build | **PARTIAL** | Passed on `34201cb`; exact current PR head still pending. |
| Release R8/minification | **PARTIAL** | Passed on `34201cb`; exact current PR head still pending. |
| Signed preview APK + AAB | **REAL + VERIFIED ON EARLIER REVISION** | Revision `877f73d`; APK and AAB built, signatures checked, SHA-256 recorded. These use a one-run ephemeral key and are not production releases. |
| Emulator install/launch/GLB/GLES smoke | **PENDING** | Earlier smoke failed before APK install because the action executed script lines in separate shells. A standalone Bash script now fixes that harness defect; corrected run pending. |
| Visible model pixels | **UNVERIFIED** | Requires inspection of the corrected emulator screenshot and then the Samsung phone. A submitted frame alone is insufficient. |
| Samsung SM-S921B runtime | **UNVERIFIED** | Must install and exercise the current build on the user's phone. No USB/ADB access assumed. |
| Production signing | **BLOCKED** | No authorized user-owned release keystore/credentials configured. Do not substitute debug or ephemeral keys. |
| Embedded LLM | **MISSING** | Ollama remains external. |
| Photo-to-identity reconstruction | **MISSING** | Deterministic prompt parser is not image reconstruction. |
| Voice lip-sync / visemes | **MISSING** | Android TTS exists; viseme extraction and avatar synchronization do not. |
| External-resource `.gltf` | **MISSING** | Self-contained GLB only. |

## Bundled fixture limits

The bundled `female.glb` is a renderer fixture, not a complete rigged avatar: one mesh, one node, seven morph targets, zero skins, zero animations and zero embedded images. It proves neither generated-avatar quality nor skeletal/facial animation. The GLB container validator accepts the fixture, but Filament runtime and visible pixels must be proven separately.

## Current gate

Do not merge PR #8 or call the app fixed until the exact current HEAD passes unit tests, lint, debug APK, R8, signed preview APK/AAB and the corrected emulator smoke. Inspect the emulator screenshot for actual model pixels. Then install and test on Samsung SM-S921B, including Ollama, generation, import, animation, lifecycle, memory and thermal behaviour. A passing CI build or emulator is not a substitute for the phone test.
