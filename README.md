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
| Working branch | [`arena/1535c8f6-3d-mega-hub`](https://github.com/davealone69-gif/3d-mega-hub/tree/arena/1535c8f6-3d-mega-hub) since 2026-11-26. It is the previous working branch `arena/1a5b3043-3d-mega-hub` plus the trunk merged into it, plus the screen-density LOD fix and the rescue of `arena/089497bd`'s unique work. See [Branch convergence handover — 2026-11-26](#branch-convergence-handover--2026-11-26) |
| Review path | The convergence pull request from `arena/1535c8f6-3d-mega-hub` into `integration/avatarforge-renderer-port`, then [PR #9](https://github.com/davealone69-gif/3d-mega-hub/pull/9) into `main`. [PR #12](https://github.com/davealone69-gif/3d-mega-hub/pull/12) (`arena/1a5b3043-3d-mega-hub` into the trunk) is **superseded** by it and left open for the owner to close; every commit of PR #12 is contained in the new line. [PR #16](https://github.com/davealone69-gif/3d-mega-hub/pull/16) (`arena/33f12efd-3d-mega-hub` into `main`) is a second, competing path into `main` from a branch 198 commits behind this line: it and PR #9 cannot both be right, and reconciling them is the owner's decision |
| Current code head | `42fa78a` on `arena/1535c8f6-3d-mega-hub` (the rescue of `arena/089497bd`'s unique work), on top of `bdaef4b` (documents), `88c2c67` (the trunk merge) and `07610b5` (screen-density LOD). **This README update creates a new head**, so per the rule below no result recorded here belongs to it; the exact-head evidence quoted is for `bdaef4b5f3446e6ae3e17ee6c1dddaa653cee6d6` and is listed in the convergence handover. Earlier head `ff9e6ba7d1d366fded3afe5c316f29c57a696785` (`ff9e6ba`). All five workflows passed on it and its phone-test APK is published; see [Verification results](#verification-results). It adds clip names and a clip strip for an imported file with animations, a model tool, and the first test of a rigged model in the glTF viewer ([below](#a-file-with-14-animations-ff9e6ba)). `50bbe25` and `96152cd` before it changed how an imported model is shown: brightness, the glTF viewer's picture, a JPEG test. The creator and the generated avatars are those of `2920265`. The commits after `ff9e6ba` add one evidence-only step to the new test (a screenshot of the screen), a second attempt in the screen capture of another test (`848632d`) and documents; nothing the app does differs. Measurements further down that name `3facc3d`, `797d156`, `041e31d`, `a5e93a8`, `2920265`, `7e4f0cb` or `96152cd` are from those commits |
| README update | Updated after the owner's third message of 2026-11-15: three screenshots of a picture creator and a folder of body references, with the words "These are very important". The documents-only commit does not change app code or the tested APK. A second documents commit answers the owner's fourth message ("describe this machine ... repairs ... how we can get to 3d photo realistic avatars") with `AGENTS.md` and `docs/PHOTOREAL_UPGRADE_PLAN.md`, and corrects two stale sentences about imported models in `BUILD_AND_VERIFY.md` and `PROJECT_STATUS.md`. A third documents commit follows the owner's fifth message, a link to a Dropbox folder of three 3D models: it records what they are ([`docs/OWNER_MODELS_2026-11-15.md`](docs/OWNER_MODELS_2026-11-15.md)) and the results of the code commit `7e4f0cb`. A fourth follows the owner's sixth message, a link to a second folder with eight more models: it records them in the same file, the results of the code commits `50bbe25` and `96152cd`, and corrects one code comment. A fifth follows the owner's seventh message, a link to one more model, a file with 14 animations: it records the file, its conversion and the results of the code commit `ff9e6ba` |
| Record for the head commit | In the description of [PR #12](https://github.com/davealone69-gif/3d-mega-hub/pull/12). A commit cannot contain the results of its own workflow runs, so the record for the head, with the APK to install, is kept there |
| `main` | `fa9fc3658145c62bb38ecc7b4023938b0e625b8b`, unchanged. This branch has not touched `main`, and no branch was force-pushed. (An earlier row here named `6c1a94d`; `main` has since moved to `fa9fc36`, which is also where both release tags sit) |

The rule is one implementation line, `integration/avatarforge-renderer-port`. That rule is now satisfied in the other direction from before: the working branch above **contains the whole trunk**, merged at `88c2c67` from `9baf7c670052748887bbadcd7aebf970380687f0`, so there is no trunk work left outside it and nothing for a future session to re-derive. The previous text here said the branch contained the trunk only up to `13117dc` and that PR #12 still merged without conflict; both statements were overtaken by the trunk's 13 later commits, which is what made PR #12 conflict. Once the convergence pull request has landed, the trunk is the single line again and this branch can be deleted. Do not open another implementation branch, and do not resume `arena/1a5b3043-3d-mega-hub`, `arena/089497bd-3d-mega-hub` or any branch listed as deleted in [`docs/BRANCH_CONVERGENCE_PLAN.md`](docs/BRANCH_CONVERGENCE_PLAN.md).

Status labels: **REAL + VERIFIED**, **IMPLEMENTED IN SOURCE** (written, built and tested by CI, never exercised by a person on a device), **UNVERIFIED**, **PARTIAL**, **MISSING**, **BLOCKED**. "Verified on the emulator" never means verified on the phone.

## Avatar assets and animation master document (2026-11-15)

**The single inventory is [`docs/AVATAR_ASSETS_ANIMATION_MASTER.html`](docs/AVATAR_ASSETS_ANIMATION_MASTER.html).** It is one self-contained HTML file with a clickable table of contents, collapsible sections, a search box over every table, status filters and links to the exact source lines. GitHub shows HTML files as source, so open it from a checkout or download it and open it in a browser. Its evidence images are in [`docs/evidence/`](docs/evidence/). It is generated by `tools/avatar_master_doc/build_doc.py` from the source tree and from the evidence record `doc_extra.json` beside it, so its source links can be refreshed after the code changes.

| Item | Value |
|---|---|
| Code the document describes | `2920265ae822d08f6508e8eac917a9817e232e1e` on `arena/1a5b3043-3d-mega-hub`. Source links in the document are permanent links to that commit |
| Commit that adds the document and this section | The one after it. A commit cannot name itself; [PR #12](https://github.com/davealone69-gif/3d-mega-hub/pull/12) shows the head and its checks |
| Integration branch contained | `integration/avatarforge-renderer-port` up to `13117dc`, merged in `2920265`; four files conflicted and were resolved by hand, see [the table near the top](#how-those-commits-were-carried-onto-arena1a5b3043-3d-mega-hub-pr-12). PR #9 is not merged. `main` is untouched |
| Labels used | REAL + VERIFIED, IMPLEMENTED IN SOURCE, PARTIAL, UNVERIFIED, MISSING, BLOCKED |

### The owner's 3D reference models and the imported-model viewer (2026-11-15)

The owner's fifth message of the day was a link to a Dropbox folder, `3d real`, holding three 3D models of unclothed adult women as references. **None of them is in this repository or in any APK.** What they are, what they show about realism and where they stand on licence is recorded in [`docs/OWNER_MODELS_2026-11-15.md`](docs/OWNER_MODELS_2026-11-15.md). In short:

| Model | What it is | Triangles | Textures | Skeleton, clips, morph targets |
|---|---|---|---|---|
| "Nude girl T-pose" (Sketchfab, milan037, CC-BY 4.0) | an AI-generated mesh | 23,230 | one, 1024 or 2048 square | none |
| "Heather" (Sketchfab, milan037, CC-BY 4.0 label) | a posed low-polygon figure in 14 parts | 16,377 | 25, small | none |
| "Ink" (Sketchfab, Yudianggara, CC-BY 4.0 label) | a Daz Genesis 8 Female with add-on hair, brows, boots and an anatomy add-on | 497,110, of which 27,292 are body skin | 18: twelve at 4096 square | 199 joints, no clips, no morph targets |

"Ink" is the kind of asset the owner's body references were rendered from, and it is Daz content that an uploader relabelled: the label does not make it free to ship.

**What the models exposed** (engine importer on a host at `e4655c3`; **REAL + VERIFIED** on the host):

- The app's own renderer keeps one colour texture for a whole imported model and draws everything opaque. "Heather" came up with 2 of 14 parts textured, "Ink" with 4 of 45 and its hair cards as solid sheets.
- Since `210ae9a` the app sent an imported file to its second renderer, the Filament glTF viewer, only when the file carried animation clips. That viewer draws such models as authored.
- Nothing limited texture memory: "Ink" needs 1,152 MiB of textures once decoded, and the glTF viewer read a whole file into the Java heap twice over.

**What `7e4f0cb` changes** (**IMPLEMENTED IN SOURCE**: built, linted and unit-tested by CI; the emulator smoke starts the changed viewer; nothing run on the phone):

| Change | Test |
|---|---|
| An imported file with clips, more than one colour texture or a transparent material opens in the glTF viewer by itself. Single-texture models and avatars made here stay on the app's own renderer | `GlbPreviewRoutingTest`, 13 tests, 6 new |
| At import, textures are reduced to a budget of 448 MiB decoded: the pixel sizes are read from the image headers, the largest images are halved until the sum fits, and the app keeps a copy of the GLB with only those images replaced | `GlbTextureBudgetTest`, 16 tests with synthetic files and a stand-in for the image scaler. The commit message says 17; there are 16 |
| The Android image scaler behind it (sampled decode, straight alpha, PNG for transparency and normal maps, JPEG otherwise) | none |
| The glTF viewer refuses a file that is still over the budget, decides before reading it, reads it with one allocation and reports running out of memory instead of ending the app | the refusal rule is unit-tested; the rest has no test |
| The glTF viewer turns with one finger, zooms with two, resets on a double tap, and has ambient light beside its one directional light | none. The emulator smoke loads the bundled sample in this viewer and presents a frame, in the debug and in the minified release build |

The same reduction, as a Python prototype, was run on the real "Ink" file on the host: twelve textures halved, 1,152 MiB decoded down to 384 MiB, file 103.6 MB down to 60.8 MB, all six geometry buffer views byte-identical, and the engine's importer reads the result in 2.0 s with a 364 MB peak (7.2 s and 962 MB before). The Kotlin code has only run against synthetic files.

**Still missing in the glTF viewer** (after `96152cd`, see below): reflections of a surrounding (there is one light and a uniform ambient term), and the skin, eye and hair shading of the app's own renderer. "Ink" has no animation and no morph targets, so it stands in its A-pose and the creator's sliders do not apply to it. Which renderer should draw authored bodies in the long run is now an open question in [`docs/PHOTOREAL_UPGRADE_PLAN.md`](docs/PHOTOREAL_UPGRADE_PLAN.md), section 5.2.
#### The second folder and what followed (`50bbe25`, `96152cd`)

The owner's sixth message was a link to a second folder, `New folder (2)`, with eight more models of adult women. **None is in this repository or in any APK.** Details: [`docs/OWNER_MODELS_2026-11-15.md`](docs/OWNER_MODELS_2026-11-15.md), section 6.

| Files | What they are | In the app |
|---|---|---|
| Five `body-raw*.glb`, 20 to 69 MB | Generated statues: one fused mesh of 0.6 to 2.3 million triangles, one 2048 JPEG with the lighting painted in. No skeleton, no morph targets | "Import GLB" opens them on the app's own renderer |
| `model2.glb`, 63 MB | The same kind, written by Tripo AI: 1.9 million triangles, colour, normal and roughness-metal maps at 4096 | The same; the importer keeps all three maps |
| `Woman Seated V22.fbx` | A Blender model, 141,612 triangles, no skeleton | Cannot be opened: the app reads GLB only |
| `ink/source/zeee.fbx` | The DAZ Studio export behind the first folder's `ink.glb`: 198 bones, **0 blend shapes** | Cannot be opened; `ink.glb` can |

They are statues: body, hair and clothes are one surface, so nothing can be reshaped, re-dressed, posed or animated. Looking at how the app treats such files found three things, and a fourth about the project itself:

| Change | Status |
|---|---|
| **Imported textures were drawn a fifth too dark** on the app's own renderer when the file leaves out `baseColorFactor`, as most exporters do. The reader used a fallback grey of 0.8 where glTF says 1. Every model of the owner's first folder and `model2.glb` are written that way. Fixed in `96152cd` | **REAL + VERIFIED on the host**: `imported_glb_pbr_test` fails without the change and passes with it; `model2.glb` reports factor 1.00. On the emulator the test fixtures declare the factor, so no pixel test covers it |
| **JPEG textures had no test.** The engine decodes PNG itself and hands JPEG to Android. `jpegTextureIsDecodedAndRenderedByAvatarForge` opens a JPEG-textured file with a foreign generator name through the app and requires its colour on screen | **REAL + VERIFIED on the emulator** (Android 11), runs [43724124384](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43724124384) and [43725536116](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43725536116); 4 x 4 test image |
| **The glTF viewer's picture**: 4x MSAA and FXAA, screen-space ambient occlusion, a shadow-casting key light (one 2048 map), the Khronos PBR Neutral tone curve, and the creator's stage colour as background instead of black | **IMPLEMENTED IN SOURCE**; seen on the CI emulator with the bundled figure only (below). No frame rate measured. None of the owner's models has been drawn by it |
| **Emulator screenshots can be read back.** A workflow step stores a small JPEG copy in the output of a check run on the commit; see [AGENTS.md](AGENTS.md), section 7 | Working: four pictures of `50bbe25` were read back and looked at. Of `96152cd` three were read back: the viewer picture was looked at, the other two are byte-identical to those of `50bbe25` |

What the screenshots showed, which no test had:

- `50bbe25` switched Filament's post-processing on with its default tone curve (ACES). The brightest part of the bundled figure, whose base colour shows as (237, 213, 196), came out as (214, 201, 188): paler and greyer. The background came out as (13, 19, 33) against the stage's (28, 34, 48). `96152cd` uses the PBR Neutral curve and compensates the background: the same place reads (210, 185, 168) and the background (28, 34, 48). Red to blue is 1.21 in the file, 1.14 with ACES, 1.25 with the neutral curve. The message of commit `96152cd` and its code comment gave the file's colour as (216, 169, 140): that is the linear factor times 255, not what a display shows, and it overstated the difference. The comment is corrected in the documents commit that follows; a commit message cannot be.
- **The generated avatar as the app's own renderer draws it** (software GPU of the emulator, default avatar): a recognisable figure with flat, plastic-looking skin, a simple face and visible contour banding on the clothes. It is far from the owner's references. This is the first real frame of the production renderer that the author of these documents has seen; the pictures made for these documents before were host software renders.
- `50bbe25` cannot be given a phone-test APK: its screenshot checks ended "neutral", GitHub lists them among the jobs of a workflow run, and the publishing gate refuses a run with a job that did not end in success. Since `96152cd` they are stored as "success" under the name "Evidence only: ...".

#### A file with 14 animations (`ff9e6ba`)

The owner's seventh message was a link to one more file in `3d real`: `beautiful_realistic_undressed_girls_-_14_anims.glb` (38.6 MB; Sketchfab, Polygonal Studios, CC-BY 4.0 label). **It is not in this repository or in any APK, and neither is the copy converted from it.** Details: [`docs/OWNER_MODELS_2026-11-15.md`](docs/OWNER_MODELS_2026-11-15.md), section 7.

- **What it is.** A showcase scene: 15 copies of one rigged figure of an unclothed adult woman in two rows. Fourteen copies each play a different motion (dances, exercise moves, idles) and one stands still. All the motions sit in one animation track of 9.33 s. One figure has 28,605 triangles and a skeleton of 66 joints with Mixamo names. It is game-style, not photoreal: its value is the movement.
- **In the app as it is.** It opens in the glTF viewer as a small crowd with a single clip in which everybody moves at once.
- **Converted.** [`tools/glb_figures/figures_to_clips.py`](tools/glb_figures/figures_to_clips.py) keeps one figure, puts it at the origin facing the camera and turns every other copy's motion into a named clip on it: 15 clips, 4.4 MB. The copy is in the owner's Dropbox at `/avatar/from-agent/models/one_figure_14_clips_from_beautiful_realistic_undressed_girls.glb`. Skinned on the CPU at five times of every clip, no vertex is further than 0.000007 m from its place in the source; the Khronos validator reports no error and no warning. It has not been opened in the app.

| Change in `ff9e6ba` | Status |
|---|---|
| **Clip names and a clip strip.** A file shown by the glTF viewer gets a row of chips under the stage, one per clip, with the names written in the file; one tap plays a clip. The Motion line shows the name. Before, clips were only "1 of 15" with Previous and Next | **IMPLEMENTED IN SOURCE.** Labels: `GlbClipNamesTest`, eight unit tests. Strip and tap: the instrumented test below |
| **`tools/glb_figures/`**: the converter, a generator of rigged test figures made of boxes, and a self-test | 24 checks pass on the host and in the new CI job "Model tools self-test (Python)". The converter has run on one real file |
| **`GlbClipPreviewTest`**, the first test that gives the glTF viewer a skeleton and clips. A generated figure with the clips Wave, March and Bow is opened through the app | **REAL + VERIFIED on the emulator** (Android 11), run [43739150428](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43739150428): the viewer listed the three clips by name; a tap on the March chip chose that clip; with Wave held at 0 s and at 1 s, 350 of 9,216 sampled points of the picture differed, and none after going back to 0 s |

The two pictures of that test were read back from the commit's check runs and looked at: the box figure with its arm down and with its arm raised, drawn with the viewer's shadows. This is the first picture of a moving model from the glTF viewer. The screen itself was read back from the run of the commit after it (run [43740884035](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43740884035) on `787e030`) and looked at. Under the stage are the chips Wave, March and Bow, with Bow marked. The Motion section shows "Animations in this file · 3 of 3 · Bow · 2.00 s", the buttons Previous, Pause and Stop / reset, and the seek slider at 1.40 of 2.00 seconds: the clip was running. **One layout fault shows:** the row of four buttons is wider than the card on that screen (411 dp wide), so "Stop / reset" is cut and "Next" is out of sight until the row is swiped sideways. Not fixed. The chips above do the same job as Previous and Next.

Not done: carrying such clips over to the avatars made in the app. They have their own skeleton of 71 joints, and moving a clip from one skeleton to another (retargeting) is not implemented; see [`docs/PHOTOREAL_UPGRADE_PLAN.md`](docs/PHOTOREAL_UPGRADE_PLAN.md), R19.

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
- **The APK with all of this:** the newest build, [3D-Mega-Hub-debug-ff9e6ba.apk](https://github.com/davealone69-gif/3d-mega-hub/releases/download/phone-test-ff9e6ba/3D-Mega-Hub-debug-ff9e6ba.apk), 57,803,456 B, SHA-256 `bb24b5afb8898196de7cb0d6c3636c73b58620825bfe43d8824efc7b75723a6a`, pre-release [`phone-test-ff9e6ba`](https://github.com/davealone69-gif/3d-mega-hub/releases/tag/phone-test-ff9e6ba), published by run [43740439495](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43740439495). Uninstall the earlier build first. The first build that had these choices was `phone-test-2920265` (57,787,072 B, SHA-256 `dd180d6f4feddb61eed5b4d3e0200f49d5e3b54f73cbb0e9f7d24799bcf9a201`, run [43705538407](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43705538407)).

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

Workflows on the code commit `ff9e6ba`:

| Workflow | Run | Result |
|---|---|---|
| Android CI | [43739150428](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43739150428) | success: lint, unit tests, native C++ sampling regression, model tools self-test, R8, debug APK build, render smoke with five instrumented tests |
| AvatarForge diagnostic APK and tests | [43739153627](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43739153627) | success, all three jobs |
| AvatarForge generated-avatar runtime verification | [43739145178](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43739145178) | success |
| Release preview and Android emulator smoke | [43739155824](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43739155824) | success, all three jobs |
| Publish phone-test APK (rules self-test) | [43739145139](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43739145139) | success |

- **APK:** [3D-Mega-Hub-debug-ff9e6ba.apk](https://github.com/davealone69-gif/3d-mega-hub/releases/download/phone-test-ff9e6ba/3D-Mega-Hub-debug-ff9e6ba.apk), 57,803,456 B, SHA-256 `bb24b5afb8898196de7cb0d6c3636c73b58620825bfe43d8824efc7b75723a6a`, pre-release [`phone-test-ff9e6ba`](https://github.com/davealone69-gif/3d-mega-hub/releases/tag/phone-test-ff9e6ba), published by run [43740439495](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43740439495). Certificate SHA-256 `cc582fd0edb055fee88fce52a0245c1ddad41220b4d356ca0e8a7835e3eebb3c`. Uninstall the earlier build first.
- **CI emulator:** the render smoke runs five tests; the new one is `GlbClipPreviewTest`, and it is the first to look at the glTF viewer's pixels. Evidence line of the run: clips "Wave,March,Bow", tapped clip "March", 24 frames drawn, 1,332 of 9,216 sampled points unlike the background, 350 changed between two times of Wave, 0 after returning. Pause, loop, speed and the seek slider of that viewer have no test. A screenshot of the screen with the strip and the Motion section is stored by the commit after (`786015b`); it is evidence, nothing judges it.
- **One failed run, and its cause.** On `787e030` (test and documents only; the app is that of `ff9e6ba`) four workflows passed and the diagnostic workflow failed, run [43740886806](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43740886806): `GeneratedAvatarRuntimeTest` reported "PixelCopy of the animated view failed expected:<0> but was:<2>". Result 2 is a time-out: the copy gives the GPU half a second to finish the frame, and the emulator took ten to eleven seconds a frame in that run. The first part of the same test had verified the picture (323,702 geometry pixels), and the same test passed on the same commit in the runtime workflow ([43740889522](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43740889522)). That capture made one attempt; since `848632d` it asks again on a time-out, as the other tests already did. Not a fault of the app, but a test that could fail by chance in the gate for a phone-test APK.
- **Host, 2026-11-15:** `test_figures_to_clips.py`, 24 checks. The owner's file converted, compared with its source by CPU skinning and validated. The message of commit `ff9e6ba` says 23 checks; there are 24.

Workflows on the code commit `96152cd`:

| Workflow | Run | Result |
|---|---|---|
| Android CI | [43725536116](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43725536116) | success: lint, unit tests, native C++ sampling regression, R8, debug APK build, GLES render smoke with four instrumented tests |
| AvatarForge diagnostic APK and tests | [43725538815](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43725538815) | success, all three jobs |
| AvatarForge generated-avatar runtime verification | [43725541882](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43725541882) | success |
| Release preview and Android emulator smoke | [43725541215](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43725541215) | success, all three jobs: the debug and the signed, minified release APK install, start and load the bundled sample in the changed glTF viewer |
| Publish phone-test APK (rules self-test) | [43725541813](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43725541813) | success |

- **APK of `96152cd`:** [3D-Mega-Hub-debug-96152cd.apk](https://github.com/davealone69-gif/3d-mega-hub/releases/download/phone-test-96152cd/3D-Mega-Hub-debug-96152cd.apk), 57,803,456 B, SHA-256 `b4b08fce24a91abb91888a568bf77c5a7ab92e3a1bba6435ddebdcd48d25787a`, pre-release [`phone-test-96152cd`](https://github.com/davealone69-gif/3d-mega-hub/releases/tag/phone-test-96152cd), published by run [43726780618](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43726780618). Certificate SHA-256 `7d6821ec3c19862e3ce6d898064197f4937ef744d90a8305cea7eb4b1859e911`.
- **`50bbe25`**, the commit before it: the same five workflows passed ([43724124384](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43724124384), [43724126817](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43724126817), [43724129255](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43724129255), [43724129545](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43724129545), [43724129252](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43724129252)). No APK was published from it, see above.
- **CI emulator:** the render smoke now runs four tests; the new one is the JPEG texture test. The generated-avatar tests are those of `2920265`. No instrumented test imports a model with several textures or looks at the glTF viewer's pixels: the viewer's picture is evidence by screenshot, judged by eye.
- **Host, 2026-11-15:** `imported_glb_pbr_test` (with and without the importer change), `glb_extension_test` and the import fuzz test (8,000 mutated files) were run on `96152cd`; the other host tests were left to CI, where the native jobs passed. The owner's six GLB files of the second folder were imported with the engine's host tools: all accepted.

Workflows on the code commit `7e4f0cb`:

| Workflow | Run | Result |
|---|---|---|
| Android CI | [43719099722](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43719099722) | success: lint, unit tests, native C++ sampling regression, R8, debug APK build, GLES render smoke |
| AvatarForge diagnostic APK and tests | [43719102303](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43719102303) | success, all three jobs |
| AvatarForge generated-avatar runtime verification | [43719104445](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43719104445) | success |
| Release preview and Android emulator smoke | [43719104559](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43719104559) | success, all three jobs: the debug and the signed, minified release APK install, start and load the bundled sample in the glTF viewer |
| Publish phone-test APK (rules self-test) | [43719104513](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43719104513) | success |

- **APK:** pre-release [`phone-test-7e4f0cb`](https://github.com/davealone69-gif/3d-mega-hub/releases/tag/phone-test-7e4f0cb), file [`3D-Mega-Hub-debug-7e4f0cb.apk`](https://github.com/davealone69-gif/3d-mega-hub/releases/download/phone-test-7e4f0cb/3D-Mega-Hub-debug-7e4f0cb.apk), 57,803,456 bytes, SHA-256 `160635e5a8bf3b59b21c23ccff309ebf2ce10aa0a4bea411c0d2e61b41cdefc1`, built by run [43719102303](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43719102303), published by run [43720447663](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43720447663) at 2026-11-15T13:43:25Z. Uninstall any earlier build first.
- **CI unit tests:** `:app:testDebugUnitTest` in run [43719099722](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43719099722), "BUILD SUCCESSFUL" (read in the job log).
- **CI emulator:** the generated-avatar tests are those of `2920265` and passed again (323,702 geometry pixels in the published APK's own test). No instrumented test imports a model with several textures, so the new routing, the texture reduction and the viewer's gestures have not run on any device or emulator.
- **Host:** the engine and its `ctest` suite are unchanged since `2920265` and were not run again. The owner's three models were imported with the engine's host tools at `e4655c3`.

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

1. **BLOCKED on a phone test.** Nothing written since the report has run on the Samsung. The APK: [3D-Mega-Hub-debug-ff9e6ba.apk](https://github.com/davealone69-gif/3d-mega-hub/releases/download/phone-test-ff9e6ba/3D-Mega-Hub-debug-ff9e6ba.apk), 57,803,456 B, SHA-256 `bb24b5afb8898196de7cb0d6c3636c73b58620825bfe43d8824efc7b75723a6a`, pre-release [`phone-test-ff9e6ba`](https://github.com/davealone69-gif/3d-mega-hub/releases/tag/phone-test-ff9e6ba), published by run [43740439495](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43740439495). Uninstall the earlier build first.
2. **What to try on the phone, imported models:** "Import GLB" with one of the `body-raw*.glb` files or `model2.glb` (it should come up textured, on the app's own renderer, and turn with one finger), and with `ink.glb` (it should open in the glTF viewer with smooth edges, soft shadows and no black background). Say which files were slow or closed the app. **A file that moves:** "Import GLB" with `one_figure_14_clips_from_beautiful_realistic_undressed_girls.glb` from the Dropbox folder `/avatar/from-agent/models/`. It should open as one figure facing you with a row of 15 named chips under it; tap a few and say whether she moves, and how smoothly.
3. **What to try on the phone, the creator:** in the creator, Look (tap an ethnicity card), Body (a body type, a breast size, a butt size), Hair (a colour tile and a style), Face (one slider), Outfit (Swimwear shows the body best), then Create avatar; tap the animations in the strip under the avatar (Idle, Walk, Wave, Jump); Motion for pause, loop and speed; Save; and "Light" in the header. Then send back `renderer_diagnostics.json` and `renderer_telemetry.json` from Downloads/3D Mega Hub Diagnostics, a screenshot of the creator, and say what was on screen if anything closes the app.
4. **UNVERIFIED on the phone and worth measuring:** the frame rate with an animation playing (nothing is culled then), generation time and memory, and whether the GPU-skinned picture is right: the emulator test proves that the picture changes, not that the pose is correct.
5. **Not done:** a seek slider for the app's own avatars; a button row in the Motion section of an imported file that fits a phone's width ("Next" is out of sight until the row is swiped); blending between clips; tests for pause, loop and speed; Accessories and Background sections; a helmet for the spacesuit; a step-by-step flow like the picture reference's; male pictures on the cards; a face or a body of the quality of the owner's references.
6. **Imported files with their own clips** open in the glTF viewer. Choosing a clip by its chip is tested on the emulator with a test figure of boxes (`GlbClipPreviewTest`, since `ff9e6ba`), and one screenshot shows the Motion section with its slider moving. Pause, loop, speed and seeking by hand have no test, and none of it has been used on a phone. The file to try is the converted one in the owner's Dropbox (`/avatar/from-agent/models/`); a small public one is [`CesiumMan.glb`](https://github.com/KhronosGroup/glTF-Sample-Assets/raw/main/Models/CesiumMan/glTF-Binary/CesiumMan.glb) (438,044 B, CC BY 4.0).
7. **Production signing** needs the owner's keystore.

Statements further down this file that this section corrects:

- "A 75-joint rig … verify exact counts": the engine in this repository builds 71 joints, counted in the exported GLB and on the emulator.
- "telemetry reported 0 joints on the displayed mesh": that was the import discarding the skeleton. Fixed in `b636f23`; the emulator now reports 71. The phone has not run the fix.
- "currently auto-plays the first clip and does not establish user-facing clip controls": the integration branch has since added clip selection, play and pause, stop, loop, speed and seeking to the Filament preview. They compile and pass lint in CI. Since `ff9e6ba` choosing a clip is tested on the emulator; none of them has been used on a phone.
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

## Latest APK monitor snapshot: 2026-11-16

**Observed PR #9:** open, targeting `main`; canonical branch `integration/avatarforge-renderer-port`; exact observed head before this README update: `f73eec156b1baf6d105b4e891d8db45f22e66f3f`. PR #9 remains unmerged. No source files were changed by this monitoring run.

### Exact-head workflow runs

| Workflow / run | Result | Job-level evidence |
|---|---|---|
| [Android CI #638](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43706109624) | **FAILED** | Unit tests, debug APK build, Android lint and R8/minification passed. Native C++ sampling regression failed to compile in job [132787460667](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43706109624/job/132787460667): `native_sampling_test.cpp:36` refers to `REGION_HAIR` without its namespace; compiler identifies `af::REGION_HAIR`. Instrumented smoke job [132787460620](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43706109624/job/132787460620) failed. |
| [Release preview and Android emulator smoke #447](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43706109650) | **SUCCESS, not a renderer readiness pass** | Signed release preview and emulator install/launch smoke passed. The release workflow creates a one-run ephemeral CI key; this is not the owner's reusable production signing identity. The emulator job is install/launch smoke only. |

### Required production generated-GLB-to-GLES SurfaceView gate

**NOT RUN on this head.** The source contains the intended production test, `GeneratedAvatarRuntimeTest.generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView()`, in `app/src/androidTest/java/com/davemega/hub/avatarforge/GeneratedAvatarRuntimeTest.kt`. It generates and validates a GLB, attaches the production `AvatarForgeSurfaceView` to the app Activity, then requires visible-pixel, draw-call, multi-frame and screenshot evidence.

However, the executed Android CI script explicitly filters instrumentation to `com.davemega.hub.AvatarForgeRendererSmokeTest`; it does not select `GeneratedAvatarRuntimeTest`. The executed smoke suite therefore does not prove the required production generated-GLB-to-GLES SurfaceView path.

The executed smoke job itself failed before completing its three-test suite. Its generated-avatar debug diagnostic crashed with `OutOfMemoryError` while allocating 134,217,744 bytes; the crash stack identifies `FilamentPreviewView.readBounded(FilamentPreviewView.kt:446)`. This is the debug diagnostic's Filament file-preview route, not a passing production AvatarForge SurfaceView test. The uploaded renderer evidence artifact does not convert this failed run into a pass.

### APK artifacts and inspection boundary

Artifacts were produced for this exact SHA:
- [Debug APK artifact 14158508686](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43706109624/artifacts/14158508686), from Android CI #638.
- [Signed preview artifact 14158503818](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43706109650/artifacts/14158503818), from release preview #447; signed with a one-run ephemeral key.

APK package ID, APK signing certificate identity/signature and APK-file SHA-256 were **NOT INSPECTED** in this run because the mandatory production render test did not pass. The artifact ZIPs and successful install/launch smoke are not sufficient evidence.

**Readiness decision: NOT READY.** The required production test was not executed; the executed generated-avatar diagnostic crashed; the native C++ regression did not compile; and APK-level identity/signature/hash inspection remains outstanding. Do not distribute either artifact as the requested ready test APK.

**Next-run requirement:** first re-read PR #9 and the exact branch head. This README update creates a new commit and changes the branch head, so none of the `f73eec156b1baf6d105b4e891d8db45f22e66f3f` workflow results may be reused as exact-head evidence for the resulting commit. Require a fresh run that actually invokes `GeneratedAvatarRuntimeTest.generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView`, then inspect the same-head APK artifact itself.

## Latest handover: 2026-11-15

**Overall avatar-comparison status: MEASURED_WITH_GAPS. No verdict.** This update records the complete supplied aligned cross-section report. Artifact identities, geometry measurements and classifications below are agent-reported; they have not been independently recomputed in this repository workspace. This is not a build, renderer-runtime or phone-verification claim.

### Step 1 — preserved artifact identities

| Artifact | Reported path | Bytes | SHA-256 | Status |
|---|---|---:|---|---|
| Generated female LOD0 | `/workspace/3d-mega-hub-canonical/exports/avatar_default_female_LOD0.glb` | 83,430,312 | `56b072cb43203fe9c2ca0b231d28a3601b59914465647fa1da95a6890daccd6c` | Report says required size/hash match |
| Reference `mylove.glb` | `/home/ubuntu/upload/eYPzGGq49EUyWbNcDwqwN2/mylove.glb` | 9,287,272 | `8ef94a008b79a2ec8524962d810d92716f03f8476726dd8d8b7fc77227426a26` | Read-only identity recorded |

The reference copy's reported SHA-256 is distinct from the previously recorded Dropbox content hash. Do not conflate the two identifiers. The reference asset is private: do not commit, publish or redistribute it without explicit approval. The generated artifact is preserved and must not be modified.

### Step 2 — Y-axis alignment

PCA major-axis angles in XZ, modulo 180°:

| Measurement | Generated | Reference |
|---|---:|---:|
| PCA angle | −175.62054° (equivalent +4.37946°) | +178.33226° (equivalent −1.66774°) |
| Unique triangle-referenced world-space vertices used | 550,098 | 64,914 |

A relative **+6.04720° yaw about Y** was applied to generated geometry; reference orientation was fixed. Rotation convention:

`x′ = cos(θ)·x + sin(θ)·z`
`z′ = −sin(θ)·x + cos(θ)·z`

This is a whole-mesh, pose-sensitive PCA estimate, not independently validated anatomical registration. The raw-axis baseline is retained in Step 5; aligned measurements re-slice the generated geometry after rotation.

### Step 3 — common height definition

Both models use the same definition: full-scene world-space `Ymax − Ymin` after node transforms. Each slice is measured upward from that model's own `Ymin`. No scale or vertical registration was applied.

| Model | Y minimum | Y maximum | H |
|---|---:|---:|---:|
| Generated | 0.000000 m | 1.814858 m | 1.814858 m |
| Reference | −0.499926 m | 0.499917 m | 0.999842 m |

All reported widths and depths are dimensionless spans divided by that model's own `H`. Slice planes use `Y = Ymin + fraction × H`.

### Step 4 — torso-only analysis

<badge color="warning">INCOMPLETE</badge>

No defensible limb mask could be derived from mesh geometry alone. Contour-component counts at normalized heights were:

| Y/H | Generated components | Reference components |
|---:|---:|---:|
| 0.10 | 2 | 26 |
| 0.25 | 2 | 41 |
| 0.50 | 1 | 25 |
| 0.75 | 1 | 103 |
| 0.90 | 9 | 35 |

A largest-contour or central-contour rule would not reliably separate torso from limbs. No proxy mask was substituted. Therefore, these results are whole-mesh cross-sections, not torso-only measurements.

### Step 5 — complete whole-mesh cross-section measurements

Each pair below is `X-span/H / Z-span/H`. Ratios are **generated ÷ reference**. Reference values remain unchanged by the generated-model yaw.

| Y/H | Raw generated X/Z | Reference X/Z | Raw ratio X/Z | Aligned generated X/Z | Aligned ratio X/Z |
|---:|---:|---:|---:|---:|---:|
| 0.10 | 0.148075 / 0.058122 | 0.400646 / 0.234685 | 0.369591 / 0.247660 | 0.147574 / 0.067592 | 0.368340 / 0.288012 |
| 0.25 | 0.172452 / 0.086226 | 0.457947 / 0.197762 | 0.376576 / 0.436009 | 0.171962 / 0.095557 | 0.375506 / 0.483192 |
| 0.50 | 0.284834 / 0.176016 | 0.544902 / 0.143478 | 0.522725 / 1.226780 | 0.283525 / 0.181012 | 0.520323 / 1.261601 |
| 0.75 | 0.239902 / 0.140886 | 0.628299 / 0.209534 | 0.381828 / 0.672378 | 0.238919 / 0.143031 | 0.380263 / 0.682615 |
| 0.90 | 0.106442 / 0.139914 | 0.186629 / 0.154751 | 0.570340 / 0.904123 | 0.109387 / 0.143800 | 0.586120 / 0.929235 |

These are descriptive geometric spans. They do not establish named anatomy, anatomical similarity, or a pass/fail threshold.

### Step 6 — residual compatibility, not a verdict

The supplied report classifies the remaining differences as follows:

| Y/H | Reported compatibility | Limitation |
|---:|---|---|
| 0.10 | Compatible with height-registration residuals | Cause not established |
| 0.25 | Compatible with height-registration residuals | Cause not established |
| 0.50 | Unresolved between residual yaw and height registration | Cannot distinguish causes from these measurements |
| 0.75 | Compatible with height-registration residuals | Cause not established |
| 0.90 | Compatible with height registration; residual yaw not ruled out | Cause not established |

No cause is established and no pass/fail is assigned. In particular, the PCA yaw is not proof that the remaining differences are anatomical or that the models are correctly anatomically registered.

### Separate mirror-matching boundary

The mirror matcher remains a separate analysis. Previously reported maximum-cardinality matching did not match all IDs; this is not proof of anatomical asymmetry. Keep the provisional 4 mm criterion unchanged. It is not a threshold derived from this cross-section report and must not be loosened or treated as a substitute for anatomical validation.

### Evidence boundaries and next actions

- Preserve the generated LOD0 GLB exactly; do not alter or replace the recorded artifact.
- Keep `mylove.glb` private and read-only.
- Do not claim torso-only measurements unless a defensible, documented mask or independent anatomical landmarks become available.
- Do not turn compatibility labels into causal findings or an anatomical verdict.
- Keep mirror matching and its provisional 4 mm criterion separate from the cross-section analysis.
- No new Android build, emulator runtime test, or physical-device test is established by this report. Samsung SM-S921B verification remains **UNVERIFIED** unless the exact APK is installed and tested on the phone.

**Status: MEASURED_WITH_GAPS — no verdict.**

## Consolidation decisions

The live branch and pull-request audit selected `integration/avatarforge-renderer-port` as the canonical integration line.

- WebP decoding/source fallback, GLES renderer fixes and imported/generated GLB path are retained in the canonical line.
- The FileProvider capability and launcher icon are retained. The older FileProvider branch's replacement Compose-only activity and disconnected diagnostic overlay are **not** merged.
- The useful GLB fixture facts and user-confirmed display note from PR #11 are already in the canonical README. PR #11 was closed as superseded; its older README was not allowed to overwrite newer project information.
- The older Filament-generated-avatar path from PR #10 was not wholesale merged because it conflicts with the selected renderer split. Its useful deformation/bounds work is already represented in the canonical engine.
- Earlier shell, standalone cube, engine snapshots, Filament ancestors and generated Gradle cache changes are classified as superseded or historical. No generated cache files are to be imported.
- No historical branch is to be treated as an active work target. All further work goes onto the canonical branch.

See [PROJECT_STATUS.md](PROJECT_STATUS.md) for the detailed branch-by-branch audit and evidence ledger.

## Current architecture

- **AvatarForge generation:** isolated `:avatarforge-engine` Android library, JNI bridge and C++ generation kernel. Avatar Studio exposes prompt generation, cancellation, resource/thermal checks, native-result validation and atomic publication of validated GLBs.
- **3D viewer:** the bundled sample uses the separate Filament preview. Imported file-based GLBs now open in the existing Filament gltfio preview by default; users can explicitly switch to the native AvatarForge GLES preview for its static imported-model path. Generated avatars continue to use the native AvatarForge renderer.
- **Animated GLB playback (implementation committed; runtime verification pending):** the existing Filament `Animator` exposes clip selection, play/pause/stop-reset, looping, playback speed and timeline seeking; the first clip starts automatically. The controls appear in Avatar Studio when the selected GLB contains animation clips. This reuses Filament gltfio rather than adding a new animation engine. Latest implementation commits: `8b80b072187c1e471107fd9bb23ff8bdae9c4790` (default animation-capable preview for imported file GLBs) and `0584611d5ffcc8d7b89416d98f14299790c3450f` (preview-mode wording).
- **Rigged animation reference asset:** private Dropbox asset `/3d-mega-hub-diagnostics/3d-mega-hub/mylove_rig_prepared.glb`, 13,221,520 bytes, SHA-256 `656cf2446eefbffc92cf569ce1a2f52b6d0ad449c282fc5a4d911abfef576d14`. It remains read-only and is not committed to Git. Binary clip/joint counts have not been independently inspected in this workspace; playback with this exact asset is **UNVERIFIED**.
- **Native AvatarForge animation limitation:** the native GLB importer still parses but discards imported skin/joint/animation data when it resets its generated rig and clip caches. Therefore the new preview is a separate working-path candidate; it does not mean the native AvatarForge GLES importer now skins or animates imported GLBs. Exact-device and clip playback verification are still pending.
- **Graphics backend:** the imported/generated avatar path explicitly forces native GLES. Skinned GLBs apply the initial rest pose and refresh bounds before camera framing. Vulkan is not selected for this phone-first route.
- **Debug verification:** debuggable builds run visible-pixel verification for both imported GLBs and the deterministic seed-1337 generated-avatar diagnostic. The native GLES path waits for successful EGL swaps, captures the actual SurfaceView with PixelCopy, estimates the background from the four corners, counts foreground geometry pixels, applies the 5,000-pixel minimum and saves a PNG plus machine-readable JSON. Imported captures use `imported_glb_diagnostic.png`; generated captures use `generated_avatar_diagnostic.png`; failures use `failed_render.png`. Release/non-debug builds do not automatically capture every import. A scoped AndroidX FileProvider exposes only app-private external-files captures for sharing.
- **Avatar imports:** selected GLBs are copied into app-private storage, size-limited and structurally validated before use. External-resource `.gltf` files are rejected.
- **Local AI:** Ollama remains an external service, not an embedded LLM. Plain HTTP is restricted to loopback; remote endpoints require HTTPS.
- **Voice:** Android TextToSpeech output is connected. Viseme extraction and avatar lip-sync are not connected.

## Diagnostic-driven validation policy

**A passing diagnostic is reusable evidence for the exact APK, asset and device state it measured. It is not a reason to rebuild repeatedly.**

- If source code, build configuration, native libraries or packaged assets have not changed, do not rebuild just to repeat a passing diagnostic. Reuse the report and only rerun a diagnostic when the asset, device state, renderer lifecycle or suspected failure condition changes.
- After a coherent batch of source changes, perform one consolidated validation cycle against the exact resulting commit: native/unit tests, Android build/lint/R8, relevant renderer instrumentation, emulator install/launch and artifact capture. Do not build between individual edits in the same batch.
- A successful build proves compilation/package assembly only. A renderer diagnostic proves only the checks it actually performs. The diagnostic must identify its APK/source commit, asset hash, device/API/backend, capture method, thresholds and result before it can be reused as evidence.
- A previous APK's passing diagnostics do not validate changed source code. A newly built APK still needs one install/runtime validation on the target device before that new APK is called phone-verified.
- A generated-avatar diagnostic must not be used as a substitute for imported-GLB verification. Imported GLBs must independently pass actual surface capture and visible-pixel checks.
- FRAME_PRESENTED means EGL presented frames; it does not mean visible geometry was verified. PIXELS_VERIFIED requires a successful PixelCopy capture and at least 5,000 foreground geometry pixels after background subtraction. FAILED, capture timeout, missing screenshot or missing report must remain failures, not be silently converted into passes.
- In debug builds, imported GLBs now use the same PixelCopy visible-pixel gate as the generated diagnostic. This closes the previous evidence gap where ordinary imported-model reports could show visiblePixels: 0 simply because pixel verification had not been enabled. A zero count from a report without pixel verification enabled is NOT MEASURED, not proof of an invisible model.
- Preserve failure artifacts and diagnose them before another code change. Do not rerun an unchanged failing test repeatedly unless the next run changes one defined variable or captures missing evidence.

### Diagnostic evidence levels

| Result | What it proves | What it does not prove |
|---|---|---|
| STARTING | Renderer worker began | EGL or model loading |
| RENDERER_ACTIVE | Native renderer session and GLES context initialized | Geometry drawn or visible |
| FRAME_PRESENTED | Required EGL swaps, draw calls, triangles and frustum checks observed | Pixels reached the displayed surface |
| PIXELS_VERIFIED | PixelCopy captured foreground geometry above threshold | Model quality, correct appearance controls, animation or long-session stability |
| FAILED | A required diagnostic stage failed | Root cause, until failure details are inspected |

Diagnostic JSON should be treated as evidence, not a generic green light. A pass is scoped to the tested APK, asset and environment.

## Sample-model limitation

The bundled `female.glb` is a renderer fixture, not proof of a complete rigged avatar. Inspection of the exact bundled file shows one mesh, one node, seven morph targets, zero skins, zero animations and zero embedded images. It can exercise GLB loading/rendering, but does not prove skeletal animation, facial animation, texture quality or generated-avatar quality.

The fixture is byte-identical to the file at 3DDD revision `be5bf17d8afe46247646465941fa1b7317d8e415` (Git blob `a1c00c6b31f6ab0db3b2e68243526cb2d0e15967`). The donor README at that revision states the generated base rigs are original work released under CC0. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Latest renderer investigation — 2026-11-15

- **Working branch:** `integration/avatarforge-renderer-port` · **PR:** [#9](https://github.com/davealone69-gif/3d-mega-hub/pull/9) · **stable `main`:** unchanged at `aaee4443dbf53ef4707cce01d99f71a59e903ec7`.
- PR #8 is **CLOSED / UNMERGED**, head `fix/mega-hub-candidate-hardening` at `41c3069301dc772bad3aa8dba1a2cbf67f8110c9`. It is not the active implementation path.
- **Failure-observability fix:** `78cb75d81557623622f6989a285ebf958dea2844` preserves the first renderer exception and any teardown exception instead of overwriting them with a generic stopped status.
- **GLES capability fix:** `ae43e0e271684d56336005450765a4577aafc591` replaces invalid legacy `glGetString(GL_EXTENSIONS)` calls with GLES 3.x `glGetStringi` enumeration. Float-colour-buffer and anisotropy capability flags now reflect actual extension support; anisotropy limit is queried once rather than once per texture.
- **Last completed exact-head CI evidence (`2a5485c17ec656659c4841826a1048d1577b7301)):**
  - **PASS:** signed preview APK and AAB build, signature verification and provenance (run `43626247432`, job `132542586035`). Ephemeral CI key only, not production signing.
  - **PASS:** Android emulator install/launch smoke (run `43626247432`, job `132542586459`).
  - **PASS:** native host regression (run `43626247376`, job `132542664347`).
  - Renderer pixel tests and Android CI had not completed when this evidence was recorded.
- **Latest source head:** `ae43e0e271684d56336005450765a4577aafc591`. Fresh exact-head CI is required after the GLES capability change. **UNVERIFIED:** visible model pixels and Samsung SM-S921B runtime. **BLOCKED:** production signing until the owner's keystore is configured in protected CI secrets.
- Do not merge to `main` or weaken the real visible-pixel gate.

## Execution plan

Work through these steps in order, on the single canonical branch. Do not skip a failed gate or mark a later step complete on the strength of an earlier one.

1. **Finish one consolidated exact-head validation cycle after the current coherent change batch.** Run native host regression, Android unit tests, debug APK assembly, lint, R8/minification, imported-GLB visible-pixel instrumentation, generated-avatar visible-pixel instrumentation, and emulator install/launch smoke together. Do not run a build after each small edit in the same batch. Inspect all failed jobs and artifacts before deciding whether another source change is justified.
2. **Use diagnostics before rebuilding.** For runtime-only questions, inspect the existing JSON, PNG, logs and device/asset identity first. Reuse a passing report when the APK, asset and relevant environment are unchanged. Only rebuild after a source/asset change or when existing evidence is insufficient. A diagnostic pass does not waive the one consolidated validation needed for a new code revision. No fake success values, mocked renderer, or weakened pixel threshold.
3. **Prove emulator runtime.** Install the exact CI APK, launch the real app, open the relevant viewer route, load the fixture/imported GLB, and verify process stability, frame production, no ANR and a non-empty screenshot. A successful Gradle build alone is not an emulator pass.
4. **Prove the Samsung device.** Install that same candidate APK on the Samsung SM-S921B. Verify visible pixels, imported and generated GLB loading, app pause/resume, surface recreation, memory/resource handling and thermal behaviour. Record the APK SHA-256 and device evidence. Until this is done, physical-device rendering remains **UNVERIFIED**.
5. **Complete release signing.** Configure the owner's production keystore through protected CI secrets. Build and verify the signed release APK and AAB, check package/version/signature and record artifact SHA-256 values. A debug key or ephemeral CI preview key is not a production release.
6. **Review and merge once.** Update PR #9 with exact-head evidence. Merge to `main` only when all required CI and runtime gates pass and the review is clean. Keep `main` as the stable release line.
7. **Clean up only after proof.** Once the canonical branch is merged and the owner confirms the release, remove obsolete branch references and close superseded PRs. Until then, keep historical branches as recovery points, not active work streams.

## Current verification ledger

| Item | Status | Evidence / next action |
|---|---|---|
| Canonical branch selected | **REAL + VERIFIED** | One working branch and PR #9 selected; `main` untouched |
| Branch/PR audit | **REAL + VERIFIED** | Detailed dispositions recorded in [PROJECT_STATUS.md](PROJECT_STATUS.md) |
| Native host regression | **PASS (older exact source)** | Passed on application source `1deaf264032195989257845b884a5829f06fe9cf`; do not attribute to current head |
| Android unit tests | **PASS (older exact source)** | Passed on application source `1deaf264032195989257845b884a5829f06fe9cf`; do not attribute to current head |
| Debug APK build | **PASS (older exact source)** | Diagnostic workflow produced a 71,377,227-byte debug APK; historical artifact SHA-256: `3d833528d116fe0660a347d6a3bff0e9c6e7c8dc1d53b82125f15718917de8ee` |
| R8/minification | **PASS (older exact source)** | Passed on application source `1deaf264032195989257845b884a5829f06fe9cf`; not current-head evidence |
| Android lint | **CANCELLED (prior cycle)** | No pass claim |
| Basic generated-avatar pixel gate | **PASS (older exact source only)** | Run `43528943730`, source `e9e7d76ef4a3f12c5c6a79951b5a1a99570c6228`: basic generated-avatar pixel test passed on Android 11 emulator. This is not the production exported-GLB SurfaceView end-to-end test |
| Production generated-GLB-to-GLES SurfaceView | **FAILED on observed head** | Runs `43673389818` (job `132688476029`) and `43673389819` (job `132688165314`), head `d66eb22af0c82de12aeafccd70b283d44ddd5c52`: `generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView` failed. Assertion: no drawable model frame within 15 seconds; `frames=2`, `presented=2`, `draws=29`, `triangles=1626176`, bounds in frustum, `glError=0x0`, `eglError=0x0`. Required gate fails |
| Required Android build jobs | **PARTIAL / FAILED on observed head** | Android CI run `43673389854`: native host regression, R8, unit tests, lint and debug APK build passed; instrumented imported-GLB GLES render smoke failed with `OutOfMemoryError` allocating 134,217,744 bytes with about 25 MB free. The separate production generated-GLB test also failed as above |
| Fresh APK artifact | **PARTIAL on observed head** | Fresh debug APK artifacts exist for the exact observed head: run `43673389854`, artifact `14145098709` (`3D-Mega-Hub-debug-apk`), and run `43673389818`, artifact `14145536569` (`3D-Mega-Hub-generated-avatar-diagnostic-debug-apk`). GitHub ZIP digests are recorded in the latest monitor snapshot; APK package, signing certificate and APK SHA-256 have not been verified. Not a ready APK |
| Current branch head after this README update | **NEW HEAD; VALIDATION PENDING** | The monitor snapshot below applies to parent head `d66eb22af0c82de12aeafccd70b283d44ddd5c52`. This README commit changes the PR head; its own exact-head Actions evidence must be checked separately |
| Samsung SM-S921B runtime | **UNVERIFIED** | Requires installation and testing on the physical phone |
| Production signing | **BLOCKED** | Requires owner-provided production keystore in protected CI secrets |
| Merge to `main` | **BLOCKED BY GATES** | Do not merge until every required gate passes |

## Strict PR #9 test APK readiness gate

A test APK is **READY** only when every condition below is independently proven for the same current PR head commit:

1. The production generated-GLB-to-GLES `SurfaceView` runtime instrumentation test passes on that exact head. A generated-avatar unit test, basic pixel test, imported-GLB test, emulator install-only smoke, or unrelated green check is not a substitute.
2. Every required Android build job passes on that exact head, including the required Android compilation, unit/lint/R8/native and renderer test jobs configured for this PR.
3. A fresh APK artifact is produced by a workflow run for that exact head. A stale artifact or artifact from a different SHA is rejected.
4. Download the artifact and verify the APK itself: package/application ID, signing certificate/signature, and APK SHA-256. Record the exact artifact URL, source commit, package, signing identity and digest.
5. Correlate workflow run head SHA, test result, artifact provenance and APK inspection. If any link is missing or ambiguous, status remains **PARTIAL**, **UNVERIFIED** or **BLOCKED**, never READY.

Do not merge PR #9 to `main`. Do not send a readiness notification until all five conditions are satisfied. On the first fully verified ready APK, notify the owner with the exact commit SHA, direct artifact download link, APK SHA-256 and production render-test evidence, then stop monitoring. Until then, keep checking the latest branch head and its associated Actions runs on the scheduled cadence; report failures internally/in the README and continue without a readiness notification.

**Latest monitor snapshot before this README-only commit (2026-11-14):** PR #9 was open and targeted `main`; branch head was `705165eebdaa525d75be861763c9a11205f33634`. The production generated-GLB-to-GLES SurfaceView test failed in run `43534561147`; required Android CI run `43534561148` failed its instrumented GLES renderer smoke. A fresh debug APK existed, but APK package/signature verification was incomplete. Therefore the prior head was **NOT READY**. This README update creates a new head, so the next scheduled check must start from the new head and must not reuse the prior head's evidence as a pass.

## Known boundaries and missing features

- The AvatarForge donor pin is `9e36197ace3b18faa6c566d93f8fce89b185c45d` from `work/android-engine-dev`. It is divergent, not a fast-forward ancestor, of the standalone AvatarForge RB-01 line (`0e782166…` / `fc30efd0…`). Several generation-core files differ in both directions. No wholesale replacement was attempted; any core-only port needs a file-by-file API review and native tests. AvatarForge remains a separate project.
- Prompt generation is deterministic native parsing, not photo-to-identity reconstruction and not an embedded LLM.
- External-resource `.gltf` is unsupported; the current viewer path is self-contained GLB.
- Voice lip-sync/visemes are missing.
- The bundled fixture is not a rigged/animated model.
- Samsung SM-S921B visible rendering, real-device generation, import lifecycle, memory and thermal behaviour remain **UNVERIFIED** until the phone test is completed.

## Older debug build

[Download the previously published debug APK](https://github.com/davealone69-gif/3d-mega-hub/releases/download/android-build-46/3D-Mega-Hub-debug.apk). This is an older troubleshooting build, not the current integration candidate and not a production release.

## Current priority: avatar quality first

**Priority correction (2026-11-13):** the owner has now physically tested the latest APK and reports that the renderer displays avatars, the generation button changes avatar features (including male/female), and GLB import works. Skin-tone and hair changes have not yet been tested. The owner also reports that the avatar quality is currently too poor to judge appearance changes reliably.

These are **OWNER-TESTED** results for the APK the owner installed. The exact APK SHA-256 and source commit used for that phone test have not been supplied, so do not attribute the result to a specific repository head or call the current PR head device-verified.

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


## Cross-project reuse audit — 2026-11-15

See [the branch and private-repository reuse audit](docs/PROJECT_REUSE_AUDIT_2026-11-15.md). AvatarForge's standalone engine has a real adult-only unclothed base surface in source, but this Hub UI still selects `Outfit.JEANS_JACKET` and does not expose the adult confirmation gate. The closest real 3D-video donor is 3DDD's Filament `MediaCodec`/`MediaMuxer` exporter; its camera update and per-frame animation sampling need correction before reuse. No detailed anatomical mesh donor was found. The prepared Dropbox rig remains **UNVERIFIED** until its binary is inspected.

## Bounded GLB import allocation fix (2026-11-17)

- Replaced the in-memory `ByteArrayOutputStream` accumulation path with `BoundedGlbReader`: the input is streamed to a bounded app-cache spool, then read into one exact-sized byte array. The configured 128 MiB import ceiling remains enforced; empty and oversized input are rejected and the temporary file is removed on success or failure.
- Added JVM regression tests for exact byte preservation, oversized/empty rejection and temporary-file cleanup. Corrected the native sampling test's `REGION_HAIR` reference to its namespaced enum.
- Exact-head `testDebugUnitTest` was started for this working tree; its result must be recorded from the completed Gradle output. No phone runtime or generated-avatar render pass is claimed. The earlier imported-GLB OOM remains a separate required gate until the exact-head test and emulator/device evidence pass.
- Root `.kotlin/` is a local generated cache and is ignored, not committed.

## Exact-head Android test APK readiness monitor (2026-11-17)

**Decision: NOT READY.** PR #9 remains open against `main`. At inspection, PR head and `integration/avatarforge-renderer-port` both resolved to `37561b19f266cd1d0d7d849b6441c47e2456121e`. The complete Actions run query for this SHA returned two runs, both push-triggered:

- [Android CI run 43957608408](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43957608408) — overall **FAILURE**.
  - Native C++ sampling regression, job `133616261791`: **PASS**.
  - Unit tests, job `133616262204`: **PASS**.
  - Android lint, job `133616262287`: **PASS**.
  - Release R8/minification, job `133616262477`: **PASS**.
  - Debug APK build, job `133616262558`: **PASS**.
  - AvatarForge GLES GLB render smoke, job `133616262483`: **FAIL**. Job log records `AvatarForgeRendererSmokeTest#generatedAvatarDebugDiagnosticVerifiesImportedGlesPixels` failed, followed by “Instrumentation run failed due to Process crashed.” The available log does not identify a more specific fatal cause. This job runs the separate renderer-smoke test class; it is not evidence that the required dedicated `GeneratedAvatarRuntimeTest#generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView` passed.
- [Release preview and Android emulator smoke run 43957608293](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43957608293) — overall **SUCCESS**.
  - Signed release preview with a one-run ephemeral CI key, job `133616200164`: **PASS**.
  - Emulator install/launch smoke, job `133616200821`: **PASS**. This is not the production render-test gate and the ephemeral signing key is not a retained production identity.

### Exact-SHA artifacts

- Fresh debug APK artifact [14271344075](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43957608408/artifacts/14271344075) was produced by the Debug APK build job for this exact SHA.
- Fresh signed-preview artifact [14271949045](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43957608293/artifacts/14271949045) was produced by the release-preview run for this exact SHA.
- Renderer diagnostic evidence artifact [14274450108](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43957608408/artifacts/14274450108) was uploaded by the failed renderer job.
- These artifacts are not a READY APK. The APK binaries have not been downloaded and inspected for package ID, signing certificate/signature and APK SHA-256 because the mandatory production render test did not pass.

### Mandatory production render test: missing on this SHA

The required test exists in source as `GeneratedAvatarRuntimeTest#generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView`. The dedicated diagnostic workflow contains a step that invokes this exact method, and the generated-runtime workflow invokes the containing test class. **Neither workflow has a run associated with this exact SHA.** The full exact-head Actions run list contains only the two runs above. The available imported-GLB/GLES smoke failure and successful emulator launch cannot substitute for the missing production generated-GLB-to-GLES SurfaceView result.

### Decision and next check

**NOT READY:** the required production test has no exact-head run/pass, and the Android CI renderer smoke job failed. All ordinary build jobs passed and fresh exact-head APK artifacts exist, but neither fact overrides the missing/failed render gate. Do not distribute either artifact as the first READY APK. No APK signing identity or APK digest has been independently inspected for this head.

This monitor update is documentation-only. It does not modify application source, merge PR #9, or modify `main`. The README commit itself will create a new branch head and invalidates all evidence above as proof for that new head; the next check must fetch the new exact PR/branch SHA and inspect only its associated workflow runs and artifacts.


## Exact-head Android test APK readiness monitor (2026-11-18)

**Decision: NOT READY.** PR #9 remains open and unmerged. At inspection, both PR #9 and `integration/avatarforge-renderer-port` pointed to `4ce9e803f839f36e69fa4bfdd927caf5cb44e1a3`.

### Exact-head Actions

- [Android CI run 44029670654 (run 662)](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44029670654): **FAILURE**.
  - Unit tests, job `133858290397`: PASS.
  - Debug APK build, job `133858290360`: PASS.
  - Native C++ sampling regression, job `133858290350`: PASS.
  - Android lint, job `133858290619`: PASS.
  - R8/minification, job `133858290505`: PASS.
  - AvatarForge GLES GLB render smoke, job `133858289938`: **FAIL**.
- [Release preview and Android emulator smoke run 44029670636 (run 460)](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44029670636): SUCCESS. Emulator install/launch and ephemeral-key preview are not the production render gate.

### Production render test is not being invoked

The required test exists at `app/src/androidTest/java/com/davemega/hub/avatarforge/GeneratedAvatarRuntimeTest.kt` as `GeneratedAvatarRuntimeTest#generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView`. It generates a GLB and requires the production `AvatarForgeSurfaceView` to show visible pixels, submit draw calls, present multiple frames and save a screenshot.

However, `.github/scripts/avatarforge-render-smoke.sh` currently invokes only `com.davemega.hub.AvatarForgeRendererSmokeTest`. The exact-head instrumentation log confirms the executed test was `AvatarForgeRendererSmokeTest#generatedAvatarDebugDiagnosticVerifiesImportedGlesPixels`; it does not show the required `GeneratedAvatarRuntimeTest` method running. Therefore the mandatory production generated-GLB-to-GLES SurfaceView test has **NO PASSING RESULT on this SHA**.

The executed imported-GLB test failed. Its uploaded [renderer evidence artifact 14305007547](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44029670654/artifacts/14305007547) contains logcat showing Filament aborting while reading the selected GLB:

```text
utils::PreconditionPanic
reason: uniform named "sheenColorIndex" not found
Fatal signal 6 (SIGABRT)
```

This is the observed failure in the imported-GLB preview path. It does not prove the production generated-avatar path has been exercised.

### Exact-head APK artifact

A fresh debug APK artifact exists for this exact source SHA:

- [Debug APK artifact 14304801634](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44029670654/artifacts/14304801634), from the Debug APK build job on this SHA.
- Renderer evidence artifact: [14305007547](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44029670654/artifacts/14305007547).
- Signed preview artifact: [14305430345](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44029670636/artifacts/14305430345), signed with an ephemeral CI key.

The debug APK binary has **not** been downloaded and inspected in this check for package ID, signing certificate/signature and APK SHA-256. Those fields remain **UNVERIFIED**. Artifact existence and a successful emulator install/launch do not override the failed renderer job or missing production test execution.

### Decision and next check

**NOT READY.** Required next work is to wire the exact `GeneratedAvatarRuntimeTest#generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView` into the CI instrumentation command, fix the observed Filament `sheenColorIndex` abort in the separate imported-GLB preview path, then rerun on one exact current head. Only after the production test passes and all required Android build jobs pass should the fresh same-SHA APK be downloaded and its package, signature and SHA-256 verified.

This entry is documentation-only. It does not change application source, modify `main`, or merge PR #9. Its README commit creates a new branch head, so the run and artifact evidence above applies only to `4ce9e803f839f36e69fa4bfdd927caf5cb44e1a3`; fresh exact-head Actions evidence is required for the new head.


## Exact-head Android test APK readiness monitor (2026-11-18, head `6a745d260997e3656eafcd2d316f8afafef35e94`)

**Decision: NOT READY.** PR #9 remains open and unmerged. PR #9 head and `integration/avatarforge-renderer-port` both resolved to this exact SHA when checked.

### Exact-SHA workflow runs

- [Android CI run 44237202777](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44237202777): **FAILURE**.
  - Debug APK build, job [134576085121](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44237202777/job/134576085121): **PASS**.
  - Native C++ sampling regression, job 134576085581: **PASS**.
  - Release R8/minification, job 134576085601: **PASS**.
  - Android lint, job 134576085616: **PASS**.
  - Unit tests, job 134576085674: **PASS**.
  - AvatarForge GLES GLB render smoke, job [134576085510](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44237202777/job/134576085510): **FAIL**. Its job log shows `AvatarForgeRendererSmokeTest#generatedAvatarDebugDiagnosticVerifiesImportedGlesPixels` failed, followed by `Instrumentation run failed due to Process crashed`; Gradle reports `:app:connectedDebugAndroidTest` failed. The log excerpt does not establish a more specific crash cause.
- [Release preview and Android emulator smoke run 44237202697](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44237202697): **SUCCESS**.
  - Signed release preview using an ephemeral one-run CI key, job 134576084735: **PASS**.
  - Emulator install/launch smoke, job 134576084984: **PASS**. This is not production renderer proof.

### Mandatory generated-GLB production SurfaceView test

The required test exists in source at `app/src/androidTest/java/com/davemega/hub/avatarforge/GeneratedAvatarRuntimeTest.kt` as `GeneratedAvatarRuntimeTest#generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView`. It generates a GLB and exercises the production `AvatarForgeSurfaceView`, requiring visible pixels, draw calls and presented frames.

However, the exact-head workflow script `.github/scripts/avatarforge-render-smoke.sh` invokes only `com.davemega.hub.AvatarForgeRendererSmokeTest`. The completed exact-SHA job log confirms that the imported-GLB test above ran and failed; it contains no execution or pass result for the required `GeneratedAvatarRuntimeTest` method. Thus the required production generated-GLB-to-GLES SurfaceView test is **MISSING on this SHA**, not passed.

### Exact-SHA artifacts

- Fresh debug APK artifact [14398465631](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44237202777/artifacts/14398465631) was uploaded by the debug build job for this exact SHA. Artifact archive digest: `sha256:032cb1810f957c4be233cb8dda8f776931b54b437cc1c18bb41c8b35f6565d52`.
- Failed-render diagnostic artifact [14398258248](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44237202777/artifacts/14398258248) was uploaded by the failed renderer job.
- Signed preview artifact [14398276866](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44237202697/artifacts/14398276866) was produced for this exact SHA using an ephemeral CI signing key.

The APK archive was **not downloaded or inspected** for package ID, APK signing certificate/signature or APK-file SHA-256. Those values remain **UNVERIFIED**. Artifact archive digests are not APK-file hashes.

### Decision and next check

**NOT READY.** The production generated-GLB-to-GLES SurfaceView test did not run; the only executed imported-GLB GLES instrumentation test failed and its process crashed. Ordinary build jobs and emulator install/launch passed, and fresh same-SHA APK artifacts exist, but none overrides the missing/failed renderer gate. Do not distribute either APK artifact as the first READY test APK.

This entry is documentation-only. No application source was changed, `main` was not modified, and PR #9 was not merged. This README update creates a new branch head, so all evidence above applies only to `6a745d260997e3656eafcd2d316f8afafef35e94`; the new head requires fresh exact-SHA workflow runs and artifact evidence.

## Launcher branding update — 2026-11-18

- Added the electric-blue / silver wolf artwork as the Android launcher icon resource at `app/src/main/res/drawable-nodpi/ic_launcher_art.png`.
- Added adaptive launcher icon definitions for Android 8.0+ and legacy bitmap resource wrappers for earlier Android versions.
- Updated the manifest to use `@mipmap/ic_launcher` and `@mipmap/ic_launcher_round`.
- The icon change is committed on `integration/avatarforge-renderer-port`; `main` remains untouched.
- **Build status: UNVERIFIED** until GitHub Actions builds the exact new head. **Physical Samsung launcher appearance: UNVERIFIED** until installed and checked on the SM-S921B.


## Exact-head Android test APK readiness monitor (2026-11-19, inspected head `ce45178d01c9e45704cea5e1ee59836ccf4c6489`)

**Decision: NOT READY.** PR #9 is open and unmerged. Fresh metadata showed PR #9 head `integration/avatarforge-renderer-port` and the branch ref both point to `ce45178d01c9e45704cea5e1ee59836ccf4c6489`.

### All Actions runs associated with this exact SHA

The complete `actions/runs?head_sha=ce45178d01c9e45704cea5e1ee59836ccf4c6489&per_page=100` query returned two runs:

- [Android CI run 44331554038](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44331554038), attempt 2: **FAILURE**.
  - Native C++ sampling regression, job [135123928802](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44331554038/job/135123928802): **PASS**.
  - Unit tests, job [135124007631](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44331554038/job/135124007631): **PASS**.
  - Android lint, job [135124000193](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44331554038/job/135124000193): **PASS**.
  - Release R8/minification, job [135123999791](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44331554038/job/135123999791): **PASS**.
  - Debug APK build, job [135123930415](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44331554038/job/135123930415): **PASS**.
  - AvatarForge GLES GLB render smoke, job [135123926596](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44331554038/job/135123926596): **FAIL**. Its log shows only `AvatarForgeRendererSmokeTest#generatedAvatarDebugDiagnosticVerifiesImportedGlesPixels` completed and failed; instrumentation then reported `Instrumentation run failed due to Process crashed`. The job script invokes the `AvatarForgeRendererSmokeTest` class, not the required production generated-GLB test.
- [Release preview and Android emulator smoke run 44331554271](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44331554271): **SUCCESS**.
  - Signed release preview with a one-run ephemeral CI key, job [134897213931](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44331554271/job/134897213931): **PASS**.
  - Emulator install/launch smoke, job [134897214639](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44331554271/job/134897214639): **PASS**. This is not the production render gate.

### Exact-SHA artifacts

- Fresh debug APK artifact [14440245997](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44331554038/artifacts/14440245997), uploaded by the successful Debug APK build job.
- Signed preview artifact [14439722134](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44331554271/artifacts/14439722134), produced by the ephemeral-key preview job.
- Renderer diagnostics artifact [14471562313](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/44331554038/artifacts/14471562313), uploaded by the failed renderer job.

These are all tied to source SHA `ce45178d01c9e45704cea5e1ee59836ccf4c6489`. APK package ID, APK signing certificate/signature and APK-file SHA-256 have **NOT BEEN INSPECTED**. The artifacts do not qualify for distribution because the mandatory production render gate failed or did not run.

### Required production generated-GLB-to-GLES SurfaceView test: MISSING

The test exists at `app/src/androidTest/java/com/davemega/hub/avatarforge/GeneratedAvatarRuntimeTest.kt` as `GeneratedAvatarRuntimeTest#generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView`. The exact-SHA workflow `.github/workflows/android.yml` runs `.github/scripts/avatarforge-render-smoke.sh`, which explicitly selects `com.davemega.hub.AvatarForgeRendererSmokeTest`. The separate `.github/workflows/avatarforge-generated-runtime.yml` and `avatarforge-diagnostic-apk.yml` files contain generated-avatar test commands, but neither has a workflow run associated with this SHA. No passing production generated-GLB-to-GLES SurfaceView result exists for this head.

### Decision

**NOT READY.** Ordinary Android build jobs passed and fresh APK artifacts exist, but the renderer instrumentation job failed on the imported-GLB diagnostic test and the required production generated-GLB SurfaceView test has no exact-SHA run. Do not distribute these artifacts as the first READY APK. No APK binary identity/signature/digest inspection is claimed.

This monitor entry is documentation-only. It does not alter application source, merge PR #9 or modify `main`. The README commit itself creates a new branch head and invalidates all run/artifact evidence above as proof for that new head. The next check must fetch the new PR and branch SHA and inspect only that exact SHA's workflow runs and artifacts.

## Branch convergence handover — 2026-11-26

Prepared on `arena/1535c8f6-3d-mega-hub`. This section replaces the branch guidance above it wherever the two disagree, and it exists because parallel sessions kept re-deriving the same features from different stale bases. The analysis behind it is [`docs/BRANCH_CONVERGENCE_PLAN.md`](docs/BRANCH_CONVERGENCE_PLAN.md).

### What was done

| Commit | What it is |
|---|---|
| `07610b5` | Screen-density avatar LOD selection. Replaces per-submesh metre thresholds (`>= 16.0f ? 2 : >= 8.0f ? 1 : 0`) that the default 3.0 m / 32° orbit framing could never reach, which is why the app always drew LOD0 — the full ~1.1 M-triangle mesh. Now one level per avatar, chosen as the finest level meeting a 4.0 px projected-edge floor, with 1.25 hysteresis. Root cause #16 in [`PROJECT_STATUS.md`](PROJECT_STATUS.md) |
| `e6f28ba` | `docs/BRANCH_CONVERGENCE_PLAN.md` |
| `bdaef4b` | `docs/ANDROID_TEST_AGENT_PROMPT.md`, the device-verification brief |
| `88c2c67` | **Merge commit**: the whole trunk `9baf7c6` into this line. Two conflicts, both double work: `README.md` (documents only) and `FilamentPreviewView.kt`, where both lines had written bounded GLB reading. Kept the trunk's unit-tested `BoundedGlbReader`. The automatic merge had taken the trunk's deletion of the local `readBounded` while keeping this line's call to it, which would not have compiled, and reported no conflict |
| `42fa78a` | Rescue of `arena/089497bd`'s unique work, described below |

15 branches were deleted across two rounds, from 22 down to 8, plus this one. Every deleted tip SHA is recorded in `docs/BRANCH_CONVERGENCE_PLAN.md` with the command that restores it. `add/mylove-rig-prepared-glb` was deleted to purge `mylove_rig_prepared.glb` (13,221,520 B) and `app/src/main/assets/mylove.glb` (9,287,272 B) from the branch set, which rule 5 of [`AGENTS.md`](AGENTS.md) forbids. All 9 surviving branch trees were then scanned: no private model binary remains anywhere. The only `.glb` files in this tree are `app/src/androidTest/assets/{humanoid_motion_pack,rigged_test_figure}.glb`, `app/src/main/assets/models/female.glb` and the animation fixtures, all with notices.

### Rescued from `arena/089497bd-3d-mega-hub` (`58aced02`)

`AvatarMp4Exporter.kt` — the only video-export path in the repository, landed **byte-identical** to the donor blob `9624de387836b8470a003064f970328756b17ff8` — plus the six `FilamentPreviewView` hooks it calls; `tools/glb_quality_targets_probe.py` with `tools/glb_animation_probe.py`, without which it exits immediately; `docs/REFERENCE_3d4me_options.md` and `docs/QUALITY_REFERENCE_ink_glb.md`; and `generation_contract_test.cpp`.

That test is a contract for features this line did not have, so two were ported with it: the **3D4ME clothing-fabric presets** (`af_spec::cloth_metalness`, the `kFabrics` table, the garment metallic override, and the exporter override that makes a chosen fabric survive export → share → re-import) and the **curated iris swatches**. The iris port fixed a measured defect: before it, `"blue eyes"` left `eye_tone` at zero and set `cloth_tone=(0.045,0.110,0.360)` — it dyed the garment blue — and `"green-eyed woman"` dropped the colour entirely. The JNI float array was not touched, so `kSpecFloatCount`, `AvatarSpec.kt`'s 45 fields and the "spec packing drifted" static assertion all still hold.

Two things were **not** rescued, for measured reasons rather than convenience:

- `imported_animation_test.cpp` (949 lines, 94 checks) calls nine `af_engine_imported_*` functions that do not exist here, and `afcore.h` documents the opposite behaviour: a skeleton that is not this engine's rig is shown static with a joint count of 0, and its animation channels are not applied. It also needs five binary fixtures (~596 KB, including Khronos `CesiumMan`/`Fox`) that the donor's own notices record as CC BY 4.0 **and as packaged into the shipped APK**. It therefore guards a capability this line reports as MISSING. It stays on `arena/089497bd-3d-mega-hub`, **which must not be deleted until that is decided**. `animation_roundtrip_test` is not a substitute: it round-trips files this engine exported.
- The donor's deepened two-layer garment normal-map relief. This branch's `af_texture.cpp` diverged the other way (1,057 lines against the donor's 793), so that is a texture-generator reconciliation of its own.

Consequence, stated rather than hidden: `generation_contract_regression` is registered in CMake and marked `DISABLED`, with both failing check names recorded beside it. Registering it red would fail the *Native C++ sampling regression* CI job for a feature documented MISSING here. Run it by hand with `./af_generation_contract_test`; delete one line to re-enable it.

### Exact-head evidence for `bdaef4b5f3446e6ae3e17ee6c1dddaa653cee6d6`

Results belong only to that commit. This README update creates a new head, so none of them may be reused as evidence for it.

| Gate | Result |
|---|---|
| Android CI, run [`45777314442`](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/45777314442) | **11 of 11 jobs passed**: native C++ sampling regression, unit tests, debug APK build, Android lint, release R8/minification, model tools self-test, **AvatarForge GLES GLB render smoke (instrumented)** and its four evidence-only screenshot jobs |
| AvatarForge generated-avatar runtime verification, run [`45777317839`](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/45777317839) | **FAILED** at step 8, *Run generated-avatar instrumentation test* |
| Host `ctest` on the merged tree, and again after `42fa78a` | 11 of 11 passed, `generation_contract_regression` reported *Not Run (Disabled)*, exit status 0 |
| `af_generation_contract_test` run by hand, alone | 98 checks, **96 passed**; the 2 failures are the unported normal-map relief |
| Generated output unchanged by the ports | `male=73,399,512 B`, `female=72,076,316 B` — byte-identical to the run before the iris port; `satin=72,076,336 B` reproducible, and satin ≠ denim |

The instrumented GLES render smoke passing is worth stating plainly: that job fails on PR #9, PR #14 and PR #15. It passed here on the merged line.

**The runtime-verification failure is attributed, not guessed.** The same workflow was dispatched against two heads that do not contain this line's work:

| Head | LOD change | Trunk merge | Result |
|---|---|---|---|
| `758bcfb4` (`arena/1a5b3043`, run [`45779172779`](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/45779172779)) | no | no | **passed** |
| `9baf7c67` (trunk, run [`45779175991`](https://github.com/davealone69-gif/3d-mega-hub/actions/runs/45779175991)) | no | is the trunk | **failed** |
| `bdaef4b5` (this line) | yes | yes | **failed** |

So the trunk fails this gate on its own, and the merged line inherits it. The trunk's 13 commits change nothing in the renderer, the LOD path, the animation path or the SurfaceView — only icon resources, documents, `BoundedGlbReader`, one line of `native_sampling_test.cpp` and `FilamentPreviewView`; it fails because it lacks this line's 59 renderer commits. What that leaves unresolved is whether this line's own LOD change also affects the gate, and **that is not settled**: GitHub refuses `workflow_dispatch` against a bare SHA (`422 No ref found`), so the pre-merge commit `e6f28ba` could not be run without creating another branch, which this convergence exists to avoid. Treat the LOD change's effect on that gate as **UNVERIFIED**.

For context, the failure this gate has historically reported is documented above in this file: `GeneratedAvatarRuntimeTest#generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView`, "no drawable EGL-presented model frame within 15 seconds", with `frames=2`, `draws=29`, `triangles=1626176`, `glError=0x0`, `eglError=0x0`. Those 1,626,176 triangles are LOD0, the full mesh, which is what the LOD fix exists to reduce — but no log for this head was readable from this workspace, so no claim is made that the fix changes that outcome. The same file also records this test failing on one commit and passing on the same commit in another workflow, because a PixelCopy capture timed out; the workflow logs and artifacts for these runs are hosted outside this workspace's permitted hosts, so the specific failing test on `bdaef4b5` could **not** be read.

### Status of what is new, in the project's own words

- Screen-density LOD selection: **REAL + VERIFIED on host** for the maths, the measured per-level triangle areas and the 11 native tests; **IMPLEMENTED IN SOURCE** for the `af_gl.cpp` GLES wiring; **UNVERIFIED on device**. No existing test asserts the chosen level, which is why CI cannot settle it.
- Trunk merge, `BoundedGlbReader` wiring, fabric presets, iris swatches: **REAL + VERIFIED** — host `ctest` for the C++, and CI's debug APK build, unit tests, lint and R8 for the Kotlin, on the exact head above.
- `AvatarMp4Exporter` and the export surface: **IMPLEMENTED IN SOURCE**. No JDK exists in this workspace, so the Kotlin was never compiled locally; CI's debug APK build is the first compile it survived. Whether it encodes a playable MP4 is **UNVERIFIED**, and it has **no UI entry point on this line**, so as a feature it is **PARTIAL** and is not presented to anyone as working.
- `tools/glb_quality_targets_probe.py`: **REAL + VERIFIED on host** against `app/src/androidTest/assets/rigged_test_figure.glb` — glTF 2.0, 240 vertices, 120 triangles, one skin with 7 joints, three named clips (Wave 2.000 s, March 1.000 s, Bow 2.000 s), bounds 0.62 × 1.72 × 0.31, and a 13,316-byte JSON report.
- Importing and playing a third-party file's own skeleton and clips: **MISSING**, and now with a rescued test that specifies exactly what building it must satisfy.
- Deepened garment normal-map relief: **MISSING** here; it exists on `arena/089497bd-3d-mega-hub`.

### For the next session

Work on the trunk once the convergence pull request has landed, and branch from it. Do not branch from `main`, from `arena/1a5b3043-3d-mega-hub`, or from any branch this plan records as deleted. Before writing a feature, grep for it: the launcher icon was built four times, the GLES diagnostics and FileProvider path four times, bounded GLB reading twice, and the animation system twice.

Device work that still needs a real toolchain is written up as a paste-ready brief in [`docs/ANDROID_TEST_AGENT_PROMPT.md`](docs/ANDROID_TEST_AGENT_PROMPT.md). It names the exact commit, the toolchain values read from this repository, the commands in order, the verbatim `Avatar LOD selected:` log format to capture, and what counts as a pass — including that the magenta debug-shader path deliberately disables LOD, so that suite is not LOD evidence.

## Neural Core planning handover — 2026-11-26

[`docs/NEURAL_CORE_MASTER_PLAN.md`](docs/NEURAL_CORE_MASTER_PLAN.md) is the master architecture, repository
intelligence and mission command for **AvatarForge Neural Core**. It is a **planning document: it contains no
implementation**, and nothing in it authorises a merge. Its statements about the existing repository were
measured at `ba38d996` and `281806e7`; results belong to the head that produced them.

**What Neural Core is, and is not.** An integrated subsystem **inside** AvatarForge — never a replacement
engine, never a competing app, never a second avatar editor, never a chatbot. It lives in
`avatarforge-engine/` as `src/neural/` + `include/afneural.h` in C++ and
`java/com/davemega/hub/avatarforge/neural/{procedural,generated}` in Kotlin, behind a CMake
`AF_ENABLE_NEURAL` option that **defaults OFF**, so with the option off the built libraries are byte-identical
to today's. The existing Ollama chat page in `:app` is demoted to a console over the engine gateway; the
dependency arrow keeps its direction — `:app` consumes the engine, never the reverse.

**Project boundaries** (§1.0). 3D-Mega-Hub is the final integrated Android app; AvatarForge is the separate
standalone engine/SDK, present here as the ported `:avatarforge-engine` module (donor pinned at
`work/android-engine-dev` @ `9e36197a`); 3DDD is a separate donor whose Filament renderer was never imported;
Cue-Next is entirely separate and has **zero** references in this repository.

**The ten missions, in execution order.** Each brief carries **ten fields** — objective, repository and
branch, files and modules, interfaces and dependencies, **detailed implementation instructions**, acceptance
criteria, tests and build evidence, **runtime verification requirements**, README handover, exclusions and
blockers.

| Seq | Mission | Wave | Starts when |
|---|---|---|---|
| 1 | **M0** Foundation and module boundary (freeze F-0) | 0 | PR #17 landed |
| 2 | **M8** Device verification and CI gate repair | 0 | immediately — needs a real Android toolchain |
| 3 | **M1** Neural Gateway and the zero-provider guarantee (F-5) | 1 | F-0 |
| 4 | **M2** Asset inspection and provenance (F-1) | 1 | F-0 |
| 5 | **M3** Evaluation harness, golden records, benchmark suite (F-2) | 2 | F-1 + one device record |
| 6 | **M4** Human Intelligence critics | 3 | F-2 |
| 7 | **M5** Animation Intelligence — **staged S0→S-A→S-B→S-C→S-D→S-E** (F-3) | 3 | F-1, F-2 |
| 8 | **M6** Learning memory, revision gate, recovery (F-4) | 3 | F-2 |
| 9 | **M7** Optimisation and asset intelligence | 4 | F-1, F-2, M8 device data |
| 10 | **M9** Versioned models, optional fine-tuning, rollback — **optional** | 4 | F-4, F-5 |

M5 is staged because "animation" covers five distinct capabilities — skeleton adoption, retargeting,
playback, **morph-target application on import** and facial animation on imported faces — which were
previously conflated and partly deferred. The morph-target gap is now assigned to M5 stage **S-D**:
`af_glb_read.cpp` contains **zero** occurrences of `morph` while the exporter writes them (`afcore.h:390`),
and facial animation on an imported face is impossible until that is closed.

**Four things that are not settled, and are the owner's to settle.**

- **D1** — merge PR #17, with a **merge commit, never a squash**. **D2** — reconcile PR #16 with PR #9 as the
  path into `main`. Until both are resolved there is no stable base to branch from, and **no implementation
  branch may be created** (§7 item 13).
- **D11** — the AvatarForge port has **no declared licence**: `docs/AVATARFORGE_INTEGRATION.md` records that no
  repository-level `LICENSE` existed at the pinned donor branch. **D9** — the engine states nothing about the
  licence of its **own output**. Until both are declared, the set of assets eligible for training is **empty**,
  generated assets are `preview = allow, redistribute = unknown, train = unknown`, and **M9 is `BLOCKED`**.
  The earlier claim that the engine's output is "free data" is **withdrawn** — it assumed an ownership nobody
  has established (§3.F.6).
- **D10** — reference body model. §6.4 compares MHR against Anny in full and **adopts neither**. MHR's 45
  `identity_coeffs` are latent Gaussian dimensions and are **not** the same as this engine's 45 wire floats;
  Anny's `[0,1]` labels are not a unit and their semantics are undocumented. **No parameter mapping is
  assumed for either**: any bridge needs a committed calibration record (§3.O.2), and M4's critics score
  against internal references until one exists.
- **D12/D13** — the donor's CC BY 4.0 fixtures as M5 test assets, and Anny's `anny-full` topology against the
  adults-only content policy.

**One owner-reported defect is now designed for.** On 2026-11-26 the owner reported *"webing on movement"*.
It is diagnosed from source as a **weight-quality and skinning-model** fault, not a rendering one: a
4-influence ceiling in the 88-byte `af_vertex` (`afcore.h:318-327`), weight sharing limited to bones within
three joints (`af_rig.cpp:369`), influences under **1 %** pruned (`:436-439`), two single-joint weight-1.0
fallbacks (`:408-409`, `:442`), pure linear blend skinning (`Rig::skin`, `:449`) and **no body pose
correctives anywhere** in the engine. The owner resolved **D15** the same day — *"Definitely 1 and 2"* — so
**weight quality is authorised first, dual-quaternion skinning second only if measurement still shows the
fault, pose correctives are not authorised, and `af_vertex` does not change**. Measurement is a new **fifth
M4 critic** (five host-runnable metrics, axilla and inguinal fold first, baseline recorded with **no tolerance
asserted**); the remedy is **M5 stage S-F**. Because both authorised tiers edit **protected** components
(`af_rig.cpp`, `src/render/*`), a new **§3.M.1a protocol** governs it: human-authored PR, ABI unchanged, no
test weakened, before/after measured, golden SHA-256 delta reported, one protected component per PR, register
entry added. Status `PARTIAL` — owner-reported and source-diagnosed, **not yet measured**.

**The quality bar now has a face.** Later on 2026-11-26 the owner attached the `/goal/` turnaround sheet
directly to the conversation — hash-identical to the Dropbox file, so provably the same bytes — with the words
*"This quality of mylove.glb"*. The plan records it in **§8.6.5**: three bust views at ≈170 px each of a
stylised-realistic adult character, i.e. a head-detail bar (skin shading, hair strands, fabric lace), not a
polygon bar; usable as a human-judged look, proportion and silhouette reference, still **not** a reconstruction
input (**D14**) and **not** a threshold-fitting source. Permissions tighten rather than loosen: as an
owner-asserted depiction of `mylove.glb`, the sheet becomes `redistribute = deny, train = deny`, and
`mylove.glb` itself stays never-committed and unfittable. Whether automated calibration to it is ever permitted
is the new open decision **D16**. Nothing was committed: only hashes and written observations enter the
repository.

**The "latest upgrade" question, answered by catalogue.** The owner's pointer *"Lastest 3d-mega-hub
upgrade"* named no link, so Dropbox, Google Drive and GitHub were swept (§8.6.6 of the plan). The honest
answer: **no new 3d-mega-hub upgrade has landed in the repository** — this branch is documents-only and
`main` has not moved. The chain is owner-side: a **v0.10.1 Android update bundle** (2026-10-21: APKs, AAR,
build evidence — native 464/464, Kotlin JVM 20/20, parity 72/72, no-INTERNET APK audit — and a warning that
its debug signing certificate changed, so in-place updates may be refused); a *"fixed"* source snapshot at
`3b98600` that is provably **27 commits behind the pin already in use**, i.e. redundant; the **latest
build** = AvatarForge release `debug-411-1`'s debug-signed, ungated `latest-debug.apk` (2026-11-20,
record-only); and the genuinely new material — an **owner-side 3d-mega-hub snapshot** (2026-11-16) whose
README adds three things GitHub lacks: a preserved **pre-integration baseline APK** (11,026,696 B,
historical 5/5 + 43/43), an interim **non-monolithic architecture rule**, and an **adult-mode handoff** in
which the owner authorises **Grok-Girls** for reuse while AvatarForge's anatomy controls remain
`BLOCKED_ENGINE_GEOMETRY` — a contract, not a mesh. Two register entries follow: **D17** (which artifact,
if any, becomes the integration baseline — recommendation: keep the pin) and **D18** (whether adult-mode
integration enters scope — recommendation: record and defer). No cloud byte entered the repository.

**"Fix all and continue with ai", executed inside the standing constraints.** *Fix*: the plan's §8.2
research table is fully dispositioned — AIST++ split (API Apache-2.0 verified; dataset repository carries no
licence, so the CC BY 4.0 data claim is uncitable), Stability SD 3.5 resolved (the repo carries only MIT
`LICENSE-CODE`; weights terms live off-repo), Hunyuan Exhibit A read in full (territory exclusion verbatim
twice, plus a NOTICE component layer), Meshcapade closed moot, Chroma/HiDream and the Anny counts closed as
unresolvable with reasons, MHR's residual narrowed to one bundle-level check, and `trimesh` verified MIT at
`mikedh/trimesh` — the earlier 404 was a wrong slug. One row stays open on purpose: the physical phone,
because it needs a phone. *Continue with ai*: a new **§3.P** specifies the six interface freezes — F-0
`afneural.h` with a tested OFF-build byte-identity rule, F-1 `af_asset_inspect`, F-2 record/metric/tolerance
schemas, F-3 both animation APIs side by side so D4 becomes a choice, F-4 memory/recovery schemas, F-5
provider interfaces with the zero-provider guarantee as three tests. Every identifier PROPOSED until its
mission freezes it; nothing implementable before D1/D2 clear. No implementation, no merge, no new branch.

**CI gate audit and the one honest fix.** Audited all five workflows against `AGENTS.md` (§8.8 of the
plan): publishing is compliant (dispatch-only, readiness record, SHA pin), the UI label
`Run generated-avatar GLES diagnostic` is intact, all nine CI scripts are present. The single red gate is
the generated-avatar runtime verification: its evidence signature (`presented=2`, `draws=29`, LOD0
triangles, zero GL/EGL errors) says the renderer works while the app-side pixel verification starves — two
cores of SwiftShader rasterising 1.6 M triangles cannot reach a verdict inside the old 300 s deadline.
Fixed without weakening anything: four cores and 6 GiB for the emulator, verdict deadline 300 s → 600 s in
a 1 500 s test budget; the 5 000-pixel threshold, the presented-frame minimum and every GL/EGL assertion
are untouched. If it is still red, the next honest evidence is a hardware session, not another guess.

**Unchanged protections.** `main` is untouched. PR #17 is **open and unmerged**.
`arena/089497bd-3d-mega-hub` (`58aced02`) and `arena/25bd4008-3d-mega-hub` (`b79a7d0c`) are **preserved**, and
§8.4 of the plan lists the donor branch's nine items of unique work with a `git show` recovery command for
each. No destructive Git operation has been performed. The SM-S921B figures in §3.L.1 belong to the APK of
`797d156`, measured 2026-11-15, and are a **historical baseline to beat — not current runtime verification**.
