# Avatar geometry evidence: orientation, symmetry and export validity

This document holds the avatar-comparison evidence that used to sit at the top of the README, the items that later README revisions dropped, and what was recomputed on 2026-11-14. Each statement carries one of the project's labels:

- **REAL + VERIFIED** — recomputed in this repository's workspace on 2026-11-14 with the commands in [How to reproduce](#how-to-reproduce).
- **UNVERIFIED** — reported earlier and not recomputed.
- **MISSING** — asked for, searched for, and not found.

## Summary

| Topic | Status | Finding |
|---|---|---|
| Preserved 83 MB LOD0 GLB and its SHA-256 | **REAL + VERIFIED** | Regenerated from source with the recorded size and SHA-256. It is the app's debug-diagnostic avatar (seed 1337). See [Protected artifacts](#protected-artifacts). |
| Exporter: local and remote difference | **REAL + VERIFIED** | The exporter on the remote branch at `a424113` cannot produce the preserved file; the exporter with accessor bounds (`fb0878e`) produces it byte for byte. The earlier workspace therefore held an exporter change that was never pushed. |
| Reference `mylove.glb` identity | **REAL + VERIFIED** | SHA-256 and Dropbox content hash recomputed from the same 9,287,272 bytes. Both recorded identifiers describe one file. |
| Cross-section report | **REAL + VERIFIED** for its measurements | Heights, unique-position counts, PCA angles and all twenty raw spans were recomputed on the two files and match the report. Contour-component counts were not recomputed. |
| Yaw "alignment" of the generated avatar | **REAL + VERIFIED** (correction) | The +6.04720° yaw was not an alignment. The preserved avatar is mirror-symmetric about `X = 0` as stored. The "aligned" columns describe a mis-rotated mesh and must not be used. |
| Facing direction | **REAL + VERIFIED** | Generated avatars face `−Z`. The exporter writes them unchanged, so exported GLBs face the opposite way to the glTF convention (`+Z`). |
| Left/right naming | **REAL + VERIFIED** (defect, not fixed) | Joints and regions named left are on `+X`; `eyeBlinkLeft` moves the eye on `−X`; `mouthSmileLeft` moves `+X`. |
| One-to-one mirror matcher at 4 mm | **REAL + VERIFIED** for the bound, **MISSING** for the exact count | At least 14,320 of 550,098 IDs (2.60%) cannot be matched. The exact maximum-cardinality count has still not been produced. |
| Exported GLB validity | **REAL + VERIFIED** (fixed) | Khronos validator: 542 errors before `fb0878e`, 0 errors and 0 warnings after. |
| Bundled `female.glb` fixture | **REAL + VERIFIED** (not changed) | Khronos validator: 7 errors, all missing bounds on morph-target accessors. |
| "D2 findings", "README merge preview" | **MISSING** | Not present in any branch, pull request or connected storage. See [Items not found](#items-not-found). |

## Protected artifacts

| Artifact | Bytes | SHA-256 | Status |
|---|---:|---|---|
| Generated female LOD0 (`avatar_default_female_LOD0.glb`) | 83,430,312 | `56b072cb43203fe9c2ca0b231d28a3601b59914465647fa1da95a6890daccd6c` | **REAL + VERIFIED** — regenerated from source with this size and hash |
| Reference `mylove.glb` (Dropbox `/mylove.glb`) | 9,287,272 | `8ef94a008b79a2ec8524962d810d92716f03f8476726dd8d8b7fc77227426a26` | **REAL + VERIFIED** — hash computed from the Dropbox copy |

The Dropbox content hash of `mylove.glb`, `388a94e7495e809577356ba6cf9890a13fe38f8cd4c63ba3228109d19a24b398`, was recomputed from the same bytes. It is a different algorithm from SHA-256; the two values belong to the same file.

### What the preserved LOD0 file is

It is the avatar that the app's debug button "Run generated-avatar GLES diagnostic" generates:

| Property | Value |
|---|---|
| Spec | female, athletic build, medium hair, jeans and jacket, no footwear, height 1.75 m, seed 1337 |
| Prompt | `Adult woman, athletic build, 175 cm tall, medium dark hair, black t-shirt and jeans` |
| Mesh | 821,215 vertices, 1,100,168 triangles, 10 submeshes, 71 joints, 52 blendshapes |
| Unique triangle-referenced positions | 550,098 (the figure the cross-section report quotes) |
| Height | 1.814858 m |

Regenerating it on x86_64 Linux (g++ 12.2, `-O2`, zlib 1.2.13):

| Exporter | Bytes | SHA-256 | Matches the preserved file |
|---|---:|---|---|
| Current exporter (accessor bounds, `fb0878e` and later) | 83,430,312 | `56b072cb43203fe9c2ca0b231d28a3601b59914465647fa1da95a6890daccd6c` | **Yes, byte for byte** |
| Exporter as it was on the remote branch at `a424113` | 83,428,008 | `f471f9bb6a4b4f1a61d2dacb5992677e439c237c501766452a6620e09cec693c` | No |

Two consequences:

- The preserved file can be recreated exactly whenever it is needed. It was never committed and does not need to be.
- The file was exported by an exporter that already wrote the accessor bounds. That change existed only in the earlier agent workspace; `fb0878e` puts an exporter with byte-identical output into the repository.

The same spec exported on the Android x86_64 emulator (run [43629991324](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43629991324)) has the same vertex and triangle counts and is 83,430,028 bytes, 284 bytes smaller. The export is therefore not byte-identical between the Linux host and Android. Which bytes differ has not been investigated.

Rules that remain in force:

- `mylove.glb` is private. Do not commit, publish or redistribute it. It was read for measurement only and is not in the repository.
- Nothing in this repository modifies either file.

## Corrected yaw explanation

### What was reported

The cross-section report estimated a PCA major-axis angle for each model in the XZ plane (generated +4.37946°, reference −1.66774°, both modulo 180°) and rotated the generated geometry by +6.04720° about Y to make the two angles agree. The README presented the re-sliced result as the "aligned" measurement.

### Why that is wrong

The PCA was computed over vertices. It is weighted by how many vertices the surface extraction placed on each side, not by where the body is. Measured on the preserved LOD0 file itself:

1. **The avatar is symmetric about `X = 0` as stored.** Turn it by a candidate yaw, mirror every position across `X = 0`, and count how many land in an occupied 1 mm cell or a neighbouring cell:

   | Yaw applied | Preserved LOD0 | Reference `mylove.glb` |
   |---:|---:|---:|
   | −6.0472° | 8.62% | 5.97% |
   | −1.0° | 44.88% | 10.35% |
   | −0.5° | 73.46% | 12.51% |
   | **0.0°** | **92.88%** | **13.75%** |
   | +0.5° | 73.92% | 12.01% |
   | +1.0° | 45.38% | 9.91% |
   | +6.0472° | 9.23% | 5.88% |

   A body that was really turned by 4° to 6° would peak there, not at zero. The reference model is not mirror-symmetric at this tolerance at any angle tested, and its highest value is also at zero, so its −1.668° PCA angle is no evidence that it is turned either.

2. **The "aligned" spans are what any off-axis rotation produces.** Spans are `X-span/H / Z-span/H` from triangle-plane intersections of the preserved LOD0 file:

   | Y/H | As stored (report: "raw") | Rotated +6.0472° (report: "aligned") | Rotated −6.0472° |
   |---:|---|---|---|
   | 0.10 | 0.148075 / 0.058122 | 0.147574 / 0.067592 | 0.147567 / 0.067596 |
   | 0.25 | 0.172452 / 0.086226 | 0.171962 / 0.095557 | 0.171960 / 0.095551 |
   | 0.50 | 0.284834 / 0.176016 | 0.283525 / 0.181012 | 0.283525 / 0.180996 |
   | 0.75 | 0.239902 / 0.140886 | 0.238919 / 0.143031 | 0.238921 / 0.143032 |
   | 0.90 | 0.106442 / 0.139914 | 0.109387 / 0.143800 | 0.109381 / 0.144040 |

   The first two columns equal the report's values to six decimal places. Rotating the opposite way gives the same change: X narrows and Z widens by the same amounts. A rotation that brought a turned body back into line would depend on its sign.

3. **The PCA angle depends on the weighting.** On a second avatar from the same generator (the engine's default female, 766,100 vertices) the vertex-weighted angle is +4.573° and the triangle-area-weighted angle is +0.540°. That avatar has 381,024 vertices at `x < 0` and 385,070 at `x > 0`; the imbalance, not the body, tilts the vertex-weighted axis. An exact nearest-neighbour test at 1 mm on that avatar peaks at yaw 0 as well (68.90%, against 3.2% to 3.4% at ±6.0472°).

### What to use instead

- Treat the generated model's **stored axes** as its correct orientation. The "aligned" generated columns and the "aligned ratio" columns of the report are superseded and must not be used for any comparison or decision.
- The raw columns of the report stand: they were recomputed and match.
- If two models need registering, use a symmetry-plane fit or named landmarks, not a vertex-weighted PCA.

### Facing direction

Four independent cues on the generated avatar agree that it faces `−Z`: the feet extend 91 mm towards `−Z` from the shanks; the eyes, lips and teeth lie on the `−Z` side of the head while the hair centroid lies on `+Z`; and at nose height the `−Z` extreme of the head is a 32 mm wide cap (a nose) while the `+Z` extreme is 188 mm wide (the back of the skull).

The exporter applies no axis conversion: a round trip through export and import returns identical bounds and the same PCA angle. glTF 2.0 defines the front of an asset as `+Z`, so an avatar exported by this engine shows its back in a standard glTF viewer.

The app compensates for its own files. Submeshes that carry engine regions are viewed from `−Z` (camera yaw 180°); foreign GLBs are viewed from `+Z` (camera yaw 0°).

A PCA angle is only defined modulo 180°, so it cannot see this front/back difference. X and Z spans do not change under a 180° turn, so the raw cross-section spans are unaffected. Anything that depends on left versus right is affected, including an overlay or a mirror match between a generated model and a standard glTF reference.

## Mirror symmetry and the unmatched-ID finding

### Preserved LOD0 file, one-to-one matching at 4 mm

The matcher works on the 550,098 unique triangle-referenced positions. A left position (`x < 0`) and a right position (`x > 0`) are a candidate pair when the mirror image of one lies within 4 mm of the other. The 4 mm criterion is the provisional one and was not changed.

| Measure | Value |
|---|---:|
| Left positions (`x < 0`) | 272,619 |
| Right positions (`x > 0`) | 277,479 |
| Positions on the plane | 0 |
| Candidate pairs within 4 mm | 5,916,475 |
| Left positions with no candidate at all | 4,730 |
| Right positions with no candidate at all | 8,079 |
| Largest number of pairs any one-to-one matching can form | 267,889 |
| **IDs that must stay unmatched** | **at least 14,320 (2.60%)** |

What this establishes:

- The earlier report that "maximum-cardinality matching did not match all IDs" is the expected outcome. The two sides have different numbers of positions, and 12,809 positions have no partner within 4 mm on the other side. No matcher can pair them.
- This is a property of the tessellation. It is not proof of anatomical asymmetry: the surface is symmetric about `X = 0` (see the yaw table above).
- The exact size of the maximum matching is still **MISSING**. The earlier run never reported one. Two exact attempts on the 5.9 million candidate pairs were made here and neither finished: a Hopcroft-Karp run was stopped after 900 seconds, and a run of SciPy's `maximum_bipartite_matching` was stopped at its 1,500-second limit. The figure above is a proven lower bound on the unmatched count, not the count.

### Where the differences are

Measured on the engine's default female avatar (766,100 vertices) with a nearest-neighbour test across `X = 0` at 4 mm. This test is many-to-one; it is not the one-to-one matcher.

| Measure | Value |
|---|---:|
| Vertices with a mirror neighbour within 4 mm | 746,741 (97.473%) |
| Vertices without one | 19,359 (2.527%) |
| Mean residual of matched vertices | 0.763 mm |

| Region | Vertices | `x < 0` | `x ≥ 0` | Without a mirror neighbour |
|---|---:|---:|---:|---:|
| skin | 717,462 | 359,085 | 358,377 | 15,369 (2.14%) |
| nail | 8,117 | 3,615 | 4,502 | 3,380 (41.64%) |
| teeth | 21,990 | 8,254 | 13,736 | 1,949 (8.86%) |
| lips | 3,972 | 2,183 | 1,789 | 58 (1.46%) |
| hair | 15,794 | 7,794 | 8,000 | 20 (0.13%) |
| tongue | 5,032 | 3,003 | 2,029 | 0 |
| eye_left | 1,346 | 0 | 1,346 | 0 |
| eye_right | 1,739 | 1,739 | 0 | 0 |

The two sides have different vertex counts in every region, including the two eyes. The nails and teeth are the regions that differ most between sides and are where a geometry review should look first.

### Left and right are not named consistently

| Item | Side it is on |
|---|---|
| Joints `eye_L`, `clavicle_L`, `hand_L`, `foot_L` | `+X` (for example `hand_L` at x = +0.2238 m) |
| Region `eye_left` | `+X` |
| Blendshape `mouthSmileLeft` | moves vertices on `+X` |
| Blendshape `eyeBlinkLeft` | moves vertices on `−X` (the eye that the region table calls `eye_right`) |
| Blendshape `eyeBlinkRight` | moves vertices on `+X` |

In glTF's right-handed frame a figure that faces `−Z` has its anatomical left on `−X`. By that rule the eye-blink channels are anatomically correct and the joints, regions and mouth channels are mirrored; whichever rule is intended, the engine does not follow one rule throughout. Nothing in the app drives blendshapes or retargets joints yet, so this is not visible to a user today. It will produce mirrored expressions or swapped limbs as soon as facial animation, lip-sync or retargeting is connected. It is recorded here as a defect in the generation core and has not been changed.

## Exporter validity

The Khronos glTF validator (`gltf-validator` 2.0.0-dev.3.10) was run before any rendering work.

| File | Errors | Detail |
|---|---:|---|
| Engine export before `fb0878e` | 542 | 30 × `ARRAY_LENGTH_NOT_IN_LIST` (three-element min/max on SCALAR animation-time accessors); 96 × `ANIMATION_SAMPLER_INPUT_ACCESSOR_WITHOUT_BOUNDS` (a consequence); 416 × `MESH_PRIMITIVE_POSITION_ACCESSOR_WITHOUT_BOUNDS` (52 morph targets × 8 primitives) |
| Engine export after `fb0878e` | 0 | 0 warnings; 7 informational notes (the vendor extension `AF_subsurface` cannot be validated; 6 unused materials) |
| Bundled `app/src/main/assets/models/female.glb` | 7 | 7 × `MESH_PRIMITIVE_POSITION_ACCESSOR_WITHOUT_BOUNDS` on its morph targets; 3,014 vertices, 5,520 triangles, no textures, no skin |

The engine's own importer accepted the file both before and after the fix, and the six native regression tests pass. The bundled fixture was left as it is: its provenance is pinned to a donor revision and Filament loads it. Its errors are recorded so that nobody mistakes it for a valid reference asset.

Two further properties of the export are worth knowing and were not changed:

- All eight primitives reference the full vertex pool (766,100 vertices each) and differ only in their index ranges. This is legal, but a loader that copies vertices per primitive will use eight times the memory.
- The export is deterministic: two independent runs produced byte-identical files.

## Preserved cross-section report

The following is the report as it stood in the README at `a424113`. Recomputed on 2026-11-14 from the two files:

| Part of the report | Result of recomputation |
|---|---|
| Step 1, both file identities | **REAL + VERIFIED** |
| Step 2, PCA angles | **REAL + VERIFIED**: generated −175.62054° (exact); reference +178.33233° against the reported +178.33226° |
| Step 2, unique positions | **REAL + VERIFIED**: generated 550,098 (exact); reference 64,915 against the reported 64,914 |
| Step 2, the yaw as an alignment | Superseded by [Corrected yaw explanation](#corrected-yaw-explanation) |
| Step 3, heights | **REAL + VERIFIED** (exact) |
| Step 4, contour-component counts | **UNVERIFIED** (not recomputed) |
| Step 5, raw generated and reference spans | **REAL + VERIFIED** (all twenty values exact) |
| Step 5, "aligned" columns | Reproduced exactly, and superseded: they are the stored mesh rotated off its symmetry plane |
| Step 6, compatibility labels | The report's judgement; not a measurement |

The report uses its own label "MEASURED_WITH_GAPS"; in this project's vocabulary that is **PARTIAL**. Its header date of 2026-11-15 was a day ahead of the commit that added it (2026-11-14). The reference paths it quotes are from the earlier agent workspace.

**Overall avatar-comparison status: MEASURED_WITH_GAPS. No verdict.** This update records the complete supplied aligned cross-section report. Artifact identities, geometry measurements and classifications below are agent-reported; they have not been independently recomputed in this repository workspace. This is not a build, renderer-runtime or phone-verification claim.

#### Step 1 — preserved artifact identities

| Artifact | Reported path | Bytes | SHA-256 | Status |
|---|---|---:|---|---|
| Generated female LOD0 | `/workspace/3d-mega-hub-canonical/exports/avatar_default_female_LOD0.glb` | 83,430,312 | `56b072cb43203fe9c2ca0b231d28a3601b59914465647fa1da95a6890daccd6c` | Report says required size/hash match |
| Reference `mylove.glb` | `/home/ubuntu/upload/eYPzGGq49EUyWbNcDwqwN2/mylove.glb` | 9,287,272 | `8ef94a008b79a2ec8524962d810d92716f03f8476726dd8d8b7fc77227426a26` | Read-only identity recorded |

The reference copy's reported SHA-256 is distinct from the previously recorded Dropbox content hash. Do not conflate the two identifiers. The reference asset is private: do not commit, publish or redistribute it without explicit approval. The generated artifact is preserved and must not be modified.

#### Step 2 — Y-axis alignment

PCA major-axis angles in XZ, modulo 180°:

| Measurement | Generated | Reference |
|---|---:|---:|
| PCA angle | −175.62054° (equivalent +4.37946°) | +178.33226° (equivalent −1.66774°) |
| Unique triangle-referenced world-space vertices used | 550,098 | 64,914 |

A relative **+6.04720° yaw about Y** was applied to generated geometry; reference orientation was fixed. Rotation convention:

`x′ = cos(θ)·x + sin(θ)·z`
`z′ = −sin(θ)·x + cos(θ)·z`

This is a whole-mesh, pose-sensitive PCA estimate, not independently validated anatomical registration. The raw-axis baseline is retained in Step 5; aligned measurements re-slice the generated geometry after rotation.

#### Step 3 — common height definition

Both models use the same definition: full-scene world-space `Ymax − Ymin` after node transforms. Each slice is measured upward from that model's own `Ymin`. No scale or vertical registration was applied.

| Model | Y minimum | Y maximum | H |
|---|---:|---:|---:|
| Generated | 0.000000 m | 1.814858 m | 1.814858 m |
| Reference | −0.499926 m | 0.499917 m | 0.999842 m |

All reported widths and depths are dimensionless spans divided by that model's own `H`. Slice planes use `Y = Ymin + fraction × H`.

#### Step 4 — torso-only analysis

**PARTIAL** (the source report labelled this step "INCOMPLETE")

No defensible limb mask could be derived from mesh geometry alone. Contour-component counts at normalized heights were:

| Y/H | Generated components | Reference components |
|---:|---:|---:|
| 0.10 | 2 | 26 |
| 0.25 | 2 | 41 |
| 0.50 | 1 | 25 |
| 0.75 | 1 | 103 |
| 0.90 | 9 | 35 |

A largest-contour or central-contour rule would not reliably separate torso from limbs. No proxy mask was substituted. Therefore, these results are whole-mesh cross-sections, not torso-only measurements.

#### Step 5 — complete whole-mesh cross-section measurements

Each pair below is `X-span/H / Z-span/H`. Ratios are **generated ÷ reference**. Reference values remain unchanged by the generated-model yaw.

| Y/H | Raw generated X/Z | Reference X/Z | Raw ratio X/Z | Aligned generated X/Z | Aligned ratio X/Z |
|---:|---:|---:|---:|---:|---:|
| 0.10 | 0.148075 / 0.058122 | 0.400646 / 0.234685 | 0.369591 / 0.247660 | 0.147574 / 0.067592 | 0.368340 / 0.288012 |
| 0.25 | 0.172452 / 0.086226 | 0.457947 / 0.197762 | 0.376576 / 0.436009 | 0.171962 / 0.095557 | 0.375506 / 0.483192 |
| 0.50 | 0.284834 / 0.176016 | 0.544902 / 0.143478 | 0.522725 / 1.226780 | 0.283525 / 0.181012 | 0.520323 / 1.261601 |
| 0.75 | 0.239902 / 0.140886 | 0.628299 / 0.209534 | 0.381828 / 0.672378 | 0.238919 / 0.143031 | 0.380263 / 0.682615 |
| 0.90 | 0.106442 / 0.139914 | 0.186629 / 0.154751 | 0.570340 / 0.904123 | 0.109387 / 0.143800 | 0.586120 / 0.929235 |

These are descriptive geometric spans. They do not establish named anatomy, anatomical similarity, or a pass/fail threshold.

#### Step 6 — residual compatibility, not a verdict

The supplied report classifies the remaining differences as follows:

| Y/H | Reported compatibility | Limitation |
|---:|---|---|
| 0.10 | Compatible with height-registration residuals | Cause not established |
| 0.25 | Compatible with height-registration residuals | Cause not established |
| 0.50 | Unresolved between residual yaw and height registration | Cannot distinguish causes from these measurements |
| 0.75 | Compatible with height-registration residuals | Cause not established |
| 0.90 | Compatible with height registration; residual yaw not ruled out | Cause not established |

No cause is established and no pass/fail is assigned. In particular, the PCA yaw is not proof that the remaining differences are anatomical or that the models are correctly anatomically registered.

#### Separate mirror-matching boundary

The mirror matcher remains a separate analysis. Previously reported maximum-cardinality matching did not match all IDs; this is not proof of anatomical asymmetry. Keep the provisional 4 mm criterion unchanged. It is not a threshold derived from this cross-section report and must not be loosened or treated as a substitute for anatomical validation.

#### Evidence boundaries and next actions

- Preserve the generated LOD0 GLB exactly; do not alter or replace the recorded artifact.
- Keep `mylove.glb` private and read-only.
- Do not claim torso-only measurements unless a defensible, documented mask or independent anatomical landmarks become available.
- Do not turn compatibility labels into causal findings or an anatomical verdict.
- Keep mirror matching and its provisional 4 mm criterion separate from the cross-section analysis.
- No new Android build, emulator runtime test, or physical-device test is established by this report. Samsung SM-S921B verification remains **UNVERIFIED** unless the exact APK is installed and tested on the phone.

**Status: MEASURED_WITH_GAPS — no verdict.**

### Items restored from earlier README revisions

These were removed by later README edits and are restored from git so that no evidence is lost.

From `06aeb67` (2026-11-14 handover, email evidence only):

- The default female LOD0 GLB export was reported unchanged and validated.
- The maximum-cardinality one-to-one mirror matcher was still running at 100% CPU and about 1.1 GiB resident memory, with no error and no output. It had not produced a mirror result or a LOD verdict.
- The actual app-generated avatar GLB was missing from that agent's workspace. Seed 1337 was identified there as a bad test fixture that must not be compared with `mylove.glb` as if it were the real generated avatar.
- The five-project donor audit remained **PARTIAL**: no donor was a ready-to-integrate photorealistic animated-avatar engine, no donor code was copied or merged, and no Android device or photo-reference test was performed.

From `d0d53b6`:

- The earlier descriptive whole-mesh normalized X/Z spans were generated **0.3085 / 0.2101** and reference **0.7996 / 0.3579**.
- Height registration uses full-scene Y extent and does not establish equivalent anatomical landmarks.

The photo-overlay quality backlog, including the ten-band reference envelope, the symmetry gate and the five-project audit notes, is in [`AVATAR_PHOTO_OVERLAY_QUALITY_TEST_BACKLOG.md`](AVATAR_PHOTO_OVERLAY_QUALITY_TEST_BACKLOG.md). It was merged from `test/photo-realistic-avatar-overlay` with its history.

## Items not found

| Item | Where it was looked for | Result |
|---|---|---|
| "D2 findings" | Every revision of every Markdown file on all 16 remote branches; all 11 pull request descriptions and comments; Dropbox search; the Dropbox notes `FINDINGS.md`, `VERIFICATION_RESULTS.md` and `RENDERER_AUDIT.md` (2026-10-30, AvatarForge project, findings numbered `AF-…`) | **MISSING**. The only match for "D2" in the history is inside the word "LOD2". |
| "README merge preview" | Same places | **MISSING** under that name. Two README changes existed outside the working branch, and both are reconciled: the one on `test/photo-realistic-avatar-overlay` (merged without conflict in `6d0194b`) and the integration branch's update of 2026-11-14 18:22–18:32 UTC (merged in `79fb562`; its content is quoted in [PROJECT_STATUS.md](../PROJECT_STATUS.md)). Whether either is the preview that was meant is **UNVERIFIED**. |
| Output of the earlier one-to-one mirror matcher | Same places | **MISSING**. The bound in this document was computed afresh. |

Google Drive was searched on 2026-11-14, after its connection had failed authentication on the earlier attempts: by file name ("D2", "merge preview", "README", "mega hub", "AvatarForge", "findings") and by full text ("D2", "D2 findings", "merge preview", "AvatarForge", "3d-mega-hub", "Mega Hub", "nCreateBuilder"). No document matched. The full-text index does not cover text inside images. If any of these items exist in the earlier agent workspace or somewhere else, they need to be supplied before they can be reconciled.

## How to reproduce

```bash
# Host build of the engine, the regression tests and the probe (x86_64 Linux, CMake 3.22+, zlib)
cmake -S avatarforge-engine/src/main/cpp -B build/host -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release -DAF_OPT_FLAGS=-O2
cmake --build build/host --parallel 2
ctest --test-dir build/host --output-on-failure

# Recreate the preserved LOD0 file and check its identity
build/host/af_host_probe --sex female --build 2 --hair 3 --outfit 4 --height 1.75 \
  --prompt "Adult woman, athletic build, 175 cm tall, medium dark hair, black t-shirt and jeans" \
  --out /tmp/avatar_default_female_LOD0.glb
sha256sum /tmp/avatar_default_female_LOD0.glb   # 56b072cb...daccd6c, 83,430,312 bytes

# Cross-sections, PCA, symmetry-plane test and matcher bound (numpy; scipy for --candidates)
python3 tools/glb_symmetry_sections.py /tmp/avatar_default_female_LOD0.glb --mirror --candidates 0.004
python3 tools/glb_symmetry_sections.py /path/to/mylove.glb --mirror

# The engine's default female avatar, with region and orientation measurements
build/host/af_host_probe --sex female --out /tmp/default_female.glb
build/host/af_host_probe --analyze /tmp/default_female.glb --extended

# Khronos validator
npm install gltf-validator
node -e "require('gltf-validator').validateBytes(new Uint8Array(require('fs').readFileSync(process.argv[1]))).then(r => console.log(r.issues))" /tmp/default_female.glb
```

Environment of the measurements above: x86_64 Linux, g++ 12.2, `-O2`, zlib 1.2.13, two cores. Generating the preserved avatar took 121 seconds; the default female took 51 to 59 seconds.
