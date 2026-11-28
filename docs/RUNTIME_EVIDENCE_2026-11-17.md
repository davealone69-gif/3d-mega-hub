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

## The generated-avatar render gate passes on the emulator

`Android CI` run `44120205601`, job *App install, launch and runtime evidence*, instrumented class
`com.davemega.hub.avatarforge.GeneratedAvatarRuntimeTest`:

```
test=generatedAvatarDebugShaderMustProduceMagentaPixels          STATUS_CODE 1 -> 0
test=generatedAvatarTextureViewWindowCaptureA_B                  STATUS_CODE 1 -> 0
test=generatedAvatarMustProduceVisiblePixels                     STATUS_CODE 1 -> 0
test=generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView  STATUS_CODE 1 -> 0
Time: 473.665
OK (4 tests)
INSTRUMENTATION_CODE: -1        # RESULT_OK; "OK (4 tests)" is the verdict
```

That is the central path of the project end to end: the native AvatarForge generator produces an
avatar, the avatar is exported as a GLB, the GLB is loaded and rendered through the **production**
`AvatarForgeSurfaceView` GLES path, and a `PixelCopy` capture confirms visible, geometrically
non-trivial pixels. Four of four pass, 473 seconds.

This is emulator (ANGLE/SwiftShader) evidence. It is strong evidence that the pipeline is correct and
that no code path is stubbed; it is not evidence about Samsung GPU driver behaviour.

## The remaining renderer blocker: a native panic in Filament's loader

Run `44130209112` captured the tombstone. The sequence is unambiguous:

```
17:03:58.723 MegaHubRenderer: Renderer status: Filament and gltfio JNI loaded; initializing OpenGL ES first
17:03:59.126 libc++abi: terminating due to uncaught exception of type N5utils17PreconditionPanicE: Precondition
17:03:59.126 libc    : Fatal signal 6 (SIGABRT) in tid 6465 (om.davemega.hub)
17:03:59.623 ActivityManager: Process com.davemega.hub (pid 6465) has died: fg TOP
```

Tombstone `tombstone_00`, thread `om.davemega.hub`:

```
Abort message: 'terminating due to uncaught exception of type N5utils17PreconditionPanicE: Precondition
#02..#10   libfilament-jni.so        (offset 0x3d48000, BuildId 88dacb49…)
#11..#14   libgltfio-jni.so          (offset 0x40e0000, BuildId 67d04d19…)
#15        filament::gltfio::AssetLoader::createAsset(unsigned char const*, unsigned int)+14
#16        Java_com_google_android_filament_gltfio_AssetLoader_nCreateAsset+46
#24        com.google.android.filament.gltfio.AssetLoader.createAsset+12
```

So: the app calls `AssetLoader.createAsset`, gltfio parses the asset, then a **Filament core
precondition** throws `utils::PreconditionPanic`, which nothing catches, which aborts the process.
It happens about 400 ms after the GLB viewer starts, i.e. on the first preview load, and it kills the
whole app rather than failing a load.

This is the Filament 1.80.1 behaviour change that compilation could not reveal: the API still
compiles, but a failure that used to return `null` now terminates the process. It is why the
*AvatarForge GLES GLB render smoke* job has been red, and it is the single reason the app's Filament
preview path is unverified — the path that owns **animation controls**, since only Filament can play
imported clips.

### What has been ruled out

The bundled `models/female.glb` was validated offline: GLB magic, version 2, declared length equal to
actual length, both chunks present and correctly sized, `asset.version` 2.0, one buffer with no URI
and a `byteLength` that fits inside the BIN chunk, all 11 accessors carrying `componentType`/`count`/
`type` with in-range `bufferView`, meshes with POSITION, no dangling indices, no skins or animations.
No structural defect explains why the loader panics. The JNI entry point was also checked: Filament's
`AutoBuffer` copies heap `ByteBuffer`s, so `ByteBuffer.wrap(bytes)` is legal and not the cause.

### Next step

The panic text wraps past logcat's single-line limit, so the capture now keeps four lines of context
after the exception and after the abort message. That text names the failing precondition and its
location in Filament, which decides the fix: either the asset or configuration we hand to gltfio, or
a defensive pre-validation step that keeps an unloadable asset from killing the app.

## Superseded: the earlier crash observation

The other instrumented class, `com.davemega.hub.AvatarForgeRendererSmokeTest`, does not fail an
assertion — it takes the whole process down:

```
test=generatedAvatarDebugDiagnosticVerifiesImportedGlesPixels
INSTRUMENTATION_STATUS_CODE: 1
INSTRUMENTATION_RESULT: shortMsg=Process crashed.
```

This is the reason `Android CI`'s *AvatarForge GLES GLB render smoke* job has been red. The capture
script now clears logcat before each instrumentation run and, when it sees `Process crashed`, posts
the fatal-signal lines, the abort message and any native backtrace frame naming `libav*`,
`libfilament` or `libgltfio`, plus the newest tombstone (`adb root` works on `google_apis` images).

### Superseded note from the first capture

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
