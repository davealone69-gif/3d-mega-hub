# 3D Mega Hub

One Android app that generates, imports and renders 3D avatars on the phone. The AvatarForge engine generates and imports; a native OpenGL ES renderer draws; Filament shows the bundled sample.

**Handover of 2026-11-15 (UTC). PR #12 remains the working review path and is not merged to `main`. The owner has now installed and tested the new APK on a Samsung SM-S921B: general app operation is reported good; avatar appearance and animation capability remain unfinished.**

## Avatar quality implementation handover — procedural hair

**Work branch:** `integration/avatarforge-renderer-port`  
**Implementation commits:**
- `3960f4d32caec799435edf6beb6a03ff29b8916b` — replace the two smooth braid ropes with deterministic three-strand interlocking tapered geometry.
- `09b87e33f7e459eba212c4144db254c7b62c473d` — replace the flat hairline cut with a curved temple-recession / widow-peak profile; add low-amplitude coil-scale surface variation to the Afro field.
- `8370967eb9248ad298e693a3357c00d1dfa0b91e` — add native regression coverage sampling all nine selectable procedural hair styles for finite, non-empty geometry.
- `f6480cefd96ab094fc2c792111bdb617c8924463` — add four shallow clump channels to medium/long rear hair, sized for the 8 mm extraction lattice.
- `f6882b16633331e30f411fb1bc2946a8008d8407` — bake a root-to-tip pigment gradient separately from the hair material factor; preserve custom `hair_tone` in GLB export; add native tests for default/custom colour and gradient direction.

**Status**
- Braid silhouette geometry, curved hairline, Afro coil-scale field, long-hair clump channels, and root-to-tip pigment gradient: **IMPLEMENTED IN SOURCE; FINAL-HEAD CI PENDING**.
- Nine-style finite/non-empty field regression and hair tone / gradient helper regression: **IMPLEMENTED IN SOURCE; FINAL-HEAD CI PENDING**.
- Existing GLES Kajiya–Kay hair specular path: **PRESENT IN SOURCE**; its strand direction remains a world-up approximation rather than groom-authored per-strand vectors.
- Shared scalp-cap socket asset, fitted hairline baby-hair cards, strand-card shells, flyaways, shared strand atlas/mask channels, alpha-to-coverage/depth-prepass profiling, and dedicated three-level hair LODs: **NOT IMPLEMENTED**.
- Visual validation, triangle/card budgets, overdraw/GPU timings, APK build and S24 runtime: **UNVERIFIED**.

The implementation remains in the existing AvatarForge signed-distance-field, texture-bake and GLB export pipeline. It does not replace existing presets or add mock assets. The braid, coil and clump geometry is deterministic and field-evaluated. The native regression samples all nine selectable hair IDs for finite, non-empty geometry; it is not a visual-quality test. The colour regression verifies natural/custom material factors and root-to-tip gradient direction; it does not substitute for visual inspection.

Every README update changes the branch head. Collect all final verification against the resulting exact head, and do not reuse older workflow results as exact-head evidence.

### How those commits were carried onto `arena/1a5b3043-3d-mega-hub` (PR #12)

The section above is the integration branch's own text, merged unchanged. This branch had already replaced the hair code those commits edit (the hairline is a height for each direction round the head, the hanging styles are a hollow bell and a sheet down the back, the plaits are laid over the shoulders), so the merge conflicted in `af_field.cpp`, `af_glb.cpp` and `af_texture.cpp`. Each commit was carried over by hand:

| Integration commit | On this branch | State |
|---|---|---|
| `3960f4d` three-strand braids | **Not taken.** Measured in this branch's body field, the straight guide line of those plaits is inside the body from the base of the neck downwards and 7 cm under the surface at its lower end (female and male, 1.70 m), so the lower half of each plait would be buried in the trunk. The plaits here stay the bead chains that rest on the neck, collarbone and chest. A new style, box braids (`AF_HAIR_BOX_BRAIDS`), divides the hanging hair into ropes instead | field measured; host render of this branch's plaits and of box braids looked at |
| `09b87e3` hairline | **Ported** into the hairline by direction: widow's point 1.4 % and temple recession 2.4 % of the head's height, the integration branch's values | host render before and after looked at |
| `09b87e3` Afro coil field | **Ported** onto this branch's Afro mass, at 3.0 mm instead of 1.6 mm: at 1.6 mm a render at the app's quality showed no difference at all. The same field at 4.2 mm gives the new curly style its outline | host render looked at |
| `8370967` hairstyle regression | **Merged**, and extended from nine styles to thirteen | host `ctest` passes |
| `f6480ce` clump channels | **Ported** onto the back of the bell and the sheet down the back. The first version returned `1e9` for "no channel here", and a smooth maximum against `1e9` rounds the other operand away and returns exactly zero: the hair came out as blocks. "Far" is now one metre | host render looked at |
| `f6882b1` root-to-tip colour, hair colour in the GLB | **Ported** as a multiplier inside the baked colour. On this branch the base colour texture carries the colour once and the material factor is white, so the chosen hair colour already reached the GLB; the factor carries it only for a file without a texture. Both helper functions and their tests are merged | host `ctest` passes |

"Looked at" means a host software render at the app's quality, seen by the author of this README. None of it has been seen on a phone.


## Single source of truth

**Start here:** [Avatar assets and animation master document](#avatar-assets-and-animation-master-document-2026-11-15), then [`docs/AVATAR_ASSETS_ANIMATION_MASTER.html`](docs/AVATAR_ASSETS_ANIMATION_MASTER.html).

| If you want | Read |
|---|---|
| The machine described for an agent who will work on it: parts, pipeline, build, tests, measured numbers, mistakes already made | [`AGENTS.md`](AGENTS.md) |
| The known faults with their repairs, the route to photoreal avatars, the recommended parts and the performance plan for the Galaxy S24 | [`docs/PHOTOREAL_UPGRADE_PLAN.md`](docs/PHOTOREAL_UPGRADE_PLAN.md). A recommendation: nothing in it is built yet |
| What was verified, where and when | this file, from here down |

## Where the work is

| Item | Value |
|---|---|
| Working branch | [`arena/1a5b3043-3d-mega-hub`](https://github.com/davealone69-gif/3d-mega-hub/tree/arena/1a5b3043-3d-mega-hub) |
| Review path | [PR #12](https://github.com/davealone69-gif/3d-mega-hub/pull/12) into `integration/avatarforge-renderer-port`, then [PR #9](https://github.com/davealone69-gif/3d-mega-hub/pull/9) into `main` |
| Current code head | `2920265ae822d08f6508e8eac917a9817e232e1e` (`2920265`). All five workflows passed on it and its phone-test APK is published; see [Verification results](#verification-results). It merges the integration branch at `13117dc` and adds the choices of the owner's picture reference to the creator. The commit after it adds only documents. Measurements further down that name `3facc3d`, `797d156`, `041e31d` or `a5e93a8` are from those older commits |
| README update | Updated after the owner's third message of 2026-11-15: three screenshots of a picture creator and a folder of body references, with the words "These are very important". The documents-only commit does not change app code or the tested APK. A second documents commit answers the owner's fourth message ("describe this machine ... repairs ... how we can get to 3d photo realistic avatars") with `AGENTS.md` and `docs/PHOTOREAL_UPGRADE_PLAN.md`, and corrects two stale sentences about imported models in `BUILD_AND_VERIFY.md` and `PROJECT_STATUS.md` |
| Record for the head commit | In the description of [PR #12](https://github.com/davealone69-gif/3d-mega-hub/pull/12). A commit cannot contain the results of its own workflow runs, so the record for the head, with the APK to install, is kept there |
| `main` | `aaee4443dbf53ef4707cce01d99f71a59e903ec7`, not modified. No force-push anywhere |

The rule is one implementation line, `integration/avatarforge-renderer-port`. This handover was prepared on the working branch above, which contains the integration branch up to `13117dc`. Once PR #12 has landed, the integration branch is the single line again. Do not open another implementation branch.

Status labels: **REAL + VERIFIED**, **IMPLEMENTED IN SOURCE** (written, built and tested by CI, never exercised by a person on a device), **UNVERIFIED**, **PARTIAL**, **MISSING**, **BLOCKED**. "Verified on the emulator" never means verified on the phone.

## Avatar assets and animation master document (2026-11-15)

**The single inventory is [`docs/AVATAR_ASSETS_ANIMATION_MASTER.html`](docs/AVATAR_ASSETS_ANIMATION_MASTER.html).** It is one self-contained HTML file with a clickable table of contents, collapsible sections, a search box over every table, status filters and links to the exact source lines. GitHub shows HTML files as source, so open it from a checkout or download it and open it in a browser. Its evidence images are in [`docs/evidence/`](docs/evidence/). It is generated by `tools/avatar_master_doc/build_doc.py` from the source tree and from the evidence record `doc_extra.json` beside it, so its source links can be refreshed after the code changes.

| Item | Value |
|---|---|
| Code the document describes | `2920265ae822d08f6508e8eac917a9817e232e1e` on `arena/1a5b3043-3d-mega-hub`. Source links in the document are permanent links to that commit |
| Commit that adds the document and this section | The one after it. A commit cannot name itself; [PR #12](https://github.com/davealone69-gif/3d-mega-hub/pull/12) shows the head and its checks |
| Integration branch contained | `integration/avatarforge-renderer-port` up to `13117dc`, merged in `2920265`; four files conflicted and were resolved by hand, see [the table near the top](#how-those-commits-were-carried-onto-arena1a5b3043-3d-mega-hub-pr-12). PR #9 is not merged. `main` is untouched |
| Labels used | REAL + VERIFIED, IMPLEMENTED IN SOURCE, PARTIAL, UNVERIFIED, MISSING, BLOCKED |

### The owner's picture reference: ethnicity, body, hair (2026-11-15)

The owner then sent three screenshots of a dark picture creator and a Dropbox folder of eight rendered body references, with the words **"These are very important."** The screenshots show three steps: Ethnicity (six cards); Body type, Breast size and Butt size (five cards each); Hair color (tiles) and Hair style (eight cards). The body references are realistic figures in swimwear, seen from the front and the back. Commit `2920265` puts those choices into the creator and connects each one to the engine.

| Choice in the reference | In the creator | What it sets in the engine | State |
|---|---|---|---|
| Ethnicity: White, Asian, Black, Latina, South Asian, Middle Eastern | Look section: six picture cards | Skin tone, hair colour, eye colour (new field `eye_tone`) and seven face offsets (new field `face`): nose width, nose bridge, lips, eye tilt, eye opening, cheekbones, jaw. A starting point: every value can be changed afterwards | IMPLEMENTED IN SOURCE. Each look generated on the host with exactly the card's values and looked at |
| Body type: Slim, Athletic, Average, Curvy, BBW | Body section: Slim, Athletic, Average, Curvy, Plus size, and Muscular | `build` (new value: curvy) with the muscle and body-fat values that go with the name | same |
| Breast size: Flat, Small, Average, Big, Huge | Body section: five cards, for a female or unspecified body | `bust`. The engine used to read this field and then overwrite it for every female body, so it did nothing | same, and a host test of range and order |
| Butt size: Small, Slim, Athletic, Big, Huge | Body section: five cards | `glute`, a new field | same, and a host test of range and order |
| Hair color: brown, black, blonde, ginger, pink | Hair section: twelve colour tiles, those five first | `hair_tone` | IMPLEMENTED IN SOURCE. Brown, dark brown and black were drawn; the other colours were not |
| Hair style: Straight, Bangs, Braids, Curly, Bun, Ponytail, Bob, Bald | Hair section: fourteen cards, those eight first | `hair_style`. Bangs, curly, bob and box braids are new styles | IMPLEMENTED IN SOURCE. All fourteen generated on the host and looked at |
| The swimwear of the body references | Outfit section: Swimwear | `outfit` (new value): a bandeau and briefs; trunks on a male body | same, and a host test of what it covers |
| The dark look | The creator's default. "Light" in the header switches to the look of the first guide | nothing | IMPLEMENTED IN SOURCE. Never seen drawn |

- **Face section: IMPLEMENTED IN SOURCE.** Eye colour tiles and the seven face sliders. Face and Eyes were two of the four sections of the first guide that did not exist; Accessories and Background still do not.
- **Adults only.** Every avatar the creator builds is 30 years old (unit-tested). In the engine `bust` and `glute` are honoured for 18 and over; below that it uses its defaults whatever is asked (host test).
- **"BBW" is called "Plus size"** here, so that it reads for a male body too. It is one string in `AppearanceControls.kt`.
- **The pictures on the cards are this engine's own renders:** 51 in all, each one avatar generated at the app's quality with exactly the values the card sends. [The sheet of the new ones](docs/evidence/creator_cards_2026-11-15.jpg). Nothing from the screenshots or from the Dropbox folder is in the repository or the APK.
- **What the reference has and this does not.** Photographs on the cards. A step-by-step flow with a progress bar and a Next button: here it is one screen with a rail of sections. And bodies and faces of that quality: the engine's body is a smooth procedural form, the largest breast and butt sizes are plain rounded masses, and the face is one stylised face moved by seven measurements. The body references remain the target; this commit does not reach it.
- **REAL + VERIFIED on the host:** `ctest` 9 of 9 at `2920265` (2026-11-15T10:28:01Z, 42 s), with new checks for the bust and butt ranges, the adult rule, the face offsets, the curvy build, what swimwear covers, thirteen hair styles and six more prompt words; 65 full generations at the app's quality for the card pictures and the preset sheets, every one with exit code 0.
- **REAL + VERIFIED on the CI emulator:** the five workflows passed on `2920265`, among them the generated-avatar test through the production view and its animation check ([43704283240](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43704283240)).
- **UNVERIFIED:** everything on a phone, and the screen itself. There is no Android device or emulator where this work is done, so the author has never seen the creator drawn, in either look.
- **The APK with all of this:** [3D-Mega-Hub-debug-2920265.apk](https://github.com/davealone69-gif/3d-mega-hub/releases/download/phone-test-2920265/3D-Mega-Hub-debug-2920265.apk), 57,787,072 B, SHA-256 `dd180d6f4feddb61eed5b4d3e0200f49d5e3b54f73cbb0e9f7d24799bcf9a201`, pre-release [`phone-test-2920265`](https://github.com/davealone69-gif/3d-mega-hub/releases/tag/phone-test-2920265), published by run [43705538407](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43705538407). Uninstall the earlier build first.

### What changed after the phone report (2026-11-15)

The owner tested the `041e31d` build and reported: **"Major fault is animation crashes and no built in controls."** No diagnostics file or crash log came with it. Three commits answer it: `9519eae` (presets), `210ae9a` and `a5e93a8` (animation, creator).

- **The crash, likely cause found by reading the code, not confirmed by a log.** The only way that build offered to see a generated avatar move was the button "Open animation-capable Filament preview". Filament's loader reserves morph storage for every vertex of every target: several hundred megabytes for these avatars. That preview is now never given such a file: the button is not offered and the preview refuses the file itself (`GlbPreviewRouting.animationPreviewRefusal`, unit-tested). If the crash was something else, it is not fixed.
- **Built-in animation.** **REAL + VERIFIED on the CI emulator** for one thing: the production view listed 15 animations, played `t_pose`, presented 3 frames without a GL error and 81,915 pixels of the picture changed (run [43697157414](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43697157414)). **UNVERIFIED on a phone.** The GLES renderer skins on the GPU: the two vertex shaders blend each vertex by a uniform block of one skin matrix per joint, filled by `af_engine_evaluate_animation`. Playing uploads 8 KB a frame instead of the whole vertex buffer. Controls: a strip of the avatar's animations under the stage; a Motion section with play / pause, stop, loop and speed.
- **The creator: IMPLEMENTED IN SOURCE.** Avatar Studio is laid out after the owner's "Create Your Avatar" guide images: the avatar on a stage with turn, zoom and reset buttons, a rail of sections (Hair, Body, Skin, Outfit, Shoes, Motion), a picture of every preset, colour dots, sliders, and Randomize, Save and Create avatar. The guide's Accessories and Background sections are not there: the engine has no options for them (Face and eye colour followed in `2920265`, see above). CI compiles, lints and unit-tests it and the emulator smoke test opens the screen. Nobody has used it on a phone.
- **Presets: REAL + VERIFIED on the host.** All 22 hair, outfit and footwear presets of that stage look like what they are named in renders at the app's quality; 14 were rebuilt in `9519eae`. `2920265` added four hair styles and swimwear and re-rendered the sheets. Sheets: [hair](docs/evidence/preset_matrix_hair_2026-11-15.jpg), [outfits](docs/evidence/preset_matrix_outfits_2026-11-15.jpg), [footwear](docs/evidence/preset_matrix_footwear_2026-11-15.jpg).
- **Save: IMPLEMENTED IN SOURCE.** Save hands the avatar's GLB to the Android share sheet. Never pressed on a device.
- **The APK of that stage (superseded by the one above):** [3D-Mega-Hub-debug-a5e93a8.apk](https://github.com/davealone69-gif/3d-mega-hub/releases/download/phone-test-a5e93a8/3D-Mega-Hub-debug-a5e93a8.apk), 57,782,572 B, SHA-256 `6c07603d08232b8c55a282aa6911d8a58429cc6c9642ddc3bcff0182699fb08f`, pre-release [`phone-test-a5e93a8`](https://github.com/davealone69-gif/3d-mega-hub/releases/tag/phone-test-a5e93a8), published by run [43698174756](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43698174756). Uninstall the earlier build first: each CI debug build has its own signing key.
- **The owner's Dropbox folder `/avatar`** (intro art, an icon, body and style references) was looked at as the quality target. Nothing from it is in the repository or the APK: who made each picture and under what licence is not known.

### Exact source locations

| What | Where |
|---|---|
| Preset numbers (hair, outfit, footwear, the whole spec) | `avatarforge-engine/src/main/cpp/include/afcore.h`; Kotlin mirror `avatarforge-engine/src/main/java/com/davemega/hub/avatarforge/AvatarSpec.kt` |
| Hair shapes | `avatarforge-engine/src/main/cpp/src/af_field.cpp`, `BodyField::hairShape` |
| Clothing, footwear, armour shapes | same file: `clothUpper`, `clothLower`, `shoeShape`, `gearShape` |
| Skin, eye, cloth and other materials; the texture bake | `avatarforge-engine/src/main/cpp/src/af_texture.cpp`, `af_uv.cpp` |
| Skeleton, skin weights, 52 blendshapes, the clip library | `avatarforge-engine/src/main/cpp/src/af_rig.cpp` |
| Prompt words to presets | `avatarforge-engine/src/main/cpp/src/af_prompt.cpp` |
| GLB export and import; adopting an imported skeleton | `af_glb.cpp`, `af_glb_read.cpp`, `af_engine.cpp` (`adoptImportedRig`, `af_engine_evaluate_animation`) |
| Native renderer and its clip player | `avatarforge-engine/src/main/cpp/src/render/af_gl.cpp` (`GlRenderer::setClip`, `updateAnimation`), `af_render_capi.cpp` (`af_renderer_set_clip`) |
| Renderer JNI (no animation entry point yet) | `avatarforge-engine/src/main/cpp/avatarforge_renderer_jni.cpp` |
| Preset pickers and the Avatar Studio screen | `app/src/main/java/com/davemega/hub/AppearanceControls.kt`, `AvatarStudioScreen.kt` |
| Which preview a file opens in | `app/src/main/java/com/davemega/hub/GlbPreviewRouting.kt` |
| Filament preview with animation controls | `app/src/main/java/com/davemega/hub/FilamentPreviewView.kt` |
| Parts painted inside a garment; armour and suit hardware; heels | `avatarforge-engine/src/main/cpp/src/af_field.cpp`: `BodyField::apparelPart`, `gearField`, `footLocal`; paint in `af_texture.cpp` |
| GPU-skinned playback | `avatarforge-engine/src/main/cpp/src/render/af_gl.cpp`: `GlRenderer::updateSkinnedAnimation`; `af_shaders_gl.h`: `forward.vert`, `shadow.vert` (edited by hand in a generated header, as the header says); wrapper in `af_render.cpp` |
| Animation from Kotlin | `avatarforge-engine/src/main/cpp/avatarforge_renderer_jni.cpp`; `AvatarForgeSurfaceView.kt`: `animationClips`, `playAnimation` |
| The creator | `app/src/main/java/com/davemega/hub/AvatarStudioScreen.kt`, `AvatarCreatorPanels.kt`; pictures in `app/src/main/res/drawable-nodpi/preset_*.jpg` |
| Which preview a file may open in | `app/src/main/java/com/davemega/hub/GlbPreviewRouting.kt` |
| Host tools and tests | `avatarforge-engine/src/test/cpp/`: `host_avatar_render.cpp`, `animation_roundtrip_test.cpp`, `prompt_preset_words_test.cpp` |
| The only model file | `app/src/main/assets/models/female.glb` |

### Asset findings

- **There is no asset library, here or in any other repository of the account.** The complete Git history of this repository (475 commits on 17 branches and 4 tags when counted at `474b2fe`) has only ever contained one model file, `female.glb`. 96 other repositories were searched: 18 GLB files in five of them, no hair, clothing, skin or texture library, no FBX or BVH.
- **Every hair style, outfit and shoe is a procedural preset**: a branch of a shape function in the engine, not a file. 13 hair styles, 10 outfits, 4 kinds of footwear (9, 9 and 4 before `2920265`). A garment is the body's surface moved outwards, so it cannot be taken off one avatar and put on another model. Details inside a garment (shirt and tie, stitching, buttons, soles, laces, plate joints) are painted by the texture bake, not modelled.
- **Connected to the app: IMPLEMENTED IN SOURCE.** Text pickers in `f90f326`; since `210ae9a` the creator described above, with a picture per preset. CI compiles, lints and unit-tests it. Nobody has tapped it on a device.
- **Reachable from the prompt box: REAL + VERIFIED on the host.** One prompt per preset, 24 of 24 resolve to the right preset (`prompt_preset_words`).
- **Generated and drawn, and looking as named: REAL + VERIFIED on the host for all 27 presets, and for bald and barefoot.** Each was generated at the app's quality with the generator of `2920265`, drawn and looked at. Known limits: box braids are ropes with painted bands and no parting; the fringe of "Bangs" is a solid band; the edges of the swimwear are ragged close up; twin braids are chains of beads; lapels, seams, studs and laces are paint; the spacesuit has no helmet; the plates of the plate armour are rounded and thick; in heels the leg is shortened by the 2.7 % of stature the ankle is raised, so the stature asked for is kept; no animation has been looked at on the new garments. That judgement is one person looking at renders, not a measurement.
- **Not connected at all (MISSING):** explicit skin colour, armour colour and metalness, cloth roughness, age, muscle, body fat, bust, the 13 tape measurements, seed, any face shape control, eye colour, accessories, the stage background, any blendshape control. No face variety: one face per sex.
- **`female.glb`:** a static sample with 7 body morph targets, from `davealone69-gif/3DDD`, CC0 by that repository's README. Commit `98dd9ec` rewrote its JSON header for Filament: 418,556 B at the source, 418,312 B here, mesh and morph data identical. `THIRD_PARTY_NOTICES.md` still called it unchanged; corrected in the commit that adds this section.
- **Donor files that matter:** `CesiumMan.glb` (Khronos sample, CC BY 4.0, 19 joints, a 2 s walk), `test-rig.glb` and `studio_avatar.glb` (the owner's small rigged fixtures with one clip each), and `avatarforge-sample.glb` (a complete AvatarForge 0.9.0 avatar, 71 joints, 15 clips). None is in this repository. Details, hashes and links are in the document.

### Animation findings

| Question | Answer |
|---|---|
| Implemented in source | **REAL + VERIFIED**: 71 joints, 4 weights per vertex, 52 facial blendshapes and 15 clips, all written into every exported GLB. **IMPLEMENTED IN SOURCE**: GPU-skinned playback in the GLES renderer with select, play, pause, stop, loop, speed and seek; playback controls on the Filament preview for files from elsewhere. Still in source and unconnected on purpose: the CPU clip player `af_renderer_set_clip`, which re-uploads every vertex each frame. **MISSING**: blending between clips, retargeting, blendshape animation, any controller |
| Connected to UI | **IMPLEMENTED IN SOURCE** for avatars made in the app: an animation strip under the stage and a Motion section (no seek slider). **IMPLEMENTED IN SOURCE** for imported files with their own clips (Filament controls) |
| Working in automated tests | **REAL + VERIFIED** on the host: export, import with skeleton and clips intact, playback of every clip compared with the generating engine to 0.2 mm (`animation_roundtrip`). **REAL + VERIFIED** on the CI emulator: the production view plays a held pose and the picture changes (`GeneratedAvatarRuntimeTest`). Pause, loop, speed and stop have no test |
| Verified at runtime | **PARTIAL**: on the CI emulator the production renderer imported a generated avatar, listed 15 animations, played `t_pose` and showed a different picture (81,915 pixels changed, no GL error). That is a software GPU at one frame every several seconds: it proves the path, not smooth motion. **No animation has been watched playing on a phone** |
| Verified on the Samsung phone | **MISSING**. The phone has run `797d156` and `041e31d`. On `041e31d` the owner reported a crash and no controls; no result of any kind has come back for a newer build |

Faults found and fixed along the way: the import discarded the skeleton and clips of the engine's own files (`b636f23`); it then adopted any skeleton on which a single track of the clip library could bind (`041e31d`); the app could send a generated avatar to a preview that cannot hold it (`210ae9a`); and the first build of the built-in player listed no animations because the renderer wrapper did not forward the new calls, which its own instrumented test caught (`a5e93a8`).

### Prepared avatar findings

`/3d-mega-hub-diagnostics/3d-mega-hub/mylove_rig_prepared.glb` was fetched from Dropbox and only read: 13,221,520 B, SHA-256 `656cf2446eefbffc92cf569ce1a2f52b6d0ad449c282fc5a4d911abfef576d14`, the file the phone imported.

- 1 mesh, 163,756 vertices, 144,073 triangles, 1 opaque material, two 2048² WebP textures (`EXT_texture_webp` required), no morph targets.
- 1 skin with 20 joints. Its own metadata says `approximate_unverified` and "Rig deformation and retargeting have not been tested".
- **0 animation clips.** It displays as a static model and cannot test any animation control.
- The engine does not adopt its skeleton, correctly: other names, a T-pose, one unit tall. Playing the engine's clips on it needs retargeting, which does not exist.
- The weights are soft and misplaced for animation: the hands own 38 and 36 vertices, the sleeves and hair hang from chest, neck and shoulder. Joints named `Left…` are on the figure's right.

### Donor findings

96 other repositories of the account were listed and the file trees of their default branches searched for model, animation and texture files. Nothing was copied into this repository.

| File | Where | What it is | Licence |
|---|---|---|---|
| `CesiumMan.glb` | `Grok-Girls/public/glb/` @ `fd638fd` | Khronos sample, byte-identical to the original: 19 joints, one 2.0 s walk | CC BY 4.0, © 2017 Cesium |
| `avatarforge-sample.glb` | `AvatarForge/artifacts/` @ `40ef65b` | A complete avatar from engine 0.9.0: 71 joints, 52 morph targets, 15 clips. Imports here with skeleton and clips | The owner's engine output; that repository has no LICENSE file |
| `test-rig.glb` | `3DDD/app/src/androidTest/assets/` @ `186e54f` | 7,524 B fixture: 2 joints, one 1.0 s clip | The owner's test file; no licence stated |
| `studio_avatar.glb` | `3DDD/app/src/main/assets/models/` @ `186e54f` | 28,800 B fixture: 4 joints, one 3.0 s clip. The file that exposed the import fault fixed in `041e31d` | The owner's test file; no licence stated |
| `male.glb`, `androgynous.glb`, `cyborg.glb` | `3DDD` | Static bodies with 7 morph targets, like `female.glb` | CC0 by that repository's README |

- No repository contains a hair, clothing, skin or texture library, an FBX or a BVH file.
- No model files at all in `3d4me`, `AetherFace-3D`, the Aura repositories, `Morph-Lab`, `morph-lab-game-pages`.
- `morph-lab-game` and `DD3-Intro` are not visible to the account this scan ran under, so the screening notes on them further down stand as written and were not re-checked.
- The source code of the donor repositories was not read again for this scan; it looked for files.

### Verification results

Workflows on the code commit `2920265`:

| Workflow | Run | Result |
|---|---|---|
| Android CI | [43704287027](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43704287027) | success |
| AvatarForge diagnostic APK and tests | [43704289087](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43704289087) | success |
| AvatarForge generated-avatar runtime verification | [43704283240](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43704283240) | success |
| Release preview and Android emulator smoke | [43704291123](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43704291123) | success |
| Publish phone-test APK (rules self-test) | [43704283245](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43704283245) | success |

- **Host, 2026-11-15T10:28:01Z:** `ctest` 9 of 9 passed in 42 s at `2920265`.
- **Host:** 65 full generations at the app's quality with the generator of `2920265`, each with exit code 0, 25 to 88 s each on a 2-core machine with two running at a time. The 28 behind the preset sheets and the diagnostic avatar measured 1.04 to 1.96 million triangles; the 37 behind the card pictures were not counted.
- **CI emulator:** run [43704283240](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43704283240) on `2920265`: the generated avatar (1,232,813 vertices, 2,125,600 triangles, 71 joints, GLB 121,744,208 B) drew 323,702 geometry pixels through the production view; the renderer listed 15 animations, `t_pose` was started, 3 frames were presented and 81,407 pixels changed; no crash, ANR or fatal signal in its logcat. That emulator draws in software, one frame every 8.7 to 10.8 s, so this says nothing about speed on a phone.

Workflows on the code commit `a5e93a8`:

| Workflow | Run | Result |
|---|---|---|
| Android CI | [43697152270](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43697152270) | success |
| AvatarForge diagnostic APK and tests | [43697155008](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43697155008) | success, all three jobs |
| AvatarForge generated-avatar runtime verification | [43697157414](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43697157414) | success |
| Release preview and Android emulator smoke | [43697157775](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43697157775) | success |
| Publish phone-test APK (rules self-test) | [43697157450](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43697157450) | success |

The commit before it, `210ae9a`, failed two of them at the new animation check ("The production renderer listed no animations for a generated avatar", runs [43695827227](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43695827227) and [43695825489](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43695825489)). `9519eae` passed all five.

- **Host, 2026-11-15T09:03:24Z:** `ctest` 9 of 9 passed in 43 s at `a5e93a8`, among them `animation_roundtrip` and `prompt_preset_words`. The host does not build the GLES renderer.
- **Host:** 23 full generations at the app's quality with the generator of `9519eae` (the default avatar and 22 variations: every preset once, and the bald head), each with exit code 0: 1.03 to 1.95 million triangles, 24 to 72 s each on a 2-core machine.
- **Host:** the two edited vertex shaders compile with glslang as GLSL ES 3.10 and in the 4.50 dialect. No device has run them except the CI emulator.
- **CI emulator:** the production `AvatarForgeSurfaceView` test generates an avatar (gown, long hair: 1,231,652 vertices, 2,121,090 triangles, 121,811,256 B), exports it, imports it, reads the pixels back, then plays `t_pose` and reads them again: 15 animations listed, 3 frames presented while it played, 81,915 pixels changed, no GL error (run [43697157414](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43697157414)).
- **Phone:** the owner's report on `041e31d` quoted above, with no files. Before that, [docs/PHONE_TEST_2026-11-15.md](docs/PHONE_TEST_2026-11-15.md) for `797d156`.
- **Renders:** host software renders only. They are not phone captures. The creator's layout has not been seen on any screen by the author: there is no Android device or emulator where this work is done.

### Blockers and what comes next

1. **BLOCKED on a phone test.** Nothing written since the report has run on the Samsung. The APK: [3D-Mega-Hub-debug-2920265.apk](https://github.com/davealone69-gif/3d-mega-hub/releases/download/phone-test-2920265/3D-Mega-Hub-debug-2920265.apk), 57,787,072 B, SHA-256 `dd180d6f4feddb61eed5b4d3e0200f49d5e3b54f73cbb0e9f7d24799bcf9a201`, pre-release [`phone-test-2920265`](https://github.com/davealone69-gif/3d-mega-hub/releases/tag/phone-test-2920265), published by run [43705538407](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43705538407). Uninstall the earlier build first.
2. **What to try on the phone:** in the creator, Look (tap an ethnicity card), Body (a body type, a breast size, a butt size), Hair (a colour tile and a style), Face (one slider), Outfit (Swimwear shows the body best), then Create avatar; tap the animations in the strip under the avatar (Idle, Walk, Wave, Jump); Motion for pause, loop and speed; Save; and "Light" in the header. Then send back `renderer_diagnostics.json` and `renderer_telemetry.json` from Downloads/3D Mega Hub Diagnostics, a screenshot of the creator, and say what was on screen if anything closes the app.
3. **UNVERIFIED on the phone and worth measuring:** the frame rate with an animation playing (nothing is culled then), generation time and memory, and whether the GPU-skinned picture is right: the emulator test proves that the picture changes, not that the pose is correct.
4. **Not done:** a seek slider; blending between clips; tests for pause, loop and speed; Accessories and Background sections; a helmet for the spacesuit; a step-by-step flow like the picture reference's; male pictures on the cards; a face or a body of the quality of the owner's references.
5. **Imported files with their own clips** still go to the Filament preview, whose controls have never been seen working. [`CesiumMan.glb`](https://github.com/KhronosGroup/glTF-Sample-Assets/raw/main/Models/CesiumMan/glTF-Binary/CesiumMan.glb) (438,044 B, CC BY 4.0) is the file to try.
6. **Production signing** needs the owner's keystore.

Statements further down this file that this section corrects:

- "A 75-joint rig … verify exact counts": the engine in this repository builds 71 joints, counted in the exported GLB and on the emulator.
- "telemetry reported 0 joints on the displayed mesh": that was the import discarding the skeleton. Fixed in `b636f23`; the emulator now reports 71. The phone has not run the fix.
- "currently auto-plays the first clip and does not establish user-facing clip controls": the integration branch has since added clip selection, play and pause, stop, loop, speed and seeking to the Filament preview. They compile and pass lint in CI, and have not been used on a device.
- "a version of `mylove.glb` containing a skeleton also loaded": that file was measured. It has a skeleton and no animation, which is why nothing looked different.
- "Imported file-based GLBs default to the Filament gltfio preview": on this branch the file decides. A GLB that carries clips and was not written by AvatarForge opens in the Filament preview; everything else opens in the GLES renderer (`GlbPreviewRouting.kt`). The switch in Avatar Studio still works by hand.
- "The current Avatar Studio UI exposes prompt-driven generation": pickers now exist in source (`f90f326`). **IMPLEMENTED IN SOURCE**, not tried on a device.
- "Preserved animation test asset … have not been freshly verified": verified now. 20 joints, 0 clips. It cannot test animation; `CesiumMan.glb` can.
- "The native AvatarForge GLES importer is still a static imported-GLB path": still true for files from other tools. For AvatarForge's own files the import now keeps skeleton and clips, and still nothing on screen plays them.
- Still true: nobody has seen an avatar animate in this app.


## Avatar and animation findings update (2026-11-15)

> Later on 2026-11-15 these findings were measured and extended. Where this section and [the master document section](#avatar-assets-and-animation-master-document-2026-11-15) differ, that section is the newer one; its last paragraph lists the corrections.

The owner's latest phone test adds an important asset observation: **a version of `mylove.glb` containing a skeleton also loaded in the app**. The owner could not identify a visible difference in the avatar's appearance compared with the prior version. Record this as **REAL + VERIFIED (owner-observed load)** for successful file loading only. It does not prove that the Hub renderer applies the skeleton, deforms the mesh, evaluates poses or plays animation. The visible appearance remains unresolved.

### Corrected animation inventory

Earlier wording in this README that implied there was no animation implementation anywhere was too broad. Source inspection found substantial existing functionality that should be reused:

- **AvatarForge** has an authored animation/pose system, skeletal rig and skin-weight implementation, facial blendshape channels, native pose sampling/application, GLES deformation/rendering, an Android viewer with pose/animation controls, and a GLB exporter with an `includeAnimations` option. A 75-joint rig and 52 facial blendshape channels were observed in the inspected source revision; verify exact counts against the current pinned revision before depending on them.
- **3D-Mega-Hub `FilamentPreviewView.kt`** already obtains Filament's `Animator` from a loaded `FilamentAsset`, reads the animation count, applies animation index 0 using elapsed time, and updates bone matrices before rendering. This is existing playback code, but it currently auto-plays the first clip and does not establish user-facing clip controls on the main avatar screen.
- **3D-Mega-Hub's user-facing generated/imported avatar route** uses `AvatarForgeSurfaceView` / the native GLES path, while `FilamentPreviewView` is used for the bundled sample. The prior inspected GLB importer/render telemetry reported 0 joints on the displayed mesh, despite the generated GLB containing joints and blendshapes. Therefore the main route's skinning/animation integration remains unproven and is a concrete connection gap to investigate.

The correct status is not “no animation code exists”. It is **PARTIAL**: animation capability exists in AvatarForge and a separate Filament preview path, but end-to-end animation of the generated/imported avatar in the actual Hub screen, with usable controls, is not verified and remains unavailable to the owner. Do not write a second animation engine. Trace the actual screen route, verify the generated/exported GLB's skin and clip payload, then connect the existing AvatarForge and/or Filament implementation with the smallest required adapter and controls.

### Donor-repository screening

- `3d4me`: reusable procedural body/geometry and customisation work; inspected version does not provide a complete rig/animation system.
- `Aura-Studio-Avatar`: an animation interface exists, but inspected `LogAnimationEngine` is a logging stub, not real playback.
- Private `morph-lab-game`: render loop and input handling found; no proven skeletal-avatar clip pipeline found in the inspected code.
- `DD3-Intro`: intro/video project, not an avatar animation engine.

These were read-only inspections. No donor repository was modified. Treat these as findings from the inspected revisions, not guarantees about future/current changes.

### Current owner-observed results

| Test / observation | Status | What it proves |
|---|---|---|
| Previously tested diagnostic APK installed and generally operated on Samsung SM-S921B | **REAL + VERIFIED (owner-reported)** | Basic install and general operation, not avatar quality or animation |
| `mylove.glb` version containing a skeleton loaded in the app | **REAL + VERIFIED (owner-observed load)** | The file loaded; no evidence yet that its skeleton was evaluated or applied |
| Owner could not tell that the skeleton version looked different | **REAL + VERIFIED (owner observation)** | No discernible visual change was observed; appearance/deformation remains unresolved |
| Main Hub screen can animate the avatar with user controls | **MISSING / UNVERIFIED** | No working user-facing animation capability has been demonstrated |


## Status against the definition of done

| # | Item | Status | Evidence |
|---:|---|---|---|
| 1 | README reconciliation pushed safely | **REAL + VERIFIED** | Two outside README changes merged with their history (`6d0194b`, `79fb562`); nothing overwritten. [Reconciliation](#reconciliation) |
| 2 | Integration candidate reviewed and resolved | **REAL + VERIFIED** | Six named branches compared by ancestry and content: four are already contained, two are superseded. [PROJECT_STATUS.md](PROJECT_STATUS.md) |
| 3 | Engine integration demonstrated | **REAL + VERIFIED** on emulator | Generate, validate, export, import and render inside the app: run [43642406837](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43642406837) |
| 4 | Native start-up issue resolved | **REAL + VERIFIED** on emulator | `Manipulator.nCreateBuilder`: cause found, dependency removed, release APK launches. [Root causes](#root-causes) |
| 5 | Real GLB loading | **REAL + VERIFIED** on emulator | Imported and generated GLBs load through the engine importer. Static meshes only |
| 6 | Actual OpenGL ES rendering | **REAL + VERIFIED** on emulator | PixelCopy of the real surface: 560,418 geometry pixels, 0 GL errors |
| 7 | Camera controls | **PARTIAL** | Drag orbit is verified by a test. Pinch zoom and double-tap reset are implemented and have no test |
| 8 | Genuine runtime diagnostics | **REAL + VERIFIED** on emulator | Measured on the render thread; reports name the APK, the device and the asset by SHA-256 |
| 9 | Basic Samsung install/launch and general app operation | **REAL + VERIFIED (owner-reported)** | Owner confirmed the new APK was installed and tested on the Samsung SM-S921B on 2026-11-15 and reported general app operation good. Detailed crash/lifecycle stress and diagnostic-log review remain **UNVERIFIED** because the JSON reports have not been supplied |
| 10 | Android build succeeds | **REAL + VERIFIED** | Unit tests, lint, R8, debug APK, native regression: run [43642402022](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43642402022) |
| 11 | Signed APK verified | **PARTIAL** | Verified and reproducible with a one-run CI key. Production signing is **BLOCKED** on the owner's keystore |
| 12 | Signed AAB verified | **PARTIAL** | Same |
| 13 | SHA-256 values recorded | **REAL + VERIFIED** | [Artifacts](#artifacts) |
| 14 | Bravo review completed | **PARTIAL** | A separate review pass was done and recorded by the same agent that did the integration, not by an independent reviewer. [docs/BRAVO_REVIEW.md](docs/BRAVO_REVIEW.md) |
| 15 | Physical Samsung test | **REAL + VERIFIED (owner-reported)** | The owner confirmed on 2026-11-15 that the new APK was installed and tested on the Samsung SM-S921B and that general app operation is good. Exact phone-side telemetry/diagnostic JSON has not been supplied, so detailed performance, thermal, memory and lifecycle measurements remain **UNVERIFIED**. |
| 16 | Final README handover | **REAL + VERIFIED** | This file |

## Next action

> Progress since this list was written: items 1 and 2 were started. The generator was reworked, the import keeps the skeleton and clips, preset pickers were added, and every preset was generated and looked at. What is left is in [Blockers and what comes next](#blockers-and-what-comes-next).

The owner has completed the initial Samsung APK test and reports that the app is generally working. Do not keep treating the basic phone install/launch check as pending. The next work is avatar completion by reusing existing code, not another broad renderer rewrite:

1. Diagnose and improve the generated avatar's visible appearance at the source: body construction, proportions, mesh deformation, surface/material generation and export. Compare actual generated GLBs and screenshots against agreed visual acceptance criteria; do not infer quality from successful rendering or GLB validation.
2. Trace the actual main avatar route and connect existing AvatarForge rig/pose/clip functionality and/or the existing Filament `Animator` playback path. Verify the actual generated GLB's skin and animation payload, imported joint count and visible deformation. Add only missing adapters and user-facing controls (select, play, pause, resume, stop, seek, loop and speed). Do not build a duplicate animation engine or claim animation works merely because a GLB contains a skeleton or clips.
3. Keep the working renderer and current tested APK baseline intact. Make focused changes on the existing implementation line; do not create a competing app or parallel implementation branch.
4. Capture phone-side `renderer_telemetry.json` and `renderer_diagnostics.json` when practical. These are still needed to verify detailed Samsung performance, thermal, memory and lifecycle behaviour, even though the owner has confirmed the APK itself works generally.
5. Update this README with each evidence-backed milestone. A feature is not **REAL + VERIFIED** until it is built, installed and exercised on the phone.

For production signing: add the keystore and its three passwords as protected repository secrets. Never commit them.

## What is implemented

- **Generation.** The AvatarForge C++ engine builds an avatar from a text prompt, validates the deformed mesh and exports a GLB that the Khronos validator accepts with 0 errors.
- **Import.** A self-contained GLB from the file picker or an HTTPS link is copied into app storage, limited to 128 MiB, structurally validated, then read by the engine importer: geometry, PBR materials, PNG and WebP textures.
- **Rendering.** Imported and generated GLBs are drawn by the native OpenGL ES 3.1 renderer on its own thread. The bundled sample is drawn by Filament. No packaged library links Vulkan.
- **Camera.** Drag to orbit, pinch to zoom, double-tap to reset.
- **Diagnostics.** Frame rate and frame-time percentiles, draw calls, triangles, import and first-frame times, requested GPU bytes, heap, GL and EGL error counts, native session counts. Saved to `Downloads/3D Mega Hub Diagnostics` with the SHA-256 of the installed APK, the device model and the SHA-256 of the asset.
- **Crash containment.** A C++ exception in the renderer becomes a reported renderer failure instead of an app abort.
- **Release engineering.** The signed APK and AAB are read back and checked against the Gradle configuration; two clean builds must be byte-identical. The debug APK for the phone is inspected, downloaded again on a second machine and tied to the test that ran it. It is published as a direct download only when the readiness gate holds for its commit.

Not implemented end-to-end: usable animation of the avatar on the main Hub screen with user-facing controls. Existing AvatarForge animation/pose code and Filament GLB playback are documented in [Avatar and animation findings update](#avatar-and-animation-findings-update-2026-11-15). The inspected GLES import/render telemetry reported 0 joints on the displayed mesh, while the generated GLB carried joints and blendshapes. The skeleton-bearing `mylove.glb` was subsequently observed to load, but the owner saw no discernible visual difference. Skinning, pose evaluation and playback on the actual Hub avatar route remain **PARTIAL / UNVERIFIED**, not evidence that no animation code exists. Avatar appearance quality is also **UNFIXED** per the owner's 2026-11-15 phone feedback. Lip-sync, an embedded language model and photo-based identity are also not implemented. See [PROJECT_STATUS.md](PROJECT_STATUS.md).

## Build and test results for `3facc3d`

All four workflows passed. The emulator is Android 11 (API 30), x86_64, software GPU.

| Workflow | Run | Result |
|---|---|---|
| Android CI | [43642402022](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43642402022) | Unit tests, lint, R8, debug APK, seven native tests, imported-GLB render smoke (three instrumented tests): pass |
| Diagnostic APK and tests | [43642404867](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43642404867) | Six native tests; debug APK inspection; generated avatar drawn directly and through the production surface; the published APK downloaded and verified on a second machine: pass |
| Generated-avatar runtime | [43642406837](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43642406837) | Five instrumented tests including the native exception barrier: pass. Started by PR #12, so it built GitHub's merge commit `38fc7b7`, whose tree is identical to the evidence commit's |
| Release preview | [43642407532](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43642407532) | Signed APK and AAB verified and reproducible; debug and signed release APK install and launch: pass |

Earlier commits on this branch passed the same four workflows: `6b05ee4` (runs 43638793711, 43638796200, 43638799198, 43638799074) and the merge commit `79fb562` (runs 43637644842, 43637647380, 43637634111, 43637649884).

## Artifacts

GitHub keeps these artifacts for 14 days. Downloading an artifact needs a signed-in GitHub account and delivers a zip. The debug APK for the phone does not depend on that: it is also published as a plain `.apk` on a pre-release, linked from PR #12.

### Debug APK of the evidence commit

The APK to install on the phone is the one named in PR #12. This is the same record for the evidence commit; the app source is identical, the signing key and therefore the SHA-256 are not.

| Field | Value |
|---|---|
| File | `3D-Mega-Hub-generated-avatar-diagnostic-debug.apk`, 55,801,294 bytes |
| SHA-256 | `bb8f9e71b32785dc8393de39c1668278b38845fab3ed4290d22893cf207c0d4f` |
| Read back from the file | `com.davemega.hub`, versionCode 1, versionName 0.1.0, minSdk 26, targetSdk 35, debuggable, ABIs `arm64-v8a` and `x86_64` |
| Signature | APK Signature Scheme v2 verifies. Certificate SHA-256 `5137bfea14d4758c91fda1abf7dd444fd401edaceb8f5dc27af4d3f7d91def72` (`CN=Android Debug`, created on the CI runner for that one job) |
| Chain | Downloaded again on a second machine and found identical to the file the build job inspected, the file named in `SHA256SUMS`, and the APK the app reported for itself while the production render test ran: `DOWNLOAD_VERIFY … chain_complete=true`, job [132595324383](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43642404867/job/132595324383) |
| Download | [artifact `3D-Mega-Hub-generated-avatar-diagnostic-debug-apk`](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43642404867/artifacts/14132579570) (zip SHA-256 `19da8173e5f544637cf4f896da4e824edac14075c879dc957a81b8dff29386e9`) |

### Signed preview

Signed with a key that existed for one CI run and was discarded. It proves the release process, not the production identity.

| File | Bytes | SHA-256 |
|---|---:|---|
| `3D-Mega-Hub-preview-release.apk` | 46,185,764 | `38bfe049f503bcd441c57b8d5830dd048fb5766d5e0c01622b0d8551b6cd6d3e` |
| `3D-Mega-Hub-preview-release.aab` | 17,283,030 | `7f91063ceadfb00763bd2e5508112ab96bba0a2783bd80e6b843e4e7d1a35f25` |

- Download: [artifact `3D-Mega-Hub-signed-preview`](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43642407532/artifacts/14132428047) (zip SHA-256 `ceef2340d1f45b99f6ef54eed146409f1659ea74a45230e1e94a2a20767adcfa`).
- Read back from both files: `com.davemega.hub`, versionCode 1, versionName 0.1.0, minSdk 26, targetSdk 35, ABIs `arm64-v8a` and `x86_64` with identical library sets.
- APK Signature Scheme v2 verified; AAB verified by `jarsigner`. Certificate SHA-256 `0740db6d75f245afbf33c174619a9227a02287995f7364f78a2cace99c8d592b` (CN=3D Mega Hub CI Preview).
- Reproducibility: APK and AAB are byte-identical across two clean builds in the same run, the second without the Gradle build cache.
- `libafrender.so` links `libEGL.so libGLESv3.so libafcore.so libandroid.so libc.so libdl.so liblog.so libm.so`. No packaged library links `libvulkan.so`.

## Runtime evidence

Read from the job logs of the evidence commit. The emulator draws with a software GPU, so its frame rates say nothing about the phone; they show that frames are produced, measured and free of errors.

| Measurement | Generated avatar through the production surface | Diagnostic avatar, imported | Bundled sample, imported |
|---|---:|---:|---:|
| Job | [132593044475](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43642404867/job/132593044475) | [132592643011](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43642402022/job/132592643011) | same job |
| GLB bytes | 116,568,812 | 83,430,028 | 418,312 |
| Vertices | 1,218,003 | 821,215 | 3,014 |
| Triangles | 1,626,176 | 1,100,168 | 5,520 |
| Draw calls per frame | 29 | 29 | 11 |
| Frame rate on the software GPU | 0.39 FPS | 0.20 FPS | 1.08 FPS |
| Frame interval, median | 3,820 ms | 4,607 ms | 961 ms |
| First frame | 7,114 ms | 7,848 ms | 840 ms |
| GLB import | 852 ms | 1,247 ms | 4 ms |
| Renderer start | 129 ms | 144 ms | 505 ms |
| Requested texture bytes | 72,701,269 | 72,701,269 | 20 |
| Requested geometry bytes | 188,784,330 | 127,890,410 | 523,180 |
| Requested render-target bytes | 126,429,184 | 77,070,408 | 77,070,408 |
| Native heap | 650,279,600 | 484,860,320 | 124,831,952 |
| GL-error frames, EGL swap failures, failed draws | 0, 0, 0 | 0, 0, 0 | 0, 0, 0 |
| Frames cut off when the screen closed | 0 | 1 (`0x300d`) | 1 (`0x300d`) |
| Native sessions after the test | 1 created, 1 destroyed | 1 created, 1 destroyed | 2 created, 2 destroyed |

- **Visible pixels.** PixelCopy of the real SurfaceView found 560,418 geometry pixels of the generated avatar (minimum 5,000), status `PIXELS_VERIFIED`. The three render-smoke tests, which include the visible-pixel check of the imported GLBs, pass.
- **Asset identity.** Generated GLB: SHA-256 `aa957ad72f83ea0c4ada65169d76845b5069dfbfde0331ec35fe83382572e9c2`, the same in every run. It carries 71 joints and 52 blendshapes; the mesh on screen has 0 joints. Diagnostic avatar: `811ad98b48a608be46d6184fdca61c041eb74a46477714e71e148d9dbcee4525`. Bundled sample: `f2002ff4201e1845097a1647108dbbaf5ad722521d067732f1765e1153bae779`, which is what the app reported and what `sha256sum app/src/main/assets/models/female.glb` gives.
- **Same avatar, second workflow.** Runtime job [132592655367](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43642406837/job/132592655367): `PIXELS_VERIFIED` with 560,418 pixels, 0.22 FPS, first frame 10,589 ms, four sessions created and four destroyed. The difference from 0.39 FPS is the speed of the CI machine.
- **Camera.** A touch drag on the bundled sample moved the measured yaw from 0° to −83.4° and changed 2,137 of 9,216 sampled pixels. On the WebP-textured GLB: 0° to −84.8°, 3,086 of 9,216.
- **Exception barrier.** Three injected C++ exceptions (`std::runtime_error`, `std::bad_alloc` and a thrown `int`) arrived as Java exceptions, and native code kept running: `BARRIER_EVIDENCE cppExceptionsConverted=3 processAlive=true`.
- **Report identity.** In all three emulator jobs the APK SHA-256 that the app reported for itself equals the installed file (`IDENTITY_CHECK … match=true`).
- **Signed release APK.** Job [132594421344](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43642407532/job/132594421344): the verified file installs, starts cold in 1,104 ms, opens Avatar, loads the sample and submits a frame.
- **Crashes.** No crash, ANR or fatal signal in the logcat of any emulator job.

## Root causes

1. **The red renderer gate.** The session ended itself unless three frames arrived within 15 seconds, then replaced the reason with "renderer stopped". The integration head's own log shows a session ended after two correct frames of 1,626,176 triangles with no GL or EGL error. A slow renderer now keeps running and reports its numbers; only a renderer that produces no valid frame fails.
2. **`Manipulator.nCreateBuilder` implementation not found.** `ModelViewer` from filament-utils was used without `Utils.init()`, so its native library was never loaded. Nothing uses filament-utils any more and the dependency is removed.
3. **Invalid exported glTF.** 542 Khronos validator errors from missing or malformed accessor bounds; 0 after the fix.
4. **Diagnostics that were not measurements.** Frame time, CPU time and texture bytes were derived or never written. All are measured or counted now.

The full list of fifteen, with the release-engineering causes, is in [PROJECT_STATUS.md](PROJECT_STATUS.md).

## Open items

| Item | Status |
|---|---|
| General APK operation on Samsung SM-S921B | **REAL + VERIFIED (owner-reported)** | Owner installed and tested the new APK on 2026-11-15 and reported that everything generally works apart from the avatar issues. Detailed telemetry, sustained performance, heat, memory and lifecycle remain **UNVERIFIED**. |
| Production signing | **BLOCKED** on the owner's keystore |
| End-to-end avatar animation on the main Hub screen | **PARTIAL / UNVERIFIED**: substantial AvatarForge animation code and Filament GLB playback already exist, but their connection to the main GLES avatar route and usable controls is not demonstrated. Owner reports no way to animate the avatar in the app. |
| Avatar visual quality | **UNFIXED**: owner explicitly confirmed on 2026-11-15 that the appearance is still not fixed. Successful generation, export or rendering does not establish acceptable visual quality. [docs/AVATAR_QUALITY_PLAN.md](docs/AVATAR_QUALITY_PLAN.md) |
| Performance of the default avatar on the phone | **UNVERIFIED**: 1.1 to 1.6 million triangles with 4x MSAA. Measure before tuning |
| Native libraries are not stripped | **PARTIAL**: every build prints "Unable to strip". Finding O4 of the review |
| Independent review | **PARTIAL**. Seventeen open findings are listed in [docs/BRAVO_REVIEW.md](docs/BRAVO_REVIEW.md) |

## Reconciliation

- **PR #8** is closed and was never merged (closed 2026-11-09, head `41c3069`). PR #9 superseded it.
- **Parallel work.** Seven commits reached the integration branch on 2026-11-14 between 18:21 and 18:32 UTC while this branch was in progress. They are merged with their history in `79fb562`. Their README and status text is quoted in [PROJECT_STATUS.md](PROJECT_STATUS.md).
- **Earlier README.** Its geometry report is preserved in [docs/AVATAR_GEOMETRY_EVIDENCE.md](docs/AVATAR_GEOMETRY_EVIDENCE.md), its validation policy and readiness gate in [BUILD_AND_VERIFY.md](BUILD_AND_VERIFY.md), its quality plan and owner-tested ledger in [docs/AVATAR_QUALITY_PLAN.md](docs/AVATAR_QUALITY_PLAN.md), and its verification ledger and renderer investigation in [PROJECT_STATUS.md](PROJECT_STATUS.md).
- **Protected 83 MB GLB.** Regenerated from source byte for byte: 83,430,312 bytes, SHA-256 `56b072cb43203fe9c2ca0b231d28a3601b59914465647fa1da95a6890daccd6c`. It was never in the repository and still is not. Only the exporter with accessor bounds reproduces it, which is the difference between the earlier local workspace and the remote branch.
- **Yaw explanation.** The +6.0472° yaw was not an alignment; the stored mesh is already mirror-symmetric. Corrected in [docs/AVATAR_GEOMETRY_EVIDENCE.md](docs/AVATAR_GEOMETRY_EVIDENCE.md), which also preserves the cross-section report, the mirror-matcher finding and the unmatched-ID bound.
- **"D2 findings" and "README merge preview".** **PARTIAL / UNVERIFIED**: not found under those exact names in the previously inspected repository/PR/Dropbox locations. Google Drive was not searchable in this handover because its connector is currently disabled by the ChatGPT workspace administrator; do not treat Drive as searched or conclude that the material is absent there. Recheck when access or a direct document link/export is available.

## Documents

| File | Content |
|---|---|
| [docs/AVATAR_ASSETS_ANIMATION_MASTER.html](docs/AVATAR_ASSETS_ANIMATION_MASTER.html) | The master inventory of avatar assets, presets, rig, animation, GLB files, donor files, UI wiring, verification and missing work. Generated by `tools/avatar_master_doc/build_doc.py` |
| [docs/PHONE_TEST_2026-11-15.md](docs/PHONE_TEST_2026-11-15.md) | The owner's Samsung test of the build from `797d156` |
| [BUILD_AND_VERIFY.md](BUILD_AND_VERIFY.md) | Render routes, session rules, what each gate proves, the owner's evidence rules and readiness gate, local commands, the phone gate |
| [PROJECT_STATUS.md](PROJECT_STATUS.md) | Component baseline, all root causes, branch review, integrated sources, missing features, history |
| [docs/BRAVO_REVIEW.md](docs/BRAVO_REVIEW.md) | Review findings: fixed, open, and what was checked |
| [docs/AVATAR_GEOMETRY_EVIDENCE.md](docs/AVATAR_GEOMETRY_EVIDENCE.md) | Protected artifacts, corrected yaw, mirror symmetry, exporter validity, the preserved report |
| [docs/AVATAR_QUALITY_PLAN.md](docs/AVATAR_QUALITY_PLAN.md) | The avatar-quality plan and the reference-asset notes |
| [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) | Licences and provenance of the bundled fixture and libraries |

## Older debug build

[Previously published debug APK](https://github.com/davealone69-gif/3d-mega-hub/releases/download/android-build-46/3D-Mega-Hub-debug.apk). An older troubleshooting build. It is not the current candidate and not a release. GitHub still labels it "Latest" on the releases page; the current phone-test build is the pre-release linked from PR #12.

---

## Carried over from `integration/avatarforge-renderer-port` at `7b19ca7`

The sections below were written on the integration branch (PR #9) while this branch was being
worked on, and came in with the merge of `7b19ca7`. They are kept word for word. Read them with
two things in mind: the CI results they quote belong to the integration head `d66eb22`, not to
this branch; and where they say a fact is unverified, the master document linked at the top of
this README may now have the measurement.

### Latest implementation update: 2026-11-15

**Animation UI change committed on the canonical branch.** Imported file-based GLBs now open directly in the existing Filament gltfio preview, which already drives the GLB `Animator` each render frame. The user no longer has to discover and manually enable the alternate preview before animation controls can appear. The AvatarForge GLES preview remains available as an explicit alternate mode; its native importer still does not apply imported skeletal animation.

- Animation preview default: **IMPLEMENTED IN SOURCE** — commit `8b80b072187c1e471107fd9bb23ff8bdae9c4790`.
- Preview-mode explanation and control wording: **IMPLEMENTED IN SOURCE** — commit `0584611d5ffcc8d7b89416d98f14299790c3450f`.
- README and rig-reference handover: **UPDATED** — commit `3c7e356365ff55db3b915365dfcc284e7cc55aaf`.
- PR #9 observed head at the latest monitor check: `d66eb22af0c82de12aeafccd70b283d44ddd5c52` (the subsequent README evidence commit will create a new head).
- Exact-head CI on `d66eb22af0c82de12aeafccd70b283d44ddd5c52`: **FAILED** — production generated-GLB-to-GLES SurfaceView test failed in runs `43673389818` and `43673389819`; Android CI run `43673389854` also failed its instrumented GLES GLB render smoke. See the latest APK monitor snapshot below.
- `mylove_rig_prepared.glb` binary inspection and clip playback: **UNVERIFIED** — the preserved Dropbox file is recorded below, but its bytes were not available in the build workspace. Do not infer clip count or playback success from its filename.

### Latest APK monitor snapshot: 2026-11-15

**Observed PR #9 state:** open, targeting `main`; branch `integration/avatarforge-renderer-port`; observed head before this README evidence commit: `d66eb22af0c82de12aeafccd70b283d44ddd5c52`. No merge was performed. `main` remains untouched.

### Exact-head Actions evidence for `d66eb22af0c82de12aeafccd70b283d44ddd5c52`

| Workflow / run | Result | Evidence |
|---|---|---|
| [Android CI #603](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43673389854) | **FAILED** | Native host regression, R8/minification, unit tests, lint and debug APK build passed. Instrumented imported-GLB GLES renderer smoke failed with `OutOfMemoryError`: attempted 134,217,744-byte allocation with about 25.1 MB free; instrumentation process crashed |
| [AvatarForge diagnostic APK/tests #276](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43673389818) | **FAILED** | Build and basic generated-avatar PixelCopy diagnostic passed. Required `generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView` failed: only 2 presented frames in 15 seconds; 29 draws; 1,626,176 reported triangles; bounds in frustum; GL/EGL errors zero |
| [AvatarForge generated-avatar runtime verification #347](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43673389819) | **FAILED** | The production generated-GLB-to-GLES SurfaceView test failed with the same 2-frame / 15-second assertion |
| [Release preview and Android emulator smoke #416](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43673389800) | **PASS, not a readiness substitute** | Emulator install/launch smoke and ephemeral-key signed release preview passed. Neither proves production generated-GLB rendering |

### Artifacts and verification boundary

- Exact-head debug APK artifact: [3D-Mega-Hub-debug-apk, artifact 14145098709](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43673389854/artifacts/14145098709). Artifact ZIP SHA-256: `49fa25b29fc8e7e8ef68fa8d6c5a0ee0c962799527baf9943ea81a57ac9d793d`.
- Exact-head diagnostic APK artifact: [3D-Mega-Hub-generated-avatar-diagnostic-debug-apk, artifact 14145536569](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43673389818/artifacts/14145536569). Artifact ZIP SHA-256: `e9a9e1bf4b95ed411eca402d6a9099fba8e83a9d6f4ae5784d02ca58e609f3f6`.
- These digests identify the downloaded workflow artifact ZIPs, **not** the APK files inside them.
- APK package/application ID, APK signing certificate/signature and APK-file SHA-256: **UNVERIFIED**.
- Samsung SM-S921B installation and runtime: **UNVERIFIED**.

**Readiness decision: NOT READY.** The required production render instrumentation fails, the imported-GLB GLES smoke fails, and APK-level package/signature/hash verification is incomplete. Do not distribute these artifacts as a ready test APK. The README update itself creates a new PR head; all run results above belong only to the observed parent commit and must not be reused as exact-head evidence for the new commit.

### Rigged animation reference asset

- **Preserved Dropbox file:** `/3d-mega-hub-diagnostics/3d-mega-hub/mylove_rig_prepared.glb`
- **Size:** 13,221,520 bytes
- **SHA-256:** `656cf2446eefbffc92cf569ce1a2f52b6d0ad449c282fc5a4d911abfef576d14`
- **Status:** **REAL + VERIFIED** Dropbox upload completed and file ID returned. Binary contents have not yet been freshly inspected in this workspace; skeleton/clip counts and playback compatibility remain **UNVERIFIED**.
- Keep this file read-only as the rigged animation test reference. Do not replace it with the older static `mylove.glb`.

### Avatar assets, appearance presets and animation handover (2026-11-15)

### Where the files are

- The only checked-in runtime model under `app/src/main/assets/models/` is `female.glb` (418,312 bytes). It is a **static renderer fixture**, not a wardrobe or animation pack. The existing fixture inspection records one mesh, seven morph targets, zero skins, zero animations and zero embedded images.
- There is **no separate pre-made skin/hair/clothing asset-pack directory** in this branch's repository tree. Hair, skin appearance and clothing are currently produced by AvatarForge's procedural C++ generation/material pipeline, rather than loaded from a library of interchangeable finished files.
- The source-level hair presets are in `avatarforge-engine/src/main/java/com/davemega/hub/avatarforge/AvatarSpec.kt`: `NONE`, `BUZZ`, `SHORT`, `MEDIUM`, `LONG`, `PONYTAIL`, `BUN`, `BRAIDS`, `AFRO`, `MOHAWK`.
- The source-level outfit presets in the same file are `NONE`, `TSHIRT`, `DRESS`, `SUIT`, `JEANS_JACKET`, `ARMOR_PLATE`, `ARMOR_LEATHER`, `SPACESUIT`, `ATHLETIC`, `COAT`. Skin melanin/tone, hair tone, cloth tone and material roughness are also represented in `AvatarSpec`.
- These are **engine options/spec fields**, not proof that the app currently presents a complete clickable wardrobe, thumbnail catalogue or swatch picker. The current Avatar Studio UI exposes prompt-driven generation; independent skin-tone and hair-style/colour changes remain **NOT YET TESTED**.

### Animation status, separated by path

- The native engine source contains a built-in authored clip library in `avatarforge-engine/src/main/cpp/src/af_rig.cpp`: `idle`, `breathe`, `walk`, `jog`, `run`, `squat`, `jump`, `wave`, `punch`, `kick`, `pushup`, `t_pose`, `a_pose`, `sit`, and `turn_360` (plus the identity `rest` pose). The rig source also defines 52 facial blendshape channels.
- Imported file-based GLBs default to the Filament gltfio preview. Source code now exposes animation-clip selection, play/pause, stop/reset, loop, 0.5x–2.0x speed and timeline seeking when the selected GLB reports clips. This is **IMPLEMENTED IN SOURCE; runtime playback on the preserved rigged reference asset remains UNVERIFIED**.
- The native AvatarForge GLES importer is still a **static imported-GLB path**: it does not yet apply imported skeletal animation/skin data. Do not switch to that preview expecting imported clips to play.
- Preserved animation test asset: private Dropbox `/3d-mega-hub-diagnostics/3d-mega-hub/mylove_rig_prepared.glb`, 13,221,520 bytes, SHA-256 `656cf2446eefbffc92cf569ce1a2f52b6d0ad449c282fc5a4d911abfef576d14`. Keep read-only and private. Its binary clip/joint inventory and playback have not been freshly verified in this workspace.

**Evidence boundary:** source declarations prove that presets and clip definitions exist in code. They do not prove that the current APK exposes all options, that generated GLBs contain every native clip, or that animation visibly plays on the target Samsung phone. Those remain separate runtime checks.

## Exact-head Android test APK readiness monitor (2026-11-15)

**Decision for inspected source SHA `7b19ca760ac529f2f157ae1bc516a3d0333029b6`: NOT READY.** PR #9 and the branch ref both pointed to this exact commit when checked. Six Actions runs were associated with this SHA (four completed PR runs and two push-triggered runs cancelled by concurrency).

### Required production generated-GLB → GLES SurfaceView gate

- **Failed:** [diagnostic workflow run 43680996825](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43680996825), job **132711632839** — “Build debug APK and verify generated avatar pixels”.
- Required step: “Run generated GLB through production GLES SurfaceView”.
- Instrumentation: `GeneratedAvatarRuntimeTest#generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView`.
- The JUnit report records 1 test, 1 failure. Assertion at `GeneratedAvatarRuntimeTest.kt:217`: no drawable EGL-presented model frame within 15 seconds. Captured counters: `frames=2`, `presented=2`, `draws=29`, `triangles=1626176`, `boundsInFrustum=true`, `outsideCorners=0`, `glError=0x0`, `eglError=0x0`.
- The separate [generated-avatar runtime run 43680996884](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43680996884), job **132711372691**, also failed the same production SurfaceView test with the same visible-frame assertion. Its uploaded test report confirms the failure.

### Other exact-SHA workflow results

- [Android CI run 43680996926](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43680996926): Unit tests, Debug APK build, Android lint, R8/minification and Native C++ sampling regression **PASSED**. Its separate imported-GLB renderer job **132711374921 FAILED**: `AvatarForgeRendererSmokeTest#generatedAvatarDebugDiagnosticVerifiesImportedGlesPixels` hit `OutOfMemoryError` while allocating 134,217,744 bytes, with 25,149,440 bytes free and a 201,326,592-byte process growth limit. This imported-GLB smoke is not a substitute for the required generated-GLB production SurfaceView test.
- [Release preview run 43680996849](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43680996849): signed preview build and emulator install/launch smoke **PASSED**. Install/launch is not production renderer evidence. The preview uses a one-run ephemeral CI signing key, not a retained production signing identity.
- Push-triggered runs **43680994601** (Android CI) and **43680994591** (release preview) were cancelled; they provide no passing evidence.

### APK artifact inspection (same source SHA, not a readiness pass)

- [Diagnostic debug APK artifact 14149070299](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43680996825/artifacts/14149070299) was produced by run 43680996825 for `7b19ca760ac529f2f157ae1bc516a3d0333029b6`. Downloaded and inspected the APK itself.
  - File: `3D-Mega-Hub-generated-avatar-diagnostic-debug.apk`
  - Package ID: `com.davemega.hub`
  - APK SHA-256: `79cd893cc68674f50ac5c05b57f54cd493a75c669aa84f807b51cb112747b125`
  - APK v2 signature and APK content digest both verified.
  - Signer: `C=US,O=Android,CN=Android Debug`
  - Signing certificate SHA-256: `bc4676bb9d180d7b5448f0728fd75624f4335585466d11313271cc6b2969aab3`
  - This is a fresh, debug-signed APK, but its required production render test failed. It is therefore **NOT READY**.
- [Signed preview artifact 14147893845](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43680996849/artifacts/14147893845) also belongs to this SHA. Its APK package is `com.davemega.hub`, SHA-256 `f6c92cc763c2181709e845340f552bad54bfd6e3ba1457e57e746ebdbdb0ca29`; the signer is `CN=3D Mega Hub CI Preview,O=Ephemeral Build,C=AU`, certificate SHA-256 `a133a285a42a930705a8ed7d13d55a3bb07ecd3f223f0cca71cc684d30968d35`. This one-run key is not a production signing identity and the artifact does not override the failed render gate.

### Next verification rule

Do not distribute or label either artifact as the first READY APK. The production generated-GLB-to-GLES SurfaceView test must pass on one exact current head, all required Android build jobs must pass, and the APK artifact from that same SHA must again be downloaded and inspected. The imported-GLB OOM is a separate failure worth fixing, but it cannot replace the production generated-GLB gate.

This README evidence update creates a new branch commit. Consequently, all run and APK evidence above applies **only** to `7b19ca760ac529f2f157ae1bc516a3d0333029b6`; it must not be carried forward as proof for the new README commit. Fresh Actions runs and a fresh artifact are required for the new exact branch head. No application source was changed, `main` was not modified, and PR #9 was not merged.
