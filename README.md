# 3D Mega Hub

**Project status: integration and validation in progress. Not yet approved for merge to `main`.**

## Single source of truth

All implementation, fixes, tests and handover updates are to be made on one branch:

- **Working branch:** [`integration/avatarforge-renderer-port`](https://github.com/davealone69-gif/3d-mega-hub/tree/integration/avatarforge-renderer-port)
- **Only active review:** [PR #9](https://github.com/davealone69-gif/3d-mega-hub/pull/9)
- **Stable branch:** `main` (unchanged; do not merge until every merge gate below passes).

Do not create another implementation branch. Do not split work across competing branches. Do not force-push. Keep historical branches as recovery references until the canonical branch is verified and the owner explicitly authorises branch deletion.

## Consolidation decisions

The live branch and pull-request audit selected `integration/avatarforge-renderer-port` as the canonical integration line.

- WebP decoding/source fallback, GLES renderer fixes and imported/generated GLB path are retained in the canonical line.
- The FileProvider capability and launcher icon are retained. The older FileProvider branch's replacement Compose-only activity and disconnected diagnostic overlay are **not** merged.
- The useful GLB fixture facts and user-confirmed display note from PR #11 are already in the canonical README. PR #11 was closed as superseded; its older README was not allowed to overwrite newer project information.
- The older Filament-generated-avatar path from PR #10 was not wholesale merged because it conflicts with the selected renderer split. Its useful deformation/bounds work is already represented in the canonical engine.
- Earlier shell, standalone cube, engine snapshots, Filament ancestors and generated Gradle cache changes are classified as superseded or historical. No generated cache files are to be imported.
- No historical branch is to be treated as an active work target. All further work goes onto the canonical branch.

See [PROJECT_STATUS.md](PROJECT_STATUS.md) for the detailed branch-by-branch audit and evidence ledger.

## Current architecture

- **AvatarForge generation:** isolated `:avatarforge-engine` Android library, JNI bridge and C++ generation kernel. Avatar Studio exposes prompt generation, cancellation, resource/thermal checks, native-result validation and atomic publication of validated GLBs.
- **3D viewer:** the bundled sample uses the separate Filament preview. Imported and generated self-contained GLBs use `AvatarForgeSurfaceView` and the integrated native AvatarForge renderer.
- **Graphics backend:** the imported/generated avatar path explicitly forces native GLES. Skinned GLBs apply the initial rest pose and refresh bounds before camera framing. Vulkan is not selected for this phone-first route.
- **Debug verification:** debuggable builds run visible-pixel verification for both imported GLBs and the deterministic seed-1337 generated-avatar diagnostic. The native GLES path waits for successful EGL swaps, captures the actual SurfaceView with PixelCopy, estimates the background from the four corners, counts foreground geometry pixels, applies the 5,000-pixel minimum and saves a PNG plus machine-readable JSON. Imported captures use `imported_glb_diagnostic.png`; generated captures use `generated_avatar_diagnostic.png`; failures use `failed_render.png`. Release/non-debug builds do not automatically capture every import. A scoped AndroidX FileProvider exposes only app-private external-files captures for sharing.
- **Avatar imports:** selected GLBs are copied into app-private storage, size-limited and structurally validated before use. External-resource `.gltf` files are rejected.
- **Local AI:** Ollama remains an external service, not an embedded LLM. Plain HTTP is restricted to loopback; remote endpoints require HTTPS.
- **Voice:** Android TextToSpeech output is connected. Viseme extraction and avatar lip-sync are not connected.

## Diagnostic-driven validation policy

**A passing diagnostic is reusable evidence for the exact APK, asset and device state it measured. It is not a reason to rebuild repeatedly.**

- If source code, build configuration, native libraries or packaged assets have not changed, do not rebuild just to repeat a passing diagnostic. Reuse the report and only rerun a diagnostic when the asset, device state, renderer lifecycle or suspected failure condition changes.
- After a coherent batch of source changes, perform one consolidated validation cycle against the exact resulting commit: native/unit tests, Android build/lint/R8, relevant renderer instrumentation, emulator install/launch and artifact capture. Do not build between individual edits in the same batch.
- A successful build proves compilation/package assembly only. A renderer diagnostic proves only the checks it actually performs. The diagnostic must identify its APK/source commit, asset hash, device/API/backend, capture method, thresholds and result before it can be reused as evidence.
- A previous APK's passing diagnostics do not validate changed source code. A newly built APK still needs one install/runtime validation on the target device before that new APK is called phone-verified.
- A generated-avatar diagnostic must not be used as a substitute for imported-GLB verification. Imported GLBs must independently pass actual surface capture and visible-pixel checks.
- FRAME_PRESENTED means EGL presented frames; it does not mean visible geometry was verified. PIXELS_VERIFIED requires a successful PixelCopy capture and at least 5,000 foreground geometry pixels after background subtraction. FAILED, capture timeout, missing screenshot or missing report must remain failures, not be silently converted into passes.
- In debug builds, imported GLBs now use the same PixelCopy visible-pixel gate as the generated diagnostic. This closes the previous evidence gap where ordinary imported-model reports could show visiblePixels: 0 simply because pixel verification had not been enabled. A zero count from a report without pixel verification enabled is NOT MEASURED, not proof of an invisible model.
- Preserve failure artifacts and diagnose them before another code change. Do not rerun an unchanged failing test repeatedly unless the next run changes one defined variable or captures missing evidence.

### Diagnostic evidence levels

| Result | What it proves | What it does not prove |
|---|---|---|
| STARTING | Renderer worker began | EGL or model loading |
| RENDERER_ACTIVE | Native renderer session and GLES context initialized | Geometry drawn or visible |
| FRAME_PRESENTED | Required EGL swaps, draw calls, triangles and frustum checks observed | Pixels reached the displayed surface |
| PIXELS_VERIFIED | PixelCopy captured foreground geometry above threshold | Model quality, correct appearance controls, animation or long-session stability |
| FAILED | A required diagnostic stage failed | Root cause, until failure details are inspected |

Diagnostic JSON should be treated as evidence, not a generic green light. A pass is scoped to the tested APK, asset and environment.
## Sample-model limitation

The bundled `female.glb` is a renderer fixture, not proof of a complete rigged avatar. Inspection of the exact bundled file shows one mesh, one node, seven morph targets, zero skins, zero animations and zero embedded images. It can exercise GLB loading/rendering, but does not prove skeletal animation, facial animation, texture quality or generated-avatar quality.

The fixture is byte-identical to the file at 3DDD revision `be5bf17d8afe46247646465941fa1b7317d8e415` (Git blob `a1c00c6b31f6ab0db3b2e68243526cb2d0e15967`). The donor README at that revision states the generated base rigs are original work released under CC0. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Execution plan

Work through these steps in order, on the single canonical branch. Do not skip a failed gate or mark a later step complete on the strength of an earlier one.

1. **Finish one consolidated exact-head validation cycle after the current coherent change batch.** Run native host regression, Android unit tests, debug APK assembly, lint, R8/minification, imported-GLB visible-pixel instrumentation, generated-avatar visible-pixel instrumentation, and emulator install/launch smoke together. Do not run a build after each small edit. Inspect all failed jobs and artifacts before deciding whether another source change is justified.
2. **Use diagnostics before rebuilding.** For runtime-only questions, inspect the existing JSON, PNG, logs and device/asset identity first. Reuse a passing report when the APK, asset and relevant environment are unchanged. Only rebuild after a source/configuration/asset change or when the existing evidence is insufficient to isolate the issue. A diagnostic pass does not waive the one consolidated validation needed for a new code revision. No fake success values, mocked renderer, or weakened pixel threshold.
3. **Prove emulator runtime.** Install the exact CI APK, launch the real app, open the relevant viewer route, load the fixture/imported GLB, and verify process stability, frame production, no ANR and a non-empty screenshot. A successful Gradle build alone is not an emulator pass.
4. **Prove the Samsung device.** Install that same candidate APK on the Samsung SM-S921B. Verify visible pixels, imported and generated GLB loading, app pause/resume, surface recreation, memory/resource handling and thermal behaviour. Record the APK SHA-256 and device evidence. Until this is done, physical-device rendering remains **UNVERIFIED**.
5. **Complete release signing.** Configure the owner's production keystore through protected CI secrets. Build and verify the signed release APK and AAB, check package/version/signature and record artifact SHA-256 values. A debug key or ephemeral CI preview key is not a production release.
6. **Review and merge once.** Update PR #9 with exact-head evidence. Merge to `main` only when all required CI and runtime gates pass and the review is clean. Keep `main` as the stable release line.
7. **Clean up only after proof.** Once the canonical branch is merged and the owner confirms the release, remove obsolete branch references and close superseded PRs. Until then, keep historical branches as recovery points, not active work streams.

## Current verification ledger

| Item | Status | Evidence / next action |
|---|---|---|
| Canonical branch selected | **REAL + VERIFIED** | One working branch and PR #9 selected; `main` untouched |
| Branch/PR audit | **REAL + VERIFIED** | Detailed dispositions recorded in [PROJECT_STATUS.md](PROJECT_STATUS.md) |
| Native host regression | **PASS** | Native host regression and Android native sampling passed on application source `1deaf264032195989257845b884a5829f06fe9cf` |
| Android unit tests | **PASS** | Unit tests passed on application source `1deaf264032195989257845b884a5829f06fe9cf` |
| Debug APK build | **PASS** | Diagnostic workflow produced a 71,377,227-byte debug APK; artifact SHA-256: `3d833528d116fe0660a347d6a3bff0e9c6e7c8dc1d53b82125f15718917de8ee` |
| R8/minification | **PASS** | Release minification job passed on application source `1deaf264032195989257845b884a5829f06fe9cf` |
| Android lint | **CANCELLED** | The CI job was cancelled before completion; no pass claim |
| Basic generated-avatar pixel gate | **PASS on emulator** | Run `43528943730`, source `e9e7d76ef4a3f12c5c6a79951b5a1a99570c6228`: `generatedAvatarMustProduceVisiblePixels` passed (1 test, 0 failures) on Android 11 emulator. APK: 71,377,227 bytes; SHA-256 `9c49b7514bbda66bdbe7e9a648e455d4793f601f184f4b86913a7631ca3ed43b`. This is not the production exported-GLB SurfaceView end-to-end test |
| Production generated-GLB-to-GLES SurfaceView | **PARTIAL / CANCELLED** | The end-to-end test generated and exported the GLB, began the production SurfaceView render test, but the workflow was cancelled before test completion and before evidence PNG/JSON could be collected |
| Imported-GLB GLES instrumentation | **CANCELLED** | The instrumented render job did not complete; no pass claim |
| Emulator install/launch smoke | **CANCELLED** | Release/emulator workflow was cancelled before its final result |
| CI concurrency correction | **COMMITTED; NEW PR RUNS PENDING** | Removed duplicate push triggers from the generated-runtime and diagnostic workflows and made PR concurrency groups unique, preventing the same push+PR pair from cancelling one another |
| Samsung SM-S921B runtime | **UNVERIFIED** | Requires installation and testing on the physical phone |
| Production signing | **BLOCKED** | Requires owner-provided production keystore in protected CI secrets |
| Merge to `main` | **BLOCKED BY GATES** | Do not merge until steps 1-6 are satisfied |

**Evidence snapshot (2026-11-14):** source `1deaf264032195989257845b884a5829f06fe9cf` retains the earlier passing native host, Android native sampling, unit, debug build and R8 evidence. A fresh exact-branch run `43528943730` at source `e9e7d76ef4a3f12c5c6a79951b5a1a99570c6228` passed native host regression and built a 71,377,227-byte debug APK; its basic generated-avatar emulator pixel test passed (1 test, 0 failures) on Android 11. APK SHA-256: `9c49b7514bbda66bdbe7e9a648e455d4793f601f184f4b86913a7631ca3ed43b`. This is a basic generated-avatar pixel gate, not proof of exported-GLB production SurfaceView rendering or visual quality. Separate production generated-avatar instrumentation run `43528943695` is still in progress at the time of this update. Android lint/imported-GLB render smoke and release/emulator smoke remain cancelled in the earlier cycle. Emulator results are not Samsung phone verification.

CI status can change after this README is updated. Check the latest [GitHub Actions runs](https://github.com/davealone69-gif/3d-mega-hub/actions) and the [canonical PR](https://github.com/davealone69-gif/3d-mega-hub/pull/9) before making a release decision.

## Known boundaries and missing features

- The AvatarForge donor pin is `9e36197ace3b18faa6c566d93f8fce89b185c45d` from `work/android-engine-dev`. It is divergent, not a fast-forward ancestor, of the standalone AvatarForge RB-01 line (`0e782166…` / `fc30efd0…`). Several generation-core files differ in both directions. No wholesale replacement was attempted; any core-only port needs a file-by-file API review and native tests. AvatarForge remains a separate project.
- Prompt generation is deterministic native parsing, not photo-to-identity reconstruction and not an embedded LLM.
- External-resource `.gltf` is unsupported; the current viewer path is self-contained GLB.
- Voice lip-sync/visemes are missing.
- The bundled fixture is not a rigged/animated model.
- Samsung SM-S921B visible rendering, real-device generation, import lifecycle, memory and thermal behaviour remain **UNVERIFIED** until the phone test is completed.

## Older debug build

[Download the previously published debug APK](https://github.com/davealone69-gif/3d-mega-hub/releases/download/android-build-46/3D-Mega-Hub-debug.apk). This is an older troubleshooting build, not the current integration candidate and not a production release.

## Current priority: avatar quality first

**Priority correction (2026-11-13):** the owner has now physically tested the latest APK and reports that the renderer displays avatars, the generation button changes avatar features (including male/female), and GLB import works. Skin-tone and hair changes have not yet been tested. The owner also reports that the avatar quality is currently too poor to judge appearance changes reliably.

These are **OWNER-TESTED** results for the APK the owner installed. The exact APK SHA-256 and source commit used for that phone test have not been supplied, so do not attribute the result to a specific repository head or call the current PR head device-verified.

The next implementation priority is **avatar visual quality**, not another renderer rebuild. Preserve the working native GLES renderer, prompt generation path and GLB import. Change them only when a reproducible defect is demonstrated.

### User reference asset: `mylove.glb`

The owner has already supplied `mylove.glb` as the reference model for improving the generated avatar. Do not ask for another upload while this source remains available.

- **Dropbox source:** `/mylove.glb`
- **Known file size:** 9,287,272 bytes (8.86 MiB)
- **Dropbox content hash:** `388a94e7495e809577356ba6cf9890a13fe38f8cd4c63ba3228109d19a24b398`
- **Second known copy:** `/3d-mega-hub-diagnostics/3d-mega-hub/app/src/main/assets/mylove.glb` (same reported size)
- **Reference role:** ground truth for measurable proportions, silhouette, anatomy, mesh density, material/texture characteristics, rig and morph structure where those features exist.
- **Inspection status: PARTIAL, with owner-supplied structural and measurement results.** The owner reports running the GLB inspection script on the exact 9,287,272-byte file. The measurements below are owner-supplied and have not yet been independently reproduced from the binary in this workspace.
- **GLB structure (owner-reported):** GLB v2; one static mesh; 163,756 vertices; 144,073 triangles; UVs and normals present; no skeleton, skin or animation; two embedded 2048x2048 WebP images; no normal map.
- **Bounds (owner-reported, model units):** X [-0.3994, 0.4000], Y [-0.4999, 0.4999], Z [-0.1793, 0.1786], dimensions approximately 0.799 x 1.000 x 0.358. Y height is 1 model unit; this does not establish real-world scale or semantic anatomy landmarks.
- **Vertex counts across 10 equal bins (owner-reported):** X: [313, 1817, 10624, 29610, 47290, 43211, 19246, 9929, 1369, 347]; Y: [18678, 9275, 11631, 10829, 8337, 7905, 10077, 41734, 36938, 8352]; Z: [1988, 1976, 2228, 3142, 16543, 52962, 34696, 18166, 24979, 7076]. These are vertex-distribution counts, not named anatomical regions. Do not label bins as shoulders, hips or limbs without slice geometry/landmark evidence.
- **Materials/textures (owner-reported):** a dark red/tan base-colour WebP and a mottled teal/green metallic-roughness WebP; material `metallicFactor` is 1.0. In glTF metallic-roughness, the texture's green channel modulates roughness and blue channel modulates metallic, each multiplied by its corresponding scalar factor. The effective metallic value therefore cannot be inferred from the scalar alone; decoded blue-channel statistics are still needed. No normal map is present.
- **Generator comparison from source inspection:** AvatarForge's procedural skin material is already authored with metallic 0.00 and roughness 0.52. Do not change this on the basis of the reference scalar alone. The reference material's effective metalness and visual response must be checked using its decoded blue channel and a controlled render.
- **Transfer limitation:** the available Dropbox text-fetch action has a 5 MiB maximum and cannot read this 9.29 MB binary. Dropbox's download-link action exposes a temporary URL but does not return file bytes to the analysis workspace; the direct transfer attempt was rejected by the runtime URL-access gate. The linked-chat attachment itself was not surfaced as a readable file in the current conversation/library search.
- **Next measurement required:** obtain horizontal cross-section extents (X width and Z depth at normalized Y heights), preferably for 10 equal Y slices, plus decoded image channel statistics and material-to-texture slot mapping. Compare those profiles against a generated avatar captured with the same coordinate convention. The current repository probe reports five descriptive slices and named landmarks only when node names exist; it does not identify anatomy from unnamed vertices.
- **Use rule:** derive a versioned reference profile and regression targets from measured data. Do not blindly replace the procedural generator with the reference mesh, and do not commit or redistribute the private reference asset without explicit approval.


### Stage 1: establish and improve avatar quality

1. Capture reproducible before-images and exported GLBs for controlled male and female prompts. Record the source commit, prompt, seed, model dimensions, triangle/vertex counts, material assignments, texture dimensions and GLB SHA-256.
2. Trace the full generation path from parsed prompt -> `af_spec` -> anthropometry/body field -> mesh extraction/refinement -> hair/face/clothing regions -> material/texture bake -> GLB export. Do not assume a parser or renderer issue from appearance alone.
3. Diagnose the visible quality defects by category: body silhouette/proportions, head and facial anatomy, eyes/mouth/ears, hands/feet, hair geometry, clothing construction, topology/normals, UVs/textures, material response and lighting. Separate geometry defects from material/rendering defects.
4. Prioritise the highest-impact geometry and silhouette defects first. A shader/lighting polish must not be used to disguise weak geometry. Avoid wholesale replacement of the procedural generator.
5. Add deterministic native regression tests for male/female baselines, spec values, mesh validity, region/material coverage, bounds, finite normals/UVs, and export/import round-trip. Tests must assert actual generated data, not just non-empty output.
6. Make small, measurable changes and preserve before/after GLB plus rendered captures. Compare under identical camera, lighting, renderer, seed and output resolution.
7. Rebuild and run CI on the exact branch head. The owner must install and test the resulting APK before the improved quality is called phone-verified.

### Stage 2: verify appearance controls

Only once the base model is good enough to judge, verify each control independently:
- skin tone: light, medium and dark prompts, checking parsed melanin/spec values, exported material/texture values and visible output;
- hair: style, length and colour, checking both parser output and actual exported geometry/materials;
- clothing colour/style and other appearance attributes;
- male/female differences under matched stature/build prompts.

Record unsupported prompt tokens honestly. Deterministic keyword parsing is not an embedded LLM and must not be presented as general natural-language understanding.

### Stage 3: end-to-end asset workflow

After quality and appearance controls are stable, verify Generate -> Save/Export -> close/reopen -> import the exact generated GLB -> render. Preserve the existing working import path and test malformed/oversized input separately.

### Owner-tested evidence ledger

| Item | Status | Evidence / limitation |
|---|---|---|
| Avatar visible in renderer on owner's phone | **OWNER-TESTED** | Owner reports latest installed APK displays the avatar; exact APK SHA-256/source commit not supplied |
| Generation button changes avatar features | **OWNER-TESTED** | Owner reports prompt-driven feature changes, including male/female |
| GLB import | **OWNER-TESTED** | Owner reports import button works; complete file/lifecycle matrix remains unverified |
| Skin-tone changes | **NOT YET TESTED** | Do not claim verified |
| Hair style/colour changes | **NOT YET TESTED** | Do not claim verified |
| Avatar visual quality | **NEEDS IMPROVEMENT** | Owner reports quality too poor to judge appearance changes reliably |
| Exact current PR-head phone verification | **UNVERIFIED** | Need identify tested APK commit and SHA-256, then test exact candidate |
| Native/CI regression and release gates | **SEPARATE GATES** | Continue exact-head build/test evidence; production signing remains dependent on owner keystore |

Do not mark avatar quality improved without matched before/after GLB and rendered-image comparisons. Do not mark a new APK phone-verified until the owner tests that exact artifact.
