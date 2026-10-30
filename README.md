# 3D Mega Hub

**An Android 3D hub in active integration. Current status: PARTIAL; physical-device and signed-release gates remain.**

## Latest available debug APK

[Download the existing GitHub debug APK](https://github.com/davealone69-gif/3d-mega-hub/releases/download/android-build-46/3D-Mega-Hub-debug.apk) from [release `android-build-46`](https://github.com/davealone69-gif/3d-mega-hub/releases/tag/android-build-46). It is a troubleshooting/debug build signed with the Android debug key, not a signed Play Store release. The published file's SHA-256 is `9b786e1fcab1b1b7ffbc657f41053ec021b719c4bcb9c66c8ccdc2b5693a61b5`.

A fresh Vulkan-first debug build is available as [`3D-Mega-Hub-vulkan-primary-debug.apk`](build-evidence/deliverables/3D-Mega-Hub-vulkan-primary-debug.apk) (36,062,582 bytes; SHA-256 `b86b76cce795614c3b30b35fd0f078d7150e1adcbd71edd0470275573020f596`). It is development-signed; do not treat it as a production release. Its verification report and build log are in `build-evidence/`.

## Current implementation

- **Navigation:** Home, Avatar, AI, Voice, and Settings screens are connected with Compose navigation.
- **Avatar library:** GLB/glTF selection and removal are wired, and the selected URI is persisted. The current app does **not** display the selected model: `FilamentPreviewView` is not mounted in the app screens and its current implementation is a clear-frame renderer bring-up, not an avatar viewer.
- **Filament:** The source explicitly loads Filament, then attempts a Vulkan renderer session first and retries with OpenGL ES if Vulkan initialization fails. Filament R8 keep rules are present. The JNI libraries and symbols are packaged, but runtime rendering has not been verified on the target phone.
- **AI:** Chat and connection checks call the configured Ollama `/api` endpoints. This is not an embedded model; it requires a reachable Ollama server and configured model.
- **Voice:** Text-to-speech uses Android's installed speech engine. Viseme extraction and avatar lip-sync are not connected.
- **AvatarForge:** The native generation core is on open [PR #5](https://github.com/davealone69-gif/3d-mega-hub/pull/5), not in the tracked source of the latest renderer branch. Generation is therefore not available in that branch's app.

## Build and release status

The latest feature source inspected is `work/filament-renderer-live-diagnostics` at commit `fbd62f55538d96dfa44e87dac12a82c5030c32dd`; [PR #4](https://github.com/davealone69-gif/3d-mega-hub/pull/4) and [PR #5](https://github.com/davealone69-gif/3d-mega-hub/pull/5) are still open. That source commit passed [GitHub Actions run 40990370483](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/40990370483). The current, unpushed Vulkan-first changes passed 7 local unit tests, lint, debug APK assembly, and release R8 minification; see `build-evidence/vulkan-primary-build.log`.

No signed release APK or AAB was produced: the release keystore file is missing. The signing alias and saved password references are available, but debug keystores cannot be used as release keys. The local Gradle release packaging tasks now fail explicitly with `BLOCKED` if the keystore or credentials are missing, rather than silently producing an unsigned release. Configure `releaseStoreFile`, `releaseStorePassword`, `releaseKeyAlias`, and `releaseKeyPassword` as Gradle properties, or use the corresponding `ANDROID_RELEASE_*` environment variables. Never commit these values.

No new GitHub release was published. The existing release is debug-only. A future public release must be reviewed and approved with its exact tag, release notes, artifact list, and hashes before publication.

## Target-device verification

The target is the Samsung Galaxy S24 SM-S921B (Exynos 2400, ARM64, Android 17/API 37). The separate Vulkan Checker log reports Vulkan 1.3.304 with Dynamic Rendering, Push Descriptor, and `fillModeNonSolid`, plus OpenGL ES 3.2 via ANGLE over Vulkan. This confirms device capability only, not app runtime. The app now attempts Filament Vulkan first and retries OpenGL ES if the Vulkan engine, renderer, or swap-chain setup fails; it does not pin a required Vulkan patch version. No physical phone is connected (`adb devices -l` is empty), so install, launch, navigation, renderer startup, and visible-pixel verification remain **BLOCKED / UNVERIFIED**. The project currently compiles with SDK 35 / target SDK 35; API 37 behavior has not been tested.

## Project records

- [Project status](PROJECT_STATUS.md): evidence and remaining gates.
- [Master blueprint](docs/MASTER_BLUEPRINT.md): architecture and renderer decision.
- [Integration plan](docs/INTEGRATION_PLAN.md): staged integration checklist.
- [Donor baseline](docs/DONOR_BASELINE.md): donor source and evidence.
- [Third-party notices](THIRD_PARTY_NOTICES.md): dependency and asset provenance.
