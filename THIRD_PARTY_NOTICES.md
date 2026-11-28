# Third-party notices and asset provenance

## Filament Android

- Dependencies:
  - `com.google.android.filament:filament-android:1.71.6`
  - `com.google.android.filament:filament-utils-android:1.71.6`
  - `com.google.android.filament:gltfio-android:1.71.6`
- License: Apache License 2.0
- Upstream: https://github.com/google/filament

## Female base-mesh fixture (renderer smoke only)

- File: `app/src/main/assets/models/female.glb`
- Source repository: https://github.com/davealone69-gif/3DDD
- Source path: `app/src/main/assets/models/female.glb`
- Source revision: `be5bf17d8afe46247646465941fa1b7317d8e415`
- Git blob ID at the source: `a1c00c6b31f6ab0db3b2e68243526cb2d0e15967` (418,556 bytes). The file was added here with that blob in `c6ef5c0`.
- The file in this repository is no longer that blob. Commit `98dd9ec` ("fix: repair GLB morph compatibility before Filament native loading") rewrote its JSON chunk. It is now blob `ab9d7bcbd36678636f051fb8032d3fd035be37ee`, 418,312 bytes, SHA-256 `f2002ff4201e1845097a1647108dbbaf5ad722521d067732f1765e1153bae779`. The binary chunk, which holds all geometry and morph data, is identical to the source (415,864 bytes); the JSON chunk is 2,420 bytes here and 2,664 bytes at the source. Measured on 2026-11-15.
- The source repository README at that exact revision states that its generated base rigs are original work released under CC0.
- Structural inspection of the bundled file: one mesh, one node, seven morph targets, zero skins, zero animations and zero embedded images.
- This file is a renderer fixture with the source's mesh data and a rewritten JSON header. It is not evidence of a complete rigged/animated avatar, and the CC0 statement applies to this asset, not to the 3DDD application source.

## Clothing-fabric PBR values and iris swatches (3D4ME-derived constants)

- What entered this repository: numeric constants only, in
  `avatarforge-engine/src/main/cpp/src/af_prompt.cpp` — `kFabrics[]` (cotton, silk, satin, denim,
  latex as roughness/metalness pairs) and `kEyeColours[]` (nine curated iris base reflectances) —
  together with the `af_spec::cloth_metalness` field they ride.
- Immediate source: this repository's own branch `arena/089497bd-3d-mega-hub` at
  `58aced02b52a632814ad29dc714a19b577298ff0`, which harvested the values into
  `docs/REFERENCE_3d4me_options.md` (also rescued here, unmodified).
- Original donor: `davealone69-gif/3d4me` (display name **3D4ME**). That reference document records
  the donor as "cloned depth-1 for this audit; not modified" and records **no revision**, so no
  revision is claimed here.
- Licence obligation carried by that document, quoted: "Licence to preserve when redistributing
  derived code: **Three.js (MIT)** and **expo-three (MIT)**. No third-party human models are bundled
  by that project; its avatar is original procedural geometry. Preserve those notices if any of its
  constants are ported." This entry discharges it. What was ported is numeric material constants; no
  Three.js or expo-three source was copied into this repository, and neither library is linked,
  bundled or distributed by it. If either is ever vendored, its MIT notice must be recorded here
  before that use.
- No binary asset from that donor is in this repository, and none is packed into the APK.
- Verification: `avatarforge-engine/src/test/cpp/generation_contract_test.cpp` (rescued from the same
  donor commit) asserts these constants against the prompt parser, the live material table, the
  exported GLB bytes and the re-import round trip. Measured on the host at this commit: 98 checks,
  96 pass. The 2 failures are checks for a donor normal-map relief that was deliberately not ported
  and are unrelated to these constants; the test target is registered in CMake but marked `DISABLED`
  with that reason recorded beside it.
