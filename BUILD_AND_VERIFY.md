# Build and verify renderer fixes

## Changes in this branch

- Filament artifacts are pinned together through the version catalog. The current pin is **1.71.6**.
- Filament camera creation uses a neutral photographic exposure: `setExposure(16.0f, 1.0f / 125.0f, 100.0f)`.
- Filament swap-chain creation is deferred until the `SurfaceView` reports a real, non-zero width and height. The projection and viewport use those actual pixel dimensions.

The exact dependency version line is:

```toml
filament = "1.71.6"
```

The three Filament artifacts resolve from this same version reference:

- `com.google.android.filament:filament-android`
- `com.google.android.filament:filament-utils-android`
- `com.google.android.filament:gltfio-android`

## Emulator verification

Use a clean checkout of the branch/commit under test and an Android emulator with API 35 (the CI smoke configuration):

```bash
./gradlew --stop
./gradlew clean
./gradlew :app:lintDebug :app:testDebugUnitTest :app:assembleDebug --stacktrace
./gradlew :app:connectedDebugAndroidTest --stacktrace
```

Confirm an emulator is visible before running instrumentation:

```bash
adb devices
```

Install and launch the debug APK manually if needed:

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am force-stop com.davemega.hub
adb shell monkey -p com.davemega.hub 1
```

Collect renderer diagnostics:

```bash
adb logcat -c
adb shell am force-stop com.davemega.hub
adb shell monkey -p com.davemega.hub 1
adb logcat -d -s MegaHubRenderer:E MegaHubRenderer:I AndroidRuntime:E
```

For the imported-GLB AvatarForge GLES path, run the instrumentation smoke test and inspect its uploaded screenshot and logcat artifact. A successful GLB parse or non-zero draw-call counter is not sufficient: the screenshot must show the actual model geometry and texture, not just a cleared background. Finally repeat the import and viewer open/close test on the physical Samsung SM-S921B. Emulator success does not prove the phone GPU path.

## Known scope limits, not marked fixed

- **Tangents:** the imported-GLB route currently selects `AvatarForgeSurfaceView` and the custom AvatarForge GLES renderer, not Filament gltfio. The changes above do not generate missing tangent attributes in that native importer. Do not force Unlit or synthesize a tangent basis without a fixture proving the affected material uses a tangent-space normal map. A GLB with and without TANGENT data must be tested through the actual imported-GLB path before marking this fixed.
- **Opaque surface helper:** this project uses a raw Android `SurfaceView`; it does not instantiate Filament's `ModelViewer` or `UiHelper`. There is no `uiHelper.isOpaque` field in this implementation to change. Do not add a fictitious property.
- **DisplayHelper:** this project does not directly create or own a `DisplayHelper`. Filament is upgraded to 1.71.6, but the specific queued display-event race is not independently reproduced here.
- **Threading / teardown:** the custom AvatarForge renderer already owns its native engine, renderer and native-window lifetime on a dedicated render worker. The Filament preview still performs frame callbacks and teardown on the view's callback/UI thread. This patch does not claim to have moved the Filament engine onto a dedicated owner thread or proven ANR-free teardown. That requires a separate serialized Filament-thread lifecycle change and stress verification.
- **Visibility:** a renderer frame or successful GPU upload is not proof of visible model pixels. Verify the captured model image and the user's actual GLB on the target phone.

## Current issue ledger

| Area | Status | Evidence / remaining gate |
|---|---|---|
| Filament exposure configuration | **REAL + VERIFIED (code change)** | Camera setup now uses Filament's photographic `setExposure(16.0f, 1.0f / 125.0f, 100.0f)` API. Emulator render verification is still pending. |
| Filament surface dimensions | **REAL + VERIFIED (code change)** | Swap-chain creation waits for a valid, non-zero surface size and uses actual pixel dimensions. Emulator lifecycle/render verification is still pending. |
| Filament tangent generation / tangent-independent material | **UNVERIFIED** | No runtime tangent generation or proven compatible fallback material has been implemented. Test a GLB that needs tangent-space normal mapping and a no-TANGENT control asset through the Filament gltfio path before selecting the fix. |
| Filament teardown threading and pending callbacks | **UNVERIFIED** | Dedicated engine-owner-thread teardown and callback-drain/cancellation have not been implemented or stress-tested. Verify engine creation, frame callbacks, surface detach, and destruction are serialized on the same owner thread. |
| Native GLES geometry upload/draw diagnostics | **PARTIAL** | Buffer allocation/upload errors are checked, failed uploads are retried, and failed avatar draws are counted as failed rather than successful. Emulator runtime evidence is pending. |
| Native GLES visible geometry / black-screen outcome | **UNVERIFIED** | Indexed bounds refresh and camera ordering are wired, but there is no passing current emulator pixel capture or Samsung SM-S921B visible-pixel evidence yet. |
| Native GLES repeated create/destroy lifecycle | **UNVERIFIED** | The renderer has a dedicated worker and ordered teardown in source; repeated physical-device lifecycle stress has not been run. |

**Path separation:** imported local GLBs currently use `AvatarForgeSurfaceView` and the custom native GLES renderer. The Filament preview is a separate route. A fix or screenshot for one route is not evidence for the other.

## Acceptance evidence

Record the exact commit SHA, Gradle task outcomes, emulator API/device, instrumentation XML, logcat, and screenshot. Mark each result separately:

- Build and lint: pass/fail.
- Unit tests: pass/fail.
- Instrumented renderer smoke: pass/fail.
- Screenshot contains the actual textured model: verified/unverified.
- Physical Samsung SM-S921B import/render and repeated teardown: verified/unverified.

Do not call the renderer fixed or the release green until the required evidence exists.

## Generated-avatar runtime verification

The dedicated instrumentation test generates the avatar directly through the native AvatarForge engine. It does not export/import GLB and does not use the built-in sample model. The same `af_engine*` handle is validated and attached to the GLES renderer.

Exact reproduction input:

- Prompt: `Adult woman, athletic build, 175 cm tall, medium dark hair, black t-shirt and jeans`
- Seed: `1337`
- Expected output: 407,090 triangles, 304,405 vertices, 71 joints, 52 blendshapes and 1.75 m height.

The test treats these as acceptance assertions. It does not rewrite generated counts to match expectations. Prompt parser output is retained in the native diagnostic report.

### Run locally

Use an API 35 emulator with x86_64 support (the native build includes x86_64 and arm64-v8a):

```bash
./gradlew :app:assembleDebug :app:assembleDebugAndroidTest --stacktrace
./gradlew :app:connectedDebugAndroidTest --stacktrace
```

The focused test can be run with:

```bash
./gradlew :app:connectedDebugAndroidTest \
  -Pandroid.testInstrumentationRunnerArguments.class=com.davemega.hub.avatarforge.GeneratedAvatarRuntimeTest \
  --stacktrace
```

### Runtime assertions

- `af_engine_validate_deformed_mesh` checks every post-skin vertex position, all four finite/non-negative weights, weight sum >= 0.99, all four joint references, metric height bounds, every submesh index range and every indexed deformed vertex against its refreshed AABB.
- CPU deformation refreshes each submesh's indexed bounds before the renderer builds the camera.
- GLES updates animation/deformation before camera and frustum calculations.
- The renderer transforms all eight model AABB corners through the actual view and view-projection matrices and records view-space depth against the camera's actual near/far planes.
- A frame is reported presented only after `glFinish()` in verification mode and a successful `eglSwapBuffers()`. Draw-call counts alone are not accepted.
- PixelCopy captures the actual SurfaceView after the successful-presentation result.
- Pixels count as visible when any RGB channel is greater than 10. The required threshold is strictly greater than 5,000 pixels.
- If PixelCopy fails or returns an effectively black image, the test uses the GLES renderer's actual pre-swap default-framebuffer `glReadPixels()` capture. The chosen capture method and both pixel counts are reported separately.

On a failed visibility assertion, the test writes `failed_render.png` to `context.getExternalFilesDir(null)` and includes the absolute path in the assertion message. The CI workflow attempts to collect that file alongside the Android instrumentation reports.

### Physical-device verification

The emulator workflow is not physical-device verification. To verify the target Samsung device, install the debug app and test APK for the matching ABI, then run:

```bash
./gradlew :app:connectedDebugAndroidTest --stacktrace
```

The device must be connected through ADB for this command. Record the device model, Android/API version, renderer backend, actual generated counts, frame-presentation result, PixelCopy result, GL readback count and screenshot path.

Do not mark the generated-avatar issue fixed until the instrumentation test passes and the rendered pixels are visible on the target physical device. An emulator pass is useful evidence but remains separate from handset verification.

