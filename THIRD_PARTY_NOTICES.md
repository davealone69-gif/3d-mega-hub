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
- Matching Git blob ID at the source and in this repository: `a1c00c6b31f6ab0db3b2e68243526cb2d0e15967`
- The source repository README at that exact revision states that its generated base rigs are original work released under CC0.
- Structural inspection of the bundled file: one mesh, one node, seven morph targets, zero skins, zero animations and zero embedded images.
- This file is an unchanged renderer fixture. It is not evidence of a complete rigged/animated avatar, and the CC0 statement applies to this asset, not to the 3DDD application source.

## Animation fixtures (Khronos glTF Sample Assets)

Three files under `app/src/main/assets/models/animation/`. They are the reference assets for the
animation core: the host suite (`animation_system_regression`) loads them by path, and
`AF_ANIMATION_TEST_ASSETS` defaults to this directory.

They live in `src/main/assets`, not `src/test/resources`, so they ARE packaged into the distributed
APK (596 KB combined). That is why their licences are recorded here rather than in a test README: the
attribution obligations below apply to the shipped artifact, and CC BY 4.0 requires attribution.

Structural facts were measured with `tools/glb_animation_probe.py`, not copied from a description, so
a future replacement of any file can be checked against them.

### Fox.glb

- File: `app/src/main/assets/models/animation/Fox.glb`
- Size: 162,852 bytes
- SHA-256: `d97044e701822bac5a62696459b27d7b375aada5de8574ed4362edbba94771f7`
- Source: Khronos glTF Sample Assets repository, https://github.com/KhronosGroup/glTF-Sample-Assets
- Model: (c) 2014 PixelMannen, CC0 1.0
- Rig and animation: (c) 2014 tomkranes, CC BY 4.0 -- https://creativecommons.org/licenses/by/4.0/
- Two separate rights holders with two separate licences on one file. The CC BY 4.0 attribution
  obligation covers the rig and animation, which is the part this project actually exercises.
- Measured structure: 26 nodes, 1 mesh, 1 (unnamed) skin with 24 joints, depth 8, root `_rootJoint`,
  inverse bind matrices present, 3 animations (`Survey` 3.417 s, `Walk` 0.708 s, `Run` 1.158 s),
  0 morph targets, 1,728 vertices, u16 JOINTS_0, embedded PNG texture.
- Units are CENTIMETRES (hip height 42.94), unlike CesiumMan below. Retargeting between the two is
  where the core's translation-scale rule is observable.

### CesiumMan.glb

- File: `app/src/main/assets/models/animation/CesiumMan.glb`
- Size: 438,044 bytes
- SHA-256: `b7001eaeea8254bd44773bcd247e78696d94169388fbb2a1800fc69434e777d9`
- Source: Khronos glTF Sample Assets repository (Cesium contributes it there)
- License: (c) 2017 Cesium, CC BY 4.0 -- https://creativecommons.org/licenses/by/4.0/
- ALSO covered by Cesium's Trademark and Logo legal mark. Upstream ships this as a separate
  `LICENSES/LicenseRef-LegalMark-Cesium.txt` alongside the CC BY 4.0 text. The legal mark restricts
  use of Cesium's name and marks; it does not add a restriction on the model data itself, but it is
  part of the upstream licence set and is recorded here rather than dropped.
- Upstream flags this asset as a non-Khronos "issue" model, meaning it is retained to reproduce a
  reported conformance problem rather than as a showcase asset. That suits this project: it is a
  deliberately awkward input.
- Measured structure: 22 nodes, 1 skin named `Armature` with 19 joints, depth 7, root
  `Skeleton_torso_joint_1`, 1 animation (`<unnamed animation 0>`, 2.000 s, 57 channels, LINEAR),
  3,273 vertices, embedded JPEG texture.
- Resolves 16 of the 17 humanoid roles: it has NO head joint, and its arm ordering comes from the
  hierarchy rather than from numeric suffixes. Both properties are why it is a useful retarget target.
- Units are METRES (hip height 0.679).

### AnimatedMorphCube.glb

- File: `app/src/main/assets/models/animation/AnimatedMorphCube.glb`
- Size: 6,752 bytes
- SHA-256: `214ee56160a50dbf22543a1d66dbf860986e87f0efac3d89feac1359d0e6aeab`
- Source: Khronos glTF Sample Assets repository
- License: (c) 2017 Microsoft/Public, CC0 1.0 -- https://creativecommons.org/publicdomain/zero/1.0/
- Measured structure: 1 node, 0 skins, 1 animation (`Square`, 4.200 s, 1 channel with path `weights`),
  2 morph targets, 127 keys, output accessor 254 SCALAR floats.
- Has NO skeleton at all. It is kept precisely for that: it proves the player drives a morph-only clip
  instead of failing, and `tools/glb_animation_probe.py --require-skin` fails on it by design.
- glTF morph `weights` accessors are SCALAR, not VECn. This file is the fixture that caught a reader
  which assumed a width of 4 and therefore silently dropped every morph and translation channel.

### CliplessRigged.glb (synthetic, generated in this repository)

- File: `app/src/main/assets/models/animation/CliplessRigged.glb`
- Size: 4,164 bytes
- SHA-256: `3106cf34fb87d5ab13471185b101db7b4179d5db5f59c970213f4dc66c541555`
- Source: **no third party.** Written by `tools/make_clipless_rigged_glb.py`, which is committed
  alongside it and produces byte-identical output on every run (verified: regenerating leaves
  `git diff` empty). There is no external licence attached because nothing external is in it.
- Measured structure: 18 nodes, 1 mesh (8 vertices / 12 triangles), 1 skin with 17 canonically named
  joints, depth 5, root `Hips`, `inverseBindMatrices` present, **0 animations**, 0 morph targets.
- Why it exists: `female.glb` covers "no skin, cannot be deformed". Nothing covered the different case
  of "fully rigged, deformable, but carries no clips" — which is what `mylove_rig_prepared.glb` (20
  joints, 17/17 humanoid roles, 0 animations) is. That file is 13 MB and lives on another branch, so it
  cannot be a fixture. This one is 4 KB and can.
- Four of its eight vertices are rigidly bound to `LeftLowerArm` and four to `Hips`. That split is what
  lets `imported_animation_test` prove the skinning is genuinely per-joint: a clip that rotates only the
  arm must move the arm's vertices and leave the hips' vertices exactly where the bind pose put them. A
  global transform faking motion would fail that check while passing every "vertices moved" check.
