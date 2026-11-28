# Android verification prompt — paste-ready brief for external test agents

**Audience:** agents with a real Android toolchain (JDK, Android SDK, NDK, emulator and/or a
physical phone). The authoring workspace was host-only — no JDK, no Android SDK, no NDK, no KVM —
so everything below the "host verification" line is **UNVERIFIED** and is exactly what you are being
asked to establish.

**Repository:** `davealone69-gif/3d-mega-hub`
**Branch to test:** `arena/1535c8f6-3d-mega-hub`
**Head commit at time of writing:** `88c2c67` — *"Merge trunk integration/avatarforge-renderer-port
into the working line"* (a true merge commit; its parents are `e6f28ba` and trunk `9baf7c6`).

> If the branch has not been pushed yet you will be told the exact ref. Test **one exact commit SHA**
> and record that SHA in your report. Do not test a moving branch tip.

---

## 1. Why this needs you

Two changes landed that no host check can validate:

1. **Screen-density LOD selection** (commit `07610b5`, now inside `88c2c67`).
   The renderer previously chose an avatar LOD per submesh from hard-coded *metre* thresholds
   (`distance >= 16.0f ? 2 : >= 8.0f ? 1 : 0`) while Avatar Studio's default orbit framing sits at
   `distance = 3.0 m`, `fovDeg = 32.0`. Those thresholds were unreachable, so the app always drew
   LOD0 — the full ~1.1 M-triangle mesh — which is what `docs/PHONE_TEST_2026-11-15.md` observed.
   The replacement picks **one level per avatar** from a projected screen-density target
   (`kAvatarLodPixelEdgeTarget = 4.0f`, `kAvatarLodHysteresis = 1.25f`) in
   `avatarforge-engine/src/main/cpp/src/render/af_lod_select.h`, wired at the draw site in
   `src/render/af_gl.cpp`.
   Host status: the maths, measured per-level triangle areas and 11/11 native tests are
   **REAL + VERIFIED on host**; the GLES wiring is **IMPLEMENTED IN SOURCE**; the on-device effect is
   **UNVERIFIED**. Only a device or emulator can confirm it.

2. **Bounded GLB import** (from trunk). `FilamentPreviewView.kt` now calls
   `BoundedGlbReader.read(stream, context.cacheDir, MAX_MODEL_BYTES)` instead of a removed local
   `readBounded(stream)`. Trunk's README records a real `OutOfMemoryError` when the old path tried to
   allocate 134,217,744 bytes. The new reader spools through the cache directory and allocates one
   exact array. The merge resolution moved the call **inside** the `else` block where `stream` is in
   scope — that is a Kotlin compile risk no host check could cover.

---

## 2. Toolchain you must have (values read from the repo, not assumed)

| Item | Value | Source |
| --- | --- | --- |
| Gradle wrapper | `8.11.1` | `gradle/wrapper/gradle-wrapper.properties` |
| Java bytecode target | 17 (`sourceCompatibility`, `kotlinOptions.jvmTarget`) | `app/build.gradle.kts:81,84` |
| JDK used by existing CI | 21 | `.github/workflows/android.yml` |
| `compileSdk` / `targetSdk` | 35 / 35 | `app/build.gradle.kts:44,49` |
| `minSdk` | 26 | `app/build.gradle.kts:48` |
| NDK | `26.1.10909125` | `avatarforge-engine/build.gradle.kts:9` |
| ABIs | `arm64-v8a`, `x86_64` | `app/build.gradle.kts:58` |
| Host native tests | CMake + Ninja, `-DBUILD_TESTING=ON -DAF_OPT_FLAGS=-O1` | `.github/workflows/android.yml:24` |

Use a **JDK 17 or 21**. Emulator: an `x86_64` system image at API 35 (or anything ≥ `minSdk` 26)
with hardware acceleration available. Report the exact device/emulator identity you used.

---

## 3. Run these, in this order, and stop at the first hard failure

Set up:

```bash
git fetch origin arena/1535c8f6-3d-mega-hub
git checkout 88c2c67            # or whatever exact SHA you were given
git rev-parse HEAD              # record this in your report
./gradlew --version
```

**Step A — host native tests** (mirrors CI; expected **11/11**, including
`avatar_lod_select_regression`):

```bash
cmake -S avatarforge-engine/src/main/cpp -B build/native-tests \
  -DBUILD_TESTING=ON -DAF_OPT_FLAGS=-O1
cmake --build build/native-tests --parallel 2
ctest --test-dir build/native-tests --output-on-failure
```

**Step B — JVM unit tests** (must include `BoundedGlbReaderTest`):

```bash
./gradlew --no-daemon testDebugUnitTest --stacktrace
```

**Step C — debug APK build** (this is the Kotlin-compile check on the merge resolution; watch for
`Unresolved reference: readBounded`, a scope error on `stream`, or an OOM during import):

```bash
./gradlew --no-daemon assembleDebug --stacktrace
ls -l app/build/outputs/apk/debug/
```

**Step D — lint and R8:**

```bash
./gradlew --no-daemon lintDebug --stacktrace
./gradlew --no-daemon minifyReleaseWithR8 --stacktrace
```

**Step E — instrumented tests on the emulator.** This is the area with known red CI: the workflows
`AvatarForge GLES GLB render smoke (instrumented)` and `Generate, render and inspect real avatar
pixels` fail on every currently open PR (#9, #14, #15), so a green result here would be real news and
a red result needs the exact failure.

Suites that exist at this commit:

- `AvatarForgeRendererSmokeTest` — `importedBundledGlbUsesAvatarForgeGlesAndProducesVisibleGeometry`,
  `requiredWebpTextureExtensionIsDecodedAndRenderedByAvatarForge`,
  `jpegTextureIsDecodedAndRenderedByAvatarForge`,
  `generatedAvatarDebugDiagnosticVerifiesImportedGlesPixels`
- `GlbClipPreviewTest` — `importedRiggedFileListsItsClipsByNameAndPlaysTheOneThatIsTapped`
- `avatarforge/GeneratedAvatarRuntimeTest` (5 tests) —
  `nativeExceptionBarrierTurnsCppExceptionsIntoJavaExceptions`,
  `generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView`,
  `generatedAvatarDebugShaderMustProduceMagentaPixels`, `generatedAvatarTextureViewWindowCaptureA_B`,
  `generatedAvatarMustProduceVisiblePixels`
  (this suite writes evidence files via its own `saveEvidence()` — collect them)

```bash
./gradlew --no-daemon connectedDebugAndroidTest --stacktrace
```

To target the runtime suite alone:

```bash
./gradlew --no-daemon connectedDebugAndroidTest \
  -Pandroid.testInstrumentationRunnerArguments.class=com.davemega.hub.avatarforge.GeneratedAvatarRuntimeTest \
  --stacktrace
```

Note `generatedAvatarDebugShaderMustProduceMagentaPixels` deliberately exercises the magenta debug
shader, and the LOD path is explicitly disabled while `debugMagenta` is on
(`useLodIndices` requires `!debugMagenta`) — a magenta result there is correct and is **not** LOD
evidence. Step F is the LOD evidence.

**Step F — the LOD observation (manual or scripted; this is the point of the exercise).**
Install the debug APK, open **Avatar Studio**, generate/load an avatar, and capture logcat while you
move the orbit camera. The renderer logs, once per change of level:

```
Avatar LOD selected: level=%d of %d sourceTriangles=%d drawnTriangles=%d \
pixelEdge=%.3f target=%.3f distance=%.3fm viewportH=%d fov=%.1f measured=%d
```

and, when the LOD streams are built:

```
Avatar LOD ready: levels=%d sourceTriangles=%d lod1Triangles=%d lod2Triangles=%d
```

```bash
adb logcat -c
adb logcat -v time | grep -E "Avatar LOD (selected|ready|unavailable)"
```

Sweep the camera through roughly **1 m → 3 m → 12 m** and hold each for a second or two.

---

## 4. What "pass" means for Step F

Report the **measured** numbers from the log lines. Do not substitute anyone else's numbers,
including the host-derived ones below — those were computed for one specific case (1080×2340
viewport, 32° vertical FOV, ~1.1 M source triangles, ~1.8 m² surface area) and are given only so you
can tell whether your measurements are in the right neighbourhood:

- world-per-pixel at 3.0 m = `2·d·tan(fov/2)/height` = **7.352446e-4 m**
- projected mean triangle edge: LOD0 ≈ **2.6 px**, LOD1 (~132k tris) ≈ **7.6 px**,
  LOD2 (~55k tris) ≈ **11.8 px**
- with a **4.0 px** target this should select **LOD1 at 3 m**, **LOD0 at ~1 m**, **LOD2 at ~12 m**
- the hysteresis dead-band (`1.25`) should prevent level flapping around **1.8 m / 2.2 m**

Pass criteria, in your words plus the raw log lines:

1. `measured=1`, and `level` is **not pinned at 0** at the app's default framing — i.e. the old
   always-LOD0 behaviour is gone.
2. `drawnTriangles` is materially below `sourceTriangles` at the default framing, and equals
   `lod1Triangles` / `lod2Triangles` from the `Avatar LOD ready` line for the selected level.
3. The chosen level's `pixelEdge` is ≥ `target`, or (for the finest level) is the finest level
   available — the policy is *finest level meeting the floor*, so LOD0 at close range is correct.
4. No flapping: at a fixed distance the logged level stays stable; walking the distance across a
   boundary changes it once, not repeatedly.
5. The avatar still renders correctly — no cracks, no detached hair or scalp, no broken skinning or
   morphs. **This matters:** every LOD index stream reuses original vertex indices, so skinning and
   morph targets should be untouched. If you see any deformation or tearing that is absent at LOD0,
   report it as a defect with a screenshot, not as a pass.
6. Visible frame-time / smoothness improvement versus the always-LOD0 baseline, if you can measure
   it. If you cannot measure it, say so — do not estimate.

If `Avatar LOD unavailable; retaining original full-detail index buffer` appears, that is a
**FAIL of the LOD path** (the streams were not built) — capture the surrounding log.

---

## 5. Hard rules (from `AGENTS.md` and the README handover notes)

- Use the project's status vocabulary and always say **where** something was verified:
  `REAL + VERIFIED`, `IMPLEMENTED IN SOURCE`, `PARTIAL`, `UNVERIFIED`, `MISSING`, `BLOCKED`.
- **No invented numbers.** Every figure in your report must come from an actual test result, log
  line, or measurement you took. If a number is unavailable, write `not measured`.
- **Exact-head evidence.** Record the full commit SHA you tested. Never carry an earlier green
  result forward to a different head — a result belongs only to the commit that produced it.
- **An emulator pass is never a Samsung pass.** State the device model, Android version and GPU.
  If you only have an emulator, say `emulator only`.
- Do **not** commit, pack into an APK, or upload the owner's Dropbox material or any `mylove*.glb`
  asset. They must never appear in the repository or a distributed artifact.
- Keep the UI label exactly `Run generated-avatar GLES diagnostic`.
- Do not publish or distribute an APK that has not passed the gates.
- Do not force-push and do not touch `main`.
- Do not hide or swallow the Filament `sheenColorIndex` `PreconditionPanic` if you encounter it —
  report it. Keep native GLES and the Filament preview clearly distinct.
- **Never "fix" a failure by disabling a test, weakening an assertion, or widening the diff.** Your
  job is evidence. If something fails, report it.

---

## 6. Report format

Reply with, in this order:

1. `commit tested:` full SHA; `device:` model / emulator identity, Android version, GPU.
2. A results table — one row per step A–F: step, command, **PASS / FAIL / BLOCKED**, and the count
   or one-line outcome (e.g. `11/11 tests`, `4/5 instrumented`).
3. The raw `Avatar LOD selected:` and `Avatar LOD ready:` lines, verbatim.
4. For every FAIL: the exact error, the failing test name, the relevant stacktrace or logcat excerpt,
   and the path to any artifact (test XML, screenshot, screen recording).
5. Anything you could not run and why (`BLOCKED: no KVM`, `BLOCKED: NDK not installed`, …).

Keep artifacts in your own workspace; do not add binaries to the repository.
