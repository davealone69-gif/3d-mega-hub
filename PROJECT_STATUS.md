# Project Status

Last updated: 2026-11-16.

## Latest engineering update — 2026-11-16

- Active branch: `arena/089497bd-3d-mega-hub`, branched from `f73eec156b1baf6d105b4e891d8db45f22e66f3f` of `integration/avatarforge-renderer-port`. Stable `main` remains unchanged.
- **Fixed — imported GLBs were parsed for animation and then dropped on the floor.** The importer read a file's skeleton and its clips, logged two warnings saying they were *not applied*, and cleared them. Every imported model rendered in its bind pose forever, on every backend, and the only trace was a log line nobody reads. This was the last entry in the ledger claiming the GLES backend could not animate an imported file.
- **Implemented:** an import now adopts the file's own skeleton and drives it through the same `af::anim::Player` the generated path and the JNI bridge already use — no importer-specific animation code. New engine ABI: `af_engine_imported_clip_count` / `_clip_name` / `_clip_duration` / `_play` / `_seek` / `_advance` / `_pause` / `_resume` / `_is_playing`. `af_engine_joint_count`, `_joint_info` and `_joint_matrices` now describe whichever skeleton is live.
- **Implemented:** `af::anim::Player::resetToBindPose()`. `loadGlb()` auto-selects clip 0 and marks it playing, which conflated "show me the model" with "play its first animation from frame 0"; a fresh import now lands in its bind pose and playback is the caller's decision.
- **Implemented (renderer):** `AvatarForgeSurfaceView.nativeCreate` starts the file's first clip and `nativeRender` advances it per frame, so the GLES preview animates an imported GLB with no Kotlin change. The initial-pose block now branches between the two skeletons — routing an import through `af_engine_apply_clip` fails the session, because that call correctly reports `AF_ERR_NOT_READY` for a file whose authored rig is empty.
- **Deliberately still refused, and reported rather than faked:** a file with no skin (so no inverse bind matrices) adopts nothing and returns `AF_ERR_NOT_READY` — `AnimatedMorphCube.glb` and `female.glb` both take this path. Morph-target *displacements* are not decoded into the vertex pool on this CPU-skinning path, so a morph-only asset is imported as honest static geometry instead of advertising a clip the engine cannot play.
- **Verified on host:** 8/8 ctest. New `imported_animation_regression` (100 checks) asserts on the vertex buffer itself — 95.9 % of Fox's coordinates move at mid-clip, displacement 23.09 units against a 175.55-unit extent, the same clip time re-sampled reproduces bit-for-bit, `af_engine_vertices_rest` is never mutated by posing, `advance` wraps where `seek` clamps, and pausing actually stops the geometry. `animation_system_regression` grew 338 → 353 checks. JNI/Kotlin parity still 86/86.
- **UNVERIFIED:** the renderer JNI change is syntax-checked against a shim only — there is no NDK in this environment, so the CI Debug APK job is the sole proof it compiles against real `jni.h`. No on-device or emulator playback has been observed; nothing here proves visible pixels.
- **Measured (Priority Zero, host-verified) — the generated avatar's real memory footprint.** Replicating the in-app diagnostic's own spec (seed 1337, `FEMALE`/`ATHLETIC`/`MEDIUM`/`JEANS_JACKET`, 1.75 m) through the production native path gives a GLB of **83,642,456 bytes = 79.77 MiB**: 823,760 vertices, 1,103,656 triangles, 11 submeshes, 71 joints, 52 blendshapes (572 sparse morph targets, 1.309 % non-zero), `geometry_bytes` 81.76 MiB, `texture_bytes` 68.00 MiB, `peak_field_bytes` 41.14 MiB against an assigned budget of 1536 MiB. Every `bufferView` byte is attributed: base attributes 56.56 MiB, indices 12.63 MiB (32-bit, required at 823 k vertices), sparse morph targets 8.49 MiB, encoded PNGs 1.86 MiB, animation 0.09 MiB, inverse bind matrices 0.06 MiB. `stats.texture_bytes` is **decoded** CPU memory and is exact, not an estimate: channels are `{4,4,4,1,4}`, so 2048 x 2048 x 17 = 71,303,168 bytes = 68.00 MiB, while the same five maps occupy 1.86 MiB encoded in the file — a 36x difference that only the decoded figure can show. These numbers are now pinned by `imported_glb_pbr_regression`, which already exported the generated GLB and so costs no extra runtime: the file must fit half the measured 192 MiB Dalvik heap growth limit, all five texture slots must be populated and sum to `texture_bytes` (an empty slot would under-report memory), each must equal `width*height*channels`, and `geometry_bytes` must match the live counts at the current `af_vertex` ABI. The measured table prints on every run.
- **Fixed (Priority Zero) — the app could not read the file its own engine generated.** The instrumented renderer job had been red for weeks with no readable cause. Making it report produced the exact failure: `java.lang.OutOfMemoryError: Failed to allocate a 134217744 byte allocation ... growth limit 201326592` at `FilamentPreviewView.readBounded`. 134217744 is precisely 128 MiB — a `ByteArrayOutputStream` doubling from 64 MiB while reading a 79.77 MiB file, against a 192 MiB heap. Two defects, not one: a doubling buffer holds the old and new arrays at once (64 + 128 MiB live) and `toByteArray()` copies a third time, so peak use was about three times the file; and `MAX_MODEL_BYTES = 128 MiB` was a declared "safety limit" **larger than the heap could ever satisfy**, so it bounded nothing. `readBounded` now takes the file's length and allocates exactly once, `openModel()` supplies that length from `AssetFileDescriptor`, `File.length()` or the content resolver's descriptor, and `requireModelBytesFit()` budgets the file at half of `Runtime.maxMemory()` with the size, budget and heap in the message. A short read is handled by `copyOf` rather than treated as corruption.
- **Fixed (Priority Zero) — CI test selection could hide an omitted production test.** The instrumented job gated on grepping logcat for a literal `run finished: 3 tests, 0 failed, 0 ignored`, which cannot distinguish "all three passed" from "one silently stopped running", and it selected only `AvatarForgeRendererSmokeTest` while `GeneratedAvatarRuntimeTest` — the four tests that drive the production generated-GLB-to-SurfaceView route — never ran at all. The gate is now derived from JUnit XML totals. `tools/ci_report_test_failures.py` could not previously see instrumented results anyway: it matched only `*/build/test-results/*/*.xml` while `connectedDebugAndroidTest` writes to `app/build/outputs/androidTest-results/connected/<device>/`, so it would have printed "No JUnit failures found" for a run that had just failed. It now reads those paths, reports totals and per-test detail, extracts logcat crash blocks (a Java `FATAL EXCEPTION` stack or a native `Fatal signal` with its abort message and backtrace), and ranks candidates so cause lines survive GitHub's 10-annotation-per-level cap instead of being crowded out by Gradle's own plumbing. 14 assertion groups in `tools/test_ci_report_test_failures.py`, run in CI.
- **Still failing, and not yet diagnosed — stated plainly.** With the OOM fixed the instrumented job still fails, differently: the last run reported `tests=1 failures=1` and `Instrumentation run failed due to Process crashed`, so one of the three tests ran and then the app process died. The reason lives in the pulled logcat, which nothing read until now; the run dispatched with the crash-block extraction is expected to name it. **`GeneratedAvatarRuntimeTest` is deliberately still not selected**: its four tests carry timeouts of 900 s + 600 s + 600 s + 600 s, which on the job's `-gpu swangle` software rasterizer can exceed the job's entire 35-minute budget, converting a diagnosable failure into a timeout that reports nothing. That is a decision recorded in the script, not an oversight. No claim of device or emulator rendering success is made anywhere in this update.
- **BLOCKED (unchanged):** production signing pending an owner keystore in protected CI secrets.

## Engineering update — 2026-11-15

- Active implementation: `integration/avatarforge-renderer-port`, PR #9. Stable `main` remains unchanged at `aaee4443dbf53ef4707cce01d99f71a59e903ec7`.
- PR #8 is closed and unmerged; its head is `41c3069301dc772bad3aa8dba1a2cbf67f8110c9`. Do not use it as the work branch.
- **Implemented:** `78cb75d81557623622f6989a285ebf958dea2844` preserves the original renderer and teardown failures in the final worker status. Diagnostic observability only, not a renderer fix.
- **Implemented:** `ae43e0e271684d56336005450765a4577aafc591` fixes GLES 3.x extension enumeration. GLES 3.0+ requires `glGetStringi(GL_EXTENSIONS, index)`; the removed legacy extension-string query could return `GL_INVALID_ENUM` and incorrectly disable ES 3.1 extension detection. Float colour-buffer and anisotropy support are now reported from actual capabilities.
- **Verified on prior exact head `2a5485c17ec656659c4841826a1048d1577b7301`:** signed ephemeral preview APK/AAB build and signature verification (run `43626247432`, job `132542586035`); Android emulator install/launch smoke (job `132542586459`); native host regression (run `43626247376`, job `132542664347`). These results do not validate the later GLES source change.
- **Latest source head:** `ae43e0e271684d56336005450765a4577aafc591`. Fresh exact-head CI must validate the GLES capability change. **UNVERIFIED:** real imported/generated avatar visible pixels and Samsung SM-S921B runtime. **BLOCKED:** production signing pending owner keystore in protected CI secrets.
- Keep `main` unchanged; no merge until exact-head renderer evidence passes.

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
| Imported GLB skeletal animation | **IMPLEMENTED + HOST-VERIFIED; DEVICE UNVERIFIED** | `imported_animation_regression` (129 checks) drives `Fox.glb`, `CesiumMan.glb` and `CliplessRigged.glb` through `af_engine_import_glb_from_memory` and asserts on the posed vertex buffer. The renderer JNI change is syntax-checked only (no NDK here): needs the Debug APK job plus an on-device look. Morph-target displacements are still not decoded on the CPU-skinning path, and skinless files refuse rather than fake it. |
| Driving a rigged import that carries no clips | **IMPLEMENTED + HOST-VERIFIED; DEVICE UNVERIFIED, ARTIFACT QUALITY PARTIAL** | A file with a skin and inverse bind matrices but zero animations is deformable and was still permanently static. `Clip::fromAuthoredByRole` + `Player::loadAuthoredForGlb` bind the engine's 16-clip authored library to such a skeleton by humanoid `Role` rather than by joint name, and `importIntoEngine` falls back to it only when the file has nothing of its own. Measured on `Fox.glb`: name binding places 3 clips / 5 channels and **refuses `walk` outright**; role binding places 14 clips / 107 channels and all 11 of walk's tracks. `imported_animation_regression` proves the fixture lands in its bind pose, moves 100% of coordinates mid-`dance`, reproduces a pose bit-for-bit, never mutates `vertices_rest`, and — because 4 of its 8 vertices are bound to `LeftLowerArm` and 4 to `Hips` — that `wave` moves the arm's vertices and leaves the hips' vertices untouched, which a global transform faking motion would not do. **Rest-shape alignment.** The generated rig binds arms-DOWN (upper-arm rest axis `+0.156, -0.987, +0.020`) while `mylove_rig_prepared.glb` binds arms-OUT (`-0.999, +0.040, 0.000`), both with identity rest rotations, so copying authored rotations across verbatim imported the generated rig's shape with the motion: `t_pose` collapsed that avatar's 0.825 arm span to 0.505 by swinging arms that were already out backward, and `dance`/`wave`/`punch` all pulled the arms inward. `fromAuthoredByRole` now conjugates each rotation by the shortest arc between the two rest bone directions, which transports the deviation into the import's own frame. Measured on that avatar: `wave` 0.670 -> 0.795 span with the arm raised instead of pulled in, `punch` stops flinging the arm sideways (1.048) and points it forward, `run`/`jog` gain a real fore-aft arm swing, `breathe` holds the span at exactly the 0.825 bind, and a 90-degree authored key now turns the arm 12.80 degrees from its own rest exactly as authored where verbatim transfer was 100.31 degrees off. The bone axis is chosen by following the role chain (Hips->Spine, Chest->Neck) and only falls back to longest-child, because a longest-child rule takes a thigh for Hips and a clavicle for Chest. **Two limitations remain, stated rather than hidden.** (1) `t_pose` and `a_pose` are still refused, and alignment is why: a clip that *means* "arms horizontal" cannot be carried by a relative transform onto a skeleton that already binds horizontal — the same arc that fixes `wave` would put those arms overhead. They are calibration poses, and an import's bind pose is its own calibration, so 14 clips bind, not 16. (2) Authored *translations* are dropped and reported per track: the library is scaled for a 1.75 m figure and an imported file gives no source rig to derive a ratio from, so there is no root motion on this path. The table of authored rest directions is hard-coded and `animation_system_test` rebuilds the real rig and compares all 14 axes (worst cos 1.000000), so a change to the rig's proportions cannot leave it quietly stale. |
| `mylove_rig_prepared.glb` specifically | **IMPORTS, RENDERS, AND CAN NOW BE DRIVEN — HAS NO ANIMATION OF ITS OWN** | 13,221,520 bytes; 163,756 vertices / 144,073 triangles; 1 skin, 20 joints (21 adopted), inverse bind matrices present, **17 of 17 humanoid roles**, 0 animations, 0 morph targets, and `EXT_texture_webp` as a REQUIRED extension (both WebP images decode: 33,554,432 texture bytes). Through the engine: `rc=0`, `joint_count=21`, `imported_clips=14`. Linear blend skinning of its real mesh was measured per clip — `dance` moves 100.0% of coordinates (max 0.3262, 24.5% of the model's own extent), `squat` 83.3%, `walk` 83.6%, `run` 36.6%, `turn_360` 66.7%, `pushup` drops head-to-foot height from 0.870 to 0.067 as the body goes prone. It is *not* broken and it is *not* animated by itself: it is a rigged sculpt that needs either clips added to the file or the engine's library retargeted onto it, and the second is what now happens. Verified on the host only — never on a device, and the second file `mylove.glb` (9,287,272 bytes) has **0 skins** and cannot be driven at all. |
| Generated-avatar memory footprint | **REAL + HOST-VERIFIED; DEVICE UNVERIFIED** | 79.77 MiB GLB / 823,760 vertices / 1,103,656 triangles / 52 blendshapes, with 68.00 MiB of decoded texture memory and 41.14 MiB peak field memory, measured by running the production native generation path on the host and pinned by `imported_glb_pbr_regression`. Never measured on a device; GPU-side memory (mipmaps are generated on the GPU, `mip_levels = 1` in the reported views) is **not** included in any figure here. |
| Viewer can read the file the engine generates | **IMPLEMENTED IN SOURCE, COMPILE-VERIFIED; BEHAVIOUR UNVERIFIED** | `readBounded` allocates once from a known file length instead of doubling a stream, and refuses a model above half of `Runtime.maxMemory()` with the size, budget and heap in the message. The Debug APK job compiles it; the instrumented job that reported the original `OutOfMemoryError` still fails, now on a process crash whose cause the next run is expected to name. |
| Instrumented renderer job can explain its own failure | **REAL + VERIFIED** | Totals, per-test assertion messages, Gradle errors and logcat crash blocks reach check-run annotations and the job summary — the only CI output reachable from a sandbox, since artifacts and raw logs are served from blob hosts that are not. 14 host-run assertion groups. Demonstrated on real runs: it surfaced the `readBounded` `OutOfMemoryError` with its exact allocation size, then `tests=1 failures=1` plus `Process crashed`. |
| `GeneratedAvatarRuntimeTest` (production SurfaceView route) | **MISSING FROM CI — DELIBERATE** | Four tests, timeouts 900 s + 600 s + 600 s + 600 s, against a 35-minute job budget on a software rasterizer. Adding them while the smoke suite is red would replace a diagnosable failure with a silent timeout. To be enabled once the smoke suite passes and its real runtime is known. |
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
