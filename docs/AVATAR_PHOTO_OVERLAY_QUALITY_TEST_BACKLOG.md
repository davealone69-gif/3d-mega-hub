# Avatar / Photo Overlay Quality Test Backlog

Branch: `test/photo-realistic-avatar-overlay`
Base: `2a1352c5994a42a586a929d995a33fcb4f6ebd67`
Scope: evaluation of generated 3D avatars against authorized real-photo references. This backlog defines tests and evidence only; it does not change the renderer or claim that the current Hub can load an avatar. At the base commit, the GLB picker is not connected to the Filament preview, so model-quality tests remain BLOCKED until the test harness demonstrably renders the supplied GLB.

## Quality gate

A result passes only when it comes from the actual app renderer or a named, reproducible renderer harness using the tested avatar asset. A polished screenshot alone is not evidence. Every run must preserve the input photo and avatar, record their SHA-256 hashes, record source commit and renderer/build configuration, and produce machine-readable measurements plus repeatable visual captures. No camera, lighting, crop, pose, or color adjustment may be hidden between comparisons.

Use only photos with documented permission and provenance. Keep reference photos local to the authorized test workspace; do not upload them to external services. Do not commit private photos or generated likenesses to the repository. Synthetic/dummy fixtures may validate plumbing, but cannot pass a photorealism gate.

## Backlog

### AVQ-00 — Prove the real avatar render path (P0, prerequisite)

- Load a self-contained, genuinely generated, rigged GLB into the renderer under test. Record the exact model hash, skin/joint/weight counts, texture/material inventory, render backend, device/GPU and app build.
- Capture the rendered avatar from the same camera configuration used by the quality tests. Include a visible frame marker or capture metadata that distinguishes renderer output from a mock, placeholder, or imported reference image.
- Acceptance: opening the asset demonstrably produces the avatar’s rendered pixels, not a clear frame or UI preview. If the app has no loader-to-render path, mark this and all downstream visual-quality cases BLOCKED; do not substitute screenshots.

### AVQ-01 — Establish a controlled reference set (P0)

- Create a small, rights-cleared local set with front, side and three-quarter views; neutral expression; consistent pose; known approximate height; and visible face, shoulders, hips, hands and feet. Include lighting and camera metadata where available.
- Keep original photo, crop, orientation, color profile and optional landmark annotations immutable. Create a manifest with consent/provenance, capture conditions, file hashes and excluded regions.
- Acceptance: another tester can select the same case from the manifest and reproduce the same input without relying on a private upload or undocumented manual edits.

### AVQ-02 — Make comparison captures repeatable (P0)

- Add a capture recipe for output resolution, aspect ratio, projection/camera parameters, avatar pose, world transform, background image, light/environment, exposure, tone mapping, shadow settings and random seeds.
- Save a structured run record (JSON) alongside images. Record the app commit, asset/reference hashes, model scale, camera pose, renderer/backend, device profile and tool versions.
- Acceptance: repeated runs with unchanged inputs/configuration produce aligned captures and measurements; any nondeterminism is measured and reported rather than hidden.

### AVQ-03 — Measure body proportions and silhouette (P1)

- Annotate or detect consistent landmarks: crown, chin, shoulders, elbows, wrists, pelvis/hip points, knees and ankles. Measure normalized head-to-body, shoulder-to-height, hip-to-height, torso-to-leg, upper/lower-limb and left/right ratios.
- Compare rendered landmarks and silhouette with reference annotations in the matched view. Report per-landmark pixel/reprojection error, normalized proportion error, silhouette intersection-over-union, and the worst case; retain per-view results rather than only a mean.
- Use multiple views for depth-related proportions. A single 2D photo cannot prove 3D depth or hidden anatomy.
- Acceptance: publish thresholds only after establishing measurement repeatability and annotator/detector variance on the chosen reference set. No “looks close” pass without the measurements and overlays.

### AVQ-04 — Check face/body alignment (P1)

- Align camera intrinsics/extrinsics and avatar scale to the photo using recorded landmarks. Evaluate face landmarks separately (eye line, nose, mouth, chin) and body landmarks (shoulders, pelvis, hands and feet).
- Produce a side-by-side, 50% alpha overlay and landmark residual plot. Report face and body reprojection error separately, plus head-to-neck and neck-to-shoulder discontinuity observations.
- Test front, side and three-quarter views; include at least one pose with visible arm/torso overlap so limb placement is not judged only against the outer silhouette.
- Acceptance: alignment remains within the agreed measured tolerance across views; changes to camera or avatar scale are logged and repeatable.

### AVQ-05 — Score skin and hair quality (P1)

- Capture matched face close-ups and full-body views under neutral, warm and side lighting. Use a color reference/gray card when the source photo provides one; record exposure and white-balance assumptions.
- Skin review: face/body texture continuity, UV seams, texture stretch, skin-tone consistency, specular/roughness response, visible normal-map artifacts, and shading across nose, cheeks, forehead, neck and hands. Where color is calibrated, report a color-difference measure (for example ΔE00) on defined skin regions, together with its sampling method.
- Hair review: hairline and scalp coverage, silhouette match from front/side/back, strand/card transparency artifacts, clumping, gaps, intersections with face/ears/shoulders, and temporal flicker in motion captures.
- Acceptance: retain standardized crops and a per-case issue checklist. A color score cannot hide obvious geometry, hairline, texture, or shading defects; no overall pass may be inferred from a single attractive angle.

### AVQ-06 — Test the avatar composited over the real photo (P1)

- Place the actual rendered avatar over its corresponding photo plate. Measure projected scale and landmark alignment; inspect foot-ground contact, perspective, contact/cast shadows, color-temperature mismatch, edge halos, and foreground/background occlusion.
- Save the unmodified source plate, avatar-only render, alpha/depth or mask inputs where available, and final composite. Include a difference/edge view so seams and accidental photo edits are visible.
- Acceptance: source pixels outside the declared composite/mask regions remain unchanged; the avatar has no unexplained floating feet, clipping, halo or impossible depth ordering. Report manual mask edits explicitly.

### AVQ-07 — Check animation stability against the photo plate (P2)

- With a fixed reference camera and background, render a specified short clip at fixed frame rate and sample start, middle, end and high-motion frames. Record clip/rig hashes and retarget mapping if used.
- Measure landmark drift, foot sliding, joint-collapse/self-intersection observations, face/neck continuity, hair silhouette flicker, shadow stability and frame-to-frame alignment. Check both the unposed/rest state and one representative movement.
- Acceptance: thresholds are derived from a repeatable baseline; failures identify frame/time and joint/region. A static still-image pass does not imply animation passes.

### AVQ-08 — Keep a regression evidence bundle (P1)

- For every accepted run, store a compact local bundle: manifest, JSON measurements, reference/rendered/composited captures, overlays/difference views, logs, hashes and a concise outcome per test ID.
- Compare new runs against the same baseline and flag changed pixels/metrics. Review all flagged regressions; do not auto-accept a prettier render if proportions or alignment worsened.
- Acceptance: a reviewer can reproduce the exact run and inspect both numeric measurements and genuine renderer captures. Do not commit personal photos; retain or share them only under their documented permission.

## Current status and next gate

- AVQ-00: BLOCKED at the recorded Hub base commit because the file-picker URI is not consumed by the preview and the preview is not mounted in the app screen.
- AVQ-01 through AVQ-08: NOT RUN. They require a real avatar render path and a rights-cleared reference set.
- No renderer, model, shader, build configuration, photo asset, or production code change is authorized by this backlog. The first implementation handoff must be a separate scoped request; the existing renderer work remains untouched.


## Ground-truth geometry notes supplied by the user (2026-11-14)

The user supplied the following whole-mesh X-width / Z-depth envelope measurements for `mylove.glb`, divided into ten equal-height Y bands, bottom-to-top. Overall Y is approximately -0.499926 to +0.499917 model units (total height 0.999843); each band is approximately 0.099984 units tall.

| Band | Y minimum | Y maximum | X width | Z depth |
|---:|---:|---:|---:|---:|
| 1 | -0.499926 | -0.399941 | 0.606548 | 0.342837 |
| 2 | -0.399941 | -0.299957 | 0.432327 | 0.229914 |
| 3 | -0.299957 | -0.199973 | 0.492954 | 0.205983 |
| 4 | -0.199973 | -0.099989 | 0.530137 | 0.178481 |
| 5 | -0.099989 | -0.000004 | 0.544348 | 0.155567 |
| 6 | -0.000004 | 0.099980 | 0.587684 | 0.148691 |
| 7 | 0.099980 | 0.199964 | 0.624390 | 0.187147 |
| 8 | 0.199964 | 0.299948 | 0.799449 | 0.218482 |
| 9 | 0.299948 | 0.399932 | 0.652151 | 0.188113 |
| 10 | 0.399932 | 0.499917 | 0.181655 | 0.156217 |

**Interpretation constraint:** these are per-band bounds across all vertices, not isolated anatomical measurements. Hair, hands, arms, feet, clothing or other geometry can influence a band's extrema. Do not label a band as a specific body part or directly copy these widths into body parameters without inspecting the reference mesh and isolating the relevant geometry.

**Symmetry gate:** inspect LOD0 vertex positions for exact mirrored pairs across X before changing anatomy. Preserve the previously agreed maximum mirror-pair tolerance of 0.004 model units. Report unmatched vertices and maximum residual with their coordinates; do not average away or silently repair failures. Apply one geometry fix at a time, then run the relevant native regression and generated-avatar diagnostic before the next fix.

**Reference provenance:** Dropbox contains two user-owned copies of `mylove.glb`, each 9,287,272 bytes; the diagnostics-folder copy was last modified 2026-11-10. The raw GLB binary still must be inspected in the implementation workspace before freezing a reference manifest or changing geometry. The ten-band envelope alone is not a substitute for that inspection.
