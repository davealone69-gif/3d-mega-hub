# Donor Baseline Snapshot

Checked against GitHub repository metadata, source files and workflow results on 2026-10-30. This is a targeted source audit, not a full line-by-line audit or local build.

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
- The `work/android-engine-dev` README reports 464 host checks and explicitly says Android NDK build, Kotlin compilation, compressed GPU upload paths and physical-device rendering are not yet verified. Treat these as repository-authored claims until the exact test command/run is inspected.
- The `arena/bf31ed07-avatarforge` README reports 386 host checks and says physical-device execution is unverified. Do not mix the check count from one branch with another branch's source.
- Candidate role: native generation kernel, shared C ABI, Kotlin SDK, rigging/blendshape/animation data and diagnostics. Do not select its custom Vulkan/GLES renderer as the host default until it passes the same visible-frame and stability tests as the chosen renderer.
- No root LICENSE file appeared in the inspected default-branch tree listing. Clarify licensing before copying or redistributing source/assets.
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

1. Inspect the exact latest AvatarForge engine branch and workflow runs; reconcile source state with the user's expected branch.
2. Inspect all donor licences, model/texture/voice licences, dependency licences and attribution obligations before importing code/assets.
3. Fix or verify the 3DDD backup-rule conflicts and Room schema/KSP failure against a current branch/commit.
4. Build/test the selected 3DDD revision in CI; review instrumentation results including GPU-related skips.
5. Create a small isolated renderer proof in Mega Hub before importing the native generation engine.
6. Record every adopted component's source repo, branch, SHA, licence, files/dependencies, local changes and test evidence.
7. Keep CI and target-phone evidence separate. Never label a feature REAL + VERIFIED without the relevant result.

## Important limitation

No donor code has been copied into Mega Hub by this snapshot. No local Gradle build was run in this audit session. No physical-device test was performed.
