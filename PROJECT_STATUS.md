# Project Status

Last updated: 2026-11-13.

## Latest engineering update — 2026-11-15

- Active implementation: `integration/avatarforge-renderer-port`, PR #9. Stable `main` remains at `aaee4443dbf53ef4707cce01d99f71a59e903ec7`.
- PR #8 is closed and unmerged; its head is `41c3069301dc772bad3aa8dba1a2cbf67f8110c9`. Do not use it as the work branch.
- Inspected PR #9 head `a42411302b7f9e9eabe171f844d8b4d646f1c23b`: Android CI and native host regression passed, but both generated-avatar GLES instrumentation workflows failed the visible-pixel gate. Their terminal message was overwritten to the generic `AvatarForge renderer stopped.`, concealing the first renderer exception.
- **Implemented:** `78cb75d81557623622f6989a285ebf958dea2844` keeps the renderer failure and any teardown failure in the final status. This improves diagnosis only; it does not claim a renderer fix.
- **Next:** run the consolidated exact-head validation on the new commit, inspect the now-preserved first renderer error and fix that root cause. Do not weaken the visible-pixel threshold or convert frame submission into a pixel pass.
- **UNVERIFIED:** actual GLES visible-pixel rendering on the new commit; physical Samsung SM-S921B runtime. **BLOCKED:** production signing until the owner's keystore is configured in protected CI.

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
| `work/samsung-s24-photoreal-patch` | Its three WebP/GLES commits were fast-forwarded into the canonical integration branch. Its ref is now a stale pre-finalization pointer; do not continue work there. |
| `integration/avatarforge-renderer-port` | Canonical integration base and target branch. Retains native AvatarForge GLES for imported/generated GLBs and separate Filament preview for the bundled sample. |
| `work/fileprovider-renderer-diagnostics` | Selectively ported the FileProvider capability. Its replacement foundation `MainActivity` and non-connected capability/diagnostic overlay were deliberately not copied. The real Avatar Studio now shares the actual PixelCopy file. |
| `agent/android-build-check-2026-11-03` | Ported its launcher vector asset and manifest icon reference. Existing current `.gitignore` already excludes generated Gradle/build outputs and signing-key files. |
| `fix/mega-hub-candidate-hardening` / PR #8 | Not merged. Its viewer architecture routes all GLBs through Filament and conflicts with the newer selected split: Filament sample preview plus AvatarForge GLES for imported/generated GLBs. Its hardening notes remain historical evidence only where explicitly referenced. |
| `feature/first-working-gles-scene` | Earlier standalone GLES test scene. Superseded by the integrated AvatarForge GLES surface and runtime tests. |
| `fix/filament-engine-jni-init`, `work/filament-renderer-live-diagnostics`, `work/engine-integration-candidate`, `work/filament-gles-renderer` | Older ancestors of the current integration line; their incorporated work is already present. No duplicate cherry-picks required. |
| `work/avatarforge-core-integration` | Earlier divergent engine snapshot. Not copied over the newer integrated engine; doing so would roll back generation-core changes. |
| `work/android-shell-foundation` | Divergent early shell/wrapper foundation. Not copied over the later application and pinned toolchain. |
| `trae/agent-hDYcEF` | Contains generated `.gradle` cache/lock artifacts and older viewer edits. Cache files are not source and were not imported. |

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

## Repository consolidation audit (2026-11-11)

The live GitHub inventory returned 15 branches. Keep exactly one active work target: `integration/avatarforge-renderer-port`. `main` remains the stable baseline and has not been modified by this consolidation.

| Branch | Audit disposition |
|---|---|
| `integration/avatarforge-renderer-port` | **CANONICAL.** Continue all implementation, fixes and evidence updates here. PR #9 is the review path to `main`. |
| `work/samsung-s24-photoreal-patch` | Behind canonical; its WebP/GLES work is already in the canonical line. No cherry-pick. |
| `work/fileprovider-renderer-diagnostics` | Five-commit divergent foundation rewrite. The useful FileProvider capability is already selectively implemented in canonical Avatar Studio; its replacement Compose-only MainActivity and disconnected overlay are not to be merged. |
| `agent/android-build-check-2026-11-03` | Launcher icon and manifest reference are already present in canonical. No additional source change required. |
| `docs/glb-display-generation-baseline` / PR #11 | README-only update is redundant with the current canonical README, which already records the user-confirmed `mylove.glb` display result, fixture facts and separation of viewing from generation quality. Do not merge the stale baseline README over the current project handover. |
| `fix/mega-hub-candidate-hardening` / closed PR #10 | Divergent older candidate. Its Filament-based generated-avatar capture path conflicts with the selected canonical split (AvatarForge GLES for imported/generated GLBs; Filament only for the bundled sample). The canonical engine already contains post-skin deformation validation and refreshed submesh bounds. Do not wholesale merge the older app, renderer or engine files. Preserve as a historical comparison until branch cleanup is approved. |
| `feature/first-working-gles-scene` | Earlier standalone GLES scene, superseded by the integrated AvatarForge GLES viewport. |
| `work/avatarforge-core-integration` | Earlier engine snapshot, not a safe replacement for the later canonical generation core. |
| `work/android-shell-foundation` | Early divergent shell and wrapper foundation, superseded by the integrated app/toolchain. |
| `trae/agent-hDYcEF` | Contains generated Gradle cache/lock files and older viewer edits; no cache files are to be imported. |
| `fix/filament-engine-jni-init`, `work/engine-integration-candidate`, `work/filament-gles-renderer`, `work/filament-renderer-live-diagnostics` | Ancestor branches behind the canonical integration; no unique forward commits against canonical. |

### Exact-head validation follow-up

- The previously recorded baseline CI set for commit `46b13ee87f0daeaf5e89ae752575243b07e98a22` passed Android lint, unit tests, debug APK assembly, R8/minification, native host regression, and signed preview APK/AAB generation.
- Emulator install and launch succeeded, but the smoke script then exited with code 1. This is a failed smoke gate, not a successful renderer test.
- The generated-avatar and AvatarForge GLES instrumentation jobs failed before execution because `AvatarForgeRendererSmokeTest.kt` called a no-argument `UiDevice.takeScreenshot()` method unavailable in the pinned UI Automator 2.3.0 dependency. The test was corrected on this branch to use `InstrumentationRegistry.getInstrumentation().uiAutomation.takeScreenshot()`, which returns the Bitmap used by the existing pixel assertions.
- The corrected test change is committed as `42da0e396555eb6fb340a072b5d9396184e0164e`. The emulator smoke script was also corrected in `5b8f6c1aef6e1a88c3d5e3352300f6febcfe4baa`: it now trusts the successful `am start -W` result plus a live process PID instead of a brittle `dumpsys` text-format grep. Existing GLB-load, frame, ANR and screenshot checks remain in place. **Fresh CI for these exact commits is required; no pass is claimed yet.**
- PR #11 was closed as superseded after confirming its unique GLB fixture facts and user-confirmed display note are already present in the canonical README. Its older baseline README was not merged.
- Samsung SM-S921B rendering and visible-pixel verification remain **UNVERIFIED**. Production signing remains **BLOCKED** pending the user's production keystore and protected CI secrets.

Do not merge to `main` until fresh exact-head CI, emulator install/launch, real GLES pixel evidence and review are complete. The user's physical-phone test remains a separate final gate.


## Owner phone-test update and revised priority (2026-11-13)

The owner reports testing the latest installed APK on the physical phone:
- Avatar renderer displays the avatar.
- The generation button changes avatar features, including male/female when given suitable prompts.
- GLB import button works.
- Skin-tone and hair changes have not yet been tested.
- Overall avatar visual quality is too poor to judge appearance changes reliably.

**Evidence classification:** OWNER-TESTED for those reported functions on the APK the owner installed. The exact APK SHA-256 and source commit are not yet known. Do not claim that the current PR head or a specific CI artifact is the tested build. Do not generalise these results to all GLBs, lifecycle states or device conditions.

**Priority is now avatar quality first.** This supersedes the earlier README instruction to defer avatar quality until every renderer gate is complete. Preserve the working renderer, generation action and import path. Diagnose the generated model from parser/spec through anatomy, mesh, face, hair, clothing, materials/textures and export. Prioritise actual geometry/silhouette defects over lighting-only polish. Add deterministic data-level regression tests and matched before/after GLB/render captures. Then verify skin-tone and hair controls independently, followed by the Generate -> Export -> Reopen -> Import -> Render workflow.

No implementation change, build, CI rerun or new phone test is claimed by this handover update. Exact-head CI, emulator tests, artifact signing and release gates remain separate. Production signing remains BLOCKED until the owner configures a production keystore in protected CI secrets. main remains unchanged.
