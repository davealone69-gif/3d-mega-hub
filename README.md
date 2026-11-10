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

## Prompt-to-avatar validation and next stage

**Owner phone-test finding (2026-11-11):** changing the prompt did not visibly change the displayed character. Treat prompt-driven visible variation as **FAILED IN USER TEST / NOT FIXED**. Renderer success and a generated GLB do not establish that prompt attributes affect the exported character.

### Two parser-supported diagnostic prompts

Use these exact inputs as a differential test:

1. `Adult man, muscular build, tall, dark skin, short black hair, red shirt`
2. `Adult woman, slim build, short, pale skin, long brown hair, blue dress`

The native parser contains explicit vocabulary for male/female, muscular/slim, tall/short, skin-tone qualifiers, hair length and named colours. These examples are chosen from that vocabulary. They are test inputs only; successful parsing is not proof that the renderer visibly applies every attribute.

### Next stage: prove prompt changes reach the model

Work only on `integration/avatarforge-renderer-port` and keep PR #9 as the sole active review.

1. **Parser differential regression (first gate).** Add native tests for both exact prompts. Assert the parsed specs differ in sex, build, height, skin melanin, hair style/colour and garment colour where those fields are supported. Also test parsing prompt B after prompt A using the same spec object, and compare against parsing B from a fresh default spec. This catches stale state and prompt-order leakage.
2. **Generation propagation (second gate).** Generate two independent GLBs from those parsed specs. Record each prompt's matched tokens and resolved spec. Compare exported GLB structure, vertex positions, bounds, normals and material/base-colour data. Require meaningful, attributable differences; different file hashes alone are insufficient.
3. **Renderer differential (third gate).** Load both generated GLBs through the real AvatarForge GLES route under the same camera, lighting and viewport conditions. Capture both images and verify visible differences in the expected regions. Preserve the GLBs, spec reports, captures and hashes as CI artifacts.
4. **UI truthfulness (fourth gate).** Show which prompt tokens were recognised and which were ignored. Warn or reject a prompt with no supported attributes. Do not report a successful personalised result merely because a default mesh was exported.
5. **Phone verification (release gate).** Install the exact candidate on the Samsung SM-S921B. Run both prompts and compare the actual displayed characters. Record APK SHA-256 and captures. Emulator evidence does not replace this phone test.

### Prompt-generation evidence ledger

| Item | Status | Evidence / next action |
|---|---|---|
| Deterministic native prompt parser exists | **REAL + VERIFIED (SOURCE)** | `af_prompt.cpp` contains explicit attribute vocabularies and spec assignments |
| Two supported diagnostic prompts documented | **REAL + VERIFIED (SOURCE)** | Exact prompts above use parser-recognised vocabulary |
| Prompt changes visibly change the character | **FAILED IN USER TEST** | Owner reports changing the prompt did not change the displayed picture; reproduce and diagnose |
| Parsed attributes propagate into distinct exported GLBs | **MISSING** | Add parser and generated-GLB differential regression tests |
| Two prompts render visibly different characters | **MISSING** | Add controlled GLES image comparison |
| Prompt recognition/ignored-token feedback | **PARTIAL / NEEDS VALIDATION** | Generation result has a prompt-match warning path; UI should expose recognised and ignored tokens clearly |
| AI/natural-language understanding | **MISSING** | Current implementation is deterministic keyword parsing, not an LLM or generative AI model |

Do not mark this stage complete based on a successful renderer smoke test, a non-empty GLB, different filenames or different file hashes alone. The proof must connect prompt → parsed spec → generated geometry/materials → visible rendered difference.
