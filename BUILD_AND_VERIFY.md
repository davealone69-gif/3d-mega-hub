# 3D Mega Hub Android renderer build and verification

## Active renderer routes

- **Bundled sample preview:** remains on the separate Filament preview route.
- **Imported and generated GLBs:** use `AvatarForgeSurfaceView` and the integrated native AvatarForge GLES renderer.
- **Backend policy:** the imported/generated route explicitly selects GLES. Vulkan is not selected for the phone-first path.
- **Initial skinned pose:** imported skinned GLBs apply `AF_CLIP_REST` before submesh bounds are read for camera target/distance. Static unskinned GLBs skip the pose call.
- **Frame ordering:** CPU animation/deformation runs before scene bounds and frustum updates. Submesh AABBs are refreshed from the actual indexed, deformed vertices.

## Generated-avatar validation

The native generator validates the live post-skinning mesh before GLB export. Validation covers finite positions, all four finite/non-negative skin weights, weight sum, all four joint references, metric height, submesh index ranges, vertex spans, and containment of every indexed deformed vertex by its refreshed AABB. Invalid geometry fails generation; it is not exported as a successful avatar.

Debug builds expose **Run generated-avatar GLES diagnostic** in Avatar Studio. It:

1. Generates the deterministic seed-1337 avatar.
2. Runs native post-skinning mesh and indexed-AABB validation before export.
3. Loads the exported GLB through the same imported-GLB GLES route used by the app.
4. Requires at least three successful `eglSwapBuffers()` presentations, non-zero avatar triangles, and bounds inside the camera frustum.
5. Uses PixelCopy to capture the actual SurfaceView. It estimates the clear colour from the four corners and counts only geometry pixels whose RGB delta sum is at least 48 and maximum channel delta is at least 16. The dark blue-grey clear colour cannot pass as avatar geometry.
6. Requires more than 5,000 geometry pixels. On success it saves `generated_avatar_diagnostic.png`; on a low-pixel result it saves `failed_render.png`. The absolute path and pixel count appear in the on-screen renderer status.

PixelCopy requests are attempt-scoped, invalidated on surface destruction, protected against duplicate/late callbacks, and fail closed after a 15-second callback timeout. The instrumentation test also has a native GLES `glReadPixels()` fallback and applies the same background-subtracted pixel count.

## CI and local verification

The GitHub PR is the authoritative current-head verification ledger. A previous passing build does not prove a newer commit passed. The required gates are:

- Host CTest: GLB extension parsing, WebP required-extension handling, and optional WebP core-source fallback.
- Android debug build, unit tests, lint, and R8/minification.
- Imported-GLB GLES instrumentation: successful EGL swaps, in-frustum bounds, draw/triangle counts, and captured viewport geometry.
- Generated-avatar direct native runtime test: deterministic seed, mesh validation, GLES draw, PixelCopy/readback and visible-pixel threshold.
- In-app generated-avatar diagnostic instrumentation: click the debug button and verify the generated GLB through the actual app import route.
- Emulator install/launch smoke and signed release-preview APK/AAB build.

Local commands:

```bash
./gradlew clean :app:lintDebug :app:testDebugUnitTest :app:assembleDebug
./gradlew :app:connectedDebugAndroidTest
```

A production release APK/AAB still requires the user's own release keystore and signing credentials. CI preview signing is not a production signing identity.

## Physical-device gate

The target phone is Samsung SM-S921B / Xclipse 940. Emulator success is not phone verification. On the phone:

1. Build/install the current branch's debug APK using the intended local debug signing identity.
2. Open Avatar Studio and tap **Run generated-avatar GLES diagnostic**.
3. Require the in-app status `REAL + VERIFIED`, more than 5,000 background-subtracted geometry pixels, at least three successful EGL swaps, in-frustum bounds, zero reported GL/EGL errors, and a saved screenshot path.
4. Import a WebP-textured GLB and verify its texture pixels.
5. Repeat open/close, background/resume, resize/rotation and repeated import cycles. Capture symbolized native failures if any occur.

Until this phone test passes, Samsung visual rendering and lifecycle status remains **UNVERIFIED**. A successful source edit, CI build, emulator run, or non-zero draw-call count alone is not a green build.

## Historical Filament notes

Filament exposure, swap-chain sizing, and preview lifecycle work apply only to the bundled Filament preview route. They do not verify or repair the imported/generated AvatarForge GLES path. Do not use a Filament preview frame as evidence that the native GLES renderer works.
