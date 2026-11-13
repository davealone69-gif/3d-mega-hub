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
- **Debug verification:** debuggable builds include a seed-1337 generated-avatar diagnostic that validates the deformed mesh, imports the exported GLB through the real GLES route, waits for successful EGL swaps and verifies background-subtracted PixelCopy geometry. A PNG result is saved. A scoped AndroidX FileProvider exposes only app-private external-files captures for sharing, including `failed_render.png`.
- **Avatar imports:** selected GLBs are copied into app-private storage, size-limited and structurally validated before use. External-resource `.gltf` files are rejected.
- **Local AI:** Ollama remains an external service, not an embedded LLM. Plain HTTP is restricted to loopback; remote endpoints require HTTPS.
- **Voice:** Android TextToSpeech output is connected. Viseme extraction and avatar lip-sync are not connected.

## Sample-model limitation

The bundled `female.glb` is a renderer fixture, not proof of a complete rigged avatar. Inspection of the exact bundled file shows one mesh, one node, seven morph targets, zero skins, zero animations and zero embedded images. It can exercise GLB loading/rendering, but does not prove skeletal animation, facial animation, texture quality or generated-avatar quality.

The fixture is byte-identical to the file at 3DDD revision `be5bf17d8afe46247646465941fa1b7317d8e415` (Git blob `a1c00c6b31f6ab0db3b2e68243526cb2d0e15967`). The donor README at that revision states the generated base rigs are original work released under CC0. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Execution plan

Work through these steps in order, on the single canonical branch. Do not skip a failed gate or mark a later step complete on the strength of an earlier one.

1. **Finish exact-head CI.** Run the complete Android workflow against the latest canonical commit: debug APK assembly, unit tests, Android lint, R8/minification, native host regression, imported-GLB GLES instrumentation, generated-avatar runtime instrumentation, and emulator install/launch smoke. Fix failures in this same branch and rerun the affected jobs.
2. **Prove real renderer output.** Require the instrumentation tests to load an actual GLB through the app's native GLES path, compile/link shaders, upload geometry and textures, draw frames, complete EGL swaps, and produce a PixelCopy image with geometry pixels above the test threshold. Preserve raw captures and diagnostics on failure. No fake success values, mocked renderer, or weakened pixel threshold.
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
| Native host regression | **PASS** | Latest audited CI run reported this job successful |
| Screenshot API correction | **COMMITTED, CI PENDING** | Instrumentation now uses `UiAutomation.takeScreenshot()`; rerun exact-head instrumentation |
| Emulator smoke correction | **COMMITTED, CI PENDING** | Uses successful `am start -W` and a live process PID rather than brittle `dumpsys` text matching; downstream renderer checks remain |
| Debug APK, unit tests, lint and R8 | **PENDING / RUNNING AT LAST CHECK** | Require completed successful jobs for the latest commit |
| Imported-GLB GLES instrumentation | **PENDING / RUNNING AT LAST CHECK** | Must produce real visible-pixel evidence |
| Generated-avatar runtime instrumentation | **PENDING / RUNNING AT LAST CHECK** | Must complete, not merely build |
| Emulator install/launch and render | **PENDING / RUNNING AT LAST CHECK** | Require completed install, launch and renderer checks |
| Samsung SM-S921B runtime | **UNVERIFIED** | Requires installation and testing on the physical phone |
| Production signing | **BLOCKED** | Requires owner-provided production keystore in protected CI secrets |
| Merge to `main` | **BLOCKED BY GATES** | Do not merge until steps 1-6 are satisfied |

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
