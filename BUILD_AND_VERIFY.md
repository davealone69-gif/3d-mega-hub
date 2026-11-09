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

## Acceptance evidence

Record the exact commit SHA, Gradle task outcomes, emulator API/device, instrumentation XML, logcat, and screenshot. Mark each result separately:

- Build and lint: pass/fail.
- Unit tests: pass/fail.
- Instrumented renderer smoke: pass/fail.
- Screenshot contains the actual textured model: verified/unverified.
- Physical Samsung SM-S921B import/render and repeated teardown: verified/unverified.

Do not call the renderer fixed or the release green until the required evidence exists.
