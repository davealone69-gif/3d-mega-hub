# Project status

Last updated: 2026-11-14. The README carries the current results; this file holds the detail behind them and the history that must not be lost.

## Repository and branches

| Item | State |
|---|---|
| Repository | [davealone69-gif/3d-mega-hub](https://github.com/davealone69-gif/3d-mega-hub) |
| `main` | `aaee4443dbf53ef4707cce01d99f71a59e903ec7` (2026-10-29). Not modified. |
| Integration branch | [`integration/avatarforge-renderer-port`](https://github.com/davealone69-gif/3d-mega-hub/tree/integration/avatarforge-renderer-port) at `a424113`. The single line that goes to `main`. |
| Working branch of this handover | `arena/1a5b3043-3d-mega-hub`, a fast-forward of the integration branch. Lands through [PR #12](https://github.com/davealone69-gif/3d-mega-hub/pull/12). |
| PR #9 | Open. Integration branch to `main`. The only review path to `main`. |
| PR #8 | Closed on 2026-11-09 without merging (head `41c3069`). Superseded by PR #9. Its Filament-only viewer conflicts with the selected renderer split. |
| `test/photo-realistic-avatar-overlay` | Merged into the working branch with history (adds the photo-overlay quality backlog only). |
| Force-pushes | None. |

## Component baseline

| Component | Status | Evidence |
|---|---|---|
| Android shell: builds and launches | **REAL + VERIFIED** on CI and emulator | Debug APK, lint, unit tests and R8 pass; debug and signed release APKs install and launch on an API 30 emulator |
| AvatarForge engine: native generation, export, import | **REAL + VERIFIED** on host and emulator | Six native regression tests pass; an avatar is generated, validated, exported and re-imported on the emulator |
| GLES renderer: real geometry | **REAL + VERIFIED** on emulator | Imported and generated GLBs draw through native OpenGL ES 3.1; PixelCopy of the real SurfaceView finds the geometry |
| Filament: native initialisation and lifecycle | **REAL + VERIFIED** on emulator, including the minified release APK | The bundled sample loads and presents a frame in both builds |
| GLB importer | **REAL + VERIFIED**, static meshes only | Geometry, PBR materials, PNG and WebP textures. Skeletons and clips in a file are not applied; morph targets are not read |
| GLB exporter | **REAL + VERIFIED** | Khronos validator: 0 errors, 0 warnings after `fb0878e` |
| Asset pipeline: copy, validate, hash | **REAL + VERIFIED** on emulator | Imports are copied to app storage and structurally validated; generated GLB hashes are recorded in the evidence |
| Camera: orbit, zoom, reset | **REAL + VERIFIED** on emulator for orbit | A touch drag changes the measured yaw and the rendered pixels. Pinch and double-tap are implemented and not exercised by a test |
| Diagnostics | **REAL + VERIFIED** on emulator | Every number is measured on the render thread or counted by the renderer; see BUILD_AND_VERIFY.md |
| Signed APK and AAB | **REAL + VERIFIED** with a one-run CI key | Read-back verification and byte-identical rebuild. Production signing is **BLOCKED** on the owner keystore |
| Samsung SM-S921B | **UNVERIFIED** for this code | Requires the owner's test of the exact APK |

## Root causes established

1. **Red renderer gate on `a424113`.** The session ended itself unless three frames were presented within 15 seconds of creation, and the render thread then replaced the failure text with "AvatarForge renderer stopped." On the software-rendered emulator the first frame of the 1.6-million-triangle avatar takes 10.4 s and the median interval between later frames is 5.1 s (run `43635337124`), so three frames cannot arrive in 15 seconds. The rule is now: fail only when no valid frame at all arrives in 15 seconds; a slow renderer keeps running and reports its measured numbers; the failure text is kept.
2. **`Manipulator.nCreateBuilder` implementation not found.** `ModelViewer` from filament-utils was constructed after `Filament.init()` without `Utils.init()`, so `libfilament-utils-jni.so` was never loaded (commit `4fa9178` records it; `3856bb6` removed `ModelViewer`; `5974328` added the R8 keep rule). No code imports `filament.utils` any more, and the dependency and its native library are now removed from the build.
3. **Invalid glTF from the exporter.** 542 Khronos validator errors: three-element bounds on scalar animation-time accessors, and no bounds on morph-target position accessors. Fixed in `fb0878e`.
4. **Diagnostics that were not measurements.** Frame time and frame rate came from a clamped delta handed to the animation system, CPU time and texture bytes were never written, and the production path did not collect GL errors. All are now measured or counted.
5. **32-bit packaging.** The app had no ABI filter, so Filament's 32-bit libraries would have been packaged without the engine's. The app now packages `arm64-v8a` and `x86_64` only, and the release gate checks that every packaged ABI carries the same libraries.
6. **APK not reproducible.** Two clean builds differed only in signing block `0x504b4453`, the dependency list that the Android Gradle plugin encrypts with a random key. It is now left out of the APK (it stays in the AAB).
7. **PixelCopy timeouts on a slow GPU.** The copy was requested immediately after the swap, while the software GPU was still drawing the queued frame, and PixelCopy answered with its timeout code (`result=2`) on every attempt. The render thread now finishes the frame's GPU work before the request and holds the next frame until the copy is answered.
8. **Input-dispatch timeout between tests.** One instrumented test left its viewport open, still drawing a 1.1-million-triangle avatar, which starved the next test's window on the software GPU. Each test now closes the app and waits for the native session to be released.
9. **Signing password in the job log.** The one-run preview keystore password was written to the job environment unmasked and printed by later steps. It is now masked. The key was discarded with the runner and never uploaded.

## Branch review

The six branches named for review were compared with the integration branch by ancestry and by file content.

| Branch | Tip | Relation to the integration branch | Disposition |
|---|---|---|---|
| `work/engine-integration-candidate` | `7ba8526` | Ancestor; no commits of its own | Already contained |
| `work/filament-gles-renderer` | `3856bb6` | Ancestor; no commits of its own | Already contained |
| `work/filament-renderer-live-diagnostics` | `2a1352c` | Ancestor; no commits of its own | Already contained |
| `fix/filament-engine-jni-init` | `5974328` | Ancestor; no commits of its own | Already contained |
| `work/avatarforge-core-integration` | `731bd31` | One commit of its own: an earlier import of the AvatarForge generation core | Superseded. Every file it has exists in the integration branch; inside `avatarforge-engine`, 23 of its files were changed later and 28 files were added. Copying it back would roll the engine back |
| `feature/first-working-gles-scene` | `31ea0b0` | Seven commits of its own, branched from `main` | Superseded. It draws a hard-coded 36-vertex cube with OpenGL ES 2.0 and shows a measured frame rate. It does not load GLBs. Its camera gestures and live diagnostics exist in the integrated renderer with real geometry |

Other branches, from the 2026-11-11 audit, unchanged:

| Branch | Disposition |
|---|---|
| `work/samsung-s24-photoreal-patch` | Its WebP and GLES commits are in the integration branch. |
| `work/fileprovider-renderer-diagnostics` | The FileProvider capability was ported selectively. Its replacement activity and disconnected overlay were not. |
| `agent/android-build-check-2026-11-03` | Launcher icon and manifest reference are in the integration branch. |
| `docs/glb-display-generation-baseline` (PR #11, closed) | README-only; its facts were already recorded. |
| `fix/mega-hub-candidate-hardening` (PR #8 and PR #10, closed) | Older Filament-only candidate. Not merged. |
| `work/android-shell-foundation` | Early shell foundation. PR #1 from it was merged into `main` on 2026-10-29; the branch itself is superseded. |
| `trae/agent-hDYcEF` | Generated Gradle cache files and older viewer edits. Not source; not imported. |

Older branches are kept as recovery points. They are not work targets.

## Integrated source

| Component | Source |
|---|---|
| Filament | `filament-android` and `gltfio-android` 1.71.6 (Apache 2.0). `filament-utils-android` is not used. |
| AvatarForge Android library | `:avatarforge-engine`: C++ generation core, GLB importer and exporter, native OpenGL ES renderer, JNI, Kotlin facade and resource governor |
| Viewer for imported and generated GLBs | `AvatarForgeSurfaceView`, native OpenGL ES. The Vulkan backend is compiled and never selected. |
| Viewer for the bundled sample | `FilamentPreviewView` |
| Bundled fixture | `app/src/main/assets/models/female.glb`, byte-identical to 3DDD revision `be5bf17d8afe46247646465941fa1b7317d8e415` (Git blob `a1c00c6b31f6ab0db3b2e68243526cb2d0e15967`). One mesh, seven morph targets, no skin, no animation, no images. Seven Khronos validator errors (morph targets without bounds). A renderer fixture, not a reference avatar. |
| Toolchain | AGP 8.7.3, Kotlin 2.1.0, Gradle 8.11.1, NDK 26.1.10909125, CMake 3.22.1, compile and target SDK 35, min SDK 26 |

The AvatarForge donor pin is `9e36197ace3b18faa6c566d93f8fce89b185c45d` from `work/android-engine-dev`. It diverges from the standalone AvatarForge RB-01 line (`0e78216643d1a305be7d278011d6732e56d62d62`, merge `fc30efd0db3d7892cfee388a783508ea18f32471`); generation-core files differ in both directions. Do not replace either tree wholesale. AvatarForge remains a separate project.

## Missing features

| Feature | Status |
|---|---|
| Skeletal animation, posing and facial animation of the avatar shown in the app | **MISSING**. Imported GLBs are static, and generated avatars are shown through export and import. |
| Voice lip-sync and visemes | **MISSING**. Text-to-speech output works. |
| Embedded language model | **MISSING**. Ollama is an external service. |
| Photo-to-identity reconstruction | **MISSING**. Prompts are parsed deterministically. |
| External-resource `.gltf` | **MISSING**. Self-contained GLB only. |
| Renderer recovery after a lost GL context while the surface stays alive | **MISSING**. The session reports the stall and is rebuilt when the surface is recreated. |

## Known defects, recorded and not changed

- Left and right are not named consistently in the generation core. See [docs/AVATAR_GEOMETRY_EVIDENCE.md](docs/AVATAR_GEOMETRY_EVIDENCE.md).
- Generated avatars face `−Z`; glTF's front is `+Z`. The app compensates for its own files only.
- The debug-only PixelCopy check encodes a PNG and writes files on the UI thread once per session.
- An exported GLB is not byte-identical between a Linux host and Android (284 bytes apart for the diagnostic avatar).

## History

**2026-11-13, owner phone test.** The owner tested an installed APK on the phone: the renderer displayed the avatar, the generation button changed avatar features including male and female, and GLB import worked. Skin-tone and hair changes were not tested. Avatar quality was too poor to judge appearance changes. The APK's SHA-256 and commit were not recorded, so this is **PARTIAL** evidence for an earlier build and **UNVERIFIED** for the current code. The avatar-quality plan that followed is in [docs/AVATAR_QUALITY_PLAN.md](docs/AVATAR_QUALITY_PLAN.md).

**2026-11-14, integration branch at `a424113`.** Nine checks passed and two failed, both in `generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView` (root cause 1). An earlier head, `705165e`, had failed the same way ("presented only 2 frames in 15 seconds") and had also hit `keyDispatchingTimedOut` (root cause 8).

**2026-11-11, consolidation.** The repository had 15 branches. `integration/avatarforge-renderer-port` was selected as the single integration line; dispositions are in the tables above. Commit `46b13ee` passed lint, unit tests, debug APK, R8, native regression and the signed preview; its emulator smoke failed and was corrected in `42da0e3` and `5b8f6c1`.
