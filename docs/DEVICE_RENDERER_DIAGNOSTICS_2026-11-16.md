# Device renderer diagnostics — 2026-11-16

**Source:** owner-supplied Google Drive folder `Di` (`1ZXxeMKSb3CmQa-VOC_9wDEOMPjmUqEY_`), 19 files: 15 `renderer_diagnostics*.json` snapshots and 4 `imported_glb_diagnostic*.png` captures. Folder modified 2026-11-16 15:31 UTC; the reports themselves are stamped 15:13–15:20 UTC.

**Provenance:** every JSON is written by the app itself, from `AvatarForgeSurfaceView.recordSmokeResult()` (`schema = com.davemega.hub.renderer-diagnostics.v1`) into `Downloads/3D Mega Hub Diagnostics/renderer_diagnostics.json`. The app overwrites one `renderer_diagnostics.json` per state change, so the 15 snapshots are successive states of **four render sessions**, exported/copied out of Downloads while the session progressed (hence the `(1)`…`(15)` name suffixes).

**Evidence classification:** DEVICE-REPORTED. These are the app's own measurements on a physical Android device, read back from the exported artifacts. They are **not** bound to a source commit or APK hash (none of the reports records one), and they are **not** an exact-head CI result. Read them as device runtime evidence for the production `AvatarForgeSurfaceView` OpenGL ES path, nothing wider.

---

## Session A — three runs, imported file-based GLB

Asset: `imported-1794842029545-b69f92bf.glb`, 13,221,520 bytes (12.6 MiB) — the name pattern is `importAvatar()`'s document-picker import (`imported-<millis>-<uuid8>.glb`), i.e. one asset imported once and re-rendered three times.
Backend: `OpenGL ES` / `OpenGL ES 3.2 Driver version: 25.6.4 git hash: 9fe6623ec0db`, surface 912 × 1260.

| Run | `STARTING` | `RENDERER_ACTIVE` | `FRAME_PRESENTED` | `PIXELS_VERIFIED` | Visible pixels |
|---|---|---|---|---|---|
| 1 | 15:13:53.742 | 15:13:54.519 | 15:13:55.347 | 15:13:55.971 | 401,066 |
| 2 | 15:14:53.756 | 15:14:54.785 | 15:14:55.675 | 15:14:56.408 | 401,066 |
| 3 | 15:16:09.461 | 15:16:12.798 | 15:16:14.144 | 15:16:15.232 | 401,062 |

Identical in all three runs at `FRAME_PRESENTED`/`PIXELS_VERIFIED`:

- `frames` 3, `presentedFrames` 3, `drawCalls` 11, `triangles` 144,073
- `boundsInFrustum` true, `outsideCorners` 0
- `glError` `0x0`, `eglError` `0x0`
- `visiblePixels` 401,066 / 401,066 / 401,062 against `minimumVisiblePixels` 5,000 — about 34.9 % of the 1,149,120-pixel surface differs from the corner-sampled clear colour
- `screenshotPathAppPrivate` `/storage/emulated/0/Android/data/com.davemega.hub/files/imported_glb_diagnostic.png`
- Downloads exports `content://media/external/downloads/1000202982`, `…1000202987`, `…1000202992`

Matching PNG captures are present (`imported_glb_diagnostic (1).png` 723,851 B, `imported_glb_diagnostic (2).png` 723,660 B, `imported_glb_diagnostic (3).png` 724,018 B), plus one 0-byte duplicate upload named `imported_glb_diagnostic (2).png` that carries no evidence.

## Session B — one run, large remote-URL GLB, incomplete

Asset: `ink-1794842238556-077e8cf1.glb`, 103,566,844 bytes (98.8 MiB) — the `<stem>-<millis>-<uuid8>.glb` pattern is `importAvatarUrl()`'s remote import, so the owner loaded a file named `ink.glb` from a URL.

| State | Timestamp | Δ from `STARTING` | Key fields |
|---|---|---|---|
| `STARTING` | 15:19:48.834 | — | all counters zero |
| `RENDERER_ACTIVE` | 15:20:02.723 | +13.889 s | GLES 3.2 reported |
| `FRAME_PRESENTED` | 15:20:04.643 | +15.809 s | frames 3, draws 59, triangles 872, bounds in frustum, `glError` `0x0`, `eglError` `0x0` |
| `PIXELS_VERIFIED` / `FAILED` | **absent** | — | no terminal state, no screenshot path |

Two defects are visible in this report:

1. **No terminal state.** The session stops at `FRAME_PRESENTED`. A reader cannot tell whether the owner left the screen, whether the process was killed, or whether the 15 s `PixelCopy` timeout had not yet fired when the report was exported.
2. **Inherited screenshot identity.** Its `screenshotDownloadsUri` is `content://media/external/downloads/1000202987` — the capture belonging to *Session A run 2*, not to this asset. `recordSmokeResult()` read the key straight out of shared preferences, which are never cleared between assets, so a report for an asset that was never pixel-verified advertises another model's screenshot.

Item 2 is fixed in this change: `recordSmokeResult("STARTING")` now clears the inherited screenshot and pixel-copy keys before the new session's first report is written.

`103,566,844` bytes is inside `GlbValidator.MAX_GLB_BYTES` (128 MiB), so this file is currently accepted and rendered. 59 draw calls against 872 triangles means the file's bulk is not triangle geometry — it is textures, attributes or a large number of small primitives. That cannot be checked from the report alone; the GLB itself was not part of this handover.

---

## What these artifacts change

**Superseded:** the standing statement that "real imported/generated avatar visible pixels … on the phone" are UNVERIFIED. For one 12.6 MiB imported GLB, the production `AvatarForgeSurfaceView` OpenGL ES path is now confirmed **three times** on a physical device with `PIXELS_VERIFIED`, zero GL/EGL errors, in-frustum bounds and a saved capture.

**Still standing:** the exact-head CI failures. Nothing here exercises the Filament gltfio file-preview path, so the CI `OutOfMemoryError` in `FilamentPreviewView.readBounded()` (134,217,744-byte allocation) — the emulator's failing route — is untouched by this evidence. The successful emulator install/launch smoke and the signed preview APK remain non-substitutes.

## Device vs emulator, same production gate

| Signal | Device (Session A) | Emulator CI at `f73eec156b1baf6d105b4e891d8db45f22e66f3f` |
|---|---|---|
| Asset | imported GLB, 12.6 MiB | generated seed-1337 GLB |
| Triangles reported | 144,073 | 1,626,176 |
| Draw calls | 11 | 29 |
| Presented frames inside the 15 s budget | 3 at +1.6 s (run 1) | 2 at +15 s → `FAILED` |
| Terminal state | `PIXELS_VERIFIED` | `FAILED` |

This is **not** a like-for-like comparison: the emulator was asked to software-rasterise a model with 11× the triangles while the production gate insists on three presented swaps plus a `PixelCopy` inside 15 s. It shows the production path works on hardware; it does not show the emulator gate can pass.

## Timing risk the artifacts expose

`AvatarForgeSurfaceView` starts its 15-second `FRAME_PRESENTED` deadline before `nativeCreate()` and therefore includes asset parsing, GPU upload and texture decode in the budget:

- 12.6 MiB asset (run 1): `STARTING` → `RENDERER_ACTIVE` **0.78 s**, `RENDERER_ACTIVE` → `FRAME_PRESENTED` **0.83 s**
- 98.8 MiB asset: `STARTING` → `RENDERER_ACTIVE` **13.89 s**, `RENDERER_ACTIVE` → `FRAME_PRESENTED` **1.92 s**

**The 15 s gate is not the problem here.** `frameWaitStarted` is taken after `nativeCreate()` returns (`AvatarForgeSurfaceView.kt:231`, deadline checked at `:281`), so Session B's 1.92 s of render-loop time cleared it comfortably. What the artifacts show instead is that **asset load is unmeasured and unbounded**: nearly 14 s of GLB parse, GPU upload and texture decode ran on the render thread outside any deadline, with no progress reporting to the user. On a slower device, a cold page cache, or a larger asset that still passes the 128 MiB import cap, that phase is where the session will appear to hang — and nothing in the diagnostics distinguishes it from a genuine renderer failure, because `STARTING` is the only state recorded before it completes.

## Remaining gaps

- Device build identity: no APK version, source commit or APK SHA-256 is recorded in any artifact. These results must not be attributed to the current branch head.
- Session B pixel verification and capture: missing.
- Filament gltfio preview on the device for these assets: unexercised.
- Lifecycle, memory pressure, thermal behaviour and repeat-import behaviour for ~100 MiB assets: unmeasured.
- Exact-head CI: still red; no green run exists at any head quoted in this document.

## Recommended next steps

1. Bind device evidence to source: have the owner record `versionName`, the installed APK SHA-256, or a build commit, and add it to a report field so this class of handover can be attributed.
2. Re-run the diagnostic on the `ink.glb` asset with the app kept in the foreground until `PIXELS_VERIFIED` or `FAILED` appears, and capture `logcat -s AvatarForge` alongside it, so an incomplete session can be distinguished from a stalled `PixelCopy`.
3. Bound and instrument the load phase, which no gate currently covers: record a distinct `ASSET_LOADING` state with the asset byte count and elapsed time before `nativeCreate()`, and give load its own explicit budget so an oversized asset fails with a clear resource message instead of an apparent hang. Leave the 15 s presentation requirement as it is.
4. Reduce peak memory in `FilamentPreviewView.readBounded()`: the asset is accumulated in a growing `ByteArrayOutputStream` and then copied again by `toByteArray()`, so peak is roughly twice the file size before Filament's own copy. Size a single buffer from `File.length()` where known and stream into the asset buffer.
5. Decide the CI gate's environment policy explicitly: either give the emulator a deadline that reflects software rasterisation of a 1.63 M-triangle asset, or move the production `AvatarForgeSurfaceView` gate to a hardware runner. Do not weaken the assertions themselves.

## Artifact index

| File | Bytes | State captured |
|---|---|---|
| `renderer_diagnostics (1).json` | 621 | A1 `STARTING` |
| `renderer_diagnostics (2).json` | 687 | A1 `RENDERER_ACTIVE` |
| `renderer_diagnostics (3).json` | 692 | A1 `FRAME_PRESENTED` |
| `renderer_diagnostics (4).json` | 870 | A1 `PIXELS_VERIFIED` (401,066) |
| `renderer_diagnostics (5).json` | 703 | A2 `STARTING` |
| `renderer_diagnostics (6).json` | 769 | A2 `RENDERER_ACTIVE` |
| `renderer_diagnostics (7).json` | 774 | A2 `FRAME_PRESENTED` |
| `renderer_diagnostics (8).json` | 870 | A2 `PIXELS_VERIFIED` (401,066) |
| `renderer_diagnostics (9).json` | 703 | A3 `STARTING` |
| `renderer_diagnostics (10).json` | 769 | A3 `RENDERER_ACTIVE` |
| `renderer_diagnostics (11).json` | 774 | A3 `FRAME_PRESENTED` |
| `renderer_diagnostics (12).json` | 870 | A3 `PIXELS_VERIFIED` (401,062) |
| `renderer_diagnostics (13).json` | 617 | B `STARTING` |
| `renderer_diagnostics (14).json` | 683 | B `RENDERER_ACTIVE` |
| `renderer_diagnostics (15).json` | 685 | B `FRAME_PRESENTED` |
| `imported_glb_diagnostic (1).png` | 723,851 | A1 capture |
| `imported_glb_diagnostic (2).png` | 723,660 | A2 capture |
| `imported_glb_diagnostic (3).png` | 724,018 | A3 capture |
| `imported_glb_diagnostic (2).png` (second copy) | 0 | empty upload, no evidence |

The PNG captures were not opened during this review; the pixel figures above are the app's own background-subtracted counts, not an independent visual judgement. Opening the three PNGs is still required before any claim about avatar appearance or quality is made.
