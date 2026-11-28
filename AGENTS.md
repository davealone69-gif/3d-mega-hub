# AGENTS.md: the machine, for anyone (human or agent) who will work on it

Read this first. It says what the repository is, how the parts fit, how to build and test, what has been measured and where, and which mistakes have already been made.

- Checked against the source at commit `2920265` (engine and creator), `96152cd` (imported models, glTF viewer, CI screenshots) and `ff9e6ba` (clips of imported files, model tools) on 2026-11-15.
- The plan for reaching photoreal avatars is a separate file: [docs/PHOTOREAL_UPGRADE_PLAN.md](docs/PHOTOREAL_UPGRADE_PLAN.md).
- The item-by-item inventory with evidence links is [docs/AVATAR_ASSETS_ANIMATION_MASTER.html](docs/AVATAR_ASSETS_ANIMATION_MASTER.html). The current results are in [README.md](README.md).

## 1. What this is

| | |
|---|---|
| Product | **3D Mega Hub**, an Android app (`com.davemega.hub`, Kotlin, Jetpack Compose). Five pages: Home, Avatar, AI (chat with an Ollama endpoint), Voice (Android text to speech), Settings. The Avatar page, "Avatar Studio", is the centre of the product |
| Engine | **AvatarForge**, a C++17 library in the module `:avatarforge-engine`. It computes a human avatar from a specification, with no asset files: body, face, hair, clothes, textures, skeleton, skin weights, 52 face blendshapes and 15 animations. It writes and reads GLB, and it has its own OpenGL ES renderer |
| Only target device | Samsung Galaxy S24 **SM-S921B**: Exynos 2400, Xclipse 940 GPU, 8 GB RAM, 1080 x 2340, 120 Hz. On this phone OpenGL ES is ANGLE on top of Vulkan (the app's own report reads `ANGLE ((Samsung Xclipse 940) on Vulkan 1.3.304)`) |
| Second renderer | Google Filament 1.71.6 (`filament-android`, `gltfio-android`), the "glTF viewer". Used for the bundled sample and for an imported file the engine's renderer cannot show as authored: one with animation clips, more than one colour texture or a transparent material |
| Build | GitHub Actions only. Kotlin, the APK and the GLES renderer cannot be built or run on a plain Linux host; the engine core and its tests can |

## 2. Rules that are not negotiable

1. **Status words.** Every claim carries one of: REAL + VERIFIED, IMPLEMENTED IN SOURCE, PARTIAL, UNVERIFIED, MISSING, BLOCKED. Say where it was verified: host, CI emulator, or the phone. "Verified on the emulator" never means verified on the phone.
2. **No invented numbers.** A measurement that was not taken is written as not measured. In the telemetry a zero means "not measured yet", never "fast".
3. **No mock features**: no placeholder generation, no inert button, no fake progress.
4. **Branches.** `main` is not touched. Work goes on a branch and lands in `integration/avatarforge-renderer-port` through a pull request with a merge commit (commit ids are cited in the documents, so do not squash). PR #9 (integration to `main`) is the owner's decision. No force-push.
5. **Private material stays out.** The owner's Dropbox guides, reference models (`mylove.glb`) and screenshots of other products are never committed and never packed into the APK. Pictures in the app are this engine's own renders.
6. **Adults only.** Every avatar the creator builds is 30 years old (unit-tested). The engine honours `bust` and `glute` only for age 18 and over (host test).
7. **Licences.** Anything that enters the repository from outside is recorded in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) with its source, revision and licence, before it is used.
8. **A test finds a button by its text.** Keep the label `Run generated-avatar GLES diagnostic`.
9. **Do not publish what was not gated.** The phone-test APK is published only by `publish-phone-test-apk.yml`, which re-checks the gate itself.

## 3. Map

Line counts are of `2920265`.

### Engine core, `avatarforge-engine/src/main/cpp/` (library `libafcore.so`)

| File | Lines | Role |
|---|---|---|
| `include/afcore.h` | 481 | The C ABI. `af_spec` (what to build), enums for sex, build, hair style, outfit, footwear, face parameters, the stage list, the engine functions. Enum values are ABI: only append |
| `src/af_prompt.cpp` | 583 | Keyword parser: text to `af_spec`. Not a language model |
| `src/af_anatomy.cpp/.h` | 592 | Anthropometric solver: `af_spec` to landmark heights and girths (`BodyMetrics`). A tape measurement given in the spec is met exactly |
| `src/af_field.cpp/.h` | 3,128 | The signed distance field: body, head, face, hands, feet, hair (`hairShape`), clothes (`clothUpper`, `clothLower`, `upperShell`, `skirtShell`), shoes (`shoeShape`), armour and gear (`gearField`). Everything visible is a branch of this function |
| `src/af_isosurface.cpp/.h`, `src/af_body.cpp/.h` | 2,084 | One lattice over the whole body (4 mm cell at full quality), marching tetrahedra, one red-green refinement round in detail regions, Taubin smoothing, re-projection onto the field, normals from the field gradient |
| `src/af_uv.cpp/.h` | 701 | Charts, projection, shelf packing into one 2048 x 2048 atlas, seam splitting |
| `src/af_texture.cpp/.h` | 1,144 | Procedural paint in UV space, as a function of the 3D point: base colour, normal, metallic-roughness, occlusion, subsurface |
| `src/af_rig.cpp/.h` | 1,096 | Skeleton (71 joints in a generated avatar), four-influence weights from distance to bone with region masks, 52 ARKit-named blendshapes as analytic displacements, the clip library |
| `src/af_glb.cpp/.h` | 1,072 | GLB export: one file, sparse morph targets, sampled animations, PNG textures, an `extras` block with the spec |
| `src/af_glb_read.cpp/.h` | 1,786 | GLB import. Refuses every glTF extension except `EXT_texture_webp`, so no Draco, meshopt or KTX2. Never crashes on a malformed file (fuzzed) |
| `src/af_engine.cpp` | 2,262 | The engine object, the stage sequence, memory budget (1,536 MB by default), statistics |
| `src/af_image.cpp`, `af_png*.cpp`, `af_json.cpp`, `af_math.h`, `af_mesh.h`, `af_threads.h`, `af_profiler.cpp` | | Support code. No third-party library except zlib. Threads are fork-join `std::thread` per stage, at most 8 |

### Renderer, `src/render/` (library `libafrender.so`)

| File | Lines | Role |
|---|---|---|
| `af_render.h` | 259 | The `Renderer` interface, `RendererOptions`, `RendererStats`, `CameraState`. The lighting model is described in its header comment |
| `af_gl.cpp` | 2,172 | **The renderer the app uses**: EGL + OpenGL ES 3.1. Shadow pass, forward pass with MSAA into an HDR target, bloom, tone map. Skins on the GPU (uniform block `AfSkinBlock`, instance `afBones`) |
| `af_shaders_gl.h` | 8,987 | All GLSL for the GLES path, as strings |
| `af_render.cpp` | 218 | `BudgetRenderer`, a wrapper round the backend that steps the quality tier. **Every new virtual of `Renderer` must be forwarded here**, or the app silently gets the base-class default |
| `af_avatar_lod.cpp/.h` | 377 | Three index-only levels of detail |
| `af_environment.h` | 181 | Analytic sky (sun, sky gradient, ground bounce) and its projection to 9 spherical-harmonic coefficients |
| `af_vk.cpp`, `af_shaders_vk.h` | 5,500 | A Vulkan backend. **Not compiled** (`AF_ENABLE_VULKAN` is off), not linked, and behind the GLES path: it has no GPU skinning. Treat as stale |

### JNI and Kotlin

| File | Role |
|---|---|
| `avatarforge_jni.cpp` (`libmega_hub_avatarforge.so`) | Generation entry point. The spec crosses as 45 floats (`kSpecFloatCount`) |
| `avatarforge_renderer_jni.cpp` | Render session, camera, animation calls, diagnostics |
| `avatarforge-engine/.../AvatarSpec.kt` | The Kotlin mirror of `af_spec` (`FLOAT_FIELDS` = 45, same order as the C packer), `FaceParam`, the enums in the C order |
| `.../AvatarForgeEngine.kt` | `generate(...)`, cancellable, on `Dispatchers.IO` |
| `.../AvatarForgeSurfaceView.kt` | The view and its render thread; camera (`resetView`, `zoomBy`, `turnBy`), animation API, PixelCopy check |
| `.../RendererTelemetry.kt`, `DiagnosticDownloads.kt`, `EvidenceIdentity.kt`, `GlbValidator.kt`, `DeviceResourceGovernor.kt` | Measured numbers, the two JSON reports in `Downloads/3D Mega Hub Diagnostics`, identity of APK, device and asset, GLB structure check, RAM, storage and thermal gate before generation |
| `app/.../MainActivity.kt` | The hub and its five pages |
| `app/.../AvatarStudioScreen.kt` | Avatar Studio: stage, rail of sections, Randomize, Save, Create avatar, animation strip, dark and light look |
| `app/.../AvatarCreatorPanels.kt`, `AppearanceControls.kt` | The sections (Look, Body, Hair, Face, Skin, Outfit, Shoes, Motion), picture cards, colour tiles; `AppearanceSelection` maps each choice to spec values |
| `app/.../GlbPreviewRouting.kt`, `FilamentPreviewView.kt` | Which preview a file opens in, and the Filament preview (orbit, zoom, one shadow-casting directional light plus ambient; 4x MSAA, FXAA, ambient occlusion, PBR Neutral tone curve). A generated avatar is never sent to Filament, and neither is a file whose textures exceed the budget. The preview plays the file's own clips (`selectAnimation`, pause, loop, speed, seek) and reports its state through `playbackState()` |
| `app/.../GlbClipNames.kt` | The labels of an imported file's clips: the names in the file, tidied; "Clip N" for a clip without one; repeated names numbered. The screen shows them as a strip of chips under the stage |
| `app/.../GlbTextureBudget.kt`, `AndroidGlbImageScaler.kt`, `ImportedGlbPreparation.kt` | Import step: reads every embedded image's pixel size from its header and, when the decoded textures exceed 448 MiB, writes a copy of the GLB with those images halved. Everything else is copied byte for byte |
| `app/src/main/res/drawable-nodpi/preset_*.jpg` | 51 card pictures, each a host render of the engine's own output |

### Tests and tools

| Where | What |
|---|---|
| `avatarforge-engine/src/test/cpp/` | Nine host tests run by `ctest`: sampling regression (shape, bust, glute, adult rule, face, hair styles, swimwear), GLB extensions, camera frustum, LOD, scene visibility, imported PBR, import robustness (fuzz), animation round trip, prompt words. Two tools: `af_host_probe` (generate and write a GLB with a report), `af_host_render` (generate or load, then software-render to PNG) |
| `avatarforge-engine/src/test/java`, `app/src/test/java` | JVM unit tests: spec packing, GLB validator, telemetry, routing, the mapping from creator choices to spec values |
| `app/src/androidTest/` | `AvatarForgeRendererSmokeTest`, `GeneratedAvatarRuntimeTest`: generate on the emulator, draw through the production view, read the pixels back, start an animation, read them again. `GlbClipPreviewTest`: a rigged file with three clips through the glTF viewer; clip names, a tap on a clip's chip, and the picture at two times of a clip. Its model is `app/src/androidTest/assets/rigged_test_figure.glb`, written by `make_test_figure.py` |
| `tools/glb_figures/` | Python tools for model files; no model is stored in the repository. `figures_to_clips.py` (needs numpy): a "showcase" GLB with N copies of one rigged figure, each moving differently inside one animation track, becomes one figure with one named clip per motion; `--report` describes a file without writing, `--verify` skins source and result on the CPU and prints the largest vertex distance. `make_test_figure.py` (no dependencies): a rigged figure of boxes, `single` or `showcase`. `test_figures_to_clips.py`: 24 checks, must print "ALL PASSED" |
| `tools/avatar_master_doc/build_doc.py` | Builds the master HTML from the source and `doc_extra.json`. Must print 0 unresolved links and 0 unmatched patterns |
| `.github/workflows/` | Five workflows, see section 7 |

## 4. The pipeline, stage by stage

`prompt or creator choices -> af_spec -> anatomy -> field -> mesh -> UV -> textures -> rig -> blendshapes -> clips -> GLB -> import -> draw`

| Stage (`af_stage`) | What happens | Numbers |
|---|---|---|
| 1 Analyse inputs | Keyword parse of the prompt; optional coarse image analysis (framing, median colours). No face reconstruction | 30 of 30 prompt-word cases pass |
| 2 Fit anatomy | Regression from sex, build and height to landmarks and girths; supplied tape values override | |
| 3 Geometry | One field for body, hair, clothes and gear; one extraction | 4 mm lattice; **1.04 to 2.13 million triangles**, 0.6 to 1.2 million vertices, depending on outfit |
| 4 UV unwrap | One atlas | 2048 x 2048 |
| 5 Bake textures | Five maps, procedural | 2048 x 2048 each |
| 6 Rig | Skeleton and weights from the same metrics that made the mesh | 71 joints, 4 influences |
| 7 Blendshapes | 52 ARKit channels | Written to the GLB; **not read back on import and not driven by the app** |
| 8 Animation | 15 clips: idle, breathe, walk, jog, run, squat, jump, wave, punch, kick, pushup, t_pose, a_pose, sit, turn_360 | Procedural keys, sampled at export |
| 9 Export | One GLB | 67 to 122 MB |

The app then **imports the file it just exported** and draws that. The importer adopts the skeleton and clips only when the file's skin is this engine's own rig; any other file is drawn static with a joint count of zero. It also keeps **one colour texture for a whole imported model** and draws every material opaque, which is why a foreign model with several textures or transparency is sent to the glTF viewer instead.

Quality: `AF_QUALITY_FULL` is what the app generates (25 to 88 s for one avatar on a 2-core host; not measured on the phone). `AF_QUALITY_DRAFT` uses a 16 mm lattice: a tenth of the triangles in a few seconds, same skeleton and clips. Use draft to iterate, full to judge.

## 5. Conventions

- Metres. +Y up. The avatar faces **-Z**. +X is the avatar's left. Soles on y = 0.
- `af_spec` fields are appended, never reordered; zero means "engine default". Fields added in `2920265`: `glute`, `eye_tone[3]`, `face[8]` (nose width, nose bridge, lip fullness, eye tilt, eye openness, cheekbones, jaw width, one reserved).
- The spec has no member `hair`; it is `hair_style`.
- Colours in the spec are linear RGB.
- Hair styles 0 to 13: none, buzz, short, medium, long, ponytail, bun, braids, afro, mohawk, bob, bangs, curly, box braids. Outfits 1 to 10: T-shirt and jeans, dress, suit, jeans and jacket, plate armour, leather armour, spacesuit, athletic, coat, swimwear. Footwear 0 to 4: none, sneaker, boot, sabaton, heel. Builds include slim, athletic, average, heavy, muscular, curvy.

## 6. The renderer the app uses

- One directional sun with GGX specular and a 5-tap PCF shadow map; ambient from 9 spherical-harmonic coefficients of an analytic sky; specular reflection of the same sky by 16 importance samples per pixel (no environment map, no lookup texture).
- Skin: wrapped diffuse, thickness-driven translucency, single scatter. Eyes: cornea refraction, limbal darkening. Hair: Kajiya-Kay highlights. Cloth: sheen. Metal path for armour.
- Post: bloom, ACES tone map, vignette, dither.
- Quality tiers (MSAA samples, shadow map, bloom levels): LOW 1, 1024, 2; MEDIUM 2, 1024, 3; HIGH 4, 2048, 4. `BudgetRenderer` steps down after more than 30 consecutive frames slower than 30 FPS.
- Vertex format: `af_vertex`, **88 bytes**, all floats except the joint indices. Textures are uploaded uncompressed (RGBA8) with mipmaps.
- The render thread paces itself with `Thread.sleep` to 16.67 ms. It is not tied to the display's vsync and it draws continuously.
- Animation: the renderer evaluates one skin matrix per joint each frame and blends the bind-pose vertices on the GPU. Play, pause, stop, loop, speed and seek exist in the native API; the app has no seek control.
- Diagnostics: `renderer_diagnostics.json` and `renderer_telemetry.json`, each naming the APK by SHA-256, the device and the asset.

## 7. Build, test, publish

Host (engine core and tests only):

```bash
cmake -S avatarforge-engine/src/main/cpp -B build/host -DBUILD_TESTING=ON
cmake --build build/host --parallel 2
ctest --test-dir build/host --output-on-failure      # 9 tests, about 45 s

# look at an avatar without a phone
build/host/af_host_render --generate --sex 2 --build 5 --hair 12 --outfit 10 \
    --out /tmp/a.png --height 900 --aspect 0.6 --tile 180,0.95,1.9 2>/dev/null
build/host/af_host_probe --sex female --build 2 --hair 3 --outfit 1 --height 1.75 --out /tmp/a.glb
```

`af_host_render` options: `--generate` or `--glb F`; `--sex --build --hair --outfit --footwear --draft --bust --glute --muscle --fat --melanin --stature --age --face a,b,c,d,e,f,g,h --hairtone r,g,b --eyetone r,g,b --clothtone r,g,b`; `--tile yaw,centreY,extent[,cx[,cz]]` (repeatable); `--clip N --time s`; `--report`; `--joints`. A GLB import prints many lines on stderr.

Android: `./gradlew :app:testDebugUnitTest :app:lintDebug :app:assembleDebug` (JDK 21, SDK 35, NDK 26.1.10909125, CMake 3.22.1). See [BUILD_AND_VERIFY.md](BUILD_AND_VERIFY.md).

CI, five workflows:

| Workflow file | What it proves | Starts by itself on a PR into the integration branch |
|---|---|---|
| `android.yml` | Unit tests, lint, native C++ tests, model tools self-test, R8, debug APK, render smoke on an emulator (five instrumented tests: four on the app's own renderer, one on the glTF viewer) | No: dispatch it |
| `avatarforge-diagnostic-apk.yml` | Host tests; builds the debug APK and verifies generated-avatar pixels on an emulator; downloads the artifact again and inspects the APK | No: dispatch it |
| `avatarforge-generated-runtime.yml` | Generate, draw through the production view, pixel check, animation check | Yes |
| `release-preview.yml` | Release build path and emulator smoke | No: dispatch it |
| `publish-phone-test-apk.yml` | Self-test of its rules on a PR. By manual dispatch with `commit=<sha>` it publishes the debug APK of that commit as pre-release `phone-test-<sha7>` | Self-test only |

- Dispatch: `gh workflow run <file> --ref <branch>`. A new run of `android.yml` or `release-preview.yml` on the same branch cancels the one in flight.
- **Publishing gate:** the commit must have passed all five conditions and be the branch head, or the head may differ from it only in root `*.md`, `docs/**/*.md` and the two publishing files. HTML, pictures and `tools/` count as code for this rule. So: publish the code commit first, then push documents.
- The emulator draws in software, one frame every several seconds. It proves that the path works and what the picture contains. It says nothing about speed.
- **Seeing what the emulator drew.** Artifacts are zip files behind a redirect. Two jobs therefore also store a small JPEG copy of their screenshots in the output of a check run on the commit (`.github/scripts/publish_screenshot_check.py`; the jobs have `checks: write` for it):

  ```bash
  gh api "repos/OWNER/REPO/commits/SHA/check-runs?per_page=100" \
      --jq '.check_runs[] | select(.name | startswith("Evidence only")) | "\(.id) \(.name)"'
  gh api repos/OWNER/REPO/check-runs/ID --jq '.output.summary + (.output.text // "")' \
      | tr -d '\n' | base64 -d > screenshot.jpg
  ```

  `release-preview.yml` stores the Avatar Studio with the bundled model in the glTF viewer. `android.yml` stores the generated avatar and the last imported GLB as the app's own renderer drew them, and from `GlbClipPreviewTest` the rigged test figure at two times of a clip as the glTF viewer drew it, and the screen with the clip strip and the Motion section. They are evidence, not tests: nothing judges the picture.
- Each CI debug APK is signed with a key made for that one job. Android will not install it over another build: uninstall first.

## 8. What has been measured, and where

| Where | Build | Result |
|---|---|---|
| **Phone** (SM-S921B, Android 17) | APK of `797d156` | Generated avatar, 1,100,168 triangles: 53.8 FPS over the last 240 frames, after the renderer had stepped from HIGH to MEDIUM because HIGH did not hold 30 FPS. Native heap 304 to 306 MB. Requested GPU bytes: textures 72.7 MB, geometry 127.9 MB, render targets 84.2 MB. Import 1.3 s, first frame 2.2 s. An imported 144,073-triangle file: no problem, heap 134 MB. Record: [docs/PHONE_TEST_2026-11-15.md](docs/PHONE_TEST_2026-11-15.md) |
| Phone | every later build | **Nothing verified.** The owner reported "animation crashes and no built in controls" on `041e31d`; the cause is inferred (a generated avatar sent to the Filament preview), not confirmed |
| CI emulator | `2920265` | Gown avatar, 2,125,600 triangles: 323,702 geometry pixels through the production view; 15 animations listed; `t_pose` started; 81,407 pixels changed; no crash or ANR. Native heap 734 MB |
| Host | `2920265` | `ctest` 9 of 9. 65 full-quality generations, all exit code 0. Default avatar: 623,493 vertices, 1,148,034 triangles, GLB 66.7 MB, 42 s |
| CI emulator | `ff9e6ba` | A rigged test figure with three clips in the glTF viewer: clips listed by name, a tapped chip chose its clip, 350 of 9,216 sampled points changed between two times of one clip and none after returning. The first test of that viewer's picture; the figure is made of boxes |
| Host | `e4655c3` | Three reference models from the owner, imported with the engine's importer: record in [docs/OWNER_MODELS_2026-11-15.md](docs/OWNER_MODELS_2026-11-15.md). The realistic one has 27,292 triangles of body skin and four 4096 x 4096 skin textures |

## 9. How to add things

**A hair style, outfit or shoe.** Append the enum value in `afcore.h`; shape in `af_field.cpp`; paint in `af_texture.cpp`; words in `af_prompt.cpp`; the same value in `AvatarSpec.kt`; label, order and picture in `AvatarCreatorPanels.kt` and `AppearanceControls.kt`; a card picture rendered with `af_host_render` into `drawable-nodpi`; cases in `native_sampling_test.cpp`, `prompt_preset_words_test.cpp`, `AvatarSpecTest.kt`, `AppearanceSelectionTest.kt`. Render it at full quality and look at it before claiming anything.

**A spec field.** Append to `af_spec` and `af_spec_init`; raise `kSpecFloatCount` in `avatarforge_jni.cpp` and `FLOAT_FIELDS` in `AvatarSpec.kt` together, same order; extend `AvatarSpecTest.kt`; add the option to `host_avatar_render.cpp`.

**A renderer call.** Add it to `Renderer`, implement it in `af_gl.cpp`, forward it in `BudgetRenderer`, expose it in `avatarforge_renderer_jni.cpp` and `AvatarForgeSurfaceView.kt`, and assert it in `GeneratedAvatarRuntimeTest`.

## 10. Mistakes already made: do not repeat them

Field and shape:
- Returning `1e9` for "nothing here" from a shape that later goes through a smooth minimum or maximum: the result rounds to exactly zero and meshes as blocks. Use 1.0 (one metre).
- A garment as an offset of the whole body with the arms cut out; garment layers at full thickness up to their edge; a hairline as `height - y`; surface noise added to the distance field; an open mouth by default; a shoe as an inflated foot; a flat heel; an afro as a hanging bell; one cloth tone for every outfit. All of these looked wrong and were replaced.
- Rib depth at the bust taken from the chest girth: the flattest chest came out the deepest. It is always the underbust now.
- Bust projection of 92 mm and face offsets above about 0.5 read as caricature.
- A curl field of 1.6 mm is invisible at app quality; 3 mm is used.
- The integration branch's three-strand braids put their guide line inside this branch's body below the neck. Not taken.

Rendering and app:
- Filament cannot hold a generated avatar: gltfio reserves about 24 bytes per vertex per morph target, hundreds of megabytes here. That ended the process on the phone. The routing guard exists for this reason.
- Routing imported files by "has animation clips" alone (`210ae9a` to `e4655c3`) left a model built from many textured parts on the engine's renderer, which showed it mostly grey. Route by what the renderer cannot show.
- Nothing limited texture memory. A desktop model with twelve 4096 x 4096 textures needs 1,152 MiB decoded. Check the image headers before loading; reduce at import.
- Reading a file through a growing stream buffer holds it twice. A 100 MB model does not fit the Java heap that way: allocate the file's length once, and catch `OutOfMemoryError`.
- Playing clips by moving vertices on the CPU uploads about 55 MB a frame. Skin on the GPU.
- A renderer virtual that `BudgetRenderer` does not forward does nothing, silently. The first build with animation listed no animations for this reason.
- Draft quality misleads about looks. Black clothing hides form in a small picture.

Process:
- **Only the repository survives a workspace restore.** `/tmp` and every directory outside the repository are gone afterwards, including helper scripts kept beside it. Put a helper that will be needed again under `tools/`, or accept writing it again. After a restore: `git fetch origin '+refs/heads/*:refs/remotes/origin/*'`, then `git reset --mixed origin/<branch>`; the clone is shallow until `git fetch --unshallow`.
- **Other sessions push to this repository.** Before writing that a branch is "untouched" or "at" a commit, fetch and look.
- Pushing while CI runs are in flight cancels some of them. A commit cannot contain its own run results: record them in the next documents commit and in the pull request.
- `gh run view --log` and artifact downloads may be unreachable from a sandbox. The job log can be read through `GET /repos/{owner}/{repo}/actions/jobs/{job}/logs`, which redirects to a short-lived URL.
- **Look at the app before describing it.** Since `50bbe25` two jobs store small copies of emulator screenshots as check runs on the commit, named "Evidence only: emulator screenshot, ...": the Avatar Studio with the bundled model in the glTF viewer (`release-preview.yml`), and the generated avatar and the last imported GLB as the app's own renderer draws them (`android.yml`). `.github/scripts/publish_screenshot_check.py` has the two `gh api` commands that read one back. They are evidence, not tests.
- **A check run made through the API shows up as a job of some workflow run of that commit**, and the publishing gate refuses a run with a job that did not end in success. Evidence checks therefore end in "success" and say "Not a test" in their title. `50bbe25` used "neutral" and cannot be published.
- The host builds neither the GLES renderer nor Kotlin. A Kotlin or GLSL change is unproved until a workflow finishes.
- The creator screen has been seen only as emulator screenshots (since `50bbe25`). No test taps a card of the creator; one test taps a clip chip of an imported file (`GlbClipPreviewTest`, since `ff9e6ba`).
- **A PixelCopy in a test must be asked again on `ERROR_TIMEOUT` and `ERROR_SOURCE_NO_DATA`.** The copy takes the buffer queued last and gives the GPU half a second to finish it; the emulator needs ten seconds and more for one frame of a generated avatar. One capture in `GeneratedAvatarRuntimeTest` made a single attempt and failed the diagnostic workflow once by chance (run 43740886806 on `787e030`; fixed in `848632d`).
- **A new instrumented test class must be added to the script that runs it.** `.github/scripts/avatarforge-render-smoke.sh` names its test classes and the number of tests it expects; a class that is not listed there never runs.
- **In a UI test, scroll the creator by a slow drag on the category rail.** A finger on the 3D view turns the model instead of moving the page, and a quick swipe flings the page past its target. The strip under the stage is below the fold of a small screen.

## 11. Known faults

The list with evidence, cause and fix is in [docs/PHOTOREAL_UPGRADE_PLAN.md](docs/PHOTOREAL_UPGRADE_PLAN.md#3-repairs). In one line each:

- The mesh is about ten times too dense for the phone and the vertex is 88 bytes.
- The face never moves: blendshapes are written to the file and then dropped on import.
- The render thread sleeps instead of following vsync, and draws when nothing changes.
- Thermal state is consulted only before generation.
- Textures are uncompressed and there is no environment map.
- The Vulkan backend in the tree is stale.
- Each CI build needs an uninstall, which also orphans the diagnostics files.
- The crash the owner reported is not confirmed fixed.
- The glTF viewer has no reflections of a surrounding (one light and a uniform ambient term), its picture has been seen on the CI emulator with the bundled figure only, and an imported model's skeleton is only posed by the file's own clips.
- Filament's default tone curve (ACES) makes bright skin tones paler and greyer under the viewer's lights; the viewer uses PBR Neutral. A change to the lights or the curve must be looked at in the screenshot.
- **A glTF colour factor is linear.** `baseColorFactor` times 255 is not the colour a display shows: convert it with the sRGB curve before comparing it with a pixel. The message of commit `96152cd` compares a linear factor with screenshot pixels and overstates the effect of the tone curve; the numbers in the README are the corrected ones.
- The Motion section of an imported file (glTF viewer) has a row of four buttons that is wider than a phone screen: "Stop / reset" is cut and "Next" is out of sight until the row is swiped. Seen in the emulator screenshot of `787e030`. The chips under the stage do the same job.
- The engine's own importer keeps one colour texture per imported model and ignores transparency. Until `96152cd` it also drew a texture a fifth too dark when the file left out `baseColorFactor`.
- The generated avatar, as the production renderer draws it on the emulator, has flat plastic-looking skin and banding on the clothes. Host renders had not shown how far it is from the references.
