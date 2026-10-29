# Project Status

Last checked: 2026-10-30.

## Repository

- Repository: [davealone69-gif/3d-mega-hub](https://github.com/davealone69-gif/3d-mega-hub)
- Default branch: `main`
- Working branch: `work/filament-gles-renderer`
- Renderer review: [PR #2](https://github.com/davealone69-gif/3d-mega-hub/pull/2)
- Current renderer commit: `4fa9178e5de36391af23a0f1c36c74b728e024f1`
- Stable-main policy: do not force-push or merge renderer changes until device verification and required checks are reviewed.

## Evidence-based feature status

| Area | Status | Evidence / remaining gate |
|---|---|---|
| Compose Android shell | PARTIAL | Shell code and unit-test CI are documented as passing in [run 40924116626](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/40924116626). |
| Filament OpenGL ES viewport | CI PASS / DEVICE UNVERIFIED | The JNI initialization fix is in commit [4fa9178](https://github.com/davealone69-gif/3d-mega-hub/commit/4fa9178e5de36391af23a0f1c36c74b728e024f1). Push and PR CI passed; see [run 40928431877](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/40928431877). Fresh debug APK artifact is available from that run. |
| Visible model pixels on Samsung SM-S921B | BLOCKED | The fresh APK must be installed and checked on the physical phone. A submitted frame alone is insufficient. |
| Avatar generation / AvatarForge native integration | MISSING | Donor code has not been integrated into the host app. |
| Embedded LLM | MISSING | No inference backend is integrated in this branch. |
| Voice and lip-sync | MISSING | No connected speech/viseme pipeline is integrated. |
| Persistent avatar library / job queue | MISSING | Not yet implemented as working host-app features. |
| Signed release AAB and APK | MISSING | Release signing and artifact verification are not yet established. |

## Renderer decision

Start with Filament and explicit OpenGL ES selection. Vulkan is optional until demonstrated stable on the target phone. Do not run multiple renderer stacks in parallel without a tested requirement.

## Next gates

1. Install the fresh APK from [CI run 40928431877](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/40928431877) and verify visible model pixels, touch rotation, and reopening after leaving the viewer.
2. Fix any device-only renderer or lifecycle failures before merging PR #2.
3. After renderer verification, inspect and selectively integrate donor components at recorded commit SHAs.
4. Configure reproducible AAB/APK outputs and secret-backed release signing.
