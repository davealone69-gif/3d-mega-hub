# 3D Mega Hub

**Project status: integration and validation in progress. Not yet approved for merge to `main`.**

## Single source of truth

All implementation, fixes, tests and handover updates are to be made on one branch:

- **Working branch:** [`integration/avatarforge-renderer-port`](https://github.com/davealone69-gif/3d-mega-hub/tree/integration/avatarforge-renderer-port)
- **Only active review:** [PR #9](https://github.com/davealone69-gif/3d-mega-hub/pull/9)
- **Stable branch:** `main` (unchanged; do not merge until every merge gate below passes).

Do not create another implementation branch. Do not split work across competing branches. Do not force-push. Keep historical branches as recovery references until the canonical branch is verified and the owner explicitly authorises branch deletion.

## Latest handover: 2026-11-15

**Overall avatar-comparison status: MEASURED_WITH_GAPS. No verdict.** This update records the complete supplied aligned cross-section report. Artifact identities, geometry measurements and classifications below are agent-reported; they have not been independently recomputed in this repository workspace. This is not a build, renderer-runtime or phone-verification claim.

### Step 1 — preserved artifact identities

| Artifact | Reported path | Bytes | SHA-256 | Status |
|---|---|---:|---|---|
| Generated female LOD0 | `/workspace/3d-mega-hub-canonical/exports/avatar_default_female_LOD0.glb` | 83,430,312 | `56b072cb43203fe9c2ca0b231d28a3601b59914465647fa1da95a6890daccd6c` | Report says required size/hash match |
| Reference `mylove.glb` | `/home/ubuntu/upload/eYPzGGq49EUyWbNcDwqwN2/mylove.glb` | 9,287,272 | `8ef94a008b79a2ec8524962d810d92716f03f8476726dd8d8b7fc77227426a26` | Read-only identity recorded |

The reference copy's reported SHA-256 is distinct from the previously recorded Dropbox content hash. Do not conflate the two identifiers. The reference asset is private: do not commit, publish or redistribute it without explicit approval. The generated artifact is preserved and must not be modified.

### Step 2 — Y-axis alignment

PCA major-axis angles in XZ, modulo 180°:

| Measurement | Generated | Reference |
|---|---:|---:|
| PCA angle | −175.62054° (equivalent +4.37946°) | +178.33226° (equivalent −1.66774°) |
| Unique triangle-referenced world-space vertices used | 550,098 | 64,914 |

A relative **+6.04720° yaw about Y** was applied to generated geometry; reference orientation was fixed. Rotation convention:

`x′ = cos(θ)·x + sin(θ)·z`
`z′ = −sin(θ)·x + cos(θ)·z`

This is a whole-mesh, pose-sensitive PCA estimate, not independently validated anatomical registration. The raw-axis baseline is retained in Step 5; aligned measurements re-slice the generated geometry after rotation.

### Step 3 — common height definition

Both models use the same definition: full-scene world-space `Ymax − Ymin` after node transforms. Each slice is measured upward from that model's own `Ymin`. No scale or vertical registration was applied.

| Model | Y minimum | Y maximum | H |
|---|---:|---:|---:|
| Generated | 0.000000 m | 1.814858 m | 1.814858 m |
| Reference | −0.499926 m | 0.499917 m | 0.999842 m |

All reported widths and depths are dimensionless spans divided by that model's own `H`. Slice planes use `Y = Ymin + fraction × H`.

### Step 4 — torso-only analysis

<badge color="warning">INCOMPLETE</badge>

No defensible limb mask could be derived from mesh geometry alone. Contour-component counts at normalized heights were:

| Y/H | Generated components | Reference components |
|---:|---:|---:|
| 0.10 | 2 | 26 |
| 0.25 | 2 | 41 |
| 0.50 | 1 | 25 |
| 0.75 | 1 | 103 |
| 0.90 | 9 | 35 |

A largest-contour or central-contour rule would not reliably separate torso from limbs. No proxy mask was substituted. Therefore, these results are whole-mesh cross-sections, not torso-only measurements.

### Step 5 — complete whole-mesh cross-section measurements

Each pair below is `X-span/H / Z-span/H`. Ratios are **generated ÷ reference**. Reference values remain unchanged by the generated-model yaw.

| Y/H | Raw generated X/Z | Reference X/Z | Raw ratio X/Z | Aligned generated X/Z | Aligned ratio X/Z |
|---:|---:|---:|---:|---:|---:|
| 0.10 | 0.148075 / 0.058122 | 0.400646 / 0.234685 | 0.369591 / 0.247660 | 0.147574 / 0.067592 | 0.368340 / 0.288012 |
| 0.25 | 0.172452 / 0.086226 | 0.457947 / 0.197762 | 0.376576 / 0.436009 | 0.171962 / 0.095557 | 0.375506 / 0.483192 |
| 0.50 | 0.284834 / 0.176016 | 0.544902 / 0.143478 | 0.522725 / 1.226780 | 0.283525 / 0.181012 | 0.520323 / 1.261601 |
| 0.75 | 0.239902 / 0.140886 | 0.628299 / 0.209534 | 0.381828 / 0.672378 | 0.238919 / 0.143031 | 0.380263 / 0.682615 |
| 0.90 | 0.106442 / 0.139914 | 0.186629 / 0.154751 | 0.570340 / 0.904123 | 0.109387 / 0.143800 | 0.586120 / 0.929235 |

These are descriptive geometric spans. They do not establish named anatomy, anatomical similarity, or a pass/fail threshold.

### Step 6 — residual compatibility, not a verdict

The supplied report classifies the remaining differences as follows:

| Y/H | Reported compatibility | Limitation |
|---:|---|---|
| 0.10 | Compatible with height-registration residuals | Cause not established |
| 0.25 | Compatible with height-registration residuals | Cause not established |
| 0.50 | Unresolved between residual yaw and height registration | Cannot distinguish causes from these measurements |
| 0.75 | Compatible with height-registration residuals | Cause not established |
| 0.90 | Compatible with height registration; residual yaw not ruled out | Cause not established |

No cause is established and no pass/fail is assigned. In particular, the PCA yaw is not proof that the remaining differences are anatomical or that the models are correctly anatomically registered.

### Separate mirror-matching boundary

The mirror matcher remains a separate analysis. Previously reported maximum-cardinality matching did not match all IDs; this is not proof of anatomical asymmetry. Keep the provisional 4 mm criterion unchanged. It is not a threshold derived from this cross-section report and must not be loosened or treated as a substitute for anatomical validation.

### Evidence boundaries and next actions

- Preserve the generated LOD0 GLB exactly; do not alter or replace the recorded artifact.
- Keep `mylove.glb` private and read-only.
- Do not claim torso-only measurements unless a defensible, documented mask or independent anatomical landmarks become available.
- Do not turn compatibility labels into causal findings or an anatomical verdict.
- Keep mirror matching and its provisional 4 mm criterion separate from the cross-section analysis.
- No new Android build, emulator runtime test, or physical-device test is established by this report. Samsung SM-S921B verification remains **UNVERIFIED** unless the exact APK is installed and tested on the phone.

**Status: MEASURED_WITH_GAPS — no verdict.**

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

## Latest renderer investigation — 2026-11-15

- **Working branch:** `integration/avatarforge-renderer-port` · **PR:** [#9](https://github.com/davealone69-gif/3d-mega-hub/pull/9) · **stable `main`:** unchanged at `aaee4443dbf53ef4707cce01d99f71a59e903ec7`.
- PR #8 is **CLOSED / UNMERGED**, head `fix/mega-hub-candidate-hardening` at `41c3069301dc772bad3aa8dba1a2cbf67f8110c9`. It is not the active implementation path.
- **Failure-observability fix:** `78cb75d81557623622f6989a285ebf958dea2844` preserves the first renderer exception and any teardown exception instead of overwriting them with a generic stopped status.
- **GLES capability fix:** `ae43e0e271684d56336005450765a4577aafc591` replaces invalid legacy `glGetString(GL_EXTENSIONS)` calls with GLES 3.x `glGetStringi` enumeration. Float-colour-buffer and anisotropy capability flags now reflect actual extension support; anisotropy limit is queried once rather than once per texture.
- **Last completed exact-head CI evidence (`2a5485c17ec656659c4841826a1048d1577b7301)):**
  - **PASS:** signed preview APK and AAB build, signature verification and provenance (run `43626247432`, job `132542586035`). Ephemeral CI key only, not production signing.
  - **PASS:** Android emulator install/launch smoke (run `43626247432`, job `132542586459`).
  - **PASS:** native host regression (run `43626247376`, job `132542664347`).
  - Renderer pixel tests and Android CI had not completed when this evidence was recorded.
- **Latest source head:** `ae43e0e271684d56336005450765a4577aafc591`. Fresh exact-head CI is required after the GLES capability change. **UNVERIFIED:** visible model pixels and Samsung SM-S921B runtime. **BLOCKED:** production signing until the owner's keystore is configured in protected CI secrets.
- Do not merge to `main` or weaken the real visible-pixel gate.

## Execution plan

Work through these steps in order, on the single canonical branch. Do not skip a failed gate or mark a later step complete on the strength of an earlier one.

1. **Finish one consolidated exact-head validation cycle after the current coherent change batch.** Run native host regression, Android unit tests, debug APK assembly, lint, R8/minification, imported-GLB visible-pixel instrumentation, generated-avatar visible-pixel instrumentation, and emulator install/launch smoke together. Do not run a build after each small edit in the same batch. Inspect all failed jobs and artifacts before deciding whether another source change is justified.
2. **Use diagnostics before rebuilding.** For runtime-only questions, inspect the existing JSON, PNG, logs and device/asset identity first. Reuse a passing report when the APK, asset and relevant environment are unchanged. Only rebuild after a source/asset change or when existing evidence is insufficient. A diagnostic pass does not waive the one consolidated validation needed for a new code revision. No fake success values, mocked renderer, or weakened pixel threshold.
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
| Native host regression | **PASS (older exact source)** | Passed on application source `1deaf264032195989257845b884a5829f06fe9cf`; do not attribute to current head |
| Android unit tests | **PASS (older exact source)** | Passed on application source `1deaf264032195989257845b884a5829f06fe9cf`; do not attribute to current head |
| Debug APK build | **PASS (older exact source)** | Diagnostic workflow produced a 71,377,227-byte debug APK; historical artifact SHA-256: `3d833528d116fe0660a347d6a3bff0e9c6e7c8dc1d53b82125f15718917de8ee` |
| R8/minification | **PASS (older exact source)** | Passed on application source `1deaf264032195989257845b884a5829f06fe9cf`; not current-head evidence |
| Android lint | **CANCELLED (prior cycle)** | No pass claim |
| Basic generated-avatar pixel gate | **PASS (older exact source only)** | Run `43528943730`, source `e9e7d76ef4a3f12c5c6a79951b5a1a99570c6228`: basic generated-avatar pixel test passed on Android 11 emulator. This is not the production exported-GLB SurfaceView end-to-end test |
| Production generated-GLB-to-GLES SurfaceView | **FAILED on prior head** | Run `43534561147`, job `132260325291`, head `705165eebdaa525d75be861763c9a11205f33634`: `generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView` failed. LOD2 generation reduced the model to 325,491 triangles, but the production renderer presented only 2 frames in 15 seconds. This fails the required gate |
| Required Android build jobs | **FAILED on prior head** | Run `43534561148` at the same prior head: unit tests, lint, R8, debug APK build and native regression passed, but instrumented GLES renderer smoke failed; bundled-GLB instrumentation also hit `keyDispatchingTimedOut` |
| Fresh APK artifact | **PARTIAL on prior head** | Fresh debug APK artifact existed for the prior head and ZIP digest matched GitHub artifact digest. Extracted APK SHA-256 was calculated, but package and APK signing-block verification was not complete. It is not a ready APK |
| Current branch head after this README update | **NEW HEAD; VALIDATION PENDING** | This documentation commit intentionally changes the PR head. All previous run results are historical and cannot satisfy the exact-current-head readiness gate. Re-run required workflows on the new head |
| Samsung SM-S921B runtime | **UNVERIFIED** | Requires installation and testing on the physical phone |
| Production signing | **BLOCKED** | Requires owner-provided production keystore in protected CI secrets |
| Merge to `main` | **BLOCKED BY GATES** | Do not merge until every required gate passes |

## Strict PR #9 test APK readiness gate

A test APK is **READY** only when every condition below is independently proven for the same current PR head commit:

1. The production generated-GLB-to-GLES `SurfaceView` runtime instrumentation test passes on that exact head. A generated-avatar unit test, basic pixel test, imported-GLB test, emulator install-only smoke, or unrelated green check is not a substitute.
2. Every required Android build job passes on that exact head, including the required Android compilation, unit/lint/R8/native and renderer test jobs configured for this PR.
3. A fresh APK artifact is produced by a workflow run for that exact head. A stale artifact or artifact from a different SHA is rejected.
4. Download the artifact and verify the APK itself: package/application ID, signing certificate/signature, and APK SHA-256. Record the exact artifact URL, source commit, package, signing identity and digest.
5. Correlate workflow run head SHA, test result, artifact provenance and APK inspection. If any link is missing or ambiguous, status remains **PARTIAL**, **UNVERIFIED** or **BLOCKED**, never READY.

Do not merge PR #9 to `main`. Do not send a readiness notification until all five conditions are satisfied. On the first fully verified ready APK, notify the owner with the exact commit SHA, direct artifact download link, APK SHA-256 and production render-test evidence, then stop monitoring. Until then, keep checking the latest branch head and its associated Actions runs on the scheduled cadence; report failures internally/in the README and continue without a readiness notification.

**Latest monitor snapshot before this README-only commit (2026-11-14):** PR #9 was open and targeted `main`; branch head was `705165eebdaa525d75be861763c9a11205f33634`. The production generated-GLB-to-GLES SurfaceView test failed in run `43534561147`; required Android CI run `43534561148` failed its instrumented GLES renderer smoke. A fresh debug APK existed, but APK package/signature verification was incomplete. Therefore the prior head was **NOT READY**. This README update creates a new head, so the next scheduled check must start from the new head and must not reuse the prior head's evidence as a pass.

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
