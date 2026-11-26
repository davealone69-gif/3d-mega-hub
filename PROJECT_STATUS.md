# Project status

Last updated: 2026-11-26 (root cause 16 and its History entry: the forward-pass LOD selection was activated; the rest of this file reflects the 2026-11-14 state). The README carries the current results; this file holds the detail behind them and the history that must not be lost.

## Repository and branches

| Item | State |
|---|---|
| Repository | [davealone69-gif/3d-mega-hub](https://github.com/davealone69-gif/3d-mega-hub) |
| `main` | `aaee4443dbf53ef4707cce01d99f71a59e903ec7` (2026-10-29). Not modified. |
| Integration branch | [`integration/avatarforge-renderer-port`](https://github.com/davealone69-gif/3d-mega-hub/tree/integration/avatarforge-renderer-port) at `c08d377`. The single line that goes to `main`. It moved from `a424113` to `c08d377` on 2026-11-14 (seven commits, 18:21–18:32 UTC) while the working branch was in progress. |
| Working branch of this handover | `arena/1a5b3043-3d-mega-hub`. It contains the integration branch up to `c08d377` (merge commit `79fb562`), so it lands as a fast-forward through [PR #12](https://github.com/davealone69-gif/3d-mega-hub/pull/12). |
| PR #9 | Open. Integration branch to `main`. The only review path to `main`. |
| PR #8 | Closed on 2026-11-09 without merging (head `41c3069`). Superseded by PR #9. Its Filament-only viewer conflicts with the selected renderer split. |
| `test/photo-realistic-avatar-overlay` | Merged into the working branch with history (adds the photo-overlay quality backlog only). |
| Force-pushes | None. |

## Component baseline

| Component | Status | Evidence |
|---|---|---|
| Android shell: builds and launches | **REAL + VERIFIED** on CI and emulator | Debug APK, lint, unit tests and R8 pass; debug and signed release APKs install and launch on an API 30 emulator |
| AvatarForge engine: native generation, export, import | **REAL + VERIFIED** on host and emulator | Seven native regression tests pass on CI; an avatar is generated, validated, exported and re-imported on the emulator |
| GLES renderer: real geometry | **REAL + VERIFIED** on emulator | Imported and generated GLBs draw through native OpenGL ES 3.1; PixelCopy of the real SurfaceView finds the geometry |
| Filament: native initialisation and lifecycle | **REAL + VERIFIED** on emulator, including the minified release APK | The bundled sample loads and presents a frame in both builds |
| GLB importer | **REAL + VERIFIED** | Geometry, PBR materials, PNG, WebP and JPEG textures (JPEG tested on the emulator since `50bbe25`). Since `96152cd` an absent `baseColorFactor` on a textured material is 1, as glTF defines it; before, such a texture was drawn a fifth too dark. Since `b636f23` the skeleton and clips of a file this engine wrote are adopted (71 joints, 15 animations; played on the GPU since `a5e93a8`, proved on the CI emulator). A foreign skeleton and its clips are not applied; morph targets are not read. 8,000 mutated files on CI and 90,000 under sanitizers on a host: no crash, no out-of-range index in an accepted file |
| GLB exporter | **REAL + VERIFIED** | Khronos validator: 0 errors, 0 warnings after `fb0878e` |
| Asset pipeline: copy, validate, hash | **REAL + VERIFIED** on emulator | Imports are copied to app storage and structurally validated; generated GLB hashes are recorded in the evidence |
| Camera: orbit, zoom, reset | **REAL + VERIFIED** on emulator for orbit | A touch drag changes the measured yaw and the rendered pixels. Pinch and double-tap are implemented and not exercised by a test |
| Diagnostics | **REAL + VERIFIED** on emulator | Every number is measured on the render thread or counted by the renderer. Saved reports name the installed APK by SHA-256, the device and the asset by SHA-256; see BUILD_AND_VERIFY.md |
| Native crash containment | **REAL + VERIFIED** on emulator | Injected C++ exceptions arrive as Java exceptions and the process keeps running |
| Signed APK and AAB | **REAL + VERIFIED** with a one-run CI key | Read-back verification and byte-identical rebuild. Production signing is **BLOCKED** on the owner keystore |
| Samsung SM-S921B | **UNVERIFIED** for this code | Requires the owner's test of the exact APK |

## Root causes established

1. **Red renderer gate on `a424113`.** The session ended itself unless three frames were presented within 15 seconds of creation, and the render thread then replaced the failure text with "AvatarForge renderer stopped." The integration head's own log shows it (run `43627204964`, job `132545683617`): the session was ended after two correct frames, "frames=2 presented=2 framePresented=true draws=29 triangles=1626176 boundsInFrustum=true ... glError=0x0 eglError=0x0". On the software-rendered emulator the first frame of that avatar takes 10.4 s and the median interval between later frames is 5.1 s (run `43635337124`), so three frames cannot arrive in 15 seconds. The rule is now: fail only when no valid frame at all arrives in 15 seconds; a slow renderer keeps running and reports its measured numbers; the failure text is kept.
2. **`Manipulator.nCreateBuilder` implementation not found.** `ModelViewer` from filament-utils was constructed after `Filament.init()` without `Utils.init()`, so `libfilament-utils-jni.so` was never loaded (commit `4fa9178` records it; `3856bb6` removed `ModelViewer`; `5974328` added the R8 keep rule). No code imports `filament.utils` any more, and the dependency and its native library are now removed from the build.
3. **Invalid glTF from the exporter.** 542 Khronos validator errors: three-element bounds on scalar animation-time accessors, and no bounds on morph-target position accessors. Fixed in `fb0878e`.
4. **Diagnostics that were not measurements.** Frame time and frame rate came from a clamped delta handed to the animation system, CPU time and texture bytes were never written, and the production path did not collect GL errors. All are now measured or counted.
5. **32-bit packaging.** The app had no ABI filter, so Filament's 32-bit libraries would have been packaged without the engine's. The app now packages `arm64-v8a` and `x86_64` only, and the release gate checks that every packaged ABI carries the same libraries.
6. **APK not reproducible.** Two clean builds differed only in signing block `0x504b4453`, the dependency list that the Android Gradle plugin encrypts with a random key. It is now left out of the APK (it stays in the AAB).
7. **PixelCopy timeouts on a slow GPU.** The copy was requested immediately after the swap, while the software GPU was still drawing the queued frame, and PixelCopy answered with its timeout code (`result=2`) on every attempt. The render thread now finishes the frame's GPU work before the request and holds the next frame until the copy is answered.
8. **Input-dispatch timeout between tests.** One instrumented test left its viewport open, still drawing a 1.1-million-triangle avatar, which starved the next test's window on the software GPU. Each test now closes the app and waits for the native session to be released.
9. **Reports that could not be attributed.** A saved renderer report named the asset and its size and nothing else, so the owner's phone test of 2026-11-13 could not be tied to a commit. Reports now carry the SHA-256 of the installed APK, the device model and Android version, and the SHA-256 of the asset; the panel shows the same lines.
10. **C++ exceptions at the JNI boundary.** The engine's C API caught its own exceptions; the renderer entry points did not, and an exception that unwinds into the JVM aborts the app. They now run inside one barrier that turns the exception into a Java exception.
11. **Vulkan in an OpenGL ES app.** `libafrender.so` compiled the unused Vulkan backend and linked `libvulkan.so`. The backend is now built only with `-DAF_ENABLE_VULKAN=ON`, and the release gate reads the dependencies back.
12. **Signing password in the job log.** The one-run preview keystore password was written to the job environment unmasked and printed by later steps. It is now masked. The key was discarded with the runner and never uploaded.
13. **Phone-test APK not inspected.** Only the SHA-256 of the debug APK was recorded; the owner's readiness rule also asks for its application ID and signing certificate. The diagnostic workflow now reads the file back before the emulator tests and fails on a mismatch.
14. **Unbalanced EGL display reference.** If `eglInitialize` failed, teardown still called `eglTerminate`. Android counts these calls per process, so the extra call would have released a reference held by the UI toolkit or Filament. Found in review; no failure was observed. The handle is now cleared on that path.
15. **Phone-test APK link was not a direct download.** The readiness record linked to a workflow artifact. Such a link downloads only for a signed-in GitHub account and delivers a zip, and the owner reported that the APK did not download. The owner's gate asks for a direct link, and this repository once published passing debug builds as release files (`78be2f5`); that job was lost in `2a1352c` and was not restored. `publish-phone-test-apk.yml` now attaches the verified APK to a pre-release, only when the readiness gate holds for its commit, and downloads it back without credentials to compare. What exactly failed on the owner's device was not observed.
16. **The forward colour pass never used its own LODs.** `AvatarLodSet` builds three index-only levels (LOD1 ~12% and LOD2 ~5% of the source triangles) and `af_gl.cpp` selects between them per submesh, but the selector keyed off two absolute camera distances, `>= 8 m` for LOD1 and `>= 16 m` for LOD2. An avatar is ~1.8 m tall and Avatar Studio's orbit camera defaults to `CameraState::distance = 3.0 m`, so neither threshold is reachable while a user looks at an avatar: every forward frame drew LOD0, the full source mesh. That is the phone measurement in `docs/PHONE_TEST_2026-11-15.md` -- 1,100,168 triangles over 194,401 pixels, about a third of a pixel per visible triangle -- and it is why the HIGH quality tier did not hold 30 FPS (repair R3 in `docs/PHOTOREAL_UPGRADE_PLAN.md`). The shadow pass did use LOD2 correctly, which is why the fault was not obvious in the draw count. Two latent defects sat behind it: (a) the forward-pass bounds re-check validated the chosen LOD range against `indexCount_` (the *source* count), but LOD1/LOD2 ranges live in the appended region of `lodIndexIbo_` at offsets `>= indexCount_`, so had a coarse level ever been selected the draw would have been rejected and `geometryValid_` cleared; and (b) `triangles_` accumulated `sm.index_count` (the source count) rather than the count actually drawn, so the telemetry would have over-reported the moment a LOD took effect. Fixed by selecting one level per frame for the whole avatar from screen-space density (`af_lod_select.h`: the finest level whose mean projected triangle edge is at least `kAvatarLodPixelEdgeTarget` px, with hysteresis, falling back to the exact source mesh whenever the inputs are not measurable), validating the draw range against the bound buffer's real size, and counting the triangles actually submitted. **REAL + VERIFIED on host** for the selection maths and the per-level surface-area measurement (`avatar_lod_select_regression`, added to CTest; the existing 10 native tests still pass), checked against the recorded S24 numbers: a full-body view now selects LOD1 (~132k), a 1 m close-up earns LOD0 back, a distant view falls to the coarsest. **IMPLEMENTED IN SOURCE** for the `af_gl.cpp` wiring, which parses clean under `g++ -fsyntax-only -Wall -Wextra` against the Khronos GLES/EGL headers (`tools/shader_check/check_android_native.sh`); the on-device frame-rate and pixel effect is **UNVERIFIED** -- it needs the NDK build and the emulator/phone gates, not run here.

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
| Viewer for imported and generated GLBs | `AvatarForgeSurfaceView`, native OpenGL ES. The Vulkan backend source is kept but not compiled in, and no packaged library links `libvulkan.so`. |
| Viewer for the bundled sample | `FilamentPreviewView` |
| Bundled fixture | `app/src/main/assets/models/female.glb`, from 3DDD revision `be5bf17d8afe46247646465941fa1b7317d8e415` (source blob `a1c00c6b31f6ab0db3b2e68243526cb2d0e15967`). Corrected 2026-11-15: not byte-identical. Commit `98dd9ec` rewrote the JSON header, so the file here is blob `ab9d7bcbd36678636f051fb8032d3fd035be37ee` (418,312 bytes) with the same mesh and morph data. One mesh, seven morph targets, no skin, no animation, no images. Seven Khronos validator errors (morph targets without bounds). A renderer fixture, not a reference avatar. |
| Toolchain | AGP 8.7.3, Kotlin 2.1.0, Gradle 8.11.1, NDK 26.1.10909125, CMake 3.22.1, compile and target SDK 35, min SDK 26 |

The AvatarForge donor pin is `9e36197ace3b18faa6c566d93f8fce89b185c45d` from `work/android-engine-dev`. It diverges from the standalone AvatarForge RB-01 line (`0e78216643d1a305be7d278011d6732e56d62d62`, merge `fc30efd0db3d7892cfee388a783508ea18f32471`); generation-core files differ in both directions. Do not replace either tree wholesale. AvatarForge remains a separate project.

## Missing features

| Feature | Status |
|---|---|
| Skeletal animation of the model shown in the app | No longer missing; **not verified on the phone**. An avatar made here plays its 15 built-in clips on the GPU (CI emulator, since `a5e93a8`). A rigged file from elsewhere plays its own clips in the glTF viewer, chosen by name (CI emulator with a test figure, since `ff9e6ba`). Clips made for another skeleton cannot be played on an avatar made here: retargeting is **MISSING** |
| Posing and facial animation | **MISSING**. Blendshapes are written to the file and dropped on import; nothing drives them. |
| Voice lip-sync and visemes | **MISSING**. Text-to-speech output works. |
| Embedded language model | **MISSING**. Ollama is an external service. |
| Photo-to-identity reconstruction | **MISSING**. Prompts are parsed deterministically. |
| External-resource `.gltf` | **MISSING**. Self-contained GLB only. |
| Renderer recovery after a lost GL context while the surface stays alive | **MISSING**. The session reports the stall and is rebuilt when the surface is recreated. |

## Known defects, recorded and not changed

The review's open findings, with severity and a suggested action for each, are in [docs/BRAVO_REVIEW.md](docs/BRAVO_REVIEW.md). The ones recorded before the review:

- Left and right are not named consistently in the generation core. See [docs/AVATAR_GEOMETRY_EVIDENCE.md](docs/AVATAR_GEOMETRY_EVIDENCE.md).
- Generated avatars face `−Z`; glTF's front is `+Z`. The app compensates for its own files only.
- The debug-only PixelCopy check encodes a PNG and writes files on the UI thread once per session.
- An exported GLB is not byte-identical between a Linux host and Android (284 bytes apart for the diagnostic avatar).

## History

**2026-11-26, forward-pass LOD selection activated (repair R3, render side).** The single-avatar renderer had built three index-only LOD levels since `af_avatar_lod.*` landed, but the forward colour pass chose between them from two absolute camera distances (`>= 8 m` → LOD1, `>= 16 m` → LOD2). Avatar Studio's orbit camera defaults to `CameraState::distance = 3.0 m` and an avatar is ~1.8 m tall, so neither threshold is reachable in normal use: every forward frame drew LOD0, the full source mesh. This is the density the owner's phone recorded (`docs/PHONE_TEST_2026-11-15.md`: 1,100,168 triangles over 194,401 pixels, ~⅓ px per visible triangle; HIGH tier did not hold 30 FPS). The shadow pass already used LOD2, which masked the fault in the aggregate draw count. Recorded as root cause 16.

- **Change.** `af_lod_select.h` (new, pure maths, no GL/Android dependency) selects one level per frame for the whole avatar: the finest level whose mean projected triangle edge is at least `kAvatarLodPixelEdgeTarget` (4.0) px, with hysteresis (`kAvatarLodHysteresis` 1.25) so an orbit on a threshold does not flip the silhouette every frame, falling back to the exact source mesh whenever any input is not measurable. `AvatarLodSet` now MEASURES each level's rest-pose surface area (summed from its triangles, once per topology change) and exposes `surfaceAreaM2` / `meanTriangleAreaM2` / `areasValid`. `af_gl.cpp` computes the view distance from the live camera eye to the REST bounds centre (not the skinning-inflated `scene_.modelMin/Max`), builds the per-level screen metrics and calls the selector. Two latent defects behind the dead path were also fixed: the forward-pass draw-range re-check now validates against the bound buffer's real size (`avatarLods_.indices().size()` when LOD indices are bound) instead of the source `indexCount_` -- a coarse level's range starts at or after `indexCount_`, so the old check would have rejected every LOD1/LOD2 draw and cleared `geometryValid_`; and `triangles_` now accumulates the count actually submitted (`drawIndexCount`) rather than `sm.index_count`, so the telemetry no longer over-reports when a LOD takes effect (rule: no invented numbers).
- **Verified on host (REAL + VERIFIED).** New CTest `avatar_lod_select_regression` (`avatar_lod_select_test.cpp`) exercises the projection maths, the measured areas, and the selection against the recorded S24 framing: full body at 3 m selects LOD1 (~132k, inside the plan's 100-150k band), a 1 m close-up earns LOD0 back, a 12 m view falls to the coarsest, and both hysteresis directions hold within the dead band. The existing 10 native tests still pass (11/11). `af_gl.cpp`, `af_render.cpp`, `af_render_capi.cpp` and `avatarforge_renderer_jni.cpp` parse clean under `g++ -std=c++17 -fsyntax-only -Wall -Wextra` against the Khronos GLES/EGL headers via `tools/shader_check/check_android_native.sh` (one pre-existing `-Wunused-variable` in the shadow pass is unchanged and unrelated).
- **Not verified here (UNVERIFIED on device).** The GLES renderer is Android-only; no NDK build, emulator run or phone test was performed in this workspace. The on-device frame-rate, quality-tier and pixel effect of the change is therefore IMPLEMENTED IN SOURCE and UNVERIFIED until the `android.yml` / `avatarforge-generated-runtime.yml` emulator gates and an owner phone test run on the exact merge SHA. Nothing here changes geometry, skinning or morphs: every level still addresses the engine's original vertex stream.

**2026-11-14 18:21–18:32 UTC (dated 2026-11-15 in the owner's local time), parallel update on the integration branch.** Seven commits (`78cb75d` to `c08d377`) were pushed to `integration/avatarforge-renderer-port` while the working branch was in progress. They are merged into the working branch with their history. The update is kept here as written:

> - Active implementation: `integration/avatarforge-renderer-port`, PR #9. Stable `main` remains unchanged at `aaee4443dbf53ef4707cce01d99f71a59e903ec7`.
> - PR #8 is closed and unmerged; its head is `41c3069301dc772bad3aa8dba1a2cbf67f8110c9`. Do not use it as the work branch.
> - **Implemented:** `78cb75d81557623622f6989a285ebf958dea2844` preserves the original renderer and teardown failures in the final worker status. Diagnostic observability only, not a renderer fix.
> - **Implemented:** `ae43e0e271684d56336005450765a4577aafc591` fixes GLES 3.x extension enumeration. GLES 3.0+ requires `glGetStringi(GL_EXTENSIONS, index)`; the removed legacy extension-string query could return `GL_INVALID_ENUM` and incorrectly disable ES 3.1 extension detection. Float colour-buffer and anisotropy support are now reported from actual capabilities.
> - **Verified on prior exact head `2a5485c17ec656659c4841826a1048d1577b7301`:** signed ephemeral preview APK/AAB build and signature verification (run `43626247432`, job `132542586035`); Android emulator install/launch smoke (job `132542586459`); native host regression (run `43626247376`, job `132542664347`). These results do not validate the later GLES source change.
> - **Latest source head:** `ae43e0e271684d56336005450765a4577aafc591`. Fresh exact-head CI must validate the GLES capability change. **UNVERIFIED:** real imported/generated avatar visible pixels and Samsung SM-S921B runtime. **BLOCKED:** production signing pending owner keystore in protected CI secrets.
> - Keep `main` unchanged; no merge until exact-head renderer evidence passes.

What was checked about that update:

- Its CI claims hold. Run `43626247432` on `2a5485c` succeeded with jobs `132542586035` and `132542586459`; job `132542664347` of run `43626247376` succeeded (the run as a whole was cancelled).
- `78cb75d` and this branch fixed the same masking of the failure text. The merge keeps this branch's teardown and takes from `78cb75d` the rule that a renderer failure and a teardown failure are reported together.
- With the failure text visible, the integration head `c08d377` showed the cause in its own log (run `43627204964`, job `132545683617`): "No drawable EGL-presented model frame within 15 seconds (frames=2 presented=2 framePresented=true draws=29 triangles=1626176 boundsInFrustum=true outsideCorners=0 glError=0x0 eglError=0x0)". The renderer had presented two correct frames; the rule demanded three. That is root cause 1, and `c08d377` still fails both pixel gates because it does not change the rule.
- `ae43e0e` is kept: the indexed query matches whole extension names, and the anisotropy capability is now queried instead of assumed. The reason given for it is not correct. OpenGL ES 3.x keeps `glGetString(GL_EXTENSIONS)`; only desktop core profiles removed it. No measured session reports a GL error with either form of the query (`glErrorFrames` 0, `lastGlError` 0x0). The code comment was corrected in `6b05ee4`.


**2026-11-13, owner phone test.** The owner tested an installed APK on the phone: the renderer displayed the avatar, the generation button changed avatar features including male and female, and GLB import worked. Skin-tone and hair changes were not tested. Avatar quality was too poor to judge appearance changes. The APK's SHA-256 and commit were not recorded, so this is **PARTIAL** evidence for an earlier build and **UNVERIFIED** for the current code. The avatar-quality plan that followed is in [docs/AVATAR_QUALITY_PLAN.md](docs/AVATAR_QUALITY_PLAN.md).

**2026-11-14, verification ledger of the previous README.** Kept for traceability. None of it is evidence for the current code.

- Native host regression, Android unit tests and R8 passed on application source `1deaf264032195989257845b884a5829f06fe9cf`. Lint was cancelled in that cycle.
- A diagnostic workflow produced a 71,377,227-byte debug APK with SHA-256 `3d833528d116fe0660a347d6a3bff0e9c6e7c8dc1d53b82125f15718917de8ee`.
- Run `43528943730` on source `e9e7d76ef4a3f12c5c6a79951b5a1a99570c6228`: the basic generated-avatar pixel test passed on the Android 11 emulator. That was not the production test of an exported GLB through the SurfaceView.
- Run `43534561147`, job `132260325291`, head `705165eebdaa525d75be861763c9a11205f33634`: `generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView` failed. LOD2 generation reduced the model to 325,491 triangles, but the production renderer presented only 2 frames in 15 seconds.
- Run `43534561148` on the same head: unit tests, lint, R8, debug APK build and native regression passed; the instrumented GLES renderer smoke failed, and the bundled-GLB instrumentation hit `keyDispatchingTimedOut`.
- A fresh debug APK artifact existed for that head and its ZIP digest matched GitHub's artifact digest. Package and signing-block verification of the APK was not complete, so it was not a ready APK.
- The previous README itself is in the Git history of this branch, last at commit `3facc3d`.

**2026-11-14, integration branch at `a424113`.** Nine checks passed and two failed, both in `generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView` (root cause 1). An earlier head, `705165e`, had failed the same way ("presented only 2 frames in 15 seconds") and had also hit `keyDispatchingTimedOut` (root cause 8).

**2026-11-11, consolidation.** The repository had 15 branches. `integration/avatarforge-renderer-port` was selected as the single integration line; dispositions are in the tables above. Commit `46b13ee` passed lint, unit tests, debug APK, R8, native regression and the signed preview; its emulator smoke failed and was corrected in `42da0e3` and `5b8f6c1`.
