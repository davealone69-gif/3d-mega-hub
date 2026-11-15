# 3D Mega Hub

One Android app that generates, imports and renders 3D avatars on the phone. The AvatarForge engine generates and imports; a native OpenGL ES renderer draws; Filament shows the bundled sample.

**Handover of 2026-11-15 (UTC). PR #12 remains the working review path and is not merged to `main`. The owner has now installed and tested the new APK on a Samsung SM-S921B: general app operation is reported good; avatar appearance and animation capability remain unfinished.**

## Where the work is

| Item | Value |
|---|---|
| Working branch | [`arena/1a5b3043-3d-mega-hub`](https://github.com/davealone69-gif/3d-mega-hub/tree/arena/1a5b3043-3d-mega-hub) |
| Review path | [PR #12](https://github.com/davealone69-gif/3d-mega-hub/pull/12) into `integration/avatarforge-renderer-port`, then [PR #9](https://github.com/davealone69-gif/3d-mega-hub/pull/9) into `main` |
| Current implementation/test head | `797d1567ab8972a4a6930e12f51ee149c5dc877` (`797d156`). PR #12's description records the four workflow runs and exact phone-test APK artifact for this head. The detailed historical measurements below are explicitly from older commit `3facc3d`, not this head |
| README update | This handover is updated after the owner's 2026-11-15 Samsung test. The README-only update does not change app code or the tested APK |
| Record for the head commit | In the description of [PR #12](https://github.com/davealone69-gif/3d-mega-hub/pull/12). A commit cannot contain the results of its own workflow runs, so the record for the head, with the APK to install, is kept there |
| `main` | `aaee4443dbf53ef4707cce01d99f71a59e903ec7`, not modified. No force-push anywhere |

The rule is one implementation line, `integration/avatarforge-renderer-port`. This handover was prepared on the working branch above, which contains the integration branch up to `c08d377`. Once PR #12 has landed, the integration branch is the single line again. Do not open another implementation branch.

Status labels: **REAL + VERIFIED**, **UNVERIFIED**, **PARTIAL**, **MISSING**, **BLOCKED**. "Verified on the emulator" never means verified on the phone.

## Status against the definition of done

| # | Item | Status | Evidence |
|---:|---|---|---|
| 1 | README reconciliation pushed safely | **REAL + VERIFIED** | Two outside README changes merged with their history (`6d0194b`, `79fb562`); nothing overwritten. [Reconciliation](#reconciliation) |
| 2 | Integration candidate reviewed and resolved | **REAL + VERIFIED** | Six named branches compared by ancestry and content: four are already contained, two are superseded. [PROJECT_STATUS.md](PROJECT_STATUS.md) |
| 3 | Engine integration demonstrated | **REAL + VERIFIED** on emulator | Generate, validate, export, import and render inside the app: run [43642406837](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43642406837) |
| 4 | Native start-up issue resolved | **REAL + VERIFIED** on emulator | `Manipulator.nCreateBuilder`: cause found, dependency removed, release APK launches. [Root causes](#root-causes) |
| 5 | Real GLB loading | **REAL + VERIFIED** on emulator | Imported and generated GLBs load through the engine importer. Static meshes only |
| 6 | Actual OpenGL ES rendering | **REAL + VERIFIED** on emulator | PixelCopy of the real surface: 560,418 geometry pixels, 0 GL errors |
| 7 | Camera controls | **PARTIAL** | Drag orbit is verified by a test. Pinch zoom and double-tap reset are implemented and have no test |
| 8 | Genuine runtime diagnostics | **REAL + VERIFIED** on emulator | Measured on the render thread; reports name the APK, the device and the asset by SHA-256 |
| 9 | Basic Samsung install/launch and general app operation | **REAL + VERIFIED (owner-reported)** | Owner confirmed the new APK was installed and tested on the Samsung SM-S921B on 2026-11-15 and reported general app operation good. Detailed crash/lifecycle stress and diagnostic-log review remain **UNVERIFIED** because the JSON reports have not been supplied |
| 10 | Android build succeeds | **REAL + VERIFIED** | Unit tests, lint, R8, debug APK, native regression: run [43642402022](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43642402022) |
| 11 | Signed APK verified | **PARTIAL** | Verified and reproducible with a one-run CI key. Production signing is **BLOCKED** on the owner's keystore |
| 12 | Signed AAB verified | **PARTIAL** | Same |
| 13 | SHA-256 values recorded | **REAL + VERIFIED** | [Artifacts](#artifacts) |
| 14 | Bravo review completed | **PARTIAL** | A separate review pass was done and recorded by the same agent that did the integration, not by an independent reviewer. [docs/BRAVO_REVIEW.md](docs/BRAVO_REVIEW.md) |
| 15 | Physical Samsung test | **REAL + VERIFIED (owner-reported)** | The owner confirmed on 2026-11-15 that the new APK was installed and tested on the Samsung SM-S921B and that general app operation is good. Exact phone-side telemetry/diagnostic JSON has not been supplied, so detailed performance, thermal, memory and lifecycle measurements remain **UNVERIFIED**. |
| 16 | Final README handover | **REAL + VERIFIED** | This file |

## Next action

The owner has completed the initial Samsung APK test and reports that the app is generally working. Do not keep treating the basic phone install/launch check as pending. The next work is avatar completion, not another broad renderer rewrite:

1. Diagnose and improve the generated avatar's visible appearance at the source: body construction, proportions, mesh deformation, surface/material generation and export. Compare actual generated GLBs and screenshots against agreed visual acceptance criteria; do not infer quality from successful rendering or GLB validation.
2. Implement a complete usable animation feature, because the owner confirms there is currently no way in the app to animate an avatar. This includes a supported animation source/clip pipeline, skeleton/skin preservation through generation/export/import, runtime playback and user-facing controls (at minimum select/load, play, pause, stop, seek and loop where supported). Do not claim animation works merely because a GLB contains joints or clips.
3. Keep the working renderer and current tested APK baseline intact. Make focused changes on the existing implementation line; do not create a competing app or parallel implementation branch.
4. Capture phone-side `renderer_telemetry.json` and `renderer_diagnostics.json` when practical. These are still needed to verify detailed Samsung performance, thermal, memory and lifecycle behaviour, even though the owner has confirmed the APK itself works generally.
5. Update this README with each evidence-backed milestone. A feature is not **REAL + VERIFIED** until it is built, installed and exercised on the phone.

For production signing: add the keystore and its three passwords as protected repository secrets. Never commit them.

## What is implemented

- **Generation.** The AvatarForge C++ engine builds an avatar from a text prompt, validates the deformed mesh and exports a GLB that the Khronos validator accepts with 0 errors.
- **Import.** A self-contained GLB from the file picker or an HTTPS link is copied into app storage, limited to 128 MiB, structurally validated, then read by the engine importer: geometry, PBR materials, PNG and WebP textures.
- **Rendering.** Imported and generated GLBs are drawn by the native OpenGL ES 3.1 renderer on its own thread. The bundled sample is drawn by Filament. No packaged library links Vulkan.
- **Camera.** Drag to orbit, pinch to zoom, double-tap to reset.
- **Diagnostics.** Frame rate and frame-time percentiles, draw calls, triangles, import and first-frame times, requested GPU bytes, heap, GL and EGL error counts, native session counts. Saved to `Downloads/3D Mega Hub Diagnostics` with the SHA-256 of the installed APK, the device model and the SHA-256 of the asset.
- **Crash containment.** A C++ exception in the renderer becomes a reported renderer failure instead of an app abort.
- **Release engineering.** The signed APK and AAB are read back and checked against the Gradle configuration; two clean builds must be byte-identical. The debug APK for the phone is inspected, downloaded again on a second machine and tied to the test that ran it. It is published as a direct download only when the readiness gate holds for its commit.

Not implemented: an end-to-end animation feature or any user-facing way to animate an avatar. The current GLB import/render path displays static meshes; the imported mesh has 0 joints in renderer telemetry even though the generated GLB carries joints and blendshapes. Animation clips, skinning, posing, facial animation and playback controls are therefore **MISSING**, not merely hidden or unverified. Avatar appearance quality is also **UNFIXED** per the owner's 2026-11-15 phone feedback. Lip-sync, an embedded language model and photo-based identity are also not implemented. See [PROJECT_STATUS.md](PROJECT_STATUS.md).

## Build and test results for `3facc3d`

All four workflows passed. The emulator is Android 11 (API 30), x86_64, software GPU.

| Workflow | Run | Result |
|---|---|---|
| Android CI | [43642402022](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43642402022) | Unit tests, lint, R8, debug APK, seven native tests, imported-GLB render smoke (three instrumented tests): pass |
| Diagnostic APK and tests | [43642404867](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43642404867) | Six native tests; debug APK inspection; generated avatar drawn directly and through the production surface; the published APK downloaded and verified on a second machine: pass |
| Generated-avatar runtime | [43642406837](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43642406837) | Five instrumented tests including the native exception barrier: pass. Started by PR #12, so it built GitHub's merge commit `38fc7b7`, whose tree is identical to the evidence commit's |
| Release preview | [43642407532](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43642407532) | Signed APK and AAB verified and reproducible; debug and signed release APK install and launch: pass |

Earlier commits on this branch passed the same four workflows: `6b05ee4` (runs 43638793711, 43638796200, 43638799198, 43638799074) and the merge commit `79fb562` (runs 43637644842, 43637647380, 43637634111, 43637649884).

## Artifacts

GitHub keeps these artifacts for 14 days. Downloading an artifact needs a signed-in GitHub account and delivers a zip. The debug APK for the phone does not depend on that: it is also published as a plain `.apk` on a pre-release, linked from PR #12.

### Debug APK of the evidence commit

The APK to install on the phone is the one named in PR #12. This is the same record for the evidence commit; the app source is identical, the signing key and therefore the SHA-256 are not.

| Field | Value |
|---|---|
| File | `3D-Mega-Hub-generated-avatar-diagnostic-debug.apk`, 55,801,294 bytes |
| SHA-256 | `bb8f9e71b32785dc8393de39c1668278b38845fab3ed4290d22893cf207c0d4f` |
| Read back from the file | `com.davemega.hub`, versionCode 1, versionName 0.1.0, minSdk 26, targetSdk 35, debuggable, ABIs `arm64-v8a` and `x86_64` |
| Signature | APK Signature Scheme v2 verifies. Certificate SHA-256 `5137bfea14d4758c91fda1abf7dd444fd401edaceb8f5dc27af4d3f7d91def72` (`CN=Android Debug`, created on the CI runner for that one job) |
| Chain | Downloaded again on a second machine and found identical to the file the build job inspected, the file named in `SHA256SUMS`, and the APK the app reported for itself while the production render test ran: `DOWNLOAD_VERIFY … chain_complete=true`, job [132595324383](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43642404867/job/132595324383) |
| Download | [artifact `3D-Mega-Hub-generated-avatar-diagnostic-debug-apk`](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43642404867/artifacts/14132579570) (zip SHA-256 `19da8173e5f544637cf4f896da4e824edac14075c879dc957a81b8dff29386e9`) |

### Signed preview

Signed with a key that existed for one CI run and was discarded. It proves the release process, not the production identity.

| File | Bytes | SHA-256 |
|---|---:|---|
| `3D-Mega-Hub-preview-release.apk` | 46,185,764 | `38bfe049f503bcd441c57b8d5830dd048fb5766d5e0c01622b0d8551b6cd6d3e` |
| `3D-Mega-Hub-preview-release.aab` | 17,283,030 | `7f91063ceadfb00763bd2e5508112ab96bba0a2783bd80e6b843e4e7d1a35f25` |

- Download: [artifact `3D-Mega-Hub-signed-preview`](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43642407532/artifacts/14132428047) (zip SHA-256 `ceef2340d1f45b99f6ef54eed146409f1659ea74a45230e1e94a2a20767adcfa`).
- Read back from both files: `com.davemega.hub`, versionCode 1, versionName 0.1.0, minSdk 26, targetSdk 35, ABIs `arm64-v8a` and `x86_64` with identical library sets.
- APK Signature Scheme v2 verified; AAB verified by `jarsigner`. Certificate SHA-256 `0740db6d75f245afbf33c174619a9227a02287995f7364f78a2cace99c8d592b` (CN=3D Mega Hub CI Preview).
- Reproducibility: APK and AAB are byte-identical across two clean builds in the same run, the second without the Gradle build cache.
- `libafrender.so` links `libEGL.so libGLESv3.so libafcore.so libandroid.so libc.so libdl.so liblog.so libm.so`. No packaged library links `libvulkan.so`.

## Runtime evidence

Read from the job logs of the evidence commit. The emulator draws with a software GPU, so its frame rates say nothing about the phone; they show that frames are produced, measured and free of errors.

| Measurement | Generated avatar through the production surface | Diagnostic avatar, imported | Bundled sample, imported |
|---|---:|---:|---:|
| Job | [132593044475](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43642404867/job/132593044475) | [132592643011](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43642402022/job/132592643011) | same job |
| GLB bytes | 116,568,812 | 83,430,028 | 418,312 |
| Vertices | 1,218,003 | 821,215 | 3,014 |
| Triangles | 1,626,176 | 1,100,168 | 5,520 |
| Draw calls per frame | 29 | 29 | 11 |
| Frame rate on the software GPU | 0.39 FPS | 0.20 FPS | 1.08 FPS |
| Frame interval, median | 3,820 ms | 4,607 ms | 961 ms |
| First frame | 7,114 ms | 7,848 ms | 840 ms |
| GLB import | 852 ms | 1,247 ms | 4 ms |
| Renderer start | 129 ms | 144 ms | 505 ms |
| Requested texture bytes | 72,701,269 | 72,701,269 | 20 |
| Requested geometry bytes | 188,784,330 | 127,890,410 | 523,180 |
| Requested render-target bytes | 126,429,184 | 77,070,408 | 77,070,408 |
| Native heap | 650,279,600 | 484,860,320 | 124,831,952 |
| GL-error frames, EGL swap failures, failed draws | 0, 0, 0 | 0, 0, 0 | 0, 0, 0 |
| Frames cut off when the screen closed | 0 | 1 (`0x300d`) | 1 (`0x300d`) |
| Native sessions after the test | 1 created, 1 destroyed | 1 created, 1 destroyed | 2 created, 2 destroyed |

- **Visible pixels.** PixelCopy of the real SurfaceView found 560,418 geometry pixels of the generated avatar (minimum 5,000), status `PIXELS_VERIFIED`. The three render-smoke tests, which include the visible-pixel check of the imported GLBs, pass.
- **Asset identity.** Generated GLB: SHA-256 `aa957ad72f83ea0c4ada65169d76845b5069dfbfde0331ec35fe83382572e9c2`, the same in every run. It carries 71 joints and 52 blendshapes; the mesh on screen has 0 joints. Diagnostic avatar: `811ad98b48a608be46d6184fdca61c041eb74a46477714e71e148d9dbcee4525`. Bundled sample: `f2002ff4201e1845097a1647108dbbaf5ad722521d067732f1765e1153bae779`, which is what the app reported and what `sha256sum app/src/main/assets/models/female.glb` gives.
- **Same avatar, second workflow.** Runtime job [132592655367](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43642406837/job/132592655367): `PIXELS_VERIFIED` with 560,418 pixels, 0.22 FPS, first frame 10,589 ms, four sessions created and four destroyed. The difference from 0.39 FPS is the speed of the CI machine.
- **Camera.** A touch drag on the bundled sample moved the measured yaw from 0° to −83.4° and changed 2,137 of 9,216 sampled pixels. On the WebP-textured GLB: 0° to −84.8°, 3,086 of 9,216.
- **Exception barrier.** Three injected C++ exceptions (`std::runtime_error`, `std::bad_alloc` and a thrown `int`) arrived as Java exceptions, and native code kept running: `BARRIER_EVIDENCE cppExceptionsConverted=3 processAlive=true`.
- **Report identity.** In all three emulator jobs the APK SHA-256 that the app reported for itself equals the installed file (`IDENTITY_CHECK … match=true`).
- **Signed release APK.** Job [132594421344](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43642407532/job/132594421344): the verified file installs, starts cold in 1,104 ms, opens Avatar, loads the sample and submits a frame.
- **Crashes.** No crash, ANR or fatal signal in the logcat of any emulator job.

## Root causes

1. **The red renderer gate.** The session ended itself unless three frames arrived within 15 seconds, then replaced the reason with "renderer stopped". The integration head's own log shows a session ended after two correct frames of 1,626,176 triangles with no GL or EGL error. A slow renderer now keeps running and reports its numbers; only a renderer that produces no valid frame fails.
2. **`Manipulator.nCreateBuilder` implementation not found.** `ModelViewer` from filament-utils was used without `Utils.init()`, so its native library was never loaded. Nothing uses filament-utils any more and the dependency is removed.
3. **Invalid exported glTF.** 542 Khronos validator errors from missing or malformed accessor bounds; 0 after the fix.
4. **Diagnostics that were not measurements.** Frame time, CPU time and texture bytes were derived or never written. All are measured or counted now.

The full list of fifteen, with the release-engineering causes, is in [PROJECT_STATUS.md](PROJECT_STATUS.md).

## Open items

| Item | Status |
|---|---|
| General APK operation on Samsung SM-S921B | **REAL + VERIFIED (owner-reported)** | Owner installed and tested the new APK on 2026-11-15 and reported that everything generally works apart from the avatar issues. Detailed telemetry, sustained performance, heat, memory and lifecycle remain **UNVERIFIED**. |
| Production signing | **BLOCKED** on the owner's keystore |
| End-to-end avatar animation pipeline | **MISSING**: current generated/imported avatar path renders static meshes; the owner has no way to animate an avatar in the app. Skeleton/skin/clip preservation, runtime playback and controls are not implemented end to end. |
| Avatar visual quality | **UNFIXED**: owner explicitly confirmed on 2026-11-15 that the appearance is still not fixed. Successful generation, export or rendering does not establish acceptable visual quality. [docs/AVATAR_QUALITY_PLAN.md](docs/AVATAR_QUALITY_PLAN.md) |
| Performance of the default avatar on the phone | **UNVERIFIED**: 1.1 to 1.6 million triangles with 4x MSAA. Measure before tuning |
| Native libraries are not stripped | **PARTIAL**: every build prints "Unable to strip". Finding O4 of the review |
| Independent review | **PARTIAL**. Seventeen open findings are listed in [docs/BRAVO_REVIEW.md](docs/BRAVO_REVIEW.md) |

## Reconciliation

- **PR #8** is closed and was never merged (closed 2026-11-09, head `41c3069`). PR #9 superseded it.
- **Parallel work.** Seven commits reached the integration branch on 2026-11-14 between 18:21 and 18:32 UTC while this branch was in progress. They are merged with their history in `79fb562`. Their README and status text is quoted in [PROJECT_STATUS.md](PROJECT_STATUS.md).
- **Earlier README.** Its geometry report is preserved in [docs/AVATAR_GEOMETRY_EVIDENCE.md](docs/AVATAR_GEOMETRY_EVIDENCE.md), its validation policy and readiness gate in [BUILD_AND_VERIFY.md](BUILD_AND_VERIFY.md), its quality plan and owner-tested ledger in [docs/AVATAR_QUALITY_PLAN.md](docs/AVATAR_QUALITY_PLAN.md), and its verification ledger and renderer investigation in [PROJECT_STATUS.md](PROJECT_STATUS.md).
- **Protected 83 MB GLB.** Regenerated from source byte for byte: 83,430,312 bytes, SHA-256 `56b072cb43203fe9c2ca0b231d28a3601b59914465647fa1da95a6890daccd6c`. It was never in the repository and still is not. Only the exporter with accessor bounds reproduces it, which is the difference between the earlier local workspace and the remote branch.
- **Yaw explanation.** The +6.0472° yaw was not an alignment; the stored mesh is already mirror-symmetric. Corrected in [docs/AVATAR_GEOMETRY_EVIDENCE.md](docs/AVATAR_GEOMETRY_EVIDENCE.md), which also preserves the cross-section report, the mirror-matcher finding and the unmatched-ID bound.
- **"D2 findings" and "README merge preview".** **PARTIAL / UNVERIFIED**: not found under those exact names in the previously inspected repository/PR/Dropbox locations. Google Drive was not searchable in this handover because its connector is currently disabled by the ChatGPT workspace administrator; do not treat Drive as searched or conclude that the material is absent there. Recheck when access or a direct document link/export is available.

## Documents

| File | Content |
|---|---|
| [BUILD_AND_VERIFY.md](BUILD_AND_VERIFY.md) | Render routes, session rules, what each gate proves, the owner's evidence rules and readiness gate, local commands, the phone gate |
| [PROJECT_STATUS.md](PROJECT_STATUS.md) | Component baseline, all root causes, branch review, integrated sources, missing features, history |
| [docs/BRAVO_REVIEW.md](docs/BRAVO_REVIEW.md) | Review findings: fixed, open, and what was checked |
| [docs/AVATAR_GEOMETRY_EVIDENCE.md](docs/AVATAR_GEOMETRY_EVIDENCE.md) | Protected artifacts, corrected yaw, mirror symmetry, exporter validity, the preserved report |
| [docs/AVATAR_QUALITY_PLAN.md](docs/AVATAR_QUALITY_PLAN.md) | The avatar-quality plan and the reference-asset notes |
| [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) | Licences and provenance of the bundled fixture and libraries |

## Older debug build

[Previously published debug APK](https://github.com/davealone69-gif/3d-mega-hub/releases/download/android-build-46/3D-Mega-Hub-debug.apk). An older troubleshooting build. It is not the current candidate and not a release. GitHub still labels it "Latest" on the releases page; the current phone-test build is the pre-release linked from PR #12.
