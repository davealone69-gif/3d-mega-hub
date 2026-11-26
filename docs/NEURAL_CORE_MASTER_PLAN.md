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
> | 8 | Keep all **ten missions** and their acceptance criteria intact | Unchanged: ten prompts, execution order `M0 → M8 → M1 → M2 → M3 → M4 → M5 → M6 → M7 → M9`, all required fields machine-verified present in each — **now ten fields per brief** under the 2026-11-26 brief, including new field 5 (detailed implementation instructions) and field 8 (runtime verification requirements). M5 gained deliverable (f) and is now **staged S0→S-E**; its acceptance criteria (a)–(e) are untouched |
>
> **No implementation, destructive Git operation, or merge has been performed under this document.**
>
> ### Owner input of 2026-11-26 (third) — four Dropbox links and one named fault
>
> | Input | Where honoured |
> |---|---|
> | Four Dropbox links: the `ink` authoring project, `/3d/` device captures, `/3d real/` reference models, and a `/goal/` three-view turnaround sheet | **§8.6** — each resolved and identified by `content_hash`; links 1 and 3 were already documented in depth by `docs/OWNER_MODELS_2026-11-15.md` and `docs/QUALITY_REFERENCE_ink_glb.md`; **nothing was downloaded into this repository** |
> | *"Current faults notice **webing on movement**"* | **§3.D item 10**, **B9**, **D15**; measured by a new fourth **M4** critic, remedied in new **M5 stage S-F**. Diagnosed from source as a weight-quality and skinning-model defect, **not** a rendering or LOD one. `PARTIAL` — owner-reported, not measured |
> | *"this is details in want on **mylove.glb**"* | **§8.6.3** — `mylove.glb` is an identity/detail target for **measurement only**: already recorded at 9,287,272 B with both hashes, private, `redistribute = deny`, `train = deny`, never committed, and no critic may be fitted to it |
> | *"**ink.glb** … close to quality wanted"* | **§8.6.3** — upgrades the repository's existing quality reference from *researched recommendation* to **owner-confirmed target**, while its licence position is unchanged: Daz Genesis 8 Female content, so `redistribute = deny` and `train = deny` |
>
> **Honest limit on all of the above:** the images could not be retrieved from this workspace
> (`dl.dropboxusercontent.com` is outside the permitted host set), so **no measurement in this plan comes from
> them**. To close **B9** and **B2**, attach the nine captures to the conversation or have an M8 agent
> reproduce the session on hardware it controls.
>
> ### Owner decision of 2026-11-26 (fourth) — D15 resolved: *"Definitely 1 and 2"*
>
> Recorded as: **tiers 1 and 2 of the §3.D item 10 deformation remedy are authorised — weight quality first,
> then dual-quaternion skinning only if measurement still shows the fault — tier 3 (pose correctives) is not
> authorised, and `af_vertex` keeps its 4 influences and 88-byte layout.** Two consequences were written into
> the plan rather than left implicit: the tiers are **sequential and measurement-gated**, and because both
> authorised tiers edit **protected** components (`af_rig.cpp`, `src/render/*`), a new **§3.M.1a protocol**
> now defines how a human-authored mission may touch one — ABI unchanged, no test weakened, before/after
> measured, golden SHA-256 delta reported, one protected component per PR, register entry added. Without that
> protocol the decision would have been unexecutable. The interpretation and its alternative reading are
> recorded in the **D15** row itself.
>
> ### Owner input of 2026-11-26 (fifth) — the goal sheet, attached and seen: *"This quality of mylove.glb"*
>
> The `/goal/` turnaround sheet arrived a second time as a **direct conversation attachment**, and this time
> it could be opened: identity with the Dropbox file is **proven by hash** (SHA-256 `69fd0de3…`, whose Dropbox
> content-hash form is exactly the recorded `c21e0ba8…`; same name, 27,300 B, 512 × 341). With it the owner
> named the quality bar — *"This quality of mylove.glb"* — converging the identity target (`mylove.glb`) and
> the quality target on one object, with the sheet as its visual illustration. Recorded in **§8.6.5**: what
> the sheet actually shows (three bust views, ≈170 px each, stylised-realistic adult, strand-level hair and
> lace detail — a head-detail bar, not a polygon bar), what it cannot do (nothing below the chest, so no
> evidence for **B9**/**D15**; no reconstruction, per **D14**; no measurement, only a goal), and what does not
> move (`mylove.glb` never committed, `redistribute = deny`, `train = deny`, no critic fitted). One
> permission tightens by strictest-wins: the sheet, as an owner-asserted depiction of mylove's likeness,
> becomes `redistribute = deny, train = deny`. The new open question — may the bar ever be enforced by
> automated calibration rather than human judgement — is **D16**.
>
> ### Owner input of 2026-11-26 (sixth) — *"Lastest 3d-mega-hub upgrade"*: the upgrade chain, catalogued
>
> A two-word pointer, so both connected clouds and GitHub were swept for what an "upgrade" could mean
> (§8.6.6). The honest headline: **no new 3d-mega-hub upgrade has landed in the repository** — this branch is
> documents-only and `main` is untouched. What exists is a chain of owner-side drops: a **v0.10.1 Android
> update bundle** (2026-10-21, APKs + AAR + build evidence + a signing-certificate warning), a **"fixed"
> source snapshot at `3b98600`** (2026-10-24) that is provably **27 commits behind the pin this plan already
> uses**, the **latest build** = AvatarForge release `debug-411-1`'s `latest-debug.apk` (2026-11-20,
> debug-signed, ungated, record-only), and — the genuinely new material — an **owner-side 3d-mega-hub
> snapshot** (2026-11-10→16) whose README carries two 2026-11-15 handover sections absent from GitHub:
> the preserved pre-integration APK baseline, the interim non-monolithic architecture rule, and the
> adult-mode handoff that owner-authorises **Grok-Girls** for reuse. Recorded with statuses in **§8.6.6**;
> the two questions it opens — which artifact, if any, becomes the integration baseline (**D17**) and
> whether adult-mode integration enters scope (**D18**) — are in the register.
>
> ### Owner input of 2026-11-26 (seventh) — *"fix all and continue with ai"*
>
> Read within the standing constraints, and recorded so the reading is checkable: **fix** = close every open
> item in this plan that evidence can close. Done: the §8.2 research table is now fully dispositioned (seven
> rows resolved or closed with reasons, one hardware-gated row left open on purpose), `trimesh` is
> MIT-verified at `mikedh/trimesh` (the earlier 404 was a wrong slug), Hunyuan's Exhibit A is read in full,
> and D10's residual list shrinks to one bundle-level check. **It does not and cannot mean implementation,
> merging or branching:** PR #17 stays unmerged without explicit approval, `main` stays untouched, and no
> implementation branch exists while D1/D2 are open. **Continue with ai** = advance the Neural Core design to
> its next artefact: the six interface freezes, specified concretely in a new **§3.P** so M0 can code against
> real signatures the day D1/D2 clear — every new identifier marked PROPOSED, every restated measurement
> cited.
>
> ### Owner brief of 2026-11-26 (second) — the GOKU handover requirements
>
> | Requirement | Where honoured |
> |---|---|
> | Mission briefs carry **ten** fields, not eight | §5 intro lists all ten; every brief machine-verified to contain fields 1–10 **in order** |
> | **Detailed implementation instructions** per mission | Field **5** of each brief — files, existing patterns to copy, traps, order of work |
> | **Runtime verification requirements** separated from build evidence | Field **8** of each brief; field **7** is now tests and build evidence only |
> | 3D-Mega-Hub = final integrated app; AvatarForge = separate standalone engine/SDK; 3DDD = separate donor; Cue-Next entirely separate | **§1.0** — evidenced table plus four boundary rules. Cue-Next: `grep` finds **zero** references in this repository outside this plan |
> | Do **not** assume Anny's `[0,1]` maps to the engine's 45 fields | **§3.O.1** (six kinds of quantity, sentinels, the 45/45 coincidence debunked) and the withdrawn implication in **§6.4.3** |
> | Define validated units, semantic mappings and calibration | **§3.O** — §3.O.1 unit table, §3.O.2 the eleven-field **calibration record**, §3.O.3 what is honestly known today (no record exists; status `MISSING`) |
> | Separate local preview from redistribution and training permissions | **§3.I.2** — three independent permissions, `unknown` behaves as deny, seven rules |
> | Stage M5: skeletons, retargeting, playback, morph targets, facial animation | **§5.7 field 5** — stages **S0 → S-A → S-B → S-C → S-D → S-E**, each with entry condition and exit evidence |
> | Preserve provenance and licence restrictions; invent no ownership or training rights | **§3.I.1** D9 recommendation **withdrawn**; **D11** added for the engine's own undeclared licence; M6 field 5(h) and M9 field 5(f) gate training on D9+D11 |
> | Protect existing work: no competing app, no duplicate APIs, no unnecessary deps, no destructive cleanup, no PR #17 merge, no `main` change, preserve both `arena` branches, keep D1/D2 visible | §7 items 1, 3, 8, 10, 13; §8.4; §1.0 rule 3; M7 field 5(g) declines new C++ deps (D8); D1/D2 flagged **OPEN** at the top of §8.3 |
> | Resolve architectural contradictions | §3.J.1 said "all **seven**" metric families where §3.E.2 defines **eight** — corrected in both places |

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

### 1.0 The project family, and where Neural Core may and may not live

Owner instruction, 2026-11-26: keep 3D-Mega-Hub as the final integrated Android application, AvatarForge as
its separate standalone engine/SDK project, 3DDD as a separate donor project, and Cue-Next entirely separate.
These boundaries are **evidenced from this repository's own documents**, not assumed.

| Project | What it actually is — with evidence | Neural Core's relationship to it |
|---|---|---|
| **3D-Mega-Hub** (`davealone69-gif/3d-mega-hub`) | **The final integrated Android application and canonical host.** `settings.gradle.kts` sets `rootProject.name = "3D Mega Hub"` and includes exactly two modules, `:app` and `:avatarforge-engine`. `docs/AVATARFORGE_INTEGRATION.md`: *"`3d-mega-hub` remains the canonical Android app."* `docs/MASTER_BLUEPRINT.md`: *"Canonical host app, architecture, integration records and release evidence."* | The **product**. `:app` is the shipping surface; Neural Core is never a competing app inside it (§7 item 8) |
| **AvatarForge** (`davealone69-gif/AvatarForge`) | **A separate standalone engine/SDK repository — the donor.** Pinned at branch `work/android-engine-dev`, commit `9e36197ace3b18faa6c566d93f8fce89b185c45d`. `docs/MASTER_BLUEPRINT.md` describes it as *"Native avatar kernel, C ABI, Kotlin SDK, rigging/blendshapes and diagnostics"*. Its renderer, JNI, app UI and prebuilt AAR/APKs were **deliberately excluded** from the port | The **upstream**. Neural Core does not modify that repository; it extends the **in-repo port** |
| **`:avatarforge-engine`** (this repository) | **The AvatarForge engine/SDK as ported into the host** — *"a deliberate port of the useful engine boundary—not a wholesale copy of the donor app or its renderer."* 39 copied native files match the pinned donor byte-for-byte (except trailing spaces removed from one blank line in `af_math.h`); the two Kotlin models differ only in package declaration | **Neural Core's home**: `src/neural/` + `afneural.h` in C++, `java/com/davemega/hub/avatarforge/neural/{procedural,generated}` in Kotlin, behind `AF_ENABLE_NEURAL` (default OFF). This is what "keep everything exclusively within AvatarForge" means in practice |
| **3DDD** (`davealone69-gif/3DDD`) | **A separate donor project — the Filament viewer.** Its renderer was **not imported**: *"The 3DDD Filament viewer remains separate; its inspected Android CI run 40981986126 failed, so its renderer was not imported."* Its `main` CI failed at `Assemble release APK (unsigned)` (release-lint `backup_rules.xml` / `data_extraction_rules.xml` conflicts) and its audit failed at `kspDebugKotlin` with Room KSP `IllegalStateException: Empty schema file`. Only `ExportManager.exportMp4`'s encoder-drain and container-validation logic was adapted, into `app/.../export/AvatarMp4Exporter.kt` | **Read-only reference.** No Neural Core code lives here; its Room/KSP failure is the recorded reason M6 avoids Room (§5.8 field 5(a)) |
| **Cue-Next** | **Entirely separate.** `grep -rIn "Cue-Next\|CueNext\|cue-next"` over this repository's `*.md`, `*.kt` and `*.kts` returns **zero hits outside this plan**. There is no code, configuration, dependency or CI relationship to it here | **Out of scope. Do not modify, reference, import or combine.** Status: `REAL` as a project the owner names, `UNVERIFIED` as to its contents — nothing in this repository evidences it |
| Others in the family | `docs/MASTER_BLUEPRINT.md` also lists **Aaa** (SDK/provider contracts), **Grok-Girls** and **Aura-Studio-Avatar** as candidate donors, each *"reuse only after source/build/licence inspection"* | Not used by Neural Core. Any future use needs the same inspection plus a `THIRD_PARTY_NOTICES.md` entry **before** first use |

**Four boundary rules that follow, and that every mission obeys:**

1. **The engine is a module here and a repository there.** `:avatarforge-engine` is *this* repository's port of
   AvatarForge. Changes to Neural Core land in the port. Nothing in this plan authorises a change to
   `davealone69-gif/AvatarForge` itself, and no mission may clone, vendor or re-pin it.
2. **`:app` consumes the engine; the engine never consumes `:app`.** M1 therefore puts the gateway in the
   engine module and demotes `:app`'s Ollama page to a console over it — the dependency arrow keeps its
   direction (§3.0).
3. **No competing app, no second renderer, no duplicate API.** §7 items 1, 3 and 8, and
   `docs/MASTER_BLUEPRINT.md`: *"Do not run two renderer stacks in parallel without an explicit, tested need."*
4. **Licence status of the port is itself undeclared.** `docs/AVATARFORGE_INTEGRATION.md`: *"No
   repository-level `LICENSE` file was present at the pinned AvatarForge branch… that does not establish
   third-party redistribution rights."* `docs/MASTER_BLUEPRINT.md`: *"A dependency's licence does not
   automatically license the host app or generated assets."* This is decision **D11**, and it is **prior** to
   D9 — the output's licence cannot be settled while the engine's own is not.

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
makes the **Anny route** licence-clean for its `data/mpfb2` component. **It does not make the "engine's own
output is free data" argument licence-clean** — that argument is `BLOCKED` on **D9** and **D11** and its
earlier "decisive licence finding" framing is withdrawn in §3.F.6. MakeHuman's §D statement is about
MakeHuman's output; it says nothing about this engine's.

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
10. **Webbing on movement — the owner's named deformation defect, 2026-11-26.** *This partially re-issues
   section D: the owner has now named one concrete deformation fault.* Reported as **"current faults notice
   webing on movement"**, with the evidence being the nine device captures in the owner's Dropbox `/3d/`
   folder (§8.6.1). **Status: `PARTIAL` — owner-reported, and not measured by this plan**, because the images
   cannot be retrieved from this workspace (§8.6's limitation). What follows is a diagnosis from the engine's
   own source, which *is* verifiable here.

   **Why this pipeline webs at concave joints — read from the code, not inferred:**

   | Evidence | Location | Consequence |
   |---|---|---|
   | A vertex carries **4 joints + 4 weights**: `uint16_t joints[4]; float weights[4]` inside the byte-exact **88-byte** `af_vertex` | `afcore.h:318-327` | A hard **4-influence ceiling**, and it is an ABI contract the renderer's vertex stride depends on (§3.M.1 protected) |
   | A vertex *"may share its weight only with bones **within three joints** of that one"* | `af_rig.cpp:369` | Weight sharing is topologically restricted, so a concave region between two chains can be denied the influences it needs |
   | Influences contributing **< 1 %** are zeroed, then survivors renormalised | `af_rig.cpp:436-439` | The prune removes exactly the small third and fourth influences that keep an armpit or hip open |
   | Two **single-joint, weight-1.0 fallbacks** | `af_rig.cpp:408-409` and `:442` | A vertex that falls back is rigidly bound to one bone — the surface there does not deform at all, which reads as webbing or a crease |
   | `Rig::skin` is **pure linear blend skinning**; `Rig::deform` adds only the **52 ARKit face** blendshapes | `af_rig.cpp:449`, `:727` | **There are no body pose correctives anywhere in the engine.** LBS linearly interpolates rotations, which collapses concave surfaces inward as a joint closes |
   | *"the skin weights were wrong too (see `computeWeights`) and hid it"* | `af_rig.cpp:807` | The repository already records weight faults **masking** visual faults — precedent for measuring weights and appearance together |
   | *"joint-collapse/self-intersection observations"* are listed as something to check, **with no metric defined** | `docs/AVATAR_PHOTO_OVERLAY_QUALITY_TEST_BACKLOG.md:63` | The project anticipated this defect class and never instrumented it. That is the gap |

   **The defect, stated precisely.** Webbing at the axilla, inguinal fold, inner elbow, knuckles and
   neck/clavicle is the expected behaviour of 4-influence LBS whose small influences are pruned at 1 % and
   whose fallback binds a vertex to a single bone, with no pose-dependent correction. It is a **weight-quality
   and skinning-model** defect, not a rendering or LOD defect — and it is therefore **not** M7's, and **not**
   fixed by the texture work.

   **Three remedy tiers, in cost order, each gated by the measurement below:**

   **Owner decision 2026-11-26: tiers 1 and 2 are AUTHORISED; tier 3 is NOT (**D15**). `af_vertex` keeps its
   4 influences and its 88-byte layout.** The tiers are **sequential and measurement-gated**, not a menu: tier
   2 may not begin until tier 1 has been measured and shown insufficient, because tier 2 costs a change to two
   protected components and can only be verified on a device.

   1. **Weight quality — AUTHORISED, DO THIS FIRST. Procedural, no ABI change, no licence question, no
      dependency.** Soften or region-gate the 1 % prune (`af_rig.cpp:436`), widen the three-joint sharing rule
      (`:369`) at concave landmarks, remove the single-joint fallbacks (`:408-409`, `:442`) where a second
      influence exists within tolerance, and smooth weights by distance or heat diffusion. Stays inside 4
      influences, so `af_vertex` and the renderer are untouched. **Touching `af_rig.cpp` is permitted here
      because it is a human-authored mission under the §3.M.1a protocol, not a learning-system write** — read
      that protocol before editing.
   2. **Dual-quaternion skinning — AUTHORISED, but only if tier 1's measurement still shows the fault.**
      DQS replaces the linear interpolation of transforms with quaternion blending, which removes
      linear-interpolation collapse (including the candy-wrapper case) **without** needing more influences.
      Two honest caveats: **(a)** DQS is not a pure win — it trades concave collapse for **joint bulging and
      volume inflation** where influences are widely separated, so the M4 critic must report *both* the
      collapse metric and a region-volume metric, and tier 2 can regress one while fixing the other; **(b)** it
      changes `Rig::skin` (`af_rig.cpp:449`) *and* the GPU skinning path in `af_gl.cpp`, which compiles only
      under `if(ANDROID)` — so it **cannot be validated by host `ctest`** and needs device verification
      (§5.9 field 5(d), and B2's constraint that the LOD/GPU path has never run on a real GPU).
   3. **Pose correctives — NOT AUTHORISED (D15).** Delta-mush-style sculpted deltas, or learned correctives:
      the industry answer, and precisely what **MHR** offers (*"non-linear pose correctives =
      neural-network-based pose-dependent deformations"*, §6.4.1). Deferred because it requires per-pose delta
      **data** or a trained **model**, which collides with **D9/D11** (no declared licence ⇒ nothing may be
      trained on, §3.I.2) and with §3.K's *"no model weights ever load into the app process"* if learned.
      **This stays designed and unauthorised**: if tiers 1 and 2 are measured and the fault survives them, the
      correct action is to report that with numbers and re-open D15 — not to start tier 3.

   **The metric that decides between them — measurable on the host, from geometry alone.** For named concave
   landmark pairs (axilla, inguinal, inner elbow, knuckle, neck/clavicle), pose the mesh through
   `af_engine_evaluate_animation` at a sweep of joint angles and report: *(i)* minimum `Bvh::signedDistance`
   (`af_mesh.h:612`) between the opposing surfaces — webbing shows as this distance falling toward and through
   zero; *(ii)* self-intersection count in the joint region; *(iii)* region surface-area change against rest;
   *(iv)* **region volume** against rest — added because the authorised tier 2 (dual-quaternion skinning)
   fixes concave collapse by *inflating* joints, so a collapse-only metric would score that inflation as a
   win; *(v)* the influence count actually used per vertex in that region, and how many were pruned by
   `af_rig.cpp:436`. All five are procedural, deterministic and host-runnable. **Landmark order is fixed:
   axilla and inguinal fold first**, then inner elbow, knuckle, neck/clavicle — those two are where a
   4-influence LBS pipeline with a 1 % prune webs first. **Tolerances must be
   established by measurement, not invented** (§2.1, P3) — the first run produces the baseline, and only a
   later run may call something a regression.
   **Three measurement points, not one.** The critic runs **(1)** as a baseline before any change, **(2)**
   after tier 1, and **(3)** after tier 2 if tier 2 is reached. **Tier 2 is authorised only if measurement (2)
   still shows the fault** — that comparison is the gate, and it is why the baseline must be recorded with no
   tolerance asserted.

   **Assignment.** Measurement → **M4**, as a fourth critic (§5.6 field 5(i)). Remedy → **M5 stage S-F**
   (§5.7 field 5). The ABI question → **D15**. The defect itself → **B9**.

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
- **The engine's own output is the most promising dataset path — and today it is `BLOCKED`, not free.**
  *Corrected 2026-11-26.* An earlier revision of this document called self-generated output "the decisive
  licence finding" on the reasoning that a deterministic solver **owns** its outputs. **That reasoning is
  withdrawn: it assumed an ownership nobody has established.** Two verified facts contradict it. First,
  `docs/AVATARFORGE_INTEGRATION.md` records *"No repository-level `LICENSE` file was present at the pinned
  AvatarForge branch"* and that copying between the owner's own repositories *"does not establish third-party
  redistribution rights"* — the **engine's own licence is undeclared** (**D11**). Second, this engine states
  nothing anywhere about the licence of what it produces (**D9**). `docs/MASTER_BLUEPRINT.md` already fixed the
  rule: *"A dependency's licence does not automatically license the host app or generated assets."*
  The precedents that looked supportive do not transfer: MakeHuman's *"no output from MakeHuman contains any
  trace of program logic… there is no limitation on what you can do with this combined output"* is a statement
  **by MakeHuman's authors about MakeHuman's output** (§6.3), and Anny's authors generating the `Anny-One`
  synthetic set were exercising rights over **their own** model. Neither says anything about this engine.
  **So: the path stays designed, the eligibility set stays empty.** Until the owner declares D11 and then D9,
  every generated asset is `preview = allow, redistribute = unknown, train = unknown` (§3.I.2) and M6 records
  it as `NOT_ELIGIBLE_FOR_TRAINING`. Once declared, self-generated data labelled by human approval (3.F.2)
  would indeed be both auditable and free of third-party terms — that is the reason to settle D11 and D9 early,
  and it is an argument for a decision, not a substitute for one.
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
output. **This document deliberately makes no recommendation about what that licence should be**, and asserts
no ownership and no training rights for generated assets. Earlier drafts suggested declaring the output "the
user's own data" by analogy with MakeHuman's §D; that suggestion is **withdrawn**, because it would have the
plan inventing a legal position: MakeHuman's statement is about MakeHuman's output, and nothing in this
repository establishes who owns an avatar this engine produces, or whether it may be trained on.

Two verified facts make the gap concrete, and both point *away* from an easy answer:

- **The engine's own licence is undeclared.** `docs/AVATARFORGE_INTEGRATION.md` records that *"No
  repository-level `LICENSE` file was present at the pinned AvatarForge branch"*, that the source *"was copied
  between the user's own repositories at their explicit direction"*, and that this *"does not establish
  third-party redistribution rights"*. That is decision **D11**, and it is **prior** to D9: an output's licence
  cannot be settled while the licence of the code that produces it is not.
- **`docs/MASTER_BLUEPRINT.md` already states the rule:** *"A dependency's licence does not automatically
  license the host app or generated assets."*

Until D11 and then D9 are declared by the owner, M2 fills the field with
`UNDECLARED-OWNER-DECISION-PENDING`, `licence_verified = 0`, and §3.I.2's permissions default to
`preview = allow, redistribute = unknown, train = unknown` — so every generated asset stays usable on its
owner's device while being barred from distribution and from training. That is safe, honest, and mildly
inconvenient, in that order. **No mission may resolve D9 or D11 by inference.**

**Status today: MISSING.** No provenance of any kind is recorded for generated or imported assets, and D9 is
undeclared.

#### 3.I.2 Local preview is not redistribution, and redistribution is not training

Owner instruction, 2026-11-26: *separate local asset preview from redistribution and training permissions.*
One licence answer is not enough, because one asset can be lawfully viewable, unlawfully shareable, and
separately unlawful to train on. **Three permissions are recorded independently**, and each defaults to
**denied** until something positive is known.

| Permission | Question it answers | Granted when | Recorded as |
|---|---|---|---|
| **`may_preview_locally`** | May this asset be loaded, inspected and rendered **on the user's own device, for the user's own eyes**? | Almost always — including for quarantined assets. A user's own file must stay usable, and blocking preview would punish honesty about licence state | `preview = allow` plus the reason |
| **`may_redistribute`** | May this asset be **exported, shared, published, or packed into a distributed APK**? | Only where a licence read from a **primary source** permits redistribution, and `licence_verified = 1` | `redistribute = allow / deny / unknown` |
| **`may_train_on`** | May this asset be used as **training, fine-tuning or evaluation-fitting data**? | Only where **both** hold: the asset's licence permits derivative computational use, **and** — for engine-generated assets — **D9** (output licence) and **D11** (engine licence) are declared | `train = allow / deny / unknown` |

Rules that make the separation real rather than decorative:

1. **The three are independent, and the strictest wins per action.** `preview=allow, redistribute=deny,
   train=deny` is the correct and common state for a quarantined asset — it stays useful to its owner without
   becoming shippable or trainable.
2. **`unknown` is not `allow`.** Every permission defaults to `unknown`, and `unknown` behaves as `deny` for
   redistribution and training while still permitting local preview. A default of "allow unless proven
   otherwise" is how a non-commercial model's output ends up in a published APK.
3. **Training is the strictest gate in the system.** §3.I.1 rule 3 already bars AI-generated assets from
   training unless the provider's output terms expressly permit it — the verified example being Hunyuan3D 2.1's
   clause defining *"Model Derivatives"* to include *"methods based on the generation of synthetic data
   Outputs… for training that model"* (§6.3.3). Note that clause's shape: a licence can permit **use** while
   forbidding **training**, which is exactly why `may_redistribute` and `may_train_on` are separate fields.
4. **Evaluation data is not a loophole.** Fitting a critic's thresholds or a calibration record (§3.O.2) to a
   dataset is a derivative computational use of it, so `may_train_on` governs it. Evaluation-only licences must
   be read as such, not as "fine for our purposes".
5. **The owner's own material is denied all three by policy, not by licence.** Dropbox material and
   `mylove*.glb`: `preview = allow` at runtime on the owner's device, `redistribute = deny`, `train = deny`,
   never committed, never packed, identified in evidence by SHA-256 rather than path (`AGENTS.md` rule 5).
6. **The APK question is a redistribution question.** The donor's five CC BY 4.0 fixtures (~596 KB) are
   recorded in the donor's own `THIRD_PARTY_NOTICES.md` as *"packaged into the distributed APK"* with *"two
   separate rights holders with two separate licences on one file"* for `Fox.glb`. CC BY 4.0 permits
   redistribution **with attribution**, so `redistribute` may be `allow` **only if** the attribution ships —
   and `train` remains a separate question. That is decision **D12**.
7. **Provenance carries the permissions.** `af_asset_provenance` gains the three flags beside `licence` and
   `licence_verified`, so an asset that travels into a GLB `extras` block carries its permission state with it
   and a later import does not have to re-derive — or re-guess — it.

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
| **Metric families** | **All eight** from §3.E.2 — Geometry; Topology/UV; Material/texture; Deformation; Render performance; Render correctness; Resource state; Identity/provenance | A revision must not improve one family by regressing another |
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

#### 3.M.1a What "no write access" does and does not mean — the protocol for touching a protected component

The protected set bars **Neural Core's autonomous machinery** — the learning loop, its proposals, its adapters
and anything it can trigger. It does **not** bar a human-directed engineering mission, because two authorised
missions cannot otherwise do their work: **M5 stage S-F** must edit `Rig::computeWeights` (`af_rig.cpp:282-447`)
and `Rig::skin` (`:449`), and **M7** must tune the LOD constants in `af_lod_select.h` / `af_avatar_lod.cpp`.
Both are protected. Refusing to edit them would make the owner's **D15** decision unexecutable; editing them
freely would destroy the protection. So the rule is a **protocol**, and it binds every mission that touches a
protected path:

1. **Human-authored, never autonomous.** The change is written by a mission agent under a human-approved brief
   and lands as a pull request. The learning system (§3.M.3's propose → test → compare loop) may **not**
   produce it, and no code path may let a proposal reach a protected file.
2. **The ABI does not move.** `af_vertex`'s **88-byte** layout and its 4-influence contract, `af_spec`'s
   meaning, the **45-float** JNI contract, `kSpecFloatCount` and its `static_assert` stay exactly as they are
   (**D15**). A protected-component change that alters an ABI is a different decision and needs the owner.
3. **Existing tests pass unchanged.** No test is weakened, skipped, re-toleranced or deleted to accommodate the
   change (§7 item 5). If an existing test legitimately must change because the *behaviour* changed, the PR
   states which test, why, and what it asserted before.
4. **Measure before and after, on the same subject.** The M4 critic's baseline record, then the post-change
   record, with the delta reported per landmark pair. A protected-component change with no measurement is
   indistinguishable from vandalism.
5. **Golden byte-identity is re-checked, and any difference is reported.** The solver is deterministic, so a
   weight change **will** alter generated bytes; that is expected here, and the PR must state the old and new
   SHA-256 and sizes rather than let a reviewer discover it (§3.J's byte-identity assertion).
6. **One protected component per pull request**, so a regression can be attributed and reverted (§3.N level 2,
   `git revert` with a merge commit).
7. **Recorded in the protected-component register.** `docs/NEURAL_CORE_PROTECTED.json` (created in M0) gains an
   entry per authorised change: path, mission, commit, the decision that authorised it, and the measurement
   that justified it. The register is how a future agent tells an approved edit from a violation.

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

### 3.O Validated units, semantic mappings and calibration

Owner instruction, 2026-11-26: *do not assume Anny's `[0,1]` parameters map directly to the engine's 45
fields; define validated units, semantic mappings and calibration.* This section is the answer, and it is the
single most likely place for a future agent to invent a correspondence that does not exist.

#### 3.O.1 What the engine's fields actually are — read from `afcore.h`, not from a summary

`af_spec` is **not** a vector of 45 numbers in one unit. It is a heterogeneous struct whose 45-float JNI wire
form (`33 + 1 + 3 + AF_FACE_PARAM_COUNT`, with `AF_FACE_PARAM_COUNT = 8` at `afcore.h:114`) packs **six
different kinds of quantity**, each with its own unit, range and sentinel:

| Kind | Fields | Unit | Declared range | Sentinel — the trap |
|---|---|---|---|---|
| Enumerated | `sex`, `build`, `hair_style`, `outfit`, `footwear` | enum (`af_sex`, `af_build`, …) | the enum's values | an out-of-enum int is not a "default", it is undefined |
| Integer control | `seed`, `struct_size` | count / bytes | any / `sizeof(af_spec)` | — |
| **Dimensional** | `height_m` | **metres** | **1.40 … 2.20** (`af_spec_init` ⇒ 1.75) | none — a real unit with real bounds |
| **Dimensional** | `age_years` | **years** | **16 … 70** | drives the adults-only invariant: below 18 the engine **ignores** `bust` and `glute` |
| **Dimensional (tape)** | `neck_cm`, `chest_cm`, `underbust_cm`, `waist_cm`, `hip_cm`, `thigh_cm`, `calf_cm`, `bicep_cm`, `forearm_cm`, `wrist_cm`, `shoulder_width_cm`, `inseam_cm`, `head_circumference_cm` | **centimetres** | honoured exactly; perimeter preserved to **<0.5%** per the header's own claim | **`0` means "derive from height/build"** — `0` is *not* zero centimetres |
| Unitless index | `muscle` (lean mass index), `body_fat`, `bust`, `skin_melanin` | none | `0..1` | anchors are documented, not linear: `body_fat` **0.10 = shredded, 0.45 = heavy** |
| Signed index | `glute` | none | **`-1..+1`** | `0` ⇒ the build's default |
| Material override | `cloth_roughness`, `cloth_metalness`, `gear_metalness` | none | `0..1` | **`<0` ⇒ AUTO / material default** |
| **Colour** | `skin_tone[3]`, `hair_tone[3]`, `cloth_tone[3]`, `gear_tone[3]`, `eye_tone[3]` | **linear-sRGB** triplets | — | **`0` ⇒ derived/auto** — writing `0` to mean black silently requests "derive" instead |
| Face | `face[8]` | see `af_face_param` | — | `0` ⇒ the default face |
| Text | `prompt[512]` | UTF-8 chars | informational; stored as GLB metadata | — |

Three consequences, each of which has already broken a mapping attempt elsewhere in this project's history:

- **`[0,1]` is not a unit.** Two parameters both spanning `[0,1]` may mean entirely different things — one may
  be a lean-mass index, another a melanin fraction, another a normalised latent coordinate with no physical
  meaning at all. Range agreement is not semantic agreement.
- **The sentinels are load-bearing.** `0` means "derive" for every tape field and every colour triplet, `<0`
  means AUTO for the material overrides, and `0` means "default face". Any bridge that writes a genuine zero
  into those fields does not set zero — it **requests derivation**, and the result will differ from the
  intention without any error being raised.
- **The 45/45 coincidence is not compatibility.** MHR has **45 `identity_coeffs`** sampled as
  `0.8 * torch.randn(batch, 45)`; this engine has **45 wire floats** that are named, bounded and heterogeneous.
  The two numbers match. **Nothing else does.** Treating that as a mapping is precisely the invented
  equivalence `AGENTS.md` §2.2 forbids.

#### 3.O.2 The calibration record — the only licence to bridge two parameter spaces

No external model's parameter may be written into an `af_spec` field, and no `af_spec` field read out of one,
except through a committed **calibration record**. Its fields are mandatory:

| Field | Meaning | Why it cannot be omitted |
|---|---|---|
| `engine_field` | the exact `af_spec` field name | a mapping to "the body shape" is not a mapping |
| `engine_unit` | metres / years / centimetres / unitless / linear-sRGB / enum | from §3.O.1, copied not guessed |
| `engine_range` | the declared bounds | the validator enforces them |
| `engine_anchors` | documented anchor points (e.g. `body_fat` 0.10 shredded, 0.45 heavy) | anchors are what make a unitless index interpretable |
| `external_model`, `external_param` | named model and named parameter | MHR's are latent; Anny's are `phenotype_labels` |
| `external_unit`, `external_range` | **read from the model's own documentation** | UNVERIFIED if the documentation does not say |
| `mapping` | the function, in one direction or both, with its inverse where one exists | "roughly linear" is not a function |
| `sentinel_handling` | exactly how `0`, `<0` and out-of-range values are treated | §3.O.1's traps, answered explicitly |
| `validation` | the **measurement** that established the mapping: sample N points, measure the resulting engine output through the engine's own path, report the error | without this the record is a hypothesis, and must be labelled one |
| `status` | `CALIBRATED` / `HYPOTHESIS` / `UNVERIFIED` / `REJECTED` | honest statuses, per §2.1 |
| `source_commit`, `verified_on`, `verified_by` | provenance of the record itself | a calibration nobody can date is a rumour |

**Rules.** *(1)* A record whose `status` is not `CALIBRATED` may not drive generated output; it may only be
reported. *(2)* Records are **data**, versioned and reviewed like golden records, never code comments. *(3)*
Every unmapped external parameter stays unmapped — partial mappings are normal, and padding them out with
plausible guesses is how a body model's "hand size" ends up driving this engine's `wrist_cm`. *(4)* Colour
mappings must state their colour space on both sides; linear-sRGB and sRGB are not interchangeable, and the
engine's `0 ⇒ derive` sentinel makes a naive "0 = black" mapping actively wrong. *(5)* The adults-only
invariant is not calibratable: any external parameter that would produce a body shaped like a minor is
**rejected at the boundary**, not scaled.

#### 3.O.3 What is known today, honestly

- **No calibration record exists for any external model.** Status: `MISSING`.
- **Anny's parameter names are readable** (`model.phenotype_labels`, `model.facial_action_labels`,
  `model.bone_labels`, all documented as being in `[0,1]`) but their **semantics and units are not documented
  in the README**, and the paper (arXiv 2511.03589) was unreachable from this workspace. So even Anny — whose
  design is closest to this engine's — is `UNVERIFIED` as a mapping source until each label is read and
  matched **one at a time, by meaning**.
- **MHR's identity space is latent**, so a mapping would have to be **fitted or learned** from paired samples,
  not read from a table. That is a project of its own, with its own validation burden, and it is **not**
  implied by any recommendation in this document.
- Therefore **M4's critics must run against internal references** (§5.6 field 5(e)) until a calibration record
  reaches `CALIBRATED`, and any external percentile or z-score they emit before then is an invented number.

### 3.P The six freezes, specified — so M0 can code against them the day D1/D2 clear

§4 names the freezes; this section specifies them. Four rules govern everything below: **(i)** every
identifier is **PROPOSED** until the mission that owns it freezes it in code, and a freeze change that
touches a protected path is a §3.M.1a-class event; **(ii)** numbers that restate measured facts cite their
source (the 88-byte / 4-influence `af_vertex` at `afcore.h:318-327`, the 45 `af_spec` floats, the 52 ARKit
face channels); **(iii)** new constants are proposals, never measurements; **(iv)** nothing here may be
implemented before D1 and D2 clear — this is a contract to code against, not permission to code.

**F-0 — `afneural.h` and the OFF-build identity rule.** Layout: `avatarforge-engine/src/neural/`,
`include/afneural.h`, Kotlin packages `neural/procedural` and `neural/generated`; CMake option
`AF_ENABLE_NEURAL` default **OFF**, mirroring `AF_ENABLE_VULKAN … OFF` (`CMakeLists.txt:51`). Proposed ABI:
`AF_NEURAL_ABI_VERSION 1`; opaque `af_neural_session*`; `af_neural_create(const af_neural_config*,
af_neural_session**)`, `af_neural_destroy`, `af_neural_generate(session, const float spec[45],
af_neural_job*)`, `af_neural_job_status`, `af_neural_cancel`, and `af_neural_capability(session,
af_neural_cap*)` returning a §3.G tier. The OFF-build identity rule is **tested, not asserted**: M0 builds
with the option OFF and must reproduce the pre-neural golden bytes (male 73,399,512 B / female 72,076,316 B)
and an unchanged exported-symbol list; a byte or symbol drift fails M0.

**F-1 — `af_asset_inspect`.** Proposed signature `af_asset_inspect(const char* path, af_asset_report* out)`.
Report fields: triangle count; decoded-texture byte estimate (against the 448 MiB budget, §3.L); skeleton
joint count; morph-target count; animation-clip count; provenance class (§3.I.1). Codes, meanings frozen
now, numeric values frozen in M2 with the first consumer test: `AF_ASSET_OK`,
`AF_ASSET_FOREIGN_SKELETON_STATIC`, `AF_ASSET_TEXTURE_OVER_BUDGET`, `AF_ASSET_EXTENSION_UNSUPPORTED`,
`AF_ASSET_NO_ANIMATION`, and `AF_MORPH_NOT_READ_ON_IMPORT` — the last names the real gap at
`af_glb_read.cpp:427-467`, where `grep -ci morph` = 0.

**F-2 — record, metric and tolerance schemas.** Golden record: subject seed (frozen suite seed 1337),
`af_spec` 45-float snapshot, build SHA, asset content hash, metric vector, tolerance verdicts, licence-check
result. Metric record: name, unit, landmark scope, baseline value, comparison value, delta, verdict.
Tolerance record: the two measurement ids it was born from, the proposed bound, a human proposer field —
**no tolerance record may exist without two measurements** (§2.1, P3), which is why the D15 critic runs at
three points and the baseline carries none.

**F-3 — M5's animation API, made selectable.** Both candidates specified side by side so **D4** becomes a
choice between two written interfaces: this repository's `af_engine_animation_*` (`afcore.h:447-463`,
`:524-525`) and the donor's nine `af_engine_imported_*` (`:271-286`), each with signatures, ownership, and
current test coverage (the donor's line is specified by 94 checks but needs nine absent fixtures), plus the
S-A acceptance test that discriminates them on real clips.

**F-4 — memory and recovery schemas.** Revision entry: record id, critic id, before/after metric deltas,
licence-gate result, rollback level used. The three §3.N recovery levels map to record operations: L1
re-run from the golden seed, L2 revert the last revision, L3 disable the capability (`AF_ENABLE_NEURAL` OFF
or per-critic disable) — each level must be executable from records alone, with no hidden state.

**F-5 — provider interfaces and the zero-provider guarantee.** C side: an `af_neural_provider` vtable
{generate, cancel, capability}. Kotlin side: a `NeuralProvider` interface with the procedural implementation
in-package and any generated implementation out-of-package. The guarantee is three tests (M1): the APK audit
asserts **no INTERNET permission** (precedent: the same audit step in the v0.10.1 build record, §8.6.6); a
grep gate fails the build if the procedural package imports any provider; and a runtime assertion proves
`generate()` with zero providers returns a procedural result without touching a network.

M0, M1, M2, M3, M5 and M6 cite this section for their freezes; a brief that contradicts §3.P is wrong, and
the contradiction is fixed in the brief, not in the freeze.

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

**M5 is now the longest chain *inside* a single mission, and the map above does not show it.** Staging M5
(§5.7 field 5) exposes an internal sequence `S0 → S-A → S-B → S-C → S-D → S-E`, where **S-D** (morph-target
application on import) needs both S-A and S-C, and **S-E** (facial animation on an imported face) needs S-D.
Two consequences for scheduling:

- The two chains above are still the longest **inter-mission** paths, but M5's internal path is longer than
  any single mission previously appeared. If the owner wants facial animation on imported models, the honest
  lead time is six sequential stages, not one mission — and **S-E is unreachable without S-D**, which was
  deferred and unowned until 2026-11-26.
- **S0 gates the rest and produces no code.** The API decision (freeze **F-3**, decision **D4**) must be
  committed and reviewed before S-A starts; implementing before deciding is how this repository acquired two
  animation systems (§7 item 3). M5 may therefore run in parallel with M4 and M6 **only from S-A onwards**.

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

**Each prompt carries the ten required fields**, in this order, exactly as the owner specified on
2026-11-26: **1** exact objective · **2** repository and correct working branch · **3** files and modules to
inspect or change · **4** required interfaces and dependencies · **5** **detailed implementation
instructions** · **6** acceptance criteria · **7** required tests and build evidence · **8** **runtime
verification requirements** · **9** README handover requirements · **10** explicit exclusions and blockers.
All ten are machine-verified present and correctly ordered in all ten briefs. Field **5** is written so that
the implementing agent does not have to redesign anything: it names the files, the existing patterns to copy,
the traps to avoid and the order of work. Field **8** is separated from field **7** deliberately — *build and
test evidence* proves the code compiles and its tests pass, while *runtime verification* proves it behaves on
a device, and this repository's history (§1.5) is a record of confusing the two.

The four additions from the owner's earlier approval are folded into the relevant briefs: asset provenance
(§3.I), the learning-revision benchmark gate (§3.J), Android-first placement (§3.K) and the SM-S921B resource
budget (§3.L).

**M5 is staged** (S0 → S-A → S-B → S-C → S-D → S-E) because "animation" covers five distinct capabilities —
skeletons, retargeting, playback, morph targets and facial animation — that were previously conflated under
one mission and one deferred exclusion. Each stage has an entry condition and its own exit evidence.

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
>     branches or development objectives in one pull request. §1.0 defines the four projects and their
>     boundaries: 3D-Mega-Hub is the final integrated app, AvatarForge is the separate standalone
>     engine/SDK (here as the ported `:avatarforge-engine` module), 3DDD is a separate donor whose renderer
>     was never imported, and Cue-Next is entirely separate — `grep` finds zero references to it in this
>     repository.
> 12. **Never bridge two parameter spaces by resemblance.** No external model's parameter may be written
>     into an `af_spec` field except through a committed **calibration record** at status `CALIBRATED`
>     (§3.O.2). `[0,1]` is not a unit; the engine's fields are metres, years, centimetres, unitless indices
>     with non-linear anchors, signed indices and **linear-sRGB** triplets, and their sentinels are
>     load-bearing — `0` means *derive* for every `*_cm` field and every colour triplet, `<0` means AUTO for
>     the material overrides. MHR's 45 `identity_coeffs` and the engine's 45 wire floats are **not** the same
>     45 (§3.O.1).
> 13. **Three permissions, not one.** Every asset records `may_preview_locally`, `may_redistribute` and
>     `may_train_on` separately; each defaults to `unknown`, and `unknown` behaves as **deny** for
>     redistribution and training while still permitting local preview (§3.I.2). Fitting a threshold or a
>     calibration to a dataset is training. The owner's own material is denied redistribution and training by
>     policy, not by licence.
> 14. **Invent no ownership and no training rights.** The engine port has **no declared licence** (D11) and
>     the engine's **output** has none either (D9). Until the owner declares both, the set of assets eligible
>     for training is **empty** and any claim that generated avatars are "free data" or "licence-clean" is a
>     violation of rule 2, not a simplification.
> 15. **Each mission brief has ten fields**, and field 8 (runtime verification) is not field 7 (tests and
>     build evidence). A green `ctest` and a green Gradle build are not runtime verification; only a named
>     device produces `REAL + VERIFIED`.
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

> **1. Exact objective.** Create the Neural Core boundary without changing any runtime behaviour. Publish
> freeze **F-0**: the `afneural.h` C ABI skeleton, the `procedural/` versus `generated/` package split, the
> `AF_ENABLE_NEURAL` CMake option, the **protected-component declaration** (§3.M.1), the **Android-first
> placement declaration** (§3.K) and `docs/NEURAL_CORE_BOUNDARY.md`.
>
> **2. Repository and correct working branch.** `davealone69-gif/3d-mega-hub`, a new branch cut from
> `integration/avatarforge-renderer-port` **after PR #17 lands**. Target: a pull request into that trunk,
> merge commit, no squash.
>
> **3. Files and modules to inspect or change.** New: `avatarforge-engine/src/main/cpp/include/afneural.h`;
> `avatarforge-engine/src/main/java/com/davemega/hub/avatarforge/neural/procedural/` and `.../neural/generated/`
> (each with a `package-info` or README stating its rule); `docs/NEURAL_CORE_BOUNDARY.md`. Modified:
> `avatarforge-engine/src/main/cpp/CMakeLists.txt` (add `AF_ENABLE_NEURAL`, **default OFF**),
> `avatarforge-engine/build.gradle.kts` only if the option needs surfacing. **Not modified:**
> `include/afcore.h`, `avatarforge_jni.cpp`, `AvatarSpec.kt`.
>
> **4. Required interfaces and dependencies.** None. Wave 0, parallel with M8. Read §3.0 (placement) and §3.H.1 (separation rules)
> first.
>
> **5. Detailed implementation instructions.** Work in this order; each step is checkable before the next.
>
> **(a) The CMake option — copy the precedent, do not invent one.** `CMakeLists.txt:51` already contains
> `option(AF_ENABLE_VULKAN "Build the Vulkan backend into libafrender (the Hub never selects it)" OFF)`.
> Add `option(AF_ENABLE_NEURAL "Build the Neural Core subsystem into libafcore (off by default)" OFF)`
> immediately below it, and mirror the same three-part shape the Vulkan option uses: a guarded
> `list(APPEND ...)` of sources, a guarded `target_compile_definitions(afcore PRIVATE AF_ENABLE_NEURAL=1)`,
> and nothing else. Neural sources attach to **`afcore`**, not to `afrender` — `afrender` is built only
> `if(ANDROID)` (`CMakeLists.txt:52-71`) and Neural Core must also build for host `ctest`.
>
> **(b) `include/afneural.h` — declarations only.** Include guard `AF_NEURAL_H`, one `extern "C"` block,
> `#define AF_NEURAL_ABI_VERSION 1`, and every public struct beginning with `int32_t struct_size;` plus a
> matching `af_*_init()` that sets it to `sizeof(...)` — the exact versioning pattern `af_spec` uses
> (`afcore.h`'s `struct_size` comment: *"forwards-compatible struct growth"*). **No definitions, no
> provider headers, no `#include <string>`.** The only function with a body in this mission is
> `int32_t af_neural_abi_version(void);` declared here and defined in the stub below.
>
> **(c) One translation unit, compiled only when the option is ON.** `src/neural/af_neural_abi.cpp`
> containing just `#include "afneural.h"` and the `af_neural_abi_version` definition returning the constant.
> Do **not** add it to `AF_RENDER_SOURCES`.
>
> **(d) The two Kotlin packages.** Under
> `avatarforge-engine/src/main/java/com/davemega/hub/avatarforge/neural/`, create `procedural/` and
> `generated/`. Kotlin has no `package-info.java`, so each gets a marker object carrying its rule as a
> compile-time constant, e.g. `internal object ProceduralBoundary { const val RULE = "Deterministic engine code. Must not import from ..neural.generated (§3.H.1)." }`,
> plus a `README.md` in each directory stating the same rule in prose. No other Kotlin in this mission.
>
> **(e) The import-isolation test — a source scanner, because a host C++ test cannot see Kotlin imports.**
> Add `src/test/cpp/neural_boundary_test.cpp` and register it exactly like the existing pairs at
> `CMakeLists.txt:88-136` (`if(BUILD_TESTING AND NOT ANDROID)` → `add_executable` →
> `target_link_libraries(... PRIVATE afcore)` → `add_test(NAME neural_boundary_regression COMMAND ...)`).
> Pass the source root in with
> `target_compile_definitions(neural_boundary_test PRIVATE AF_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")`,
> then have the test walk `neural/procedural/**/*.kt` and fail on any line matching
> `import\s+.*\.neural\.generated`, printing `file:line`. Also assert `neural/generated/**/*.kt` never
> re-exports a procedural type by `typedef`/`actual` aliasing. **The test must fail loudly if the directories
> are missing** — a scanner that silently finds nothing is a false green.
>
> **(f) The protected-component declaration as data, not prose.** Write
> `docs/NEURAL_CORE_PROTECTED.json`: an array of objects `{"path": "...", "reason": "...", "since": "..."}`
> covering every entry of §3.M.1 verbatim. Add a second host test that reads it and asserts each `path`
> exists in the tree at `AF_SOURCE_ROOT`, so a rename breaks the build instead of silently un-protecting a
> component. Enforcement of *"no change without a human-authored commit"* belongs in CI, not in ctest: add
> `.github/scripts/protected-path-gate.sh` that reads the same JSON, diffs
> `git diff --name-only "$BASE".."$HEAD"`, and exits non-zero if any protected path changed without
> `Human-authored:` in a commit message in that range. Wire it into `android.yml` as its own step. **Do not
> make it delete, revert or rewrite anything** — it reports and fails.
>
> **(g) The placement matrix as data.** `docs/NEURAL_CORE_PLACEMENT.csv` with header
> `capability,phone,workstation,ci,evidence` and one row per capability of the §3.K matrix, using only the
> values `yes`, `no`, `blocked-licence`, `blocked-hardware`, `n/a` and a citation for `evidence`. A third
> host test asserts the CSV parses, has no empty `evidence` cell, and that no row claims `phone=yes` for a
> capability whose §3.K row says otherwise.
>
> **(h) Prove zero behaviour change.** Build twice from a clean tree — `-DAF_ENABLE_NEURAL=OFF` and `=ON` —
> and `sha256sum` `libafcore.so` (and `libafrender.so` where an Android build produced one). The OFF build's
> hashes must equal a pre-mission build's. Commit both hash lists into the PR description, with the exact
> `cmake` command lines used, so a reviewer can reproduce them.
>
> **(i) Do not touch** `include/afcore.h`, `src/android/af_jni.cpp`, `AvatarSpec.kt`, `:app`, or any
> existing test. If a step above seems to require it, stop and report instead.
>
> **6. Acceptance criteria.** `afcore.h` untouched; the 45-float JNI contract untouched; the
> **protected-component list of §3.M.1 is committed as machine-readable data** (not prose) with a test that
> fails if a future change touches a protected path without a human-authored commit; the **§3.K placement
> matrix is committed** stating, per capability, whether it runs on the phone, on a workstation, or in CI;
> `static_assert(33 + 1 + 3 + AF_FACE_PARAM_COUNT == kSpecFloatCount, "spec packing drifted")` still
> compiles; `afneural.h` uses the same `struct_size` versioning pattern as `af_spec`; with
> `AF_ENABLE_NEURAL=OFF` the built libraries are **byte-identical** to the pre-mission build; a test asserts
> that nothing in `procedural/` imports from `generated/`; no existing test changes.
>
> **7. Required tests and build evidence.** Host `ctest` **11 of 11** with
> `generation_contract_regression` reported *Not Run (Disabled)*, exit status 0. Two builds — option OFF and
> ON — with SHA-256 of `libafcore.so` and `libafrender.so` from the OFF build matching the pre-mission
> build. `gh workflow run android.yml --ref <branch>` fully green; quote the run id and the per-job
> conclusions. The import-isolation test's output.
>
> **8. Runtime verification requirements.** M0 changes no runtime behaviour, so runtime verification is a
> **negative** proof, and it must be performed, not assumed:
> *(a)* Install the debug APK produced by CI on the **SM-S921B** if one is available to the agent; otherwise
> on the CI emulator and labelled `CI_EMULATOR`, never `DEVICE`. *(b)* Run the existing
> `Run generated-avatar GLES diagnostic` path (**keep that label exactly**) and record triangle count,
> draw calls, native heap and first-frame time; each must match the values already recorded for the base
> head within the tolerances that head's own record states — a mismatch is a regression, not noise.
> *(c)* Prove the option is off in the shipped binary:
> `unzip -p app-debug.apk lib/arm64-v8a/libafcore.so | strings | grep -c af_neural` must print **0**.
> *(d)* Confirm the app's cold start emits **no new log tag** containing `afneural` or `NeuralCore`.
> *(e)* Every figure carries commit SHA, date, device model and build variant. Anything not measured on a
> named device is reported `UNVERIFIED` — and **no part of M0 may be reported `REAL + VERIFIED` on the
> strength of an emulator run** (§7 item 7).
>
> **9. README handover requirements.** Add a short *Neural Core* subsection under the current handover:
> what the boundary is, that `AF_ENABLE_NEURAL` defaults OFF, that no behaviour changed, the exact head
> SHA, and the CI run id. State explicitly that results belong to that head only.
>
> **10. Explicit exclusions and blockers.** No provider code, no inference, no network, no asset inspection, no metrics,
> no models, no datasets, no third-party dependency of any kind. **No model weights may ever be loadable
> into the app process** (§3.K consequence 1) — declare that constraint in the boundary document now, while
> it is free to declare. Do not implement any later mission's
> interface beyond the empty declaration. Do not touch `:app`. Do not enable the option by default.

### 5.2 M8 — Device verification and CI gate repair

> **1. Exact objective.** Convert this repository's `IMPLEMENTED IN SOURCE` claims into `REAL + VERIFIED`, and
> attribute, fix or obtain a waiver for the failing generated-avatar runtime gate. **Runs from wave 0 and
> continuously — it is on no critical path but gates the credibility of every other mission.**
>
> **2. Repository and correct working branch.** As §5.0. This mission needs a **real Android toolchain**: a device or
> emulator with JDK, Android SDK, NDK and (for the emulator) KVM. A host-only workspace cannot complete it.
>
> **3. Files and modules to inspect or change.** Start from `docs/ANDROID_TEST_AGENT_PROMPT.md` — the standing brief,
> with toolchain values, ordered commands, the verbatim `Avatar LOD selected:` log format and the pass
> criteria. Also: `.github/workflows/android.yml`, `avatarforge-generated-runtime.yml`,
> `publish-phone-test-apk.yml`; `app/src/androidTest/.../GeneratedAvatarRuntimeTest.kt`;
> `FilamentPreviewView.kt`; `src/render/af_lod_select.h`; and `kotlin-compile-gate.yml`, which exists on
> **`arena/25bd4008-3d-mega-hub` only**.
>
> **4. Required interfaces and dependencies.** None — start immediately. **M3 needs at least one device record from this mission**,
> and M7 needs its device measurements, so produce those early and publish them even before the gate is
> resolved.
>
> **5. Detailed implementation instructions.**
>
> **(a) Establish what actually fails, on hardware the agent controls.** The gate
> *"Generate, render and inspect real avatar pixels"* fails at **step 8, `Run generated-avatar
> instrumentation test`**. Its last recorded failure on a **documents-only** head (`281806e7`, run
> `45813704590`) proves the failure is not caused by the code delta between that head and its parent — a
> markdown file cannot fail an instrumentation test. The GitHub API yields no test name for it (annotations
> give only *"The process '/usr/bin/sh' failed with exit code 1"*) because logs and artifacts are served from
> hosts outside a restricted workspace. **So do not diagnose from the API.** Check out the head, build the
> APK, and run the instrumentation test yourself with
> `./gradlew :app:connectedDebugAndroidTest --tests '*GeneratedAvatarRuntimeTest*'` (or the module's
> equivalent), capturing the full stack trace and the test's own diagnostic lines.
>
> **(b) Separate flake from fault by repetition, not by reading.** Run the same head **at least five times**
> on the same device/emulator, recording per run: pass/fail, `frames`, `draws`, `triangles`, wall-clock to
> first presented frame, and the failure message if any. The known failure text is *"no drawable EGL-presented
> model frame within 15 seconds"* with `frames=2`, `draws=29`, `triangles=1626176` (= LOD0), and the same
> test has previously flipped pass/fail on a single commit via a PixelCopy timeout. Five identical failures
> ⇒ fault. A mix ⇒ flake, and then the *variance* is the finding.
>
> **(c) Fix the cause; never the assertion.** §7 item 5 forbids weakening a failing test to go green. The
> legitimate outcomes are: fix the renderer/lifecycle defect; or, if the measurement shows the emulator
> genuinely needs longer than the hard-coded 15 s for a 1.6 M-triangle first frame, replace the magic number
> with a **named, measured** constant whose comment cites the five-run table from (b) — and say so in the PR.
> Raising a timeout without that table is weakening the test.
>
> **(d) Recover the compile gate from the branch that owns it.** `arena/25bd4008-3d-mega-hub`
> (`b79a7d0c362d8e25df21238d5085f47aa5b71084`) is the **sole owner** of `.github/workflows/kotlin-compile-gate.yml`.
> Recover it read-only with
> `git show b79a7d0c:.github/workflows/kotlin-compile-gate.yml`, review it, and land it as its own commit.
> **Do not delete or force-push that branch** (§8.4 rule 3).
>
> **(e) Make `arena/**` testable.** `.github/workflows/android.yml` fires on
> `branches: [main, "work/**", "fix/**", "integration/**"]` for `push` and `[main, "work/**", "fix/**"]` for
> `pull_request` — so it never runs on an `arena/**` push, and `workflow_dispatch` refuses a bare SHA
> (`422 No ref found`). Document the two working invocations in `docs/ANDROID_TEST_AGENT_PROMPT.md`:
> `gh workflow run android.yml --ref <branch-name>` and a PR-triggered run. Do not add `arena/**` to the
> trigger list without an owner decision — that would run the full matrix on every agent branch.
>
> **(f) Produce the device evidence table** required by field 8 and append it to `AGENTS.md` beside the
> existing real-device results table (line ~180) **as a new dated row**, never by editing the historical one.
>
> **(g) Leave the P1 stale-session defect diagnosed even if unfixed.** `docs/PHONE_TEST_2026-11-15.md`
> records a stale session overwriting the current session's record after returning from the file picker and
> re-importing an 83 MB file for nothing, with a `FAILED` record citing *"Surface was released while the
> renderer was starting or drawing: … eglCreateWindowSurface failed"*. Reproduce it or disprove it, with the
> log lines. If unfixed, it stays an open blocker with evidence — not a rumour.
>
> **6. Acceptance criteria.** Four jobs, in order:
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
> **7. Required tests and build evidence.** For every result: the exact commit SHA, the device model,
> Android version and GPU. Raw log lines, not summaries. Repeated-run counts wherever flake is a
> possibility. The workflow run ids. Whether the same-SHA APK was inspected, and its SHA-256. If a job could
> not be run, say which and why — an absent measurement is `not measured`, never an inferred pass.
>
> **8. Runtime verification requirements.** This mission **is** runtime verification, so its bar is the
> highest in the plan:
> *(a)* **≥5 repeated instrumented runs on one named head**, tabled per run as in 5(b), with the device model,
> Android version, build variant and APK SHA-256 above the table. *(b)* **One real SM-S921B session** through
> the existing diagnostic path, recording: FPS over the last 240 frames, frame-time median/p95/p99/max,
> native heap at start and end, GPU request totals (textures / geometry / render targets), quality-level
> transitions with the frame-time evidence that caused each, generated-avatar import ms, first-frame ms,
> and GL/EGL error codes. *(c)* **Carry the honest gaps forward verbatim**: heat, battery and sessions longer
> than 38 s remain `NOT MEASURED`; level-2 frame times remain `PARTIAL` unless actually captured. *(d)* Every
> number tagged `REAL + VERIFIED` **only** with a named physical device; emulator results are tagged
> `CI_EMULATOR` and may never be described as phone results. *(e)* The historical baseline in §3.L.1 belongs
> to the APK of `797d156` (2026-11-15) and must be presented beside the new figures **as a baseline to beat**,
> never merged into them or quoted as current (§7 item 14). *(f)* Results belong to the head that produced
> them; if any later commit lands on the branch, the table is re-run or explicitly marked stale (§7 item 6).
>
> **9. README handover requirements.** Update the exact-head evidence record with the new head's results,
> and **state which earlier results are now superseded** — a result never transfers to a different commit.
> Record the runtime-gate verdict and its attribution, the observed LOD behaviour with device identity, and
> that `kotlin-compile-gate.yml` now runs on the trunk. Keep the standing rules visible: an emulator pass is
> never a Samsung pass, and readiness stays **NOT READY** until the production generated-GLB-to-GLES
> SurfaceView test passes on one exact head with all required Android jobs green and the same-SHA APK
> inspected.
>
> **10. Explicit exclusions and blockers.** Do not fix the gate by weakening, skipping or disabling the test, or by
> widening a timeout to make it pass — that hides the defect this mission exists to find. Do not "resolve"
> the Filament `sheenColorIndex` `PreconditionPanic` by suppressing it. Do not publish an ungated APK
> (`publish-phone-test-apk.yml` requires `commit` and `expected_apk_sha256`, and its readiness gate must
> stay in front of publication). Do not claim a phone result from an emulator, or a Samsung result from any
> other device. Do not attribute a failure to a commit without an A/B run against a second head.

### 5.3 M1 — Neural Gateway and the zero-provider guarantee

> **1. Exact objective.** Build the provider-independent intelligence interface (§3.A, §3.A.1) **and prove the
> zero-provider guarantee** (§3.G). Publish freeze **F-5**.
>
> **2. Repository and correct working branch.** As §5.0. One branch, one PR into the trunk.
>
> **3. Files and modules to inspect or change.** New, under `avatarforge-engine/.../neural/`: `NeuralProvider`,
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
> **4. Required interfaces and dependencies.** **F-0** (M0). Reuses `DeviceResourceGovernor` for thermal back-off — do not write a
> second resource sampler. No dependency on M2 or M3.
>
> **5. Detailed implementation instructions.**
>
> **(a) Put the gateway in the engine module, not in `:app`.** AvatarForge is the standalone engine/SDK
> (§1.0), so its intelligence boundary lives at
> `avatarforge-engine/src/main/java/com/davemega/hub/avatarforge/neural/gateway/`. `:app` may call it; it may
> never be required by it. Freeze **F-5** is exactly this surface: `NeuralGateway`, `CapabilityProbe`,
> `ProviderTransport`, `ProviderDescriptor(tier, id, baseUrl, costModel, licence)`, and the `CapabilityStatus`
> enum with the §3.G values (`NO_PROVIDER`, `PROVIDER_UNREACHABLE`, `TIMEOUT`, `THERMAL_BACKOFF`,
> `BUDGET_EXCEEDED`, `OUTPUT_REJECTED_BY_VALIDATOR`, `LICENCE_BLOCKED`). Publish F-5 in
> `docs/NEURAL_CORE_CONTRACTS.md` with a version number before implementing against it.
>
> **(b) Tier 0 is a real code path, not a null object.** Implement `ProceduralOnlyTransport`: it answers
> every generative capability with `NO_PROVIDER` and routes everything else to the deterministic engine. Tier
> resolution is ordered 0 → 1 (local Ollama) → 2 (local free weights on the user's GPU) → 3 (paid,
> user-funded, user-configured, disclosed opt-in) and **stops at the first tier that is both configured and
> reachable**. A tier is never selected implicitly: tier 3 requires an explicit user setting plus a visible
> cost/licence disclosure at the moment of the call.
>
> **(c) One result type, no partial success.** `sealed class CapabilityResult` with
> `Ok(spec: AvatarSpec, receipt: ArtifactReceipt)` and `Unavailable(status: CapabilityStatus, detail: String)`.
> **No nullable spec, no half-filled spec, no `Ok` without a receipt.** A caller that cannot distinguish
> "no provider" from "provider refused" will eventually render a default avatar and call it a result.
>
> **(d) The validation boundary is total, and it never clamps.** Provider output is parsed into `AvatarSpec`
> through the validation that already exists in `AvatarSpec.kt`. If **any** field is out of range, the whole
> proposal is rejected with `OUTPUT_REJECTED_BY_VALIDATOR` and the offending field named in `detail`. Silent
> clamping is how a model's hallucinated `height_m = 47.0` becomes a 47-metre avatar with a green test. Note
> the sentinel traps while validating: `0` means *"derive from height/build"* for every `*_cm` tape field,
> `<0` means AUTO for `cloth_roughness`/`cloth_metalness`/`gear_metalness`, and `0` means auto for the colour
> triplets — so a provider that legitimately means "zero" cannot express it, and the gateway must not
> reinterpret those values (§3.O).
>
> **(e) Reuse the engine's existing execution discipline.** `docs/AVATARFORGE_INTEGRATION.md` records that
> the facade already *"runs native work on an interruptible background dispatcher, checks
> memory/storage/thermal state, caps the engine budget at 1.5 GiB and at half measured free RAM"*. Route
> gateway calls through that same path; do not build a second scheduler, a second thermal check or a second
> budget. One named timeout constant per transport, all cancellable.
>
> **(f) Demote the chat page; do not delete it.** `app/src/main/java/com/davemega/hub/MainActivity.kt` and
> `OllamaEndpoint.kt` today form a dead end whose output reaches nothing (§1.5). Retarget them into a console
> **over the gateway**: the page shows the resolved tier, the `CapabilityStatus` of each capability, and the
> receipt of the last call. Keep the UI label `Run generated-avatar GLES diagnostic` byte-for-byte. Do not add
> a second chat surface.
>
> **(g) Receipts field-for-field.** Every provider call emits an `ArtifactReceipt` mapping one-to-one onto
> `af_asset_provenance` (§3.I), including `licence_verified`, which may only be `1` where the licence text was
> read from a primary source (§6.3 method).
>
> **(h) The exit gate is a CI job that proves the negative.** Add
> `.github/workflows/neural-tier0-gate.yml`: it runs the entire Tier-0 path with the provider URL set to an
> unreachable address (`http://127.0.0.1:1/`), then asserts (i) the generated GLB's SHA-256 equals the
> committed golden, (ii) every generative capability reports `NO_PROVIDER`, (iii) no test exceeds a bounded
> wall-clock — a hang is a failure, not a timeout to be tolerated. **This job is M1's exit gate**: M2 and
> later may not start until it is green on the M1 head.
>
> **(i) Licence hygiene.** No model weights enter the repository or the APK. Any new third-party library is
> recorded in `THIRD_PARTY_NOTICES.md` **before** first use, per `AGENTS.md`. Ollama is the user's own local
> server; the gateway must work when it is absent.
>
> **6. Acceptance criteria.** Two or more providers behind one interface; a new provider needs no engine
> change. With **zero providers configured**: generation is **byte-identical** to the pre-mission golden
> SHA-256, asset inspection verdicts unchanged, and **no network call is attempted** — proven by pointing
> the HTTP layer at an unreachable host and still succeeding. Fallback order ends at
> `af_engine_analyze_prompt`, treated as success, not error. `ModelOutputValidator` bounds-checks against
> `af_spec`'s real limits and honours the adults-only rule; out-of-bounds output degrades to
> `OUTPUT_REJECTED_BY_VALIDATOR` and falls through. `Proposed<AvatarSpec>` is not assignable to
> `AvatarSpec`. No inference on the render thread. No key in the repository, any log, or any diagnostic
> report. `LICENCE_BLOCKED` is a working degradation reason.
>
> **7. Required tests and build evidence.** The **Tier-0 CI job** (§3.G.4) added to `android.yml` and
> green, with all four assertions visible in its output. JVM unit tests for routing, fallback, validation,
> thermal back-off and degradation reasons — a fake provider is legitimate here as a *test* double, not a
> shipped mock. Both golden SHA-256 values (before and after) quoted. `ctest` 11 of 11 on host. CI
> `android.yml` fully green including lint and R8; run id quoted. Live-model behaviour is **UNVERIFIED**
> unless you ran a real Ollama — then name the model, its version and the host.
>
> **8. Runtime verification requirements.**
> *(a)* **Airplane mode, nothing configured:** generate an avatar on the device and compare its SHA-256 to the
> host golden — it must be **byte-identical**. The UI must name `NO_PROVIDER`, not spin, not show a generic
> error. *(b)* **Provider present but stopped:** expect `PROVIDER_UNREACHABLE` within the named timeout;
> record the measured elapsed time against the constant. *(c)* **Provider running with a free local model:**
> one end-to-end proposal → validated `AvatarSpec` → generated avatar; record the receipt and output SHA-256;
> then feed a deliberately out-of-range proposal and confirm `OUTPUT_REJECTED_BY_VALIDATOR` names the field
> **and** that the procedural fallback still yields the byte-identical golden. *(d)* **Cost of the gateway:**
> measure added latency and native-heap delta against the pre-M1 head; both must sit inside §3.L.2's budget
> (**≤8 MB added RAM, zero for weights**) or the mission has failed its own constraint. *(e)* **Thermal:**
> if elevated `PowerManager.currentThermalStatus` can be induced, show `THERMAL_BACKOFF`; if it cannot, report
> `UNVERIFIED` — do not assert the behaviour. *(f)* No ANR, no crashed process, GL/EGL error count `0x0`, and
> every figure tagged with commit SHA, date, device and build variant.
>
> **9. README handover requirements.** Record: the provider interface and how to add one; the tier table
> from §3.G.1; the Tier-0 guarantee **with the CI job name and run id**; what changed in the AI page; that
> paid providers are opt-in and never required; and every status word, including that no live model was
> exercised unless it was.
>
> **Also map `ArtifactReceipt` field-for-field onto `af_asset_provenance`** (§3.I rule 5): bytes, SHA-256,
> declared licence, provider and model revision already correspond, so an artifact arrives with provenance
> attached rather than needing it reconstructed. Set `licence_verified=1` **only** for a licence read from a
> primary source per §6.3.
>
> **10. Explicit exclusions and blockers.** No image or 3D generation *implementation* — only the `ArtifactProvider`
> interface and one fake for tests. **No model weights may be loaded into the app process** — on-device text
> inference means a localhost server the user runs, whose weights live in its own memory (§3.K consequence
> 1). No model download, no weights in the repository or APK. No on-device
> inference claim. No change to the solver, `af_spec`'s meaning, or the deterministic parser. Do not make
> any feature require a provider. Do not add a paid default.

### 5.4 M2 — 3D Intelligence: asset inspection and provenance

> **1. Exact objective.** Promote the four existing Python probes into an in-engine asset diagnostic API
> (§3.B), **and implement asset provenance** (§3.I). Publish freeze **F-1**.
>
> **2. Repository and correct working branch.** As §5.0.
>
> **3. Files and modules to inspect or change.** Read first: `tools/glb_anatomy_probe.py` (338 lines),
> `tools/glb_animation_probe.py` (352), `tools/glb_quality_targets_probe.py` (531),
> `tools/glb_symmetry_sections.py` (190), and `avatarforge-engine/src/main/cpp/src/af_mesh.h`, which already
> provides `Bvh::build(mesh, leafSize=8)`, `Bvh::raycast`, `Bvh::signedDistance` and a 13-value `Region`
> enum. **First deliverable is a mapping table: probe field → engine source.** New: `af_asset_inspect` and
> `af_asset_report` **and `af_asset_provenance`** in `afneural.h`; implementation under `cpp/src/neural/`; host test
> `cpp/src/test/cpp/asset_inspect_test.cpp` registered in CMake. Reuses `af_glb_read` for parsing.
>
> **4. Required interfaces and dependencies.** **F-0**. Parallel with M1. No dependency on M3.
>
> **5. Detailed implementation instructions.**
>
> **(a) Append, never reorder.** New declarations go at the **end** of `afcore.h` in a clearly marked block.
> `af_spec`'s field order, the 45-float JNI wire contract, `kSpecFloatCount` and the
> `static_assert(33 + 1 + 3 + AF_FACE_PARAM_COUNT == kSpecFloatCount, ...)` (with `AF_FACE_PARAM_COUNT = 8`
> at `afcore.h:114`) must all still compile unchanged. Reordering one field silently corrupts every existing
> caller.
>
> **(b) Reuse the readers that already exist.** `BoundedGlbReader` (128 MiB ceiling), the sparse-accessor
> handling at `af_glb_read.cpp:427-467`, `GlbValidator.kt` and `GlbTextureBudget.kt` (448 MiB decoded-texture
> budget). **P8 applies: this mission is mostly aggregation of facts the engine already extracts.** Write new
> parsing only where a fact is genuinely not available — and say which in the PR.
>
> **(c) Inspection is read-only and non-throwing.** `af_asset_inspect(path_or_memory, af_asset_report* out)`
> returns a code and fills a versioned struct. It never mutates the file, never allocates the whole file when
> the ceiling is exceeded, and never throws across the C ABI.
>
> **(d) Codes compose, because a file can fail two ways at once.** Return
> `AF_ASSET_OK`, `AF_ASSET_FOREIGN_SKELETON_STATIC`, `AF_ASSET_TEXTURE_OVER_BUDGET`,
> `AF_ASSET_EXTENSION_UNSUPPORTED`, `AF_ASSET_NO_ANIMATION` and `AF_MORPH_NOT_READ_ON_IMPORT` as a
> **fixed-size array plus count** in the report, with `AF_ASSET_OK` only when the array is empty. A single
> scalar code would force the inspector to pick one truth and hide the other.
>
> **(e) Detect morph targets; do not drive them.** Count `mesh.primitives[*].targets` entries and whether
> their accessors are sparse, report the count, and set `AF_MORPH_NOT_READ_ON_IMPORT`. `grep -ci morph
> af_glb_read.cpp` is **0** today; after this mission it must be **non-zero** — but only for counting.
> **Driving** morph targets is M5 deliverable (f), and this mission must not pre-empt it (§7 item 4).
>
> **(f) Provenance rides inside the GLB.** Implement `af_asset_provenance` exactly as §3.I and §3.I.1 define
> it, including `derived_from_sha256` so an imported asset re-exported by the engine **chains** instead of
> inheriting the engine's licence. Write it into the existing GLB `extras` block (the exporter already writes
> morph targets there — `afcore.h:390`) and read it back on import. **No sidecar format**: a `.json` next to a
> `.glb` gets separated the first time a user shares the file.
>
> **(g) Quarantine is a state, not an error.** Where `licence` or `generation_method` cannot be stated, set the
> quarantine flag; the asset remains inspectable and reportable but is barred from distribution, APK packing
> and training data (§3.I.1 rules 1 and 3). **Never guess a licence from a filename, a URL, an extension or a
> user's assertion.**
>
> **(h) Tests.** Host `ctest` `asset_inspect_test.cpp` over the three repository assets
> (`rigged_test_figure.glb`, `humanoid_motion_pack.glb`, `female.glb`) asserting the **exact** code arrays;
> an `extras` round-trip test (export → import → compare every provenance field); a negative test with a
> truncated file and one over the 128 MiB ceiling; and an extension of M0's boundary scanner covering the new
> files (§3.H.1's import-parsing rule).
>
> **6. Acceptance criteria.** `af_asset_inspect` **reproduces `glb_quality_targets_probe.py`'s numbers on
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
> **7. Required tests and build evidence.** Run the Python probe yourself against
> `app/src/androidTest/assets/rigged_test_figure.glb` and **quote your own output** — do not trust the
> reference values below blindly. Reference from the run that rescued this probe: glTF 2.0, 240 vertices,
> 120 triangles, 4 primitives, 4 materials, 1 skin, 7 joints, max joint depth 3, root `Hips`, clips Wave
> 2.000 s / March 1.000 s / Bow 2.000 s, 0 morph slots, bounds 0.62 × 1.72 × 0.31, estimated height 1.72.
> Then the C++ test's output asserting the same values. `ctest` **12 of 12** (the new test added), with
> `generation_contract_regression` still Disabled. CI green; run id quoted.
>
> **8. Runtime verification requirements.**
> *(a)* On the device, inspect three real inputs — a generated avatar, a foreign-rig import, and an
> over-budget file — and show the diagnostic export naming the exact code array for each. **No foreign-rig
> file may be presented as animatable.** *(b)* Prove the ceiling rejects before allocating: record peak native
> heap while inspecting a large file; it must not approach the process limit or OOM. *(c)* Measure inspection
> wall-clock milliseconds for a file in the 144,073-triangle class; the historical baseline for importing that
> class was **312 ms on the APK of `797d156`** — quote it as a baseline, never as a current result
> (§7 item 14). *(d)* Prove provenance survives the device round trip: export a generated avatar on the phone,
> re-import it, read back `licence`, `generation_method` and `output_sha256`, and compare. *(e)* The owner's
> Dropbox material and `mylove.glb` are inspected **at runtime only** — never committed, never packed, and
> identified in evidence by SHA-256 rather than by path (`AGENTS.md` rule 5). *(f)* Every measurement tagged
> with commit, date, device and build variant; anything not measured on a named device is `CI_EMULATOR` or
> `UNVERIFIED`.
>
> **9. README handover requirements.** Document `af_asset_inspect`, the verdict codes, which measurements
> are implemented and which report `not measured`, the probe-equivalence evidence with both outputs quoted,
> and the exact head.
>
> **10. Explicit exclusions and blockers.** No AI, no provider, no network. Do not add Draco, meshopt, KTX2, assimp or
> Eigen — zlib is the engine's only third-party C++ dependency and adding one is an owner decision. Do not
> implement morph-target *application* on import (that is a separate capability, currently MISSING). Do not
> modify `af_glb_read.cpp`'s extension policy.

### 5.5 M3 — Evaluation harness, golden records and the benchmark suite

> **1. Exact objective.** Build §3.E: one `EvaluationRecord` type with provenance, reconciling measurements
> the repository already takes — **plus the §3.J benchmark suite and its two controls**, which are what make
> every later "no regression" claim meaningful. Publish freeze **F-2**. **This is the pivot mission — five missions wait on
> it (§4.5).**
>
> **2. Repository and correct working branch.** As §5.0. **One owner, not split across agents** (§4.6 rule 2).
>
> **3. Files and modules to inspect or change.** Read first: `RendererTelemetry.kt` (≥20 fields including fps and
> frame-interval p50/p95/p99, frames presented / not presented / without valid content, stall events, EGL
> shutdown error), `EvidenceIdentity.kt`, `DiagnosticDownloads.kt`, `DeviceResourceGovernor.kt`,
> `avatar_lod_select_test.cpp`, `animation_roundtrip_test.cpp` (already compares imported playback to the
> generating engine at **0.2 mm**), `imported_glb_pbr_test.cpp`, `generation_contract_test.cpp`. New:
> `EvaluationRecord`, `Measurement`, `Verdict`, `MetricKey` registry, `Provenance`, the tolerance table, and
> golden-record storage under `.../neural/procedural/`.
>
> **4. Required interfaces and dependencies.** **F-1** (M2) for the geometry and material metric families, plus **at least one
> device record from M8**. Depends on nothing in M4–M6.
>
> **5. Detailed implementation instructions.**
>
> **(a) Two types, and they refuse to be built incomplete.** `Measurement(value, unit, method, source)` with
> `source ∈ {HOST_CTEST, CI_EMULATOR, DEVICE, PROBE, MANUAL}`, and
> `EvaluationRecord(subject, commit, device, metrics, verdicts, provenance)`. Make `commit` and `device`
> non-nullable **and** rejected when blank at construction, so a record without provenance cannot exist. A
> record that cannot say where it came from is the exact defect §1.5 records.
>
> **(b) `UNMEASURED` is never zero.** The verdict set is `AF_EVAL_PASS`, `AF_EVAL_REGRESSION`,
> `AF_EVAL_IMPROVED`, `AF_EVAL_UNMEASURED`, `AF_EVAL_INCOMPARABLE`. Emitting `0` for a metric that was not
> taken is prohibited by `MASTER_BLUEPRINT.md` (*"no telemetry that reports missing measurements as zero"*)
> and by §2.1. `INCOMPARABLE` exists for the case that will actually happen: a metric measured on an emulator
> compared against a device baseline.
>
> **(c) Aggregate; do not re-measure.** Each of the **eight** families in §3.E.2 already has a source — the
> `Region` enum and `Bvh` in `af_mesh.h`, `glb_anatomy_probe.py`, `glb_symmetry_sections.py`, `af_uv.cpp`,
> `GlbTextureBudget.kt`, `GlbValidator.kt`, `generation_contract_test.cpp` Part E,
> `af_engine_evaluate_animation`, `animation_roundtrip_test.cpp`, `RendererTelemetry.kt`, the
> `Avatar LOD selected:` / `Avatar LOD ready:` log lines from `07610b5`, the three instrumented suites,
> `DeviceResourceGovernor.kt` and `EvidenceIdentity.kt`. M3's job is the record type, the tolerances and the
> comparison logic. Re-implementing a measurement here creates a second source of truth for the same number.
>
> **(d) Golden records are files, with identities.** Store them as JSON under
> `avatarforge-engine/src/test/golden/`, named `<subject>_<commit>.json`, each carrying the GLB SHA-256, the
> full metric vector with units and sources, and the tolerances used. Commit the goldens for the seed-1337
> subjects. A golden record is the **baseline**, so comparison is against a record, not against a memory or a
> README claim.
>
> **(e) Build the §3.J.1 frozen suite exactly as specified** — subjects (deterministic **seed 1337**, one per
> gender, one per fabric in `kFabrics`, one per iris colour in `kEyeColours`, one minimal and one maximal
> complexity), the three repository assets, **all eight** metric families, and the device contexts (host and
> CI emulator as a minimum; SM-S921B for any revision touching rendering or performance). Freeze it: the suite
> definition is data, versioned, and changing it is itself a reviewed change.
>
> **(f) Tolerances are cited, not chosen.** Symmetry deviation **0.004 model units**, animation round trip
> **0.2 mm** (already enforced by `animation_roundtrip_test.cpp`), visible pixels **≥5,000** (already enforced
> by the instrumented suites). Any new tolerance must arrive with the measurement that justifies it.
>
> **(g) The four assertions, then both controls.** No metric beyond tolerance; **at least one named measured
> improvement**; byte-identity where claimed; complete provenance. Then the **null control** — an unchanged
> revision must produce a zero diff — and the **negative control** — a deliberately worsened constant must be
> **detected and named**. Run both; a gate that has never been shown to fail is not a gate.
>
> **(h) Reproduce the byte-identity precedent, do not cite it.** `42fa78a` produced
> `male = 73,399,512 B` and `female = 72,076,316 B` byte-identical across a change. Re-run that comparison on
> the M3 head and record the actual sizes; if they differ, that is a finding, not an error in the plan.
>
> **(i) Wire the gate into CI** as a job that fails on `AF_EVAL_REGRESSION` and prints the family, metric,
> baseline value, new value, tolerance and source tag for every failing row.
>
> **6. Acceptance criteria.** The **§3.J.1 benchmark suite exists**: a frozen subject set (the
> deterministic **seed 1337** already used in this repository's evidence, one subject per gender, one per
> fabric in `kFabrics`, one per iris colour in `kEyeColours`, one minimal and one maximal-complexity spec),
> the three repository assets, **all eight** metric families of §3.E.2, and host + CI-emulator contexts. **Both controls
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
> **7. Required tests and build evidence.** A golden record for the deterministic **seed 1337** generated
> avatar on **host**, and one from the **CI emulator**, with both `commit` and `device` populated and
> **different** — quote both records. A demonstration that changing one `af_spec` field produces a
> measurable diff against the golden record, with the metric, both values and the tolerance. A written
> statement of which metrics are host-only, which need a device, and which are `not measured` today. `ctest`
> green; CI green including the emulator suites; run ids quoted.
>
> **8. Runtime verification requirements.**
> *(a)* Run the frozen suite in **both** minimum contexts — host `ctest` and the CI emulator — and table the
> eight families per context with their source tags. *(b)* Any revision touching rendering or performance must
> additionally run on the **SM-S921B**; until it does, its device families are `UNMEASURED`, not zero.
> *(c)* Show the **null control** passing (zero diff) and the **negative control** failing with the worsened
> constant named in the report — both outputs committed as evidence. *(d)* Emulator results are labelled
> `CI_EMULATOR` and may never be reported as phone results (§7 item 7); a device figure without a device model,
> commit and date is rejected by the record type itself. *(e)* Where a family cannot be measured in a given
> context, the report prints `UNMEASURED` and the reason — the absence of a number is a result, and it is
> recorded as one.
>
> **9. README handover requirements.** Document the record schema, the metric registry, the tolerance table
> with each tolerance's source, where golden records live, and the rule that a record never transfers to a
> different commit or device. Include both example records verbatim.
>
> **10. Explicit exclusions and blockers.** No AI grading of AI output — evaluation is procedural only (§3.H.2). No new
> telemetry sampling where `RendererTelemetry` already reports it. No device claims from emulator data. No
> change to the solver or the renderer. Do not weaken an existing assertion to make a metric pass.

### 5.6 M4 — Human Intelligence critics

> **1. Exact objective.** Build proportion, face and skin critics that **measure** rather than describe
> (§3.C), emitting into M3's records.
>
> **2. Repository and correct working branch.** As §5.0.
>
> **3. Files and modules to inspect or change.** Read first: `af_anatomy.cpp/.h` (spec → `BodyMetrics`: landmark
> heights and girths; a tape measurement in the spec is met exactly), `af_field.cpp/.h` (the single SDF for
> body, head, face, hands, feet, `hairShape`, `clothUpper`, `clothLower`, `upperShell`, `skirtShell`,
> `shoeShape`, `gearField`), `af_rig.cpp` (71 joints, 52 ARKit blendshapes as analytic displacements),
> `af_texture.cpp` (skin with subsurface, sheen, sheen tint, anisotropy, IOR), `af_prompt.cpp`
> (`kFabrics[]`, `kEyeColours[]`), and `docs/PHOTOREAL_UPGRADE_PLAN.md`, `docs/AVATAR_GEOMETRY_EVIDENCE.md`,
> `docs/AVATAR_QUALITY_PLAN.md`, `docs/DONOR_BASELINE.md`. New: critics under `.../neural/procedural/`,
> reference constants as data, host tests.
>
> **4. Required interfaces and dependencies.** **F-2** (M3). Parallel with M5 and M6. No dependency on M1 — critics are procedural
> and must work at Tier 0 (§3.G.2).
>
> **5. Detailed implementation instructions.**
>
> **(a) Critics are procedural, so they live in `neural/procedural/`** (§3.H.1) and must work at **Tier 0** —
> no provider, no network, no weights. A critic that needs a model is not a critic, it is a guess.
>
> **(b) One critic per existing artifact; do not build new measurement.** The proportion critic reads
> `BodyMetrics` from `af_anatomy.cpp/.h` (landmark heights and girths — the header states a tape measurement
> in the spec is met exactly, perimeter preserved to **<0.5%**). The face critic reads the 52 ARKit blendshape
> analytic displacements in `af_rig.cpp` and the `face[AF_FACE_PARAM_COUNT]` parameters. The skin critic reads
> `af_texture.cpp`'s outputs (subsurface, sheen, sheen tint, anisotropy, IOR). Each critic is a **pure
> function** from that artifact to a list of `Measurement`s — no global state, no clock, no RNG.
>
> **(c) Measurement discipline is enforced by the type, not by review.** Every measurement carries value,
> unit, method and source (M3's `Measurement`); ≥5 per critic; **no adjectives anywhere in a measurement**. A
> test asserts each unit string is non-empty and each value finite. "Good proportions" is not a measurement;
> "shoulder-to-hip width ratio 1.42, unitless, method `BodyMetrics`, source `HOST_CTEST`" is.
>
> **(d) The external reference is a calibration problem, not a lookup — see §3.O.** WHO growth standards via
> Anny's anthropometry remain the researched recommendation for a *reference population*, but **no mapping is
> assumed**. Anny's parameters are named labels in `[0,1]`; this engine's fields are metres (`height_m`,
> 1.40–2.20), years (`age_years`, 16–70), centimetres (thirteen `*_cm` tape fields), unitless indices with
> documented anchors (`body_fat`: 0.10 shredded, 0.45 heavy), signed indices (`glute`: −1..+1), and
> **linear-sRGB** triplets. **"`[0,1]`" is not a unit**, and a shared numeric range proves nothing about
> meaning. Any bridge must be a committed **calibration record** (§3.O.2) naming the unit, the semantic, the
> valid range, the anchors, the mapping function and the measurement that validated it.
>
> **(e) Until a calibration record exists, use internal references and say so.** Score against what this
> repository already proves: `af_anatomy`'s landmark heights and girths, the tape-measurement exactness
> property, the `kFabrics` and `kEyeColours` constants in `af_prompt.cpp`, and the existing **0.004
> model-unit** symmetry tolerance with `glb_symmetry_sections.py`. Tag the reference `ENGINE_INTERNAL` and
> mark any external comparison `UNVERIFIED`. **Do not emit a WHO-percentile number you have not calibrated.**
>
> **(f) The adults-only invariant is a critic, not a footnote.** Assert the age-30 default and that `bust` and
> `glute` are ignored for `age_years < 18` (the `af_spec` comment states this explicitly). A critic that
> scores a minor-shaped body is a policy failure; a test must fail if that invariant is weakened.
>
> **(g) Reference constants are data.** Put any anthropometric table in a versioned data file with a citation
> per row, not in C++ literals — so a reviewer can check the source and a future mission can replace it
> without touching code (§3.F's "improvement output is always data plus a test").
>
> **(h) Tests.** One host `ctest` per critic; a golden-record comparison against M3's frozen subjects; and a
> **negative control** that deliberately distorts one landmark and requires the proportion critic to name it.
> A gate never shown to fail is not a gate.
>
> **(i) The deformation critic — added 2026-11-26 after the owner reported *"webing on movement"* (§3.D
> item 10, B9).** This is the fourth critic, and it is the one that turns a reported visual fault into a
> number. Implement exactly the metric specified in §3.D item 10: for the named concave landmark pairs
> (axilla, inguinal, inner elbow, knuckle, neck/clavicle), pose the mesh with `af_engine_evaluate_animation`
> across a sweep of joint angles and emit **(1)** the minimum `Bvh::signedDistance` between opposing surfaces
> (`af_mesh.h:612`), **(2)** the self-intersection count in the joint region, **(3)** the region's
> surface-area change against rest, **(4)** the region's **volume** change against rest, and **(5)** the
> influence count actually used per vertex in that region **plus how many were pruned** by `af_rig.cpp:436`.
> **Instrument the axilla and inguinal fold first.** Measurement **(4)** exists because the authorised tier 2
> (dual-quaternion skinning) fixes concave collapse by inflating joints — without a volume metric, that
> inflation scores as a victory. **M4 delivers the baseline; M5 stage S-F re-runs this same critic after each
> tier**, so the critic must be callable on demand with a named subject and sweep, not only inside the frozen
> §3.J.1 suite. Read `Rig::computeWeights` (`af_rig.cpp:282-447`) before
> writing it — in particular the three-joint sharing rule at `:369`, the 1 % prune at `:436-439`, and the two
> single-joint weight-1.0 fallbacks at `:408-409` and `:442`, because measurement **(4)** exists to show
> whether those code paths are firing in the regions that web. **The first run is a baseline, not a verdict**:
> it must be recorded with no tolerance asserted, and a tolerance may only be set from a second, measured
> comparison (§2.1, P3 — no invented numbers). Do **not** attempt a fix in this mission; the remedy is M5
> stage **S-F**, and a critic that also fixes cannot tell you whether the fix worked.
>
> **(j) No new dependency, dataset, model or weight** enters the repository or the APK in this mission.
>
> **6. Acceptance criteria.** Three critics (proportions, face, skin) each emitting **≥5 measurements** with
> units into an `EvaluationRecord`. No adjectives in a measurement. The reference population is **named and
> cited**, not an invented ideal — and **no external mapping is assumed**: WHO growth standards via Anny's
> anthropometry remain the researched recommendation (§6.3.2), but any bridge from Anny's `[0,1]` phenotype
> labels to this engine's metres/years/centimetres/unitless-index/linear-sRGB fields requires a committed
> **calibration record** at status `CALIBRATED` (§3.O.2). Until one exists, critics score against
> `ENGINE_INTERNAL` references and every external comparison is reported `UNVERIFIED` (§5.6 field 5(d),(e)). Known reference points already in this repository's documents: symmetry tolerance **0.004
> model units**; a generated body is **1,148,034 triangles** where a Daz Genesis 8 body skin is **27,292**;
> textures are **one shared 2048² atlas** against **four 4096² maps**. Generated bytes must not change
> unless a golden-record diff is approved by the owner. The adults-only invariant still passes.
> **Added 2026-11-26, and the three-critic minimum above is unchanged:** a **fourth critic (deformation)**
> emits the four §3.D item 10 measurements for each named concave landmark pair, as a **baseline record with
> no tolerance asserted**, so that *"webing on movement"* (**B9**) becomes measurable before anyone attempts
> to fix it. Its output must include the pruned-influence count, because that is the evidence for or against
> tier 1 of the remedy (**D15**).
>
> **7. Required tests and build evidence.** A baseline `EvaluationRecord` for the deterministic seed with
> all critic measurements populated, quoted verbatim. `ctest` green including the new critic tests. CI
> green; run id quoted. For any external reference: the licence read from its **own repository**, recorded in
> `THIRD_PARTY_NOTICES.md` before use, with the four-question report required by §5.0 rule 10.
>
> **8. Runtime verification requirements.**
> *(a)* Run all three critics at **Tier 0** on the device — airplane mode on, no provider configured — over the
> frozen subject set, and confirm **zero network egress** (no provider call, no DNS) while records are still
> produced. *(b)* **Determinism:** the same subject twice must produce byte-identical record JSON; hash both and
> compare. *(c)* **Cost:** measure added wall-clock per avatar and native-heap delta against the pre-M4 head;
> both must sit inside §3.L.2 (**≤8 MB added RAM, zero for weights**). *(d)* Critics are geometric, so host and
> device values should agree — assert equality per measurement and report `AF_EVAL_INCOMPARABLE` with the delta
> if they do not, rather than averaging them. *(e)* Every figure tagged with commit, date, device and build
> variant; unmeasured families reported `UNMEASURED`, never zero.
>
> **9. README handover requirements.** Document each critic, its measurements and units, its reference
> population with citation and verification date, the baseline values, and what each measurement does and
> does **not** prove.
>
> **10. Explicit exclusions and blockers.** **Do not change the solver** in this mission (P1). Do not integrate Anny's
> Python/PyTorch code into the engine — it is authoring/CI-time only and cannot run on `minSdk 26`. Do not
> use the SMPL family, or any HMR model that outputs SMPL parameters (§6.3.3). Do not vendor MakeHuman
> **code** — its source is AGPL-3.0; only its **assets** are CC0 (§6.3.2). Do not relax the adults-only rule
> to improve a metric. Do not add hair-card geometry or a new texture pipeline here — those are separate
> missions with separate gates.

### 5.7 M5 — Animation Intelligence

> **1. Exact objective.** Third-party skeleton adoption, foot-contact and slide detection, IK (§3.D),
> **morph-target application on import** (owner instruction 6, 2026-11-26 — see acceptance criterion **(f)**),
> **and deformation quality: the remedy for the owner-reported *"webing on movement"* fault** (§3.D item 10,
> **B9**, stage **S-F**) — **beginning with a written API decision, not code.** Publish freeze **F-3**.
>
> **2. Repository and correct working branch.** As §5.0. The decision document and the implementation may be one PR, but the
> decision must be committed and reviewed **before** implementation starts.
>
> **3. Files and modules to inspect or change.** Read first: `afcore.h`'s import contract, which currently states a
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
> **4. Required interfaces and dependencies.** **F-1** (M2) for skeleton, joint-depth and skin-weight inspection, and **F-2** (M3)
> for the contact and slide metrics. Parallel with M4 and M6.
>
> **5. Detailed implementation instructions.** **M5 is staged, because five different capabilities are
> routinely conflated under one name.** Each stage has an entry condition and an exit evidence requirement. **A stage may
> not begin before its entry condition is met, and no stage may be reported complete on the strength of a
> later stage's evidence.**
>
> | Stage | Capability | Entry condition | Exit evidence |
> |---|---|---|---|
> | **S0** | The **API decision** (freeze **F-3**) | Both APIs read: this line's `af_engine_animation_{count,name,duration,info}` (`afcore.h:447-463`) and `af_engine_import_motions[_from_memory]` (`:524-525`), against the donor's nine `af_engine_imported_*` (`58aced02`, `afcore.h:271-286`) plus its 949-line, 94-check test | A committed decision naming the chosen API, the rejected alternative, **and what the rejected one would have cost**. No code before this is committed |
> | **S-A** | **Skeleton adoption and inspection** | **F-1** (M2) reports skeleton, joint-depth and skin-weight facts | A foreign-rig file is either adopted through an explicit mapping or reported `AF_ASSET_FOREIGN_SKELETON_STATIC` with joint count 0 — the behaviour `afcore.h:413-414` already documents. Host test with a foreign-rig fixture |
> | **S-B** | **Retargeting** | S-A | Normalise **both** rigs to a canonical T-pose **before** computing the mapping matrix (rest-pose offsets corrupt results silently), smooth per channel with an EMA or one-euro filter **after** pose lifting, then report measured joint-angle error, end-effector drift and penetration into an M3 record. No NaN transforms |
> | **S-C** | **Playback** | S-B | One playback path through `af_engine_evaluate_animation` (one column-major 4×4 skin matrix per joint) driven by `AnimationLibrary` / `AnimationPlayback`; play/pause/resume/seek semantics defined and tested; BVH round trip within **1e-4**; `animation_roundtrip_test` still passing at **0.2 mm** |
> | **S-D** | **Morph targets on import** — deliverable **(f)** | S-A and S-C | `grep -ci morph af_glb_read.cpp` returns **non-zero** (it is **0** today); sparse morph targets read and drivable; `afcore.h:413-414` amended to match reality; host test round-tripping a generated avatar's blendshapes export → import → evaluate, with a stated tolerance. Reuse the sparse-accessor precedent already at `af_glb_read.cpp:427-467` |
> | **S-E** | **Facial animation on imported faces** | **S-D only** — never before | The 52 ARKit channels driven on an imported face through its morph targets, via `FaceAnimation.kt`; per-channel displacement magnitude measured; **no channel silently zero** |
>
> | **S-F** | **Deformation quality — the remedy for *"webing on movement"*** (**B9**, §3.D item 10). **D15 is RESOLVED: tiers 1 and 2 are authorised, tier 3 is not, and `af_vertex` does not change** | **M4's deformation critic has produced a baseline record** — a fix with no baseline cannot be shown to work. **Tier 2 additionally requires tier 1 to have been measured and shown insufficient** | **Tier 1 first**, inside the existing 4 influences: region-gate the 1 % prune (`af_rig.cpp:436`), widen the three-joint sharing rule (`:369`), remove the single-joint fallbacks (`:408-409`, `:442`), smooth by distance or heat diffusion. Re-run the critic; report the **delta per landmark pair**. **Tier 2 (dual-quaternion skinning) only if the fault survives tier 1** — it changes `Rig::skin` *and* `af_gl.cpp`, cannot be host-tested, needs device verification, and trades concave collapse for **joint bulging**, so the critic must report a region-volume metric beside the collapse metric. **Both tiers edit protected components, so the §3.M.1a protocol binds**: human-authored PR, ABI unchanged, no test weakened, before/after measured, golden SHA-256 delta reported, one protected component per PR, entry added to `docs/NEURAL_CORE_PROTECTED.json`. **Tier 3 (pose correctives) is NOT AUTHORISED** — if the fault survives tiers 1 and 2, report that with numbers and re-open D15 |
>
> **Rules across the stages.** *(i)* S-A → S-B → S-C is strictly ordered; S-D needs S-A and S-C; S-E needs S-D.
> **S-F is independent of S-D and S-E** — it needs only M4's baseline — and it may therefore start as soon as
> that baseline exists, in parallel with the morph-target work. *(i-b)* **S-F must not change `af_vertex`**
> (`afcore.h:318-327`, 88 bytes): **D15 is resolved and the answer is no** — the engine keeps 4 influences,
> which is why tier 1 (weight quality) and tier 2 (dual-quaternion blending) are the authorised routes.
> *(i-c)* **Tiers are sequential, not parallel.** Do tier 1, measure it, and only then consider tier 2. Tier 2
> costs a change to two protected components and can only be verified on a device, so starting it before tier
> 1 is measured spends the expensive option first.
> *(ii)* **Learned motion generation is not a stage** — it is `BLOCKED` on licence (§3.D item 8) and must be
> reported as blocked, not deferred silently. *(iii)* Do not implement both animation APIs (§7 item 3); S0
> exists to prevent that. *(iv)* The donor branch is read-only reference material: recover its specification
> with `git show 58aced02:avatarforge-engine/src/test/cpp/imported_animation_test.cpp` and **do not delete or
> force-push `arena/089497bd-3d-mega-hub`** (§8.4). *(v)* Contact and slide metrics need **no new engine
> capability**: skin the rest mesh with `af_engine_evaluate_animation`, track ankle and toe landmarks from
> `BodyMetrics`, represent contact as a per-frame **F×4 binary array** (left heel, left toe, right heel, right
> toe), and report slide **in millimetres** into an M3 record. Reimplement independently — the published
> reference is under the non-commercial Adobe Research License (§6.3.3). *(vi)* Validate contact **after**
> retargeting, not before; foot-skating is mean foot velocity during contact frames.
>
> **6. Acceptance criteria.**
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
> *(g) Deformation quality — stage S-F, added 2026-11-26 after the owner reported **"webing on movement"**
> (§3.D item 10, **B9**); **D15 resolved the same day — tiers 1 and 2 authorised, tier 3 not**.* M4's
> deformation critic must show a **measured improvement** in the concave landmark pairs it baselined — minimum
> `Bvh::signedDistance`, self-intersection count, region area change, **region volume** and pruned-influence
> count, each reported **per landmark pair** with the sweep angles used, **axilla and inguinal fold first**.
> **`af_vertex` must not change** — 88 bytes, 4 influences, per D15. **Tier 1 must be measured before tier 2
> is attempted**, and tier 2's report must show the bulging/volume metric as well as the collapse metric,
> because dual-quaternion skinning can fix one and worsen the other. Every edit to `af_rig.cpp` or `af_gl.cpp`
> follows the **§3.M.1a protocol**: human-authored PR, no test weakened, before/after measured, golden SHA-256
> delta stated, one protected component per PR, register entry added. **No pose-corrective data or learned
> model may be introduced** — that is tier 3 and is **NOT AUTHORISED**. If tier 1 produces no measured
> improvement, the honest outcome is a report saying so with the numbers and a request to re-open D15 — **not**
> a claim that webbing is fixed, and not a quiet move to tier 3.
>
> **7. Required tests and build evidence.** The decision document, committed. For **(f)**: the new
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
> **8. Runtime verification requirements.**
> *(a)* On the device, play the repository's `humanoid_motion_pack.glb` on **both** a generated avatar and an
> imported rigged figure; record frames presented, draw calls, triangle count, native heap and FPS over the
> last 240 frames for each. *(b)* Report **foot-slide in millimetres on a named clip**, with contact intervals,
> inside an `EvaluationRecord` carrying `commit` and `device`. *(c)* For **S-E**, show per-channel displacement
> magnitudes for the 52 ARKit channels on an imported face and prove none is silently zero. *(d)* No animation
> may be reported as working without a measured presented-frame count at or above the existing instrumented
> floor; "it played" is not evidence. *(e)* Heat, battery and sessions beyond the measured window remain
> `NOT MEASURED` unless actually measured (§3.L.1's banner applies to this mission's numbers too). *(f)*
> Emulator playback is `CI_EMULATOR`; only a named physical device yields `REAL + VERIFIED`.
>
> **9. README handover requirements.** Record the chosen API and the rejected alternative with its reason;
> the contact and slide metric definitions with units; the measured baseline; the status of third-party
> skeleton adoption (it moves from MISSING only when a test proves it); and the standing note that
> `arena/089497bd-3d-mega-hub` **must not be deleted** until the fixture and licence questions are settled.
>
> **10. Explicit exclusions and blockers.** **No motion foundation model — MotionGPT, MDM or similar. This is BLOCKED on
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

> **1. Exact objective.** Build §3.F.1–F.6 (experience memory, approved feedback, experiment history, golden
> quality measurements, regression detection, dataset management), **the §3.J revision gate, the §3.M
> autonomous-improvement guards, and §3.N recovery**. Publish freeze **F-4**.
>
> **2. Repository and correct working branch.** As §5.0.
>
> **3. Files and modules to inspect or change.** Read first: `af_prompt.cpp`'s `kFabrics[]` and `kEyeColours[]`;
> `generation_contract_test.cpp` **Part E**, which asserts a fabric through the parser, the live material
> table, the exported GLB bytes and the re-import round trip; and the `THIRD_PARTY_NOTICES.md` entry
> *"Clothing-fabric PBR values and iris swatches (3D4ME-derived constants)"*. **That is the pattern to
> generalise — do not invent a new one.** New: `Experience`, `Decision`, `ExperimentRecord`, golden-record
> promotion and comparison, the dataset manifest, all under `.../neural/procedural/`. Storage: app-private
> on device, exported JSON off device, reusing `DiagnosticDownloads.kt`'s existing path.
>
> **4. Required interfaces and dependencies.** **F-2** (M3) — golden records and regression detection are meaningless without the
> evaluation schema. Parallel with M4 and M5. **M9 depends on this mission's F-4.**
>
> **5. Detailed implementation instructions.**
>
> **(a) Storage: append-only JSONL plus a manifest — deliberately not a database.** Room is the obvious choice
> and is deliberately rejected here with evidence: `docs/DONOR_BASELINE.md` records that the 3DDD donor's
> `main` audit failed at Gradle `kspDebugKotlin` with Room KSP reporting `IllegalStateException: Empty schema
> file`. Adding Room adds a KSP build step, a schema-export obligation and a dependency to
> `THIRD_PARTY_NOTICES.md`, in exchange for queries this subsystem does not need. Store experience memory,
> approved feedback, experiment history and decision logs as **append-only JSONL** under app-private storage
> (`filesDir`), with a versioned manifest naming each file, its schema version and its SHA-256.
>
> **(b) Freeze F-4 is the memory schema.** Publish it in `docs/NEURAL_CORE_CONTRACTS.md` before writing a
> record: field names, types, units, the schema version, and the rule that a record with an unknown schema
> version is **refused, not migrated silently**.
>
> **(c) The protected set is data, and the learning system has no write access to it.** Read
> `docs/NEURAL_CORE_PROTECTED.json` (created in M0) at runtime and assert that every write the learning system
> performs targets the **writable** set only: new constant-table rows, golden records on human approval,
> memory / decision / experiment files, adapter manifests, and new files under `neural/`. A test must fail if a
> proposal names a protected path.
>
> **(d) propose → shadow → compare → report → HUMAN decides.** There is **no automatic apply path** and no
> code branch that could become one. A proposal is a diff of *data* plus a *test*, landed as a PR (§3.F). One
> kill switch — a single persisted setting, default **off** — that stops proposal generation entirely; flipping
> it must be provable from logs.
>
> **(e) Shadow mode must not perturb the deterministic path.** While shadow-running, generated output must stay
> **byte-identical** to the Tier-0 golden. A test asserts this: run the frozen subject set with learning
> enabled in shadow and compare SHA-256 against M3's goldens. Any difference means the learning system touched
> the solver, which §7 item 2 forbids.
>
> **(f) The revision gate is §3.J, invoked not reimplemented.** On every proposed revision: run the frozen
> suite, evaluate the four assertions, and run **both** controls (null: unchanged revision ⇒ zero diff;
> negative: a deliberately worsened constant must be detected **and named**).
>
> **(g) Recovery is §3.N, and it reuses the existing export path.** Export the bundle through
> `DiagnosticDownloads.kt` to `Downloads/3D Mega Hub Diagnostics` with SHA-256, commit and device recorded.
> Implement the three rollback levels (manifest repoint / `git revert` with a merge commit / restore from a
> recorded SHA). **Restore is validated, never trusted:** check schema version, verify the hash, dry-run against
> the §3.J.1 suite, and **refuse to overwrite a newer record silently** — report and stop instead.
>
> **(h) Training rights are not assumed.** No external dataset enters the repository or the APK. The engine's
> own output may become a usable dataset **only once D11 (the engine port's own licence, undeclared at the
> pinned donor SHA) and then D9 (the licence of the engine's output) are declared by the owner**; until
> then generated assets are **quarantined from training** (§3.I.1 rules 1 and 3) and `dataset management`
> records them as `NOT_ELIGIBLE_FOR_TRAINING`. Note the prior gap: `docs/AVATARFORGE_INTEGRATION.md` records
> that the donor had **no repository-level `LICENSE`**, so the engine's own licence is undeclared too (**D11**).
> **Invent no ownership and no training rights** — the honest value is "undeclared, owner decision pending".
>
> **(i) Tests.** Memory append/read round trip; schema-version refusal; the regression detector firing on a
> known-bad revision; the kill switch stopping proposals; shadow-mode byte-identity; restore refusing a newer
> record; backup bundle hash verification.
>
> **6. Acceptance criteria.** Experience memory is append-only, content-addressed by subject hash, bounded
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
> **7. Required tests and build evidence.** A demonstration that changing one constant is caught by
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
> **8. Runtime verification requirements.**
> *(a)* On the device, enable learning in **shadow mode**, generate the frozen subject set, and confirm
> **byte-identical** output SHA-256 against the host goldens — shadow learning that changes output has failed.
> *(b)* Record memory growth (file count and bytes) over that run against a stated bound; unbounded growth is a
> defect even when nothing else breaks. *(c)* Flip the **kill switch** and show from logs that no proposals are
> produced afterwards. *(d)* Export a recovery bundle on the device, verify its SHA-256, restore it into a
> **fresh install**, and confirm the §3.J.1 dry run passes; then attempt to restore an **older** bundle over a
> newer record and confirm it is **refused** with a reported reason. *(e)* Inspect the built APK and prove no
> dataset, weights file or adapter is packaged, reporting the APK size delta against the pre-M6 build.
> *(f)* Battery and heat remain `NOT MEASURED` unless actually measured; a long shadow run is exactly the place
> where an unmeasured thermal claim would be tempting, and it must not appear.
>
> **9. README handover requirements.** Document the three stores and where they live; how an improvement is
> proposed, approved and landed; how a regression is reported; the eviction rule; the dataset rules —
> including that the engine's own output is the **most promising** dataset path but is **`BLOCKED` on D9 and
> D11** today, so the eligible set is empty and generated assets are recorded
> `NOT_ELIGIBLE_FOR_TRAINING` (§3.F.6, §3.I.2); the accepted improvement and the recorded rejection, both with
> their measurements. **Do not write "licence-clean" into the README** — that claim was withdrawn from §3.F.6
> on 2026-11-26 because it assumed an ownership nobody has established.
>
> **10. Explicit exclusions and blockers.** **No write access to any §3.M.1 protected component, ever** — not by code
> path, not by configuration, not by "temporary" experiment. No automatic apply. **No fine-tuning, no
> adapters, no model versioning — that is M9.** No online
> learning inside the app. No telemetry leaving the device. No self-modifying shaders, fields or prompts. No
> automatic activation of anything a human has not approved. No external dataset downloaded, vendored or
> packed. No weights in Git or in the APK. Do not let a learned value bypass the P2 validation boundary or
> the adults-only rule.

### 5.9 M7 — Optimisation and asset intelligence

> **1. Exact objective.** Reduce what a frame and an asset cost, **measured on a named device** (§1.3 row 18,
> `docs/PHOTOREAL_UPGRADE_PLAN.md` repairs R4–R8).
>
> **2. Repository and correct working branch.** As §5.0. Each optimisation is its own commit with its own before/after, so a
> regression can be attributed to one change.
>
> **3. Files and modules to inspect or change.** Read first: `afcore.h:318-327` — `af_vertex` is **88 bytes**
> (pos 12 + normal 12 + tangent 16 + uv 8 + color 16 + joints 8 + weights 16) against a 32-byte target;
> `src/render/af_avatar_lod.cpp/.h` (three index-only levels, `kAvatarLodLevels = 3`,
> `kLodFractions = {0.12f, 0.05f}`); `src/render/af_lod_select.h` (`kAvatarLodPixelEdgeTarget = 4.0f`,
> `kAvatarLodHysteresis = 1.25f`); `src/render/af_render.cpp`'s `BudgetRenderer` tiers; `af_uv.cpp` (one
> shared 2048² atlas); `af_texture.cpp`.
>
> **4. Required interfaces and dependencies.** **F-1** (M2) for asset cost reporting and **F-2** (M3) for measurement. Also
> consumes M8's device evidence. Wave 4, parallel with M9.
>
> **5. Detailed implementation instructions.**
>
> **(a) Start from the measured gap, not from an optimisation idea.** The recorded photoreal comparison is
> **1,148,034 triangles** for a generated body against **27,292** for a Daz Genesis 8 skin, and **one shared
> 2048² atlas** here against **four 4096² maps** there. The polygon count is the problem; the atlas is already
> the better design and must be **preserved, not "modernised"** into four maps.
>
> **(b) Extend the LOD system that already exists — do not add a second one.** `07610b5` added
> screen-density-aware selection with `kAvatarLodPixelEdgeTarget = 4.0f`, `kAvatarLodHysteresis = 1.25f`,
> `kAvatarLodLevels = 3` and `kLodFractions = {0.12f, 0.05f}`, plus the `Avatar LOD selected:` /
> `Avatar LOD ready:` log lines and `avatar_lod_test` / `avatar_lod_select_test` in host `ctest`. M7 tunes and
> extends **those** constants and levels, keeping the hysteresis behaviour, and must not introduce a parallel
> LOD path (§7 item 1).
>
> **(c) Index-only decimation preserves the vertex contract.** The existing levels are index-only, which is
> why `af_vertex` stays **88 bytes** (`afcore.h:318-327`) and why the renderer's skinning path is untouched.
> Keep it that way: any change to `af_vertex`'s layout breaks the 45-float JNI contract's neighbours and the
> renderer's stride assumptions. If a level genuinely needs a different vertex format, that is an owner
> decision, not an implementation detail.
>
> **(d) `af_gl.cpp` compiles only under `if(ANDROID)`** (`CMakeLists.txt:52-71`), so its changes cannot be
> validated by host `ctest`. Use the documented syntax check —
> `g++ -std=c++17 -fsyntax-only -Wall -Wextra -D__ANDROID__ -DANDROID -I<...> src/render/af_gl.cpp` — and
> note that a **pre-existing `-Wunused-variable` warning at `af_gl.cpp:1647` is not yours**; do not "fix"
> unrelated warnings in an optimisation PR.
>
> **(e) Forward-pass bounds must use `elementBound`.** `maxf`/`minf` are unreachable in `af_gl.cpp`, and
> `scene_.modelMin/Max` are **skinning-padded**, so they over-report. Using the padded bounds for culling or
> LOD selection silently disables the optimisation.
>
> **(f) Every claimed win is a measured delta against a named baseline.** Report triangles drawn, draw calls,
> frame-time p50/p95/p99, selected LOD level, native heap and GPU request totals — before and after, same head
> pair, same device. **FID must not regress by more than 5%** and no §3.J metric may move beyond tolerance:
> an optimisation that fails the revision gate is not an optimisation.
>
> **(g) No new third-party C++ dependency.** Draco, meshopt, KTX2, Eigen and assimp are all **declined for
> now** (**D8**); zlib is the only dependency today (`af_png.cpp` needs `compress2`) and M7 must proceed
> without adding another. If a compression library looks essential, stop and raise it as an owner decision.
>
> **(h) This mission is gated by B2, not by code.** The LOD system has **never run on a real GPU**: the phone
> reports `ANGLE ((Samsung Xclipse 940) on Vulkan 1.3.304)`, so every GLES assumption on the target device
> rests on a translation layer, and the stale `af_vk.cpp` is not a shortcut around it. M7 may implement, but
> **may not claim a performance win** until it has device measurements.
>
> **6. Acceptance criteria.** A measured before/after for drawn triangles, frame-interval p50/p95/p99, and
> bytes per vertex, each recorded in an M3 `EvaluationRecord` with `commit` and `device`. **LOD indices are
> original vertex indices**, so skinning and morphs must be untouched: any crack, detached hair or scalp, or
> broken deformation at a coarser level is a **defect, not a saving**, and fails the mission.
> `BudgetRenderer` must never silently change the model — only MSAA, shadow-map size and bloom — which is
> why LOD selection is logged. **Every new `Renderer` virtual must be forwarded in `af_render.cpp`** or the
> app silently gets the base-class default. No regression in any golden record.
>
> **7. Required tests and build evidence.** Device measurements on a **named device** (model, Android
> version, GPU) — a host-only number must be labelled host-only, and no phone result may be claimed from an
> emulator. The `Avatar LOD selected:` and `Avatar LOD ready:` lines verbatim. `ctest` green; CI green
> including the instrumented GLES smoke and the generated-avatar runtime gate, with the gate's status stated
> honestly whether it passes or fails; run ids quoted. Visual evidence that no crack or detachment was
> introduced, from the existing evidence-only screenshot jobs.
>
> **8. Runtime verification requirements.** **This mission cannot be completed on an emulator.**
> *(a)* A real **SM-S921B** session per optimisation, recording: FPS over the last 240 frames, frame-time
> median/p95/p99/max, drawn triangles, draw calls, **selected LOD level with the transitions and the frame-time
> evidence that caused each**, native heap start/end, and GPU request totals (textures / geometry / render
> targets). *(b)* The historical baseline in §3.L.1 (**53.8 FPS at 1,100,168 triangles**, APK of `797d156`,
> 2026-11-15) is quoted **beside** the new figures as the baseline to beat — never merged into them, never
> presented as current (§7 item 14). *(c)* The known measurement gap must be closed or carried honestly:
> quality level **2** frame times were never recorded because the renderer steps down only after >30
> consecutive frames slower than 30 FPS, so level-2 data is `PARTIAL` unless actually captured. *(d)* Heat,
> battery and sessions longer than the measured window remain `NOT MEASURED` unless measured. *(e)* Confirm
> `ANGLE …on Vulkan 1.3.304` in the device log for every run, so the translation layer is on the record rather
> than assumed away. *(f)* Results belong to the head that produced them (§7 item 6); re-run after any further
> commit or mark the table stale.
>
> **9. README handover requirements.** Record each optimisation, its before/after numbers with units and
> device, what it cost in quality if anything, and which `PHOTOREAL_UPGRADE_PLAN` repair it corresponds to.
> Update the plan's repair table rather than leaving a parallel record.
>
> **10. Explicit exclusions and blockers.** **Adding Draco, meshopt, KTX2, ASTC tooling, Eigen or assimp is an owner
> decision, not an agent's** — zlib is currently the engine's only third-party C++ dependency, and the
> repository has deliberately declined compression dependencies. Do not touch the Vulkan path
> (`af_vk.cpp` / `af_shaders_vk.h` are stale and not compiled). Do not change the deterministic pipeline's
> output without an approved golden-record diff. Do not optimise by reducing a measured quality tolerance.

### 5.10 M9 — Versioned models, optional local fine-tuning and rollback

> **1. Exact objective.** Build §3.F.7–F.8: optional local fine-tuning, adapter versioning, activation
> pinning and rollback. **This is the only optional mission** — the owner may cancel it without stranding
> any other.
>
> **2. Repository and correct working branch.** As §5.0, plus a separate branch for any training harness, which lives
> outside the Android build entirely.
>
> **3. Files and modules to inspect or change.** Read first: §3.F.6 (dataset management), §3.F.8 (versioning rules),
> §6.3.2 (the verified free base-model list), and M6's `Experience` / `Decision` / `ExperimentRecord`
> schemas. New: adapter manifest format, activation pinning per task kind, rollback, and the validation gate
> that runs an adapter against golden records before it may be activated. Training harness: a **separate**
> tool, not part of `:app` or the engine build.
>
> **4. Required interfaces and dependencies.** **F-4** (M6) and **F-5** (M1) — an adapter improves a *proposer*, so the gateway and
> the validation boundary must exist. Also needs M3's golden records to validate against. Wave 4, parallel
> with M7.
>
> **5. Detailed implementation instructions.**
>
> **(a) Confirm the mission is authorised before writing anything.** M9 is the **only optional mission** and
> **D6** decides whether it proceeds at all. Cancellation is free of consequence **by design**: every other
> mission is Tier-0 (§3.G), so nothing depends on M9. If D6 is unresolved, stop and report — do not build a
> speculative fine-tuning path.
>
> **(b) Workstation-only, and provably so.** Fine-tuning runs on the owner's workstation, never on the phone
> and never in the app process (§3.K consequence 1: **no model weights may ever load into the app process**).
> The engine module must contain **no** training code path; the deliverable that ships is an **adapter
> manifest**, not an adapter.
>
> **(c) LoRA-class or smaller, inert until approved.** A fine-tuned adapter is inert on disk until a human
> approves it. Approval is a recorded decision (who, when, which evaluation record justified it), not a flag
> flipped by a script.
>
> **(d) Weights never enter Git or the APK.** Git holds the **manifest**: adapter id, base model id and
> version, SHA-256 of the weight file, its provenance, and the `EvaluationRecord` that justified it. A CI check
> asserts the APK contains no weight file and that no file above a stated size was added to the repository.
>
> **(e) Versioned models with rollback = §3.N applied to adapters.** Each adapter version is immutable and
> addressable by SHA-256; the manifest points at exactly one active version; rollback is a **manifest repoint**
> (level 1 of §3.N.2), which is why the manifest is data and not code. Record the honest limit from §3.N.4:
> **a lost adapter weight file is not recoverable by this project** — the manifest can name it, but cannot
> regenerate it.
>
> **(f) The licence gate is absolute.** Only assets eligible under §3.I.1 may be used as training data, and
> today that set is **empty**: generated assets are quarantined from training until **D9** declares the licence
> of the engine's own output, and **D11** records that the donor engine had **no repository-level `LICENSE`**
> at all. Until both are declared, M9 must report `LICENCE_BLOCKED` and **train on nothing**. Do not invent
> ownership or training rights (§3.I.1 rule 3, owner instruction of 2026-11-26).
>
> **(g) Prove inertness with a test, not a promise.** With an adapter present but unapproved, generated output
> must be **byte-identical** to the Tier-0 golden. With it approved, output changes and the change is recorded
> in an `EvaluationRecord` that passed the §3.J gate.
>
> **6. Acceptance criteria.** Base models restricted to the §6.3.2 verified-free list, each with its licence
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
> **7. Required tests and build evidence.** A before/after `EvaluationRecord` pair for at least one task
> kind, showing whether the adapter improved or regressed it, with `commit` and `device`. The manifest for
> the activated version and for the version rolled back to, both quoted. The approval decision and its
> reason. Proof that no weight file entered Git (`git log --stat` over the branch) or the APK (an APK
> listing). Proof that the zero-adapter configuration still passes M1's Tier-0 CI job.
>
> **8. Runtime verification requirements.**
> *(a)* On the device, with an adapter **present but unapproved**: generate the frozen subject set and confirm
> **byte-identical** SHA-256 against the Tier-0 goldens — proof the adapter is inert. *(b)* With an approved
> adapter and **D9/D11 resolved**: one end-to-end run showing the measured improvement that justified approval,
> inside §3.J tolerances, tagged with commit, date and device. *(c)* Inspect the shipped APK and prove **no
> weight file** is packaged, reporting the size delta against the pre-M9 build. *(d)* Demonstrate **rollback**
> on the device: repoint the manifest to the previous adapter version and show output returning to the previous
> golden SHA-256. *(e)* If **D6**, **D9** or **D11** is unresolved, the correct runtime evidence is a report
> stating `BLOCKED` with the decision names — **not** a demonstration on synthetic data, and not a claim that
> the path "would work". *(f)* Nothing in this mission may be reported `REAL + VERIFIED` from an emulator run.
>
> **9. README handover requirements.** Document the adapter lifecycle end to end: propose → train → validate
> against golden records → human approve → activate → monitor → roll back. State plainly that fine-tuning is
> **optional** and that disabling it changes nothing. Record which base models are permitted and their
> verified licences, and the rule that a new base model requires the four-question licence report first.
>
> **10. Explicit exclusions and blockers.** **No fine-tuning of the solver, the field, the mesh, the UV, the texture or
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
| Transitive dependencies | `PyMomentum` (`facebookresearch/momentum`) — **MIT, verified**; `trimesh >= 4.8.3` (tools only) — **MIT, verified 2026-11-26 at `mikedh/trimesh`; the earlier 404 was the wrong slug `trimesh/trimesh`**| PyTorch as above; MakeHuman **assets** are CC0, but MakeHuman **code is AGPL-3.0** and its §B expressly includes *glsl shaders* — so **assets only, never code** |
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
  bounded parameters**, which is the *style* of `af_spec`, `af_anatomy` and the adults-only invariant — and
  what **P2** (validated boundary) and **P6** (auditable learning) require. Latent Gaussian coefficients
  cannot be bounds-checked field by field, cannot be unit-tested against a named anthropometric reference,
  and cannot be audited by a human reading a decision log.
- **But stylistic similarity is not a mapping, and this document previously implied one.** Anny's labels are
  in `[0,1]`; `af_spec`'s fields are metres (`height_m` 1.40–2.20), years (`age_years` 16–70), centimetres
  (thirteen `*_cm` tape fields where **`0` means "derive"**), unitless indices with non-linear anchors
  (`body_fat` 0.10 shredded / 0.45 heavy), signed indices (`glute` −1..+1), material overrides where
  **`<0` means AUTO**, and **linear-sRGB** triplets where **`0` means "derive"**. **"`[0,1]`" is not a unit.**
  Anny's label *semantics* are not documented in its README and its paper was unreachable from this
  workspace, so its parameters are `UNVERIFIED` as a mapping source. **No direct mapping is assumed for
  either candidate**: every bridge requires a committed calibration record at status `CALIBRATED` (§3.O.2),
  and until one exists M4's critics must run on `ENGINE_INTERNAL` references (§3.O.3).

**One owner-reported fault makes the capability gap concrete.** On 2026-11-26 the owner reported *"webing on
movement"* in the engine's own output (**B9**, §3.D item 10). Diagnosed from source, it is a weight-quality
and skinning-model defect: a 4-influence ceiling in the 88-byte `af_vertex`, a 1 % influence prune
(`af_rig.cpp:436`), single-joint fallbacks (`:408-409`, `:442`), pure LBS (`Rig::skin`, `:449`) and **no body
pose correctives anywhere in the engine**. Non-linear pose-dependent correction is *the* standard remedy for
exactly this artifact class — and it is the capability MHR has that this engine lacks. **That strengthens
MHR's case as a *reference* for tier 3 and does not adopt it**: tier 3 needs corrective **data** or a trained
**model**, both of which are `BLOCKED` on **D9/D11** because the training-eligible set is empty (§3.I.2), and
MHR's correctives are neural, which §3.K bars from the app process. Tiers 1 and 2 — weight quality and
dual-quaternion skinning — are procedural, licence-clean and must be measured first (**D15**).

So the honest recommendation is **not "pick one**"":

| Use | Candidate | Why |
|---|---|---|
| Anthropometric and proportion reference for M4's critics | **Anny** *(candidate only)* | Labelled phenotypes, WHO-calibrated anthropometry, CC0 assets — auditable in principle per **P3** and **P6**. **Not usable as a mapping until a §3.O.2 calibration record reaches `CALIBRATED`**; its label semantics are `UNVERIFIED` today |
| LOD strategy and corrective-deformation reference for M7 and §3.D | **MHR** | 7 LODs and NN pose correctives are exactly what this engine lacks; useful as a *reference*, and its `.glb` export is a ready-made import test oracle for M2 and M5 |
| Anything shipped in the APK | **Neither** | Python/PyTorch; `minSdk 26`; §6.5 forbids weights in the APK |

**This is a recommendation, not an adoption.** Decision **D10** remains open and the owner decides. Before
either is used, the residual checks above must be closed: MHR's release-asset licence (scoped in §8.2),
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
| B8 | **The brief's section D ended at the single word "Deformation."** Deformation is designed here as measurement (§3.D item 9, §3.E.2). If the owner intended corrective blendshapes, cloth or muscle simulation, or penetration resolution, **that is not designed in this document**. **PARTIALLY RE-ISSUED 2026-11-26:** the owner has now named one concrete deformation fault — *"webing on movement"* — which is designed as §3.D item 10 and tracked as **B9**. The remainder of section D (cloth, muscle simulation, penetration resolution) is still not issued | M4, M5 scope | Re-issue the remainder of section D |
| B9 | **"Webbing on movement" — the owner's named deformation defect, reported 2026-11-26** with nine device captures in Dropbox `/3d/` as evidence (§8.6.1). Diagnosed from source as a **weight-quality and skinning-model** fault, not a rendering one: a 4-influence ceiling in the 88-byte `af_vertex` (`afcore.h:318-327`), weight sharing limited to bones within three joints (`af_rig.cpp:369`), influences under **1 %** pruned (`:436-439`), two single-joint weight-1.0 fallbacks (`:408-409`, `:442`), pure LBS in `Rig::skin` (`:449`) and **no body pose correctives anywhere** (§3.D item 10). `PARTIAL` — owner-reported and source-diagnosed, **not measured**, because the captures are unreadable from this workspace | M4 (needs the critic), M5 S-F (the remedy), and the credibility of any "photoreal" claim | **(a)** The owner attaches the nine captures to the conversation, or an M8 agent reproduces the session on hardware it controls, so the fault is *seen* and a baseline measured. **(b)** M4 builds the **five-part** host metric (min `signedDistance`, self-intersection count, region area change, **region volume**, influences used vs pruned), axilla and inguinal fold first. **(c)** **D15 is RESOLVED (2026-11-26): tiers 1 and 2 authorised, tier 3 not, `af_vertex` keeps its 4 influences.** The remedy path is therefore settled and the *only* remaining gap is the **baseline measurement** — M4's critic must run before M5 stage S-F may change anything, and every edit to `af_rig.cpp` or `af_gl.cpp` follows the **§3.M.1a** protocol |

### 8.2 Unverified research — do not rely on these yet

Each was checked against a primary source and could not be confirmed. None may be cited in code or in a
`THIRD_PARTY_NOTICES.md` entry until read.

| Item | What is unverified | How to verify |
|---|---|---|
| ~~Anny's pip package and hosted model assets~~ | **RESOLVED 2026-11-26.** PyPI `anny` **v0.6.2** exists but declares **no licence field and no licence classifier**, so the GitHub README governs — and its per-component licence section was read in full: code Apache-2.0, `data/mpfb2` and `data/faceunits01` **CC0 1.0**, `data/soma` Apache 2.0, and **`smplx`/`smpl` topologies "non-commercial use only"** | **Closed.** Remaining check: ensure no script fetches the `smplx`/`smpl` bundle |
| ~~`model_licenses/LICENSE-FLUX1-schnell`~~ | **RESOLVED 2026-11-26** — the file was fetched from `black-forest-labs/flux` and is the full Apache License 2.0 text | **Closed** |
| **AIST++** | **RESOLVED (split) 2026-11-26.** First-party repositories exist: `google/aistplusplus_api` = **Apache-2.0 verified**; `google/aistplusplus_dataset` = **no licence file at all**, so the reported CC BY 4.0 on the motion data has no first-party source anywhere this sandbox can reach. The Apache-2.0 code is moot because the *data* was the point: uncitable, and unusable under P9 | **Closed** — the free-motion-data search still ends with engine-generated clips and the donor fixtures (**D12**) |
| **Chroma / "Chroma1"**, **HiDream-O1** | **CLOSED AS UNRESOLVABLE 2026-11-26.** Re-searched: no first-party repository exists for either (only third-party wrappers), so the reported Apache-2.0 / MIT claims have no primary source to read | **Closed** — uncitable from here; neither was in the ACCEPTED set, so nothing loses a dependency |
| **Stability AI Community License** (SD 3.5 weights) | **RESOLVED 2026-11-26.** `Stability-AI/sd3.5` carries **only `LICENSE-CODE` (MIT)** plus inference code — no weights licence in the repository at all; sibling `generative-models` carries `LICENSE-CODE` (MIT) and a `model_licenses/` directory covering only the SDXL/SVD/SV3D family. The SD 3.5 weights terms therefore live outside any repository this sandbox can reach | **Closed** — uncitable here; SD 3.5 stays EXCLUDED, which the absence only reinforces |
| **Hunyuan3D 2.0 Acceptable Use Policy (Exhibit A)** | **RESOLVED 2026-11-26.** The repository's `LICENSE` (82 lines) was read in full: agreement body, then **EXHIBIT A from line 56**; the territory exclusion — *"DOES NOT APPLY IN THE EUROPEAN UNION, UNITED KINGDOM AND SOUTH KOREA"* — appears verbatim in the preamble and again inside Exhibit A; `NOTICE` adds a second layer of third-party components under their own terms | **Closed** — the P9 disqualification now stands on primary text twice over, plus a component layer |
| **Meshcapade pricing** | **CLOSED AS MOOT 2026-11-26.** The SMPL family is EXCLUDED, so its pricing cannot affect any decision this plan makes | **Closed** |
| **Anny's blendshape count and `Anny-One` size** | **CLOSED AS UNRESOLVABLE 2026-11-26.** The README and repository root are silent on the conflicting 564/1,136 and 780k/800k figures, and the paper is unreachable; the numbers have no primary source here | **Closed** — uncitable; nothing in this plan depends on either figure |
| ~~**"MHR"**~~ | **RESOLVED 2026-11-26**, residual **narrowed 2026-11-26**: Apache-2.0 verified from its own licence section, and the repository root carries **exactly one `LICENSE` file** and no second licence; the only remaining check is bundle-level (does the Hugging-Face release mirror the repository licence?), which this sandbox cannot reach | Residual scoped and recorded against **D10**'s execution gate; gates nothing today |
| **`avatarforge-engine` on a physical phone** | No claim about the Galaxy S24 has ever been verified here; `PROJECT_STATUS.md` records SM-S921B as UNVERIFIED | M8, on the actual device |

**After this pass one row remains open — the physical-phone row — and it is open because it needs a
phone, not because nobody looked.** Every other item is resolved, closed as moot, or closed as unresolvable
with the reason stated, so this table can no longer accumulate stale uncertainty.

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
| D9 | **The licence terms of this engine's own output** (§3.I.1, §3.I.2, §3.F.6) | declare a licence / declare output unrestricted / leave undeclared | **No recommendation is made, deliberately.** An earlier draft suggested declaring generated avatars "the user's own data" by analogy with MakeHuman's §D; that is **withdrawn** — it would have the plan inventing ownership and training rights nobody has verified (§3.I.1). **D9 is also gated behind D11**: an output's licence cannot be settled while the licence of the code producing it is undeclared. Until both are declared, generated assets carry `UNDECLARED-OWNER-DECISION-PENDING`, `licence_verified=0`, `redistribute=unknown`, `train=unknown` — usable locally, barred from distribution and training |
| **D11** | **NEW, and prior to D9: the licence of the AvatarForge engine port itself.** `docs/AVATARFORGE_INTEGRATION.md` records *"No repository-level `LICENSE` file was present at the pinned AvatarForge branch"* (donor `work/android-engine-dev` @ `9e36197ace3b18faa6c566d93f8fce89b185c45d`) and that copying between the owner's own repositories *"does not establish third-party redistribution rights"*. `docs/MASTER_BLUEPRINT.md`: *"A dependency's licence does not automatically license the host app or generated assets."* | declare a project licence for `:avatarforge-engine` / keep the app internal-only and never distribute / obtain clarification from the donor repository's owner (the owner) | **Declare it before D9, and before any external distribution of the app.** This is the owner's own repository, so it may be a one-line decision — but it must be *made*, because every downstream licence statement (D9, §3.I.2's `redistribute`, M6's training eligibility, M9's dataset gate) inherits it. Until then the honest status of the engine's licence is `MISSING`, not "presumably fine" |
| **D12** | **The donor's five animation fixtures** (~596 KB, Khronos `CesiumMan` and `Fox`, CC BY 4.0) as M5 test assets. The donor's own `THIRD_PARTY_NOTICES.md` records that they *"ARE packaged into the distributed APK"* and that `Fox.glb` has *"two separate rights holders with two separate licences on one file"* | vendor them with attribution shipped in the APK / use only in host tests, never in the APK / generate equivalent fixtures procedurally | **Host tests only; never in the APK.** CC BY 4.0 permits redistribution *with attribution*, so `redistribute` could be `allow` only if the attribution actually ships — and `Fox.glb`'s second rights holder makes that a two-licence obligation for one file (§3.I.2 rule 6). M5's stages S-A and S-C need fixtures either way; procedural ones avoid the question entirely |
| **D13** | **Anny's `anny-full` topology disables its nudity-related mesh edits**, which intersects `AGENTS.md` §2.6's adults-only content policy — and M4 may want Anny as an anthropometric reference (D10) | permit `anny-full` for authoring/CI reference use only / restrict to the default `anny` topology / do not use Anny at all | **Reference use only, on the default `anny` topology, never generating assets that ship.** This is a content-policy decision about the owner's product, not an engineering preference, so it is the owner's. Anny's `smplx`/`smpl` topologies remain excluded outright (non-commercial) |
| **D14** | **NEW: is multi-view turnaround reconstruction a Neural Core target?** The owner's `/goal/` folder holds a three-view character turnaround sheet (512 × 341 px, uploaded 2026-11-26). `afcore.h`'s `af_image_role` explicitly states the current analyser *"does not reconstruct a face, copy a garment, or fuse calibrated views"* and that `AF_IMAGE_EXTRA_VIEW` means *"no geometric fusion"* — so this is a `MISSING` subsystem, not a gap in an existing one (§8.6.2) | treat the sheet as a **proportion/silhouette reference** for M4's proportion critic (no new subsystem) / commission multi-view fusion as a **new mission** with its own brief / defer until M4 exists / decline | **Treat it as a proportion and silhouette reference for M4, and defer fusion.** At 512 × 341 px (≈170 px per view) the sheet cannot support face reconstruction at any engine quality, but it *can* anchor proportion targets — which is a measurement M4 already needs. Commissioning fusion would add an eleventh mission whose input resolution does not justify it. **Also required either way:** the sheet's provenance and licence are `UNKNOWN` and its filename suggests it is AI-generated, so under §3.I.2 it is `preview = allow, redistribute = unknown, train = unknown` — it may be looked at, but not shipped, and **not used to fit a critic's thresholds**, because fitting is training. **Update, second owner event of 2026-11-26:** the owner attached this very sheet to the conversation (identity proven by SHA-256, §8.6.5) and said *"This quality of mylove.glb"* — naming mylove.glb's quality as the bar the sheet illustrates. Direct observation confirms the bust-only crop and ≈170 px per view, so the recommendation above stands unchanged: proportion, silhouette and human-judged look reference; fusion deferred. One permission tightens: as an owner-asserted depiction of `mylove.glb`, the sheet inherits mylove's denial under §3.I.2 strictest-wins, so `redistribute` moves from `unknown` to **`deny`** and `train` to **`deny`**. Whether the bar may ever be enforced by automated calibration against mylove.glb or the sheet is **D16** |
| **D15** | **RESOLVED BY THE OWNER 2026-11-26 — "Definitely 1 and 2".** Recorded as: **tiers 1 and 2 of §3.D item 10 are authorised; tier 3 is not; `af_vertex` does not change.** So the engine keeps its 4-influence, 88-byte skinning ABI, fixes webbing through **weight quality first** and **dual-quaternion skinning second if measurement still shows the fault**, and does **not** pursue pose correctives now. *Interpretation note, recorded for honesty: the owner's answer was two characters long and the plan contained two numbered lists that day — the three remedy tiers of §3.D item 10, and the webbing-location options. It is read as the **tiers**, because that is the list the plan presented as numbered and because it answers D15, which was open. If the intent was the **locations** (armpit and hip), nothing changes: §3.D item 10's metric already instruments the **axilla and inguinal fold first**. Either way no re-work follows.* Original question: **may the 88-byte `af_vertex` skinning ABI change to allow more than 4 influences?** The owner reported *"webing on movement"* (§3.D item 10, **B9**). `af_vertex` carries `uint16_t joints[4]; float weights[4]` (`afcore.h:318-327`), the renderer's vertex stride depends on that layout, and it sits in §3.M.1's **protected set** — so no mission may change it unilaterally | keep 4 influences and fix weight quality + consider dual-quaternion skinning (tiers 1–2) / raise the influence count and accept an ABI + renderer change / raise it only for a new optional vertex format beside the existing one | **Keep 4 influences; do tiers 1 and 2 first.** Tier 1 (weight quality: region-gate the 1 % prune at `af_rig.cpp:436`, widen the three-joint sharing rule at `:369`, remove the single-joint fallbacks at `:408-409`/`:442`, smooth by distance or heat diffusion) is procedural, needs no ABI change, no dependency and no licence — and the repository's own note at `af_rig.cpp:807` says weight faults have already masked visual faults here, so the weights are the first thing to measure. Tier 2 (dual-quaternion skinning) removes linear-interpolation collapse without more influences but touches `af_gl.cpp`, which cannot be host-tested. Only if both are measured and still insufficient should the ABI be reopened — and tier 3 (pose correctives) needs **D9/D11** settled first, because corrective data or a trained model cannot be produced or shipped while the training-eligible set is empty |
| D10 | **Reference body model: MHR or Anny** — full comparison now in **§6.4**; supersedes D5 | MHR for everything / Anny for everything / **split use as §6.4.3 recommends** / neither / defer until M3 exists | **Split use, neither adopted as the engine's body model.** MHR for **LOD strategy and corrective-deformation reference** (7 LODs vs this engine's 3; NN pose correctives vs linear blend skinning) and as a **`.glb` import oracle** for M2/M5. Anny for **anthropometric reference** in M4's critics — labelled phenotypes in `[0,1]` match `af_spec`'s named bounded fields, which MHR's latent Gaussian `identity_coeffs` (45 latent dims, **not** the same as `kSpecFloatCount = 45`) cannot. Both Apache-2.0 code, both ungated downloads, both Python/PyTorch ⇒ **authoring and CI only, never in the APK**. **Exclude Anny's `smplx`/`smpl` topologies** (non-commercial). **Two residual checks first:** MHR's release-bundle licence (scoped in §8.2) and a guarantee no script fetches the non-commercial bundle |
| **D16** | **NEW (second owner event of 2026-11-26): may the quality bar ever be enforced by automated calibration against `mylove.glb` or the `/goal/` sheet?** The owner attached the sheet and said *"This quality of mylove.glb"*, naming mylove.glb's quality as the bar (§8.6.5). Standing rules forbid it today: §3.I.2 rule 4 makes fitting a critic to an asset *training*, and `mylove.glb` is `train = deny` with its provenance and licence undeclared; the sheet is now `redistribute = deny, train = deny` as an owner-asserted depiction of mylove's likeness | keep the bar human-judged plus procedural metrics (recommended) / grant a one-off calibration exception / declare mylove.glb's provenance and licence first, then revisit | **Keep the bar human-judged plus procedural metrics.** The owner's sentence *defines* the bar in words and by example; it does not grant a training right, and nothing in §3.I.2 lets a decision by implication override an explicit `deny`. If the owner later wants critics calibrated to mylove.glb, that is a licence declaration first — provenance, rights holder, scope — and an explicit reversal of `train = deny`, recorded here so it cannot happen by drift. **OPEN — awaiting owner** |
| **D17** | **NEW: which artifact, if any, becomes the integration baseline?** §8.6.6 catalogues four candidates: the in-repo port at pin `9e36197a`; the v0.10.1 bundle's APK/AAR (2026-10-21); release `debug-411-1`'s `latest-debug.apk` (2026-11-20); and the owner-side diagnostics snapshot (2026-11-16). The cloud binaries are ungated, debug-signed under a certificate that changed between artifact sets, and sit behind an undeclared licence (D11) | keep the in-repo port at the pin (recommended) / adopt a cloud APK/AAR as a binary dependency / re-pin AvatarForge to `78ffeda7` / treat the diagnostics snapshot as the authoritative copy | **Keep the in-repo port at pin `9e36197a`.** The cloud artifacts are evidence and immutable recovery baselines — the owner's own README says exactly that of the pre-integration APK — not dependencies: adopting them would import an ungated, debug-signed binary whose licence is undeclared, and re-pinning would discard 27 commits of hardening the pin already contains. Re-pin or adopt only by owner decision with A/B build evidence and a D11 declaration first |
| **D18** | **NEW: does adult-mode integration (Grok-Girls port, adult anatomy, render-video) enter Neural Core scope?** The owner-side 2026-11-16 doc defines a port scope and the owner authorises Grok-Girls reuse; AvatarForge's anatomy controls are `BLOCKED_ENGINE_GEOMETRY` — a contract with no mesh behind it — and owner-side status is `PARTIAL` | record and defer (recommended) / add an eleventh mission now / fold pieces into existing missions | **Record and defer.** The ten missions stay intact; adult mode is recorded in §8.6.6 with its owner authorisation and its gates (default-off, 18+ confirmation, minors rejected, fictional adults only, consent on references, no anatomy UI before geometry, real rendered frames not Ken-Burns). Scheduling it before D1/D2 land and before M3's harness exists would put an ungated capability ahead of the credibility gate (M8) — and `AGENTS.md` §2.6's adults-only rules apply unchanged whenever it is scheduled |

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

### 8.6 Owner-supplied material checked 2026-11-26 — four Dropbox links

The owner supplied four links. Each was resolved through the Dropbox connector and its metadata read.
**Nothing was downloaded into this repository and nothing here may ever be committed or packed into an APK**
(`AGENTS.md` rule 5; `docs/QUALITY_REFERENCE_ink_glb.md` already states it for `ink.glb`). Files are
identified below by **Dropbox `content_hash`**, not by path, per §3.I.1's evidence rule.

**One hard limitation on this check:** image and model **contents could not be read from this workspace**.
`get_temporary_link` resolves, but retrieval fails — `dl.dropboxusercontent.com` is outside the permitted host
set (`SSL_ERROR_SYSCALL`, `http=000`, 0 bytes). So every statement below is from **metadata plus this
repository's own prior records**, and no measurement is claimed from a file that could not be opened.

| Link | Resolved to | Identity | What is established | Status |
|---|---|---|---|---|
| 1 | Folder `/New folder (2)/ink/` — the **authoring project** behind `ink.glb`: `source/zeee.fbx` (72,370,736 B, hash `93dc26ac…`) and `textures/` (**18 files, ≈42,974,739 B**) | — | **Already documented**, in depth, by `docs/OWNER_MODELS_2026-11-15.md` §6 and `docs/QUALITY_REFERENCE_ink_glb.md`. `zeee.fbx` is the **DAZ Studio** export (FBX SDK 2020.2): 198 bones, 6 skin deformers, one 1-second animation stack, **0 blend shapes** | `REAL + VERIFIED` **from the repository's own prior inspection**, not re-verified now |
| 2 | Folder `/3d/` — **9 files, all uploaded 2026-11-26 14:32Z**: `generated_avatar_diagnostic (1).png` (561,510 B, `25c856f9…`), `imported_glb_diagnostic (1).png` (539,517 B, `2a59e11f…`), and **7 device screenshots** `Screenshot_20261126_222519…223144.jpg` | — | **NEW — and the most significant item in this check.** See below | Existence `REAL + VERIFIED` via Dropbox metadata; **contents `UNVERIFIED`** (unreadable here) |
| 3 | Folder `/3d real/` — **9 GLB files**: `ink.glb` **103,566,844 B** (`081e2ddd…`), `beautiful_realistic_undressed_girls_-_14_anims.glb` 38,595,284 B (`9cb25d27…`), `woman_dancing_nude.glb` 10,252,696 B, `heather.glb` 6,923,180 B **and** `heather (1).glb` 6,569,344 B (**different hashes ⇒ genuinely two versions**), `nude_girl_t_pose_…_fbx.glb` 1,493,892 B with a `(1)` copy at the **identical hash** `1f863999…`, and `(2)`/`(3)` copies both at `3bd5d831…` (**exact duplicates**) | — | **Already documented** by `docs/OWNER_MODELS_2026-11-15.md` §1–§5 and §7 | `REAL + VERIFIED` from prior inspection |
| 4 | `/goal/three_view_character_turnaround_sheet_same_layout_and_presentati_4a719b64.jpeg` — **512 × 341 px**, 27,300 B (`c21e0ba8…`), uploaded 2026-11-26 14:33Z, in a folder named **`/goal/`** | — | **NEW.** A stated *goal*: a three-view character turnaround sheet. See below | Dimensions `REAL + VERIFIED` from media metadata; **contents `REAL + VERIFIED` by direct observation on 2026-11-26** (owner attachment, hash-identical to this file, §8.6.5); **file provenance still `UNKNOWN`**, and now `redistribute = deny, train = deny` as an owner-asserted depiction of `mylove.glb` (**D14**, **D16**) |

#### 8.6.1 Why link 2 matters: the first device evidence since `797d156`

`docs/PHONE_TEST_2026-11-15.md` records the only prior real-device session — a **38-second** run of the APK of
`797d156`, after which *"every build after `797d156`: nothing verified."* Link 2 changes that, **partly**:

- The two PNG names are not arbitrary. Per `README.md` and `BUILD_AND_VERIFY.md`, the app writes
  `generated_avatar_diagnostic.png` and `imported_glb_diagnostic.png` **only when its PixelCopy
  visible-pixel check passes** (failures write `failed_render.png` instead), and
  `docs/PHONE_TEST_2026-11-15.md` already relies on exactly that inference for the 2026-11-15 capture.
  **No `failed_render.png` is present in the folder.** So a visible-pixel check passed on **both** paths —
  generated and imported — on whatever device produced these.
- The seven screenshots span **22:25:19 → 22:31:44** in device-local time. In Perth (UTC+8) that is
  **14:25:19 → 14:31:44 UTC**, matching the 14:32Z upload — a session of roughly **6.5 minutes**, against the
  38-second window of 2026-11-15. That directly touches the standing gap *"heat, battery, sessions >38 s:
  NOT MEASURED."*
- **But none of it is readable here**, so **no number may be recorded**: not FPS, not triangles, not heap, not
  pixel counts, not the device model, not the APK's commit. Which head produced these captures is **unknown**
  from metadata alone, and §7 item 6 forbids transferring a result to a head that did not produce it.

**Consequence for the plan.** Blockers **B1** and **B2** move from *"no device evidence exists"* to *"device
evidence exists and is unread"* — which is a **different and more actionable** state, and it is why **M8** is
in wave 0. The correct next step is not to guess at these files but to have the owner attach them to the
conversation (attachments are readable) or have an M8 agent reproduce the session on hardware it controls,
recording the APK's commit SHA beside every figure. Until then: `PARTIAL`.

#### 8.6.2 Why link 4 matters: it states a goal the engine explicitly cannot meet

A three-view turnaround sheet is a **multi-view reconstruction** target. `afcore.h`'s `af_image_role` enum
disclaims precisely that, in its own words: the roles *"describe caller intent, not capabilities the current
classical analyser does not have: it samples coarse framing/proportions/colours and **does not reconstruct a
face, copy a garment, or fuse calibrated views**"*, and `AF_IMAGE_EXTRA_VIEW` is *"additional independently
analysed view; **no geometric fusion**."*

Three honest observations, none of them a criticism of the goal:

1. **The capability is `MISSING`, not partial.** Multi-view fusion is not a tuning problem on the existing
   analyser; it is a different subsystem, and §3.D/§3.C do not design one today.
2. **512 × 341 px is small for the purpose** — roughly 170 px per view if the three views share the width.
   Face reconstruction from that is not plausible at any engine quality; a turnaround sheet at this size is a
   *proportion and silhouette* reference, which **is** usable, and M4's proportion critic is where it would
   land.
3. **Its provenance is unknown and its filename pattern suggests it is AI-generated** (a truncated prompt as
   the name plus a hash suffix). Under §3.I.1 that makes it class **AI-GENERATED** with
   `licence = UNKNOWN`, `licence_verified = 0`, and under §3.I.2 `preview = allow, redistribute = unknown,
   train = unknown`. **It may be looked at; it may not be shipped, published, or used to fit a critic's
   thresholds** — fitting is training (§3.I.2 rule 4).

That is decision **D14**.

#### 8.6.3 What the owner stated about this material on 2026-11-26

Four statements now accompany this material. Three accompanied the links; **the fourth arrived later the
same day as a direct conversation attachment**, together with the `/goal/` sheet itself — the first owner
material this workspace has actually *seen*. All are **owner-sourced**, which makes them authoritative as to
*intent* and *target*, and `UNVERIFIED` as to any measurement; the attached sheet adds observation (§8.6.5)
but no measurement.

| Owner's words | What it establishes | Consequence for the plan |
|---|---|---|
| *"Current faults notice **webing on movement**"*, pointing at the `/3d/` captures | A **named, current visual defect** in the engine's own output, with the nine captures as its evidence | Now **B9** and **§3.D item 10**, diagnosed from source as a weight-quality and skinning-model fault (4-influence `af_vertex`, 1 % prune at `af_rig.cpp:436`, single-joint fallbacks, pure LBS, no body pose correctives). Measurement → **M4**'s new deformation critic; remedy → **M5 stage S-F**; the ABI question → **D15**. It also **partially re-issues section D**, so **B8** narrows |
| *"this is details in want on **mylove.glb**"*, pointing at the same folder | The `/3d/` material is the **detail reference** the owner wants realised on `mylove.glb` | `mylove.glb` is already recorded in `docs/AVATAR_GEOMETRY_EVIDENCE.md` — **9,287,272 B**, SHA-256 `8ef94a00…`, Dropbox content hash `388a94e7…`, and **"private. Do not commit, publish or redistribute"** (`AGENTS.md` rule 5). It is therefore an **identity/detail target for measurement only**: `preview = allow, redistribute = deny, train = deny` (§3.I.2). No mission may embed it, and no critic may be fitted to it (§3.I.2 rule 4) |
| *"**ink.glb** this is close to quality wanted"* | **Owner confirmation** of the quality target the repository had already designated | `docs/QUALITY_REFERENCE_ink_glb.md` already names `ink.glb` the quality reference, and §1.3 row 18 and §3.C already quote its numbers (body skin **27,292** triangles against this engine's **1,148,034**; four 4096² skin maps against one shared 2048² atlas). The owner's statement upgrades that from *researched recommendation* to **owner-confirmed target** — but changes nothing about its licence: it is **Daz Genesis 8 Female** content, so `redistribute = deny` and `train = deny` (§8.6.4) |
| *"This quality of mylove.glb"*, attached with the `/goal/` turnaround sheet — **second owner event of 2026-11-26** | The owner **names the quality bar**: the quality this sheet shows is `mylove.glb`'s quality, so the identity/detail target and the quality target converge on one object | Identity with link 4's file **proven by hash**, and the sheet **directly observed** for the first time (**§8.6.5**): three bust views at ≈170 px each, stylised-realistic adult, gothic makeup, strand-level hair, lace collar — a head-detail bar, not a polygon bar. Permissions tighten by strictest-wins rather than loosen: the sheet becomes `redistribute = deny, train = deny` as an owner-asserted depiction of mylove's likeness; `mylove.glb` itself does not move. Whether automated calibration to it is ever allowed is **D16** |

**One caution that must not be lost.** `ink.glb` being "close to the quality wanted" is a statement about
**appearance**, and its appearance comes from a professionally sculpted mesh, 18 large textures and 45
separately drawn parts — not from anything this engine's procedural solver does. Matching it is therefore a
**content** problem (§3.C, `docs/PHOTOREAL_UPGRADE_PLAN.md`), and **no part of it may be copied into the
engine or an APK**. It is a target to measure against, at a distance.

#### 8.6.4 `ink.glb` is the worked example of §3.I.2, and its licence position is already settled

Links 1 and 3 add no new licence facts, but they make §3.I.2 concrete, because the repository has already
established them: `ink.glb` is a **Daz 3D Genesis 8 Female** figure with commercial add-ons (hair, brows,
boots, an anatomy add-on), and *"whoever uploaded it to Sketchfab could not give a CC-BY licence to content
they do not own, so the label does not make it free to ship."* Its three permissions therefore split exactly
as §3.I.2 predicts:

| Permission | Value | Reason |
|---|---|---|
| `may_preview_locally` | **allow** | It is the owner's own file and the project's **quality reference target** (`docs/QUALITY_REFERENCE_ink_glb.md`) |
| `may_redistribute` | **deny** | Daz content; shipping it in an app needs Daz's Interactive Licence **per product**, bought by the owner |
| `may_train_on` | **deny** | Same, and §3.I.2 rule 4: fitting to it would be a derivative computational use |

Two further facts from the prior inspection that constrain missions directly:

- **`zeee.fbx` has 0 blend shapes**, so `ink.glb`'s absence of morph targets is **not a conversion loss** —
  the source had no shape controls either. Sliders can never drive that file, which is why **M5 stage S-D**
  must be proven on an avatar **this engine generated** (whose exporter does write morph targets,
  `afcore.h:390`), not on `ink.glb`.
- Its 18 textures (twelve 4096², six 2048²) need **1,152 MiB decoded with mipmaps** against the **448 MiB**
  budget; `7e4f0cb` already reduces that to **384 MiB** and the file to 60.8 MB, cutting host import from
  **7.2 s / 962 MB peak** to **2.0 s / 364 MB peak**. So M2's over-budget path is **already implemented and
  unit-tested** (16 JVM tests) — M2 must **report and reuse** it, not rebuild it (**P8**).

#### 8.6.5 The `/goal/` sheet, directly observed — and the quality bar named (second owner event of 2026-11-26)

The sheet arrived a second time, this time **as a direct conversation attachment**, which this workspace can
open. Its identity with the `/goal/` file is **proven, not assumed**: the attachment is
`three_view_character_turnaround_sheet_same_layout_and_presentati_4a719b64.jpeg`, **27,300 B**, **512 × 341**,
SHA-256 `69fd0de3e93d05c51ce475e4d942dbffd5cca2a4bb50f48092a19db737c5eec6` — and Dropbox's `content_hash` form
of that digest (SHA-256 over the per-4 MiB block digests; a single block here, so `sha256(sha256(file))`) is
exactly the `c21e0ba8…` recorded from `get_file_metadata` on 2026-11-26. Same bytes, therefore, as link 4.

**What is actually in it** (`REAL + VERIFIED` by direct observation): three views of one character side by side
on a neutral grey studio ground — left three-quarter, front, right three-quarter — cropped at the upper chest:
a bust, with no arms, hands or torso below the collarbone. The subject presents as an **adult woman**,
rendered stylised-realistic: smooth shaded skin, dark smoky eye makeup and oxblood lips, long wavy dark-auburn
hair with strand-level highlight detail, and a black lace collar with a centre bow. The three views agree on
one individual and one grooming, which is exactly what a turnaround reference exists to demonstrate. At ≈170 px
per view it confirms **D14**'s arithmetic — no reconstruction is possible from it — while remaining ample for a
**human-judged** look, proportion and silhouette reference.

**What the owner said with it:** *"This quality of mylove.glb."* Read plainly, the owner names the **quality
bar**: the quality the sheet shows is the quality of `mylove.glb`, the identity/detail target. The alternative
reading — that the sheet merely *depicts* mylove.glb — changes nothing operational, but it is recorded because
it tightens one permission: an owner-asserted depiction of mylove.glb inherits mylove.glb's restrictions under
§3.I.2's strictest-wins rule, so the sheet moves from `redistribute = unknown` to **`redistribute = deny`**
(unknown file provenance alone already blocked shipping; publishing mylove's likeness blocks it twice), and to
**`train = deny`** for the same reason that fitting is training. **What does not move:** `mylove.glb` stays
`preview = allow, redistribute = deny, train = deny`, never committed, and **no critic may be fitted to it**;
the sheet stays a reference — never a reconstruction input (D14), never a threshold-fitting source.

**Three statements, one bar.** With this, the owner's quality language converges: `mylove.glb` supplies the
identity and detail bar (now named outright), `ink.glb` remains the **measurable** asset reference with
documented numbers (§8.6.4), and the sheet is the **visual illustration** of the bar. All three point at the
same place — head-level material realism: skin shading, hair strands, fabric detail — not polygon count. The
engine's current output paints eyes, teeth and lashes onto one shared 2048² atlas (§8.6.4), which is why the
bar reads as a content problem (§3.C) as much as a solver problem.

**What the sheet cannot do.** It shows nothing below the chest, so it carries **no evidence for B9 or D15** —
the axilla and inguinal fold are not in frame; the webbing measurement still needs the `/3d/` captures or an
M8 hardware session. And it is not a measurement of anything: one rendered image of a target is a goal, not a
baseline. The copy examined here lives outside the repository (owner attachment, ephemeral workspace scratch);
**only its hashes and these observations are recorded**, consistent with every prior owner asset.

Whether the bar may ever be enforced by **automated calibration** against mylove.glb or this sheet, rather than
by human judgement plus procedural metrics, is decision **D16**.

#### 8.6.6 *"Lastest 3d-mega-hub upgrade"* — the upgrade chain, catalogued (owner pointer of 2026-11-26)

The owner's pointer named no link, so both connected clouds and GitHub were swept: the Dropbox root and
eight folders beneath it, Google Drive's most-recently-modified list, the AvatarForge release and commit
history, and every 3d-mega-hub branch. **The headline, stated plainly: no new 3d-mega-hub upgrade has landed
in the repository.** This working branch is documents-only, `main` has not moved, and no new branch or PR
appeared. What "the latest upgrade" can mean is therefore a chain of owner-side artifacts, each recorded
below with what is verified about it and what is not.

| # | Artifact | Date | What it is | Status |
|---|---|---|---|---|
| 1 | `/AAvatarForge-Android-updates (1)/AvatarForge-Android-updates/` — `README.txt`, `SHA256SUMS.txt`, `docs/{STATUS,DEVICE,BUILD,README}.md`, `artifacts/{BUILD-0.10.1.txt, avatarforge-sdk-arm64.aar 1,324,508 B, avatarforge-release-arm64.apk 7,190,802 B, avatarforge-debug-arm64.apk 10,163,456 B}` | 2026-10-21 | **The last packaged update bundle**, version **0.10.1**, branch `work/android-engine-dev`: distribution bundle of Android artifacts and update docs, explicitly *not* a source checkout | Manifests and build record **read**; artifact bytes **not fetched** (no need, and APKs are record-only). Build evidence `REAL + VERIFIED` *as a record of AvatarForge's own builds*; device acceptance **BLOCKED/UNVERIFIED** by its own text |
| 2 | `/Update/AvatarForge-fixed-3b98600/AvatarForge/` — 13 top-level dirs, 13 root files, `ARCHIVE_INFO.txt` | 2026-10-24 | Source snapshot of `work/android-engine-dev` @ `3b986001` *"Harden GLB and PNG import and renderer safety"*; artifacts intentionally omitted as pre-fix; no `.git` | **Proven redundant**: `compare/9e36197a...3b98600` = ahead 0 / behind 27, i.e. the snapshot is an **ancestor of the pin this plan already uses** — the "fix" is already inside the pin. No `LICENSE` at the snapshot root, so **D11** stands |
| 3 | GitHub `davealone69-gif/AvatarForge` release `debug-411-1`, asset `latest-debug.apk` 9,824,564 B, from commit `78ffeda7` | 2026-11-20 | **The latest build in the lineage** — "Latest Android debug build", "not a production-signed release", 1 download; preceded by the 2026-10-22 launcher rename to *Grok Faces* | Metadata `REAL + VERIFIED` via API. **Ungated and debug-signed ⇒ record-only**: `AGENTS.md` forbids publishing ungated APKs, and this is an AvatarForge-app artifact, not a 3d-mega-hub one |
| 4 | `/3d-mega-hub-diagnostics/3d-mega-hub/` — full repo snapshot *with `.git`*, `README.md` 10,662 B, `docs/{DONOR_BASELINE, INTEGRATION_PLAN, MASTER_BLUEPRINT, ADULT_MODEL_RENDER_VIDEO_INTEGRATION}.md`, `mylove_rig_prepared.glb` 13,221,520 B at root | 2026-11-10→16 | **The latest owner-side 3d-mega-hub state and guidance.** Its README carries two 2026-11-15 handover sections and one 2026-11-16 doc link that are **absent from the GitHub README** (grep-verified: *Non-monolithic*, *fixed-3b98600*, *pre-integration*, *BLOCKED_ENGINE_GEOMETRY*, *AdultSafetyTest* = 0 hits in repo) | README and adult-mode doc **read**; snapshot's `.git` **not examined** (binary). Status: **owner-side, UNVERIFIED against GitHub** — a parallel working copy, not a supersession |
| 5 | `/3d finally kind of/avatar-1337.glb` — 34,558,456 B | 2026-10-17 | A generated avatar the owner judges *"finally kind of"* acceptable; name echoes the frozen suite seed 1337 | Metadata only; contents unreadable here. A **quality datapoint**, not a measurement |
| 6 | `/3d-mega-hub-avatarforge-fix/` — `mylove_gles31_render.png`, `mylove_avatarforge_proof_of_fix.png`, `samsung_s24_mylove_photoreal.png` | 2026-11-10 | Device captures backing the already-tracked `patches/avatarforge-mylove-gles-fix.patch` | Metadata only; ties an existing repo patch to owner evidence |
| 7 | `/fault/` — 3 × `Screenshot_2026101{7,8}_*_{AvatarForge,Gallery}.jpg` + 4 × `not made on my engine but.loaded online-*.jpg` | 2026-10-17 | An **earlier** fault-evidence set, predating the `/3d/` webbing captures | The four *"not made on my engine"* images are **external renders used as comparison**, never engine output — recorded so no future reader mistakes them for evidence |
| 8 | Google Drive: `Extracting Android App Packages` (AI Studio prompt, 73,378 B), `Avatar_Engine_2.0_Upgrade_Audit.pdf` ×2, `Hard_completion_matrix.pdf` ×2, `Aaa_Final_Zero-Trust_Gap_Audit.pdf` ×2 (all 2026-10-04), `mylove_rig_prepared` + copy, `Easy_Pose_v1.6.10_MOD (1).apk` | 2026-10-04→20 | The Drive side of the same history | Nothing on Drive is newer than the Dropbox chain; the "Upgrade Audit" PDFs **predate** bundle 1 and are superseded by it |

**What each candidate "latest upgrade" would require to become a real one.** The v0.10.1 bundle and the
`debug-411-1` APK are **AvatarForge-app** artifacts: adopting either into 3d-mega-hub means D1/D2 resolved,
the device gate (B2) passed on hardware, D11's licence declared, and a signing story — the bundle's own
`README.txt` warns its APKs carry debug certificate SHA-256 `960c75a9648a74dc189f3510a6c7f80e7029a63680b5dbdea0a2d87b244e5514`,
*different from the preceding artifact set*, so Android may refuse an in-place update and uninstalling can
erase app-private data. The `3b98600` snapshot requires nothing: it is already inside the pin. The
diagnostics snapshot requires a reconciliation decision, not a merge: it is a parallel copy whose extra
guidance is valuable but whose `.git` and divergence are unexamined.

**New facts extracted, with their statuses.** From `BUILD-0.10.1.txt` (a record of AvatarForge's own builds,
local only — *"No remote GitHub Actions run is recorded"*): native host harness **464/464**, focused codec
**16/16**, Kotlin SDK JVM **20/20**, Rust **37** passed + 2 ignored-then-run, C++/Rust prompt parity
**72/72**, customization contracts **2/2**, **13** shader stages compiled, `verify.sh --quick` PASS, and an
APK audit reporting **no INTERNET permission, 47/47 C ABI exports, 51/51 JNI exports**, zip-aligned,
v2-signed. Thermal policy (compiled and JVM-tested, **not hardware-verified**): menu → *Live device report*
refreshes every second without USB/ADB or network; generation permitted through **Severe (3)**, blocked at
**Critical (4)+** with cooperative cancellation of an active job; renderer drops to LOW at Severe+; API
26–28 explicitly report thermal telemetry unavailable. From the diagnostics README: a preserved
**pre-integration baseline APK** `/AvatarForge/3d engine/AvatarForge-pre-integration-debug-f12a35acefd1 (1).apk`,
**11,026,696 B**, with historical handoff evidence of **5/5 app tests and 43/43 SDK tests** — historical,
like the SM-S921B record (§3.L.1), never a claim about today; an **interim architecture rule** (keep one
coherent product for now, clean seams, revisit splitting only on measured grounds); and the **adult-mode
handoff**: AvatarForge carries `config/customization/adult-anatomy.inputs.json`,
`requirements/custom-inputs/adult-anatomy.user.json` and `sdk/.../AdultSafetyTest.kt` on
`work/android-engine-dev`, where the anatomy config marks detailed geometry controls
**`BLOCKED_ENGINE_GEOMETRY`** — *a contract, not a mesh* — so adult-mode status owner-side is `PARTIAL`;
**Grok-Girls** is owner-authorised for reuse (*authored for this project by Grok*), with integration
direction: disabled by default, explicit 18+ confirmation, minors and ambiguous-age prompts rejected,
fictional adult subjects only, consent/ownership on reference material, adult assets private by default,
provider policy enforced with no silent fallback, anatomy controls hidden while geometry is missing, and
video from **real rendered engine frames** — its current recorder is a 2D-canvas Ken-Burns effect, *not* 3D
character video. Every one of those gates is already this plan's language (§7's no-mock rule, §3.I.2,
`AGENTS.md` §2.6); none of it is in the ten missions today.

**What changes and what does not.** Changes: this section, **D17** (which artifact, if any, becomes the
integration baseline) and **D18** (whether adult-mode/Grok-Girls enters scope). Does not change: the pin
stays `9e36197a`; the ten missions and their acceptance criteria stay intact; no APK, AAR, GLB or snapshot
byte enters this repository; the owner-side artifacts remain **evidence and immutable baselines**, exactly as
the owner's own README instructs for the pre-integration APK. B2's status is untouched: Galaxy S24 acceptance
remains **BLOCKED/UNVERIFIED** by the bundle's own words.

### 8.7 Verification ledger — what must be verified before each mission may begin

Requirement 7 of the owner's approval: **verify all eight blockers and ten unverified research items before
their dependent missions begin.** Status as at 2026-11-26, after a primary-source verification pass that read
licence files and repository headers directly rather than relying on secondary sources.

| Item | Verified? | Verified how, or what remains | Gate: which mission may not start until it is closed |
|---|---|---|---|
| **B1** runtime gate fails | **PARTIAL — narrowed, and device evidence now exists but is unread (§8.6.1)** | Attribution is measured (lead passes, trunk fails, merged line fails at `bdaef4b5`, `ba38d996` **and `281806e7`**, run 45813704590). Because `281806e7` is documents-only, the failure is now shown to be **independent of the code delta between the last two heads** — it is a property of the gate or its runner environment. The *specific failing test* still could not be read: logs and artifacts are on hosts outside this workspace's permitted set, and the annotations give only `exit code 1` | **M3** may not treat any device metric as settled; **M7** may not claim a performance win; **M8(a)** must still separate flake from fault with repeated runs on hardware the agent controls |
| **B9** "webing on movement" | **PARTIAL — owner-reported and source-diagnosed, NOT measured** | The owner named the fault on 2026-11-26 and pointed at nine device captures; the images are unreadable from this workspace, so the *diagnosis* is from source (§3.D item 10: 4-influence `af_vertex` at `afcore.h:318-327`, three-joint sharing limit at `af_rig.cpp:369`, 1 % prune at `:436-439`, single-joint fallbacks at `:408-409`/`:442`, pure LBS at `:449`, **no body pose correctives**) and the *baseline* does not exist yet | **M4** must build the deformation critic and record a baseline **with no tolerance asserted** — five measurements, axilla and inguinal fold first. **M5 S-F** may not claim a fix without that baseline, must do tier 1 before tier 2, and must follow the **§3.M.1a** protocol for every edit to `af_rig.cpp` or `af_gl.cpp`. **D15 is RESOLVED** (tiers 1–2 authorised, tier 3 not, `af_vertex` keeps 4 influences and its 88-byte layout) |
| **B2** LOD never run on a real GPU | **NO — but see §8.6.1** | Requires hardware, and no substitute exists. Device captures dated **2026-11-26** (`generated_avatar_diagnostic (1).png`, `imported_glb_diagnostic (1).png`, plus 7 screenshots spanning ~6.5 minutes) exist in the owner's Dropbox and imply a **passing** visible-pixel check on both paths — but they are unreadable from this workspace, the producing head is unknown, and no LOD level, FPS or triangle count may be inferred from them | **M7** may not start; **M3**'s device records are provisional |
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
licence, or Anny's blendshape count.

**Correction to an earlier claim in this paragraph.** A previous revision stated that *"no mission is blocked
by an open licence question."* **That is false and is withdrawn.** Two missions are licence-blocked today:

- **M9** is `BLOCKED` on **D9** (the licence of the engine's own output) **and D11** (the licence of the
  engine port itself, undeclared at the pinned donor SHA). Until both are declared, the set of assets eligible
  for training is **empty**, and M9's correct output is a `BLOCKED` report — not a demonstration on synthetic
  data (§5.10 fields 5(f) and 8(e)).
- **M6**'s *dataset management* component is `PARTIAL` for the same reason: it can classify, quarantine and
  record eligibility, and it must, but it can admit nothing to training (§5.8 field 5(h)).

Everything else holds: no mission's **core** deliverable waits on an external licence, because the excluded
dependencies (§6.3.3) were excluded rather than substituted, and the accepted ones (§6.3.2) were verified from
primary sources. M4's *external* anthropometric reference additionally waits on a **calibration record**
(§3.O.2), which is a measurement gap rather than a licence one — until it exists, M4 scores against
`ENGINE_INTERNAL` references and that is a complete, useful deliverable.

**Two gates stop everything regardless:** **D1** (merge PR #17) and **D2** (reconcile PR #16 with PR #9).
Until those are settled there is no stable base to branch from, and releasing an agent now would reproduce
the exact stale-base failure this convergence was created to end.
