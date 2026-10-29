# Project Status

Last checked: 2026-10-30.

## Repository

- Repository: [davealone69-gif/3d-mega-hub](https://github.com/davealone69-gif/3d-mega-hub)
- Default branch: `main`
- Working branch: `work/filament-gles-renderer`
- Renderer review: [PR #2](https://github.com/davealone69-gif/3d-mega-hub/pull/2)
- Stable-main policy: do not force-push or merge renderer changes until the required checks are reviewed.

## Evidence-based feature status

| Area | Status | Evidence / remaining gate |
|---|---|---|
| Compose Android shell | PARTIAL | Shell code and unit-test CI are documented as passing in [run 40924116626](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/40924116626). |
| Filament OpenGL ES viewport | UNVERIFIED | Renderer implementation exists on the working branch and is under PR review. Current PR CI must be checked before merge. |
| Visible model pixels on Samsung SM-S921B | BLOCKED | No physical-device pixel capture/runtime evidence is available in this work session. A submitted frame alone is insufficient. |
| Avatar generation / AvatarForge native integration | MISSING | Donor code has not been integrated into the host app. |
| Embedded LLM | MISSING | No inference backend is integrated in this branch. |
| Voice and lip-sync | MISSING | No connected speech/viseme pipeline is integrated. |
| Persistent avatar library / job queue | MISSING | Not yet implemented as working host-app features. |
| Signed release AAB and APK | MISSING | Release signing and artifact verification are not yet established. |

## Renderer decision

Start with Filament and explicit OpenGL ES selection. Vulkan is optional until demonstrated stable on the target phone. Do not run multiple renderer stacks in parallel without a tested requirement.

## Next gates

1. Check the current status and logs for PR #2 CI.
2. Fix compile/test/lifecycle problems before merging.
3. Verify visible pixels and lifecycle behaviour on the Samsung device.
4. Then inspect and selectively integrate donor components at recorded commit SHAs.
5. Configure reproducible AAB/APK outputs and secret-backed release signing.
