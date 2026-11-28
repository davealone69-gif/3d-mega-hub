# Samsung SM-S921B test of 2026-11-15: what the phone's own reports say

The owner ran the published debug APK on the phone and sent back 13 `renderer_diagnostics (N).json` files and 6 PNG captures from `Downloads/3D Mega Hub Diagnostics`. This file records what those files contain. The files themselves are not in the repository: one capture shows the owner's private reference model.

Status labels: **REAL + VERIFIED**, **UNVERIFIED**, **PARTIAL**, **MISSING**, **BLOCKED**. Everything below is evidence for **this APK on this phone only**. It says nothing about any later commit.

## Which build the reports come from

| Field in the reports | Value |
|---|---|
| Package, version | `com.davemega.hub` 0.1.0 (1), debuggable, not installed from splits |
| APK size and SHA-256 (computed by the app from its own file) | 55,801,294 bytes, `b35ed11d2415d6c09258889032928576928694d29dd668a50c11d6e9ec896fa1` |
| Published APK it equals | pre-release `phone-test-797d156`, source commit `797d1567ab8972a4a6930e12f51ee149c5dc877a` |
| Device | samsung SM-S921B, Android 17 (SDK 37), `arm64-v8a` |
| Graphics | `OpenGL ES 3.2 Driver version: 25.6.4 git hash: 9fe6623ec0db`, renderer string `ANGLE ((Samsung Xclipse 940) on Vulkan 1.3.304)` |

Reports 13 to 21 (2026-11-15 02:42:46Z to 02:43:39Z) carry this identity. Reports 1, 2 and 10 to 12 (2026-11-13) have no build block, so they come from earlier builds and cannot be tied to a commit. Reports 3 to 9, 19 and anything after 21 were not sent.

The app asks Android for OpenGL ES and links no Vulkan library. On this phone Android's own OpenGL ES driver is ANGLE, which runs on top of Vulkan. That is the system's choice, not the app's.

## Generated diagnostic avatar (seed 1337)

| Measurement | Value | Status |
|---|---|---|
| Exported GLB | 83,430,028 bytes, SHA-256 `e6f32bbf707ce7fe56cdae3989fe035fded0d518d9261ce037eaa17f38537fc8`. Same size as the emulator's export of the same spec | **REAL + VERIFIED** |
| Mesh shown | 1,100,168 triangles, 821,215 vertices, 10 submeshes, **0 joints** | **REAL + VERIFIED** |
| Pixel check | `PIXELS_VERIFIED`: 194,401 geometry pixels (minimum 5,000) after 3 presented frames, 29 draw calls, bounds inside the frustum, GL error `0x0`, EGL error `0x0` | **REAL + VERIFIED** |
| Capture | `generated_avatar_diagnostic (4).png`, 912 x 1260. Counting it again with the app's rule gives exactly 194,401 | **REAL + VERIFIED** |
| Start-up | GLB import 1,309 ms, renderer start 240 ms, first frame 2,162 ms | **REAL + VERIFIED** |
| Frames | 1,915 requested, 1,914 presented, 0 not presented, 0 without valid content, 0 stalls, 1 cut off when the surface was released | **REAL + VERIFIED** |
| Frame rate, last 240 frames | 53.8 FPS. Interval median 17.7 ms, p95 24.6 ms, p99 30.6 ms, maximum 42.4 ms | **REAL + VERIFIED** for a 38 s session |
| Quality level | Started at 2 (4x MSAA, 2048 shadow map, 4 bloom levels) and ended at 1 (2x MSAA, 1024, 3). The renderer steps down only after more than 30 consecutive frames slower than 30 FPS, so the phone did not hold 30 FPS at level 2 with this mesh. The frame times at level 2 were not recorded | **PARTIAL** |
| Memory | Native heap 304 to 306 MB. Requested: textures 72.7 MB, geometry 127.9 MB, render targets 84.2 MB, then 44.0 MB after the step down | **REAL + VERIFIED** |
| Camera | Yaw 180.0° to 212.8°, pitch 4.0° to −32.5°, distance unchanged: a one-finger orbit reached the renderer. Pinch zoom and double-tap were not used | **PARTIAL** |
| Heat, battery, sessions longer than 38 s | Not measured | **UNVERIFIED** |

## Imported GLB

| Measurement | Value | Status |
|---|---|---|
| File | 13,221,520 bytes, SHA-256 `656cf2446eefbffc92cf569ce1a2f52b6d0ad449c282fc5a4d911abfef576d14`. Not the 9,287,272-byte `mylove.glb` on record, but the same 163,756 vertices and 144,073 triangles | **REAL + VERIFIED** |
| Mesh shown | 144,073 triangles, 163,756 vertices, 1 submesh, 0 joints | **REAL + VERIFIED** |
| Frames | `FRAME_PRESENTED`: 3 frames, 11 draw calls, bounds inside the frustum, GL and EGL errors `0x0`. Import 312 ms, first frame 828 ms, native heap 134 MB | **REAL + VERIFIED** |
| Pixel check | The capture `imported_glb_diagnostic.png` exists and counts 401,403 geometry pixels. The app writes that file name only when its check passes. The report holding the `PIXELS_VERIFIED` record (it would be number 22) was not sent | **PARTIAL** |

## Defects the reports exposed

| # | What the reports show | Reading | Status |
|---|---|---|---|
| P1 | Report 20, 02:43:39.191Z: `FAILED` for the generated avatar, "Surface was released while the renderer was starting or drawing: … eglCreateWindowSurface failed". Report 18 shows the imported model's session had started 1.5 s earlier, and report 21 shows it presenting frames 0.1 s later | When the app came back from the file picker it restarted the old viewport for 34 ms before switching to the imported file. The old session was cut off while starting and wrote its failure into the shared report after the new session had begun. Nothing crashed, and the imported model rendered. A stale session can overwrite the current session's record, and it re-imported an 83 MB file for nothing | **REAL + VERIFIED** as recorded; the reading is inferred from the timestamps |
| P2 | Final telemetry of the generated session: `glErrorFrames` 1, `lastGlError` `0x502`, `failedDrawCalls` 0, together with `framesCutOffByShutdown` 1 and `shutdownEglError` `0x300d` | One GL error in 1,915 frames. No avatar draw call failed, so the error came from the final passes that write to the window. The cut-off frame is the likely place, but the telemetry does not record which frame raised it | **UNVERIFIED** cause |
| P3 | 13 numbered report files and 5 numbered captures | After a reinstall the app no longer owns its earlier files in Downloads, cannot find them by name and creates a new numbered file for every status change | **REAL + VERIFIED** |
| P4 | Captures of 2026-11-13 (earlier builds) show moiré bands on the clothing; the capture from this APK does not | Those captures cannot be tied to a build. Not reproduced with this APK | **UNVERIFIED** |

## What the captures show about the avatar

`generated_avatar_diagnostic (4).png`, from this APK, front view of the seed-1337 avatar:

- The hair is a black cap that covers the forehead and both eyes. Black blocks stand beside the head.
- The shoulders are spheres. The hands are mittens with two prongs. The feet are bulbs with thin toe spikes.
- The neckline, hem and waistband are ragged, with skin showing through between the garments.
- Small dark specks cover skin and cloth.

This is the generator's output, shown correctly by the renderer: the same spec produces a file of the same size on the emulator. The owner's verdict of 2026-11-15, "appearance still not fixed", matches the captures. Avatar appearance: **PARTIAL**, not acceptable.

## What this settles and what it does not

| Question | Answer |
|---|---|
| Does the published APK install, start, generate, import and draw with OpenGL ES on the SM-S921B? | Yes. **REAL + VERIFIED** for APK `b35ed11d…` |
| Do the runtime diagnostics work on the phone? | Yes, with defects P1 to P3. **PARTIAL** |
| Any crash or ANR? | None in these reports. The "Previous app exits" reading from Settings was not sent. **PARTIAL** |
| Is the avatar animated? | No. Both models report 0 joints. **MISSING** in this APK |
| Is the avatar's appearance acceptable? | No. **PARTIAL** |
