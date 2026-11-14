# 3D Mega Hub: build and verification

Current status and results live in [README.md](README.md). This file describes how the app is built, what each automated gate proves, and how to verify a build on the phone.

## Render routes

| Content | Renderer | Notes |
|---|---|---|
| Imported and generated GLBs | `AvatarForgeSurfaceView` and the native AvatarForge OpenGL ES renderer | OpenGL ES only. The Vulkan backend source stays in the tree but is not compiled into `libafrender.so`, and the library does not link `libvulkan.so` (CMake option `AF_ENABLE_VULKAN`, off). The release gate reads the library's dependencies back from the APK. |
| Bundled sample `female.glb` | `FilamentPreviewView` (Filament core and gltfio) | OpenGL ES first. Filament tries its own Vulkan backend only if its OpenGL ES start-up fails. |

Filament is initialised with `Filament.init()` and `Gltfio.init()` on the main thread before any Filament object is created. `filament-utils-android` is not a dependency.

## Renderer session rules

One render thread owns the EGL context, the engine, the renderer and the `ANativeWindow` for the life of a surface.

- A **valid frame** is one that was presented by `eglSwapBuffers()` and drew in-frustum model geometry (draw calls and triangles above zero).
- The session **fails** only if frames come back for 15 seconds without a single valid frame. The failure text, with the frame counters and GL/EGL error codes, stays on screen as the final status.
- A session that presents valid frames slowly keeps running and reports its measured numbers.
- If a running session stops producing valid frames for 15 seconds it reports that and keeps retrying.
- A C++ exception in the renderer does not abort the app. The JNI entry points that create a session, draw a frame or resize turn it into a Java exception, which the render thread reports as the renderer failure before it tears the session down.
- A renderer failure and a teardown failure are reported together in the final status. Neither replaces the other.
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

Below the measured numbers the panel shows three identity lines: the app version, build type, phone model and Android version; the SHA-256 of the installed APK; and the SHA-256 of the GLB on screen. An APK installed directly from its file is stored unchanged, so the APK line equals the published SHA-256 of that file. For an install from an app bundle the line says that the value is not comparable.

Files written to `Downloads/3D Mega Hub Diagnostics`:

- `renderer_diagnostics.json` at each renderer checkpoint, including the latest measured snapshot.
- `renderer_telemetry.json` with the final measured numbers when a viewport closes.
- `imported_glb_diagnostic.png`, `generated_avatar_diagnostic.png` or `failed_render.png` from the PixelCopy check (debug builds).

Both JSON files carry a `build` block (application ID, version, debuggable flag, SHA-256 and size of the installed APK, split-install flag, device manufacturer and model, Android release and API level, ABI) and `assetSha256`. A saved report therefore names the APK, the phone and the file it measured.

**Settings → Previous app exits** shows Android's own record of crashes, native crashes and ANRs from earlier runs (Android 11 and newer).

## Automated gates

A gate proves only what it ran on. An emulator pass is not a phone pass.

| Gate | Workflow | What it proves |
|---|---|---|
| Native regression (CTest) | `android.yml` (all seven tests), `avatarforge-diagnostic-apk.yml` (six, without the sampling test) | GLB parsing, WebP handling, camera frustum, LOD, visibility and PBR import on a host build |
| Importer robustness (CTest `glb_import_robustness`) | same | 8,000 deterministic mutations of a valid GLB (truncation, wrong lengths, changed numbers, flipped bytes, swapped tokens) are each rejected with an error or accepted; the test fails on a crash, and on an accepted file whose index or submesh range lies outside its buffers |
| Unit tests, lint, debug APK, R8 | `android.yml` | The app compiles, unit logic passes, lint is clean, the release minifies |
| Imported-GLB render smoke | `android.yml` | The real app renders an imported GLB and a WebP-textured GLB through native OpenGL ES on an emulator; touch drags move the camera and change the picture; no GL, EGL or draw errors; native sessions are released when the screen closes |
| Generated avatar, direct | `avatarforge-diagnostic-apk.yml`, `avatarforge-generated-runtime.yml` | Native generation, mesh validation, OpenGL ES draw and pixel readback |
| Generated avatar through the production surface | same | Generate, export, import and render full-screen through `AvatarForgeSurfaceView`, with PixelCopy proof |
| Native exception barrier | `avatarforge-generated-runtime.yml` | Real C++ exceptions thrown inside the JNI barrier arrive as Java exceptions with the right text, and native code keeps running afterwards |
| Debug APK inspection | `avatarforge-diagnostic-apk.yml` | The debug APK that is published for the phone test is read back before the emulator tests run: application ID, version, SDK levels, debuggable flag, ABIs, library set, library dependencies, signature and signing certificate. A mismatch fails the job. The log prints `APK_INSPECTION`, and `APK_CHAIN` shows that the inspected file and the uploaded file are the same |
| Report identity | every emulator job that renders | The job log prints the identity block from the saved report and compares the APK SHA-256 the app reports for itself with the APK file that was installed. Printed as evidence, not enforced |
| Signed release preview | `release-preview.yml` | Package name, versions, SDK levels, ABI set, required libraries and both signatures are read back from the APK and AAB and compared with the Gradle configuration. A second clean build, made without the Gradle build cache, must be byte-identical for both files; when it is not, the gate fails and names the part of the file that differs |
| Emulator launch, debug and signed release | `release-preview.yml` | Install, launch, Filament sample loads and presents a frame, no crash or ANR. The signed, minified release APK is the same file whose hash was verified |

The signed-preview gate also reads the dependencies of every packaged native library back from the APK with `readelf`. It fails if `libafrender.so`, `libafcore.so` or `libmega_hub_avatarforge.so` links `libvulkan.so`, or if `libafrender.so` does not link `libGLESv3.so`.

Each emulator job prints the measured renderer evidence in its log and uploads logcat, the diagnostics files and a SHA-256 manifest.

## Evidence rules

These are the owner's rules for using diagnostics as evidence. They are kept as written.

**A passing diagnostic is reusable evidence for the exact APK, asset and device state it measured. It is not a reason to rebuild repeatedly.**

- If source code, build configuration, native libraries or packaged assets have not changed, do not rebuild just to repeat a passing diagnostic. Reuse the report and only rerun a diagnostic when the asset, device state, renderer lifecycle or suspected failure condition changes.
- After a coherent batch of source changes, perform one consolidated validation cycle against the exact resulting commit: native/unit tests, Android build/lint/R8, relevant renderer instrumentation, emulator install/launch and artifact capture. Do not build between individual edits in the same batch.
- A successful build proves compilation/package assembly only. A renderer diagnostic proves only the checks it actually performs. The diagnostic must identify its APK/source commit, asset hash, device/API/backend, capture method, thresholds and result before it can be reused as evidence.
- A previous APK's passing diagnostics do not validate changed source code. A newly built APK still needs one install/runtime validation on the target device before that new APK is called phone-verified.
- A generated-avatar diagnostic must not be used as a substitute for imported-GLB verification. Imported GLBs must independently pass actual surface capture and visible-pixel checks.
- FRAME_PRESENTED means EGL presented frames; it does not mean visible geometry was verified. PIXELS_VERIFIED requires a successful PixelCopy capture and at least 5,000 foreground geometry pixels after background subtraction. FAILED, capture timeout, missing screenshot or missing report must remain failures, not be silently converted into passes.
- In debug builds, imported GLBs use the same PixelCopy visible-pixel gate as the generated diagnostic. A zero count from a report without pixel verification enabled is NOT MEASURED, not proof of an invisible model.
- Preserve failure artifacts and diagnose them before another code change. Do not rerun an unchanged failing test repeatedly unless the next run changes one defined variable or captures missing evidence.

| Result in a report | What it proves | What it does not prove |
|---|---|---|
| STARTING | Renderer worker began | EGL or model loading |
| RENDERER_ACTIVE | Native renderer session and GLES context initialized | Geometry drawn or visible |
| FRAME_PRESENTED | Required EGL swaps, draw calls, triangles and frustum checks observed | Pixels reached the displayed surface |
| PIXELS_VERIFIED | PixelCopy captured foreground geometry above threshold | Model quality, correct appearance controls, animation or long-session stability |
| FAILED | A required diagnostic stage failed | Root cause, until failure details are inspected |

A test APK counts as ready for the phone only when all five conditions hold for one commit:

1. The production generated-GLB-to-GLES `SurfaceView` instrumentation test passes on that commit.
2. Every required Android build job passes on that commit.
3. A fresh APK artifact comes from a workflow run of that commit.
4. The APK itself is verified: application ID, signing certificate and SHA-256, with the artifact URL recorded.
5. The run's commit, the test result, the artifact and the APK inspection all point at the same commit.

A commit that changes only documentation does not change the APK's source. The README names the commit that the evidence belongs to, and `git diff --stat <that commit>..HEAD` shows that only documents changed after it.

Each CI job builds its own debug APK and signs it with a debug key created on that runner, so debug APKs from different jobs have different SHA-256 values even for the same commit. Only the artifact named in the README matches the SHA-256 published there.

## Local commands

```bash
# Native engine on a host machine (CMake 3.22+, a C++17 compiler, zlib)
cmake -S avatarforge-engine/src/main/cpp -B build/host -DBUILD_TESTING=ON
cmake --build build/host --parallel 2
ctest --test-dir build/host --output-on-failure

# The same tests under AddressSanitizer and UndefinedBehaviorSanitizer, then a longer robustness run
SAN="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined"
cmake -S avatarforge-engine/src/main/cpp -B build/asan -DBUILD_TESTING=ON -DAF_OPT_FLAGS=-O1 \
  -DCMAKE_CXX_FLAGS="$SAN" -DCMAKE_SHARED_LINKER_FLAGS="$SAN" -DCMAKE_EXE_LINKER_FLAGS="$SAN"
cmake --build build/asan --parallel 2
ASAN_OPTIONS=detect_leaks=1:allocator_may_return_null=1 \
  build/asan/af_glb_import_robustness_test app/src/main/assets/models/female.glb 30000 1337

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

Only the owner can run this. Use the debug APK named under "Artifacts" in the [README](README.md): it is the file that passed the emulator gates, and its SHA-256, application ID and signing certificate are recorded there. The debug build is the one that carries the on-screen diagnostics and the visible-pixel check.

1. Install the APK. Each CI debug APK is signed with its own throwaway key, so Android refuses to install it over an earlier build: uninstall the earlier build first.
2. Open **Avatar**. The bundled sample should appear in the viewport.
3. Tap **Import GLB**, choose a GLB, and wait for the viewport to show it.
4. Check the line **Installed APK SHA-256** under the measured numbers against the SHA-256 published for the APK you downloaded. If they differ, stop: a different build is installed.
5. Read **Measured renderer diagnostics** under the viewport after about ten seconds. Note the frame rate, the p95 frame interval, GLB import time, the GPU byte figures and the error line. The error line should read zero for all three counters. "1 cut off by surface shutdown" in the saved file is normal.
6. Drag to orbit, pinch to zoom and double-tap to reset. The camera line should change and the frame counter should keep rising.
7. Tap **Generate avatar**, wait for it to finish, and repeat steps 5 and 6.
8. Leave the Avatar tab and return, rotate the phone, and send the app to the background and back, three times each. The "Native sessions" line should show created and destroyed staying one apart while the viewport is open.
9. Open **Settings** and read **Previous app exits**. It should report no crashes, native crashes or ANRs.
10. Copy `renderer_telemetry.json` and `renderer_diagnostics.json` from `Downloads/3D Mega Hub Diagnostics`.

The two files name the APK, the phone and the asset they measured, so they are the evidence. Until they are supplied, phone rendering, performance and lifecycle stay **UNVERIFIED**.
