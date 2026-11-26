# AvatarForge Neural Core — master architecture, repository intelligence and mission command

**Role of this document.** Prepared as PRINCIPAL AI ARCHITECT / TECHNICAL DIRECTOR / MISSION COMMANDER.
It contains no implementation. It establishes what actually exists, fixes the architecture, divides the
work into independent engineering missions, and gives each mission a paste-ready execution prompt for a
specialist agent.

**Scope and merge decisions remain the owner's.** Nothing here authorises a merge.

**Status of this document: PLANNING. UNVERIFIED as to every forward-looking claim.** All statements about
the *existing* repository carry a status word and were measured on the working tree at
`ba38d996074c2f8a9981ebbaeb0fee3e3cd4af5e` on 2026-11-26 unless another commit is named. This document was
committed as `281806e7dcd455fcfc8885ca9faff1b14004aa0d` on the same day — a documents-only commit whose
entire diff is this file — so any statement measured "on the working tree at `ba38d996`" remains true at
`281806e7` **except** the runtime-gate results in §8.1 **B1**, which were re-measured at the new head.

**Placement decision, and why.** `settings.gradle.kts` declares exactly two modules, `:app` and
`:avatarforge-engine`, with `rootProject.name = "3D Mega Hub"`. AvatarForge *is*
`avatarforge-engine/` — the standalone engine, SDK and laboratory. Neural Core therefore belongs in
`avatarforge-engine/`, and **not** in `:app`. That reading also satisfies the instruction not to merge
this work into 3D-Mega-Hub and not to build a chatbot in a 3D application: the chatbot already exists in
`:app` (`MainActivity.kt`), it is a dead end, and §3.A explains how it is demoted rather than extended.

> ### Owner control — eight standing instructions, 2026-11-26
>
> | # | Instruction | Where honoured |
> |---|---|---|
> | 1 | Do not merge **PR #17** without explicit approval | §8.3 **D1** open; nothing merged; PR #17 remains OPEN and unmerged |
> | 2 | Do not create implementation branches until PR #17 and the **PR #16 / PR #9** overlap are resolved | §8.3 **D1**+**D2** both open; §7 item **13** makes this a prohibition; §4.4's waves are gated behind them |
> | 3 | Prioritise **MHR** for a full technical **and licence** comparison against **Anny**; adopt neither automatically | **§6.4** — full comparison, neither adopted, D10 left open |
> | 4 | Complete **D9** with separate licensing/provenance rules for **procedural, AI-generated and imported** assets | **§3.I.1** — three rule sets, three provenance profiles, five cross-class rules |
> | 5 | Preserve the historical **SM-S921B** measurements as a **baseline**; never as current runtime verification | **§3.L.1** carries a standing warning banner; §7 item **14** prohibits quoting them as current |
> | 6 | Ensure the **morph-target importer gap** is assigned to the correct engine mission | **ASSIGNED TO M5** as deliverable **(f)** — §5.7, §4.1 register, §3.D item 6, §8.1 **B5** |
> | 7 | Preserve `arena/089497bd` and all unique work | **§8.4** — nine items of unique work listed with recovery commands; §7 item **10** |
> | 8 | Keep all **ten missions** and their acceptance criteria intact | Unchanged: ten prompts, execution order `M0 → M8 → M1 → M2 → M3 → M4 → M5 → M6 → M7 → M9`, all eight required fields machine-verified present in each. M5 gained deliverable (f); its acceptance criteria (a)–(e) are untouched |
>
> **No implementation, destructive Git operation, or merge has been performed under this document.**

**How this document is committed.** It was originally intended for its own branch and its own
documents-only PR, to avoid combining unrelated objectives with PR #17. That intent is **superseded** by two
facts: the owner instructed on 2026-11-26 to *"push the 10 commands to git"*, and instruction 2 forbids
creating branches while PR #17 and the PR #16/#9 overlap are unresolved. The only permitted push target is
therefore the existing working branch `arena/1535c8f6-3d-mega-hub`, which is PR #17's head.

**Consequence, stated plainly:** the commit containing the ten mission prompts **appears in PR #17**. It
does **not** merge PR #17, does not change PR #17's code, and does not approve it — the PR's *code* diff is
unchanged, only a document is added. If the owner would rather the plan were not visible on PR #17, the
remedy is to close PR #17 and re-open it from a new branch **after** D1/D2 are resolved; that is the owner's
call (**D3**), and no agent may make it.

**Sections E and F are confirmed.** The commissioning brief was delivered truncated twice, ending mid-word
inside subsystem D at "Deformation". On 2026-11-26 the owner confirmed the two missing sections as
**E — Evaluation Intelligence** and **F — Learning, Memory and Continual Improvement**, and required them
completed. They are now written in full (§3.E, §3.F), no longer marked as inferred, and F covers all eight
components the owner listed: experience memory, approved feedback, experiment history, quality measurements,
regression detection, dataset management, optional local model fine-tuning, and versioned models with
rollback.

**One residual gap.** The brief's own section D ended at the single word "Deformation". Deformation is
designed here as a *measurement* discipline (§3.D item 9, §3.E.2). If the owner's section D intended more —
corrective blendshapes, cloth or muscle simulation, penetration resolution — that is **not designed in this
document** and must be re-issued. Recorded as an open item in §8.

---

## 1. Repository intelligence

Rule applied throughout, from `AGENTS.md` §2.1: **REAL + VERIFIED**, **IMPLEMENTED IN SOURCE**,
**PARTIAL**, **UNVERIFIED**, **MISSING**, **BLOCKED**, and *where* it was verified. A class, interface or
button existing is not evidence that a feature works.

### 1.1 Shape of the repository

| Item | Measured fact |
|---|---|
| Modules | `:app`, `:avatarforge-engine`. No third module. |
| C/C++ | 42,167 lines under `avatarforge-engine/src/main/cpp/` |
| Kotlin, engine | 3,319 lines under `avatarforge-engine/src/main/java/` |
| Kotlin, app | 4,704 lines under `app/src/main/java/` |
| C ABI | One header, `include/afcore.h`; 55 exported `af_*` functions |
| Native libraries | `libafcore.so` (engine), `libafrender.so` (renderer), `libmega_hub_avatarforge.so` (generation JNI) |
| JNI bridges | `avatarforge_jni.cpp` (4 `JNIEXPORT`, generation) and `avatarforge_renderer_jni.cpp` (31 `JNIEXPORT`, render session / camera / animation / diagnostics) |
| Cross-language contract | `af_spec` crosses as **45 floats** (`kSpecFloatCount = 45`), mirrored field-for-field by `AvatarSpec.kt`, guarded by `static_assert(33 + 1 + 3 + AF_FACE_PARAM_COUNT == kSpecFloatCount, "spec packing drifted")` |
| Third-party C++ | **zlib only.** No Eigen, no assimp, no Draco, no meshopt, no KTX2 |
| Toolchain | Gradle wrapper 8.11.1, JDK 17 target (CI uses 21), compileSdk/targetSdk 35, minSdk 26, NDK 26.1.10909125, ABIs arm64-v8a + x86_64 |

### 1.2 The generation pipeline

`AGENTS.md` §4 states it, and it is the single most important fact for Neural Core:

```
prompt or creator choices -> af_spec -> anatomy -> field -> mesh -> UV -> textures -> rig -> blendshapes -> clips -> GLB -> import -> draw
```

**The engine is not a generative model. It is a deterministic solver.** Every stage is a pure function of
`af_spec` plus a seed. That is why the same spec and seed export byte-identical GLBs — measured:
`male=73,399,512 B`, `female=72,076,316 B`, reproducible across runs and across two different code changes.

Consequence for architecture: **Neural Core must not replace the solver.** It must produce better *inputs*
to it and better *judgements* of its outputs. This is the same conclusion `docs/PHOTOREAL_UPGRADE_PLAN.md`
reaches independently ("keep the machine, change the content").

### 1.3 Inventory against the eighteen requested areas

| # | Area | What genuinely exists | Status |
|---|---|---|---|
| 1 | Engine architecture | Deterministic SDF pipeline, §1.2. `af_field.cpp` (3,128 lines at `2920265`) is the single signed-distance field for body, head, face, hands, feet, hair, clothes, shoes, armour and gear — *"everything visible is a branch of this function"* | **REAL + VERIFIED** on host by 12 `ctest` targets |
| 2 | Android app + SDK modules | `:app` is the hub (5 pages: Home, Avatar, AI, Voice, Settings). `:avatarforge-engine` is the SDK: `AvatarForgeEngine`, `AvatarForgeSurfaceView`, `AvatarSpec`, `DeviceResourceGovernor`, `DiagnosticDownloads`, `EvidenceIdentity`, `GlbValidator`, `RendererTelemetry`, `animation/` | **REAL + VERIFIED** — CI debug APK build, lint, R8, unit tests all green on `ba38d99` |
| 3 | C++ engine components | `af_anatomy` (anthropometric solver → `BodyMetrics`, landmarks, girths; a tape measurement in the spec is met exactly), `af_isosurface`/`af_body` (one 4 mm lattice, marching tetrahedra, red-green refinement, Taubin smoothing, re-projection, normals from field gradient), `af_uv` (charts, shelf packing into one 2048² atlas, seam splitting), `af_texture` (procedural paint in UV space as a function of the 3D point: base colour, normal, metallic-roughness, occlusion, subsurface), `af_rig` (71 joints, four-influence weights, 52 ARKit blendshapes as analytic displacements, clip library), `af_glb`/`af_glb_read` | **REAL + VERIFIED** on host |
| 4 | Kotlin/Java interfaces | `AvatarSpec.kt` mirrors `af_spec` in the C order; `AvatarForgeEngine.generate(...)` is cancellable on `Dispatchers.IO`; `AvatarForgeSurfaceView` owns the render thread, camera (`resetView`, `zoomBy`, `turnBy`), animation API and a PixelCopy check | **REAL + VERIFIED** via CI unit tests + instrumented smoke |
| 5 | C ABI and JNI | 55 `af_*` functions; 35 `JNIEXPORT` entry points. `af_spec` is versioned: `struct_size` plus a `memcpy` bounded by `min(sizeof(af_spec), struct_size)`, so a caller built against an older ABI is safe and new fields keep their `af_spec_init` defaults. Enum values are ABI: **append only** | **REAL + VERIFIED** on host; the ABI-growth rule is exercised by the `cloth_metalness` addition in `42fa78a` |
| 6 | Geometry and GLB pipelines | Export: one file, sparse morph targets, sampled animations, PNG textures, an `extras` block carrying the spec. Import: `af_glb_read.cpp` (1,786 lines), fuzzed, refuses every glTF extension except `EXT_texture_webp` | **REAL + VERIFIED** on host (`glb_extension`, `imported_glb_pbr`, `glb_import_robustness` fuzz, `animation_roundtrip`). Draco/meshopt/KTX2 — **MISSING** by design. **Corrected 2026-11-26 by direct inspection:** the importer contains **zero occurrences of the string
`morph`** — morph targets are **not read at all** on import, not merely read-and-ignored. The only related
line is `af_glb_read.cpp:1510`: `else continue; /* "weights" is not used by this engine's rig */`.
`afcore.h:390` confirms the **exporter** does write them (*"targets as morph targets"*), and
`afcore.h:413-414` documents the import side as unsupported: a foreign rig *"is shown static and the joint
count reads 0"*, its animation channels are not applied, and morph targets are excluded — *"facial
blendshapes are available on a generated avatar, not on an import"*. Export and import are **asymmetric**. Morph-target import is therefore **MISSING** |
| 7 | Filament integration | `FilamentPreviewView.kt` (1,180 lines after `42fa78a`): orbit, zoom, one shadow-casting directional light plus ambient, 4× MSAA, FXAA, ambient occlusion, PBR Neutral tone curve; plays a file's own clips. `GlbPreviewRouting.kt` decides which preview a file opens in. **A generated avatar is never sent to Filament**, nor a file whose textures exceed the budget | **REAL + VERIFIED** by CI instrumented smoke (`GlbClipPreviewTest`, `AvatarForgeRendererSmokeTest`) |
| 8 | GLES rendering | `af_gl.cpp` — *"the renderer the app uses"*: EGL + OpenGL ES 3.1, shadow pass, forward pass with MSAA into an HDR target, bloom, tone map, GPU skinning via uniform block `AfSkinBlock` / instance `afBones`. `af_shaders_gl.h` holds all GLSL as strings (8,987 lines at `2920265`). `af_render.cpp` is `BudgetRenderer`, four quality tiers (LOW/MEDIUM/HIGH/ULTRA over MSAA, shadow-map size, bloom); AUTO starts at MEDIUM and steps only MSAA/shadow/bloom. `af_vk.cpp` + `af_shaders_vk.h` (5,500 lines): **stale, not compiled** (`AF_ENABLE_VULKAN` off), no GPU skinning | GLES path **REAL + VERIFIED** on the CI emulator (visible-pixel assertions). On a physical phone: **UNVERIFIED**. Vulkan: **MISSING** in effect |
| 9 | Materials and shaders | Region-based `MaterialDef` table; spec overrides for `cloth_roughness`, `cloth_metalness` (new in `42fa78a`), `gear_metalness`; subsurface, sheen, sheen tint, anisotropy, IOR carried per material; 3D4ME-harvested fabric and iris constants | **REAL + VERIFIED** on host — `generation_contract_test` asserts the live material table, the exported bytes and the re-import round trip (96 of 98 checks; see §1.6) |
| 10 | Skeleton and animation | Generated avatar: 71 joints, four joints/vertex weights, 52 ARKit-named blendshapes as analytic displacements, a built-in clip library. Import: this engine's own rig is adopted and the library is bound to it by name. Kotlin side: `animation/` (`AnimationLibrary`, `AnimationPlayback`, `FaceAnimation`, `LocomotionStateMachine`); C++ side `af_anim_player.cpp`, `af_retarget.cpp`, GPU face animation | Own-rig round trip **REAL + VERIFIED** on host (0.2 mm against the generating engine) and on the CI emulator. **A foreign skeleton is shown static, joint count 0, its channels not applied — `afcore.h` documents this and logs `AF_LOG_WARN`: MISSING.** IK, foot-contact/slide detection, motion evaluation, mocap ingestion: **MISSING** |
| 11 | Avatar generation | `af_engine_create` → `af_engine_run_all` stage sequence, 1,536 MB default memory budget, `af_engine_stats`, `af_engine_export_glb`. Prompt entry `af_engine_analyze_prompt` (583-line keyword parser, *"Not a language model"*) | **REAL + VERIFIED** on host and on the CI emulator |
| 12 | Asset import/export | See 6 and 7. Plus `ImportedGlbPreparation.kt` / `GlbTextureBudget.kt` / `AndroidGlbImageScaler.kt`: reads every embedded image's pixel size from its header and, when decoded textures exceed **448 MiB**, rewrites the GLB with those images halved, copying everything else byte for byte. `BoundedGlbReader` spools an import through the cache directory under a 128 MiB ceiling | **REAL + VERIFIED** on host (`BoundedGlbReaderTest`) and by CI |
| 13 | Existing AI and model integrations | `MainActivity.kt` holds a real Ollama client: `GET {base}/api/tags` for model discovery and `POST {base}/api/chat`, default `http://localhost:11434` / `llama3.2:1b`, persisted in SharedPreferences, `OllamaEndpoint.kt` validating the URL and model name. **The response is appended to a chat transcript and goes nowhere else** — it never reaches `af_spec`, the engine, or any 3D path. No OpenAI-compatible, OpenRouter, image-generation or 3D-generation provider. No routing, fallback or scheduling | Ollama chat **IMPLEMENTED IN SOURCE**, and **UNVERIFIED** end-to-end (no test exercises a live model; `OllamaEndpointTest` covers only validation). As an *intelligence subsystem for 3D*: **MISSING**. This is the dead end §3.A replaces |
| 14 | Storage and asset management | No database. SharedPreferences for settings; app-private external files for captures, exposed through a scoped AndroidX FileProvider; two JSON diagnostic reports written to `Downloads/3D Mega Hub Diagnostics`; `EvidenceIdentity.kt` records APK/device/asset identity. **No model is stored in the repository** — `tools/glb_figures/` generates fixtures instead | **REAL + VERIFIED** by CI unit tests |
| 15 | Diagnostics and testing | `RendererTelemetry` carries ≥20 measured fields including fps and frame-interval **p50/p95/p99**, frames presented/not-presented/without-valid-content, stall events, EGL shutdown error. `DeviceResourceGovernor` snapshots RAM, storage and `PowerManager.currentThermalStatus`. Host: 12 `ctest` targets (11 registered + 1 disabled). JVM: 17 unit-test files. Instrumented: `AvatarForgeRendererSmokeTest` (4), `GlbClipPreviewTest` (1), `avatarforge/GeneratedAvatarRuntimeTest` (5). Python: `tools/glb_figures/test_figures_to_clips.py` (24 checks) run by CI | **REAL + VERIFIED** — this is the strongest asset in the repository and the substrate Neural Core must build on |
| 16 | Build configurations | 7 workflows: `android.yml` (7 jobs + 4 evidence jobs), `avatarforge-diagnostic-apk.yml`, `avatarforge-generated-runtime.yml`, `publish-phone-test-apk.yml`, `release-preview.yml`, plus `kotlin-compile-gate.yml` which exists on **`arena/25bd4008` only**, and an emulator-smoke workflow on another branch. Triggers are branch-pattern-scoped: `android.yml` fires on `main`, `work/**`, `fix/**`, `integration/**` — **not** on `arena/**`, so a session branch gets no CI unless a workflow is dispatched or a PR is opened | **REAL + VERIFIED** by observed runs. The trigger gap is a live trap |
| 17 | Branches and uncommitted changes | 9 branches. `main` = `fa9fc36` untouched. Trunk `integration/avatarforge-renderer-port` = `9baf7c67`. Working line `arena/1535c8f6` = `ba38d99`, which contains the entire trunk (merge `88c2c67`). Open PRs: #9 (trunk→main), #12 (superseded, left open), #14, #15 (draft), #16 (`arena/33f12efd`→main, a **second competing path into main**), **#17 (the convergence PR)**. 15 branches were deleted from 22; every tip SHA is recoverable from `docs/BRANCH_CONVERGENCE_PLAN.md`. No private model binary remains in any surviving tree | Current as of this document |
| 18 | Technical limitations | `af_vertex` is **88 bytes** (`afcore.h:318-327`) against a 32-byte target; a generated body is **1,148,034 triangles** where a Daz Genesis 8 body skin is 27,292; textures are **one shared 2048² atlas** against four 4096² maps; sparse accessors **are** read (`af_glb_read.cpp:427-467`) but morph targets are **not read at all** on import while the exporter writes them, so the two directions are asymmetric; no Draco/meshopt/KTX2; foreign skeletons static; Vulkan stale; production signing **BLOCKED**; avatar-quality gates AVQ-00 **BLOCKED** and AVQ-01..08 **NOT RUN**; the generated-avatar runtime gate currently **FAILS** on the trunk and on the merged line (§1.5) | Recorded, not hidden |

### 1.4 Reusable components — do not rebuild these

Neural Core's first architectural duty is to consume what exists. Concretely:

| Existing component | Why Neural Core reuses it instead of writing a new one |
|---|---|
| `af_spec` + the 45-float JNI contract + its `static_assert` | It is already a **validated, versioned, bounded schema** for a human character. This is the target format for every model output. Do not invent a JSON schema for anatomy |
| `af_engine_analyze_prompt` (583-line deterministic parser) | Ground truth and fallback for the gateway. A model's structured output is only acceptable where the deterministic parser is also checked, so a hallucinated field cannot silently reach geometry |
| `Bvh` in `af_mesh.h` — `build(mesh, leafSize=8)`, `raycast(origin, dir, tMax)`, `signedDistance(p, searchRadius)` | Real spatial queries already in the engine. 3D Intelligence's intersection, proximity and inside/outside tests are **already written** |
| `Region` enum (13 semantic regions) in `af_mesh.h` | Semantic part labelling exists. Do not add a parallel part taxonomy |
| `af_anatomy.cpp` → `BodyMetrics` (landmark heights, girths) | Proportion reasoning has a solver. Human Intelligence tunes its inputs and judges its outputs; it does not re-derive anthropometry |
| `af_rig.cpp` — 71 joints, 52 ARKit blendshapes matching `FaceChannels.ARKIT` in `FaceAnimation.kt` | The facial and skeletal vocabulary is fixed and already agreed across C++ and Kotlin. Animation Intelligence emits into it |
| The four host probes: `glb_anatomy_probe.py` (338), `glb_animation_probe.py` (352), `glb_quality_targets_probe.py` (531), `glb_symmetry_sections.py` (190) — 1,411 lines producing structured JSON | **This is 3D Intelligence, already built, offline.** `glb_quality_targets_probe.py` alone reports counts, geometry, per-primitive attributes, skins, joint names and depth, morph targets, per-clip channels/interpolation/durations, material extension union, scene bounds and an `honestyNotes`/`unsupportedMeasurements` list. Mission M2 *promotes* this, it does not rewrite it |
| `RendererTelemetry` (percentiles, stall events, frame validity), `DeviceResourceGovernor` (RAM, storage, `currentThermalStatus`), `EvidenceIdentity`, `DiagnosticDownloads` | The evaluation and resource-governance substrate. The gateway's scheduling and thermal back-off, and the evaluator's performance scores, read these rather than sampling anything new |
| `GlbValidator.kt`, `GlbTextureBudget.kt` | Asset compatibility and texture-budget judgement already exist on-device |
| The 3D4ME constant tables `kFabrics[]` and `kEyeColours[]` in `af_prompt.cpp`, with `generation_contract_test.cpp` as their contract | **This is the prototype of Neural Core's memory.** Curated, measured constants, harvested from a reference, landed as data, guarded by a test that asserts them through the parser, the live material table, the exported bytes and the re-import. Mission M6 generalises exactly this pattern |
| The repo's evidence discipline — status words, exact-head evidence, "never transfer an earlier green result to a current head", "an emulator pass is never a Samsung pass", `not measured` instead of a guess | This *is* a continual-learning safety property. Neural Core inherits it verbatim; a learned improvement that cannot name the commit and device it was measured on is not an improvement |

### 1.5 The two failures that constrain every mission

**(a) The generated-avatar runtime gate fails.** Measured, not inferred. The same workflow dispatched
against three heads:

| Head | Contains this line's work? | Result |
|---|---|---|
| `758bcfb4` (`arena/1a5b3043`) | no | **passed** — run 45779172779 |
| `9baf7c67` (trunk) | is the trunk | **failed** — run 45779175991 |
| `bdaef4b5` and `ba38d996` (merged line) | yes | **failed** — runs 45777317839, 45789457244 |

The trunk fails it alone, so the merged line inherits it. What remains **UNVERIFIED** is whether the new
screen-density LOD selection also affects it: `workflow_dispatch` against a bare SHA is refused
(`422 No ref found`), Actions logs are served from `results-receiver.actions.githubusercontent.com` and
artifacts from `productionresultssa12.blob.core.windows.net`, and neither host is reachable from a
host-only workspace. Historically this gate reports
`GeneratedAvatarRuntimeTest#generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView`: "no drawable
EGL-presented model frame within 15 seconds", `frames=2`, `draws=29`, **`triangles=1626176`**,
`glError=0x0`, `eglError=0x0`. Those 1,626,176 triangles are LOD0 — the full mesh — which is what the LOD
work exists to reduce. **No claim is made that the LOD change fixes this gate.** Settling it is mission M8.

Meanwhile, on the same heads, `android.yml` passed **11 of 11 jobs** including the instrumented GLES render
smoke that fails on PRs #9, #14 and #15.

**(b) The device-verification gap is structural.** The authoring workspace has no JDK, Android SDK, NDK or
KVM. Everything about on-device behaviour — the LOD fix, MP4 encoding, thermal back-off, any real inference
latency — is **UNVERIFIED** until a device or emulator runs it. `docs/ANDROID_TEST_AGENT_PROMPT.md` is the
standing brief for that. Any Neural Core mission that claims a performance or quality number without a
named device is violating `AGENTS.md` §2.1 and §2.2.

### 1.6 The one disabled test, and why

`generation_contract_regression` is registered in CMake and marked `DISABLED`, with both failing check
names in a comment beside it. Run by hand it reports **98 checks, 96 passing**. The two failures assert the
donor branch's deepened two-layer garment normal-map relief (6.5 mm twill taken from 0.10 mm to 0.22 mm,
plus 18 mm raised stitch/panel rows), which was **not** ported: this branch's `af_texture.cpp` diverged the
other way (1,057 lines against the donor's 793) and porting it would change every generated avatar's
pixels. Registering it red would fail the *Native C++ sampling regression* CI job for a feature documented
MISSING here. It is disabled, named, and re-enabled by deleting one line — not deleted, not weakened.

A second item is **not** in the repository at all: `imported_animation_test.cpp` (949 lines, 94 checks)
stays on `arena/089497bd-3d-mega-hub` (`58aced02`) because 85 of its lines call nine `af_engine_imported_*`
functions that do not exist here, and it needs five CC BY 4.0 fixtures (~596 KB) that the donor's own
notices record as packaged into the shipped APK. **That branch must not be deleted until this is decided.**
It is the specification for mission M5.

---

## 2. Architectural principles

Non-negotiable, and each one is a reaction to something this repository has already suffered.

**P1 — The solver stays.** Neural Core proposes `af_spec` values, asset choices, diagnostic verdicts and
animation parameters. It never emits vertices, never bypasses `af_engine_run_all`, and never becomes an
alternative geometry path. Determinism (`same spec + seed ⇒ byte-identical GLB`) is a property the
repository has paid for and tests; a model in the geometry path destroys it.

**P2 — Every model output crosses a validated boundary.** Model text becomes `af_spec` fields only through
a parser that bounds-checks against the same limits the deterministic parser uses, honours the adult-safety
rule (`AGENTS.md` §2.6: every avatar is 30; `bust`/`glute` only at 18+), and records which fields the model
set versus which fell back. An unverifiable field is dropped and logged, never guessed.

**P3 — No invented numbers, machine-enforced.** `AGENTS.md` §2.2 already forbids them. Neural Core extends
it: an evaluation record must carry the commit SHA, the device identity, and the measurement source. A
score with no provenance is rejected at the type level, not by review.

**P4 — Structured diagnostics, never prose.** The brief is explicit. Every intelligence subsystem returns
typed records with named fields and units. The four Python probes and `RendererTelemetry` already model
this; prose summaries are a *rendering* of a record, produced last, and never the thing stored.

**P5 — Provider-independent by interface, and cheap by default.** A new provider is a new implementation of
one interface plus a capability descriptor. No engine code changes. Local inference is the default and the
fallback; remote providers are opt-in, never required for the engine to work, and never a hidden network
dependency in a rendering path.

**P6 — Learning is data a human approved, plus the measurements that justified it.** Not silent weight
updates, not self-modifying prompts. The `kFabrics`/`kEyeColours` pattern: a curated constant table, its
provenance in `THIRD_PARTY_NOTICES.md` or an evidence document, and a contract test asserting it end to
end. This keeps `AGENTS.md` §2.3 (no mock features) and §2.7 (licences) satisfiable by construction.

**P7 — Missions are independently mergeable.** Each mission has its own branch off the trunk, its own gate,
and can be abandoned without stranding another. This is the direct antidote to the double work that
produced four launcher icons, four GLES-diagnostic paths, two bounded GLB readers and two animation
systems.

**P8 — Grep before building.** Before any new subsystem, search for it. The convergence audit found the
same features rebuilt two to four times because sessions branched from stale bases. A mission prompt that
cannot name what it searched for has not started.

**P9 — Free only. No licence fee, no revenue cap, no territory exclusion, no paid API required.**
Stated by the owner on 2026-11-26 and treated as a hard architectural constraint, not a preference. The
engine's default configuration must be obtainable at **zero cost** and must keep working at zero cost.
Concretely: no dependency whose commercial use requires payment; none with a revenue threshold (a cap is
not free); none with a territory exclusion; none that is non-commercial-only; and no *paid* remote provider
that the engine needs in order to function. Paid providers may exist as an **opt-in the user configures and
pays for themselves**, disclosed as such, never bundled, never required, never a hidden network dependency.
"Open weights" is not evidence of free — see §6, where several popular models are free to download and
still not free to ship. Every licence claim in this document is UNVERIFIED until its own licence file has
been read and recorded in `THIRD_PARTY_NOTICES.md`.


---

## 3. Neural Core architecture

### 3.0 Placement

```
avatarforge-engine/                       :app  (thin consumer, no intelligence logic)
  src/main/cpp/
    include/afcore.h                      existing C ABI — append only
    include/afneural.h                    NEW  C ABI for intelligence: capability, request, verdict
    src/neural/                           NEW  provider-free core: schema, validation, evaluation
      af_neural_gateway.cpp               provider routing, fallback, scheduling, budgets
      af_neural_schema.cpp                af_spec-bound structured output validation (P2)
      af_neural_eval.cpp                  mesh/asset/material metrics reusing Bvh + Region
      af_neural_memory.cpp                approved-improvement store (P6)
    src/af_*.cpp, src/render/             UNCHANGED — the solver and the renderer
  src/main/java/.../avatarforge/
    neural/                               NEW  Kotlin surface
      NeuralGateway.kt                    interface + Ollama/OpenAI-compatible/OpenRouter providers
      NeuralCapability.kt                 model discovery, capability descriptors
      NeuralRequest.kt / NeuralVerdict.kt typed request/result, never prose
      InferenceScheduler.kt               concurrency, timeouts, thermal back-off
      NeuralMemory.kt                     approved-improvement persistence + provenance
      ModelOutputValidator.kt             the P2 boundary, mirroring af_neural_schema.cpp
  src/test/cpp/                           NEW host tests per mission
app/src/main/java/.../MainActivity.kt     CHANGED: the Ollama chat transcript is demoted to a
                                          diagnostic console that *calls* the gateway; it stops
                                          being the only consumer of inference
```

Rationale: the intelligence core is C++ beside the solver so it can read meshes, `Bvh`, `Region` and
`af_spec` without serialising them across JNI; the providers are Kotlin because that is where HTTP,
SharedPreferences, `PowerManager` thermal state and connectivity already live. The C ABI stays the only
contract the app depends on.

### 3.A Neural Gateway

**Purpose.** One provider-independent intelligence interface, so a new provider never touches the engine.

**What exists to build on.** A working Ollama client (`/api/tags` discovery, `/api/chat`), URL and
model-name validation, SharedPreferences persistence, `DeviceResourceGovernor` for RAM/storage/thermal,
and `RuntimeCapability` / `RendererBackendFactory` as precedents for capability-gated selection.

**Design.**

```kotlin
interface NeuralProvider {
    val id: String                                   // "ollama", "openai-compatible", "openrouter"
    suspend fun capabilities(): NeuralCapability     // models, modalities, context, structured-output?
    suspend fun complete(req: NeuralRequest): NeuralResult
    fun cost(req: NeuralRequest): InferenceBudget    // tokens/latency/thermal estimate
}

data class NeuralCapability(
    val models: List<ModelDescriptor>,               // id, family, modalities, context window
    val supportsStructuredOutput: Boolean,           // JSON schema / grammar constraint
    val supportsVision: Boolean,
    val local: Boolean,                              // true => no network, no egress
    val measured: MeasuredAt                         // commit + device + timestamp, per P3
)
```

- **Routing.** A `NeuralRequest` declares a *task kind* (`SPEC_FROM_PROMPT`, `ASSET_DIAGNOSIS`,
  `ANIMATION_CRITIQUE`, `TEXTURE_CRITIQUE`, `MOTION_EVALUATION`) and a required capability. The router
  picks the cheapest provider satisfying it, preferring local. Routing decisions are recorded, because a
  result must be attributable to the model that produced it.
- **Structured output.** Where a provider supports grammar/JSON-schema constraint, the `af_spec`-derived
  schema is passed to it. Where it does not, output is parsed and then validated identically — the
  validator, not the provider, is the guarantee.
- **Fallback.** Ordered chain per task kind: local Ollama → configured OpenAI-compatible endpoint →
  OpenRouter → **the deterministic `af_engine_analyze_prompt`**. The last is not an error state; it is the
  floor. The engine must remain fully functional with zero providers configured (**P5**), so a gateway
  failure degrades to today's behaviour rather than blocking generation.
- **Scheduling and resources.** One in-flight request per task kind by default; timeouts; retry with
  backoff; and thermal back-off driven by `DeviceResourceGovernor.snapshot().thermalStatus`, reusing the
  same governor that already gates generation. No inference on the render thread, ever.
- **Secrets.** Remote keys in the existing SharedPreferences-backed settings surface, never in the
  repository, never logged, and never inside a diagnostic report that gets shared.
- **Demoting the chatbot.** `MainActivity`'s AI page keeps its transcript but becomes a *console over the
  gateway*: it shows the task kind, the provider chosen, the validated structured result, and which fields
  reached `af_spec`. That converts a dead end into an inspection tool without adding a feature nobody asked
  for.

#### 3.A.1 Image-generation and specialist 3D-generation providers

These are **not** a variant of text completion and must not share its interface. Three differences drive
the design:

1. They return **binary artifacts**, not parseable text. So the result type is a file plus a receipt
   (bytes, SHA-256, declared licence, provider, model revision), and validation is not parsing — it is
   running **M2's `af_asset_inspect`** over the artifact. A generated mesh enters the engine through the
   *same* compatibility verdicts as a user-imported one. That is what keeps **P1** true: a 3D-generation
   model produces an *asset to inspect*, never a replacement for `af_engine_run_all`.
2. They are **slow and storage-bound** (tens of seconds to minutes, megabytes out), so they are queued and
   cancellable, never synchronous with a UI gesture.
3. **They cannot run on a phone.** Every candidate in §6 needs a CUDA-class GPU and gigabytes of VRAM,
   against this project's `minSdk 26` and a 1,536 MB engine memory budget. So for these providers
   "local" means *the owner's workstation on the LAN*, not on-device. Text providers can be genuinely
   on-device-adjacent (Ollama on localhost); artifact providers cannot. Claiming otherwise would be an
   invented capability, which `AGENTS.md` §2.2 forbids.

```kotlin
interface ArtifactProvider {                       // distinct from NeuralProvider
    val id: String                                 // "tripo", "hunyuan3d", "image-gen-x"
    suspend fun capabilities(): ArtifactCapability // modalities, output formats, licence, VRAM, latency
    suspend fun generate(req: ArtifactRequest): ArtifactReceipt   // queued, cancellable
}
data class ArtifactReceipt(
    val bytes: ByteArray, val sha256: String,
    val declaredLicence: String,                   // recorded, and checked against §6 before use
    val provider: String, val modelRevision: String,
    val inspection: AssetReport?                   // M2's verdict; null => not yet inspected
)
```

Two concrete uses, both of which feed the learning loop rather than bypassing it:

- **Image generation as a curation aid.** Generate candidate skin, fabric, iris or hair *reference*
  imagery, measure it with M3, have a human approve it, and land the result as a constant table — exactly
  the `kFabrics`/`kEyeColours` pattern (M6, **P6**). The image is a reference for a human, not a texture
  pasted onto an avatar; that keeps the deterministic pipeline intact and avoids shipping a model's output
  whose licence is unverified.
- **3D generation as an import source, gated.** A generated mesh is imported through `af_glb_read` and
  judged by `af_asset_inspect`. Expect `AF_ASSET_FOREIGN_SKELETON_STATIC` for anything not built on this
  engine's 71-joint rig — which is precisely the capability **M5** exists to add, and why 3D generation is
  *sequenced after* M5 rather than in parallel with it. Importing generated humans before M5 lands would
  produce static statues and be reported, honestly, as broken.

**Free-only provider shortlist** (**P9**; all licences UNVERIFIED until read — see §6.3):

| Modality | Free candidates | Licence as reported |
|---|---|---|
| Text / structured reasoning | **Ollama** local server (already integrated), llama.cpp-class local runtimes | free, local, no egress |
| Image generation | **FLUX.1 [schnell]**, **Qwen-Image** | Apache 2.0 — both VERIFIED from their own repositories (`Chroma1` and `HiDream-O1` could not be located, so are UNVERIFIED and not offered) |
| Image → 3D mesh | **TripoSR** (MIT covering code *and* pretrained weights, VERIFIED from its README), **TRELLIS** (MIT, VERIFIED), **InstantMesh** (Apache-2.0, VERIFIED) | Free; all three need a workstation GPU |
| Body reference model | **MHR** (Momentum Human Rig) and **Anny** | Apache 2.0 both — verified from each project's own licence text (§6.3.2) |

Explicitly **excluded** by P9, each verified from its own licence file in §6.3.3: every **FLUX.1 [dev]**
variant including Fill, Canny, Depth, Redux, Kontext and Krea (*"FLUX.1-dev Non-Commercial License"*, listed
variant by variant in the repository's own table); **Hunyuan3D 2.1** (Community Licence whose third line
reads *"DOES NOT APPLY IN THE EUROPEAN UNION, UNITED KINGDOM AND SOUTH KOREA"*); **SD 3.5 / SDXL** (MIT code,
weights licence absent from the repository, a revenue cap reported but unverified); the **SMPL** family;
**HumanML3D / AMASS / MDM / MotionGPT**; **contact-human-dynamics** (Adobe Research Licence); and anything
reached through a paid API.

Paid remote providers (OpenAI, OpenRouter and other OpenAI-compatible hosts) stay in the interface because
the brief asks for them, but under P9 they are **opt-in, user-funded, and never load-bearing**: the engine
must generate, inspect, evaluate and improve with zero providers configured and zero spend, falling back to
the deterministic `af_engine_analyze_prompt`. A user who configures a paid key gets more; nobody is required
to.

**Capability discovery** is a first-class operation, not an assumption: `NeuralCapability` and
`ArtifactCapability` are *queried* (Ollama's `/api/tags` already does this) and cached with the commit and
device they were measured on (**P3**), because a provider's model list changes underneath you.

**Status today.** Local Ollama chat **IMPLEMENTED IN SOURCE, UNVERIFIED end-to-end**; routing, fallback,
scheduling, image generation, 3D generation and capability caching: **MISSING**.

### 3.B 3D Intelligence

**Purpose.** Structured diagnostics of meshes and assets — not descriptions of them.

**What exists.** 1,411 lines of Python probes already emit exactly this, plus `GlbValidator`,
`GlbTextureBudget`, `af_glb_read`, `Bvh`, `Region`, and `RendererTelemetry`.

**Design.** Promote the probes' *vocabulary* into the engine as a first-class API, so diagnostics run
on-device and in CI without a Python checkout:

```c
/* afneural.h — append-only, like afcore.h */
typedef struct af_asset_report {
    int32_t struct_size;
    int32_t gltf_version_ok, meshes, primitives, vertices, triangles;
    int32_t skins, joints, max_joint_depth, animations, morph_slots;
    int32_t uv_sets, has_normals, has_tangents, has_vertex_colors, skinning_attrs;
    float   bounds_min[3], bounds_max[3], estimated_height;
    int32_t material_count, mr_texture_count, extension_union_count;
    float   symmetry_max_deviation;        /* from glb_symmetry_sections' method */
    int32_t unsupported_count;             /* the probes' honestyNotes, as codes */
} af_asset_report;
int32_t af_asset_inspect(const uint8_t* bytes, size_t size, af_asset_report* out);
```

- **Topology and geometry**: counts, per-primitive attributes, index/vertex ratios, degenerate and
  duplicate-face detection via `Bvh`.
- **Normals/tangents/UVs**: presence, consistency, seam and chart statistics — reusing `af_uv`'s own
  chart machinery rather than reimplementing it.
- **Materials/textures**: extension union, metallic-roughness presence, texture resolution against the
  448 MiB decoded budget `GlbTextureBudget` already enforces.
- **Morph targets**: slot and name inventory. Note the standing gap, **corrected by direct inspection on
  2026-11-26**: sparse accessors *are* read (`af_glb_read.cpp:427-467` applies a sparse patch over a dense
  buffer), but the importer contains **zero occurrences of `morph`** — they are not read at all, while the
  *exporter* does write them (`afcore.h:390`). The report must therefore say
  `AF_MORPH_NOT_READ_ON_IMPORT`, never `supported` and never `parsed_but_not_applied`.
- **Skeleton/skin weights**: joint count, names, depth, inverse-bind presence, influences per vertex,
  weight-sum deviation, and zero-weight or orphan-joint detection.
- **Clips**: per-clip channels, interpolation kinds, durations, driven nodes, morph-weight channels —
  exactly what `glb_animation_probe.py` reports today.
- **Compatibility verdict**: a code, not a sentence — `AF_ASSET_OK`, `AF_ASSET_FOREIGN_SKELETON_STATIC`,
  `AF_ASSET_TEXTURE_OVER_BUDGET`, `AF_ASSET_EXTENSION_UNSUPPORTED`, `AF_ASSET_NO_ANIMATION`. These map to
  real, documented engine behaviour (§1.3 row 10), so the verdict is checkable against what the renderer
  then does.

**Rule.** Every field has a unit and a source. Where a measurement is not implemented, the field reports
`not measured` — mirroring the probes' existing `unsupportedMeasurements` list, which is the honest
pattern to keep.

**Status today.** Offline, host-side **REAL + VERIFIED** (the probes ran against
`app/src/androidTest/assets/rigged_test_figure.glb` and reported glTF 2.0, 240 vertices, 120 triangles,
1 skin / 7 joints / depth 3, clips Wave 2.000 s, March 1.000 s, Bow 2.000 s, bounds 0.62 × 1.72 × 0.31,
plus a 13,316-byte JSON report). In-engine, on-device: **MISSING**.

### 3.C Human Intelligence

**Purpose.** Understand and improve human characters: face, proportions, skin, hair, hands, feet, clothing,
anatomical consistency, expression, deformation.

**What exists.** `af_anatomy` (anthropometric solver: spec → landmark heights and girths, meeting a tape
measurement exactly), `af_field` (one SDF for body/head/face/hands/feet/hair/clothes/shoes/gear),
`af_rig` (52 ARKit blendshapes as analytic displacements), `af_texture` (skin with subsurface, sheen,
anisotropy), `docs/AVATAR_GEOMETRY_EVIDENCE.md`, `docs/AVATAR_QUALITY_PLAN.md`,
`docs/PHOTOREAL_UPGRADE_PLAN.md`, `docs/DONOR_BASELINE.md`, `docs/OWNER_MODELS_2026-11-15.md`, and the
3D4ME reference harvest.

**Roadmap, in the order the evidence supports.** The governing finding is already in
`docs/PHOTOREAL_UPGRADE_PLAN.md`: the engine computes the human purely from formulas, so realism comes from
*content*, not from a new solver. A generated body is 1,148,034 triangles where a Daz Genesis 8 body skin
is 27,292, and textures are one shared 2048² atlas where the reference uses four 4096² maps.

1. **Measure before changing.** Land the evaluation harness (M3) so every anatomical claim has a number
   and a baseline. Symmetry tolerance is already fixed at **0.004 model units**.
2. **Proportions.** Treat `BodyMetrics` as the observable. Build a proportion critic that reports landmark
   heights and girth ratios against a stated reference population, with the reference named and licensed.
   Do not silently retarget the solver.
3. **Face.** The 52 ARKit channels are the fixed vocabulary (`FaceChannels.ARKIT` in `FaceAnimation.kt`
   must match `kBlendshapeNames` in `af_rig.cpp`). Facial work means better analytic displacements and
   better neutral topology — plus closing the gap that morph targets are not applied on import, without
   which no scanned face can be driven.
4. **Skin.** Already carries subsurface, sheen, sheen tint, anisotropy and IOR. The lever is texture
   resolution and separate maps, i.e. `PHOTOREAL_UPGRADE_PLAN` R7 (prefiltered environment cubemap plus one
   LUT — named there as the biggest realism lever) and R8 (ASTC/KTX2 plus meshopt).
5. **Hair.** Currently a branch of the SDF (`hairShape`). Hair *cards* are the documented route; that is a
   geometry-pipeline change and must be its own mission with its own gate.
6. **Hands and feet.** Present in the SDF. Critic first: joint count, digit segmentation, nail and palm
   crease detail are all measurable before they are improved.
7. **Clothing.** The fabric presets landed in `42fa78a` are the template — measured constants, a contract
   test, provenance recorded. Garment *geometry* (the deepened normal-map relief, `upperShell`,
   `skirtShell`) is the deferred reconciliation from §1.6.
8. **Datasets and licences.** See §3.C.1 and §6 — this is now the highest-leverage open decision in the
   whole plan, because one widely-assumed default (the SMPL family) is **non-commercial** and would
   silently contaminate a shipping APK.
9. **Adults-only invariant.** Every avatar is 30, unit-tested; `bust`/`glute` honoured only at 18+. Any
   learned or model-proposed anatomical parameter passes through that check, which already exists as a
   host test and must not be relaxed to make a metric improve.

#### 3.C.1 Candidate models, datasets and techniques — researched 2026-11-26

Researched externally; **every licence and figure below is UNVERIFIED against its primary source** and
must be confirmed on the model's own repository or model card before any mission depends on it
(`AGENTS.md` §2.7). Sources disagree in places, and the disagreements are recorded rather than resolved by
picking a favourite.

**Headline finding — `Anny` is an unusually good fit for this engine.** NAVER LABS Europe released
[`naver/anny`](https://github.com/naver/anny) (paper arXiv 2511.03589, announced 2025-11-06) under
**Apache 2.0**: a differentiable, **scan-free** parametric human body model built on MakeHuman community
assets and calibrated with **WHO population statistics**. Why it matters here specifically:

| Anny | This engine already | Consequence |
|---|---|---|
| Interpretable *phenotype* parameters in continuous `[0,1]` — age, gender, height, weight, muscle, and local traits such as head fat or foot width | Has `af_spec` as 45 bounded floats, and `af_anatomy` mapping spec → landmark heights and girths | **Same design philosophy.** No learned latent space to explain, no opaque embedding — so a model-proposed parameter is inspectable and unit-testable, satisfying **P2** and **P6** by construction |
| Scan-free, built from artist/authored anthropometric priors | Computes the human purely from formulas; `docs/PHOTOREAL_UPGRADE_PLAN.md` already concludes *"keep the machine, change the content"* and recommends MakeHuman-family assets | The existing plan's recommendation now has a maintained, Apache-2.0, WHO-calibrated realisation instead of a bare asset licence |
| Blendshape-driven shape space on a MakeHuman base mesh (**>13,000 vertices**), with **two rigs** verified from the README: the default `anny` rig at **104 bones** (facial-expression, eye and tongue bones pruned; facial actions available separately via `facial_actions="all"`) and `rig="makehuman"` exposing the full **163-bone** MakeHuman rig | Has 52 ARKit blendshapes as analytic displacements and a 71-joint rig | A **rig and topology mapping** problem, not a rewrite — and Anny's paper states it *supports alternative mesh topologies and rigs, allowing simple interoperability and character animation re-targeting*, which is directly relevant to **M5** |
| Reported **millimetre-accurate** scan fitting | Tests an animation round trip at **0.2 mm** and symmetry at **0.004 model units** | Compatible with this repository's measurement culture; both sides speak in millimetres |
| Apache 2.0, `pip install anny`, **no registration or gated download** | Ships an APK; `AGENTS.md` §2.7 requires provenance before use | Commercially usable and attributable without negotiating a licence |

Two cautions, stated rather than smoothed over:
- Anny is **Python/PyTorch**. It cannot run inside the C++ engine on `minSdk 26`. Its role is
  **authoring-time and CI-time** — like the four Python probes, it informs constants and evaluation
  references; it is not a runtime dependency. Any claim that it runs on-device would be false.
- Two sources disagree on its blendshape count (**564** artist-defined blendshapes vs **1,136**
  prototypical mesh variations), and on the size of its `Anny-One` synthetic dataset (**800k** vs **780k**
  images, released February 2026). Both are recorded as **UNVERIFIED**; read the paper and the repository.
- Anny's authors **removed sensitive anatomical detail** (nipples, genitals) from the MakeHuman base and
  enforced left/right skinning symmetry. That intersects `AGENTS.md` §2.6 (adults-only, and the
  `bust`/`glute` restrictions) and must be reconciled deliberately by the owner, not inherited silently.

**Licence position — see §6.3, verified from primary sources.** In summary: the SMPL family (SMPL, SMPL-X,
FLAME, MANO, SMPL+H, DMPL) is non-commercial and **excluded**; the trap is that permissively-licensed code
can still require restricted model files and restricted training data, and **`EricGuo5513/HumanML3D` is the
proved example** — an MIT repository whose own README says the data comes from AMASS and may not be
redistributed, and instructs the reader to download the non-commercial SMPL+H and DMPL body models.
MakeHuman's **assets** are **CC0 1.0 Universal** (its code is AGPL — assets only, never code), which is what
makes the Anny route and the "engine's own output is free data" argument in §3.F.6 licence-clean.

**Techniques worth adopting, independent of any model choice:**

| Technique | What it is | Where it lands |
|---|---|---|
| Phenotype-parameterised shape space | Interpretable `[0,1]` controls over authored blendshapes instead of learned latent PCs | M4 proportion critic; the natural target vocabulary for a model proposing `af_spec` changes |
| Anthropometric calibration against WHO growth standards | Mapping an `age` parameter to chronological years, and height/weight to the mean and SD of population growth charts | M4: gives the proportion critic a **named, citable reference population** instead of an invented ideal (**P3**, §2.2) |
| Canonical **T-pose normalisation** before computing a retarget mapping matrix | Normalise both source and target rigs to T-pose first, so rest-pose offsets cannot corrupt the mapping | M5 retargeting — a concrete rule that prevents a whole class of silent failure |
| Per-channel smoothing after pose lifting (EMA or one-euro filter) | Jerky raw estimates break physics-based cloth and IK | M5, before any IK pass |
| Foot-contact representation as a per-frame **F×4 binary array** — left heel, left toe, right heel, right toe | A four-channel labelling convention observed in published work | M5 foot-contact detection. **Caution:** `davrempe/contact-human-dynamics` was checked and is under the **ADOBE RESEARCH LICENSE, non-commercial only** — vendor nothing from it (§6.3.3). The *convention* of four binary channels is a trivial data shape, not copyrightable expression, so M5 reimplements it independently from this engine's own `BodyMetrics` landmarks and skin matrices |
| **Foot-skating metric**: mean foot velocity during contact frames, with IK correction applied when it exceeds ~1 cm/frame | Standard external practice | M5 — a candidate threshold; the repository must measure its own baseline and may not adopt 1 cm as policy without evidence (**P3**) |
| Motion-quality regression checks: BVH round-trip joint positions within 1e-4, no NaN transforms, FID not regressing >5% against baseline | Validation suite used by motion-foundation-model work | M3/M5 exit gates |
| Motion foundation models (MotionGPT, MDM named as pretrained checkpoints) | Text/seed-conditioned motion generation | **Deferred, see §3.D** — and note that most published motion models emit SMPL-family parameters, which re-enters the licence trap above |

**Status today.** Generation **REAL + VERIFIED** on host and CI emulator. Photorealism **MISSING**, with
the plan and the measurements already written down. Facial animation on an *imported* model **MISSING**.
No external body model is integrated in any form: **MISSING**, and now with a researched, licence-clear
candidate (`Anny`) and a researched trap to avoid (SMPL family).

### 3.D Animation Intelligence

**Purpose.** Skeleton analysis, retargeting, mocap processing, animation generation, blending, facial
animation, IK, motion evaluation, foot contact and slide detection, deformation.

**What exists.** `af_rig` (71 joints, clip library), `af_anim_player.cpp`, `af_retarget.cpp`,
`af_engine_evaluate_animation` (writes one column-major 4×4 skin matrix per joint; the renderer blends
bind-pose vertices on the GPU), Kotlin `animation/` (`AnimationLibrary`, `AnimationPlayback`,
`FaceAnimation`, `LocomotionStateMachine`), GPU face animation, and `animation_roundtrip_test` comparing
imported playback against the generating engine to **0.2 mm**.

**Gaps, in priority order.**

1. **Third-party skeleton adoption — MISSING, and already specified.** `afcore.h` documents that a
   skeleton which is not this engine's rig is shown static with joint count 0 and its channels not applied.
   `imported_animation_test.cpp` on `arena/089497bd` is a 94-check specification of the opposite behaviour,
   and the donor's nine `af_engine_imported_*` functions are one design of it. This line already has a
   *different, richer* API — `af_engine_animation_count/name/duration/info`, `af_engine_import_motions`,
   `af_engine_import_motions_from_memory` with an `af_motion_import_report`. **Mission M5 must decide
   between the two APIs, not implement both** — implementing both is precisely the duplication that produced
   two animation systems already.
2. **Retargeting evaluation.** `af_retarget.cpp` exists; what is missing is a measured verdict on
   retarget quality (joint-angle error, end-effector drift, penetration).
3. **IK — MISSING.** No solver. Needed for foot placement, hand contact, look-at.
4. **Foot contact and slide detection — MISSING.** Fully computable on the host from existing data:
   skin the rest mesh with `af_engine_evaluate_animation`, track ankle/toe landmarks from `BodyMetrics`,
   and report contact intervals plus slide distance in millimetres. This is a high-value, low-risk first
   mission because it needs no new engine capability — only `Bvh`, landmarks and skin matrices.
5. **Blending.** `LocomotionStateMachine` exists in Kotlin; cross-fade quality is unmeasured.
6. **Facial animation.** 52 ARKit channels are wired, but driving an *imported* face needs morph targets
   applied on import — currently **MISSING**, and now **ASSIGNED TO M5** as deliverable (f) by owner
   instruction 6 of 2026-11-26. Facial work on imports is gated on that deliverable, not on new blendshape
   math.
7. **Mocap ingestion — MISSING.** The BVH/FBX question is a licence and dependency question first: the
   engine's only third-party C++ dependency is zlib, and `AGENTS.md` §2.7 plus the no-Draco/meshopt stance
   mean any new decoder is an owner decision. Grounded practice from §3.C.1 applies if it proceeds:
   canonical T-pose normalisation before computing the mapping matrix, per-channel smoothing (EMA or
   one-euro) after pose lifting, and a BVH round-trip check at 1e-4. Reference implementations exist
   (`toyxyz/RGB MoCap` does contact correction, Mixamo FBX/VRM/PMX retargeting and FBX/BVH export under an
   MIT app licence with models under their own terms) but are **Windows + NVIDIA, ~41 GiB** — useful as a
   design reference, not a dependency for an Android engine.
8. **Animation generation — MISSING, and constrained to non-learned methods by P9.** This is the one place
   where the free-only rule changes the design rather than merely the shopping list. The practical route to
   learned motion generation runs through **MotionGPT** and **MDM**, and both are trained on
   **HumanML3D / AMASS** — and that data is **not free**:
   - **AMASS** is released for non-commercial scientific research, education and artistic projects only
     (Motion-X documents it as academic-research-only; another source records CC BY-NC-SA). Commercial use
     needs separate rights *and* may depend on the underlying source datasets.
   - **HumanML3D** is frequently labelled **MIT** — 14,616 motions, 44,970 descriptions, a 263-dimensional
     per-frame representation including foot-contact labels. **That label is a trap.** It derives its motion
     from AMASS, and the repositories distributing processed versions state plainly that the data is for
     academic purposes and that the AMASS licence governs. Permissive annotations over restricted motion do
     not make the motion permissive.
   - Weights trained on restricted data inherit the restriction, so a "free" checkpoint is not free to ship.

   Therefore animation generation here means **procedural and measured**: composing, blending, timing and
   refining motion from the engine's *own* clip library (`af_rig.cpp`, already REAL + VERIFIED), driven by
   the critics in §3.E — not sampling from a learned motion model. If the owner ever wants learned motion
   generation, the free path is a dataset with a genuinely permissive licence (**AIST++** is recorded as
   CC BY 4.0, a licence class this repository already accepts for its Khronos fixtures; CMU mocap is
   commonly cited as free — both **UNVERIFIED**, check before use), and even then the model weights must be
   checked for what they were trained on. Until that is done, this item is **BLOCKED on licence**, not
   merely deferred, and no mission may work around it.
9. **Deformation** *(the brief's section D ended mid-word here — see the truncation note)*. Treated in this
   plan as a **measurement** problem, not a new solver: skin-matrix sanity, weight-sum deviation,
   rest-pose identity and blendshape displacement magnitude, all in §3.E, computed from
   `af_engine_evaluate_animation` and `Bvh::signedDistance` which already exist. Deformation *quality*
   improvement follows from M4's critics and M6's approved constants. If the brief's own deformation
   requirements are broader — corrective blendshapes, cloth or muscle simulation, penetration resolution —
   they are **not designed here** and need re-issuing.

### 3.E Evaluation Intelligence

Confirmed by the owner 2026-11-26 (previously inferred from section 1's objective bullets; now authoritative).

**Purpose.** Let the engine judge its own output, with provenance, so that "better" is a measurement and not
an opinion. Every other intelligence subsystem depends on this one: a critic that cannot measure cannot
improve, and a learning loop that cannot detect regression will silently make things worse.

#### 3.E.1 The evaluation record

```kotlin
data class EvaluationRecord(
    val subject: SubjectIdentity,      // spec hash + seed, or asset SHA-256
    val commit: String,                // exact head that produced it — AGENTS.md §2.1
    val device: DeviceIdentity,        // model, Android version, GPU; or "host" with OS and compiler
    val metrics: Map<MetricKey, Measurement>,
    val verdicts: List<Verdict>,       // typed codes, never prose
    val provenance: Provenance         // which tool measured it, its version, when
)
data class Measurement(
    val value: Double, val unit: Unit,
    val method: String,                // e.g. "Bvh::signedDistance", "RendererTelemetry.fps"
    val source: SourceKind             // HOST_CTEST | CI_EMULATOR | DEVICE | PROBE | MANUAL
)
```

An `EvaluationRecord` missing `commit` or `device` **must fail to construct**. That is the machine-enforced
form of `AGENTS.md` §2.1–§2.2 (**P3**): a result cannot be separated from the commit and hardware that
produced it, so it cannot be carried forward to a different head or claimed for a phone it never ran on.

#### 3.E.2 Metric families, each sourced from something that already exists

| Family | Metrics | Existing source — do not re-implement |
|---|---|---|
| Geometry | triangle and vertex counts, per-region distribution, symmetry deviation, degenerate/duplicate faces, bounds, estimated height | `Region` enum and `Bvh` in `af_mesh.h`; `glb_anatomy_probe.py`; `glb_symmetry_sections.py` |
| Topology / UV | chart count, seam length, atlas occupancy, UV-set presence | `af_uv.cpp`; probe attribute reporting |
| Material / texture | decoded texture bytes vs the 448 MiB budget, extension union, factor overrides surviving export → re-import | `GlbTextureBudget.kt`, `GlbValidator.kt`, `generation_contract_test.cpp` Part E (already proves this round trip for fabrics) |
| Deformation | skin-matrix sanity, weight-sum deviation, rest-pose identity, blendshape displacement magnitude, imported-playback drift | `af_engine_evaluate_animation`; `animation_roundtrip_test.cpp` already compares to the generating engine at **0.2 mm** |
| Render performance | fps, frame-interval p50/p95/p99, frames presented / not presented / without valid content, stall events, drawn triangles, selected LOD level | `RendererTelemetry.kt`; the `Avatar LOD selected:` and `Avatar LOD ready:` log lines added in `07610b5` |
| Render correctness | visible pixels after background subtraction, GL/EGL error counts, magenta-debug-shader detection | the three instrumented suites, which already assert a ≥5,000 visible-pixel floor |
| Resource state | RAM, free storage, `PowerManager.currentThermalStatus` | `DeviceResourceGovernor.kt` |
| Identity / provenance | APK, device, asset identity | `EvidenceIdentity.kt` |

The point of the right-hand column is **P8**: evaluation is mostly an *aggregation* problem here, not a
measurement problem. The repository already measures nearly everything; what is missing is one record type
with provenance.

#### 3.E.3 When evaluation runs

| Trigger | Scope | Cost |
|---|---|---|
| Every generation | geometry + material families, host-side | milliseconds; runs on-device |
| Every export/import round trip | deformation + material-override families | already incurred by existing tests |
| Every rendered frame batch | performance + correctness families | already collected by `RendererTelemetry` |
| CI, per commit | all families that are host-computable, plus the emulator suites | existing workflows |
| On demand, device | full suite including thermal | `docs/ANDROID_TEST_AGENT_PROMPT.md` |

#### 3.E.4 Verdicts are codes

`AF_EVAL_PASS`, `AF_EVAL_REGRESSION`, `AF_EVAL_IMPROVED`, `AF_EVAL_UNMEASURED`, `AF_EVAL_INCOMPARABLE`
(different commit or device, so no claim may be made). Prose is a *rendering* of a record, produced last for
a human, and never stored in place of one (**P4**).

#### 3.E.5 Hard rules

1. A metric not taken is `not measured` — never estimated, never borrowed from a similar run.
2. A host metric is never presented as a device metric, and an emulator pass is never a phone pass.
3. A record never transfers to another commit. A documents-only commit still invalidates the evidence above
   it, which is why this repository keeps exact-head records in pull requests rather than in the commit.
4. Comparing two records requires the same `subject`, and reports `AF_EVAL_INCOMPARABLE` if `commit` or
   `device` differ in a way that affects the metric family being compared.

**Status today.** The individual measurements are **REAL + VERIFIED** but scattered across host tests, CI
instrumented suites, telemetry and four Python probes. A unified record with provenance: **MISSING**.

### 3.F Learning, Memory and Continual Improvement

Confirmed by the owner 2026-11-26 (previously inferred; now authoritative, and expanded to the eight
components the owner specified).

**Purpose.** Remember what worked, remember what was rejected and why, and improve future outputs — without
becoming opaque, unauditable, or dependent on anything paid (**P6**, **P9**).

**Governing insight.** The improvement loop's output is always **data plus a test**, reviewed by a human,
landed as a pull request. Never a mutated binary, never a self-modifying prompt, never a silent weight
update. The working prototype already exists in this repository: `kFabrics[]` and `kEyeColours[]` in
`af_prompt.cpp`, harvested from a reference, landed in `42fa78a`, with provenance in
`THIRD_PARTY_NOTICES.md` and a contract test asserting them through parser → live material table → exported
GLB bytes → re-import. M6 generalises that exact pattern; it does not invent a new one.

#### 3.F.1 Experience memory

Every generation, evaluation and diagnosis produces an **experience**: `(subject, action, context, record,
outcome)`. Stored append-only, content-addressed by the `subject` hash, in app-private storage on device and
as exported JSON off device. Experiences are **evidence, not policy** — nothing reads them to change
behaviour automatically. They exist so a human or a critic can answer "what happened last time we tried
this?"

Bounded by design: a ring buffer with a configurable cap, and eviction that keeps every experience whose
outcome was `AF_EVAL_REGRESSION` or an explicit human decision, because failures and decisions are the
informative ones.

#### 3.F.2 Approved feedback

A human decision attached to an experience: **APPROVED**, **REJECTED**, or **NEEDS_WORK**, with a reason and
the measurements that were visible at the time. Approval is the only thing that promotes an experience into
a candidate improvement.

Rejections are stored with the same weight as approvals. A memory that records only successes will
re-propose the same failed idea to a later session — which is precisely the failure mode that produced four
launcher icons and two animation systems in this repository.

#### 3.F.3 Experiment history

One row per experiment: hypothesis, the change made, the `EvaluationRecord` before and after, the verdict,
the decision, and the commit it landed on (or `not landed`). This is a **decision log**, and it is the
mechanism that makes "progressively improving" auditable rather than asserted.

It also gives a later agent the answer to "has this been tried?" — the question **P8** exists to force.

#### 3.F.4 Quality measurements

§3.E's records, retained as **golden records**: an accepted baseline per subject, so regression is detected
by re-measuring and comparing rather than by re-judging. A golden record is promoted only by human approval
(3.F.2) and is immutable once promoted; superseding it creates a new one and records the reason.

#### 3.F.5 Regression detection

On every generation and every CI run, re-measure the golden subjects and compare. Rules:

- Any metric moving beyond its stated tolerance against the golden record ⇒ `AF_EVAL_REGRESSION`, with the
  metric, both values, the tolerance and the two commits named.
- Tolerances are declared per metric with their unit and their source. Existing precedents in this
  repository: symmetry **0.004 model units**, animation round trip **0.2 mm**, visible pixels **≥5,000**.
- **Byte-identity is the strongest available check and is already in use**: exporting the same spec and seed
  produced `male=73,399,512 B` and `female=72,076,316 B` identically before and after the `42fa78a` ports,
  which is what proved those ports caused no regression. Where byte identity holds, it is preferred over
  tolerances.
- Detection never auto-reverts. It reports, and a human decides — because a legitimate improvement is also a
  change in output.

#### 3.F.6 Dataset management

Datasets are the highest licence-risk item in the whole initiative (§6). Rules:

- **No external dataset in the repository or the APK.** Rule 5 of `AGENTS.md` already forbids private model
  binaries; the same mechanism keeps multi-gigabyte data out of a shipping artifact.
- **The engine's own output is the primary free dataset.** This is the decisive licence finding: a
  deterministic solver whose outputs it owns can generate unlimited training data with no third-party terms
  at all. MakeHuman's licence makes the same point for its own outputs — *"no output from MakeHuman contains
  any trace of program logic… there is no limitation on what you can do with this combined output"* (§6.3).
  Anny's authors did exactly this at scale, generating the `Anny-One` synthetic set from their own model.
  Self-generated data, labelled by human approval (3.F.2), is both **free** and **auditable**.
- Any external dataset requires: licence of the data, licence of anything it was derived from, licence of
  any model trained on it, and a `THIRD_PARTY_NOTICES.md` entry — all four verified from primary sources
  before use. HumanML3D fails this test (§6.3).
- Datasets are versioned and content-addressed; a training run records the exact dataset version it used.

#### 3.F.7 Optional local model fine-tuning

**Optional, local, and never required** (**P5**, **P9**). Design:

- Base models restricted to the free list (§6.3): Apache-2.0 or MIT weights, verified from the licence file.
- Training data restricted to 3.F.6: the engine's own outputs plus human-approved labels.
- Output is a **LoRA-class adapter**, not a full model — small enough to version, cheap to roll back.
- Runs on the owner's workstation, never on device (`minSdk 26`, 1,536 MB engine budget — the same
  constraint that keeps artifact generation off the phone, §3.A.1).
- An adapter is inert until a human approves it against golden records (3.F.4) and it is registered with
  provenance (3.F.8).
- **Fine-tuning never touches the solver.** It may improve a *proposer* — prompt→`af_spec`, texture
  critique, motion selection — and every proposal still crosses the P2 validation boundary and is still
  judged by §3.E. A fine-tuned model that proposes a worse avatar is caught by regression detection, not by
  trust.

If the owner never enables this, nothing degrades: the deterministic parser and the curated constant tables
remain the floor.

#### 3.F.8 Versioned models and rollback

| Property | Rule |
|---|---|
| Storage | Adapters and constant tables are addressed by SHA-256. **Weights never in Git and never in the APK**; Git holds the manifest, the hash, the provenance and the evaluation record |
| Manifest | Per version: base model id and revision, adapter hash, dataset version, training commit, the golden records it was validated against, and the approval decision with its reason |
| Activation | Exactly one active version per task kind, pinned in a manifest, so a run is reproducible |
| Rollback | Repoint the manifest at a previous version. Because the solver is untouched and adapters are inert until approved, rollback is a data change, not a code change |
| Auditability | Every version traceable to the commit, dataset, device and human decision that produced it |
| Failure | A version that cannot be validated against golden records is never activated; a version that later regresses is rolled back and its rejection recorded (3.F.2) |

#### 3.F.9 The loop

```
propose (gateway or deterministic parser)
  -> instantiate (af_spec / asset choice)          [P2 boundary validates]
  -> generate (solver, unchanged)                  [P1]
  -> evaluate (§3.E, with commit + device)         [P3]
  -> compare to golden record                      [3.F.5]
  -> human approves or rejects                     [3.F.2]
  -> land as data: constant table | golden record | adapter manifest | decision log
```

**Anti-goals, stated so no agent "improves" them later:** no online learning inside the app; no telemetry
leaving the device; no self-modifying shaders, fields or prompts; no automatic activation of anything a
human has not approved; no dependency on a paid service at any point in the loop.

**Status today.** The pattern exists in exactly one place and is proven end to end (`kFabrics` /
`kEyeColours`, commit `42fa78a`, with contract test and provenance). Everything else in §3.F — experience
memory, approved feedback, experiment history, golden records, regression detection, dataset management,
fine-tuning, versioned adapters: **MISSING**.

### 3.G Zero-provider architecture

Requirement 4 of the owner's brief, and the concrete form of **P5** and **P9**: *AvatarForge must retain its
core intelligence, diagnostics, asset inspection, procedural generation and evaluation capabilities without
subscriptions or remote services.*

This is not a fallback mode. **Tier 0 is the product.** Everything above it is additive.

#### 3.G.1 Capability tiers

| Tier | Configuration | Cost | What is available |
|---|---|---|---|
| **0 — Zero provider** *(default, and the guaranteed floor)* | Nothing configured, no network, no models | £0 | The entire deterministic engine, plus all of Neural Core's structural intelligence: asset inspection, evaluation, regression detection, memory, procedural generation |
| **1 — Local text** | A local Ollama (or equivalent) the user already runs | £0 | Natural-language → `af_spec` proposals; structured critique of diagnostics in prose |
| **2 — Local artifact** | The user's own workstation GPU running free weights (FLUX.1 [schnell], TripoSR) | £0 in licence | Reference-image generation, image → mesh candidates, subject to M2 inspection |
| **3 — Paid opt-in** | A user-supplied API key (OpenAI, OpenRouter, other OpenAI-compatible hosts) | user-funded | Whatever tier 1 does, with larger models. **Never required, never bundled, never a default** |

Tiers are strictly additive and strictly optional. Removing tier 3 must not change tier 0 behaviour at all.

#### 3.G.2 What Tier 0 guarantees

Every one of these is **free, offline and native**, and none is degraded by the absence of a provider:

| Capability | Tier-0 mechanism | Why it needs no model |
|---|---|---|
| Avatar generation | `af_spec` → the deterministic solver → GLB | Already REAL + VERIFIED; same spec and seed give byte-identical output |
| Prompt understanding | `af_engine_analyze_prompt` (583-line deterministic parser) | Already shipped; the fabric and iris tables in `42fa78a` extended it without any model |
| Asset inspection (**B**) | `af_asset_inspect`, native C++ beside the solver, reusing `af_glb_read`, `Bvh`, `Region` | Measurement, not inference |
| Mesh and geometry diagnostics | `Bvh::raycast` / `signedDistance`, region distribution, symmetry deviation | Already implemented in `af_mesh.h` |
| Human-character critique (**C**) | `af_anatomy` → `BodyMetrics`; proportions judged against WHO-calibrated reference values landed as **constants** | A reference table is data, not a service — exactly how `kFabrics` works |
| Animation (**D**, procedural part) | The verified clip library in `af_rig.cpp`, blending, retargeting to this engine's rig, foot-contact and slide metrics | Deterministic; and §3.D item 8 is BLOCKED on licence precisely *because* the learned route is not free |
| Evaluation (**E**) | `EvaluationRecord` from `RendererTelemetry`, host tests, instrumented suites, probes | Aggregation of measurements already taken |
| Learning and memory (**F**) | Constant tables, golden records, experiment history, decision log — all data in Git or app-private storage | Human-approved data, not weights |
| Regression detection | Golden-record comparison, tolerances, byte-identity where it holds | Arithmetic |
| Optimisation | Screen-density LOD (`af_lod_select.h`), `BudgetRenderer` tiers | Already REAL + VERIFIED on host |

**The consequence worth stating plainly:** subsystems **B, C, D, E and F are entirely Tier-0**. Only the
*generative extras* in A sit above it. An engine with no AI provider configured is still an intelligent 3D
engine by the brief's own definition — it analyses, diagnoses, measures, evaluates, remembers and improves.
What it does not do is accept an open-ended natural-language description beyond the deterministic parser's
vocabulary, or synthesise new textures and meshes from images. That is an honest, bounded limitation, not a
broken product.

#### 3.G.3 Degradation is explicit, never silent

Every capability that can degrade declares its floor:

```kotlin
data class CapabilityStatus(
    val capability: TaskKind,
    val activeTier: Tier,          // 0..3
    val floorTier: Tier,           // always 0 for structural capabilities
    val degraded: Boolean,         // true when activeTier < the tier the user configured
    val reason: DegradationReason  // NO_PROVIDER | PROVIDER_UNREACHABLE | TIMEOUT | THERMAL_BACKOFF
                                   // | BUDGET_EXCEEDED | OUTPUT_REJECTED_BY_VALIDATOR | LICENCE_BLOCKED
)
```

`OUTPUT_REJECTED_BY_VALIDATOR` matters: a provider that returns malformed or out-of-bounds output is treated
as **no provider**, falling through to the deterministic parser rather than being partially trusted
(**P2**). `LICENCE_BLOCKED` is a first-class reason, so a capability excluded by **P9** reports itself as
blocked instead of quietly vanishing.

#### 3.G.4 The guarantee is tested, not promised

A CI job runs the **whole** generation and evaluation path with zero providers configured, no network, and
asserts:

1. Generation succeeds and is **byte-identical** to the golden SHA-256 for the deterministic seed.
2. `af_asset_inspect` returns the same verdicts as with a provider configured.
3. `EvaluationRecord`s are produced with `commit` and `device` populated.
4. No network call is attempted — asserted, not assumed, by pointing the HTTP layer at an unreachable host
   and requiring success.

Until that job exists and passes, "works with zero providers" is **UNVERIFIED** and must be reported as
such. This is mission **M1**'s exit gate.

### 3.H Procedural capabilities versus AI-generated capabilities

Requirement 6 of the owner's brief: *the engine must remain useful without generative providers.* The
mechanism is a hard separation — in code, in types, and in the capability register below.

#### 3.H.1 Separation rules

1. **Different packages.** `.../avatarforge/neural/procedural/` versus `.../avatarforge/neural/generated/`.
   Nothing in `procedural/` may import from `generated/`. Enforced by a test that parses the imports.
2. **Different types.** Procedural results are plain values. AI results are wrapped: `Proposed<T>` carries the
   value, the provider, the model revision, the validation outcome, and the fallback that was *not* used. A
   `Proposed<AvatarSpec>` can never be silently assigned to an `AvatarSpec`.
3. **Every AI capability names its procedural fallback** in the register below, and the fallback is what
   runs at Tier 0.
4. **AI output is a proposal, never an instruction.** It crosses the P2 boundary or it is discarded.
5. **The solver is procedural-only** (**P1**). No model may emit vertices, and no generated capability may
   call `af_engine_run_all` with values that did not cross the boundary.

#### 3.H.2 Capability register

| Capability | Procedural (Tier 0, always) | AI-generated (optional, additive) | Fallback relationship |
|---|---|---|---|
| Prompt → character | `af_engine_analyze_prompt`, 583-line deterministic parser; fabric/iris/eye tables | Tier 1+ model proposes `af_spec` fields from free-form language | AI proposes → validator bounds-checks → deterministic parser fills anything rejected. **The parser is the floor, not an error state** |
| Body proportions | `af_anatomy` solver → `BodyMetrics`; WHO-calibrated reference constants | Tier 1 critique of proportions in prose; tier 2 reference imagery | AI cannot change proportions directly, only propose constants for human approval (§3.F) |
| Mesh / asset diagnostics | `af_asset_inspect`, `Bvh`, `Region`, probes | Tier 1 summarises a report in prose | Prose is a rendering of the record, produced last (**P4**) |
| Textures and materials | `af_texture.cpp` procedural paint; `kFabrics`/`kEyeColours` constants | Tier 2 reference images for human curation | Reference images inform constants; they are never pasted onto an avatar |
| Skeleton and skin weights | `af_rig.cpp`: 71 joints, four-influence weights, 52 ARKit blendshapes | none proposed | Procedural only |
| Animation clips | The verified clip library, blending, retargeting to this engine's rig | **BLOCKED** — learned motion generation is excluded by licence (§3.D item 8, §6.3) | Procedural only, and this is a deliberate P9 outcome |
| Animation critique | Foot-contact F×4 detection, slide in mm, joint-angle error, retarget drift | Tier 1 explains a critique in prose | Metrics are procedural; only the explanation is AI |
| IK and foot placement | New procedural solver (M5) | none proposed | Procedural only |
| Evaluation | `EvaluationRecord` from existing telemetry and tests | none | Procedural only — an AI may not grade its own output |
| Regression detection | Golden-record comparison, tolerances, byte identity | none | Procedural only |
| Memory and learning | Constant tables, golden records, experiment history, decision log | Optional tier-2+ fine-tuned adapters (§3.F.7), inert until approved | Adapters improve *proposers* only; every proposal still crosses P2 and is still judged procedurally |
| New geometry / meshes | The solver | Tier 2 image → mesh candidates (TripoSR), imported as assets and inspected by M2 | Generated meshes arrive as *foreign assets* and inherit `AF_ASSET_FOREIGN_SKELETON_STATIC` until M5 lands |
| Performance optimisation | Screen-density LOD, `BudgetRenderer` tiers | none | Procedural only |

Read down the middle column: **the engine's skeleton, its evaluation, its regression detection, its memory
and its performance work contain no AI at all.** That is the design, not an omission. AI is concentrated
where it is genuinely additive — interpreting free-form language, and proposing new constants for a human to
approve — and excluded where it would be unauditable or unfree.

### 3.I Asset provenance

Requirement 1 of the owner's approval: **every generated or imported asset must record its source, model,
model version, licence and generation method.**

**Reuse, do not invent.** `afcore.h:390` already documents that the exporter writes *"an `extras` block
carrying the spec"* into every GLB. Provenance extends that existing mechanism rather than adding a sidecar
format for generated assets.

```c
/* afneural.h — append-only, struct_size-versioned like af_spec */
typedef struct af_asset_provenance {
    int32_t struct_size;
    char    source[64];          /* "engine" | "import" | "artifact-provider" | "owner-file" */
    char    generator[96];       /* e.g. "af_engine_run_all", "tripsr", "user" */
    char    model_id[96];        /* provider model name; empty when procedural */
    char    model_version[64];   /* revision/tag/hash the provider reported; empty when procedural */
    char    licence[96];         /* SPDX id or "PROPRIETARY-OWNER"; never empty */
    char    generation_method[32]; /* "procedural" | "model-inference" | "human-authored" */
    char    commit[41];          /* the exact head that produced it */
    uint8_t input_sha256[32];    /* for imports: the file; for generation: the spec+seed digest */
    uint8_t output_sha256[32];   /* the asset itself */
    int64_t created_unix_ms;
    int32_t licence_verified;    /* 1 only if the licence text was read from a primary source (§6.3) */
} af_asset_provenance;
```

Rules:

1. **`licence` and `generation_method` are never empty.** An asset that cannot state them is quarantined,
   not displayed — because an unstated licence is exactly how §6.1's trap springs.
2. **`licence_verified` distinguishes read-from-primary-source from asserted.** Only §6.3.2 entries may set
   it to 1. This makes requirement 3 (*do not treat secondary-source claims as verified*) a field in the
   data, not a habit.
3. **Procedural assets carry full provenance too**, with `generator="af_engine_run_all"`,
   `generation_method="procedural"`, empty model fields, and `licence="CC0-1.0"` or whatever the engine's
   own output terms are — justified by §3.F.6 and MakeHuman's own §D position that output carries no
   upstream logic. That claim must be confirmed for *this* engine by the owner (decision D9), not assumed by
   analogy.
4. **Imports record what the file declares, not what we hope.** A GLB with no `extras` gets
   `source="import"`, `licence="UNKNOWN"`, `licence_verified=0`, and is flagged in the M2 report. Unknown is
   a valid, honest state; a guessed licence is not.
5. **Artifact-provider assets inherit the receipt.** `ArtifactReceipt` (§3.A.1) already carries bytes,
   SHA-256, declared licence, provider and model revision — it maps field-for-field onto this struct, so a
   generated mesh arrives with provenance already attached.
6. **Provenance travels with the asset.** Written into the GLB `extras` on export, read back on import, and
   surfaced in the M2 report. An asset whose provenance cannot be read back is treated as `UNKNOWN`.

#### 3.I.1 D9 completed — separate licensing and provenance rules per asset class

Owner instruction, 2026-11-26. Three classes, three rule sets. **An asset's class is determined by how it was
produced, never by where it is stored or what it is called.**

| Class | Definition | Licence rule | Mandatory provenance |
|---|---|---|---|
| **PROCEDURAL** | Produced by `af_engine_run_all` from `af_spec` + seed, with no external model in the path | The licence of **this engine's own output** — which is currently **declared nowhere** and is decision **D9**. It may **not** be assumed CC0 by analogy to MakeHuman's §D; that statement is about MakeHuman's output, not this engine's. Until D9 is declared, `licence` carries `UNDECLARED-OWNER-DECISION-PENDING` and `licence_verified=0` | `source="engine"`, `generator="af_engine_run_all"`, `generation_method="procedural"`, **model fields empty**, `commit`, the spec+seed digest, `output_sha256` |
| **AI-GENERATED** | Produced by any provider (Tier 1–3): an image, a mesh, a texture, or a proposed spec | The licence is **the provider's own output terms**, recorded per receipt and **never assumed free**. `licence_verified=1` only where those terms were read from a primary source. If the producing model is on the §6.3.3 excluded list, the artifact is **rejected at ingestion, not merely labelled** — a non-commercial model's output cannot be laundered by recording its licence honestly | `source="artifact-provider"`, `generator=<provider id>`, `model_id`, `model_version`, `licence=<provider output terms>`, `generation_method="model-inference"`, plus a digest of the request parameters |
| **IMPORTED** | A file supplied by the user, or downloaded | The licence is **whatever the file itself declares**. No declaration ⇒ `licence="UNKNOWN"`, `licence_verified=0`, flagged in the M2 report. **Never inferred** from a filename, a source URL, an extension or a user's assertion | `source="import"` (or `"owner-file"`), `generator="user"`, `licence=<declared or UNKNOWN>`, `input_sha256` |

Five rules that apply across all three classes:

1. **Quarantine, do not guess.** An asset whose licence is `UNKNOWN`, `UNDECLARED-…` or `licence_verified=0`
   may be **inspected and reported**, but may **not** be distributed, packed into an APK, or used as training
   data. Quarantine is a state, not an error — the engine keeps working (§3.G).
2. **Provenance chains; it never overwrites.** An imported asset that the engine re-exports becomes a **new
   PROCEDURAL asset with an IMPORTED ancestor**. `af_asset_provenance` therefore gains one field,
   `uint8_t derived_from_sha256[32]`, so the chain survives the round trip. Overwriting an ancestor's licence
   with the engine's own is how a restricted file would become "procedural" and slip into an APK.
3. **Training-data eligibility is a licence question, not a convenience.** Only PROCEDURAL assets with a
   declared licence (post-D9) may be used as training data (§3.F.6). AI-GENERATED assets may **not** be used
   as training data unless the provider's output terms expressly permit it — Hunyuan3D 2.1's clause defining
   "Model Derivatives" to include *"methods based on the generation of synthetic data Outputs… for training
   that model"* is the verified example of why this rule exists (§6.3.3).
4. **The owner's own material is a fourth, closed case.** Dropbox material and reference models
   (`mylove*.glb`) are always `source="owner-file"`, `licence="PROPRIETARY-OWNER"`, and are **never**
   committed, packed, exported as training data, or sent to a provider (`AGENTS.md` rule 5). No mission may
   change this.
5. **`licence_verified` is earned, not asserted.** It is `1` only where a human or an agent read the licence
   text from a primary source and recorded where and when. This is requirement 3 of the owner's approval
   (*do not treat secondary-source licence claims as verified*) expressed as a field in the data.

**D9 itself — the decision the owner still owes:** declare the licence terms of this engine's own generated
output. Recommended: an explicit statement in the README and `AGENTS.md` that avatars generated by
`af_engine_run_all` are the user's own data, on the reasoning that the engine's inputs are its own code plus
constants whose provenance is already recorded in `THIRD_PARTY_NOTICES.md`. That reasoning must be **stated
by the owner**, because it is a legal position about their project, not an engineering inference. Until it
is, M2 fills the field with `UNDECLARED-OWNER-DECISION-PENDING` and every generated asset is quarantined
from training and distribution — which is safe, honest, and mildly inconvenient, in that order.

**Status today: MISSING.** No provenance of any kind is recorded for generated or imported assets, and D9 is
undeclared.

### 3.J Learning verification benchmarks

Requirement 2: **measurable benchmarks proving that each learning-system revision improves results without
introducing regressions.**

A "learning-system revision" is a versioned change to any of: the curated constant tables, the golden
records, an adapter manifest, or a critic's thresholds. Each revision must pass this gate before it lands.

#### 3.J.1 The fixed benchmark suite

| Element | Definition | Why |
|---|---|---|
| **Subjects** | A frozen set of specs and seeds, including the deterministic **seed 1337** already used in this repository's evidence, plus one subject per gender, one per fabric in `kFabrics`, one per iris colour in `kEyeColours`, one minimal and one maximal-complexity spec | Covers every constant table a revision could touch |
| **Assets** | `app/src/androidTest/assets/rigged_test_figure.glb` and `humanoid_motion_pack.glb`, plus the bundled `female.glb` | Already in the repository, already licensed, already used by CI |
| **Metric families** | All seven from §3.E.2 | A revision must not improve one family by regressing another |
| **Device contexts** | **host** and **CI emulator** as a minimum; **SM-S921B** for any revision touching rendering or performance (§3.L) | An emulator pass is never a phone pass |
| **Baseline** | The golden records for the revision being replaced | Comparison is against a record, not a memory |

#### 3.J.2 The four assertions a revision must satisfy

1. **No regression.** No metric in any family moves beyond its declared tolerance against the baseline.
   Tolerances are the repository's existing ones where they exist — symmetry **0.004 model units**,
   animation round trip **0.2 mm**, visible pixels **≥5,000** — and declared per metric where they do not.
2. **At least one measured improvement.** A revision that changes nothing measurable is not an improvement
   and must be rejected, not merged. The improvement is named: metric, before, after, tolerance, subject.
3. **Byte-identity where it is claimed to hold.** This repository's strongest check: the `42fa78a` ports
   were proven regression-free because `male=73,399,512 B` and `female=72,076,316 B` came out
   **byte-identical** before and after. Where a revision claims not to affect generation, SHA-256 equality
   is required, not tolerance.
4. **Provenance complete.** Every record in the suite carries `commit` and `device`; a record missing either
   fails the gate (§3.E.1).

#### 3.J.3 Two controls that validate the harness itself

A benchmark that cannot detect a regression is worse than no benchmark, because it manufactures confidence.
So the harness is validated with both controls, and both run in CI:

| Control | What it does | Pass condition |
|---|---|---|
| **Null control** | Runs the suite against an *unchanged* revision | **Zero** metric differences and byte-identical outputs. Any non-zero result means the harness itself is noisy — fix the harness before trusting any verdict |
| **Negative control** | Runs the suite against a *deliberately worsened* constant (e.g. set a fabric roughness outside its documented range, or perturb an iris swatch) | The regression **must be detected and named**. If it is not, the suite has a blind spot and the revision under test is not cleared |

The negative control is the one most often omitted and the one that matters most: it is the only evidence
that "no regression" means anything.

#### 3.J.4 Reporting

A revision report states: revision id; baseline revision; subjects and devices; per-family before/after with
units; the named improvement; the null-control result; the negative-control result and which metric caught
it; and the exact head SHA. **Nothing lands without all of it** — the same discipline this repository already
applies to CI evidence, where a result belongs only to the commit that produced it.

**Status today: MISSING.** The measurements exist (§3.E.2) and byte-identity has been used once, manually,
to prove a port safe. No benchmark suite, no controls, no revision gate.

### 3.K Android-first execution

Requirement 3: **distinguish capabilities that run natively on the phone from optional workstation/offline
training and generation.**

The rule: **the phone is the product; the workstation is a tool.** Anything that makes the engine intelligent
runs on the phone. Anything that is expensive, optional or generative runs off it — and its absence must not
degrade the phone.

| Capability | Runs natively on SM-S921B | Workstation / offline | CI only | Notes |
|---|---|---|---|---|
| Deterministic generation (`af_spec` → GLB) | **Yes** | — | — | Already REAL + VERIFIED on the phone: 1,100,168 triangles at 53.8 FPS (§3.L) |
| Prompt parsing (`af_engine_analyze_prompt`) | **Yes** | — | — | 583 lines, no model, no network |
| Asset inspection `af_asset_inspect` (**B**) | **Yes** | — | — | Native C++ beside the solver; the Python probes are the CI/authoring equivalent |
| Evaluation records (**E**) | **Yes** | — | — | Aggregates `RendererTelemetry`, which is already collected on device |
| Regression detection | **Yes** (against on-device golden records) | Yes (full suite) | Yes | Golden-record comparison is arithmetic |
| Experience memory, feedback, experiment history (**F.1–F.3**) | **Yes** | — | — | App-private storage, bounded ring buffer |
| Dataset management (**F.6**) | Records manifests only | **Yes** — dataset assembly | Yes | No dataset stored on device |
| Learning constant tables (**F.4–F.5**) | **Yes** (read) | Authored and validated offline | Validated | Tables are data, compiled in or shipped as small JSON |
| Local text inference (Tier 1) | **Only via a localhost server the user runs** | Yes | — | Ollama is a separate process with its own memory; the app never loads weights |
| Image generation (Tier 2) | **No** | **Yes** | — | ~8–12 GB VRAM for FLUX.1 [schnell] |
| Image → 3D mesh (Tier 2) | **No** | **Yes** | — | TripoSR ~4–8 GB VRAM |
| Body-model reference (MHR, Anny) | **No** | **Yes** | **Yes** | Python/PyTorch; authoring and CI only |
| Fine-tuning (**F.7**) | **No** | **Yes** | — | Optional; disabling it changes nothing |
| Paid remote inference (Tier 3) | Network call only | — | — | Opt-in, user-funded, never required |
| LOD selection, `BudgetRenderer` tiers | **Yes** | — | — | Already on device; the pixel effect is UNVERIFIED (B2) |
| Foot-contact and slide metrics (**D**) | **Yes** | — | — | Computed from skin matrices and landmarks the engine already has |
| IK (**D**) | **Yes** | — | — | Procedural solver |
| Learned motion generation (**D**) | **No — BLOCKED on licence** | **No — BLOCKED on licence** | — | §3.D item 8; not a hardware limitation, a P9 one |

**Three consequences that must not be designed around:**

1. **No model weights are ever loaded into the app process.** On-device text inference means talking to a
   localhost server the user runs; the weights live in that server's memory, not the engine's. This is what
   keeps §3.L's memory budget intact.
2. **Anything marked "No" must have a Tier-0 path** (§3.G.2) or be reported as `LICENCE_BLOCKED` /
   `NOT_AVAILABLE_ON_DEVICE` — never silently absent.
3. **A workstation capability must never be on a phone code path.** If a feature needs a workstation, the
   phone either does without it or consumes an artifact the workstation produced, with provenance (§3.I).

### 3.L Device resource management — SM-S921B

Requirement 4: **account for RAM, GPU, thermal throttling, storage and battery on the actual SM-S921B.**

**The target device, from `AGENTS.md`:** Samsung Galaxy S24 **SM-S921B**, **Exynos 2400**, **Xclipse 940**
GPU, **8 GB RAM**, **1080 × 2340**, **120 Hz**, Android 17. **Critically: on this phone OpenGL ES is ANGLE
on top of Vulkan** — the app's own report reads `ANGLE ((Samsung Xclipse 940) on Vulkan 1.3.304)`. Every GPU
assumption in this plan is therefore an assumption about a **translation layer**, which is why
`docs/PHOTOREAL_UPGRADE_PLAN.md`'s GLES reasoning cannot be transferred to this device unmeasured, and why
the stale, non-compiled `af_vk.cpp` is *not* a shortcut to native Vulkan here.

#### 3.L.1 What has actually been measured on the phone

> ### ⚠ HISTORICAL BASELINE — **NOT** CURRENT RUNTIME VERIFICATION
>
> Every number in this subsection was measured on the **APK of commit `797d156`**, in a **38-second session**,
> on **2026-11-15**. That build **predates the screen-density LOD selection entirely** (`07610b5`) and
> predates the trunk merge (`88c2c67`), the rescue (`42fa78a`) and everything since.
>
> These figures are preserved as the **baseline to beat**. They must **never** be presented, quoted or
> summarised as evidence about the current line, as a current FPS claim, or as verification that any present
> commit runs well on a phone. `AGENTS.md` is explicit that for every build after `797d156`, **"Nothing
> verified."** Any statement of the form "the app runs at N FPS on the S24" without a commit SHA and a date
> attached is a violation of §2.1 and of owner instruction 5 of 2026-11-26.

From `AGENTS.md` and `docs/PHONE_TEST_2026-11-15.md` (APK of `797d156`, a 38-second session). These are the
only real device numbers this project has, and Neural Core's budget must be built on them rather than on
estimates — **as a baseline, not as a current result**.

| Resource | Measured value | Status |
|---|---|---|
| Frame rate, generated avatar (**1,100,168 triangles**), last 240 frames | **53.8 FPS**; interval median **17.7 ms**, p95 **24.6 ms**, p99 **30.6 ms**, max **42.4 ms** | **REAL + VERIFIED** for a 38 s session |
| Quality tier behaviour | Started at level **2** (4× MSAA, 2048 shadow map, 4 bloom levels), ended at level **1** (2× MSAA, 1024, 3). The renderer steps down only after **more than 30 consecutive frames slower than 30 FPS**, so the phone did not hold 30 FPS at level 2 with this mesh. **Frame times at level 2 were not recorded** | **PARTIAL** |
| Native heap, generated avatar | **304 → 306 MB** | **REAL + VERIFIED** |
| Requested GPU bytes | textures **72.7 MB**, geometry **127.9 MB**, render targets **84.2 MB**, falling to **44.0 MB** after the tier step-down | **REAL + VERIFIED** |
| Generated-avatar load timings | import **1.3 s**, first frame **2.2 s** | **REAL + VERIFIED** |
| Imported file, **144,073 triangles** | 3 frames, 11 draw calls, bounds in frustum, GL and EGL errors `0x0`; import **312 ms**, first frame **828 ms**, native heap **134 MB** | **REAL + VERIFIED** |
| **Heat, battery drain, sessions longer than 38 s** | **Not measured** | **UNVERIFIED** |
| Every build after `797d156` | **"Nothing verified."** The owner reported *"animation crashes and no built in controls"* on `041e31d`; the cause is **inferred** (a generated avatar sent to the Filament preview), **not confirmed** | **UNVERIFIED** |
| CI emulator, for contrast (`2920265`) | Gown avatar **2,125,600 triangles**: 323,702 geometry pixels through the production view, 15 animations listed, `t_pose` started, 81,407 pixels changed, no crash or ANR, native heap **734 MB** | REAL + VERIFIED **on the emulator only** |

A recorded defect worth designing against (`docs/PHONE_TEST_2026-11-15.md`, P1): a stale session **overwrote
the current session's record** after the app returned from the file picker, and re-imported an **83 MB** file
for nothing. That is a resource-management bug and a provenance bug at once, and §3.I plus §3.N both exist
partly to prevent its recurrence.

#### 3.L.2 Neural Core's resource budget

| Resource | Existing measured cost | Neural Core allowance | Enforcement |
|---|---|---|---|
| **RAM (native heap)** | 304–306 MB generated, 134 MB imported, against a **1,536 MB** engine default budget and **8 GB** device RAM | **≤ 8 MB** additional, and **zero** for model weights | No weights in the app process (§3.K). Experience memory is a bounded ring buffer with a byte cap, evicting by policy (§3.F.1). Asserted by a host test and checked on device by M8 |
| **GPU** | textures 72.7 MB + geometry 127.9 MB + render targets 84.2 MB, dropping to 44.0 MB at tier 1 | **Zero.** Neural Core allocates no GPU resource | Diagnostics are CPU-side; `af_asset_inspect` reads bytes, not textures. The only GPU consumer remains the renderer, whose tier logic is untouched |
| **Thermal** | `DeviceResourceGovernor` reads `PowerManager.currentThermalStatus` — **REAL + VERIFIED in source and CI**; actual thermal behaviour on SM-S921B **never measured** | Inference and any burst work must back off on thermal status, reusing the existing governor | M8 must measure a sustained session and record `currentThermalStatus` transitions. Until then, thermal headroom is **UNVERIFIED** and must not be claimed |
| **Storage** | `BoundedGlbReader` ceiling **128 MiB**; decoded-texture budget **448 MiB**; asset fixtures ~596 KB (donor, unlanded) | Provenance adds **bytes per asset**, not megabytes; experience memory capped in bytes; golden records are JSON | **No dataset, weights or adapter in the APK** (§6.5). Adapter files live outside the repository and the APK (§3.F.8) |
| **Battery** | **Never measured** | No claim may be made | **MISSING.** M8 must measure drain over a defined session before any battery statement is written anywhere. An unmeasured battery cost is reported as `not measured`, never estimated (P3) |

#### 3.L.3 The honest gap

**Everything on this device after `797d156` is unverified, and the LOD work has never run on it at all**
(blocker B2). The 53.8 FPS figure describes a build *before* screen-density LOD selection existed, drawing
the full 1,100,168-triangle mesh at tier 2 stepping to tier 1. It is therefore a **baseline to beat**, not
evidence that the current line performs well. No number in this plan may be presented as a current-device
result until M8 produces one, and M8 must report `currentThermalStatus`, battery and a session longer than
38 seconds — the three things the 2026-11-15 test explicitly did not measure.

### 3.M Autonomous improvement without silent overwrite

Requirement 5: **allow Neural Core to propose, test and compare improvements, but never silently overwrite
proven engine components.**

#### 3.M.1 The protected set

These are the proven components. Neural Core has **no write access** to any of them, by construction:

| Protected | Why |
|---|---|
| `af_field.cpp` — the single SDF for everything visible | The engine's geometric heart; 3,128 lines |
| `af_isosurface` / `af_body` — lattice, marching tetrahedra, refinement, Taubin | Verified meshing pipeline |
| `af_uv.cpp` — charts, shelf packing, seam splitting | Verified UV pipeline |
| `af_texture.cpp` — procedural paint | Verified; already diverged once between branches (§1.6) |
| `af_rig.cpp` — 71 joints, 52 ARKit blendshapes, clip library | The skeletal and facial vocabulary is ABI-adjacent |
| `af_glb.cpp` / `af_glb_read.cpp` — export and import | Verified round trip; fuzzed |
| `src/render/*` — `af_gl.cpp`, `af_lod_select.h`, `af_avatar_lod.cpp`, `af_render.cpp` | The renderer the app actually uses; the R3 fix is verified on host |
| `af_spec`'s **meaning** and the **45-float** JNI contract, `kSpecFloatCount`, the `static_assert` | Cross-language ABI; `afcore.h`'s enum values are append-only ABI |
| The adults-only invariant (`AGENTS.md` §2.6) | A safety rule, never a tunable |
| The UI label `Run generated-avatar GLES diagnostic` | Owner-mandated exact string |

A change to any of these is a **human pull request**, reviewed like any other. Neural Core may *propose* one
by writing a diff into an experiment record; it may not apply one.

#### 3.M.2 What Neural Core may write

| Writable | Mechanism | Guard |
|---|---|---|
| New rows in curated constant tables | `kFabrics` / `kEyeColours` pattern | Contract test must pass end to end (parser → live table → exported bytes → re-import) |
| Golden records | Promotion on human approval only | Immutable once promoted (§3.F.4) |
| Experience memory, decisions, experiment history | Append-only | Eviction preserves regressions and decisions (§3.F.1) |
| Adapter manifests | Versioned, pinned | Inert until approved; rollback by repointing (§3.F.8) |
| New files under `neural/` | Normal code review | The `procedural/` ↔ `generated/` import isolation test (§3.H.1) |

#### 3.M.3 Propose → test → compare, never apply

```
propose        a candidate change, written as DATA into an experiment record
  -> shadow    evaluate it alongside the current configuration on the §3.J.1 suite
  -> compare   before/after per family, with the §3.J.2 assertions and §3.J.3 controls
  -> report    metric, both values, tolerance, subject, commit, device
  -> decide    HUMAN. There is no automatic apply path.
```

**Shadow evaluation is the key mechanism:** a proposal is measured *in parallel* with the live
configuration, on the same subjects, in the same run. Nothing is mutated to test it, so a bad proposal
cannot leave the engine in a worse state even transiently — which matters on a phone where a crash is a
support burden.

**Kill switch.** A single setting disables all proposal generation and all adapter activation, leaving the
Tier-0 engine (§3.G) exactly as it was. Because the protected set is untouched by design, the kill switch
has nothing to undo.

**Status today: MISSING**, and deliberately so — the current repository has no autonomous path at all, which
is why it is safe. This section defines how to add one without losing that safety.

### 3.N Recovery: rollback and backup

Requirement 6: **preserve model versions, learning memory, experiment history and working assets through
rollback and backup.**

#### 3.N.1 What must survive, and where it lives

| Artifact | Durable store | In Git? | In APK? | Backup |
|---|---|---|---|---|
| Curated constant tables | Source tree | **Yes** | compiled in | Git history |
| Golden records | Source tree (host) + app-private (device) | **Yes** for the canonical set | no | Git history + export bundle |
| Experiment history and decisions | Source tree (landed) + app-private (in flight) | **Yes** for landed | no | Git history + export bundle |
| Experience memory | App-private, bounded ring buffer | no | no | **Export bundle only** — it is volatile by design |
| Adapter weights | Outside the repository, content-addressed | **No** — manifest only | **No** | Owner-controlled storage; the manifest records SHA-256 so a lost file is detected, not silently replaced |
| Adapter manifests | Source tree | **Yes** | no | Git history |
| Datasets | Outside the repository | **No** | **No** | Owner-controlled; manifest records version and hash |
| Working assets (GLBs) | App-private / `Downloads` | Only licensed fixtures | Only licensed fixtures | Export bundle |
| Provenance (§3.I) | Inside the GLB `extras` | with the asset | with the asset | Travels with the asset |

**Rule:** anything large or licensed stays out of Git and out of the APK. Git holds the *manifest, the hash
and the reason* — which is enough to detect loss and to refuse to run, and is what keeps `AGENTS.md` rule 5
satisfied.

#### 3.N.2 Three rollback levels

| Level | Scope | Mechanism | Reversibility |
|---|---|---|---|
| **1 — Manifest repoint** | Adapters, active constant-set version, pinned tier | Change one pinned reference | Immediate; the previous version stays available |
| **2 — Git revert** | Constant tables, golden records, decision log, manifests, any code | A revert commit on the branch, merged with a **merge commit** per `AGENTS.md` rule 4 | Full history retained; nothing is rewritten |
| **3 — Branch or commit restore** | A whole line of work | This repository already proved it: 15 deleted branches were recorded with their tip SHAs in `docs/BRANCH_CONVERGENCE_PLAN.md` and are recoverable with `git push origin <sha>:refs/heads/<name>` | Works only because the SHAs were **written down first** |

Level 3 is why this plan requires every experiment record to name its commit. Provenance is a recovery
mechanism, not just an audit one.

#### 3.N.3 Backup and restore

- **Export bundle:** one JSON document containing golden records, experiment history, decisions, the active
  manifest set, and the experience memory — written through the **existing** `DiagnosticDownloads.kt` path
  to `Downloads/3D Mega Hub Diagnostics`, which already produces two JSON reports and is already reachable
  through the scoped AndroidX FileProvider. No new storage mechanism.
- **Bundle integrity:** SHA-256 of the bundle, plus the `commit` and device identity it was exported from,
  recorded inside it. A bundle that cannot state its provenance is refused on restore.
- **Restore is validated, not trusted:** schema version check, hash check, and a **dry run against the
  §3.J.1 suite** before anything becomes active. A restore that would regress a golden record reports the
  regression and requires explicit human confirmation.
- **Restore never overwrites a newer record silently.** Conflicts are reported per record with both values
  and both timestamps, and a human chooses — the same reason §3.M has no automatic apply path.

#### 3.N.4 What recovery does not cover, stated honestly

- **A lost adapter weight file is not recoverable by this project.** The manifest detects the loss via
  SHA-256 and refuses to activate; retraining is the only route. Backups of weights are the owner's
  responsibility, and the plan says so rather than implying otherwise.
- **Experience memory is volatile by design** and survives only if exported. That is a deliberate trade: it
  is telemetry-like, potentially large, and not worth forcing into Git.
- **No recovery mechanism may be tested by deleting real data.** Tests use fixtures and temporary
  directories; the `BoundedGlbReader` cache-directory pattern is the existing precedent.

**Status today: MISSING**, except that Git history and the recorded deleted-branch SHAs already provide
level 2 and level 3 for everything committed, and `DiagnosticDownloads.kt` already provides the export path.

## 4. Dependency graph, interface freezes and agent execution order

### 4.1 Mission register

| ID | Mission | Subsystem | Delivers the interface | Optional? |
|---|---|---|---|---|
| **M0** | Foundation and module boundary | — | `afneural.h`, the `procedural/` vs `generated/` package split (§3.H.1), `AF_ENABLE_NEURAL` | No |
| **M1** | Neural Gateway and the zero-provider guarantee | A, G | `NeuralProvider`, `ArtifactProvider`, `CapabilityStatus`, `ModelOutputValidator`, the Tier-0 CI job | No |
| **M2** | 3D Intelligence: asset inspection **and provenance** | B, §3.I | `af_asset_inspect` + `af_asset_report` + verdict codes + `af_asset_provenance` | No |
| **M3** | Evaluation harness, golden records **and the benchmark suite** | E, §3.J | `EvaluationRecord`, `Measurement`, `Verdict`, the tolerance table, the §3.J.1 suite with both controls | No |
| **M4** | Human Intelligence critics | C | proportion / face / skin critics emitting `Measurement`s | No |
| **M5** | Animation Intelligence: third-party skeletons, foot contact, IK, **morph-target application on import** *(learned motion generation is **BLOCKED on licence**, §3.D item 8)* | D, §3.D item 6 | the **API decision**, contact + slide metrics, IK, imported morph targets | No |
| **M6** | Learning memory, **revision gate**, autonomous-improvement guards **and recovery** | F.1–F.6, §3.J, §3.M, §3.N | `Experience`, `Decision`, `ExperimentRecord`, dataset manifest, the revision gate, the protected-set guard, export/restore | No |
| **M7** | Optimisation and asset intelligence | — | vertex packing / texture compression, measured | No |
| **M8** | Device verification and CI gate repair | — | real-device evidence, the runtime-gate verdict, `kotlin-compile-gate.yml` | No — **runs from wave 0** |
| **M9** | Versioned models, optional local fine-tuning, rollback | F.7–F.8 | adapter manifest, activation pinning, rollback | **Yes** — the only optional mission |

### 4.2 Dependency graph

```
                         ┌──────────────────────────────────────────────┐
   WAVE 0 (parallel)     │  M0 Foundation          M8 Device verif.     │
                         └───────┬──────────────────────┬───────────────┘
                                 │ afneural.h +          │ device evidence,
                                 │ package split         │ runtime-gate verdict
                    ┌────────────┴────────────┐          │
   WAVE 1 (parallel)│                         │          │
                    ▼                         ▼          │
              M1 Gateway                M2 3D Intelligence│
              (A, G)                    (B)              │
                    │                         │ af_asset_inspect
                    │ CapabilityStatus        │ + verdict codes
                    │                         ▼          │
   WAVE 2           │                   M3 Evaluation ◄───┘
                    │                   (E)  │ EvaluationRecord
                    │                        │ + golden records
                    │        ┌───────────────┼───────────────┐
   WAVE 3 (parallel)│        ▼               ▼               ▼
                    │   M4 Human        M5 Animation    M6 Learning memory
                    │   (C)             (D)             (F.1–F.6)
                    │        │               │               │
                    │        │               │               │ adapter manifest
   WAVE 4           │        ▼               │               ▼
   (parallel)       └──► M7 Optimisation     │          M9 Versioned models
                           (needs M2+M3)     │          + fine-tuning (OPTIONAL)
                                             │
                          M5 also needs M2 ──┘ (skeleton/joint inspection)
```

Edges, stated precisely:

| Edge | Why it is a hard edge |
|---|---|
| M0 → M1, M2 | Both need `afneural.h` and the package split; without it they would each invent a boundary and collide, which is how this repository got two bounded GLB readers |
| M2 → M3 | Evaluation's geometry and material metric families are sourced from `af_asset_inspect` (§3.E.2). M3 would otherwise re-parse GLBs |
| M8 → M3 | A device `EvaluationRecord` needs a real device identity and the runtime-gate question settled; M3's device-side records are meaningless until M8 has produced at least one |
| M3 → M4, M5, M6 | Critics, motion metrics and regression detection all emit into `EvaluationRecord`. Building them first means three private metric formats to reconcile later |
| M2 → M5 | Skeleton analysis, joint depth and skin-weight inspection are M2 outputs (§3.B) |
| M6 → M9 | Fine-tuning requires dataset management (F.6) and the approval workflow (F.2) to exist, or an adapter could be activated with no provenance |
| M2, M3 → M7 | Optimisation must be measured, and asset intelligence must already report what costs what |

### 4.3 Interface freeze points

Parallelism is safe only where the depended-on **interface** is frozen, not where the whole mission is
finished. Each freeze is a committed header or schema plus a test that pins it. A dependent mission may
start the moment its freeze lands, even if the provider mission is still working.

| Freeze | Published by | Contents | Unblocks |
|---|---|---|---|
| **F-0** | M0 | `afneural.h` skeleton with `struct_size` versioning; `neural/procedural/` and `neural/generated/` packages; `AF_ENABLE_NEURAL` default OFF; `docs/NEURAL_CORE_BOUNDARY.md` | M1, M2 |
| **F-1** | M2 | `af_asset_inspect` signature, `af_asset_report` fields with units, the verdict code enum | M3, M5, M7 |
| **F-2** | M3 | `EvaluationRecord` / `Measurement` / `Verdict` schemas, the metric-key registry, the tolerance table | M4, M5, M6, M7 |
| **F-3** | M5 | The **chosen** imported-animation API, recorded as a decision document with the rejected alternative and its reason | M5 implementation; anything touching animation later |
| **F-4** | M6 | `Experience` / `Decision` / `ExperimentRecord` schemas, dataset manifest format | M9 |
| **F-5** | M1 | `NeuralProvider`, `ArtifactProvider`, `CapabilityStatus`, task kinds, `Proposed<T>` | Tier 1–3 consumers (M4's prose critiques, M9's adapters) |

**F-3 is a decision freeze, not a code freeze**, and it is the most important one in the plan: two
incompatible imported-animation APIs already exist (§3.D item 1), and until one is chosen and written down,
any animation work risks building the second system this repository has already paid for once.

### 4.4 Execution waves

| Wave | Missions | Parallelism | Entry condition | Exit condition |
|---|---|---|---|---|
| **0** | **M0**, **M8** | Fully parallel — no shared files. M0 is engine scaffolding; M8 is CI and device work | Trunk merged (PR #17 landed) and branch cut | F-0 published; M8 has attributed the runtime-gate failure and produced ≥1 device record |
| **1** | **M1**, **M2** | Fully parallel — M1 is Kotlin providers plus `CapabilityStatus`; M2 is C++ inspection. They share only `afneural.h` | F-0 | F-1 and F-5 published; **M1's Tier-0 CI job green** (§3.G.4) |
| **2** | **M3** | Single mission — it is the pivot and must not be split | F-1, plus ≥1 device record from M8 | F-2 published; golden records exist for host **and** device |
| **3** | **M4**, **M5**, **M6** | Parallel, with one sequencing rule: **M5 must publish F-3 before writing animation code.** M4 and M6 are unconstrained | F-2 | Critics emit measurements; F-3 recorded; experience/decision stores working |
| **4** | **M7**, **M9** | Parallel. **M9 is optional** and may be cancelled by the owner without stranding M7 | F-2 + M2 for M7; F-4 for M9 | Measured before/after on a named device; adapter versioning with a demonstrated rollback |

### 4.5 Critical path

```
M0 → M2 → M3 → M6 → M9        (longest chain, and M9 is optional)
M0 → M2 → M3 → M4 → M7        (longest chain of required work)
```

**M3 is the pivot of the whole initiative.** Five missions wait on F-2. If M3 slips, everything in waves 3
and 4 slips; if M3 is rushed, four missions inherit a metric schema they must each privately work around —
which is exactly the duplication this convergence was created to end. Staff M3 first and give it the most
care, even though it is the least visually interesting mission.

**M8 is on no critical path but gates credibility.** It is the only mission that can convert this
repository's `IMPLEMENTED IN SOURCE` claims into `REAL + VERIFIED`, and §1.5(a) — the failing
generated-avatar runtime gate — stays unresolved without it. Run it from wave 0 and continuously.

### 4.6 What must not be parallelised

1. **M5's API decision (F-3) before M5's implementation.** Deciding and building at once is how two
   animation systems already appeared.
2. **M3 split across agents.** One owner for the metric schema, or four missions get four formats.
3. **Two missions editing `afneural.h` simultaneously.** It is append-only ABI like `afcore.h`; sequence
   additions through M0's owner or through frozen, non-overlapping blocks.
4. **M9 before M6.** Fine-tuning without dataset management and approval produces adapters with no
   provenance, which violates P3 and P6 at once.
5. **Any mission before PR #17 lands.** Branching from the pre-convergence trunk re-creates the stale-base
   problem in a single step.

## 5. Execution prompts for specialist agents

**The ten prompts below are in execution order** — the order in which agents should be released, per the
waves in §4.4. Sequence number and mission id differ deliberately, because the two missions that start in
wave 0 are M0 and **M8**, not M0 and M1.

| Seq | § | Mission | Wave | Starts when | Blocks |
|---|---|---|---|---|---|
| 1 | 5.1 | **M0** Foundation and module boundary | 0 | PR #17 landed | M1, M2 |
| 2 | 5.2 | **M8** Device verification and CI gate repair | 0 | immediately — needs a real Android toolchain | M3 (needs ≥1 device record), M7 |
| 3 | 5.3 | **M1** Neural Gateway and the zero-provider guarantee | 1 | F-0 | Tier 1–3 consumers, M9 |
| 4 | 5.4 | **M2** 3D Intelligence: asset inspection and provenance | 1 | F-0 | M3, M5, M7 |
| 5 | 5.5 | **M3** Evaluation harness, golden records, benchmark suite | 2 | F-1 + one device record | M4, M5, M6, M7 |
| 6 | 5.6 | **M4** Human Intelligence critics | 3 | F-2 | — |
| 7 | 5.7 | **M5** Animation Intelligence | 3 | F-1, F-2 | — |
| 8 | 5.8 | **M6** Learning memory, revision gate, recovery | 3 | F-2 | M9 |
| 9 | 5.9 | **M7** Optimisation and asset intelligence | 4 | F-1, F-2, M8 device data | — |
| 10 | 5.10 | **M9** Versioned models, optional fine-tuning, rollback | 4 | F-4, F-5 — **optional** | — |

Each prompt carries the eight required fields — exact mission, repository and branch, files and interfaces
involved, dependencies, acceptance criteria, required build and runtime evidence, README handover
requirements, explicit exclusions — plus, where applicable, the four additions from the owner's approval:
asset provenance (§3.I), the learning-revision benchmark gate (§3.J), Android-first placement (§3.K) and the
SM-S921B resource budget (§3.L).

Paste §5.0 first, then the mission block. §5.0 is repeated into every agent deliberately so no prompt
depends on another document having been read.

### 5.0 Shared preamble — paste at the top of every mission prompt

> You are a specialist engineering agent on **AvatarForge Neural Core**.
>
> **Read first, in this order:** `AGENTS.md` (rules and map), the README section *Branch convergence
> handover — 2026-11-26*, `docs/NEURAL_CORE_MASTER_PLAN.md` §1–§4 and §6, `PROJECT_STATUS.md`. Treat the
> latest README as the project handover.
>
> **Non-negotiable rules** (`AGENTS.md` §2 plus this plan's P1–P9):
> 1. Every claim carries a status word — REAL + VERIFIED, IMPLEMENTED IN SOURCE, PARTIAL, UNVERIFIED,
>    MISSING, BLOCKED — and says *where* verified: host, CI emulator, or phone. An emulator pass is never a
>    phone pass.
> 2. No invented numbers. A measurement not taken is written `not measured`.
> 3. No mock features: no placeholder generation, no inert button, no fake progress.
> 4. `main` is not touched. Work lands in `integration/avatarforge-renderer-port` through a pull request
>    with a **merge commit — never squash**, because commit ids are cited throughout the documents. No
>    force-push.
> 5. The owner's Dropbox material and reference models (`mylove*.glb`) are never committed and never packed
>    into the APK.
> 6. Adults only: every avatar is 30, unit-tested; `bust`/`glute` honoured only at 18+. Never relax this to
>    improve a metric.
> 7. Anything entering the repository from outside is recorded in `THIRD_PARTY_NOTICES.md` with source,
>    revision and licence **before** use.
> 8. Keep the UI label exactly `Run generated-avatar GLES diagnostic`.
> 9. Do not publish what was not gated.
> 10. **Free only (P9).** No licence fee, no revenue cap, no territory exclusion, no non-commercial-only
>     dependency, no paid API the engine requires. Before adding any dependency report four things: the
>     licence of the code, of the model or data files it needs to run, of what it was trained on where
>     knowable, and whether any of it is redistributed inside the APK. All four must be free. An MIT
>     repository badge is not an answer to the second or third — §6.3.1 lists five cases where secondary
>     sources were wrong and the primary licence was not.
> 11. **This initiative is exclusively inside AvatarForge** (`avatarforge-engine/`). Do not merge Neural
>     Core work into 3D-Mega-Hub, do not modify Cue-Next, and do not combine unrelated repositories,
>     branches or development objectives in one pull request.
>
> **Read §6 before choosing any model, dataset or library.** Verified-free shortlist: TripoSR (MIT covering
> weights), FLUX.1 [schnell] and Qwen-Image (Apache 2.0), TRELLIS (MIT), InstantMesh (Apache-2.0), Anny
> (Apache 2.0), MakeHuman **assets** (CC0 — its code is AGPL, so assets only). Verified-excluded: the SMPL
> family, AMASS, HumanML3D, MDM, MotionGPT, every FLUX.1 [dev] variant, Hunyuan3D 2.1, SD 3.5 weights,
> contact-human-dynamics.
>
> **Branch from the trunk** `integration/avatarforge-renderer-port`, never from `main`, never from a branch
> `docs/BRANCH_CONVERGENCE_PLAN.md` lists as deleted or superseded, and never before PR #17 has landed.
>
> **Before writing any code: grep for it.** This repository rebuilt the launcher icon four times, the GLES
> diagnostics path four times, bounded GLB reading twice and the animation system twice. Your first
> deliverable is a list of what you searched for and what you found.
>
> **CI traps:** `android.yml` does not fire on `arena/**` — dispatch it (`gh workflow run android.yml --ref
> <branch>`) or open the PR. `workflow_dispatch` refuses a bare SHA (`422 No ref found`). Actions logs and
> artifacts are served from hosts a restricted workspace may not reach, so capture what you need locally.
>
> **Host verification loop** (a host-only workspace has no JDK, Android SDK, NDK or KVM):
> ```
> cmake -S avatarforge-engine/src/main/cpp -B build/native-tests -DBUILD_TESTING=ON -DAF_OPT_FLAGS=-O1
> cmake --build build/native-tests --parallel 2
> ctest --test-dir build/native-tests --output-on-failure   # expect 11 of 11, one target Disabled
> ```
> If the system lacks `zlib.h`, build zlib 1.3.1 locally and pass `CMAKE_PREFIX_PATH`. Run one test binary
> at a time: concurrent instances share `/tmp` paths and corrupt each other's results.
>
> **Report format:** the exact commit SHA tested; a table of gates with PASS/FAIL/BLOCKED; raw evidence
> lines, not summaries; what you could not run and why. Never fix a failure by disabling a test, weakening
> an assertion or widening a diff — if you must disable something, name it, justify it in a comment beside
> it, and say so in the report.

### 5.1 M0 — Foundation and module boundary

> **1. Exact mission.** Create the Neural Core boundary without changing any runtime behaviour. Publish
> freeze **F-0**: the `afneural.h` C ABI skeleton, the `procedural/` versus `generated/` package split, the
> `AF_ENABLE_NEURAL` CMake option, the **protected-component declaration** (§3.M.1), the **Android-first
> placement declaration** (§3.K) and `docs/NEURAL_CORE_BOUNDARY.md`.
>
> **2. Repository and branch.** `davealone69-gif/3d-mega-hub`, a new branch cut from
> `integration/avatarforge-renderer-port` **after PR #17 lands**. Target: a pull request into that trunk,
> merge commit, no squash.
>
> **3. Files and interfaces involved.** New: `avatarforge-engine/src/main/cpp/include/afneural.h`;
> `avatarforge-engine/src/main/java/com/davemega/hub/avatarforge/neural/procedural/` and `.../neural/generated/`
> (each with a `package-info` or README stating its rule); `docs/NEURAL_CORE_BOUNDARY.md`. Modified:
> `avatarforge-engine/src/main/cpp/CMakeLists.txt` (add `AF_ENABLE_NEURAL`, **default OFF**),
> `avatarforge-engine/build.gradle.kts` only if the option needs surfacing. **Not modified:**
> `include/afcore.h`, `avatarforge_jni.cpp`, `AvatarSpec.kt`.
>
> **4. Dependencies.** None. Wave 0, parallel with M8. Read §3.0 (placement) and §3.H.1 (separation rules)
> first.
>
> **5. Acceptance criteria.** `afcore.h` untouched; the 45-float JNI contract untouched; the
> **protected-component list of §3.M.1 is committed as machine-readable data** (not prose) with a test that
> fails if a future change touches a protected path without a human-authored commit; the **§3.K placement
> matrix is committed** stating, per capability, whether it runs on the phone, on a workstation, or in CI;
> `static_assert(33 + 1 + 3 + AF_FACE_PARAM_COUNT == kSpecFloatCount, "spec packing drifted")` still
> compiles; `afneural.h` uses the same `struct_size` versioning pattern as `af_spec`; with
> `AF_ENABLE_NEURAL=OFF` the built libraries are **byte-identical** to the pre-mission build; a test asserts
> that nothing in `procedural/` imports from `generated/`; no existing test changes.
>
> **6. Required build and runtime evidence.** Host `ctest` **11 of 11** with
> `generation_contract_regression` reported *Not Run (Disabled)*, exit status 0. Two builds — option OFF and
> ON — with SHA-256 of `libafcore.so` and `libafrender.so` from the OFF build matching the pre-mission
> build. `gh workflow run android.yml --ref <branch>` fully green; quote the run id and the per-job
> conclusions. The import-isolation test's output.
>
> **7. README handover requirements.** Add a short *Neural Core* subsection under the current handover:
> what the boundary is, that `AF_ENABLE_NEURAL` defaults OFF, that no behaviour changed, the exact head
> SHA, and the CI run id. State explicitly that results belong to that head only.
>
> **8. Explicit exclusions.** No provider code, no inference, no network, no asset inspection, no metrics,
> no models, no datasets, no third-party dependency of any kind. **No model weights may ever be loadable
> into the app process** (§3.K consequence 1) — declare that constraint in the boundary document now, while
> it is free to declare. Do not implement any later mission's
> interface beyond the empty declaration. Do not touch `:app`. Do not enable the option by default.

### 5.2 M8 — Device verification and CI gate repair

> **1. Exact mission.** Convert this repository's `IMPLEMENTED IN SOURCE` claims into `REAL + VERIFIED`, and
> attribute, fix or obtain a waiver for the failing generated-avatar runtime gate. **Runs from wave 0 and
> continuously — it is on no critical path but gates the credibility of every other mission.**
>
> **2. Repository and branch.** As §5.0. This mission needs a **real Android toolchain**: a device or
> emulator with JDK, Android SDK, NDK and (for the emulator) KVM. A host-only workspace cannot complete it.
>
> **3. Files and interfaces involved.** Start from `docs/ANDROID_TEST_AGENT_PROMPT.md` — the standing brief,
> with toolchain values, ordered commands, the verbatim `Avatar LOD selected:` log format and the pass
> criteria. Also: `.github/workflows/android.yml`, `avatarforge-generated-runtime.yml`,
> `publish-phone-test-apk.yml`; `app/src/androidTest/.../GeneratedAvatarRuntimeTest.kt`;
> `FilamentPreviewView.kt`; `src/render/af_lod_select.h`; and `kotlin-compile-gate.yml`, which exists on
> **`arena/25bd4008-3d-mega-hub` only**.
>
> **4. Dependencies.** None — start immediately. **M3 needs at least one device record from this mission**,
> and M7 needs its device measurements, so produce those early and publish them even before the gate is
> resolved.
>
> **5. Acceptance criteria.** Four jobs, in order:
> *(a) Attribute and resolve the runtime gate.* It fails on trunk head `9baf7c67` (run 45779175991) and on
> the merged line (`bdaef4b5` run 45777317839, `ba38d996` run 45789457244), always at step 8 *Run
> generated-avatar instrumentation test*, and passes on lead head `758bcfb4` (run 45779172779). Note that
> the only real phone measurement this project has — 53.8 FPS at 1,100,168 triangles, native heap 304–306 MB,
> APK of `797d156` — **predates the LOD work entirely**, so it is a baseline to beat, not evidence about the
> current line (§3.L.3). The historical assertion is
> `GeneratedAvatarRuntimeTest#generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView`: *"no
> drawable EGL-presented model frame within 15 seconds"*, `frames=2`, `draws=29`, `triangles=1626176`,
> `glError=0x0`, `eglError=0x0`. Get the **actual** log on hardware you control and state whether the
> screen-density LOD selection changes `drawnTriangles` and the frame count. **Do not assume it does.** The
> same file records this test failing on one commit and passing on the same commit in another workflow
> because a PixelCopy capture timed out, so **distinguish flake from fault by repeating the run**, not by
> reasoning about it. The verdict must be PASS, FAIL-with-cause, or WAIVED-BY-OWNER, with evidence.
> *(b) Observe the LOD fix on a real GPU.* Sweep the orbit camera ~1 m → 3 m → 12 m in Avatar Studio and
> capture the log lines. Pass means `measured=1`, level not pinned at 0 at default framing,
> `drawnTriangles` materially below `sourceTriangles`, the chosen level's `pixelEdge` ≥ 4.0 (or it is the
> finest level), no flapping at fixed distance, and **no cracks, detached hair or scalp, or broken
> skinning/morphs**. For orientation only, computed for a 1080×2340 viewport at 32° vertical FOV with
> ~1.1 M source triangles: world-per-pixel at 3 m is `7.352446e-4 m`, projected edges ≈2.6 px (LOD0),
> ≈7.6 px (LOD1, ~132k), ≈11.8 px (LOD2, ~55k), so expect LOD1 at 3 m, LOD0 near 1 m, LOD2 near 12 m.
> **Report your measured numbers, never these.**
> *(c) Measure the three resources the 2026-11-15 phone test explicitly did not.* That record measured a
> 38-second session and states plainly: **"Heat, battery, sessions longer than 38 s: Not measured —
> UNVERIFIED."** Produce a sustained session and record `PowerManager.currentThermalStatus` transitions
> (via the existing `DeviceResourceGovernor`), battery drain over a stated interval, and native heap over
> time. Also record the device facts this plan depends on: SM-S921B, Exynos 2400, Xclipse 940, 8 GB RAM,
> 1080 × 2340, 120 Hz, Android 17 — and confirm the app's own GPU report still reads
> `ANGLE ((Samsung Xclipse 940) on Vulkan 1.3.304)`, because **GLES on this phone is a translation layer
> over Vulkan** and every GPU assumption in this plan rests on that (§3.L).
> *(d) Rescue `kotlin-compile-gate.yml`* onto the trunk. It exists on no other branch and is exactly the
> gate that would have caught a real compile break automatically: a merge once deleted a helper while
> keeping its call site and reported **no conflict**.
>
> **6. Required build and runtime evidence.** For every result: the exact commit SHA, the device model,
> Android version and GPU. Raw log lines, not summaries. Repeated-run counts wherever flake is a
> possibility. The workflow run ids. Whether the same-SHA APK was inspected, and its SHA-256. If a job could
> not be run, say which and why — an absent measurement is `not measured`, never an inferred pass.
>
> **7. README handover requirements.** Update the exact-head evidence record with the new head's results,
> and **state which earlier results are now superseded** — a result never transfers to a different commit.
> Record the runtime-gate verdict and its attribution, the observed LOD behaviour with device identity, and
> that `kotlin-compile-gate.yml` now runs on the trunk. Keep the standing rules visible: an emulator pass is
> never a Samsung pass, and readiness stays **NOT READY** until the production generated-GLB-to-GLES
> SurfaceView test passes on one exact head with all required Android jobs green and the same-SHA APK
> inspected.
>
> **8. Explicit exclusions.** Do not fix the gate by weakening, skipping or disabling the test, or by
> widening a timeout to make it pass — that hides the defect this mission exists to find. Do not "resolve"
> the Filament `sheenColorIndex` `PreconditionPanic` by suppressing it. Do not publish an ungated APK
> (`publish-phone-test-apk.yml` requires `commit` and `expected_apk_sha256`, and its readiness gate must
> stay in front of publication). Do not claim a phone result from an emulator, or a Samsung result from any
> other device. Do not attribute a failure to a commit without an A/B run against a second head.

### 5.3 M1 — Neural Gateway and the zero-provider guarantee

> **1. Exact mission.** Build the provider-independent intelligence interface (§3.A, §3.A.1) **and prove the
> zero-provider guarantee** (§3.G). Publish freeze **F-5**.
>
> **2. Repository and branch.** As §5.0. One branch, one PR into the trunk.
>
> **3. Files and interfaces involved.** New, under `avatarforge-engine/.../neural/`: `NeuralProvider`,
> `ArtifactProvider` (**separate interface** — artifacts are binary, not text), `NeuralCapability`,
> `ArtifactCapability`, `ArtifactReceipt`, `NeuralRequest` / `NeuralResult` with the five task kinds
> (`SPEC_FROM_PROMPT`, `ASSET_DIAGNOSIS`, `ANIMATION_CRITIQUE`, `TEXTURE_CRITIQUE`, `MOTION_EVALUATION`),
> `CapabilityStatus` with the `DegradationReason` enum, `InferenceScheduler`, `ModelOutputValidator`,
> `Proposed<T>` (§3.H.1 rule 2), and providers for Ollama, OpenAI-compatible and OpenRouter. Modified:
> `app/src/main/java/com/davemega/hub/MainActivity.kt` — the AI page becomes a **console over the gateway**
> showing task kind, provider chosen, validated result and which fields reached `af_spec`; the chat
> transcript stays. Read first: `MainActivity.kt`'s existing Ollama client (`GET {base}/api/tags`,
> `POST {base}/api/chat`, defaults `http://localhost:11434` / `llama3.2:1b`) and `OllamaEndpoint.kt`, and
> report exactly what exists before changing it.
>
> **4. Dependencies.** **F-0** (M0). Reuses `DeviceResourceGovernor` for thermal back-off — do not write a
> second resource sampler. No dependency on M2 or M3.
>
> **5. Acceptance criteria.** Two or more providers behind one interface; a new provider needs no engine
> change. With **zero providers configured**: generation is **byte-identical** to the pre-mission golden
> SHA-256, asset inspection verdicts unchanged, and **no network call is attempted** — proven by pointing
> the HTTP layer at an unreachable host and still succeeding. Fallback order ends at
> `af_engine_analyze_prompt`, treated as success, not error. `ModelOutputValidator` bounds-checks against
> `af_spec`'s real limits and honours the adults-only rule; out-of-bounds output degrades to
> `OUTPUT_REJECTED_BY_VALIDATOR` and falls through. `Proposed<AvatarSpec>` is not assignable to
> `AvatarSpec`. No inference on the render thread. No key in the repository, any log, or any diagnostic
> report. `LICENCE_BLOCKED` is a working degradation reason.
>
> **6. Required build and runtime evidence.** The **Tier-0 CI job** (§3.G.4) added to `android.yml` and
> green, with all four assertions visible in its output. JVM unit tests for routing, fallback, validation,
> thermal back-off and degradation reasons — a fake provider is legitimate here as a *test* double, not a
> shipped mock. Both golden SHA-256 values (before and after) quoted. `ctest` 11 of 11 on host. CI
> `android.yml` fully green including lint and R8; run id quoted. Live-model behaviour is **UNVERIFIED**
> unless you ran a real Ollama — then name the model, its version and the host.
>
> **7. README handover requirements.** Record: the provider interface and how to add one; the tier table
> from §3.G.1; the Tier-0 guarantee **with the CI job name and run id**; what changed in the AI page; that
> paid providers are opt-in and never required; and every status word, including that no live model was
> exercised unless it was.
>
> **Also map `ArtifactReceipt` field-for-field onto `af_asset_provenance`** (§3.I rule 5): bytes, SHA-256,
> declared licence, provider and model revision already correspond, so an artifact arrives with provenance
> attached rather than needing it reconstructed. Set `licence_verified=1` **only** for a licence read from a
> primary source per §6.3.
>
> **8. Explicit exclusions.** No image or 3D generation *implementation* — only the `ArtifactProvider`
> interface and one fake for tests. **No model weights may be loaded into the app process** — on-device text
> inference means a localhost server the user runs, whose weights live in its own memory (§3.K consequence
> 1). No model download, no weights in the repository or APK. No on-device
> inference claim. No change to the solver, `af_spec`'s meaning, or the deterministic parser. Do not make
> any feature require a provider. Do not add a paid default.

### 5.4 M2 — 3D Intelligence: asset inspection and provenance

> **1. Exact mission.** Promote the four existing Python probes into an in-engine asset diagnostic API
> (§3.B), **and implement asset provenance** (§3.I). Publish freeze **F-1**.
>
> **2. Repository and branch.** As §5.0.
>
> **3. Files and interfaces involved.** Read first: `tools/glb_anatomy_probe.py` (338 lines),
> `tools/glb_animation_probe.py` (352), `tools/glb_quality_targets_probe.py` (531),
> `tools/glb_symmetry_sections.py` (190), and `avatarforge-engine/src/main/cpp/src/af_mesh.h`, which already
> provides `Bvh::build(mesh, leafSize=8)`, `Bvh::raycast`, `Bvh::signedDistance` and a 13-value `Region`
> enum. **First deliverable is a mapping table: probe field → engine source.** New: `af_asset_inspect` and
> `af_asset_report` **and `af_asset_provenance`** in `afneural.h`; implementation under `cpp/src/neural/`; host test
> `cpp/src/test/cpp/asset_inspect_test.cpp` registered in CMake. Reuses `af_glb_read` for parsing.
>
> **4. Dependencies.** **F-0**. Parallel with M1. No dependency on M3.
>
> **5. Acceptance criteria.** `af_asset_inspect` **reproduces `glb_quality_targets_probe.py`'s numbers on
> the same file**. Verdicts are codes, not sentences: `AF_ASSET_OK`, `AF_ASSET_FOREIGN_SKELETON_STATIC`,
> `AF_ASSET_TEXTURE_OVER_BUDGET`, `AF_ASSET_EXTENSION_UNSUPPORTED`, `AF_ASSET_NO_ANIMATION`. Morph targets
> report `AF_MORPH_NOT_READ_ON_IMPORT` — verified 2026-11-26: the importer contains zero occurrences of
> `morph`, while the *exporter* does write them (§1.3 row 6). Reporting `supported`, or even
> `parsed_but_not_applied`, would be a false claim.
> **Provenance is complete for every asset** (§3.I): `licence` and `generation_method` are **never empty**,
> and an asset that cannot state them is **quarantined, not displayed**. Generated assets carry
> `generator="af_engine_run_all"`, `generation_method="procedural"` and the exact `commit`; imports record
> **what the file declares, not what we hope** — a GLB with no `extras` gets `licence="UNKNOWN"` and
> `licence_verified=0` and is flagged in the report. Provenance is written into the GLB `extras` on export,
> read back on import, and an asset whose provenance cannot be read back is treated as `UNKNOWN`. **Reuse the
> existing `extras` block that `afcore.h:390` already documents — do not invent a sidecar format.**
> Every field has a unit and a source; anything unimplemented reports `not measured`,
> mirroring the probes' existing `unsupportedMeasurements`. No new part taxonomy beside `Region`; no
> re-implementation of a measurement the probes already make.
>
> **6. Required build and runtime evidence.** Run the Python probe yourself against
> `app/src/androidTest/assets/rigged_test_figure.glb` and **quote your own output** — do not trust the
> reference values below blindly. Reference from the run that rescued this probe: glTF 2.0, 240 vertices,
> 120 triangles, 4 primitives, 4 materials, 1 skin, 7 joints, max joint depth 3, root `Hips`, clips Wave
> 2.000 s / March 1.000 s / Bow 2.000 s, 0 morph slots, bounds 0.62 × 1.72 × 0.31, estimated height 1.72.
> Then the C++ test's output asserting the same values. `ctest` **12 of 12** (the new test added), with
> `generation_contract_regression` still Disabled. CI green; run id quoted.
>
> **7. README handover requirements.** Document `af_asset_inspect`, the verdict codes, which measurements
> are implemented and which report `not measured`, the probe-equivalence evidence with both outputs quoted,
> and the exact head.
>
> **8. Explicit exclusions.** No AI, no provider, no network. Do not add Draco, meshopt, KTX2, assimp or
> Eigen — zlib is the engine's only third-party C++ dependency and adding one is an owner decision. Do not
> implement morph-target *application* on import (that is a separate capability, currently MISSING). Do not
> modify `af_glb_read.cpp`'s extension policy.

### 5.5 M3 — Evaluation harness, golden records and the benchmark suite

> **1. Exact mission.** Build §3.E: one `EvaluationRecord` type with provenance, reconciling measurements
> the repository already takes — **plus the §3.J benchmark suite and its two controls**, which are what make
> every later "no regression" claim meaningful. Publish freeze **F-2**. **This is the pivot mission — five missions wait on
> it (§4.5).**
>
> **2. Repository and branch.** As §5.0. **One owner, not split across agents** (§4.6 rule 2).
>
> **3. Files and interfaces involved.** Read first: `RendererTelemetry.kt` (≥20 fields including fps and
> frame-interval p50/p95/p99, frames presented / not presented / without valid content, stall events, EGL
> shutdown error), `EvidenceIdentity.kt`, `DiagnosticDownloads.kt`, `DeviceResourceGovernor.kt`,
> `avatar_lod_select_test.cpp`, `animation_roundtrip_test.cpp` (already compares imported playback to the
> generating engine at **0.2 mm**), `imported_glb_pbr_test.cpp`, `generation_contract_test.cpp`. New:
> `EvaluationRecord`, `Measurement`, `Verdict`, `MetricKey` registry, `Provenance`, the tolerance table, and
> golden-record storage under `.../neural/procedural/`.
>
> **4. Dependencies.** **F-1** (M2) for the geometry and material metric families, plus **at least one
> device record from M8**. Depends on nothing in M4–M6.
>
> **5. Acceptance criteria.** The **§3.J.1 benchmark suite exists**: a frozen subject set (the
> deterministic **seed 1337** already used in this repository's evidence, one subject per gender, one per
> fabric in `kFabrics`, one per iris colour in `kEyeColours`, one minimal and one maximal-complexity spec),
> the three repository assets, all seven metric families, and host + CI-emulator contexts. **Both controls
> run in CI and both pass: the null control** (an unchanged revision produces **zero** metric differences and
> byte-identical outputs — any non-zero result means the harness is noisy and must be fixed before any
> verdict is trusted) **and the negative control** (a deliberately worsened constant **is detected and
> named** — if it is not, the suite has a blind spot). A record missing `commit` or `device` **fails to construct** — enforced by the
> type, not by review. Every metric family in §3.E.2 is sourced from an existing mechanism named in that
> table, with no parallel re-measurement. Verdicts are the §3.E.4 codes. Tolerances are declared per metric
> with unit and source, and must include the repository's existing precedents: symmetry **0.004 model
> units**, animation round trip **0.2 mm**, visible pixels **≥5,000**. LOD evidence is captured from the
> real log lines: `Avatar LOD selected: level=%d of %d sourceTriangles=%d drawnTriangles=%d pixelEdge=%.3f
> target=%.3f distance=%.3fm viewportH=%d fov=%.1f measured=%d` and `Avatar LOD ready: levels=%d
> sourceTriangles=%d lod1Triangles=%d lod2Triangles=%d`. The magenta debug-shader path deliberately disables
> LOD, so it is recorded as *not* LOD evidence.
>
> **6. Required build and runtime evidence.** A golden record for the deterministic **seed 1337** generated
> avatar on **host**, and one from the **CI emulator**, with both `commit` and `device` populated and
> **different** — quote both records. A demonstration that changing one `af_spec` field produces a
> measurable diff against the golden record, with the metric, both values and the tolerance. A written
> statement of which metrics are host-only, which need a device, and which are `not measured` today. `ctest`
> green; CI green including the emulator suites; run ids quoted.
>
> **7. README handover requirements.** Document the record schema, the metric registry, the tolerance table
> with each tolerance's source, where golden records live, and the rule that a record never transfers to a
> different commit or device. Include both example records verbatim.
>
> **8. Explicit exclusions.** No AI grading of AI output — evaluation is procedural only (§3.H.2). No new
> telemetry sampling where `RendererTelemetry` already reports it. No device claims from emulator data. No
> change to the solver or the renderer. Do not weaken an existing assertion to make a metric pass.

### 5.6 M4 — Human Intelligence critics

> **1. Exact mission.** Build proportion, face and skin critics that **measure** rather than describe
> (§3.C), emitting into M3's records.
>
> **2. Repository and branch.** As §5.0.
>
> **3. Files and interfaces involved.** Read first: `af_anatomy.cpp/.h` (spec → `BodyMetrics`: landmark
> heights and girths; a tape measurement in the spec is met exactly), `af_field.cpp/.h` (the single SDF for
> body, head, face, hands, feet, `hairShape`, `clothUpper`, `clothLower`, `upperShell`, `skirtShell`,
> `shoeShape`, `gearField`), `af_rig.cpp` (71 joints, 52 ARKit blendshapes as analytic displacements),
> `af_texture.cpp` (skin with subsurface, sheen, sheen tint, anisotropy, IOR), `af_prompt.cpp`
> (`kFabrics[]`, `kEyeColours[]`), and `docs/PHOTOREAL_UPGRADE_PLAN.md`, `docs/AVATAR_GEOMETRY_EVIDENCE.md`,
> `docs/AVATAR_QUALITY_PLAN.md`, `docs/DONOR_BASELINE.md`. New: critics under `.../neural/procedural/`,
> reference constants as data, host tests.
>
> **4. Dependencies.** **F-2** (M3). Parallel with M5 and M6. No dependency on M1 — critics are procedural
> and must work at Tier 0 (§3.G.2).
>
> **5. Acceptance criteria.** Three critics (proportions, face, skin) each emitting **≥5 measurements** with
> units into an `EvaluationRecord`. No adjectives in a measurement. The reference population is **named and
> cited** — WHO growth standards via Anny's calibration are the researched recommendation (§6.3.2) — not an
> invented ideal. Known reference points already in this repository's documents: symmetry tolerance **0.004
> model units**; a generated body is **1,148,034 triangles** where a Daz Genesis 8 body skin is **27,292**;
> textures are **one shared 2048² atlas** against **four 4096² maps**. Generated bytes must not change
> unless a golden-record diff is approved by the owner. The adults-only invariant still passes.
>
> **6. Required build and runtime evidence.** A baseline `EvaluationRecord` for the deterministic seed with
> all critic measurements populated, quoted verbatim. `ctest` green including the new critic tests. CI
> green; run id quoted. For any external reference: the licence read from its **own repository**, recorded in
> `THIRD_PARTY_NOTICES.md` before use, with the four-question report required by §5.0 rule 10.
>
> **7. README handover requirements.** Document each critic, its measurements and units, its reference
> population with citation and verification date, the baseline values, and what each measurement does and
> does **not** prove.
>
> **8. Explicit exclusions.** **Do not change the solver** in this mission (P1). Do not integrate Anny's
> Python/PyTorch code into the engine — it is authoring/CI-time only and cannot run on `minSdk 26`. Do not
> use the SMPL family, or any HMR model that outputs SMPL parameters (§6.3.3). Do not vendor MakeHuman
> **code** — its source is AGPL-3.0; only its **assets** are CC0 (§6.3.2). Do not relax the adults-only rule
> to improve a metric. Do not add hair-card geometry or a new texture pipeline here — those are separate
> missions with separate gates.

### 5.7 M5 — Animation Intelligence

> **1. Exact mission.** Third-party skeleton adoption, foot-contact and slide detection, IK (§3.D), **and
> morph-target application on import** (owner instruction 6, 2026-11-26 — see acceptance criterion **(f)**) —
> **beginning with a written API decision, not code.** Publish freeze **F-3**.
>
> **2. Repository and branch.** As §5.0. The decision document and the implementation may be one PR, but the
> decision must be committed and reviewed **before** implementation starts.
>
> **3. Files and interfaces involved.** Read first: `afcore.h`'s import contract, which currently states a
> skeleton that is not this engine's rig is shown **static** with joint count 0 and its animation channels
> **not applied**, warned via `AF_LOG_WARN`; `af_rig.cpp` (71 joints, clip library); `af_anim_player.cpp`;
> `af_retarget.cpp`; `af_engine_evaluate_animation` (writes one column-major 4×4 skin matrix per joint; the
> renderer blends bind-pose vertices on the GPU); Kotlin `animation/` (`AnimationLibrary`,
> `AnimationPlayback`, `FaceAnimation`, `LocomotionStateMachine`); `animation_roundtrip_test.cpp`; and
> `af_anatomy`'s `BodyMetrics` landmarks. For criterion **(f)** also read `af_glb_read.cpp` — it contains
> **zero occurrences of `morph`** today — the sparse-patch application already present at `:427-467` (a
> working precedent for reading sparse accessors), `afcore.h:390` (the exporter **does** write morph targets)
> and `afcore.h:413-414` (the import contract that must be amended). Recover the donor specification with
> `git show 58aced02:avatarforge-engine/src/test/cpp/imported_animation_test.cpp` (949 lines, 94 checks, 85
> of which call the absent API).
>
> **4. Dependencies.** **F-1** (M2) for skeleton, joint-depth and skin-weight inspection, and **F-2** (M3)
> for the contact and slide metrics. Parallel with M4 and M6.
>
> **5. Acceptance criteria.**
> *(a) The decision, first.* Two incompatible imported-animation APIs exist: this line's
> `af_engine_animation_count/name/duration/info`, `af_engine_import_motions`,
> `af_engine_import_motions_from_memory` with `af_motion_import_report`; and `arena/089497bd`'s nine
> `af_engine_imported_*` functions (advance, clip_count, clip_duration, clip_name, is_playing, pause, play,
> resume, seek). **Choose one, write down why, and record what the other would have cost.** Implementing
> both is how this repository already acquired two animation systems.
> *(b) Foot contact and slide.* Needs **no new engine capability**: skin the rest mesh with
> `af_engine_evaluate_animation`, track ankle and toe landmarks from `BodyMetrics`, report contact intervals
> and slide distance **in millimetres** into an M3 record. Represent contact as a per-frame **F×4 binary
> array** — left heel, left toe, right heel, right toe. **Reimplement this independently**: the published
> reference (`davrempe/contact-human-dynamics`) is under the **Adobe Research License, non-commercial only**
> and must not be vendored in any form (§6.3.3).
> *(c) Retargeting quality.* Joint-angle error, end-effector drift and penetration, measured. Apply the
> verified practices from §3.C.1: normalise **both** rigs to a canonical T-pose before computing the mapping
> matrix, or rest-pose offsets corrupt the result silently; smooth per channel with an EMA or one-euro
> filter after pose lifting.
> *(d) IK* for foot placement and look-at, only once (b) and (c) can measure whether it helped.
> *(e)* `animation_roundtrip_test` still passes at its existing **0.2 mm** tolerance.
> *(f) Morph-target application on import — ASSIGNED HERE by owner instruction 6, 2026-11-26.* Previously
> deferred by this mission's own exclusions and left unowned; it is now an M5 deliverable, because M5 already
> owns the imported-asset animation capability and facial animation on an imported face is impossible without
> it (§3.D item 6). Verified state: `af_glb_read.cpp` contains **zero occurrences of `morph`**, while
> `afcore.h:390` confirms the exporter writes sparse morph targets and `afcore.h:413-414` documents the import
> side as unsupported — the two directions are asymmetric. Scope: read sparse morph targets on import and make
> them drivable, update `afcore.h`'s import contract to match reality, and add a host test that round-trips a
> generated avatar's blendshapes through export → import → evaluate. **This is an addition to M5; acceptance
> criteria (a)–(e) are unchanged** (owner instruction 8).
>
> **6. Required build and runtime evidence.** The decision document, committed. For **(f)**: the new
> export → import → evaluate morph round-trip test passing on the host, with its measured tolerance quoted,
> plus the corrected line in `afcore.h`'s import contract, plus `grep -ci morph af_glb_read.cpp` returning a
> **non-zero** count (it is **0** today — that single number is the proof the gap closed). Foot-slide reported in
> millimetres on a **named clip**, with `commit` and `device` recorded in an `EvaluationRecord` — quote the
> record. External practice applies IK correction above ~1 cm/frame, but **that number is not this
> repository's policy**: measure your own baseline, report it, and justify any threshold you adopt.
> Validation gates: BVH round-trip joint positions within **1e-4** if BVH is touched, and no NaN transforms
> after retargeting. `ctest` green including the new tests; CI green including the emulator animation
> suites; run ids quoted.
>
> **7. README handover requirements.** Record the chosen API and the rejected alternative with its reason;
> the contact and slide metric definitions with units; the measured baseline; the status of third-party
> skeleton adoption (it moves from MISSING only when a test proves it); and the standing note that
> `arena/089497bd-3d-mega-hub` **must not be deleted** until the fixture and licence questions are settled.
>
> **8. Explicit exclusions.** **No motion foundation model — MotionGPT, MDM or similar. This is BLOCKED on
> licence, not deferred** (§3.D item 8): their code is MIT but they train and evaluate on **HumanML3D**,
> whose own README states the data comes from AMASS and may not be redistributed, and which requires the
> non-commercial **SMPL+H and DMPL** body models. Generate motion **procedurally** from the engine's own
> verified clip library. Do not use AMASS, SMPL-family models, or anything from
> `contact-human-dynamics`. **AIST++** is reported CC BY 4.0 and is the only free motion-data candidate
> found, but it is **UNVERIFIED** (not reachable from github.com) — if you want it, verify its licence from
> its own host first and raise it as an owner decision. Do not vendor the donor's five CC BY 4.0 fixtures
> (~596 KB, including Khronos `CesiumMan` and `Fox`) into the APK without an owner decision on the
> attribution obligation.
>
> **Amended by owner instruction 6, 2026-11-26.** Morph-target application on import is no longer deferred
> out of this mission — it is deliverable **(f)** above, and with it facial animation on imported faces comes
> into scope **once (f) passes**. The ordering is the whole point of the assignment: build (f) first, then
> drive the 52 ARKit channels on an imported face. Do not attempt facial animation on imported faces before
> (f), and do not report morph targets as supported until (f)'s test passes.

### 5.8 M6 — Learning memory, revision gate and recovery

> **1. Exact mission.** Build §3.F.1–F.6 (experience memory, approved feedback, experiment history, golden
> quality measurements, regression detection, dataset management), **the §3.J revision gate, the §3.M
> autonomous-improvement guards, and §3.N recovery**. Publish freeze **F-4**.
>
> **2. Repository and branch.** As §5.0.
>
> **3. Files and interfaces involved.** Read first: `af_prompt.cpp`'s `kFabrics[]` and `kEyeColours[]`;
> `generation_contract_test.cpp` **Part E**, which asserts a fabric through the parser, the live material
> table, the exported GLB bytes and the re-import round trip; and the `THIRD_PARTY_NOTICES.md` entry
> *"Clothing-fabric PBR values and iris swatches (3D4ME-derived constants)"*. **That is the pattern to
> generalise — do not invent a new one.** New: `Experience`, `Decision`, `ExperimentRecord`, golden-record
> promotion and comparison, the dataset manifest, all under `.../neural/procedural/`. Storage: app-private
> on device, exported JSON off device, reusing `DiagnosticDownloads.kt`'s existing path.
>
> **4. Dependencies.** **F-2** (M3) — golden records and regression detection are meaningless without the
> evaluation schema. Parallel with M4 and M5. **M9 depends on this mission's F-4.**
>
> **5. Acceptance criteria.** Experience memory is append-only, content-addressed by subject hash, bounded
> by a ring buffer whose eviction **keeps every regression and every human decision**. Approved feedback
> stores **rejections with the same weight as approvals**, each with a reason and the measurements visible
> at the time. Experiment history answers *"has this been tried?"* for a given hypothesis. Regression
> detection re-measures golden subjects and reports metric, both values, tolerance and both commits — and
> **prefers byte identity where it holds**, which is the strongest check this repository already has
> (`male=73,399,512 B`, `female=72,076,316 B` reproduced identically across the `42fa78a` ports). Detection
> **never auto-reverts**. Dataset management keeps all external data out of the repository and the APK.
> **One accepted improvement lands end to end** as data plus a contract test plus provenance, following the
> `kFabrics` pattern exactly — **and one rejected proposal is recorded with its measurements.** A memory
> that can only record acceptances is not a memory.
>
> **Additional acceptance criteria from the owner's approval.**
> *(§3.J revision gate)* No learning-system revision — constant table, golden record, adapter manifest or
> critic threshold — may land without the four §3.J.2 assertions: no metric beyond tolerance; **at least one
> named measured improvement** (a revision that changes nothing measurable is rejected, not merged);
> byte-identity where the revision claims not to affect generation; and complete provenance. Both §3.J.3
> controls must pass.
> *(§3.M autonomous improvement)* Neural Core may **propose, test and compare** but has **no write access**
> to the §3.M.1 protected set — `af_field`, `af_isosurface`/`af_body`, `af_uv`, `af_texture`, `af_rig`,
> `af_glb`/`af_glb_read`, `src/render/*`, `af_spec`'s meaning and the 45-float contract, the adults-only
> invariant, and the UI label `Run generated-avatar GLES diagnostic`. Proposals are evaluated in **shadow**,
> in parallel with the live configuration on the same subjects in the same run, so nothing is mutated to test
> them. **There is no automatic apply path** — a human decides. A single **kill switch** disables all
> proposal generation and adapter activation, and because the protected set is untouched by design it has
> nothing to undo. Enforce this with a test, not a comment.
> *(§3.N recovery)* Export one **backup bundle** through the existing `DiagnosticDownloads.kt` path
> (`Downloads/3D Mega Hub Diagnostics`) containing golden records, experiment history, decisions, active
> manifests and experience memory, with its own SHA-256 plus the `commit` and device it came from. **Restore
> is validated, not trusted**: schema check, hash check, and a dry run against the benchmark suite before
> anything becomes active; a restore that would regress a golden record reports it and needs explicit human
> confirmation; a restore never silently overwrites a newer record — conflicts are reported per record with
> both values and both timestamps. Demonstrate **all three rollback levels**: manifest repoint, git revert,
> and branch/commit restore from a recorded SHA. **No recovery test may delete real data** — use fixtures and
> temporary directories, following the `BoundedGlbReader` cache-directory precedent.
> *(§3.L budget)* Experience memory is a bounded ring buffer with a **byte cap** keeping Neural Core's added
> native heap within the **≤8 MB** allowance against the phone's measured 304–306 MB and the engine's
> 1,536 MB default budget. Assert the cap in a host test.
>
> **6. Required build and runtime evidence.** A demonstration that changing one constant is caught by
> regression detection against the golden record, with the report quoted. **A full §3.J.4 revision report**
> for one real revision: revision id, baseline, subjects and devices, per-family before/after with units, the
> named improvement, the null-control result, and the negative-control result **including which metric caught
> it**. A demonstrated backup → restore round trip with hashes. A demonstrated rollback at each of the three
> levels. A shadow-evaluation run showing a proposal compared without being applied. The measured byte size
> of the experience-memory cap. A demonstration that eviction
> preserves a regression experience. The accepted improvement's contract test passing through parser → live
> table → exported bytes → re-import, with the test output quoted. `ctest` green; CI green; run ids quoted.
> Both golden SHA-256 values before and after, proving no unintended change.
>
> **7. README handover requirements.** Document the three stores and where they live; how an improvement is
> proposed, approved and landed; how a regression is reported; the eviction rule; the dataset rules
> including that **the engine's own output is the primary free dataset** and why that is licence-clean
> (§3.F.6, §6.3.2); the accepted improvement and the recorded rejection, both with their measurements.
>
> **8. Explicit exclusions.** **No write access to any §3.M.1 protected component, ever** — not by code
> path, not by configuration, not by "temporary" experiment. No automatic apply. **No fine-tuning, no
> adapters, no model versioning — that is M9.** No online
> learning inside the app. No telemetry leaving the device. No self-modifying shaders, fields or prompts. No
> automatic activation of anything a human has not approved. No external dataset downloaded, vendored or
> packed. No weights in Git or in the APK. Do not let a learned value bypass the P2 validation boundary or
> the adults-only rule.

### 5.9 M7 — Optimisation and asset intelligence

> **1. Exact mission.** Reduce what a frame and an asset cost, **measured on a named device** (§1.3 row 18,
> `docs/PHOTOREAL_UPGRADE_PLAN.md` repairs R4–R8).
>
> **2. Repository and branch.** As §5.0. Each optimisation is its own commit with its own before/after, so a
> regression can be attributed to one change.
>
> **3. Files and interfaces involved.** Read first: `afcore.h:318-327` — `af_vertex` is **88 bytes**
> (pos 12 + normal 12 + tangent 16 + uv 8 + color 16 + joints 8 + weights 16) against a 32-byte target;
> `src/render/af_avatar_lod.cpp/.h` (three index-only levels, `kAvatarLodLevels = 3`,
> `kLodFractions = {0.12f, 0.05f}`); `src/render/af_lod_select.h` (`kAvatarLodPixelEdgeTarget = 4.0f`,
> `kAvatarLodHysteresis = 1.25f`); `src/render/af_render.cpp`'s `BudgetRenderer` tiers; `af_uv.cpp` (one
> shared 2048² atlas); `af_texture.cpp`.
>
> **4. Dependencies.** **F-1** (M2) for asset cost reporting and **F-2** (M3) for measurement. Also
> consumes M8's device evidence. Wave 4, parallel with M9.
>
> **5. Acceptance criteria.** A measured before/after for drawn triangles, frame-interval p50/p95/p99, and
> bytes per vertex, each recorded in an M3 `EvaluationRecord` with `commit` and `device`. **LOD indices are
> original vertex indices**, so skinning and morphs must be untouched: any crack, detached hair or scalp, or
> broken deformation at a coarser level is a **defect, not a saving**, and fails the mission.
> `BudgetRenderer` must never silently change the model — only MSAA, shadow-map size and bloom — which is
> why LOD selection is logged. **Every new `Renderer` virtual must be forwarded in `af_render.cpp`** or the
> app silently gets the base-class default. No regression in any golden record.
>
> **6. Required build and runtime evidence.** Device measurements on a **named device** (model, Android
> version, GPU) — a host-only number must be labelled host-only, and no phone result may be claimed from an
> emulator. The `Avatar LOD selected:` and `Avatar LOD ready:` lines verbatim. `ctest` green; CI green
> including the instrumented GLES smoke and the generated-avatar runtime gate, with the gate's status stated
> honestly whether it passes or fails; run ids quoted. Visual evidence that no crack or detachment was
> introduced, from the existing evidence-only screenshot jobs.
>
> **7. README handover requirements.** Record each optimisation, its before/after numbers with units and
> device, what it cost in quality if anything, and which `PHOTOREAL_UPGRADE_PLAN` repair it corresponds to.
> Update the plan's repair table rather than leaving a parallel record.
>
> **8. Explicit exclusions.** **Adding Draco, meshopt, KTX2, ASTC tooling, Eigen or assimp is an owner
> decision, not an agent's** — zlib is currently the engine's only third-party C++ dependency, and the
> repository has deliberately declined compression dependencies. Do not touch the Vulkan path
> (`af_vk.cpp` / `af_shaders_vk.h` are stale and not compiled). Do not change the deterministic pipeline's
> output without an approved golden-record diff. Do not optimise by reducing a measured quality tolerance.

### 5.10 M9 — Versioned models, optional local fine-tuning and rollback

> **1. Exact mission.** Build §3.F.7–F.8: optional local fine-tuning, adapter versioning, activation
> pinning and rollback. **This is the only optional mission** — the owner may cancel it without stranding
> any other.
>
> **2. Repository and branch.** As §5.0, plus a separate branch for any training harness, which lives
> outside the Android build entirely.
>
> **3. Files and interfaces involved.** Read first: §3.F.6 (dataset management), §3.F.8 (versioning rules),
> §6.3.2 (the verified free base-model list), and M6's `Experience` / `Decision` / `ExperimentRecord`
> schemas. New: adapter manifest format, activation pinning per task kind, rollback, and the validation gate
> that runs an adapter against golden records before it may be activated. Training harness: a **separate**
> tool, not part of `:app` or the engine build.
>
> **4. Dependencies.** **F-4** (M6) and **F-5** (M1) — an adapter improves a *proposer*, so the gateway and
> the validation boundary must exist. Also needs M3's golden records to validate against. Wave 4, parallel
> with M7.
>
> **5. Acceptance criteria.** Base models restricted to the §6.3.2 verified-free list, each with its licence
> read from its own repository and recorded. Training data restricted to §3.F.6 — **the engine's own outputs
> plus human-approved labels**. Output is a **LoRA-class adapter**, not a full model. Training runs on the
> owner's workstation, never on device. An adapter is **inert until a human approves it against golden
> records**. Weights are **never in Git and never in the APK** — Git holds the manifest, the SHA-256, the
> provenance and the evaluation record. Exactly one active version per task kind, pinned in a manifest, so a
> run is reproducible. **Rollback demonstrated at all three §3.N.2 levels**: manifest repoint, git revert,
> and restore from a recorded SHA — each showing behaviour returning to the earlier golden record. A **lost
> adapter weight file must be detected by its SHA-256 and refused**, not silently replaced; §3.N.4 is
> explicit that this project cannot recover it and that retraining is the only route, so say so in the
> handover rather than implying a backup exists. **With fine-tuning entirely disabled, nothing
> degrades** — the deterministic parser and curated constant tables remain the floor.
>
> **6. Required build and runtime evidence.** A before/after `EvaluationRecord` pair for at least one task
> kind, showing whether the adapter improved or regressed it, with `commit` and `device`. The manifest for
> the activated version and for the version rolled back to, both quoted. The approval decision and its
> reason. Proof that no weight file entered Git (`git log --stat` over the branch) or the APK (an APK
> listing). Proof that the zero-adapter configuration still passes M1's Tier-0 CI job.
>
> **7. README handover requirements.** Document the adapter lifecycle end to end: propose → train → validate
> against golden records → human approve → activate → monitor → roll back. State plainly that fine-tuning is
> **optional** and that disabling it changes nothing. Record which base models are permitted and their
> verified licences, and the rule that a new base model requires the four-question licence report first.
>
> **8. Explicit exclusions.** **No fine-tuning of the solver, the field, the mesh, the UV, the texture or
> the rig** — those are procedural and stay procedural (P1, §3.H.2). No training on any external dataset
> (§6.3.3 excludes every candidate found). No paid training service, no paid base model, no
> non-commercial-only weights. No automatic activation. No weights in the repository or the APK. No
> on-device training. Do not enable this by default — it is opt-in, and the default build must not require
> it.

## 6. External dependency and licence intelligence

Researched 2026-11-26 from external sources. **Every entry is UNVERIFIED against its primary source** and
carries the source's own disagreement where sources conflict. Nothing here may be vendored, depended upon
or cited in code until the licence text itself has been read and recorded in `THIRD_PARTY_NOTICES.md`
(`AGENTS.md` §2.7). This section exists because the obvious default choice in this field is the wrong one.

### 6.1 The blocking risk: permissive code, non-commercial models

The single most dangerous pattern for this project is a dependency whose **code** is MIT or Apache 2.0 but
whose **output** requires a restricted model file. Named examples from the research: HMR 2.0, TokenHMR,
SMPLer-X, ROMP, HybrIK, WHAM — all permissively licensed code that emits SMPL-family parameters. Converting
those parameters into a mesh needs the SMPL/SMPL-X/FLAME/MANO model files, which are **non-commercial
scientific research only**, single-user, **no redistribution**, commercial use via a separate Meshcapade
licence (pricing not public; secondary sources report anything from ~€1–5 per scan on a credit basis to
~€150,000 annually — both UNVERIFIED, and the disagreement is itself the reason not to plan around it).

**Rule for every mission:** a dependency's licence is the licence of *what it needs to run*, not of its
repository. An agent adding a human-mesh-recovery or motion model must report the licence of the model
files, not of the code, and must state whether those files would be redistributed inside the APK.

### 6.2 The compliant pattern already used in the field

Projects facing this exact constraint (research recorded `Corridor Tech/PoseCap`, decision ADR-0006) adopt:
**never distribute the body models** — not in the repository, not in the installer — and offer at most an
*optional, local-only loader behind a user-supplied path*, with the obligation surfaced to the user. This
is directly adoptable here and satisfies `AGENTS.md` §2.7 and rule 5 (nothing private packed into the APK)
at the same time. **Note that P9 is stricter than this pattern:** §6.2 describes how a project *may* legally
touch a restricted model, whereas P9 says this project will not. Recorded for completeness and because it
explains what other projects do, not as an option to take.

### 6.3 Dependency matrix — verified against primary sources

**Method (requirement 3 of the owner's brief: do not treat secondary-source licence claims as verified).**
On 2026-11-26 the licence of each candidate was read from its **own repository** via the GitHub API:
`GET /repos/{owner}/{repo}/license` for the detected SPDX id and path, then the raw licence file itself, and
for models whose weights are licensed separately from their code, the repository's own licence table. What
could not be read from a primary source is marked **UNVERIFIED** and is *not* relied upon.

Primary-source reading corrected **five** claims this document previously carried from secondary sources.
Those corrections are listed first, because they are the reason requirement 3 exists.

#### 6.3.1 Corrections to earlier secondary-source claims

| Candidate | Secondary sources said | Primary source says | Effect |
|---|---|---|---|
| `naver/anny` | "Apache 2.0" (blog, arXiv abstract, three aggregators) | GitHub licence detection returns **`NOASSERTION` / "Other"**; the `LICENSE` file itself opens *"Anny, Copyright (C) 2025 NAVER Corporation… The files within this subdirectory are licensed under the Apache License, Version 2.0"*. **Then the README's own per-component licence section was read**, which the secondary sources never mention | **Apache 2.0 CONFIRMED for the code**, and the assets are better than claimed — `data/mpfb2` and `data/faceunits01` are **CC0 1.0**, `data/soma` is Apache 2.0. **But the README also states the `smplx` and `smpl` topologies are "non-commercial use only"**, a restriction no secondary source reported. PyPI `anny` v0.6.2 declares **no licence at all** in its metadata |
| `TencentARC/InstantMesh` | "MIT" | **`Apache-2.0`** | Still free; the citation was wrong |
| `microsoft/TRELLIS` | "MIT" in one source, "Apache 2.0" in another | **`MIT`** | Conflict resolved by primary source |
| `davrempe/contact-human-dynamics` | Cited here as a reference for the F×4 foot-contact data shape, licence unchecked | **ADOBE RESEARCH LICENSE**: *"noncommercial research purposes only"*, expressly excluding *"commercial licensing or distribution, development of commercial products, or any other activity which results in commercial gain"* | **EXCLUDED.** This document had recommended it. Corrected: vendor nothing from it — see §6.3.3 |
| `Stability-AI/sd3.5` | "Stability Community License, free under $1M revenue" | The repository contains **`LICENSE-CODE` = MIT** and **no weights licence at all** — the community licence lives on HuggingFace, which is not reachable from this workspace | Code MIT verified; **the revenue cap is UNVERIFIED from any primary source**. Excluded under P9 pending verification, *without* repeating the $1M figure as fact |

Also corrected: the secondary claim that Hunyuan3D 2.1 has a "1M MAU threshold" does **not** appear in
sections 1–3 of the licence text actually read. That figure is withdrawn as unverified; the exclusion stands
on the territory clause, which *was* read verbatim.

#### 6.3.2 ACCEPTED — free to ship, licence verified

| Candidate | Licence (primary source) | Evidence read | Conditions and residual checks |
|---|---|---|---|
| **TripoSR** `VAST-AI-Research/TripoSR` | **MIT**, expressly covering weights | `LICENSE` (full MIT text, *"Copyright (c) 2024 Tripo AI & Stability AI"*, grants sublicence and sale) **and** `README.md` line 27: *"The model is released under the MIT license, which includes the source code, pretrained models, and an interactive online demo."* | **The strongest candidate in this matrix** — the only one whose weights are confirmed MIT in the repository's own words. Retain the copyright and permission notice (`AGENTS.md` §2.7). Cannot run on device (~4–8 GB VRAM) |
| **FLUX.1 [schnell]** `black-forest-labs/flux` | **Apache 2.0** for `[schnell]` | `README.md` licence table: `[schnell]` → `apache-2.0`, linking `model_licenses/LICENSE-FLUX1-schnell`; autoencoder weights also apache-2.0 | **VERIFIED 2026-11-26:** `model_licenses/LICENSE-FLUX1-schnell` was fetched and is the **full Apache License 2.0 text**, not merely a table entry. Note the repo root `LICENSE` is Apache-2.0 for *code*, which is exactly how the variants get confused — every `[dev]` variant is separately non-commercial |
| **Qwen-Image** `QwenLM/Qwen-Image` | **Apache 2.0** | GitHub licence API SPDX | Weights distribution to be confirmed on the model host |
| **TRELLIS** `microsoft/TRELLIS` | **MIT** | GitHub licence API SPDX | Heavy (24 GB+ reported). Confirm whether weights are separately licensed |
| **InstantMesh** `TencentARC/InstantMesh` | **Apache 2.0** | GitHub licence API SPDX | Confirm weights licence separately |
| **Anny** `naver/anny` | **Apache 2.0** code; default assets **CC0 1.0** | `LICENSE` read in full, **and** the README's own per-component licence section read in full: code Apache-2.0 (© 2025 NAVER Corp.); `data/mpfb2` (MakeHuman assets via MPFB2) = **CC0 1.0 Universal**; `data/faceunits01` (Face Units pack, Mika Suominen) = **CC0 1.0 Universal**; `data/soma` (from NVlabs/SOMA-X) = **Apache 2.0**. PyPI `anny` resolves to **v0.6.2** but its metadata declares **no licence field and no licence classifier**, so the README governs | **A trap lives inside Anny:** the README states the **`smplx` and `smpl` topologies are "non-commercial use only"**, downloaded separately from a NAVER host. Use `topology="anny"`, `"anny-quads"`, `"anny-full"`, `"makehuman"` or `"soma"` — **never `smplx` or `smpl`**. Two rigs: default `anny` = **104 bones**; `rig="makehuman"` = **163 bones**. Topology `"anny-full"` additionally **disables Anny's nudity-related mesh edits**, which intersects `AGENTS.md` §2.6 and is an owner decision, not an agent's. Python/PyTorch ⇒ **authoring/CI-time only, never on device** (`minSdk 26`). **This row certifies the licence, not an adoption — see §6.4: Anny's labelled `[0,1]` phenotypes are the better fit for `af_spec`, and adoption is decision D10.** |
| **MHR — Momentum Human Rig** `facebookresearch/MHR` | **Apache 2.0**, unqualified | GitHub SPDX = `Apache-2.0`, `LICENSE` at root; the README's own licence section read in full: *"MHR is licensed under the Apache Software License 2.0, as found in the LICENSE file"* — stated for **MHR**, not merely its code. 917 stars, **pushed 2026-11-25** | **The strongest structural fit found — and previously recorded in this document as an unidentified "MHR", now identified.** Meta's anatomically-inspired parametric full-body model: a parametric skeletal model, a skinned mesh **with levels of detail**, a **body blendshape and pose-corrective model**, and a **facial blendshape model** — LOD, deformation correctives and face, mapping directly onto this engine's own LOD system, its 52 ARKit blendshapes and §3.D's deformation work. Ships a **glTF export tool** (this project is glTF/GLB native) plus SMPL/SMPL-X conversion. `pip install mhr` + `mhr-download-assets`, **no registration or gating**; from v1.2.0 runs on PyTorch alone from per-release converted assets (**~25 MB for LOD 1**); CI covers Ubuntu 22.04/24.04, macOS 15 Apple silicon, Windows and Rocky Linux 9. API: `identity_coeffs`, `model_parameters`, `face_expr_coeffs` → `vertices, skeleton_state`; differentiable. **Residual check:** confirm the downloaded asset bundle carries no separate licence file. Python/PyTorch ⇒ **authoring/CI-time only**. **CAUTION — read §6.4 before acting on this row.** MHR's 45 `identity_coeffs` are **latent Gaussian dimensions sampled with `torch.randn`**, not named bounded fields: they are **not** interchangeable with `kSpecFloatCount = 45`, and a latent space cannot be bounds-checked field by field or audited per P2/P6. The "strongest structural fit" claim here is about **capability** (LOD, correctives, glTF) and was written before the parameterisation comparison; §6.4.3 therefore recommends MHR as a **LOD/deformation reference and `.glb` oracle**, not as the engine's body model. Adoption is **D10** |
| **MakeHuman assets** `makehumancommunity/makehuman` | **CC0 1.0 Universal** for assets | `LICENSE.md` §C read in full: assets = *"the base mesh and proxies, targets and modifiers, textures, clothes (any MHCLO-based asset), poses and expressions"* — *"released under CC0 1.0 Universal… anyone can do whatever they want with it"* | **CC0 is the most permissive outcome available and it validates `docs/PHOTOREAL_UPGRADE_PLAN.md`'s existing recommendation.** Three conditions below — read them, they are not boilerplate |
| **Ollama** (local inference) | Free, open source | Already integrated in this repository (§1.3 row 13), so already inside its existing notice posture | Local only; no egress; no per-token cost |

**MakeHuman conditions, all three verified from `LICENSE.md`:**

1. **Source code is AGPL-3.0-or-later, not CC0.** `LICENSE.md` §B defines source as *"files that contain
   program logic… python files, bat scripts, shell scripts and **glsl shaders**"*, plus UI images. **This
   repository holds 8,987 lines of GLSL in `af_shaders_gl.h`.** Copying MakeHuman shader or script code
   would import AGPL copyleft into the engine. **Assets only — never code.**
2. **Output is unrestricted.** §D: *"no output from MakeHuman contains any trace of program logic… As the
   assets have been released under CC0, there is no limitation on what you can do with this combined
   output."* This is the licence basis for §3.F.6's central claim that **the engine's own generated output is
   a free dataset**.
3. **Third-party assets are not covered.** §D's closing note: assets from MakeHuman's *asset repositories*
   carry their own licences and are the user's responsibility. Only assets **bundled in the MakeHuman
   distribution** are CC0. Any harvested value must record which of the two it came from.

#### 6.3.3 EXCLUDED — not free to ship

| Candidate | Licence (primary source) | Why P9 excludes it |
|---|---|---|
| **SMPL / SMPL-X / FLAME / MANO / SMPL+H / DMPL** | Non-commercial scientific research only; commercial via a paid Meshcapade licence. **Not readable from github.com** (gated distribution), so the text itself is UNVERIFIED here — but the restriction is confirmed indirectly: `EricGuo5513/HumanML3D`'s own README instructs the reader to *"Download SMPL+H… and DMPL model"* from `mano.is.tue.mpg.de` / `smpl.is.tue.mpg.de` | Paid commercial licence is the only route to shipping. Technically excellent (SMPL-X: 10,475 vertices, 54 joints incl. jaw, eyeballs, fingers) and still excluded. P9 is stricter than §6.2's optional-loader pattern |
| **AMASS** | Non-commercial; distribution restricted. Own licence text not reachable from github.com → **UNVERIFIED directly**, but **VERIFIED indirectly** by HumanML3D's README: *"Due to the distribution policy of AMASS dataset, we are not allowed to distribute the data directly"* | Restricted data, and cannot even be redistributed by a project that lawfully holds it |
| **HumanML3D** `EricGuo5513/HumanML3D` | Repository SPDX = **MIT** — and that is precisely the trap | README VERIFIED: it *"originates from a combination of HumanAct12 and Amass dataset"*, may not redistribute the data, and **requires the non-commercial SMPL+H and DMPL body models**. So: MIT code, restricted data, restricted body models. **Excluded on two independent grounds** |
| **MDM** `guytevet/motion-diffusion-model`, **MotionGPT** `OpenMotionLab/MotionGPT` | Both **MIT** for code (SPDX verified) | Weights and evaluation depend on HumanML3D/AMASS and SMPL body models, so they inherit both restrictions. This is what makes §3.D item 8 **BLOCKED on licence** rather than deferred |
| **FLUX.1 [dev]** and every dev variant — Fill, Canny, Depth, Canny/Depth LoRA, Redux, **Kontext**, Krea | **"FLUX.1-dev Non-Commercial License"** — VERIFIED from `README.md`'s own licence table, which lists each variant individually | Free to download, not free to ship. The repo states commercial use requires licensing via `bfl.ai/pricing/licensing`, and ships a *"reference implementation… with usage tracking enabled for commercial licensing"* |
| **Hunyuan3D 2.1** `Tencent-Hunyuan/Hunyuan3D-2` | **"TENCENT HUNYUAN 3D 2.0 COMMUNITY LICENSE AGREEMENT"** — `NOASSERTION`, text read | Licence line 3, verbatim: *"THIS LICENSE AGREEMENT DOES NOT APPLY IN THE EUROPEAN UNION, UNITED KINGDOM AND SOUTH KOREA AND IS EXPRESSLY LIMITED TO THE TERRITORY"*; §1.l defines Territory as *"worldwide… excluding… the European Union, United Kingdom and South Korea"*. Royalty-free but non-exclusive, non-transferable, subject to an Acceptable Use Policy (Exhibit A, not read), with distribution conditions: provide the agreement to recipients, mark modified files, ship a "Notice" file, and disclose that Tencent is not affiliated. "Model Derivatives" is defined to include distillation and synthetic-data training | **Territory exclusion alone disqualifies it under P9**, before the AUP and obligations are even considered. The owner is in Australia, so it would not block *them* personally — but P9 excludes territory-restricted licences as a class, because the APK can be distributed anywhere. The reported "1M MAU threshold" is **withdrawn as unverified** |
| **Stable Diffusion 3.5 / SDXL** `Stability-AI/sd3.5` | `LICENSE-CODE` = **MIT**; **weights licence absent from the repository** | A revenue-capped community licence is reported by multiple secondary sources but could not be read from a primary source. Excluded under P9 pending verification — a cap would not be free even if confirmed |
| **contact-human-dynamics** `davrempe/contact-human-dynamics` | **ADOBE RESEARCH LICENSE** — noncommercial research only, expressly excluding commercial product development | **Excluded; vendor nothing from it.** Also depends on OpenPose and SMPL. **What M5 may still use:** the *idea* of labelling per-frame foot contact as four binary channels (left heel, left toe, right heel, right toe) is a trivial data convention, not copyrightable expression, and M5 reimplements it independently from the engine's own `BodyMetrics` landmarks and skin matrices. No code, no weights, no derived data |
| **Stable Fast 3D**, **HunyuanImage**, **Adobe Firefly**, hosted image APIs, **Meshcapade**, paid mocap libraries | Various: revenue caps, region exclusions, per-image pricing | Excluded by P9. Licences not individually read because the category is already disqualified |
| **RGB MoCap** `toyxyz` | MIT app, *"models under their own terms"* (secondary claim, not verified) | Design reference only: Windows + NVIDIA, ~41 GiB, and the models inside it are not cleared |
| **OpenAI / OpenRouter / paid OpenAI-compatible hosts** | Pay-per-token | Permitted **only** as a user-configured, user-funded, disclosed Tier-3 opt-in that the engine never requires (§3.G.1). Never bundled, never a default, never load-bearing |

#### 6.3.4 UNVERIFIED — not recommended until read

| Candidate | Claim | Status |
|---|---|---|
| **AIST++** | Reported CC BY 4.0 — would be the only genuinely free motion dataset found, and CC BY 4.0 is a class this repository already accepts for its Khronos fixtures | Dataset is **not hosted on github.com**; not reachable from this workspace. **UNVERIFIED.** If M5 ever needs free motion data, verify this first — and separately verify what any model trained on it was also trained on |
| **Chroma** / "Chroma1" | Reported Apache 2.0, a de-distilled FLUX.1-[schnell] derivative | Repository **could not be located** by GitHub search from this workspace. **UNVERIFIED** — do not cite it as an option until found and read |
| **HiDream-O1** | Reported MIT | Not located. **UNVERIFIED** |
| ~~**"MHR"**~~ | **RESOLVED 2026-11-26** — it is `facebookresearch/MHR`, **Momentum Human Rig**, Apache-2.0 verified from its own README licence section and SPDX. **Moved to ACCEPTED** in §6.3.2, where it is now the strongest structural fit found | **Closed** |
| ~~Anny's pip package and hosted model assets~~ | **RESOLVED 2026-11-26.** PyPI `anny` v0.6.2 exists but declares **no licence**, so the README's per-component section governs and was read in full (§6.3.2). One check remains: confirm no script this project runs ever fetches the **`smplx`/`smpl`** bundle, which the README marks *"non-commercial use only"* | **Closed** except that one check |

**Standing rule derived from this exercise.** A licence is verified only when its text has been read from
the distributing repository or host. An SPDX badge is evidence about *one file* — HumanML3D, MDM,
MotionGPT and SD 3.5 are all MIT-licensed repositories whose *usable output* is restricted. Read the data
licence and the weights licence, not the code badge.

### 6.4 MHR versus Anny — full technical and licence comparison

Owner instruction, 2026-11-26: *prioritise MHR as the first candidate for a full technical and licence
comparison against Anny; do not adopt either automatically.* **Neither is adopted by this document.** All
facts below were read from each project's own README and licence file on 2026-11-26.

#### 6.4.1 Technical comparison

| Dimension | **MHR** (`facebookresearch/MHR`) | **Anny** (`naver/anny`) | Which fits this engine |
|---|---|---|---|
| Nature | Momentum Human Rig — parametric body model with identity, pose and facial-expression parameterisation. arXiv 2511.15586 | Differentiable human body mesh model, "from infants to elders using a common topology and parameter space" | — |
| **Shape parameterisation** | `identity_coeffs`, shape `[batch, 45]`. The README's own example samples it as **`0.8 * torch.randn(batch, 45)`** — a **latent Gaussian space**. Documented structure: first 20 body, second 20 head, last 5 hands | `phenotype_kwargs = {key: 0.5 for key in model.phenotype_labels}` — **named, labelled parameters, each in `[0,1]`** | **Anny.** This engine's `af_spec` is 45 *named, bounded, human-meaningful* fields and `af_anatomy` maps them to landmarks and girths. Anny's labelled phenotypes are the same design; MHR's latent coefficients are not |
| Pose | `model_parameters`, shape `[batch, 204]` — "joint angles and scalings" | Per-bone 4×4 matrices (`torch.eye(4).repeat(1, model.bone_count, 1, 1)`); `pose_parameterization="local-ref"` or `"local-bone"`; documented pose-transfer portability | **Anny** for retargeting (M5): explicit per-bone matrices and a documented pose-transfer story |
| Face | `face_expr_coeffs`, shape `[batch, 72]`, sampled as `0.3 * torch.randn(batch, 72)` — **latent** | `facial_action_labels`, values in `[0,1]`; Face Units asset pack; part-specific face and hand models | **Anny** — labelled actions map onto this engine's 52 named ARKit channels; 72 latent coefficients do not |
| **Levels of detail** | **7 LODs (LOD 0–6)**, with `corrective_blendshapes_lod?.npz` per level and an LOD-conversion tool | Not advertised | **MHR, decisively.** This engine has 3 index-only levels (`kAvatarLodLevels = 3`, `kLodFractions = {0.12f, 0.05f}`) and M7 exists to improve exactly this |
| **Deformation** | **Non-linear pose correctives: neural-network-based pose-dependent deformations** (`corrective_activation.npz` = "pose corrective MLP sparse activations") | Not advertised | **MHR, decisively.** This engine uses linear blend skinning only, and §3.D's deformation item has no corrective mechanism today |
| **glTF export** | **`tools/mhr_export`** — "Export MHR poses and animations to glTF (`.glb`) for Blender and other tools" | Exports `.ply` in the README example | **MHR.** This project is glTF/GLB native, so an external `.glb` producer is directly useful as an import test oracle |
| Skeleton | Not stated as a bone count in the README | Two rigs: default `anny` = **104 bones** (facial-expression, eye and tongue bones pruned); `rig="makehuman"` = **163 bones** | Both differ from this engine's **71 joints**, so either way M5 faces a retargeting mapping, not a match |
| Topology control | Not advertised | **Five topologies**: `anny` (default, triangulated, unattached vertices removed, nudity edits applied), `anny-quads`, `anny-full` (keeps unattached vertices **and disables the nudity edits**), `makehuman`, plus `soma` | **Anny** — but see the content-policy consequence below |
| Differentiability | Yes, PyTorch, CPU or GPU; also PyMomentum for a fast CPU solver | Yes, PyTorch | Equal |
| Distribution | `pip install mhr` + `mhr-download-assets`; assets **"downloaded and unzipped from release"** — a GitHub release, no registration or gating. TorchScript route needs no codebase or assets but supports **LOD 1 only** | `pip install anny`, **v0.6.2** on PyPI; no registration or gated download | Equal — both ungated |
| Size | **~25 MB** for the converted LOD 1 assets | Not stated in the README | MHR is known and small |
| Maintenance | **pushed 2026-11-25**, 917 stars, CI across Ubuntu 22.04/24.04, macOS 15 Apple silicon, Windows, Rocky Linux 9, Python 3.12/3.13, CPU-only and CUDA 13 | v0.6.2 released 2026-10-26 | MHR, marginally |
| Runtime placement | Python/PyTorch ⇒ **authoring and CI only, never on device** | Same | Equal — neither can run on `minSdk 26` |

**The number that must not be misread.** MHR has **45 shape parameters** and this engine's spec crosses as
**45 floats** (`kSpecFloatCount = 45`). **These are not the same 45 and there is no field-for-field mapping.**
MHR's are latent coefficients sampled from a Gaussian; the engine's are named, bounded, human-meaningful
fields validated by `AvatarSpec.kt` and guarded by a `static_assert`. Treating that coincidence as
compatibility would be exactly the invented-equivalence `AGENTS.md` §2.2 forbids. Bridging them requires a
**fitted or learned transform**, which is a project of its own, not a wiring task.

#### 6.4.2 Licence comparison

| Element | **MHR** | **Anny** |
|---|---|---|
| Code | **Apache 2.0**, unqualified — README: *"MHR is licensed under the Apache Software License 2.0, as found in the LICENSE file"*; SPDX `Apache-2.0` | **Apache 2.0** — `LICENSE` read in full; © 2025 NAVER Corp. |
| Assets | Distributed **from the project's own GitHub release** under the same unqualified statement. **Residual check:** confirm the release bundle carries no separate licence file | Stated **per component** in the README: `data/mpfb2` (MakeHuman via MPFB2) **CC0 1.0**; `data/faceunits01` (Face Units, Mika Suominen) **CC0 1.0**; `data/soma` (NVlabs/SOMA-X) **Apache 2.0** |
| **Internal restriction** | None found | **The `smplx` and `smpl` topologies are "non-commercial use only"**, downloaded separately from a NAVER host. **Excluded** — use `anny`, `anny-quads`, `anny-full`, `makehuman` or `soma` only |
| Transitive dependencies | `PyMomentum` (`facebookresearch/momentum`) — **MIT, verified**; `trimesh >= 4.8.3` (tools only) — **UNVERIFIED**, its repository was not at the path checked; PyTorch — SPDX `NOASSERTION` (BSD-style with extensive third-party notices), **UNVERIFIED here** | PyTorch as above; MakeHuman **assets** are CC0, but MakeHuman **code is AGPL-3.0** and its §B expressly includes *glsl shaders* — so **assets only, never code** |
| Packaging metadata | — | PyPI `anny` v0.6.2 declares **no licence field and no licence classifier**, so the README governs |
| Redistribution in an APK | Neither: both are Python/PyTorch, authoring/CI-time only, and §6.5 forbids weights in the APK regardless | Same |

#### 6.4.3 Conclusion — neither adopted, and they are not interchangeable

The comparison **contradicts the provisional ranking recorded earlier in this document**, which put MHR
first on the strength of its Apache-2.0 licence, active maintenance and feature set. That ranking was
incomplete: it had not read how each model is *parameterised*, which is the property that decides fit here.

- **MHR is stronger on capability this engine lacks**: 7 LODs against 3, neural non-linear pose correctives
  against linear blend skinning only, and a `.glb` export tool against a `.ply` example. Those map onto
  **M7** (optimisation) and **§3.D item 9** (deformation).
- **Anny is stronger on the property this engine's architecture depends on**: **interpretable, labelled,
  bounded parameters in `[0,1]`**, which is what `af_spec`, `af_anatomy` and the adults-only invariant
  already are — and what **P2** (validated boundary) and **P6** (auditable learning) require. Latent
  Gaussian coefficients cannot be bounds-checked field by field, cannot be unit-tested against a named
  anthropometric reference, and cannot be audited by a human reading a decision log.

So the honest recommendation is **not "pick one"**:

| Use | Candidate | Why |
|---|---|---|
| Anthropometric and proportion reference for M4's critics | **Anny** | Labelled phenotypes in `[0,1]`, WHO-calibrated, CC0 assets — auditable per **P3** and **P6** |
| LOD strategy and corrective-deformation reference for M7 and §3.D | **MHR** | 7 LODs and NN pose correctives are exactly what this engine lacks; useful as a *reference*, and its `.glb` export is a ready-made import test oracle for M2 and M5 |
| Anything shipped in the APK | **Neither** | Python/PyTorch; `minSdk 26`; §6.5 forbids weights in the APK |

**This is a recommendation, not an adoption.** Decision **D10** remains open and the owner decides. Before
either is used, the residual checks above must be closed: MHR's release-asset licence, `trimesh`'s licence,
and a standing guarantee that no script fetches Anny's non-commercial `smplx`/`smpl` bundle.

### 6.5 Standing constraints on all of this

- The engine's **only** third-party C++ dependency today is **zlib**. Adding Draco, meshopt, KTX2, Eigen or
  assimp is an owner decision with a `THIRD_PARTY_NOTICES.md` entry, not an agent's call — the repository
  has already declined compression dependencies deliberately.
- No external model may be **redistributed inside the APK**. Rule 5 already forbids private model binaries;
  the same mechanism keeps multi-gigabyte weights out of a shipping artifact.
- The precedent for the *right* kind of external harvest is already in this repository: the 3D4ME work took
  **measured constants and a reference document**, recorded provenance and the MIT obligation in
  `THIRD_PARTY_NOTICES.md`, and vendored **no code and no binaries**. That is the pattern §3.F generalises,
  and it is why a licence-heavy model can still be useful here without being shipped.
- Anything an agent learns externally must be attributed in the commit or document that uses it, with the
  date it was checked, so a later reader can tell a 2026 fact from a stale one.

### 6.6 The zero-cost default configuration

What a user gets with **nothing configured and nothing paid**, which P9 requires to be fully functional:

| Layer | Zero-cost default | Cost |
|---|---|---|
| Text / structured reasoning | Local **Ollama** if the user happens to run one; otherwise the deterministic `af_engine_analyze_prompt` parser | £0 — and the parser path needs no model at all |
| Geometry generation | The existing deterministic solver, unchanged (**P1**) | £0 |
| Asset inspection | **M2's `af_asset_inspect`**, native C++ beside the solver | £0, on-device, no network |
| Evaluation | **M3's records** from measurements the repository already takes (`RendererTelemetry`, host tests, instrumented suites) | £0 |
| Body-reference proportions | **Anny** + WHO growth standards, at authoring/CI time | £0 |
| Learning and memory | **M6's** constant tables, golden records and decision log — data in Git, reviewed by a human | £0 |
| Image / 3D generation | **Off by default.** Optional, and only via free self-hosted models with licences verified in §6.3.2 (FLUX.1 [schnell], Qwen-Image, TripoSR) on the user's own GPU | £0 in licence; the user's own electricity and hardware |

The important consequence: **every subsystem that makes the engine intelligent — B, C, E and F — is free,
local and offline.** Only the generative extras in A are optional, and only artifact generation needs a
workstation GPU. Nothing in the critical path depends on a provider, a subscription, a revenue threshold or
a territory. That is what makes P9 satisfiable rather than aspirational.

**Standing instruction to every mission agent:** before adding any dependency, report (1) the licence of the
code, (2) the licence of the model or data files it needs to run, (3) the licence of what it was *trained
on* where knowable, and (4) whether any of those would be redistributed inside the APK. A dependency is
free only if all four answers are free. "MIT repository" is not an answer to (2) or (3) — that is exactly
how HumanML3D and the HMR models in §6.1 get misread.

## 7. What must not happen

Drawn from this repository's own recorded history, not from general advice.

1. **Do not branch from a stale base and rebuild an adjacent feature.** Four launcher icons, four
   GLES-diagnostic paths, two bounded GLB readers, two animation systems, and duplicated subsurface /
   `eye_tone` / garment-relief work all came from this. Branch from the trunk and grep first.
2. **Do not put a model in the geometry path.** The solver's determinism is tested by byte-identical
   exports. A model that emits vertices destroys the only property that makes regression detectable.
3. **Do not add a second imported-animation API.** Decide between the two that already exist (M5) and
   write the decision down.
4. **Do not report a morph target as supported.** Sparse accessors are read; morph targets are not applied
   on import — the importer contains zero occurrences of `morph`, while the exporter writes them. The
   diagnostic must say `AF_MORPH_NOT_READ_ON_IMPORT`.
5. **Do not weaken a failing test to make CI green.** The one disabled target in this repository is
   disabled with both failing check names in a comment beside it and a stated reason. That is the standard.
6. **Do not transfer a green result to a different head.** Results belong to the commit that produced them.
   A README update creates a new head and invalidates the evidence above it.
7. **Do not call an emulator result a phone result.** No claim about the Galaxy S24 has ever been verified
   here; `PROJECT_STATUS.md` records SM-S921B as UNVERIFIED.
8. **Do not ship a chatbot as intelligence.** The existing Ollama chat page is a dead end whose output
   reaches nothing. M1 converts it into a console over a gateway whose output is validated `af_spec`.
9. **Do not let a learned improvement be unauditable.** If it cannot be shown as a diff of data plus a
   test, with the commit and device that justified it, it does not land.
10. **Do not delete `arena/089497bd-3d-mega-hub`** until the M5 decision is made — it holds the only
    specification for third-party skeleton adoption, and 15 other branches are already gone.
11. **Do not treat "open weights" or an MIT repository as free.** P9 is about what it costs to *ship*.
    HumanML3D is labelled MIT and is still restricted, because its motion comes from AMASS; FLUX.1 [dev]
    downloads for nothing and still needs a paid licence; SD 3.5 is free until the owner earns $1M. Check
    the model files and the training data, not the repository badge.
12. **Do not make the engine depend on a paid provider.** Opt-in, user-funded and disclosed is acceptable;
    required, bundled or silently defaulting to a network call is not. With zero providers configured the
    engine must still generate, inspect, evaluate and improve.
13. **Do not create an implementation branch while D1 and D2 are open.** Owner instruction 2, 2026-11-26.
    Every mission in §4.4 branches from the result of PR #17 (D1) along a path into `main` that is currently
    ambiguous between PR #16 and PR #9 (D2). Branching before both are resolved reproduces §7 item 1 —
    the exact failure this repository has already paid for fifteen deleted branches to learn.
14. **Do not quote the SM-S921B baseline as a current result.** Owner instruction 5, 2026-11-26. §3.L.1's
    figures belong to the APK of `797d156`, measured 2026-11-15, before the LOD work. Any FPS, heap,
    triangle-count or import-time claim about the phone must name its commit and its date, or not be made.
    "53.8 FPS on the S24" with no SHA attached is a violation, not a summary.
15. **Do not adopt MHR or Anny from this document.** §6.4 compares them and recommends a split use; owner
    instruction 3 requires that neither be adopted automatically. Adoption is **D10**, and it needs the
    three residual licence checks in §6.4.2 closed first.

---

## 8. Open items, unresolved blockers and required owner decisions

Nothing in this section is designed around. Each item is either unverified research or a decision only the
owner can make, and each names the mission it blocks.

### 8.1 Unresolved blockers

| # | Blocker | Blocks | What would resolve it |
|---|---|---|---|
| B1 | **The generated-avatar runtime gate fails** on the trunk (`9baf7c67`, run 45779175991) and on the merged line (`bdaef4b5` run 45777317839, `ba38d996` run 45789457244, **`281806e7` run 45813704590 — 2026-11-26, step 8, all other steps `success`**), at step 8. **The `281806e7` result is the most informative of the four: that commit is documents-only — its entire diff against `ba38d996` is this markdown file — yet it fails the same step identically. A markdown file cannot make an instrumentation test fail, so the failure is a property of the gate or its environment on this line, not of the code delta between those two heads.** The API yields no test name for it: the job's only annotations are a Node-20 deprecation warning and *"The process '/usr/bin/sh' failed with exit code 1"*, because the logs and artifacts live on hosts outside this workspace's permitted set. It passes on lead head `758bcfb4` (run 45779172779), so the trunk fails it alone. **Whether the new screen-density LOD selection also affects it is UNVERIFIED**: `workflow_dispatch` refuses a bare SHA (`422 No ref found`), and Actions logs and artifacts are served from hosts unreachable from a restricted workspace | M3 (device records), M7, and the credibility of every `IMPLEMENTED IN SOURCE` claim | **M8(a)**: the actual log on hardware the agent controls, with repeated runs to separate flake from fault |
| B2 | **The LOD fix has never run on a real GPU.** It is REAL + VERIFIED on host for the maths and the 11 native tests, IMPLEMENTED IN SOURCE for the GLES wiring, and no existing test asserts the chosen level | M7, M8 | **M8(b)**: an orbit sweep at ~1 m / 3 m / 12 m with `Avatar LOD selected:` lines captured |
| B3 | **Two incompatible imported-animation APIs exist** (§3.D item 1). **VERIFIED 2026-11-26 from both headers.** This line: `af_engine_animation_count` / `_name` / `_duration` / `_info` (`afcore.h:447-463`) plus `af_engine_import_motions` (`:524`) and `af_engine_import_motions_from_memory` (`:525`). Donor `58aced02`: exactly **nine** `af_engine_imported_*` at `afcore.h:271-286` — `clip_count`, `clip_name`, `clip_duration`, `play`, `seek`, `advance`, `pause`, `resume`, `is_playing`. The conflict is real and precisely bounded. Until one is chosen, any animation work risks building the second system this repository has already paid for once | M5 entirely; anything touching animation later | **M5's freeze F-3**: a committed decision document naming the choice and the cost of the rejected alternative. The *facts* are now verified; the **choice is decision D4** |
| B4 | **`arena/089497bd-3d-mega-hub` must not be deleted.** **VERIFIED present 2026-11-26 at `58aced02b52a632814ad29dc714a19b577298ff0`.** It holds the only specification for third-party skeleton adoption (`imported_animation_test.cpp`, 949 lines, 94 checks). Its own `THIRD_PARTY_NOTICES.md` was read and confirms the obligation verbatim: the fixtures *"live in `src/main/assets`, not `src/test/resources`, so they ARE packaged into the distributed APK (596 KB combined)"*, *"CC BY 4.0 requires attribution"*, and for `Fox.glb` specifically *"Two separate rights holders with two separate licences on one file."* 15 other branches are already gone | M5; branch hygiene | An owner decision on D4 plus the fixture/licence question. **Do not delete the branch until both are settled** |
| B5 | **Morph targets are not read on import at all** — **VERIFIED 2026-11-26**: zero occurrences of `morph` in `af_glb_read.cpp`, while `afcore.h:390` confirms the exporter writes them and `afcore.h:413-414` documents the import side as unsupported; the two directions are asymmetric. No scanned or generated face can be driven until this changes  | Facial animation on imported models (§3.C item 3, §3.D item 6); **part of B3's import capability** | **ASSIGNED TO M5** by owner instruction 6, 2026-11-26 — see §5.7 deliverable (f). M5 is the correct owner because it already owns the imported-asset animation capability and its own exclusion previously deferred this; M2 is *not* the owner because M2 inspects and reports, and its exclusions forbid implementing morph application. M2's duty is unchanged: report `AF_MORPH_NOT_READ_ON_IMPORT`, never `supported` |
| B6 | **PR #16 (`arena/33f12efd` → `main`) is a second competing path into `main`** alongside PR #9. **VERIFIED 2026-11-26: both open, both non-draft.** Full open-PR set: #17 (this line → trunk, non-draft, MERGEABLE), #16, #15 (draft), #14, #12 (superseded, left open), #9. They cannot both be right | The trunk's future, and therefore every mission's base | Owner reconciliation (**D2**). Neural Core work must not start on a branch whose base is unsettled |
| B7 | **PR #17 must land before any mission branches.** **VERIFIED 2026-11-26: still open, non-draft, `MERGEABLE`, head `ba38d996074c2f8a9981ebbaeb0fee3e3cd4af5e`.** Every mission cuts from `integration/avatarforge-renderer-port`; branching earlier re-creates the stale-base problem in one step | Wave 0, therefore all of it | Merge PR #17 **with a merge commit, not a squash** (**D1**) |
| B8 | **The brief's section D ended at the single word "Deformation."** Deformation is designed here as measurement (§3.D item 9, §3.E.2). If the owner intended corrective blendshapes, cloth or muscle simulation, or penetration resolution, **that is not designed in this document** | M4, M5 scope | Re-issue the remainder of section D |

### 8.2 Unverified research — do not rely on these yet

Each was checked against a primary source and could not be confirmed. None may be cited in code or in a
`THIRD_PARTY_NOTICES.md` entry until read.

| Item | What is unverified | How to verify |
|---|---|---|
| ~~Anny's pip package and hosted model assets~~ | **RESOLVED 2026-11-26.** PyPI `anny` **v0.6.2** exists but declares **no licence field and no licence classifier**, so the GitHub README governs — and its per-component licence section was read in full: code Apache-2.0, `data/mpfb2` and `data/faceunits01` **CC0 1.0**, `data/soma` Apache 2.0, and **`smplx`/`smpl` topologies "non-commercial use only"** | **Closed.** Remaining check: ensure no script fetches the `smplx`/`smpl` bundle |
| ~~`model_licenses/LICENSE-FLUX1-schnell`~~ | **RESOLVED 2026-11-26** — the file was fetched from `black-forest-labs/flux` and is the full Apache License 2.0 text | **Closed** |
| **AIST++ (reported CC BY 4.0)** | The only genuinely free motion-data candidate found, and CC BY 4.0 is a class this repository already accepts. **Checked 2026-11-26: a GitHub repository search for it returned nothing**, so it is not reachable from this workspace's permitted hosts | Read the licence on its own host. If free, it unblocks part of §3.D item 8. **Still UNVERIFIED — nothing depends on it today** |
| **Chroma / "Chroma1"**, **HiDream-O1** | Reported Apache 2.0 and MIT respectively. **Re-checked 2026-11-26:** a Chroma search returned nothing; a HiDream search returned only **third-party wrappers** (`prateekjoshi565/HiDream-API`, two app repos), not an official model repository. Neither official source could be located | Locate the official repository, then read its licence. **Both remain excluded and are offered nowhere in this plan** |
| **Stability AI Community License** (SD 3.5 weights) | `Stability-AI/sd3.5` contains `LICENSE-CODE` = MIT and **no weights licence at all**. **Re-checked 2026-11-26:** a code search found 13 matches for "Stability AI Community License" inside `org:Stability-AI`, and `Stability-AI/generative-models` lists a `LICENSE.md` and a `model_licenses/` directory, but fetching that `LICENSE.md` returned **404** (non-default branch or moved path), so the text was still not read. The widely-reported $1M revenue cap is therefore **not repeated as fact here** | Read the licence from `Stability-AI/generative-models` at its default branch, or from the model host. **Excluded under P9 either way until proven free** |
| **Hunyuan3D 2.0 Acceptable Use Policy (Exhibit A)** | The licence body was read — the territory exclusion in line 3 is verbatim — but Exhibit A was not, and the licence's later sections were not | Not needed: the territory exclusion already disqualifies it under P9 |
| **Meshcapade pricing** | Two secondary sources gave ~€150,000 annually and ~€1–5 per scan. Both **UNVERIFIED** and mutually inconsistent | Irrelevant — the SMPL family is excluded on licence class, not on price |
| **Anny's blendshape count and `Anny-One` size** | Reported as both **564** blendshapes and **1,136** prototypical mesh variations; the dataset as both **780k** and **800k** images. The README (v0.6.2, 2026-10-26) was read and mentions *"compact phenotype blendshapes by default"* but states **no count** | Still **UNVERIFIED** — needs arXiv 2511.03589, which is not reachable from this workspace. Neither figure is load-bearing for any mission, so nothing is blocked |
| ~~**"MHR"**~~ | **RESOLVED 2026-11-26** — `facebookresearch/MHR`, Momentum Human Rig, Apache-2.0 verified from its own licence section. Now in §6.3.2 ACCEPTED. Residual: confirm the `mhr-download-assets` bundle carries no separate licence file | **Closed** except that one check |
| **`avatarforge-engine` on a physical phone** | No claim about the Galaxy S24 has ever been verified here; `PROJECT_STATUS.md` records SM-S921B as UNVERIFIED | M8, on the actual device |

### 8.3 Decisions required from the owner

| # | Decision | Options | Recommended |
|---|---|---|---|
| D1 | **OPEN — owner instruction 1 forbids merging without explicit approval.** Merge **PR #17** (the convergence) | merge with a merge commit / hold | **Merge, no squash** — commit ids are cited throughout the documents, and every mission branches from the result |
| D2 | **OPEN — blocks all branching with D1 (owner instruction 2).** **PR #16 versus PR #9** as the path into `main` | close #16 / close #9 / rebase #16 onto the trunk | Cannot be decided from inside this initiative; it blocks B6 |
| D3 | **SUPERSEDED IN PART by owner instruction of 2026-11-26** (*push the 10 commands to git*) combined with instruction 2 (no new branches). The plan is committed to the working branch `arena/1535c8f6-3d-mega-hub` and therefore appears in PR #17 as a document addition only | keep it there / re-home it on a new documents-only branch **after** D1+D2 resolve | **Keep it there.** Re-homing requires creating a branch, which instruction 2 forbids today |
| D4 | **M5's animation API** (B3) | this line's `af_engine_animation_*` / the donor's `af_engine_imported_*` | Decide after reading both; the donor's is specified by 94 checks but needs nine absent functions and CC BY 4.0 fixtures in the APK |
| D5 | **SUPERSEDED by D10 and by §6.4.** Original recommendation (*endorse MHR as primary*) was made **before** the parameterisation comparison and is **withdrawn** | endorse MHR / endorse Anny / both for different purposes / defer until M3 exists / no external body model | **Endorse MHR as the primary candidate** — Apache-2.0 verified, actively maintained (pushed 2026-11-25), and the only one offering LOD, pose correctives, facial blendshapes **and glTF export**, which map onto systems this engine already has. Keep **Anny** for its WHO-calibrated anthropometry and CC0 MakeHuman assets. Both are Python/PyTorch ⇒ authoring/CI-time only. **Exclude Anny's `smplx`/`smpl` topologies** (non-commercial) |
| D6 | Whether **M9** (optional fine-tuning) proceeds at all | proceed / cancel | Cancel is free of consequence by design — every other mission is Tier-0 (§3.G) |
| D7 | **Deformation scope** (B8) | as designed (measurement) / broader | Re-issue the remainder of section D if broader |
| D8 | Adding any third-party C++ dependency (Draco, meshopt, KTX2, Eigen, assimp) | allow / decline | **Decline for now** — zlib is the only one today, and M7 can proceed without them |
| D9 | **The licence terms of this engine's own output** (§3.I rule 3, §3.F.6) | declare output unrestricted / declare a licence / leave undeclared | **Declare it.** The whole free-dataset argument in §3.F.6 rests on the engine's generated avatars being the project's own data. MakeHuman's licence makes that claim explicitly for its own output; this engine makes no such statement anywhere. Until it does, `af_asset_provenance.licence` for generated assets has no honest value to carry, and M2 cannot fill a required field |
| D10 | **Reference body model: MHR or Anny** — full comparison now in **§6.4**; supersedes D5 | MHR for everything / Anny for everything / **split use as §6.4.3 recommends** / neither / defer until M3 exists | **Split use, neither adopted as the engine's body model.** MHR for **LOD strategy and corrective-deformation reference** (7 LODs vs this engine's 3; NN pose correctives vs linear blend skinning) and as a **`.glb` import oracle** for M2/M5. Anny for **anthropometric reference** in M4's critics — labelled phenotypes in `[0,1]` match `af_spec`'s named bounded fields, which MHR's latent Gaussian `identity_coeffs` (45 latent dims, **not** the same as `kSpecFloatCount = 45`) cannot. Both Apache-2.0 code, both ungated downloads, both Python/PyTorch ⇒ **authoring and CI only, never in the APK**. **Exclude Anny's `smplx`/`smpl` topologies** (non-commercial). **Three residual checks first:** MHR's release-bundle licence, `trimesh`'s licence, and a guarantee no script fetches the non-commercial bundle |

### 8.4 Standing prohibition: preserve `arena/089497bd-3d-mega-hub` and all unique work

Owner instruction 7, 2026-11-26. **`arena/089497bd-3d-mega-hub` must not be deleted, force-pushed over, or
left to rot.** Verified present on 2026-11-26 at `58aced02b52a632814ad29dc714a19b577298ff0`.

It is not a stale branch to be tidied. It is the **only specification** for third-party skeleton adoption,
and M5 deliverable (f) plus freeze F-3 both depend on being able to read it. Its unique work, recorded here
so that preservation is knowing rather than accidental:

| Unique work on `arena/089497bd` | Status | Recover with |
|---|---|---|
| `imported_animation_test.cpp` — 949 lines, 94 checks | **Not rescued.** 85 lines call nine absent `af_engine_imported_*` functions | `git show 58aced02:avatarforge-engine/src/test/cpp/imported_animation_test.cpp` |
| The nine `af_engine_imported_*` functions (`afcore.h:271-286`) | **Not rescued.** One of the two competing APIs in B3 | `git show 58aced02:avatarforge-engine/src/main/cpp/include/afcore.h` |
| Divergent `src/anim/` (8 files) | **Not rescued** | `git show 58aced02:avatarforge-engine/src/main/cpp/src/anim/` |
| `ClipMapping.kt` and its test, `NativeAnimationDriver.kt`, `avatarforge_animation_jni.cpp` | **Not rescued** | `git show 58aced02:<path>` |
| `AVATARFORGE_SKELETON_MANIFEST.json` | **Not rescued** — a skeleton manifest is directly relevant to M5's retargeting mapping | `git show 58aced02:AVATARFORGE_SKELETON_MANIFEST.json` |
| Five `tools/` scripts | **Not rescued** | `git show 58aced02:tools/` |
| Five animation fixtures, `assets/models/animation/*.glb` (~596 KB, Khronos `CesiumMan` / `Fox`) | **Not rescued, and must not be landed without an owner decision.** The donor's own `THIRD_PARTY_NOTICES.md` records that they *"ARE packaged into the distributed APK"* and that `Fox.glb` carries **two rights holders under two licences** | `git show 58aced02:THIRD_PARTY_NOTICES.md` |
| The deepened two-layer garment normal-map relief in `af_texture.cpp` (793 lines against this line's 1,057) | **Not rescued.** It is the cause of the two failing checks in the disabled contract test (§1.6) | `git show 58aced02:avatarforge-engine/src/main/cpp/src/af_texture.cpp` |

Three rules follow:

1. **No mission may delete, rename or force-push this branch.** Fifteen branches have already been deleted in
   this project; their tip SHAs were recorded first, which is the only reason they are recoverable. This
   branch's SHA is recorded here and in `docs/BRANCH_CONVERGENCE_PLAN.md`.
2. **M5 must read it before deciding F-3.** Choosing an animation API without reading the rejected
   alternative is how two animation systems already appeared.
3. **`arena/25bd4008-3d-mega-hub` is preserved for the same reason.** It is the sole owner of
   `kotlin-compile-gate.yml` (M8 deliverable (d)) and of 29 unique paths including `RendererTimeBudget.kt`
   and `GlbValidatorAnimationInventoryTest.kt`. PR #14 remains open against it.

### 8.5 What is deliberately not designed here

So that its absence is not mistaken for an oversight:

- **No implementation.** This document is planning only, as instructed.
- **On-device inference.** No model runs on the phone. Text providers are localhost-adjacent; artifact
  providers need a workstation GPU (§3.A.1).
- **Learned motion generation.** BLOCKED on licence, not deferred (§3.D item 8, §6.3.3).
- **Hair cards, garment geometry reconciliation, and the deepened normal-map relief.** Named in §1.6 and
  §3.C as separate work with separate gates; not scoped into any mission here.
- **A new geometry or rendering path.** P1 forbids it, and §3.H.2 shows the solver, rig, evaluation,
  regression detection, memory and performance work contain no AI at all.
- **Anything outside `avatarforge-engine/`.** `:app` is touched only where a mission says so, and only to
  consume an engine interface. No 3D-Mega-Hub merge, no Cue-Next change, no unrelated objective in any PR.

### 8.6 Verification ledger — what must be verified before each mission may begin

Requirement 7 of the owner's approval: **verify all eight blockers and ten unverified research items before
their dependent missions begin.** Status as at 2026-11-26, after a primary-source verification pass that read
licence files and repository headers directly rather than relying on secondary sources.

| Item | Verified? | Verified how, or what remains | Gate: which mission may not start until it is closed |
|---|---|---|---|
| **B1** runtime gate fails | **PARTIAL — and narrowed** | Attribution is measured (lead passes, trunk fails, merged line fails at `bdaef4b5`, `ba38d996` **and `281806e7`**, run 45813704590). Because `281806e7` is documents-only, the failure is now shown to be **independent of the code delta between the last two heads** — it is a property of the gate or its runner environment. The *specific failing test* still could not be read: logs and artifacts are on hosts outside this workspace's permitted set, and the annotations give only `exit code 1` | **M3** may not treat any device metric as settled; **M7** may not claim a performance win; **M8(a)** must still separate flake from fault with repeated runs on hardware the agent controls |
| **B2** LOD never run on a real GPU | **NO** | Requires hardware. No substitute exists | **M7** may not start; **M3**'s device records are provisional |
| **B3** two animation APIs | **YES — facts** | Both headers read: this line's six functions at `afcore.h:447-463`, `:524`, `:525`; the donor's nine `af_engine_imported_*` at `afcore.h:271-286` | Closed as research. **M5** still needs the **D4 decision** before writing code |
| **B4** donor branch and fixtures | **YES** | Branch present at `58aced02`; its `THIRD_PARTY_NOTICES.md` read, confirming the fixtures *"ARE packaged into the distributed APK (596 KB combined)"*, that *"CC BY 4.0 requires attribution"*, and that `Fox.glb` has two rights holders under two licences | Closed as research. **M5** needs D4 plus a fixture decision |
| **B5** morph targets on import | **YES — and CORRECTED** | `grep -ci morph` over `af_glb_read.cpp` = **0**: not read at all, not merely unapplied. `afcore.h:390` (exporter writes them) and `:413-414` (import side unsupported) read directly | Closed. **M2** must use `AF_MORPH_NOT_READ_ON_IMPORT` |
| **B6** PR #16 vs #9 | **YES — states** | Both open, both non-draft, confirmed via the API | **All missions** — the base is unsettled until the owner reconciles (D2) |
| **B7** PR #17 must land first | **YES — state** | Open, non-draft, `MERGEABLE`, **unmerged** (`mergedAt=null`), head now `281806e7…` after the documents-only commit of 2026-11-26; 186 files listed, of which the only addition at that head is this plan | **All missions** (D1) |
| **B8** section D truncated at "Deformation" | **NO** | Only the owner can supply it | **M4** and **M5** scope is provisional |
| Anny licences | **YES** | `LICENSE` and the README's per-component section read in full; PyPI v0.6.2 checked and declares no licence | Closed. Residual: never fetch `smplx`/`smpl` |
| MHR licence | **YES** | SPDX `Apache-2.0` plus the README's own licence section read in full | Closed. Residual: confirm the `mhr-download-assets` bundle carries no separate licence |
| FLUX.1 [schnell] | **YES** | `model_licenses/LICENSE-FLUX1-schnell` fetched — full Apache 2.0 text | Closed |
| MakeHuman assets | **YES** | `LICENSE.md` §B (AGPL code), §C (**CC0 assets**) and §D (unrestricted output) read in full | Closed — and §D is the licence basis for §3.F.6 |
| HumanML3D / AMASS / SMPL family | **YES — excluded** | HumanML3D's own README read: data from AMASS, *"not allowed to distribute the data directly"*, requires SMPL+H and DMPL downloads | Closed. **M5** may not use them |
| Hunyuan3D 2.1 | **YES — excluded** | Licence line 3 and §1.l read verbatim: territory excludes the EU, UK and South Korea | Closed. The "1M MAU" figure is **withdrawn** — it was not in the sections read |
| contact-human-dynamics | **YES — excluded** | `LICENSE.txt` read: Adobe Research License, non-commercial only | Closed. **M5** reimplements the F×4 convention independently |
| TripoSR / TRELLIS / InstantMesh / Qwen-Image | **YES** | SPDX verified for each; TripoSR's README additionally states MIT covers *"the source code, pretrained models, and an interactive online demo"* | Closed |
| AIST++ | **NO** | Not reachable; a GitHub search returned nothing | Only gates a *hypothetical* free-motion-data path in M5. Nothing depends on it today |
| Chroma / HiDream-O1 | **NO** | Official repositories could not be located; only third-party wrappers found | Nothing depends on them — they are offered nowhere |
| SD 3.5 weights licence | **NO** | `LICENSE-CODE` is MIT; the community licence text was not readable (404 at the attempted path) | Excluded under P9 regardless |
| Anny blendshape / dataset counts | **NO** | arXiv 2511.03589 is not reachable from this workspace | **Not load-bearing** for any mission |
| SM-S921B thermal, battery, >38 s sessions | **NO** | `docs/PHONE_TEST_2026-11-15.md` states them as *"Not measured — UNVERIFIED"* | **M8(c)** exists to close this; **M7** may not claim a device performance result until it does |

**The honest summary.** Of eight blockers, **five are now verified** (B3, B4, B5, B6, B7) and three remain
open (B1 partially, B2, B8) — all three requiring either hardware or the owner. Of the ten unverified
research items, **six are closed**, one is closed-as-excluded-withdrawn, and three remain open but
**non-load-bearing**: nothing in the ten missions depends on AIST++, Chroma, HiDream-O1, the SD 3.5 weights
licence, or Anny's blendshape count. No mission is blocked by an open licence question.

**Two gates stop everything regardless:** **D1** (merge PR #17) and **D2** (reconcile PR #16 with PR #9).
Until those are settled there is no stable base to branch from, and releasing an agent now would reproduce
the exact stale-base failure this convergence was created to end.
