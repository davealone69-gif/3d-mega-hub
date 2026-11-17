# Runtime evidence — 2026-11-17 (UTC)

What the app actually did when it was built, installed, launched and interacted with. Every line
below comes from a workflow run on this branch, captured by
`tools/ci/emulator_runtime_capture.sh` and published on
[PR #14](https://github.com/davealone69-gif/3d-mega-hub/pull/14) because this repository's workflow
logs and artifacts are served from hosts that are not always reachable.

**This is emulator evidence, not physical-device evidence.** The emulator renders through
ANGLE/SwiftShader on the host, so it proves the pipeline runs and produces pixels; it does not prove
Samsung SM-S921B behaviour, GPU driver behaviour, thermal behaviour or real frame rates.

## Levels

| Level | Result | Evidence |
| --- | --- | --- |
| 1 — source validation | **PASS** | `testDebugUnitTest` (46 tests), `lintDebug`, `minifyReleaseWithR8`, native `ctest` (6 suites) |
| 2 — Android build | **PASS** | `assembleDebug` produces `app-debug.apk`; `assembleDebugAndroidTest` produces the test APK |
| 3 — installation | **PASS (emulator)** | `adb install -r -t` returned `0` for both APKs |
| 4 — runtime | **PASS (emulator)** | cold launch, resumed activity, non-black frame, 150 monkey events, no crash |
| 5 — integration | **PARTIAL** | one renderer instrumentation class passes its first test; see the open question below |

## Level 3/4 detail

`Android CI` run `44099563805`, job *App install, launch and runtime evidence*, on this branch:

```
Starting: Intent { cmp=com.davemega.hub/.MainActivity }
Status: ok
LaunchState: COLD
TotalTime: 1353          # ms, cold start
```

```
mResumedActivity: ActivityRecord{... com.davemega.hub/.MainActivity}
```

Raw `screencap` analysed on the runner (no third-party libraries):

```
raw frame: 8294416 bytes, header says 1080x1920 format=1
mean rgb: (44, 43, 49)
non-black fraction: 0.9362
bright-pixel fraction: 0.6185
distinct quantised colours: 66
verdict: the captured frame contains rendered content
```

After 150 fixed-seed monkey events (second capture):

```
monkey exit: 0
Events injected: 150
// Monkey finished
non-black fraction: 0.9265
distinct quantised colours: 117
mResumedActivity: ActivityRecord{... com.davemega.hub/.MainActivity}
crash buffer lines: 0
```

So the app starts, stays up, survives random interaction without a crash or ANR, and keeps drawing.
Memory after the pass: **TOTAL PSS 133,929 KB** (Java heap 18,112 KB, native heap 65,412 KB).

## The renderer really initialises

Filtered logcat from the same run. This is Filament's own output, not the app's claim:

```
MegaHubRenderer: Renderer status: Loading Filament native runtime
MegaHubRenderer: Renderer status: Filament and gltfio JNI loaded; initializing OpenGL ES first
Filament: FEngine (64 bits) created at 0x76d416d0d110 (threading is enabled)
Filament: FEngine resolved backend: OpenGL
Filament: [Google], [Android Emulator OpenGL ES Translator (ANGLE ... SwiftShader)], [OpenGL ES 3.1], [OpenGL ES GLSL ES 3.10]
Filament: Feature level: 1
Filament: Active workarounds: disable_es2_srgb_ext
MegaHubRenderer: Renderer status: Filament OpenGL ES session initialized; starting GLB viewer
MegaHubRenderer: Renderer status: Filament OpenGL ES frame submitted; model-pixel visibility still requires device confirmation
```

The engine is created once, resolves the OpenGL backend, reports feature level 1, applies the
`disable_es2_srgb_ext` workaround and submits a frame. That is the lifecycle the previous code path
was suspected of breaking.

## Open question: the instrumented renderer tests

`GeneratedAvatarRuntimeTest` reports `numtests=4`:

1. `generatedAvatarDebugShaderMustProduceMagentaPixels` — **passed** (`STATUS_CODE: 0`)
2. `generatedAvatarTextureViewWindowCaptureA_B`
3. `generatedAvatarMustProduceVisiblePixels`
4. `generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView`

The earlier capture truncated the log before the verdict for tests 2–4, and the pre-existing
`Android CI` job *AvatarForge GLES GLB render smoke (instrumented)* — which runs a different class,
`AvatarForgeRendererSmokeTest` — still fails. The capture script now posts one line per status code
plus the first stack trace, so the next run states exactly which assertions fail and why.

**Until that lands, level 5 stays PARTIAL and no animation or model-visibility claim is made.**

## What this evidence does not cover

- Physical Samsung SM-S921B: **UNVERIFIED**, no device run exists.
- Generated-avatar pixel visibility on real hardware: **UNVERIFIED**.
- Animation playback of imported clips: **UNVERIFIED**; the GLES static path does not animate
  imported rigs, and the Filament preview's animation controls have never been exercised.
- Release signing: **BLOCKED** by the keystore gate, unchanged.
- R8-minified runtime behaviour: **UNVERIFIED**; only `minifyReleaseWithR8` compiling is proven.
