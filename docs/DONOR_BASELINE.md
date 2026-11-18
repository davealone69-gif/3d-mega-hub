# Donor Baseline Snapshot

Historical snapshot checked against GitHub repository metadata, source files and workflow results on 2026-10-30. It is not a description of the current 2026-11-10 integration branch; see the current branch handover in README.md and BUILD_AND_VERIFY.md.

## 3DDD

- Repository: https://github.com/davealone69-gif/3DDD
- Observed default branch `main` HEAD: `be5bf17d8afe46247646465941fa1b7317d8e415` (2026-10-04).
- Source inspected: `app/build.gradle.kts`, `settings.gradle.kts`, `gradle/libs.versions.toml`, `StudioRenderer.kt`, `AndroidManifest.xml`, `RuntimePipelineTest.kt`, and Android CI workflow.
- Implementation evidence: Android/Kotlin/Compose; Filament Android 1.71.5; OpenGL ES 3.0 declared required in manifest; a renderer that explicitly detects Filament NOOP and attempts OPENGL fallback; a project glTF/GLB reader; bundled GLB fixtures; instrumented tests for visible offscreen pixels, real backend, rig loading, animation, morph/material edits, export and image-to-avatar pipeline.
- Important qualification: tests containing GPU-dependent assertions can be skipped when the driver cannot create resources. Read test reports and skip reasons, not just overall workflow status.
- Latest observed `main` Android CI run: [37218532057](https://github.com/davealone69-gif/3DDD/actions/runs/37218532057), failed at `Assemble release APK (unsigned)`. Exact cause in log: release lint reports four fatal errors because `backup_rules.xml` and `data_extraction_rules.xml` include `exports/` and `audio/` while also excluding those same paths. The instrumented job was skipped because the build job failed.
- Latest observed `main` Full Repository Audit run: [37218532019](https://github.com/davealone69-gif/3DDD/actions/runs/37218532019), failed at Gradle `kspDebugKotlin`: Room KSP reported `IllegalStateException: Empty schema file` while deserializing an exported schema.
- A more recent PR CI run on 2026-10-17 also failed: [39074442550](https://github.com/davealone69-gif/3DDD/actions/runs/39074442550). It is not the current main commit.
- These are two concrete donor blockers to fix or verify against a newer commit before integration. Do not hide them with a lint baseline or skip unless the underlying data/backup behaviour is understood.
- No successful fresh build or target-phone test was performed by this audit. Status for Mega Hub: **source candidate; latest observed main CI not green; device runtime UNVERIFIED**.
- Licence gate: README identifies Filament/filamat as Apache-2.0 and generated base rigs as CC0, but no repository-level LICENSE file appeared in the inspected root listing. Clarify the app/source and each asset licence before copying.

## AvatarForge

- Repository: https://github.com/davealone69-gif/AvatarForge
- Observed default branch `main` HEAD: `40ef65ba74686a119d8a576d740a7981fad6f41c`. Its latest commit message is “Restore InputScreen and show the Grok Faces title.” This is not automatically the engine donor state we want.
- Candidate engine branch `work/android-engine-dev` HEAD observed: `9e36197ace3b18faa6c566d93f8fce89b185c45d`.
- User's previously referenced branch `arena/bf31ed07-avatarforge` HEAD observed: `262abae3205f599979f4ae34db6e21addd1d91ff`.
- The `work/android-engine-dev` README reports 464 host checks and explicitly says Android NDK build, Kotlin compilation, compressed GPU upload paths and physical-device rendering are not yet verified. Treat the check count as a repository-authored claim until the exact command/run is reviewed.
- Positive CI evidence: [workflow run 40102403904](https://github.com/davealone69-gif/AvatarForge/actions/runs/40102403904) on `work/android-engine-dev` commit `9e36197ace3b18faa6c566d93f8fce89b185c45d` completed successfully on 2026-10-24. Its jobs report success for the host/native regression checks, ASan/UBSan GLB importer fuzzing, headless real GLES/Vulkan frame checks and Android package build/ABI audit. It uploaded artifact `AvatarForge-Android-9e36197ace3b18faa6c566d93f8fce89b185c45d` (about 11.4 MB). This is strong CI evidence, **not target-phone verification**; the headless GPU checks do not establish stability on the Samsung.
- The `arena/bf31ed07-avatarforge` README reports 386 host checks and says physical-device execution is unverified. Do not mix the check count from one branch with another branch's source.

> **RECONCILED BY MEASUREMENT — 464 is stale, and 405 and 386 were never comparable to it.** Run read-only from `3d-mega-hub` at `a0c7c09` against `AvatarForge` `work/android-engine-dev` at `ba45d592773988599d1d7f99618c911e63f9216e` (branch head dated 2026-11-15, and **newer than the `9e36197ace3b18faa6c566d93f8fce89b185c45d` head this baseline recorded** — the donor has advanced past its own baseline): `tools/build_host.sh test` finishes with **`All 537 checks complete: 0 failure(s)`**. So the numbers in circulation are one time series plus one unrelated branch, not four claims about one thing. `405` is the donor's own 2026-10-18 checkpoint and its `STATUS.md` already labels it historical, noting later runs reached 411, 421 then 426. `464` is what `README.md` and `build_host.sh`'s own header comment still say at a head whose harness has since grown to 537 — both are repository-authored and out of date, which is precisely what the line above told a reader to assume until the command was reviewed. It has now been reviewed. `386` belongs to `arena/bf31ed07-avatarforge` and stays non-comparable.
>
> Corroborating measurements from that same 537-check run, which agree with the fixture this repository took from the donor's `artifacts/avatarforge-sample.glb`: section `[11]` reports `71 joints, 52 blendshapes` on 293,887 verts / 392,218 tris / 12 submeshes; `[14]` exported 36,496,868 bytes (34.8 MB) and imported it back as 268,923 unique vertices from 2,420,307 referenced, 365,680 triangles, 72 nodes, 13 materials, 5 textures in 1 pool. The 71-joint / 52-morph / 72-node topology is therefore the donor's own, confirmed by its own harness and not inferred from one file.

> **DIVERGENCE DIRECTION, MEASURED — the port here is AHEAD of the donor engine, not behind.** Diffing the donor's `native/src` against the copies this repository carried at its base commit `f73eec1`, donor-only vs port-only lines are: `af_engine.cpp` 101 vs 420, `af_field.cpp` 69 vs 185, `af_texture.cpp` 30 vs 39, `af_body.cpp` 4 vs 23, `af_glb.cpp` 4 vs 7, with `af_rig.cpp` and `af_anatomy.cpp` identical. The donor-only lines are the older text of functions the port rewrote; the donor defines **no** `af_*` engine entry point the port lacks. A symmetric diff total read as "the donor has 521 new lines in `af_engine.cpp`" was wrong, and it is corrected here because it changes the decision: there is no engine code to take back from AvatarForge. What is worth taking from it is *data* — which is what `app/src/main/assets/models/animation/AvatarForgeSkeleton.glb` and `docs/AVATARFORGE_SKELETON_MANIFEST.json` now do.
- Candidate role: native generation kernel, shared C ABI, Kotlin SDK, rigging/blendshape/animation data and diagnostics. Do not select its custom Vulkan/GLES renderer as the host default until it passes the same visible-frame and stability tests as the chosen renderer.
- No root source `LICENSE` file appeared in the inspected branch tree. Third-party codec licences are present, but those do not grant a licence to the full project. Clarify source and asset licensing before copying or redistributing code.
- Status for Mega Hub: **candidate only; not imported or compiled here**.

## Other donor candidates

### Aaa
- Repository: https://github.com/davealone69-gif/Aaa
- Default branch previously observed as `main`.
- Candidate role: SDK/provider contracts, model lifecycle, jobs, persistence and resource governance.
- README claims about APK/AAB/tests require exact workflow/artifact verification. Physical-device rendering and generation were not established here.
- Status: **candidate only; source/build/licence audit pending**.

### Grok-Girls
- Repository: https://github.com/davealone69-gif/Grok-Girls
- Default branch previously observed as `main`.
- README describes React/Vite and Capacitor packaging. This is not automatically a native renderer or embedded LLM runtime.
- Candidate role: inspect specific useful UX flows, regression cases and optional provider adapters.
- Status: **candidate only; source/build/licence audit pending**.

### Aura-Studio-Avatar
- Repository: https://github.com/davealone69-gif/Aura-Studio-Avatar
- Default branch previously observed as `main`.
- Candidate role: inspect avatar library, settings and diagnostic patterns.
- Status: **candidate only; source/build/licence audit pending**.

## Next evidence required

1. Resolve source licence and model/texture/voice asset licences for each intended donor.
2. Fix or verify the 3DDD backup-rule conflicts and Room schema/KSP failure against a current branch/commit.
3. Review the successful AvatarForge `work/android-engine-dev` workflow artifact and its exact branch history; do not use the unrelated default `main` branch as a substitute.
4. Create a small isolated renderer proof in Mega Hub before importing the native generation engine.
5. Record every adopted component's source repo, branch, SHA, licence, files/dependencies, local changes and test evidence.
6. Keep CI and target-phone evidence separate. Never label a feature REAL + VERIFIED without the relevant result.

## Important limitation

No donor code has been copied into Mega Hub by this snapshot. No local Gradle build was run in this audit session. No physical-device test was performed.
