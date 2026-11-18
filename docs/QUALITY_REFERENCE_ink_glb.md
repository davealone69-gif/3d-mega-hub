# Quality reference target — `ink.glb`

**Status: RECORDED, NOT YET FRESHLY MEASURED.** The structural figures below were recorded by the
project owner from a prior inspection and pasted into conversation on 2026-11-18; they have **not**
been independently re-derived from the binary in this workspace. They are corroborated only by the
file size (the recorded "103.6 MB" equals the 103,566,844-byte Dropbox object found the same day)
and by the texture filenames beside the asset (DAZ "G8F" decomposition). Treat every number as
*owner-recorded* until a fresh probe replaces it; the Required fresh measurements section lists
exactly what must be confirmed.

**Privacy.** `ink.glb` is a private owner asset. Do **not** commit, publish or redistribute it. This
document records its identity and measurements only, never its bytes. Some neighbouring assets in the
same Dropbox folder carry adult-content names; any route that adopts such content must respect the
adult-confirmation gate already flagged as unbuilt in
[PROJECT_REUSE_AUDIT_2026-11-15.md](PROJECT_REUSE_AUDIT_2026-11-15.md).

## 1. Asset identity

| Field | Value | Source |
|---|---|---|
| Primary copy | Dropbox `/ink.glb` | Dropbox search, 2026-11-18 |
| Duplicate (byte-identical) | Dropbox `/3d real/ink.glb` | same search; identical content hash |
| Size | 103,566,844 bytes (~103.6 MB decimal / ~98.8 MiB) | Dropbox metadata |
| Content hash (SHA-256) | `081e2ddd1f3d774063d2bdf383be1972ef6a38cbd5086a14d3fab23b6295f80b` | Dropbox metadata |
| Date observed | client/server modified 2026-11-15 | Dropbox metadata |
| Project package | Dropbox `/New folder (2)/ink.zip` (~115 MB); source `/New folder (2)/ink/source/zeee.fbx` (~72 MB) | Dropbox search |
| Generator lineage (INFERRED, not from the binary) | **DAZ Studio, Genesis 8 Female ("G8F")** — inferred from the sibling source FBX and texture names (`G8FBaseFaceMapD`, `G8FBaseArmsMapD`, `G8FBaseLegsMapD`, `G8FBaseMouthMapD`, `G8FBaseEyes01`, `G8FBaseEyelashes`, plus hair/outfit alpha maps). Confirm the generator string on fresh inspection. | filename evidence |

## 2. Owner-recorded metrics (to verify)

| Measurement | Recorded result | Confidence |
|---|---|---|
| GLB file size | 103.6 MB | corroborated (matches Dropbox object) |
| Skeleton joints | 199 | owner-recorded; verify |
| Body geometry | 27,292 triangles | owner-recorded; verify |
| Skin textures | 4 × 4096 × 4096 | owner-recorded; verify dimensions/channels |
| Morph targets / blendshapes | 0 | owner-recorded; verify |
| Animation clips | 0 | owner-recorded; verify |
| Default pose | A-pose | owner-recorded; verify bind transform |
| Renderer parts | 45 | owner-recorded; verify node/mesh split |
| Parts reported as textured | 4 | owner-recorded; verify material assignments |
| Estimated decoded texture memory | ~1,152 MB | owner-recorded estimate; recompute |
| Import-optimisation prototype | ~384 MB decoded | owner-recorded; method unstated |

## 3. Generator baseline (this repository, documented)

For comparison only; sources noted inline. These describe the procedural AvatarForge pipeline, not
`ink.glb`.

- Rig: **71 named joints** built by `avatarforge-engine/src/main/cpp/src/af_rig.cpp`; **52 facial
  blendshape channels**; authored clip library of 16 poses/clips (`idle … turn_360`).
- Generated avatar mesh: roughly **823,760 vertices / ~1,103,656 triangles**, 11 submeshes,
  **13 materials** (seed-1337 female/athletic/medium, JEANS_JACKET), per `PROJECT_REUSE_AUDIT_2026-11-15.md`
  and the AvatarForge integration notes.
- Materials emitted: base PBR plus `KHR_materials_sheen` on 10 of 13 materials, `KHR_materials_ior`
  on 13 of 13, custom `AF_subsurface` on 7 of 13; five 2048² PNG textures. The pinned Filament 1.71.6
  preview cannot consume `sheenColorIndex`, which is why file-backed avatars route to the native GLES
  renderer instead.
- Decoded texture cost for one seed-1337 avatar: **~68 MiB** (`af_engine_stats.texture_bytes`), with
  geometry ~81.8 MiB and peak field ~41.1 MiB.
- Memory ceiling: a generated-avatar load previously died with `OutOfMemoryError` near a
  **~128–200 MB** process heap limit (since mitigated by memory-mapped loading, but the handset
  budget is unchanged).
- Hair: signed-distance-field extracted geometry; specular currently uses a **world-up approximation**,
  not groom-authored per-strand tangents. Missing pieces enumerated in the README hair handover:
  scalp-cap socket, fitted hairline baby-hair cards, strand-card shells, flyaways, shared strand
  atlas/mask channels, three-level hair LODs.
- Proportions: the whole-mesh cross-section report recorded unresolved yaw/height-registration
  residuals and no defensible torso mask; the owner judged generated quality "too poor to judge
  appearance changes reliably".

## 4. Quantified gap analysis (prioritised)

The decisive observation: `ink.glb` carries **~27,292 triangles**; the generator emits **~1.1 million**.
We already hold ~40× the geometry and look worse, so **polygon count is not the lever**. The gap is
how surfaces interact with light, plus shape correctness.

**P0 — Texture detail.** Replace/augment the flat procedural bake with photographic micro-detail:
high-resolution albedo plus a **detail-normal map** (pores, creasing) and roughness/AO variation,
decomposed by region the way the G8F set is (face/arms/legs/mouth/eyes/lashes separately). Bake
micro-detail into normals so albedo can stay small (see §5).

**P0 — Skin shading.** Ship a complete standard-PBR channel set and make skin **subsurface scattering**
actually approximate translucency, rather than the custom `AF_subsurface` extension the pinned
preview rejects. Calibrate effective metalness/roughness from decoded texture channels, not scalar
guesses (the same open task recorded for `mylove.glb`).

**P0 — Eyes.** Model eyes as separate cornea/sclera/iris geometry with their own textures instead of
painted-on eyes.

**P1 — Hair.** Add textured hair-card / alpha layers over the SDF volume, groom-authored strand
tangent direction for the specular term, and the hairline/flyaway/atlas/LOD items already listed as
missing.

**P1 — Anatomy/proportion targeting.** Derive measured landmark and slice-proportion targets *from
`ink.glb`* and drive the anthropometry toward them, backed by native regressions, so silhouette
correctness is asserted rather than hoped for.

**P2 — Bone density (not capability).** `ink.glb` records 199 joints versus our 71, which can smooth
deformation around hands/face; however it records **0 morphs and 0 clips**, so we already lead on
animation and facial expression. Treat added bone density as polish, secondary to texturing/shading.

## 5. Mobile budget reality

Recorded decoded texture memory is **~1,152 MB**; even the optimisation prototype is **~384 MB** —
both far above the **~128–200 MB** heap ceiling that has crashed the app. Raw `ink.glb`-class
textures therefore cannot ship on the handset. Matching the *look* requires: ASTC texture
compression (large constant-factor saving), dropping 4K→2K where it reads identically, atlasing the
per-region G8F maps, baking surface micro-detail into normal maps so albedo stays small, full mipmap
chains, and a geometry+texture LOD chain with lazy per-part loading. Aim for decoded texture cost in
the **tens of MB**, not hundreds, and re-measure against the device, not the desktop.

## 6. Strategy (recommended order)

Do **not** replace the procedural generator with the reference mesh (standing project constraint).

1. **Hybrid (recommended).** Keep the procedural body, rig and animation; raise fidelity by adopting
   *authored, mobile-compressed* high-quality texture and hair-card assets in the DAZ style, adding
   genuine skin SSS and a real eye model, and tuning proportions against `ink.glb`. Targets the actual
   bottleneck (texture/material/hair) without discarding the working pipeline.
2. **Premium import tier.** Make `ink.glb`-class assets directly usable as an imported top-end option.
   Caveats: no animations or morphs in the recorded asset, so our clips must be retargeted onto its
   199-bone skeleton (DAZ bone naming differs from ours and would need role mapping), and only the
   compressed-texture variant may ship (§5).
3. **Full procedural parity.** Hardest: synthesize scanned-grade texture detail procedurally. Unlikely
   to match authored 4K detail; pursue only after (1) proves the look.

## 7. Required fresh measurements

Convert "owner-recorded" to "measured" by running the repository probes on the actual file wherever it
lives and recording their output here:

```
python3 tools/glb_quality_targets_probe.py <path-to>/ink.glb   # verts/tris, UV sets, materials+per-material extensions, IMAGE DIMENSIONS, skin joint NAMES, morph NAMES (--json for a comparable record)
python3 tools/glb_anatomy_probe.py         <path-to>/ink.glb   # bounds, slice widths, named landmarks
python3 tools/glb_animation_probe.py       <path-to>/ink.glb   # clips, durations, driven joints, animated morph channels
```

Must capture: generator string; exact vertex/index/triangle counts and primitive layout; UV set count
and TEXCOORD attributes; the full image list with dimensions, mime type and channel usage; material
count and `extensionsUsed`/`extensionsRequired` (note any `KHR_materials_*` transmission/clearcoat/
sheen/specular and subsurface variants); skin count, joint names, inverse-bind presence, skeleton root
and depth; animation clips, durations and per-channel target counts; morph target names and count;
scene bounds and scale; default/bind pose. Until then the figures in §2 stand as owner-recorded only.

## 8. Provenance log

- 2026-11-18 — located in Dropbox (`/ink.glb`, duplicate `/3d real/ink.glb`); size and SHA-256 read
  from Dropbox metadata. Structure figures entered from the owner's prior inspection (second-hand).
  No bytes inspected in-workspace; DAZ G8F lineage inferred from sibling filenames, not the binary.
