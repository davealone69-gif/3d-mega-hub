# AvatarForge Engine Integration

## Decision

`3d-mega-hub` remains the canonical Android app. AvatarForge `afcore` is the selected generation kernel; rendering stays a separate, replaceable adapter. This is a deliberate port of the useful engine boundary—not a wholesale copy of the donor app or its renderer.

## Source and provenance

| Item | Evidence |
|---|---|
| Donor | [davealone69-gif/AvatarForge](https://github.com/davealone69-gif/AvatarForge) |
| Source branch and SHA | `work/android-engine-dev` at `9e36197ace3b18faa6c566d93f8fce89b185c45d` |
| Donor CI | [AvatarForge workflow run 40102403904](https://github.com/davealone69-gif/AvatarForge/actions/runs/40102403904) succeeded; this does not establish target-phone behavior |
| Host module | `:avatarforge-engine`, built for `arm64-v8a` and `x86_64` |

The prior repository scan enumerated 99 accessible repositories and identified 17 likely first-party 3D/avatar projects. AvatarForge was chosen as the generation-engine source. The 3DDD Filament viewer remains separate; its inspected [Android CI run 40981986126](https://github.com/davealone69-gif/3DDD/actions/runs/40981986126) failed, so its renderer was not imported.

## What the host module contains

The module carries AvatarForge's public `afcore.h` and all 18 C++ kernel sources listed in the pinned CMake file, plus the donor's Kotlin `AvatarSpec` and `DeviceResourceGovernor` models (their package name is adapted for this app). The 39 copied native files match the pinned donor byte-for-byte except that trailing spaces were removed from one blank line in `af_math.h`; the two Kotlin models differ only in package declaration.

A narrow JNI bridge checks ABI/version, maps the ABI-v1 spec fields, calls the donor's deterministic prompt analyzer before `run_all()`, exports GLB, and returns native mesh statistics plus the parser's attribute-match count. Prompt parsing is keyword/measurement logic, not an LLM and not image-to-identity reconstruction. An unrecognized prompt is reported as zero matches rather than silently described as understood.

Generation is opt-in through an API and does not run at app startup. The facade runs native work on an interruptible background dispatcher, checks memory/storage/thermal state, caps the engine budget at 1.5 GiB and at half measured free RAM, and writes GLBs only under app-private `filesDir/avatars`. Failed or cancelled work removes partial output. These resource checks are conservative gates, not a guarantee against Android memory reclamation. Adult-mode confirmation is checked against the prompt-parsed spec as well as the caller's explicit `AvatarSpec`.

## Deliberately excluded

AvatarForge's `native/src/render` directory, renderer JNI, Android app UI, prebuilt AAR/APKs and generated artifacts are not copied. This avoids introducing a second renderer or depending on an artifact whose source SHA is unknown. The host does not yet expose a generation screen, avatar viewer, real-time performance claim, image reconstruction or embedded AI.

## Licensing note

No repository-level `LICENSE` file was present at the pinned AvatarForge branch. The source was copied between the user's own repositories at their explicit direction; that does not establish third-party redistribution rights. Before external distribution, choose and document the intended project license and audit future assets/dependencies separately. No AvatarForge model or image assets were copied.

## Verification status

| Check | Status |
|---|---|
| Source pin and donor CI | Verified |
| Native source provenance | Verified: 39 copied native files match the donor; one whitespace-only line was trimmed |
| Clean Android build | Passed: `clean testDebugUnitTest lintDebug assembleDebug assembleRelease` with NDK `26.1.10909125` and CMake `3.22.1` |
| Kotlin unit tests | Passed: 8 total (4 app, 4 engine), zero failures |
| Android lint | Passed |
| Debug APK and release/R8 assembly | Passed for `arm64-v8a` and `x86_64`; release APK is unsigned |
| APK inspection | Both AvatarForge libraries and all three JNI exports are present for both ABIs; no `INTERNET` permission |
| Device/runtime generation | Blocked here: no `adb` or emulator is available; no Samsung target attached |
| Renderer, memory/thermal and cancellation behavior under load | Unverified on device |

## Recommended next steps

1. Add a user-triggered generation flow that displays parser matches, stage progress, cancellation and durable errors; never launch heavy generation automatically.
2. Run one bounded request on the Samsung SM-S921B / Exynos 2400 and record the resource snapshot, GLB size, triangles, timings and failures.
3. Select and test one viewer separately for visible model pixels, import/export, lifecycle, memory and thermal behavior. Revisit 3DDD only after its CI/build and licensing gates pass.
