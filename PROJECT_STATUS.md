# Project Status

Last updated: 2026-11-10.

## Repository and active branch

- Repository: [davealone69-gif/3d-mega-hub](https://github.com/davealone69-gif/3d-mega-hub)
- Default branch: `main`; protected baseline remains unchanged at `aaee4443dbf53ef4707cce01d99f71a59e903ec7`.
- Single integration branch: [`integration/avatarforge-renderer-port`](https://github.com/davealone69-gif/3d-mega-hub/tree/integration/avatarforge-renderer-port).
- Review PR: [#9](https://github.com/davealone69-gif/3d-mega-hub/pull/9).
- The three-commit Samsung photoreal/WebP/GLES update and the FileProvider/launcher-icon consolidation are included in the final integration source.
- PR #8 (`fix/mega-hub-candidate-hardening`) is a superseded, older Filament-only candidate. Do not merge it into the current integration.
- No force-push or merge to `main` has been performed.

## Consolidation audit

The repository had 14 branches at the time of the audit. The working source was consolidated into the single integration branch without replacing the newer GLES path with older experimental implementations.

| Branch / workstream | Disposition |
|---|---|
| `work/samsung-s24-photoreal-patch` | Three commits ahead of `integration/avatarforge-renderer-port`; contains the latest WebP texture decoding/fallback, GLES upload retry and generated-runtime test changes. Included in the integration branch. |
| `integration/avatarforge-renderer-port` | Canonical integration base and target branch. Retains native AvatarForge GLES for imported/generated GLBs and separate Filament preview for the bundled sample. |
| `work/fileprovider-renderer-diagnostics` | Selectively ported the FileProvider capability. Its replacement foundation `MainActivity` and non-connected capability/diagnostic overlay were deliberately not copied. The real Avatar Studio now shares the actual PixelCopy file. |
| `agent/android-build-check-2026-11-03` | Ported its launcher vector asset and manifest icon reference. Existing current `.gitignore` already excludes generated Gradle/build outputs and signing-key files. |
| `fix/mega-hub-candidate-hardening` / PR #8 | Not merged. Its viewer architecture routes all GLBs through Filament and conflicts with the newer selected split: Filament sample preview plus AvatarForge GLES for imported/generated GLBs. Its hardening notes remain historical evidence only where explicitly referenced. |
| `feature/first-working-gles-scene` | Earlier standalone GLES test scene. Superseded by the integrated AvatarForge GLES surface and runtime tests. |
| `fix/filament-engine-jni-init`, `work/filament-renderer-live-diagnostics`, `work/engine-integration-candidate`, `work/filament-gles-renderer` | Older ancestors of the current integration line; their incorporated work is already present. No duplicate cherry-picks required. |
| `work/avatarforge-core-integration` | Earlier divergent engine snapshot. Not copied over the newer integrated engine; doing so would roll back generation-core changes. |
| `work/android-shell-foundation` | Divergent early shell/wrapper foundation. Not copied over the later application and pinned toolchain. |
| `trae/agent-hDYcEF` | Contains generated `.gradle` cache/lock artifacts and older viewer edits. Cache files are not source and were not imported. |
| `agent/android-build-check-2026-11-03` | Only the launcher icon was useful and is now included; old manifest contents were not copied. |

Older branch refs are retained as historical recovery points; they are not parallel active work targets. Continue work only on `integration/avatarforge-renderer-port`. Keep `main` stable.

## Integrated source

| Component | Source / evidence |
|---|---|
| Filament host/viewer | Separate bundled-sample preview, Filament Android/gltfio 1.71.5 |
| AvatarForge Android library | `:avatarforge-engine`, JNI/C++ generation kernel, Kotlin facade and resource governor |
| Imported/generated avatar viewer | `AvatarForgeSurfaceView`, native GLES route; Vulkan is not selected for this phone-first path |
| Bundled GLB fixture | `app/src/main/assets/models/female.glb` |
| Fixture provenance | 3DDD revision `be5bf17d8afe46247646465941fa1b7317d8e415`; Git blob `a1c00c6b31f6ab0db3b2e68243526cb2d0e15967` |
| Filament licence | Apache License 2.0 |
| Fixture licence statement | 3DDD README at the pinned revision states generated base rigs are original work released under CC0 |
| Diagnostic image sharing | AndroidX FileProvider restricted to app-specific external-files storage; real PixelCopy capture path is shared with ClipData and temporary read permission |

The AvatarForge donor pin is `9e36197ace3b18faa6c566d93f8fce89b185c45d` from `work/android-engine-dev`. It diverges from the standalone AvatarForge RB-01 line (`0e78216643d1a305be7d278011d6732e56d62d62` / merge `fc30efd0db3d7892cfee388a783508ea18f32471`). Multiple generation-core files differ in both directions. Do not wholesale replace either tree; any donor update must be a controlled core-only port with API review and native tests. AvatarForge remains a separate project.

## Current implementation

- AvatarForge generation is an isolated Android library with JNI/C++ generation, cancellation, resource/thermal checks, deformed-mesh validation and atomic publication of validated GLBs.
- Imported/generated self-contained GLBs use the native GLES renderer. The bundled sample stays on the separate Filament preview.
- Skinned GLBs apply the rest pose before camera framing. CPU deformation refreshes indexed submesh bounds before frustum checks.
- WebP image decoding and `EXT_texture_webp` source resolution include core-source fallback. Geometry uploads are checked/retried.
- Debug builds provide seed-1337 generated-avatar diagnostics, successful EGL swap checks, in-frustum checks and background-subtracted PixelCopy. The capture is saved as `generated_avatar_diagnostic.png` or `failed_render.png`.
- Avatar Studio shares the actual capture only after the file exists. FileProvider path is app-private external-files storage, not broad shared storage.
- GLB imports are copied to app-private storage, size-limited and structurally validated before publication. External-resource `.gltf` remains unsupported.
- Ollama is an external service, not an embedded LLM. Cleartext HTTP is limited to loopback; remote endpoints require HTTPS.
- Android TextToSpeech output is connected. Viseme extraction and avatar lip-sync are not connected.

## Verification ledger

| Gate | Status | Evidence / next requirement |
|---|---|---|
| Ollama endpoint utility | **REAL + VERIFIED on earlier revision** | Kotlin compiler smoke and Android unit tests passed on revision `34201cb`; phone-to-Ollama connection remains unverified. |
| GLB validator | **REAL + VERIFIED on earlier revision** | Kotlin/synthetic GLB checks and unit tests passed on `34201cb`; bundled fixture passed structural validation. |
| Native generation and atomic GLB publication | **PARTIAL** | Earlier release preview built the engine; exact consolidated HEAD requires fresh CI/build evidence. |
| WebP decoding/source fallback | **IMPLEMENTED, UNVERIFIED on consolidated HEAD** | Source and host regression tests are present; run exact-head CTest and Android build. |
| PixelCopy diagnostic and GLES renderer | **IMPLEMENTED, DEVICE UNVERIFIED** | Source requires successful EGL swaps, draw/triangle counts, in-frustum bounds and more than 5,000 background-subtracted pixels. Inspect actual saved capture. |
| FileProvider capture sharing | **IMPLEMENTED, BUILD UNVERIFIED** | Provider and scoped XML plus share action are present. Must verify Android manifest merge, install, chooser and receiving app read access. |
| Launcher icon | **IMPLEMENTED, BUILD UNVERIFIED** | Vector drawable and manifest reference are present. |
| Android unit tests / lint / debug APK / R8 | **PARTIAL** | Passed on earlier revisions; exact consolidated HEAD needs fresh run. |
| Emulator install/launch/GLES smoke | **UNVERIFIED** | Prior emulator attempt timed out during boot; corrected smoke workflow exists but needs a successful run and screenshot inspection. |
| Samsung SM-S921B runtime / visible pixels | **UNVERIFIED** | Requires installing this exact consolidated APK on the phone and exercising rendering, generation, import, lifecycle, memory and thermal behaviour. |
| Production APK/AAB signing | **BLOCKED** | User-owned release keystore and protected CI secrets are not configured. Never substitute debug or ephemeral preview keys. |
| Embedded LLM | **MISSING** | Ollama remains external. |
| Photo-to-identity reconstruction | **MISSING** | Deterministic prompt parsing is not image reconstruction. |
| Voice lip-sync / visemes | **MISSING** | TTS exists; viseme extraction and avatar synchronization do not. |
| External-resource `.gltf` | **MISSING** | Self-contained GLB only. |

## Fixture and release limitations

The bundled `female.glb` is a renderer fixture, not a complete rigged avatar: one mesh, one node, seven morph targets, zero skins, zero animations and zero embedded images. It does not prove skeletal animation, facial animation, texture quality or generated-avatar quality.

Production release signing remains blocked until a user-owned keystore is configured through protected CI secrets. Preview signing is not a production signing identity.

## Next single gate

Run the exact consolidated branch through host CTest, Android unit tests, lint, debug APK, R8/minification, signed preview APK/AAB, corrected emulator GLES smoke and artifact/signature checks. Inspect the captured model pixels. Then install and test that exact APK on Samsung SM-S921B. Until the phone test passes, the app is not a green build and renderer status remains **UNVERIFIED**.

## Black-viewport renderer audit (2026-11-10, branch `arena/33f12efd-3d-mega-hub`, PR #16)

Device report (Galaxy S24, SM-S921B, Xclipse 940): imported 9 MB GLB renders a black viewport and the PixelCopy gate fails with "0 geometry pixels"; the same failure hits the generated-avatar diagnostic. Native diagnostics were healthy throughout (draw calls, triangles, boundsInFrustum, EGL swaps), and a black-shaded silhouette would still clear the threshold against the dark blue-grey clear colour - so literally zero fragments were reaching the presented frame.

Full host-side audit (all GLES shaders compiled and linked clean under Khronos glslang 11.7; probe-generated seed-1337 avatar is 389,717 CCW-agree vs 17,211 disagree, export/import roundtrip clean) ruled out shader errors, uniform/sampler binding, camera math, FBO completeness, UBO layouts and alpha defaults.

Confirmed defects fixed in this pass:

1. **Ground quad culled on GLES** - the studio floor strip winds clockwise from above while `glDisable(GL_CULL_FACE)` was never applied to it (the Vulkan ground pipeline uses `VK_CULL_MODE_NONE` for exactly this reason). Now drawn culling-disabled.
2. **Imported submesh culling regression** - the photoreal patch narrowed the gles-fix's "no culling for all foreign submeshes (`region < 0`)" rule to double-sided-only (`region == -2`), so any scan/export with CW winding or mirrored node transforms vanished entirely with healthy statistics. Restored `region < 0` culling disable, and the forward shader's backface normal flip is widened from `mat.uRegion.x < -1.5` to `< -0.5` accordingly.
3. **Skinned imports skipped node-transform baking** (`if (asset.skins.empty() && ...)`) although imported skeletons are never applied - the mesh node's world transform is the only placement those files get. Baking now runs for all files with nodes. Verified by `probe/import_fixes_probe`: skinned GLB with node translation +10 X bakes bounds to (10..11).
4. **`alphaMode: OPAQUE` now pins `base_color_factor[3]` to 1** (glTF 2.0 3.9.2). A stored zero alpha previously drove `if (fragColor.a <= 0.0) discard` for every fragment and erased the whole model. Verified by the same probe; BLEND alpha preserved.
5. **MSAA->HDR resolve self-test with fallback ladder** (`verifyResolvePath`): some GLES drivers (Xclipse/ANGLE class) report complete framebuffers yet resolve multisampled float colour to black. On failure the targets rebuild at 1x MSAA, then RGBA8, instead of shipping an invisible frame.
6. **Failure evidence chain**: the PixelCopy diagnostic session now enables native `verifyFramePixels` (per-frame GL error drain + glReadPixels of the presented framebuffer), `nativeGetStats` carries `readbackValid`/`readbackNonBlackPixels`, and the FAILED message states whether geometry was already present in the GL framebuffer (compositor/PixelCopy loss) or absent (raster/culling/shading path). Also forces MATCH_PARENT layout on the SurfaceView (grey side bars in the screenshots were the uncovered container strip).

Host evidence: all four CTest host tests rebuilt against the modified engine and pass (`native_sampling_test`, `glb_extension_test`, `camera_frustum_test`, `imported_glb_pbr_test`), plus `probe/import_fixes_probe` for fixes 3-4. The remaining unknown is device behaviour on the Xclipse driver; if the black viewport persists, the new failure message localises it to either the compositing path or the raster path in a single run.
