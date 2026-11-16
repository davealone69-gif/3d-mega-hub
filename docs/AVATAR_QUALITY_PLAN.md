# Avatar quality plan and reference asset

This is the avatar-quality plan and the owner-supplied reference data that used to sit in the README. The text is preserved as written on 2026-11-13 and 2026-11-14; only the status words were mapped onto the project's five labels (**REAL + VERIFIED**, **UNVERIFIED**, **PARTIAL**, **MISSING**, **BLOCKED**).

Two later findings change how parts of this plan should be read:

- The yaw "alignment" used in the cross-section comparison was not an alignment, and generated avatars face `−Z`. See [AVATAR_GEOMETRY_EVIDENCE.md](AVATAR_GEOMETRY_EVIDENCE.md) before comparing a generated model with `mylove.glb`.
- An avatar that is generated, exported and then shown in the app is rendered as a **static mesh**: the importer does not apply the exported skeleton, animation clips or blendshapes. Appearance can be judged; posing and facial animation cannot.

**Priority correction (2026-11-13):** the owner has now physically tested the latest APK and reports that the renderer displays avatars, the generation button changes avatar features (including male/female), and GLB import works. Skin-tone and hair changes have not yet been tested. The owner also reports that the avatar quality is currently too poor to judge appearance changes reliably.

These are owner-tested results for the APK the owner installed. The exact APK SHA-256 and source commit used for that phone test have not been supplied, so do not attribute the result to a specific repository head or call the current PR head device-verified.

The next implementation priority is **avatar visual quality**, not another renderer rebuild. Preserve the working native GLES renderer, prompt generation path and GLB import. Change them only when a reproducible defect is demonstrated.

### User reference asset: `mylove.glb`

The owner has already supplied `mylove.glb` as the reference model for improving the generated avatar. Do not ask for another upload while this source remains available.

- **Dropbox source:** `/mylove.glb`
- **Known file size:** 9,287,272 bytes (8.86 MiB)
- **Dropbox content hash:** `388a94e7495e809577356ba6cf9890a13fe38f8cd4c63ba3228109d19a24b398`
- **Second known copy:** `/3d-mega-hub-diagnostics/3d-mega-hub/app/src/main/assets/mylove.glb` (same reported size)
- **Reference role:** ground truth for measurable proportions, silhouette, anatomy, mesh density, material/texture characteristics, rig and morph structure where those features exist.
- **Inspection status: PARTIAL, with owner-supplied structural and measurement results.** The owner reports running the GLB inspection script on the exact 9,287,272-byte file. The measurements below are owner-supplied and have not yet been independently reproduced from the binary in this workspace.
- **GLB structure (owner-reported):** GLB v2; one static mesh; 163,756 vertices; 144,073 triangles; UVs and normals present; no skeleton, skin or animation; two embedded 2048x2048 WebP images; no normal map.
- **Bounds (owner-reported, model units):** X [-0.3994, 0.4000], Y [-0.4999, 0.4999], Z [-0.1793, 0.1786], dimensions approximately 0.799 x 1.000 x 0.358. Y height is 1 model unit; this does not establish real-world scale or semantic anatomy landmarks.
- **Vertex counts across 10 equal bins (owner-reported):** X: [313, 1817, 10624, 29610, 47290, 43211, 19246, 9929, 1369, 347]; Y: [18678, 9275, 11631, 10829, 8337, 7905, 10077, 41734, 36938, 8352]; Z: [1988, 1976, 2228, 3142, 16543, 52962, 34696, 18166, 24979, 7076]. These are vertex-distribution counts, not named anatomical regions. Do not label bins as shoulders, hips or limbs without slice geometry/landmark evidence.
- **Materials/textures (owner-reported):** a dark red/tan base-colour WebP and a mottled teal/green metallic-roughness WebP; material `metallicFactor` is 1.0. In glTF metallic-roughness, the texture's green channel modulates roughness and blue channel modulates metallic, each multiplied by its corresponding scalar factor. The effective metallic value therefore cannot be inferred from the scalar alone; decoded blue-channel statistics are still needed. No normal map is present.
- **Generator comparison from source inspection:** AvatarForge's procedural skin material is already authored with metallic 0.00 and roughness 0.52. Do not change this on the basis of the reference scalar alone. The reference material's effective metalness and visual response must be checked using its decoded blue channel and a controlled render.
- **Transfer limitation:** the available Dropbox text-fetch action has a 5 MiB maximum and cannot read this 9.29 MB binary. Dropbox's download-link action exposes a temporary URL but does not return file bytes to the analysis workspace; the direct transfer attempt was rejected by the runtime URL-access gate. The linked-chat attachment itself was not surfaced as a readable file in the current conversation/library search.
- **Next measurement required:** obtain horizontal cross-section extents (X width and Z depth at normalized Y heights), preferably for 10 equal Y slices, plus decoded image channel statistics and material-to-texture slot mapping. Compare those profiles against a generated avatar captured with the same coordinate convention. The current repository probe reports five descriptive slices and named landmarks only when node names exist; it does not identify anatomy from unnamed vertices.
- **Use rule:** derive a versioned reference profile and regression targets from measured data. Do not blindly replace the procedural generator with the reference mesh, and do not commit or redistribute the private reference asset without explicit approval.

### Stage 1: establish and improve avatar quality

1. Capture reproducible before-images and exported GLBs for controlled male and female prompts. Record the source commit, prompt, seed, model dimensions, triangle/vertex counts, material assignments, texture dimensions and GLB SHA-256.
2. Trace the full generation path from parsed prompt -> `af_spec` -> anthropometry/body field -> mesh extraction/refinement -> hair/face/clothing regions -> material/texture bake -> GLB export. Do not assume a parser or renderer issue from appearance alone.
3. Diagnose the visible quality defects by category: body silhouette/proportions, head and facial anatomy, eyes/mouth/ears, hands/feet, hair geometry, clothing construction, topology/normals, UVs/textures, material response and lighting. Separate geometry defects from material/rendering defects.
4. Prioritise the highest-impact geometry and silhouette defects first. A shader/lighting polish must not be used to disguise weak geometry. Avoid wholesale replacement of the procedural generator.
5. Add deterministic native regression tests for male/female baselines, spec values, mesh validity, region/material coverage, bounds, finite normals/UVs, and export/import round-trip. Tests must assert actual generated data, not just non-empty output.
6. Make small, measurable changes and preserve before/after GLB plus rendered captures. Compare under identical camera, lighting, renderer, seed and output resolution.
7. Rebuild and run CI on the exact branch head. The owner must install and test the resulting APK before the improved quality is called phone-verified.

### Stage 2: verify appearance controls

Only once the base model is good enough to judge, verify each control independently:
- skin tone: light, medium and dark prompts, checking parsed melanin/spec values, exported material/texture values and visible output;
- hair: style, length and colour, checking both parser output and actual exported geometry/materials;
- clothing colour/style and other appearance attributes;
- male/female differences under matched stature/build prompts.

Record unsupported prompt tokens honestly. Deterministic keyword parsing is not an embedded LLM and must not be presented as general natural-language understanding.

### Stage 3: end-to-end asset workflow

After quality and appearance controls are stable, verify Generate -> Save/Export -> close/reopen -> import the exact generated GLB -> render. Preserve the existing working import path and test malformed/oversized input separately.

### Owner-tested evidence ledger

| Item | Status | Evidence / limitation |
|---|---|---|
| Avatar visible in renderer on owner's phone | **PARTIAL** (owner-tested) | Owner reports latest installed APK displays the avatar; exact APK SHA-256/source commit not supplied |
| Generation button changes avatar features | **PARTIAL** (owner-tested) | Owner reports prompt-driven feature changes, including male/female |
| GLB import | **PARTIAL** (owner-tested) | Owner reports import button works; complete file/lifecycle matrix remains unverified |
| Skin-tone changes | **UNVERIFIED** (not yet tested) | Do not claim verified |
| Hair style/colour changes | **UNVERIFIED** (not yet tested) | Do not claim verified |
| Avatar visual quality | **PARTIAL** (needs improvement) | Owner reports quality too poor to judge appearance changes reliably |
| Exact current PR-head phone verification | **UNVERIFIED** | Need identify tested APK commit and SHA-256, then test exact candidate |
| Native/CI regression and release gates | See [README](../README.md) | Exact-head build and test evidence is in the README; production signing is **BLOCKED** on the owner keystore |

Do not mark avatar quality improved without matched before/after GLB and rendered-image comparisons. Do not mark a new APK phone-verified until the owner tests that exact artifact.
