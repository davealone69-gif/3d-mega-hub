# Project Status

Last checked: 2026-10-30 (UTC+08:00).

## Repository state

- Repository: [davealone69-gif/3d-mega-hub](https://github.com/davealone69-gif/3d-mega-hub)
- Default branch: `main`.
- Latest updated feature source inspected: `work/filament-renderer-live-diagnostics`, commit `fbd62f55538d96dfa44e87dac12a82c5030c32dd`.
- [PR #4](https://github.com/davealone69-gif/3d-mega-hub/pull/4) (renderer startup diagnostics/JNI) and [PR #5](https://github.com/davealone69-gif/3d-mega-hub/pull/5) (AvatarForge core) remain **OPEN**. PR #1 is merged; PRs #2 and #3 remain open.
- Local build and documentation changes are on a detached checkout of the latest source commit; they have not been committed or pushed. No new branch was created.

## Evidence-based status

| Area | Status | Evidence / remaining gate |
|---|---|---|
| Clean local Android build | **REAL + VERIFIED** | On the latest source plus local signing-gate and Vulkan-first changes, `clean testDebugUnitTest lintDebug assembleDebug minifyReleaseWithR8` passed with forced task execution and no Gradle build-cache reuse. Full log: `build-evidence/vulkan-primary-build.log`. |
| Unit tests | **REAL + VERIFIED** | 7 tests passed, 0 failures/errors/skips, including Vulkan-first ordering and OpenGL ES fallback cases. |
| Android lint | **REAL + VERIFIED** | `lintDebug` passed locally and in the latest-commit CI run. |
| Debug APK | **REAL + VERIFIED** | Vulkan-first APK assembled and verified with `apksigner`; signed by the standard Android debug certificate, not a release identity. Size: 36,062,582 bytes. SHA-256: `b86b76cce795614c3b30b35fd0f078d7150e1adcbd71edd0470275573020f596`. File: `build-evidence/deliverables/3D-Mega-Hub-vulkan-primary-debug.apk`. |
| R8/minification | **REAL + VERIFIED** | `minifyReleaseWithR8` passed without packaging an unsigned APK/AAB. Filament keep rules are present. |
| Release APK/AAB signing | **BLOCKED** | Alias `3D` and saved store/key password references are available, but the release keystore file is missing. Only debug keystores were found; they are not valid substitutes. `assembleRelease` and `bundleRelease` fail explicitly without signing material. |
| Native ABI/JNI packaging | **REAL + VERIFIED (static inspection)** | Debug APK includes Filament JNI for `arm64-v8a`, `armeabi-v7a`, `x86`, and `x86_64`. In ARM64 libraries, `Engine.nCreateBuilder` and `Manipulator.nCreateBuilder` JNI symbols are exported; Filament `PT_LOAD` segments inspected at 16 KB alignment (`0x4000`). This does not prove runtime initialization on the phone. |
| Filament backend selection | **PARTIAL** | Source calls `Filament.init()`, tries the complete `Engine.Backend.VULKAN` renderer-session setup first, then retries with `Engine.Backend.OPENGL` if that setup throws or returns no result. It avoids `ModelViewer`/`Manipulator`. `FilamentPreviewView` is not mounted by `MainActivity`, and currently only submits a clear frame; no model is rendered by the app UI. |
| App navigation and existing controls | **PARTIAL / UNVERIFIED** | Home, Avatar, AI, Voice, and Settings composables are connected in source. No device interaction test was possible. Avatar import persists a URI but does not display the model. |
| Ollama assistant | **PARTIAL** | The app sends real HTTP requests to the configured Ollama `/api/tags` and `/api/chat` endpoints; it is not an embedded model and requires a reachable endpoint/model. |
| Speech output | **PARTIAL** | Android `TextToSpeech` is connected; visemes and avatar lip-sync are missing. |
| AvatarForge generation | **MISSING from latest branch** | The integration code remains on open PR #5, separate from the latest renderer branch; no generation flow is available in this build. |
| SM-S921B/API 37 runtime | **BLOCKED / UNVERIFIED** | `adb devices -l` returned an empty device list. No install, launch, crash, navigation, touch, renderer startup, or visible-pixel test was performed. Current compile/target SDK are 35. |
| SM-S921B graphics capability | **REAL + VERIFIED (checker log only)** | The user-provided Vulkan Checker log reports Xclipse 940, ARM64, 4 KB pages, Vulkan 1.3.304 plus Dynamic Rendering, Push Descriptor and `fillModeNonSolid`, and GLES 3.2 via ANGLE over Vulkan. This confirms the separate checker app's device probe, not 3D Mega Hub behavior. |
| New GitHub release | **NOT PUBLISHED** | Existing [release `android-build-46`](https://github.com/davealone69-gif/3d-mega-hub/releases/tag/android-build-46) is an already-published debug-only release. No new release was created; exact new public payload still requires signed outputs and owner approval. |

## Recent verification

The live [GitHub Actions run 40990370483](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/40990370483) succeeded for the base source commit. The current, unpushed Vulkan-first local tree passed a forced clean build, all 7 unit tests, lint, debug packaging, and R8 minification; detailed output is in `build-evidence/vulkan-primary-build.log`.

The published debug-only APK from release `android-build-46` has SHA-256 `9b786e1fcab1b1b7ffbc657f41053ec021b719c4bcb9c66c8ccdc2b5693a61b5`; its direct URL is https://github.com/davealone69-gif/3d-mega-hub/releases/download/android-build-46/3D-Mega-Hub-debug.apk. The freshly built Vulkan-first debug APK has SHA-256 `b86b76cce795614c3b30b35fd0f078d7150e1adcbd71edd0470275573020f596` and is included in the local deliverables directory.

## Local changes made for this task

- Added a release keystore/credential preflight to `app/build.gradle.kts`; missing signing material now blocks release APK/AAB packaging with an explicit message.
- Updated `.github/workflows/android.yml` so CI checks release minification without creating unsigned release packages and does not auto-publish a public GitHub release.
- Updated Filament startup to prefer Vulkan and retry OpenGL ES if the full Vulkan renderer session fails. The device-provided Vulkan Checker report (Vulkan 1.3.304) is capability evidence only; app startup and rendering remain unverified without the phone.
- Updated this status file and `README.md` to reflect current evidence and limitations.
- These changes have not been pushed. The changed workflow has not yet run on GitHub.

## Next required gates

1. Provide the existing release keystore file; the signing alias and password references are already available. Then build, verify signatures, and calculate hashes for the signed AAB and APK.
2. Connect the Samsung SM-S921B with USB debugging authorized so ADB can install and launch the exact build; exercise navigation and controls, and inspect a visible rendered model.
3. Connect the existing Filament view and the AvatarForge engine to the latest app UI; rebuild and rerun static, unit, and device checks.
4. Prepare the exact public release tag, title, notes, artifact list, and hashes. Publish only after the owner approves that exact payload.
