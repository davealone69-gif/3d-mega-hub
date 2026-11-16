# AGENTS.md

Working notes for an agent in this repository. Everything here was learned by hitting it; the point is
that none of it should have to be learned twice.

## What this repository is

A 3D avatar app (`app/`) with its own native engine module (`avatarforge-engine/`). Two renderers
coexist and both are real:

- **AvatarForge GLES** — the engine's own renderer. CPU deform / GPU skin, `AvatarForgeSurfaceView`.
- **Filament** — Google's renderer via gltfio, `FilamentPreviewView`, used for imported animated GLBs.

The animation core (`avatarforge-engine/src/main/cpp/src/anim/`, namespace `af::anim`) is
renderer-agnostic and is consumed by **both**. It owns *time and blending*; each renderer owns
*sampling and skinning*. That split is the architecture, not a detail — a second state machine or
viseme table written in Kotlin or in a renderer backend would be a second animation engine, and the two
would drift.

## Environment reality (read this before trying to build)

There is **no JDK, no Gradle, no Android SDK and no NDK** in the sandbox, and `dl.google.com` is
unreachable, so the Android Gradle Plugin and the Filament AARs cannot be downloaded. Consequences:

- `./gradlew` cannot run. **Never claim a locally built APK or AAB.**
- Kotlin in this repo can only be compiled by GitHub Actions. Treat any Kotlin change as unverified
  until a CI run reports it. Lint and R8 jobs compile Kotlin; only the `Debug APK build` job runs
  `externalNativeBuild`, so it is the only job that proves a JNI file compiles against the real NDK
  `jni.h`.
- Host C++ **does** build and test, with two pieces of setup that are sandbox gaps, not repo defects:
  - `cmake` from a venv: `python3 -m venv ~/.venv-cmake && ~/.venv-cmake/bin/pip install cmake`
    (a system `pip install cmake` fails under PEP 668; do not use `--break-system-packages`).
  - a **zlib shim**: the sandbox has `libz.so.1` but no zlib development headers. `af_png.cpp` is the
    only consumer and needs `crc32`, `compressBound`, `compress2`, `Z_OK`, `Bytef`, `uInt`, `uLong`,
    `uLongf`. Note `uLongf` is a **value** type (`typedef uLong FAR uLongf`) and `compress2` takes a
    **pointer** to it; getting that backwards breaks the build. **Never commit the shim and never add
    zlib fallback code to the tree** — the repo is correct, the sandbox is not.

`/tmp` and anything outside the repository root does **not** persist between turns. Build directories,
shims, donor clones and logs can vanish; expect to recreate them and do not treat their absence as a
regression.

## Host build and test (the loop that actually works)

```bash
export PATH=$HOME/.venv-cmake/bin:$PATH
cmake -S avatarforge-engine/src/main/cpp -B /tmp/build -DBUILD_TESTING=ON -DAF_OPT_FLAGS=-O1 \
  -DCMAKE_CXX_FLAGS="-I/tmp/zshim" \
  -DCMAKE_EXE_LINKER_FLAGS="-L/tmp/zshim" -DCMAKE_SHARED_LINKER_FLAGS="-L/tmp/zshim"
cmake --build /tmp/build --parallel 8
ctest --test-dir /tmp/build --output-on-failure
```

Eight ctest jobs. `animation_system_regression` (`af_animation_system_test`, 398 checks) covers the
animation core; `imported_animation_regression` (`af_imported_animation_test`, 129 checks) covers the
engine's import path end to end — it imports the real `Fox.glb`, `CesiumMan.glb`,
`AnimatedMorphCube.glb`, `CliplessRigged.glb` and `female.glb` through `af_engine_import_glb_from_memory`
and asserts on the **vertex buffer a renderer would be handed**, not on the animation core's internals. Both load fixtures
from `app/src/main/assets/models/animation/` — override with `-DAF_ANIMATION_TEST_ASSETS`, `argv[1]`, or
`AF_TEST_ASSET_DIR`.

All three JNI translation units (`avatarforge_jni.cpp`, `avatarforge_renderer_jni.cpp`,
`avatarforge_animation_jni.cpp`) syntax-check clean under `-Wall -Wextra` against the throwaway shim in
`/tmp/jnishim` — **never commit the shim**. Beyond `jni.h` it needs `android/native_window.h`,
`android/native_window_jni.h`, `android/bitmap.h` and `android/log.h`, plus `JavaVM`,
`jmethodID`/`jfieldID`, the `JNI_VERSION_*` and `JNI_E*` constants, and the `Call*Method` /
`Get*Field` / `Set*Field` families. A shim that only covers one file proves nothing about the other two.

Two gotchas: piping the test binary through `head` gives exit 141 (SIGPIPE), so redirect to a log and
grep it; and `CMAKE_CACHE` entries go stale when an in-tree default changes, so `rm CMakeCache.txt`
after editing one.

## Running CI

`Android CI` triggers on `main`, `work/**`, `fix/**`, `integration/**` — **not** on `arena/**`
branches, which is the branch name this workspace uses. Dispatch it explicitly:

```bash
gh workflow run android.yml --ref <current-branch>
gh run watch <run-id> --exit-status
```

Job logs are served from a separate results host that is usually unreachable, and the check-run API
otherwise reports only "Process completed with exit code 1". So the Gradle jobs tee to a log and, on
failure, `tools/ci_report_test_failures.py` re-emits the cause as annotations and a job summary. Read
them with:

```bash
gh api repos/davealone69-gif/3d-mega-hub/check-runs/<job-id>/annotations \
  -q '.[] | "[\(.annotation_level)] \(.title) :: \(.message)"'
```

GitHub caps annotations at **10 per level per run**. That cap is a real constraint: an earlier version
listed every testcase element and spent the whole error budget on tests that had passed before
reporting a single failure. Spend it on failures first, log context second.

**`AvatarForge GLES GLB render smoke (instrumented)` is red on the base branch too.** It is
pre-existing and unrelated to animation work; do not chase it, and do not claim it as a new failure.

## Tools

- `tools/glb_animation_probe.py` — dependency-free GLB inspector. `--require-skin` fails on assets with
  no skeleton, which is the point for `AnimatedMorphCube.glb`.
- `tools/check_jni_kotlin_parity.py` — compares every Kotlin `external fun` against its JNI symbol by
  name, arity and type. **A JNI mismatch is not a link error**: the `.so` builds, the APK installs, and
  it fails at the first call as an `UnsatisfiedLinkError` on a device. This runs in CI before the native
  build. If you add a bridge method, add it to both sides and run this.
- `tools/ci_report_test_failures.py` — see above.

## Verification discipline

This repo has a history of plausible-looking claims that were not measurements. Hold the line:

- **Measure, don't assume.** Print the number. "The engine builds a body mesh" was reported alongside
  `0 triangles` for a long time before anyone looked at the output.
- **Beware vacuous truths.** `all {}` on an empty collection is `true`, `any {}` on empty is `false`,
  and a test that asserts a mesh has vertices passes when it has zero triangles. A coarse
  `BodyBuildOptions` produces 18,976 vertices and **zero** triangles, and `Rig::build` accepts it — the
  skeleton comes from the `BodyField`, not the mesh, so animation tests pass on a mesh that cannot
  render. Always assert `!mesh.empty()` and use the shipped options (`detailRounds >= 1`,
  `smoothPasses = 1`).
- **Assert definitions and closure properties, not asset-unit guesses.** Absolute thresholds on bone
  palettes or root deltas encode one asset's scale. Fox is in centimetres (hip 42.94); CesiumMan is in
  metres (hip 0.679). Assert `scale == dstHip/srcHip`, and that a looping clip's first and last pose
  agree, rather than that a delta is under some magic number.
- **A refused operation must be visible.** The core never invents motion: `Clip::fromGlb` fails rather
  than synthesising, `FaceController::unbound()` reports requested-but-absent channels, `buildPalette`
  produces nothing without inverse bind matrices, `requestState` refuses an undeclared edge,
  `AnimationLibrary` lists only clips that really loaded, and the reader *reports* an unreadable
  channel instead of guessing. A UI control must surface those refusals — a button that silently does
  nothing is indistinguishable from a bug.
- **State what is NOT verified.** Audio-driven lip-sync is not implemented; the text-driven
  `LipSyncTrack` maps spelling to a 15-viseme set and must never be presented as audio-driven. On-device
  pixels and emulator smoke remain unverified. Signed release APK/AAB is blocked on a user keystore in
  CI secrets — never substitute debug or ephemeral keys.

## Traps that have already cost time

- glTF accessor widths: morph `weights` are **SCALAR**, translation/scale are **VEC3**, rotation is
  VEC4. A hard-coded component count of 4 makes `readAccessorFloats` reject every translation channel,
  and a bare `continue` swallows it — Fox then walks in place with no error anywhere. Never let
  `continue` drop a channel without setting `error`.
- CUBICSPLINE per-key order is **(in-tangent, value, out-tangent)**; all samplers go through
  `cubicSlot()`. Equal in/out tangents cancel exactly, so pin each slot separately in a test.
- Filament's `Animator.applyCrossFade(previousIndex, previousTime, alpha)`: the animation passed to
  `applyAnimation` is **current at alpha = 1**, the previous one is **alpha = 0**. Inverting this does
  not crash — it plays every transition backwards, fading out of the clip the user asked for. The
  convention lives in `af::anim::CrossFadeCommand` and is pinned by host tests, because a JNI
  expression cannot be compiled or tested here.
- During a crossfade the two clips run on **independent clocks** and usually differ in duration. The
  source clock is `Player::previousPosition()`, not something derived from `position()` and the
  progress.
- `Quat` is stored `{x,y,z,w}`; `Mat4` is column-major. Any angle metric built from `2*acos|qa·qb|`
  must divide by `|qa||qb|` — `blendPoses` output has `|q|^2 ≈ 0.999999881`, which otherwise reads as
  a phantom 0.056°/joint difference.
- `Skeleton::buildPalette` emits **skinning matrices**, ≈ identity at rest. It is not a
  character-scale measure; use `rest.world[]` or `hipHeight()`.
- `jointForRole(Role::Hips)` returns the **deepest** Hips joint; root motion goes through
  `rootMotionJoint()`. A midline `hip`/`pelvis` bone is spine, not leg (sided `hip_L/R` stays leg).
- `findFuzzy` token matching means some humanoid clips DO bind to a quadruped like Fox. Never assert
  `bound == 0`; assert `bound + skipped == total` and that every skip message names its track.
- Drawing a blink interval eagerly inside setters makes the RNG stream call-order dependent. Draw
  lazily; `setBlinkSeed` must restart the timer and phase but not the weights.
- Python heredoc `str.replace` **silently no-ops** when the target text has drifted. Always
  `assert old in s` — an `AssertionError` means the edit did not land. Bash heredocs also occasionally
  return an empty result; re-issue as a single `mkdir -p … && cd … && cat > file <<'EOF'` command and
  never assume the file was written.
- **Two skeletons, two transports.** An import sets the engine's authored `Rig` aside, so
  `af_engine_apply_clip` answers `AF_ERR_NOT_READY` for an imported file — including for
  `AF_CLIP_REST`, which reads like it should always work. Drive an imported file through
  `af_engine_imported_*`. The GLES renderer's `nativeCreate` called `apply_clip(AF_CLIP_REST)` whenever
  `joint_count > 0`; since an import now adopts the file's skeleton that condition is true for imports
  too, and the whole session would have been thrown away over a file that renders fine. Branch on
  `af_engine_imported_clip_count()` first.
- `Player::loadGlb` **auto-selects clip 0 and sets `playing_ = true`**. That suits a player about to be
  ticked and is wrong for "show me the model I just loaded": left alone, every import silently appears
  in the first frame of whichever animation the file happened to list first. Call `resetToBindPose()`
  (the equivalent of gltfio's `Animator.resetBoneMatrices()`).
- `Player::seek` **clamps** to `[0, duration]`; `Player::advance` **wraps** a looping clip. A test that
  assumes one behaves like the other still passes on an asset whose last keyframe equals its first, so
  pin both explicitly — `imported_animation_test.cpp` asserts `advance(1.5*dur)` lands on `0.5*dur` and
  that `seek(1.25*dur)` equals `seek(dur)`.
- `Player::seek` is a **no-op while no clip is selected** (`currentClip() < 0`). Forwarding it and
  returning success reports a move that never happened, so `af_engine_imported_seek` and `_advance`
  refuse with `AF_ERR_NOT_READY` and a message naming the call that is missing.
- **Two different `checkf` helpers.** `animation_system_test.cpp`'s is variadic, so the format string
  must match the argument types (`%d` with an `int`, `%zu` with a `size_t`). `imported_animation_test.cpp`'s
  takes exactly two `double`s, so a `%d` there reads a double as an int and prints plausible garbage —
  one run reported a joint count of `-1635907128` for a skeleton that had 21, and a later one printed
  `4626885667169763328 of 1 matrices changed`. A green check with a nonsense number in its message is
  worse than a red one, because the message is what the next reader trusts.
- `AvatarForgeSurfaceView.nativeGetStats` must return **exactly 9 ints** — Kotlin asserts the size, so
  adding a field breaks the render loop. And `nativeHandle` is a **local inside the render thread**, not
  a field: new JNI entry points that need the session handle require hoisting it first, with real
  lifecycle care, because a call that lands after `nativeDestroy` is a use-after-free in native code.
- Keep generated artifacts, build directories and large datasets out of Git. `.gitignore` already
  covers `__pycache__`, keystores and the usual build outputs.
