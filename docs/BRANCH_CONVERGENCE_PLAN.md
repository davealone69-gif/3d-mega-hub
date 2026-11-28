# Branch convergence plan — stop the double work

Written 2026-11-26 after a full audit of all branches. Read this before starting any new work
branch or re-implementing a feature. The goal is **two branches**: `main` (frozen baseline) and
`integration/avatarforge-renderer-port` (the one trunk). Everything else converges into the trunk
and is deleted.

## Why this file exists

Repeated Arena sessions branched from *different stale bases* and rebuilt the same features instead
of merging into one trunk. Measured duplication found in the 2026-11-26 audit:

- **Animation system — built twice, two architectures.** The lead branch `arena/1a5b3043-3d-mega-hub`
  has a Kotlin `animation/` module (`AnimationLibrary`, `AnimationPlayback`, `FaceAnimation`,
  `LocomotionStateMachine`) plus C++ `af_anim_player.cpp` / `af_retarget.cpp` and GPU face animation.
  `arena/089497bd-3d-mega-hub` independently built a separate C++ `src/anim/` package
  (`af_anim_clip/_face/_library/_player/_pose/_retarget/_skeleton/_statemachine`) with
  `AnimationEngine.kt` / `NativeAnimationDriver.kt` / `ClipMapping.kt`. `arena/69f20785-3d-mega-hub`
  added a third pass ("character studio presets and animation controls").
- **Photoreal skin/eye/garment — duplicated.** `arena/089497bd`'s "subsurface scattering on skin",
  "iris colour (`eye_tone`)" and "garment woven/stitched relief" commits are already present in the
  lead (verified: identical `eye_tone` symbol and subsurface code paths in both).
- **Launcher icon — built four times** (`integration`, `arena/25bd4008`, `arena/69f20785`,
  `arena/33f12efd`).
- **GLES diagnostics / FileProvider** were rebuilt across `feature/first-working-gles-scene`,
  `work/fileprovider-renderer-diagnostics` and `fix/mega-hub-candidate-hardening` before landing in
  `integration`.

## The trunk

`integration/avatarforge-renderer-port` is the single trunk (per `AGENTS.md` rule #4; it is the base
of PR #9, the only review path to `main`). `main` stays frozen until the merge gates pass. Work
happens on a short-lived branch cut **from the trunk**, lands by pull request with a merge commit
(no squash — commit ids are cited in the docs), and the branch is then deleted.

## Disposition of the 8 branches that remain

| Branch | What it is | Action |
|---|---|---|
| `main` | Frozen stable baseline | **Keep.** Do not touch until gates pass. |
| `integration/avatarforge-renderer-port` | The trunk (PR #9 → main) | **Keep.** Everything merges here. |
| `arena/1a5b3043-3d-mega-hub` | Newest work: renderer crossfades/overlay, GPU face animation, Kotlin animation module, face rig + retargeting. Head of PR #12. | **Merge via PR #12, then delete.** This is the animation/face line that survives. |
| `arena/089497bd-3d-mega-hub` | Orphan, no PR. Holds the **only** `AvatarMp4Exporter.kt` (video export), quality probes (`glb_quality_targets_probe.py`, `generation_contract_test.cpp`), `ink.glb` reference, 3d4me fabric/material presets — **and** a duplicate `src/anim/` system + duplicate SSS/`eye_tone`. | **Cherry-pick the unique work only** (video export, quality probes, fabric presets) into one branch off the trunk; **discard** its parallel animation system and its SSS/eye work (the trunk already has them). Then delete. |
| `arena/25bd4008-3d-mega-hub` | PR #14. App features the trunk lacks: `AvatarAppearance`, `ChatTranscript`, `SpeechSettings`, `RendererTimeBudget`; the Filament preview blocker diagnosis (`BLOCKER_FILAMENT_PREVIEW_GLTFIO_PANIC.md`); compileSdk 37 toolchain. | **Merge via PR #14, then delete.** Skip its launcher icon (already on the trunk). |
| `arena/69f20785-3d-mega-hub` | PR #15 (draft). "Character studio presets and animation controls" — duplicates the lead's animation module. Also carries the trunk's bounded-GLB/reader fixes. | **Close or reduce.** Keep only non-animation UI presets if the trunk lacks them; do not merge a second animation controller. Then delete. |
| `fix/mega-hub-candidate-hardening` | Orphan, PRs #8/#10 closed unmerged. Diagnostics: explicit screenshot capture, PixelCopy timeout isolation, Filament frustum calls. 8 of its 11 files overlap the lead. | **Cherry-pick the explicit-screenshot-capture diagnostics** if the trunk lacks them; otherwise superseded — delete. |
| `arena/33f12efd-3d-mega-hub` | New session branch that re-imported the old `work/samsung-s24-photoreal-patch`; **198 commits behind** the lead; adds only a 4th launcher icon. | **Redundant.** Delete once confirmed it is not an active session. |

## Convergence order

1. **Resolve and merge PR #12** (`arena/1a5b3043` → trunk). It is currently conflicting; the trunk's
   13 unique commits are docs + the launcher icon + two GLB-import fixes
   (`BoundedGlbReader`, reader syntax). Take both sides deliberately.
2. **Decide the animation system once: keep the lead's** (Kotlin `animation/` + GPU face animation,
   already working with host checks). Do not merge `arena/089497bd`'s `src/anim/` or PR #15's
   animation controls on top of it.
3. **Cherry-pick `arena/089497bd`'s genuinely unique work** into one branch off the trunk:
   `AvatarMp4Exporter.kt` + the view-side export API, `glb_quality_targets_probe.py`,
   `generation_contract_test.cpp`, `imported_animation_test.cpp`, `ink.glb` quality reference,
   3d4me fabric/material presets. Leave its duplicate animation/SSS/eye work behind.
4. **Resolve and merge PR #14** (`arena/25bd4008`): app features + Filament blocker + toolchain.
5. **Reduce or close PR #15** (`arena/69f20785`) so it does not reintroduce a second animation
   controller.
6. **Cherry-pick or drop** `fix/mega-hub-candidate-hardening`'s screenshot-capture diagnostics.
7. **Delete** every work branch once its unique commits are on the trunk: `arena/1a5b3043`,
   `arena/089497bd`, `arena/25bd4008`, `arena/69f20785`, `fix/mega-hub-candidate-hardening`,
   `arena/33f12efd`.
8. **End state: `main` + `integration/avatarforge-renderer-port` only.** Then PR #9 promotes the
   trunk to `main` when the owner's gates pass.

## Already cleaned up (2026-11-26)

Deleted 15 branches that were contained in a kept ref, superseded, squash-merged, or held private
material. Tip SHAs were recorded before deletion and are recoverable with
`git push origin <sha>:refs/heads/<name>` while GitHub retains the objects:

- Contained in the trunk (zero loss): `fix/filament-engine-jni-init`, `work/engine-integration-candidate`,
  `work/filament-gles-renderer`, `work/filament-renderer-live-diagnostics`, `work/samsung-s24-photoreal-patch`.
- Abandoned single commits: `trae/agent-hDYcEF`, `agent/android-build-check-2026-11-03`,
  `work/avatarforge-core-integration`, `docs/glb-display-generation-baseline`, `arena/5db93cc5-3d-mega-hub`.
- Contained in the lead: `test/photo-realistic-avatar-overlay`.
- Superseded bring-up: `feature/first-working-gles-scene`, `work/fileprovider-renderer-diagnostics`.
- Squash-merged into `main`: `work/android-shell-foundation`.
- **Private material (`AGENTS.md` rule #5):** `add/mylove-rig-prepared-glb` committed
  `mylove_rig_prepared.glb` (13.2 MB) and `app/src/main/assets/mylove.glb` (9.3 MB, would pack into
  the APK). Deleted. The owner's reference model belongs in Dropbox, not in git.

## Rules to prevent recurrence

1. **Branch from the trunk only** — never from another work branch or an old photoreal branch.
2. **Search the trunk before implementing.** Animation, GPU face animation, launcher icon,
   FileProvider/diagnostics, subsurface skin and `eye_tone` already exist. Extend them; do not
   rebuild them.
3. **One implementation per feature.** If two branches need the same feature, merge one into the
   other; do not open a third.
4. **Delete your branch when it merges.** A merged branch left alive becomes a stale base for the
   next session's double work.
