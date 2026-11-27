# 3D Mega Hub

**Project status: integration and validation in progress. Not yet approved for merge to `main`.**

## Avatar quality implementation handover — procedural hair

**Work branch:** `integration/avatarforge-renderer-port`  
**Implementation commits:**
- `3960f4d32caec799435edf6beb6a03ff29b8916b` — replace the two smooth braid ropes with deterministic three-strand interlocking tapered geometry.
- `09b87e33f7e459eba212c4144db254c7b62c473d` — replace the flat hairline cut with a curved temple-recession / widow-peak profile; add low-amplitude coil-scale surface variation to the Afro field.
- `8370967eb9248ad298e693a3357c00d1dfa0b91e` — add native regression coverage sampling all nine selectable procedural hair styles for finite, non-empty geometry.
- `f6480cefd96ab094fc2c792111bdb617c8924463` — add four shallow clump channels to medium/long rear hair, sized for the 8 mm extraction lattice.
- `f6882b16633331e30f411fb1bc2946a8008d8407` — bake a root-to-tip pigment gradient separately from the hair material factor; preserve custom `hair_tone` in GLB export; add native tests for default/custom colour and gradient direction.

**Status**
- Braid silhouette geometry, curved hairline, Afro coil-scale field, long-hair clump channels, and root-to-tip pigment gradient: **IMPLEMENTED IN SOURCE; FINAL-HEAD CI PENDING**.
- Nine-style finite/non-empty field regression and hair tone / gradient helper regression: **IMPLEMENTED IN SOURCE; FINAL-HEAD CI PENDING**.
- Existing GLES Kajiya–Kay hair specular path: **PRESENT IN SOURCE**; its strand direction remains a world-up approximation rather than groom-authored per-strand vectors.
- Shared scalp-cap socket asset, fitted hairline baby-hair cards, strand-card shells, flyaways, shared strand atlas/mask channels, alpha-to-coverage/depth-prepass profiling, and dedicated three-level hair LODs: **NOT IMPLEMENTED**.
- Visual validation, triangle/card budgets, overdraw/GPU timings, APK build and S24 runtime: **UNVERIFIED**.

The implementation remains in the existing AvatarForge signed-distance-field, texture-bake and GLB export pipeline. It does not replace existing presets or add mock assets. The braid, coil and clump geometry is deterministic and field-evaluated. The native regression samples all nine selectable hair IDs for finite, non-empty geometry; it is not a visual-quality test. The colour regression verifies natural/custom material factors and root-to-tip gradient direction; it does not substitute for visual inspection.

Every README update changes the branch head. Collect all final verification against the resulting exact head, and do not reuse older workflow results as exact-head evidence.


## Single source of truth

All implementation, fixes, tests and handover updates are to be made on one branch:

- **Working branch:** [`integration/avatarforge-renderer-port`](https://github.com/davealone69-gif/3d-mega-hub/tree/integration/avatarforge-renderer-port)
- **Only active review:** [PR #9](https://github.com/davealone69-gif/3d-mega-hub/pull/9)
- **Stable branch:** `main` (unchanged; do not merge until every merge gate below passes).

Do not create another implementation branch. Do not split work across competing branches. Do not force-push. Keep historical branches as recovery references until the canonical branch is verified and the owner explicitly authorises branch deletion.

## Latest implementation update: 2026-11-15

**Animation UI change committed on the canonical branch.** Imported file-based GLBs now open directly in the existing Filament gltfio preview, which already drives the GLB `Animator` each render frame. The user no longer has to discover and manually enable the alternate preview before animation controls can appear. The AvatarForge GLES preview remains available as an explicit alternate mode; its native importer still does not apply imported skeletal animation.

- Animation preview default: **IMPLEMENTED IN SOURCE** — commit `8b80b072187c1e471107fd9bb23ff8bdae9c4790`.
- Preview-mode explanation and control wording: **IMPLEMENTED IN SOURCE** — commit `0584611d5ffcc8d7b89416d98f14299790c3450f`.
- README and rig-reference handover: **UPDATED** — commit `3c7e356365ff55db3b915365dfcc284e7cc55aaf`.
- PR #9 observed head at the latest monitor check: `d66eb22af0c82de12aeafccd70b283d44ddd5c52` (the subsequent README evidence commit will create a new head).
- Exact-head CI on `d66eb22af0c82de12aeafccd70b283d44ddd5c52`: **FAILED** — production generated-GLB-to-GLES SurfaceView test failed in runs `43673389818` and `43673389819`; Android CI run `43673389854` also failed its instrumented GLES GLB render smoke. See the latest APK monitor snapshot below.
- `mylove_rig_prepared.glb` binary inspection and clip playback: **UNVERIFIED** — the preserved Dropbox file is recorded below, but its bytes were not available in the build workspace. Do not infer clip count or playback success from its filename.

## Latest APK monitor snapshot: 2026-11-15

**Observed PR #9 state:** open, targeting `main`; branch `integration/avatarforge-renderer-port`; observed head before this README evidence commit: `d66eb22af0c82de12aeafccd70b283d44ddd5c52`. No merge was performed. `main` remains untouched.

### Exact-head Actions evidence for `d66eb22af0c82de12aeafccd70b283d44ddd5c52`

| Workflow / run | Result | Evidence |
|---|---|---|
| [Android CI #603](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43673389854) | **FAILED** | Native host regression, R8/minification, unit tests, lint and debug APK build passed. Instrumented imported-GLB GLES renderer smoke failed with `OutOfMemoryError`: attempted 134,217,744-byte allocation with about 25.1 MB free; instrumentation process crashed |
| [AvatarForge diagnostic APK/tests #276](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43673389818) | **FAILED** | Build and basic generated-avatar PixelCopy diagnostic passed. Required `generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView` failed: only 2 presented frames in 15 seconds; 29 draws; 1,626,176 reported triangles; bounds in frustum; GL/EGL errors zero |
| [AvatarForge generated-avatar runtime verification #347](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43673389819) | **FAILED** | The production generated-GLB-to-GLES SurfaceView test failed with the same 2-frame / 15-second assertion |
| [Release preview and Android emulator smoke #416](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43673389800) | **PASS, not a readiness substitute** | Emulator install/launch smoke and ephemeral-key signed release preview passed. Neither proves production generated-GLB rendering |

### Artifacts and verification boundary

- Exact-head debug APK artifact: [3D-Mega-Hub-debug-apk, artifact 14145098709](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43673389854/artifacts/14145098709). Artifact ZIP SHA-256: `49fa25b29fc8e7e8ef68fa8d6c5a0ee0c962799527baf9943ea81a57ac9d793d`.
- Exact-head diagnostic APK artifact: [3D-Mega-Hub-generated-avatar-diagnostic-debug-apk, artifact 14145536569](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43673389818/artifacts/14145536569). Artifact ZIP SHA-256: `e9a9e1bf4b95ed411eca402d6a9099fba8e83a9d6f4ae5784d02ca58e609f3f6`.
- These digests identify the downloaded workflow artifact ZIPs, **not** the APK files inside them.
- APK package/application ID, APK signing certificate/signature and APK-file SHA-256: **UNVERIFIED**.
- Samsung SM-S921B installation and runtime: **UNVERIFIED**.

**Readiness decision: NOT READY.** The required production render instrumentation fails, the imported-GLB GLES smoke fails, and APK-level package/signature/hash verification is incomplete. Do not distribute these artifacts as a ready test APK. The README update itself creates a new PR head; all run results above belong only to the observed parent commit and must not be reused as exact-head evidence for the new commit.

## Latest APK monitor snapshot: 2026-11-16

**Observed PR #9:** open, targeting `main`; canonical branch `integration/avatarforge-renderer-port`; exact observed head before this README update: `f73eec156b1baf6d105b4e891d8db45f22e66f3f`. PR #9 remains unmerged. No source files were changed by this monitoring run.

### Exact-head workflow runs

| Workflow / run | Result | Job-level evidence |
|---|---|---|
| [Android CI #638](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43706109624) | **FAILED** | Unit tests, debug APK build, Android lint and R8/minification passed. Native C++ sampling regression failed to compile in job [132787460667](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43706109624/job/132787460667): `native_sampling_test.cpp:36` refers to `REGION_HAIR` without its namespace; compiler identifies `af::REGION_HAIR`. Instrumented smoke job [132787460620](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43706109624/job/132787460620) failed. |
| [Release preview and Android emulator smoke #447](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43706109650) | **SUCCESS, not a renderer readiness pass** | Signed release preview and emulator install/launch smoke passed. The release workflow creates a one-run ephemeral CI key; this is not the owner's reusable production signing identity. The emulator job is install/launch smoke only. |

### Required production generated-GLB-to-GLES SurfaceView gate

**NOT RUN on this head.** The source contains the intended production test, `GeneratedAvatarRuntimeTest.generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView()`, in `app/src/androidTest/java/com/davemega/hub/avatarforge/GeneratedAvatarRuntimeTest.kt`. It generates and validates a GLB, attaches the production `AvatarForgeSurfaceView` to the app Activity, then requires visible-pixel, draw-call, multi-frame and screenshot evidence.

However, the executed Android CI script explicitly filters instrumentation to `com.davemega.hub.AvatarForgeRendererSmokeTest`; it does not select `GeneratedAvatarRuntimeTest`. The executed smoke suite therefore does not prove the required production generated-GLB-to-GLES SurfaceView path.

The executed smoke job itself failed before completing its three-test suite. Its generated-avatar debug diagnostic crashed with `OutOfMemoryError` while allocating 134,217,744 bytes; the crash stack identifies `FilamentPreviewView.readBounded(FilamentPreviewView.kt:446)`. This is the debug diagnostic's Filament file-preview route, not a passing production AvatarForge SurfaceView test. The uploaded renderer evidence artifact does not convert this failed run into a pass.

### APK artifacts and inspection boundary

Artifacts were produced for this exact SHA:
- [Debug APK artifact 14158508686](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43706109624/artifacts/14158508686), from Android CI #638.
- [Signed preview artifact 14158503818](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43706109650/artifacts/14158503818), from release preview #447; signed with a one-run ephemeral key.

APK package ID, APK signing certificate identity/signature and APK-file SHA-256 were **NOT INSPECTED** in this run because the mandatory production render test did not pass. The artifact ZIPs and successful install/launch smoke are not sufficient evidence.

**Readiness decision: NOT READY.** The required production test was not executed; the executed generated-avatar diagnostic crashed; the native C++ regression did not compile; and APK-level identity/signature/hash inspection remains outstanding. Do not distribute either artifact as the requested ready test APK.

**Next-run requirement:** first re-read PR #9 and the exact branch head. This README update creates a new commit and changes the branch head, so none of the `f73eec156b1baf6d105b4e891d8db45f22e66f3f` workflow results may be reused as exact-head evidence for the resulting commit. Require a fresh run that actually invokes `GeneratedAvatarRuntimeTest.generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView`, then inspect the same-head APK artifact itself.

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
- **3D viewer:** the bundled sample uses the separate Filament preview. Imported file-based GLBs now open in the existing Filament gltfio preview by default; users can explicitly switch to the native AvatarForge GLES preview for its static imported-model path. Generated avatars continue to use the native AvatarForge renderer.
- **Animated GLB playback (implementation committed; runtime verification pending):** the existing Filament `Animator` exposes clip selection, play/pause/stop-reset, looping, playback speed and timeline seeking; the first clip starts automatically. The controls appear in Avatar Studio when the selected GLB contains animation clips. This reuses Filament gltfio rather than adding a new animation engine. Latest implementation commits: `8b80b072187c1e471107fd9bb23ff8bdae9c4790` (default animation-capable preview for imported file GLBs) and `0584611d5ffcc8d7b89416d98f14299790c3450f` (preview-mode wording).
- **Rigged animation reference asset:** private Dropbox asset `/3d-mega-hub-diagnostics/3d-mega-hub/mylove_rig_prepared.glb`, 13,221,520 bytes, SHA-256 `656cf2446eefbffc92cf569ce1a2f52b6d0ad449c282fc5a4d911abfef576d14`. It remains read-only and is not committed to Git. Binary clip/joint counts have not been independently inspected in this workspace; playback with this exact asset is **UNVERIFIED**.
- **Native AvatarForge animation limitation:** the native GLB importer still parses but discards imported skin/joint/animation data when it resets its generated rig and clip caches. Therefore the new preview is a separate working-path candidate; it does not mean the native AvatarForge GLES importer now skins or animates imported GLBs. Exact-device and clip playback verification are still pending.
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
| Production generated-GLB-to-GLES SurfaceView | **FAILED on observed head** | Runs `43673389818` (job `132688476029`) and `43673389819` (job `132688165314`), head `d66eb22af0c82de12aeafccd70b283d44ddd5c52`: `generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView` failed. Assertion: no drawable model frame within 15 seconds; `frames=2`, `presented=2`, `draws=29`, `triangles=1626176`, bounds in frustum, `glError=0x0`, `eglError=0x0`. Required gate fails |
| Required Android build jobs | **PARTIAL / FAILED on observed head** | Android CI run `43673389854`: native host regression, R8, unit tests, lint and debug APK build passed; instrumented imported-GLB GLES render smoke failed with `OutOfMemoryError` allocating 134,217,744 bytes with about 25 MB free. The separate production generated-GLB test also failed as above |
| Fresh APK artifact | **PARTIAL on observed head** | Fresh debug APK artifacts exist for the exact observed head: run `43673389854`, artifact `14145098709` (`3D-Mega-Hub-debug-apk`), and run `43673389818`, artifact `14145536569` (`3D-Mega-Hub-generated-avatar-diagnostic-debug-apk`). GitHub ZIP digests are recorded in the latest monitor snapshot; APK package, signing certificate and APK SHA-256 have not been verified. Not a ready APK |
| Current branch head after this README update | **NEW HEAD; VALIDATION PENDING** | The monitor snapshot below applies to parent head `d66eb22af0c82de12aeafccd70b283d44ddd5c52`. This README commit changes the PR head; its own exact-head Actions evidence must be checked separately |
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

### Rigged animation reference asset

- **Preserved Dropbox file:** `/3d-mega-hub-diagnostics/3d-mega-hub/mylove_rig_prepared.glb`
- **Size:** 13,221,520 bytes
- **SHA-256:** `656cf2446eefbffc92cf569ce1a2f52b6d0ad449c282fc5a4d911abfef576d14`
- **Status:** **REAL + VERIFIED** Dropbox upload completed and file ID returned. Binary contents have not yet been freshly inspected in this workspace; skeleton/clip counts and playback compatibility remain **UNVERIFIED**.
- Keep this file read-only as the rigged animation test reference. Do not replace it with the older static `mylove.glb`.

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


## Avatar assets, appearance presets and animation handover (2026-11-15)

### Where the files are

- The only checked-in runtime model under `app/src/main/assets/models/` is `female.glb` (418,312 bytes). It is a **static renderer fixture**, not a wardrobe or animation pack. The existing fixture inspection records one mesh, seven morph targets, zero skins, zero animations and zero embedded images.
- There is **no separate pre-made skin/hair/clothing asset-pack directory** in this branch's repository tree. Hair, skin appearance and clothing are currently produced by AvatarForge's procedural C++ generation/material pipeline, rather than loaded from a library of interchangeable finished files.
- The source-level hair presets are in `avatarforge-engine/src/main/java/com/davemega/hub/avatarforge/AvatarSpec.kt`: `NONE`, `BUZZ`, `SHORT`, `MEDIUM`, `LONG`, `PONYTAIL`, `BUN`, `BRAIDS`, `AFRO`, `MOHAWK`.
- The source-level outfit presets in the same file are `NONE`, `TSHIRT`, `DRESS`, `SUIT`, `JEANS_JACKET`, `ARMOR_PLATE`, `ARMOR_LEATHER`, `SPACESUIT`, `ATHLETIC`, `COAT`. Skin melanin/tone, hair tone, cloth tone and material roughness are also represented in `AvatarSpec`.
- These are **engine options/spec fields**, not proof that the app currently presents a complete clickable wardrobe, thumbnail catalogue or swatch picker. The current Avatar Studio UI exposes prompt-driven generation; independent skin-tone and hair-style/colour changes remain **NOT YET TESTED**.

### Animation status, separated by path

- The native engine source contains a built-in authored clip library in `avatarforge-engine/src/main/cpp/src/af_rig.cpp`: `idle`, `breathe`, `walk`, `jog`, `run`, `squat`, `jump`, `wave`, `punch`, `kick`, `pushup`, `t_pose`, `a_pose`, `sit`, and `turn_360` (plus the identity `rest` pose). The rig source also defines 52 facial blendshape channels.
- Imported file-based GLBs default to the Filament gltfio preview. Source code now exposes animation-clip selection, play/pause, stop/reset, loop, 0.5x–2.0x speed and timeline seeking when the selected GLB reports clips. This is **IMPLEMENTED IN SOURCE; runtime playback on the preserved rigged reference asset remains UNVERIFIED**.
- The native AvatarForge GLES importer is still a **static imported-GLB path**: it does not yet apply imported skeletal animation/skin data. Do not switch to that preview expecting imported clips to play.
- Preserved animation test asset: private Dropbox `/3d-mega-hub-diagnostics/3d-mega-hub/mylove_rig_prepared.glb`, 13,221,520 bytes, SHA-256 `656cf2446eefbffc92cf569ce1a2f52b6d0ad449c282fc5a4d911abfef576d14`. Keep read-only and private. Its binary clip/joint inventory and playback have not been freshly verified in this workspace.

**Evidence boundary:** source declarations prove that presets and clip definitions exist in code. They do not prove that the current APK exposes all options, that generated GLBs contain every native clip, or that animation visibly plays on the target Samsung phone. Those remain separate runtime checks.

## Exact-head Android test APK readiness monitor (2026-11-15)

**Decision for inspected source SHA `7b19ca760ac529f2f157ae1bc516a3d0333029b6`: NOT READY.** PR #9 and the branch ref both pointed to this exact commit when checked. Six Actions runs were associated with this SHA (four completed PR runs and two push-triggered runs cancelled by concurrency).

### Required production generated-GLB → GLES SurfaceView gate

- **Failed:** [diagnostic workflow run 43680996825](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43680996825), job **132711632839** — “Build debug APK and verify generated avatar pixels”.
- Required step: “Run generated GLB through production GLES SurfaceView”.
- Instrumentation: `GeneratedAvatarRuntimeTest#generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView`.
- The JUnit report records 1 test, 1 failure. Assertion at `GeneratedAvatarRuntimeTest.kt:217`: no drawable EGL-presented model frame within 15 seconds. Captured counters: `frames=2`, `presented=2`, `draws=29`, `triangles=1626176`, `boundsInFrustum=true`, `outsideCorners=0`, `glError=0x0`, `eglError=0x0`.
- The separate [generated-avatar runtime run 43680996884](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43680996884), job **132711372691**, also failed the same production SurfaceView test with the same visible-frame assertion. Its uploaded test report confirms the failure.

### Other exact-SHA workflow results

- [Android CI run 43680996926](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43680996926): Unit tests, Debug APK build, Android lint, R8/minification and Native C++ sampling regression **PASSED**. Its separate imported-GLB renderer job **132711374921 FAILED**: `AvatarForgeRendererSmokeTest#generatedAvatarDebugDiagnosticVerifiesImportedGlesPixels` hit `OutOfMemoryError` while allocating 134,217,744 bytes, with 25,149,440 bytes free and a 201,326,592-byte process growth limit. This imported-GLB smoke is not a substitute for the required generated-GLB production SurfaceView test.
- [Release preview run 43680996849](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43680996849): signed preview build and emulator install/launch smoke **PASSED**. Install/launch is not production renderer evidence. The preview uses a one-run ephemeral CI signing key, not a retained production signing identity.
- Push-triggered runs **43680994601** (Android CI) and **43680994591** (release preview) were cancelled; they provide no passing evidence.

### APK artifact inspection (same source SHA, not a readiness pass)

- [Diagnostic debug APK artifact 14149070299](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43680996825/artifacts/14149070299) was produced by run 43680996825 for `7b19ca760ac529f2f157ae1bc516a3d0333029b6`. Downloaded and inspected the APK itself.
  - File: `3D-Mega-Hub-generated-avatar-diagnostic-debug.apk`
  - Package ID: `com.davemega.hub`
  - APK SHA-256: `79cd893cc68674f50ac5c05b57f54cd493a75c669aa84f807b51cb112747b125`
  - APK v2 signature and APK content digest both verified.
  - Signer: `C=US,O=Android,CN=Android Debug`
  - Signing certificate SHA-256: `bc4676bb9d180d7b5448f0728fd75624f4335585466d11313271cc6b2969aab3`
  - This is a fresh, debug-signed APK, but its required production render test failed. It is therefore **NOT READY**.
- [Signed preview artifact 14147893845](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43680996849/artifacts/14147893845) also belongs to this SHA. Its APK package is `com.davemega.hub`, SHA-256 `f6c92cc763c2181709e845340f552bad54bfd6e3ba1457e57e746ebdbdb0ca29`; the signer is `CN=3D Mega Hub CI Preview,O=Ephemeral Build,C=AU`, certificate SHA-256 `a133a285a42a930705a8ed7d13d55a3bb07ecd3f223f0cca71cc684d30968d35`. This one-run key is not a production signing identity and the artifact does not override the failed render gate.

### Next verification rule

Do not distribute or label either artifact as the first READY APK. The production generated-GLB-to-GLES SurfaceView test must pass on one exact current head, all required Android build jobs must pass, and the APK artifact from that same SHA must again be downloaded and inspected. The imported-GLB OOM is a separate failure worth fixing, but it cannot replace the production generated-GLB gate.

This README evidence update creates a new branch commit. Consequently, all run and APK evidence above applies **only** to `7b19ca760ac529f2f157ae1bc516a3d0333029b6`; it must not be carried forward as proof for the new README commit. Fresh Actions runs and a fresh artifact are required for the new exact branch head. No application source was changed, `main` was not modified, and PR #9 was not merged.


## Cross-project reuse audit — 2026-11-15

See [the branch and private-repository reuse audit](docs/PROJECT_REUSE_AUDIT_2026-11-15.md). AvatarForge's standalone engine has a real adult-only unclothed base surface in source, but this Hub UI still selects `Outfit.JEANS_JACKET` and does not expose the adult confirmation gate. The closest real 3D-video donor is 3DDD's Filament `MediaCodec`/`MediaMuxer` exporter; its camera update and per-frame animation sampling need correction before reuse. No detailed anatomical mesh donor was found. The prepared Dropbox rig remains **UNVERIFIED** until its binary is inspected.

## Bounded GLB import allocation fix (2026-11-17)

- Replaced the in-memory `ByteArrayOutputStream` accumulation path with `BoundedGlbReader`: the input is streamed to a bounded app-cache spool, then read into one exact-sized byte array. The configured 128 MiB import ceiling remains enforced; empty and oversized input are rejected and the temporary file is removed on success or failure.
- Added JVM regression tests for exact byte preservation, oversized/empty rejection and temporary-file cleanup. Corrected the native sampling test's `REGION_HAIR` reference to its namespaced enum.
- Exact-head `testDebugUnitTest` was started for this working tree; its result must be recorded from the completed Gradle output. No phone runtime or generated-avatar render pass is claimed. The earlier imported-GLB OOM remains a separate required gate until the exact-head test and emulator/device evidence pass.
- Root `.kotlin/` is a local generated cache and is ignored, not committed.

## Exact-head Android test APK readiness monitor (2026-11-17)

**Decision: NOT READY.** PR #9 remains open against `main`. At inspection, PR head and `integration/avatarforge-renderer-port` both resolved to `37561b19f266cd1d0d7d849b6441c47e2456121e`. The complete Actions run query for this SHA returned two runs, both push-triggered:

- [Android CI run 43957608408](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43957608408) — overall **FAILURE**.
  - Native C++ sampling regression, job `133616261791`: **PASS**.
  - Unit tests, job `133616262204`: **PASS**.
  - Android lint, job `133616262287`: **PASS**.
  - Release R8/minification, job `133616262477`: **PASS**.
  - Debug APK build, job `133616262558`: **PASS**.
  - AvatarForge GLES GLB render smoke, job `133616262483`: **FAIL**. Job log records `AvatarForgeRendererSmokeTest#generatedAvatarDebugDiagnosticVerifiesImportedGlesPixels` failed, followed by “Instrumentation run failed due to Process crashed.” The available log does not identify a more specific fatal cause. This job runs the separate renderer-smoke test class; it is not evidence that the required dedicated `GeneratedAvatarRuntimeTest#generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView` passed.
- [Release preview and Android emulator smoke run 43957608293](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43957608293) — overall **SUCCESS**.
  - Signed release preview with a one-run ephemeral CI key, job `133616200164`: **PASS**.
  - Emulator install/launch smoke, job `133616200821`: **PASS**. This is not the production render-test gate and the ephemeral signing key is not a retained production identity.

### Exact-SHA artifacts

- Fresh debug APK artifact [14271344075](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43957608408/artifacts/14271344075) was produced by the Debug APK build job for this exact SHA.
- Fresh signed-preview artifact [14271949045](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43957608293/artifacts/14271949045) was produced by the release-preview run for this exact SHA.
- Renderer diagnostic evidence artifact [14274450108](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43957608408/artifacts/14274450108) was uploaded by the failed renderer job.
- These artifacts are not a READY APK. The APK binaries have not been downloaded and inspected for package ID, signing certificate/signature and APK SHA-256 because the mandatory production render test did not pass.

### Mandatory production render test: missing on this SHA

The required test exists in source as `GeneratedAvatarRuntimeTest#generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView`. The dedicated diagnostic workflow contains a step that invokes this exact method, and the generated-runtime workflow invokes the containing test class. **Neither workflow has a run associated with this exact SHA.** The full exact-head Actions run list contains only the two runs above. The available imported-GLB/GLES smoke failure and successful emulator launch cannot substitute for the missing production generated-GLB-to-GLES SurfaceView result.

### Decision and next check

**NOT READY:** the required production test has no exact-head run/pass, and the Android CI renderer smoke job failed. All ordinary build jobs passed and fresh exact-head APK artifacts exist, but neither fact overrides the missing/failed render gate. Do not distribute either artifact as the first READY APK. No APK signing identity or APK digest has been independently inspected for this head.

This monitor update is documentation-only. It does not modify application source, merge PR #9, or modify `main`. The README commit itself will create a new branch head and invalidates all evidence above as proof for that new head; the next check must fetch the new exact PR/branch SHA and inspect only its associated workflow runs and artifacts.


## Exact-head Android test APK readiness monitor (2026-11-18)

**Decision: NOT READY.** PR #9 remains open and unmerged. At inspection, both PR #9 and `integration/avatarforge-renderer-port` pointed to `4ce9e803f839f36e69fa4bfdd927caf5cb44e1a3`.

### Exact-head Actions

- [Android CI run 44029670654 (run 662)](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44029670654): **FAILURE**.
  - Unit tests, job `133858290397`: PASS.
  - Debug APK build, job `133858290360`: PASS.
  - Native C++ sampling regression, job `133858290350`: PASS.
  - Android lint, job `133858290619`: PASS.
  - R8/minification, job `133858290505`: PASS.
  - AvatarForge GLES GLB render smoke, job `133858289938`: **FAIL**.
- [Release preview and Android emulator smoke run 44029670636 (run 460)](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44029670636): SUCCESS. Emulator install/launch and ephemeral-key preview are not the production render gate.

### Production render test is not being invoked

The required test exists at `app/src/androidTest/java/com/davemega/hub/avatarforge/GeneratedAvatarRuntimeTest.kt` as `GeneratedAvatarRuntimeTest#generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView`. It generates a GLB and requires the production `AvatarForgeSurfaceView` to show visible pixels, submit draw calls, present multiple frames and save a screenshot.

However, `.github/scripts/avatarforge-render-smoke.sh` currently invokes only `com.davemega.hub.AvatarForgeRendererSmokeTest`. The exact-head instrumentation log confirms the executed test was `AvatarForgeRendererSmokeTest#generatedAvatarDebugDiagnosticVerifiesImportedGlesPixels`; it does not show the required `GeneratedAvatarRuntimeTest` method running. Therefore the mandatory production generated-GLB-to-GLES SurfaceView test has **NO PASSING RESULT on this SHA**.

The executed imported-GLB test failed. Its uploaded [renderer evidence artifact 14305007547](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44029670654/artifacts/14305007547) contains logcat showing Filament aborting while reading the selected GLB:

```text
utils::PreconditionPanic
reason: uniform named "sheenColorIndex" not found
Fatal signal 6 (SIGABRT)
```

This is the observed failure in the imported-GLB preview path. It does not prove the production generated-avatar path has been exercised.

### Exact-head APK artifact

A fresh debug APK artifact exists for this exact source SHA:

- [Debug APK artifact 14304801634](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44029670654/artifacts/14304801634), from the Debug APK build job on this SHA.
- Renderer evidence artifact: [14305007547](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44029670654/artifacts/14305007547).
- Signed preview artifact: [14305430345](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44029670636/artifacts/14305430345), signed with an ephemeral CI key.

The debug APK binary has **not** been downloaded and inspected in this check for package ID, signing certificate/signature and APK SHA-256. Those fields remain **UNVERIFIED**. Artifact existence and a successful emulator install/launch do not override the failed renderer job or missing production test execution.

### Decision and next check

**NOT READY.** Required next work is to wire the exact `GeneratedAvatarRuntimeTest#generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView` into the CI instrumentation command, fix the observed Filament `sheenColorIndex` abort in the separate imported-GLB preview path, then rerun on one exact current head. Only after the production test passes and all required Android build jobs pass should the fresh same-SHA APK be downloaded and its package, signature and SHA-256 verified.

This entry is documentation-only. It does not change application source, modify `main`, or merge PR #9. Its README commit creates a new branch head, so the run and artifact evidence above applies only to `4ce9e803f839f36e69fa4bfdd927caf5cb44e1a3`; fresh exact-head Actions evidence is required for the new head.


## Exact-head Android test APK readiness monitor (2026-11-18, head `6a745d260997e3656eafcd2d316f8afafef35e94`)

**Decision: NOT READY.** PR #9 remains open and unmerged. PR #9 head and `integration/avatarforge-renderer-port` both resolved to this exact SHA when checked.

### Exact-SHA workflow runs

- [Android CI run 44237202777](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44237202777): **FAILURE**.
  - Debug APK build, job [134576085121](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44237202777/job/134576085121): **PASS**.
  - Native C++ sampling regression, job 134576085581: **PASS**.
  - Release R8/minification, job 134576085601: **PASS**.
  - Android lint, job 134576085616: **PASS**.
  - Unit tests, job 134576085674: **PASS**.
  - AvatarForge GLES GLB render smoke, job [134576085510](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44237202777/job/134576085510): **FAIL**. Its job log shows `AvatarForgeRendererSmokeTest#generatedAvatarDebugDiagnosticVerifiesImportedGlesPixels` failed, followed by `Instrumentation run failed due to Process crashed`; Gradle reports `:app:connectedDebugAndroidTest` failed. The log excerpt does not establish a more specific crash cause.
- [Release preview and Android emulator smoke run 44237202697](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44237202697): **SUCCESS**.
  - Signed release preview using an ephemeral one-run CI key, job 134576084735: **PASS**.
  - Emulator install/launch smoke, job 134576084984: **PASS**. This is not production renderer proof.

### Mandatory generated-GLB production SurfaceView test

The required test exists in source at `app/src/androidTest/java/com/davemega/hub/avatarforge/GeneratedAvatarRuntimeTest.kt` as `GeneratedAvatarRuntimeTest#generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView`. It generates a GLB and exercises the production `AvatarForgeSurfaceView`, requiring visible pixels, draw calls and presented frames.

However, the exact-head workflow script `.github/scripts/avatarforge-render-smoke.sh` invokes only `com.davemega.hub.AvatarForgeRendererSmokeTest`. The completed exact-SHA job log confirms that the imported-GLB test above ran and failed; it contains no execution or pass result for the required `GeneratedAvatarRuntimeTest` method. Thus the required production generated-GLB-to-GLES SurfaceView test is **MISSING on this SHA**, not passed.

### Exact-SHA artifacts

- Fresh debug APK artifact [14398465631](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44237202777/artifacts/14398465631) was uploaded by the debug build job for this exact SHA. Artifact archive digest: `sha256:032cb1810f957c4be233cb8dda8f776931b54b437cc1c18bb41c8b35f6565d52`.
- Failed-render diagnostic artifact [14398258248](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44237202777/artifacts/14398258248) was uploaded by the failed renderer job.
- Signed preview artifact [14398276866](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44237202697/artifacts/14398276866) was produced for this exact SHA using an ephemeral CI signing key.

The APK archive was **not downloaded or inspected** for package ID, APK signing certificate/signature or APK-file SHA-256. Those values remain **UNVERIFIED**. Artifact archive digests are not APK-file hashes.

### Decision and next check

**NOT READY.** The production generated-GLB-to-GLES SurfaceView test did not run; the only executed imported-GLB GLES instrumentation test failed and its process crashed. Ordinary build jobs and emulator install/launch passed, and fresh same-SHA APK artifacts exist, but none overrides the missing/failed renderer gate. Do not distribute either APK artifact as the first READY test APK.

This entry is documentation-only. No application source was changed, `main` was not modified, and PR #9 was not merged. This README update creates a new branch head, so all evidence above applies only to `6a745d260997e3656eafcd2d316f8afafef35e94`; the new head requires fresh exact-SHA workflow runs and artifact evidence.

## Launcher branding update — 2026-11-18

- Added the electric-blue / silver wolf artwork as the Android launcher icon resource at `app/src/main/res/drawable-nodpi/ic_launcher_art.png`.
- Added adaptive launcher icon definitions for Android 8.0+ and legacy bitmap resource wrappers for earlier Android versions.
- Updated the manifest to use `@mipmap/ic_launcher` and `@mipmap/ic_launcher_round`.
- The icon change is committed on `integration/avatarforge-renderer-port`; `main` remains untouched.
- **Build status: UNVERIFIED** until GitHub Actions builds the exact new head. **Physical Samsung launcher appearance: UNVERIFIED** until installed and checked on the SM-S921B.


## Exact-head Android test APK readiness monitor (2026-11-19, inspected head `ce45178d01c9e45704cea5e1ee59836ccf4c6489`)

**Decision: NOT READY.** PR #9 is open and unmerged. Fresh metadata showed PR #9 head `integration/avatarforge-renderer-port` and the branch ref both point to `ce45178d01c9e45704cea5e1ee59836ccf4c6489`.

### All Actions runs associated with this exact SHA

The complete `actions/runs?head_sha=ce45178d01c9e45704cea5e1ee59836ccf4c6489&per_page=100` query returned two runs:

- [Android CI run 44331554038](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44331554038), attempt 2: **FAILURE**.
  - Native C++ sampling regression, job [135123928802](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44331554038/job/135123928802): **PASS**.
  - Unit tests, job [135124007631](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44331554038/job/135124007631): **PASS**.
  - Android lint, job [135124000193](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44331554038/job/135124000193): **PASS**.
  - Release R8/minification, job [135123999791](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44331554038/job/135123999791): **PASS**.
  - Debug APK build, job [135123930415](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44331554038/job/135123930415): **PASS**.
  - AvatarForge GLES GLB render smoke, job [135123926596](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44331554038/job/135123926596): **FAIL**. Its log shows only `AvatarForgeRendererSmokeTest#generatedAvatarDebugDiagnosticVerifiesImportedGlesPixels` completed and failed; instrumentation then reported `Instrumentation run failed due to Process crashed`. The job script invokes the `AvatarForgeRendererSmokeTest` class, not the required production generated-GLB test.
- [Release preview and Android emulator smoke run 44331554271](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44331554271): **SUCCESS**.
  - Signed release preview with a one-run ephemeral CI key, job [134897213931](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44331554271/job/134897213931): **PASS**.
  - Emulator install/launch smoke, job [134897214639](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44331554271/job/134897214639): **PASS**. This is not the production render gate.

### Exact-SHA artifacts

- Fresh debug APK artifact [14440245997](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44331554038/artifacts/14440245997), uploaded by the successful Debug APK build job.
- Signed preview artifact [14439722134](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44331554271/artifacts/14439722134), produced by the ephemeral-key preview job.
- Renderer diagnostics artifact [14471562313](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44331554038/artifacts/14471562313), uploaded by the failed renderer job.

These are all tied to source SHA `ce45178d01c9e45704cea5e1ee59836ccf4c6489`. APK package ID, APK signing certificate/signature and APK-file SHA-256 have **NOT BEEN INSPECTED**. The artifacts do not qualify for distribution because the mandatory production render gate failed or did not run.

### Required production generated-GLB-to-GLES SurfaceView test: MISSING

The test exists at `app/src/androidTest/java/com/davemega/hub/avatarforge/GeneratedAvatarRuntimeTest.kt` as `GeneratedAvatarRuntimeTest#generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView`. The exact-SHA workflow `.github/workflows/android.yml` runs `.github/scripts/avatarforge-render-smoke.sh`, which explicitly selects `com.davemega.hub.AvatarForgeRendererSmokeTest`. The separate `.github/workflows/avatarforge-generated-runtime.yml` and `avatarforge-diagnostic-apk.yml` files contain generated-avatar test commands, but neither has a workflow run associated with this SHA. No passing production generated-GLB-to-GLES SurfaceView result exists for this head.

### Decision

**NOT READY.** Ordinary Android build jobs passed and fresh APK artifacts exist, but the renderer instrumentation job failed on the imported-GLB diagnostic test and the required production generated-GLB SurfaceView test has no exact-SHA run. Do not distribute these artifacts as the first READY APK. No APK binary identity/signature/digest inspection is claimed.

This monitor entry is documentation-only. It does not alter application source, merge PR #9 or modify `main`. The README commit itself creates a new branch head and invalidates all run/artifact evidence above as proof for that new head. The next check must fetch the new PR and branch SHA and inspect only that exact SHA's workflow runs and artifacts.


## Exact-head Android test APK monitor update — 2026-11-27

**Decision: NOT READY.** Fresh PR metadata and the branch ref both resolved to `integration/avatarforge-renderer-port` at exact head `9baf7c670052748887bbadcd7aebf970380687f0`. PR #9 remains open and unmerged. `main` was not modified.

### All workflow runs associated with this exact SHA

The repository Actions run listing was checked and filtered by the full `head_sha`. Three runs were associated with this exact commit:

- [AvatarForge generated-avatar runtime verification #45779175991](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/45779175991), event `workflow_dispatch`: **FAILURE**.
  - Job [139770174776 — Generate, render and inspect real avatar pixels](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/45779175991/job/139770174776): **FAILURE**.
  - The job explicitly ran `GeneratedAvatarRuntimeTest`. Its log confirms the required method `GeneratedAvatarRuntimeTest#generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView` executed against the production `AvatarForgeSurfaceView`.
  - The method **FAILED**, not passed: `No drawable EGL-presented model frame within 15 seconds (frames=2 presented=2 framePresented=true draws=29 triangles=1642936 boundsInFrustum=true outsideCorners=0 glError=0x0 eglError=0x0)`. The generated GLB reached the production renderer and issued draw calls, but the visible/drawable frame assertion did not pass.

- [Android CI #44464520176](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44464520176), event `push`: **FAILURE**.
  - Debug APK build, job [135358958838](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44464520176/job/135358958838): **PASS**.
  - Native C++ sampling regression, job [135358958867](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44464520176/job/135358958867): **PASS**.
  - Release R8/minification, job [135358958900](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44464520176/job/135358958900): **PASS**.
  - Unit tests, job [135358958950](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44464520176/job/135358958950): **PASS**.
  - Android lint, job [135358959006](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44464520176/job/135358959006): **PASS**.
  - AvatarForge GLES GLB render smoke, job [135358958953](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44464520176/job/135358958953): **FAIL**. The executed test was `AvatarForgeRendererSmokeTest#generatedAvatarDebugDiagnosticVerifiesImportedGlesPixels`, not the required generated-GLB production SurfaceView method. Instrumentation reported a process crash after the imported-GLB test failed.

- [Release preview and Android emulator smoke #44464520154](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44464520154), event `push`: **SUCCESS**.
  - Signed release preview, job [135358958510](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44464520154/job/135358958510): **PASS**, using a one-run ephemeral CI signing key.
  - Emulator install/launch smoke, job [135358958766](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44464520154/job/135358958766): **PASS**. This is not a production render pass.

### Exact-SHA artifacts

- Runtime test evidence artifact [15098675509](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/45779175991/artifacts/15098675509) was uploaded by the failed generated-runtime job. It is diagnostic evidence, not an APK.
- Renderer smoke evidence artifact [14502385622](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44464520176/artifacts/14502385622) was uploaded by the failed imported-GLB smoke job.
- Emulator smoke diagnostics artifact [14502130483](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44464520154/artifacts/14502130483).
- Signed preview artifact [14502035803](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44464520154/artifacts/14502035803) was produced for this exact SHA by the successful ephemeral-key release-preview job. The workflow's signature/provenance step passed, but the APK binary has not been independently downloaded and inspected in this monitor run.

APK package/application ID, APK signing certificate identity and APK-file SHA-256: **UNVERIFIED**. No APK-file digest is inferred from an artifact ZIP or workflow provenance record.

### Readiness decision

**NOT READY.** The required production generated-GLB-to-GLES SurfaceView test now has an exact-SHA execution, but it **fails** with only two presented frames and no drawable model frame within 15 seconds. All ordinary Android build jobs passed, and a fresh signed preview artifact exists for the same SHA, but neither can override the failed production render gate. Do not distribute this APK as the first READY test APK.

This update is documentation-only. No application source was changed, no branch was created, no force-push occurred, PR #9 remains unmerged, and `main` remains untouched. This README commit changes the branch head; all evidence above applies only to `9baf7c670052748887bbadcd7aebf970380687f0`. Fetch the new PR and branch SHA and require fresh exact-head Actions and artifact evidence on the next check.
