# 3D Mega Hub: build and verification

Current status and results live in [README.md](README.md). This file describes how the app is built, what each automated gate proves, and how to verify a build on the phone.

## Render routes

| Content | Renderer | Notes |
|---|---|---|
| Imported and generated GLBs | `AvatarForgeSurfaceView` and the native AvatarForge OpenGL ES renderer | OpenGL ES is forced. The Vulkan backend is compiled but never selected. |
| Bundled sample `female.glb` | `FilamentPreviewView` (Filament core and gltfio) | OpenGL ES first. Filament tries Vulkan only if its OpenGL ES start-up fails. |

Filament is initialised with `Filament.init()` and `Gltfio.init()` on the main thread before any Filament object is created. `filament-utils-android` is not a dependency.

## Renderer session rules

One render thread owns the EGL context, the engine, the renderer and the `ANativeWindow` for the life of a surface.

- A **valid frame** is one that was presented by `eglSwapBuffers()` and drew in-frustum model geometry (draw calls and triangles above zero).
- The session **fails** only if frames come back for 15 seconds without a single valid frame. The failure text, with the frame counters and GL/EGL error codes, stays on screen as the final status.
- A session that presents valid frames slowly keeps running and reports its measured numbers.
- If a running session stops producing valid frames for 15 seconds it reports that and keeps retrying.
- The UI thread never waits for the render thread. When a screen closes, Android can take the Surface away while a frame is still being drawn; that frame is counted as "cut off by surface shutdown" with its EGL code, not as an error. A failed swap while the session is meant to be rendering is an error.
- Visible-pixel verification (debug builds) needs three presented frames. The render thread then finishes the frame's GPU work, asks PixelCopy for the real SurfaceView, and holds the next frame until the copy is answered. The background sampled at the four corners is subtracted and at least 5,000 geometry pixels are required.
- Imported GLBs are shown as static meshes. The importer does not apply a file's skeleton or animation clips and does not read its morph targets, so the on-screen joint count is zero. Posing and facial animation need the engine's own rig, which exists only inside the engine session that generated the avatar. The app exports a generated avatar and then imports it, so generated avatars are also shown static.

## Measured diagnostics

The numbers under the Avatar viewport are measured on the device about once per second.

| Number | How it is measured |
|---|---|
| Frame rate, frame-interval p50/p95/p99/max | `System.nanoTime()` between successful presents, over the last 240 frames |
| Render-call p50/p95/max | `System.nanoTime()` around the native render call |
| Last frame CPU and swap time | `steady_clock` inside the renderer, split at `eglSwapBuffers()` |
| Draw calls, triangles, vertices | The renderer's counters for the frame |
| GLB import, renderer init, first frame | `steady_clock` around each native step |
| GPU bytes for textures, geometry and render targets | Sizes the renderer requested from the driver. Not physical GPU memory. |
| Native and Java heap | `Debug.getNativeHeapAllocatedSize()` and `Runtime` |
| GL-error frames, EGL swap failures, failed draws | `glGetError()` drained every frame; `eglSwapBuffers()` results |
| Frames cut off by surface shutdown | A swap that failed after Android had asked the session to stop; reported separately from errors |
| Native sessions created and destroyed | Counters in the JNI layer |

A value that has not been measured is shown as "not measured", never as zero time.

Files written to `Downloads/3D Mega Hub Diagnostics`:

- `renderer_diagnostics.json` at each renderer checkpoint, including the latest measured snapshot.
- `renderer_telemetry.json` with the final measured numbers when a viewport closes.
- `imported_glb_diagnostic.png`, `generated_avatar_diagnostic.png` or `failed_render.png` from the PixelCopy check (debug builds).

**Settings → Previous app exits** shows Android's own record of crashes, native crashes and ANRs from earlier runs (Android 11 and newer).

## Automated gates

A gate proves only what it ran on. An emulator pass is not a phone pass.

| Gate | Workflow | What it proves |
|---|---|---|
| Native regression (CTest) | `android.yml`, `avatarforge-diagnostic-apk.yml` | GLB parsing, WebP handling, camera frustum, LOD, visibility and PBR import on a host build |
| Unit tests, lint, debug APK, R8 | `android.yml` | The app compiles, unit logic passes, lint is clean, the release minifies |
| Imported-GLB render smoke | `android.yml` | The real app renders an imported GLB and a WebP-textured GLB through native OpenGL ES on an emulator; touch drags move the camera and change the picture; no GL, EGL or draw errors; native sessions are released when the screen closes |
| Generated avatar, direct | `avatarforge-diagnostic-apk.yml`, `avatarforge-generated-runtime.yml` | Native generation, mesh validation, OpenGL ES draw and pixel readback |
| Generated avatar through the production surface | same | Generate, export, import and render full-screen through `AvatarForgeSurfaceView`, with PixelCopy proof |
| Signed release preview | `release-preview.yml` | Package name, versions, SDK levels, ABI set, required libraries and both signatures are read back from the APK and AAB and compared with the Gradle configuration. A second clean build, made without the Gradle build cache, must be byte-identical for both files; when it is not, the gate fails and names the part of the file that differs |
| Emulator launch, debug and signed release | `release-preview.yml` | Install, launch, Filament sample loads and presents a frame, no crash or ANR. The signed, minified release APK is the same file whose hash was verified |

Each emulator job prints the measured renderer evidence in its log and uploads logcat, the diagnostics files and a SHA-256 manifest.

## Local commands

```bash
# Native engine on a host machine (CMake 3.22+, a C++17 compiler, zlib)
cmake -S avatarforge-engine/src/main/cpp -B build/host -DBUILD_TESTING=ON
cmake --build build/host --parallel 2
ctest --test-dir build/host --output-on-failure

# Android (JDK 21, Android SDK 35, NDK 26.1.10909125, CMake 3.22.1)
./gradlew :app:testDebugUnitTest :app:lintDebug :app:assembleDebug
./gradlew :app:connectedDebugAndroidTest        # needs a device or emulator

# Signed release (needs your keystore; never commit it)
ANDROID_RELEASE_STORE_FILE=/path/to/keystore.jks \
ANDROID_RELEASE_STORE_PASSWORD=... ANDROID_RELEASE_KEY_ALIAS=... ANDROID_RELEASE_KEY_PASSWORD=... \
./gradlew :app:assembleRelease :app:bundleRelease
bash .github/scripts/verify-release.sh verify \
  app/build/outputs/apk/release/app-release.apk \
  app/build/outputs/bundle/release/app-release.aab release-check
```

A release build without all four signing values stops with a `BLOCKED` message. No unsigned release is produced.

## Phone gate: Samsung SM-S921B

Only the owner can run this. Record the SHA-256 of the APK that was installed.

1. Install the APK. If Android reports a signature conflict, uninstall the previous build first.
2. Open **Avatar**. The bundled sample should appear in the viewport.
3. Tap **Import GLB**, choose a GLB, and wait for the viewport to show it.
4. Read **Measured renderer diagnostics** under the viewport after about ten seconds. Note the frame rate, the p95 frame interval, GLB import time, the GPU byte figures and the error line. The error line should read zero for all three counters. "1 cut off by surface shutdown" in the saved file is normal.
5. Drag to orbit, pinch to zoom and double-tap to reset. The camera line should change and the frame counter should keep rising.
6. Tap **Generate avatar**, wait for it to finish, and repeat steps 4 and 5.
7. Leave the Avatar tab and return, rotate the phone, and send the app to the background and back, three times each. The "Native sessions" line should show created and destroyed staying one apart while the viewport is open.
8. Open **Settings** and read **Previous app exits**. It should report no crashes, native crashes or ANRs.
9. Copy `renderer_telemetry.json` and `renderer_diagnostics.json` from `Downloads/3D Mega Hub Diagnostics`.

Until those files and the APK hash are supplied, phone rendering, performance and lifecycle stay **UNVERIFIED**.
