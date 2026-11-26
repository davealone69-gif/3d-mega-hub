# Photoreal avatars on the Galaxy S24: where the machine stands, what to repair, what to build

Written 2026-11-15 against app code `2920265`. The description of the machine itself, for agents, is [AGENTS.md](../AGENTS.md).

**Revised the same day** after the owner supplied three 3D reference models ([OWNER_MODELS_2026-11-15.md](OWNER_MODELS_2026-11-15.md)). What changed here: the measured comparison under the bottom line, repairs R16 to R18, and the renderer row in section 5.2, which is now an open question instead of a decision.

Status words used here are the project's: REAL + VERIFIED, IMPLEMENTED IN SOURCE, PARTIAL, UNVERIFIED, MISSING, BLOCKED. Where a figure is an estimate or a target and not a measurement, it says so. Nothing in sections 4 to 8 is built: it is a recommendation.

## Bottom line

1. **The engine computes the whole human from formulas.** That gives a consistent, rigged, offline avatar from nothing. It cannot give a photograph, because there is no sculpted or scanned data anywhere in it. Tuning the formulas further will not close that gap.
2. **Keep the machine, change what it is fed.** The specification, the creator, the animation path, GLB export, the tests and the CI gates stay. Which of the app's two renderers draws the authored body is open (section 5.2). The source of the human changes from "distance field plus procedural paint" to **an authored base body with morph targets, scan-grade textures, separate eyes and teeth, and hair cards**, packed as a kit the engine assembles in milliseconds.
3. **Start with the MakeHuman family of assets.** They are CC0, the licence explicitly allows building a character generator from them, their sliders are the ones your creator already shows, and an agent can build the kit in CI without a PC. Design the kit so better licensed content can replace the head, skin and hair later with no engine change.
4. **On the S24 the first performance fix is the mesh.** It is about ten times too dense. That, not the phone, is the likely reason the highest quality tier did not hold 30 FPS in your test.
5. **How real it gets.** With scan-grade content and the lighting and skin work in section 4, a still on the phone can read as a photograph. Close up and in motion it will read as a high-end game character. That is my engineering estimate, not a measurement. Neural (Gaussian) avatars come closer to a photograph for a captured person, but they cannot be edited with sliders and need a server to create.

### Measured on the owner's realistic reference model

The owner's Daz Genesis 8 model is the kind of asset the references were rendered from. Its numbers against an avatar this app generates:

| | The Daz model | A generated avatar (`2920265`) |
|---|---|---|
| Triangles in the body skin | 27,292 | 1,148,034 for the whole avatar |
| Skin textures | four regions at 4096 x 4096 | one 2048 x 2048 atlas shared with hair and clothes |
| Eyes, teeth, lashes | separate parts | painted on the body shell |
| Hair | cards and strands with transparency (273,332 triangles) | a solid shape |

So the realistic model spends about a fortieth of the triangles on the body and has sixteen times as many texels on its skin. That is point 2 above, measured.

It is not a shortcut, though. As downloaded it has no morph targets (no sliders), no animation, a skeleton that is not the engine's, 1,152 MiB of textures once decoded, and it is Daz content that an uploader relabelled CC-BY (section 5.3).

## 1. What a photoreal human needs, and what the machine has

| Ingredient | What it takes | What AvatarForge has at `2920265` |
|---|---|---|
| Form | Sculpted or scanned anatomy, a face mesh whose edge loops follow eyelids, lips and nostrils | A union of analytic primitives. One generic face, moved by seven offsets |
| Surface detail | Pores and wrinkles: normal and colour maps of 4K or more for the head alone, from scans | Procedural paint in one 2048 x 2048 atlas shared by body, hair and clothes. Roughly a millimetre per texel (estimate): about ten times too coarse for skin |
| Skin shading | Subsurface diffusion, two specular lobes, roughness that varies across the face | Wrapped diffuse, thickness-driven translucency, single scatter. The right ideas in simplified form |
| Eyes | A separate eyeball with a corneal bulge, a wet line, a soft shadow under the lids, eyes that move and blink | A refraction shader on the body shell. The eyes do not move and the lids never close in the app |
| Hair | Cards or strands with soft edges, two highlights, self-shadow | A solid shape from the distance field with painted strands |
| Light | A real environment (HDR image), a soft key light, ambient occlusion, contact shadows | One sun, an analytic sky, a 5-tap shadow map, no ambient occlusion |
| Cloth | Modelled garments with folds and fabric maps | The body's surface moved outwards and painted |
| Motion | Captured motion, blinking, breathing, eye darts, soft-tissue follow-through | 15 procedural clips. The face is static: blendshapes are exported and then dropped on import |
| Image | Anti-aliasing that holds on hair and highlights, a filmic curve, depth of field for portraits | MSAA, ACES tone map, bloom |

The renderer is not the weakest part. The content is.

## 2. What the phone has measured

One phone test carries numbers: the APK of `797d156` on the SM-S921B ([record](PHONE_TEST_2026-11-15.md)). REAL + VERIFIED for that APK only.

| Measurement | Value |
|---|---|
| Graphics path | OpenGL ES 3.2 through `ANGLE ((Samsung Xclipse 940) on Vulkan 1.3.304)` |
| Generated avatar | 1,100,168 triangles, 821,215 vertices, GLB 83.4 MB |
| Frame rate | 53.8 FPS over the last 240 frames, median interval 17.7 ms, p95 24.6 ms |
| Quality tier | Started at HIGH (4x MSAA, 2048 shadow map, 4 bloom levels), ended at MEDIUM (2x, 1024, 3). The renderer steps down only after more than 30 consecutive frames slower than 30 FPS |
| Memory | Native heap 304 to 306 MB. Requested GPU bytes: textures 72.7 MB, geometry 127.9 MB, render targets 84.2 MB |
| Start | Import 1.3 s, first frame 2.2 s |

What the figures imply (inference; per-pass GPU times were not recorded):

- The avatar covered 194,401 pixels of the capture and had 1.1 million triangles. About half face the camera, so each visible triangle covers about **a third of a pixel**. A GPU shades in blocks of 2 x 2 pixels, and with MSAA nearly every sample then belongs to a different triangle, so shading work multiplies. A mesh this dense is slow on any GPU.
- The median interval of 17.7 ms sits one millisecond above the render thread's own 16.67 ms sleep rhythm. At MEDIUM most frames were waiting for the timer, not for the GPU.
- 72.7 MB of textures is one 2048 x 2048 set, uncompressed.

Not measured on the phone: generation time, heat, battery, any session longer than 38 seconds, and every build since `797d156`.

## 3. Repairs

Faults of the machine as it is, most valuable first. Size: S = one working session and one CI cycle; M = several; L = many, with phone tests between.

| # | Fault | Evidence | Repair | Size |
|---|---|---|---|---|
| R1 | Every test build needs an uninstall. That wipes settings and orphans the diagnostics files, which then pile up as `renderer_diagnostics (N).json` | Each CI debug APK is signed with a key made for one job; phone test defect P3 | One fixed debug keystore, stored as a GitHub Actions secret, used by every test build so a new build installs over the old one | S, needs the owner |
| R2 | The crash you reported on `041e31d` is not confirmed fixed | Cause inferred: a generated avatar sent to the Filament preview, which reserves about 24 bytes per vertex per morph target. Guarded since `210ae9a`. No phone evidence since | Write Android's own exit history (`getHistoricalProcessExitReasons`: crash, native crash, ANR, low-memory kill) into `renderer_diagnostics.json` at start-up. Today it is shown only on the Settings page | S |
| R3 | The mesh is about ten times too dense, and each vertex is 88 bytes of floats | 1.04 to 2.13 million triangles; a third of a pixel per triangle on the phone; HIGH did not hold 30 FPS; native heap 304 MB on the phone and 734 MB on the emulator for the gown | Simplify after extraction to 100 to 150 thousand triangles with the detail baked into the normal map, or raise the lattice cell on trunk and limbs. Pack the vertex into 32 bytes. **Partly done on this branch (render-side only):** the renderer already built three index-only LODs but the forward colour pass selected them from unreachable 8 m / 16 m camera thresholds, so at Avatar Studio's 3 m orbit it always drew LOD0 -- the full ~1.1 M-triangle mesh the phone measured. It now chooses one level per frame from screen-space density (`src/render/af_lod_select.h`), which lands a full-body view on LOD1 (~132k, inside the 100-150k band above), earns LOD0 back on a close-up and falls to the coarsest level when the avatar is small; the selection maths and the per-level surface-area measurement are host-verified (`avatar_lod_select_regression`) against the recorded S24 numbers. Still open: the extraction-side simplification and normal-map bake, the 32-byte vertex, and the on-device frame-rate/pixel confirmation (NDK build + emulator/phone gates, not run here). See `PROJECT_STATUS.md` root cause 16 | M |
| R4 | The face never moves | The importer does not read morph targets, the app draws the re-imported file, and no code drives a blendshape | Read sparse morph targets on import, morph on the GPU, add an automatic blink and small eye movements | M |
| R5 | The render thread sleeps to a 16.67 ms rhythm instead of following the display, and draws continuously when nothing changes | `AvatarForgeSurfaceView.kt`; p95 interval 24.6 ms on the phone | Drive frames from vsync with the Android Frame Pacing library (Swappy) or Choreographer; call `Surface.setFrameRate`; stop drawing when camera and pose are still | S to M |
| R6 | Heat is consulted once, before generation, and only at the emergency level. Quality steps down on frame rate alone | `DeviceResourceGovernor.kt`, `af_render.cpp` | Poll thermal headroom and step quality before the system throttles; tell the scheduler what the render and generation threads are doing (section 6) | M |
| R7 | Reflections cost 16 samples of an analytic sky per pixel and show no real surroundings | `af_render.h` header, `af_environment.h` | A prefiltered environment cube map and one lookup texture: one or two fetches per pixel, and a real room in the eyes and on the skin | M |
| R8 | Textures are uploaded uncompressed and the GLB is 67 to 122 MB | 72.7 MB requested for one 2048 set; `af_gl.cpp` uses RGBA8; the importer refuses meshopt, Draco and KTX2 | Compressed textures (ASTC, once the phone's report confirms it) in KTX2, quantised and meshopt-compressed geometry, and an importer that accepts them | M |
| R9 | Nobody who wrote the creator has seen it drawn, and no test taps a card | No Android device where the work is done | An instrumented test that opens every section, taps a card in each and saves screenshots as CI evidence. A first step exists since `ff9e6ba`: `GlbClipPreviewTest` opens a file, scrolls the screen and taps a chip | S to M |
| R10 | A Vulkan backend sits in the tree, switched off and behind | `AF_ENABLE_VULKAN` off; `af_vk.cpp` has no GPU skinning; its shaders are 2,578 lines against 8,987 for GLES | Decide: bring it level from one shared shader source, or remove it. Do not carry it as it is | S to remove, L to finish |
| R11 | A stale failure can overwrite a good report, and one GL error is logged at shutdown | Phone test defects P1 and P2 | Tag each report with its session; ignore errors of a frame cut off by surface release | S |
| R12 | Controls missing | No seek slider for the app's own avatars; the Motion section of an imported file has a row of four buttons that is wider than a phone screen ("Next" out of sight until swiped; seen in the emulator screenshot of `787e030`); pause, loop, speed and Save have no test; no Accessories or Background section; no intro screen or icon; card pictures are all female; no step-by-step flow | Add them in that order | M |
| R13 | Generation time on the phone is unknown | Never recorded | Record the time of each stage in the diagnostics report | S |
| R14 | The toolchain is a year behind the phone | AGP 8.7.3, Kotlin 2.1.0, Compose BOM 2024.12.01, target SDK 35, NDK 26.1; the phone runs Android 17 (SDK 37) | One dedicated update. The newer CPU and GPU headroom calls need it | M |
| R15 | Two documents still said every model is shown static | `BUILD_AND_VERIFY.md`, `PROJECT_STATUS.md` | Corrected in the commit that adds this file | done |
| R16 | An imported model built from several textured parts came up mostly grey, and nothing limited texture memory | The owner's reference models on the host: 2 of 14 and 4 of 45 parts textured; 1,152 MiB of textures in one file | Such a file opens in the glTF viewer (Filament); textures are reduced at import to a 448 MiB budget; the viewer got orbit, zoom and ambient light | done in `7e4f0cb`, unit-tested, not run on a phone |
| R17 | The glTF viewer shows no tone mapping, anti-aliasing, shadows or reflections | Filament post-processing was switched off in `FilamentPreviewView.kt`; one directional light and a uniform ambient term | **Partly done in `50bbe25` and `96152cd`:** MSAA and FXAA, ambient occlusion, a shadow-casting key light and the PBR Neutral tone curve (Filament's default curve washed skin tones out). Seen on the CI emulator only. Still open: a prefiltered environment for reflections, and the phone test of each step | M |
| R18 | The engine's own importer keeps one colour texture per imported model, ignores transparency and adopts no foreign skeleton | `af_engine.cpp` (`selectedBaseTex`), host import of the reference models | Either extend it (materials with their own textures, alpha, foreign skeletons, GPU morph targets) or leave authored models to Filament: the decision in section 5.2 | L |
| R19 | An avatar made here cannot use a clip made for another skeleton | Its skeleton has 71 joints of its own and its 15 clips are written for it. The owner's file with 14 animations uses the 66-joint Mixamo skeleton, for which clips exist in large numbers; they play only on the figure they came with ([`OWNER_MODELS_2026-11-15.md`](OWNER_MODELS_2026-11-15.md), section 7) | Retarget at import: read the clips of a rigged file, map the joints by name, convert the rotations between the two rest poses, write them as clips of the avatar. `tools/glb_figures/` already reads and resamples such clips on a computer. The rights to each clip have to be settled before any ships | M |

R1 and R2 come first because every later step is judged by a phone test.

## 4. The upgrade: assemble the human from authored data

### 4.1 The principle

Stop deriving the human from mathematics. Assemble it from a base mesh that an artist sculpted, with one morph target per slider, and with textures that came from scans. Every realistic character creator works this way. The engine's job becomes choosing weights, blending, and rendering well.

The last two days show why. The preset commits moved hair and clothes from wrong to recognisable, and each step cost hours for a few millimetres of form. That curve does not reach an eyelid crease.

### 4.2 The kit (built offline, shipped as an asset pack)

| Part | Content |
|---|---|
| Body | One base mesh for both sexes, one skeleton, skin weights, two or three levels of detail. Target: 100 to 120 thousand triangles at the closest level, 40 to 60 thousand for a full-body view |
| Shape targets | Sparse vertex deltas: sex, weight, muscle, height and proportions, breast size and firmness, glutes, waist and hips, the six starting looks, face detail. Adults only: no child range is baked |
| Expressions | 52 ARKit-named face shapes and 15 visemes, for the head only |
| Skin sets | Per look and sex: colour, normal, roughness and specular, thickness. A tint model on top for continuous skin tone. A tiling pore-detail normal |
| Eyes, teeth, lashes, brows | Separate meshes. Iris colour is a shader parameter (the renderer already has the eye shader) |
| Hair | Card meshes per style, skinned to the head with a few spring bones; flow and depth maps; colour as a parameter |
| Garments | Meshes skinned to the same skeleton, carrying the same shape deltas so they follow every slider; a mask that hides the body underneath |
| Motion | Clips retargeted to the kit skeleton |
| `kit.json` | Source, revision and licence of every asset, checked by CI |

Formats: glTF for meshes, KTX2 with ASTC for textures.

### 4.3 The runtime

```
creator choices or prompt
        |
     af_spec                 unchanged: this stays the contract
        |
   kit solver                sex, build, height, bust, glute, face[8], tape values -> morph weights
        |
   assembler (CPU, milliseconds)
        base + sum(weight x sparse delta)  -> rest vertices and normals
        garments and hair follow through the same deltas
        |
   GPU                       skinning, 52 expression morphs on the head, skin / eye / hair / cloth materials
```

- **Shape sliders are applied on the CPU when a slider moves; only expressions are GPU morphs.** That keeps morph storage to the head and makes the creator live: a slider changes the avatar in the next frame instead of after a regeneration.
- The anthropometric solver keeps its job. Today it turns a waist of 78 cm into radii of a field; in the kit it finds the weights that make the mesh measure 78 cm.
- The distance-field generator stays for what it does acceptably and what no kit covers: plate armour, helmets, props, and a fully procedural fallback.

### 4.4 What stays, what changes

| Stays | Changes |
|---|---|
| `af_spec`, the prompt parser, the creator screens and their tests | Body, face, hair and clothes come from the kit, not from `af_field.cpp` |
| A renderer already in the app (which one: section 5.2), telemetry and diagnostics | Skin, eye and hair shading, lighting, anti-aliasing (below) |
| Skeleton-driven animation on the GPU, the clip API | The skeleton and the clips come from the kit; expressions are added |
| GLB export and import, the validator, the fuzz tests | Compressed textures and geometry are accepted |
| The five CI workflows and the publishing gate | A kit-build job; a licence check on `kit.json` |

### 4.5 Rendering work that goes with it

In order of visible effect for the cost:

1. **Environment light.** Prefiltered HDR environment maps, a soft key light, studio presets. Faces need something real to reflect.
2. **Skin.** Pre-integrated subsurface shading first (one lookup, no extra pass), two specular lobes, a cavity map, pore detail. Screen-space diffusion on skin pixels only as a higher tier, after measuring its cost on the phone.
3. **Eyes.** Separate geometry, look-at, saccades, blink, a soft shadow mesh under the lids. Eyes decide whether a face reads as alive.
4. **Hair.** Cards with soft edges (alpha to coverage or dithered alpha), a depth pre-pass, two shifted highlights, back-light transmission, root darkening.
5. **Ambient occlusion and contact shadows** at half resolution.
6. **Anti-aliasing.** Measure 4x MSAA against temporal anti-aliasing on the phone; add specular anti-aliasing either way.
7. **Photo mode.** When the avatar is still, accumulate 16 to 64 jittered frames with randomised shadow and depth-of-field samples. This is the cheapest route to a still that looks rendered offline, and it is where ray-traced shadows become affordable (section 6).

## 5. Best parts to match

### 5.1 Content

| Need | Recommended part | Licence | Why this one |
|---|---|---|---|
| Body mesh and shape sliders | **MakeHuman base mesh and targets**, baked with **MPFB2** in headless Blender | Assets CC0. MPFB2 is GPL: used as a build tool, never shipped | The project states that its base mesh, targets and skins are CC0 and that building your own character generator from them is encouraged. Its targets cover most of your creator's sliders: sex, weight, muscle, height and proportions, breast size, and ethnic macro shapes that mix |
| Parameter layer | **Anny** (NAVER LABS Europe) | Apache-2.0 | Built on the MakeHuman assets, calibrated on WHO population data, interpretable sliders, several rigs and mesh resolutions |
| Alternative body | **MHR, Momentum Human Rig** (Meta) | Apache-2.0 | A skeleton, a mesh with levels of detail, pose correctives and face blendshapes. Its description lists no skin textures |
| Face shapes and visemes | MPFB asset packs `faceunits01` (ARKit-compatible, 52) and `visemes02` (15) | Published as CC0 target packs | Same mesh, so no grafting |
| Scan-derived face variety | **ICT-FaceKit** (USC ICT), light model | MIT | 100 identity shape modes and 53 expression shapes from light-stage scans. Needs fitting to the body mesh |
| Sculpting reference | Blender Studio **Human Base Meshes** | CC0 | Realistic bodies, heads, hands and feet with clean topology |
| Skin textures, first tier | MakeHuman CC0 skins plus our own pore detail and tint model | CC0 | Free and clean. Quality: a good game character, not a photograph |
| Skin, head and hair, top tier | **Your choice, see section 9**: your own Daz content with an Interactive Licence for every product used; or MetaHuman characters exported from Unreal Engine 5.6 or later; or purchased scans whose licence fits | Daz: per-product licence. MetaHuman: Unreal EULA, free below US$1 million a year, usable in other engines since June 2025 | This tier is what makes the difference between "game" and "photo". It costs money, a PC, or both |
| Environment light | Poly Haven HDRIs | CC0 | Studio and outdoor light without a licence question |
| Motion | **Quaternius Universal Animation Library** | CC0 | More than 120 humanoid clips made to be retargeted |
| First garments | MPFB clothing packs for fit tests; then garments authored for the kit, swimwear first | Use only the packs labelled CC0 (some community packs are CC-BY); your own | Swimwear is the simplest garment and is what your references wear |

### 5.2 Engine and tools

| Need | Part | Licence | Note |
|---|---|---|---|
| Renderer | **Open: decide by a test on the phone.** Two candidates, both already in the APK | AvatarForge's renderer is yours; Filament is Apache-2.0 | **AvatarForge's renderer** has skin, eye, hair and cloth shading, GPU skinning, measured telemetry and a pixel-checked test path, and its lighting can be changed freely. For an authored model it lacks materials with their own textures, transparency, foreign skeletons and GPU morph targets (R18). **Filament** draws authored glTF models completely today, including the owner's reference models, and brings tone mapping, anti-aliasing, ambient occlusion, shadows and environment lighting as switches (R17). Its built-in skin model is simple; a custom skin material is possible but its lighting model is fixed in more places. The first version of this plan chose AvatarForge's renderer outright. After importing real authored models that is no longer safe to assume: load the same kit body in both, on the phone, and compare picture and frame time before building further on either |
| Texture compression | **astcenc** for ASTC, **KTX-Software** for KTX2 | Apache-2.0 | One device, so ship one format. The app has never asked this phone which compressed formats it supports: add that to the diagnostics report first. ETC2 is guaranteed by OpenGL ES 3 if ASTC is not there |
| Mesh compression | **meshoptimizer** and `gltfpack`; `KHR_mesh_quantization`, `EXT_meshopt_compression` | MIT | The importer must learn both |
| Kit build | Headless Blender with MPFB2 in a GitHub Actions job | GPL tools, not shipped | An agent can run it; no PC needed |
| Frame pacing | **Android Frame Pacing library (Swappy)** | Apache-2.0 | Works with the EGL swap the renderer already does |
| Heat and scheduling | **Android Dynamic Performance Framework**: performance hints, thermal headroom, game mode; CPU and GPU headroom on Android 16 and later | platform | Section 6 |
| Upscaling | **Arm ASR** (temporal, derived from AMD FSR 2) or FSR 1 (spatial) | MIT | Only if the measured GPU time needs it |
| Ambient occlusion | A ground-truth AO pass (XeGTAO is a public reference) | MIT | Half resolution |
| Profiling | The app's own telemetry JSON first; Perfetto and RenderDoc on a debug build when a number needs explaining | | |

### 5.3 Parts that look right and are not

| Part | Why not |
|---|---|
| Reallusion Character Creator content | Their staff state that no Reallusion content may be used in an in-app character generator or customizer without a negotiated Enterprise licence |
| SMPL and SMPL-X family | Research licences; commercial use needs a paid licence. Many neural-avatar projects depend on them |
| 3D Scan Store scans | The commercial licence excludes character-maker programs whose results can be exported as 3D models. This app has a Save that writes a GLB |
| Daz content without the Interactive Licence | The ordinary licence covers pictures and video. Shipping the 3D data in an app needs the Interactive Licence for the base figure and for each morph, skin, hair and garment product |
| MetaHuman without a check | Usable in other engines, but not to train AI, and it needs a PC with Unreal Engine. Read the content addendum before building a runtime customizer on it |
| Models and textures found on the web, and the pictures in the Dropbox guides | No licence, no use |

I am not a lawyer. Each licence must be read against its current text before money or work is spent.

## 6. Performance on the S24

### 6.1 Budgets

| | Today (measured) | Target (not yet measured) |
|---|---|---|
| Triangles drawn | 1.10 million on the phone; 2.13 million for the gown | 120 thousand close up, 60 thousand full body |
| Vertex size | 88 bytes | 32 bytes |
| Texture memory | 72.7 MB for one uncompressed 2048 set | Under 200 MB for a 4K head, a 4K body, hair, eyes and a garment in ASTC |
| File per avatar | 67 to 122 MB | 5 to 20 MB; the kit is shared |
| Native heap | 304 MB on the phone, 734 MB on the emulator for the gown | Under 400 MB with everything loaded |
| Slider to picture | A full regeneration: 25 to 88 s on a 2-core host, unknown on the phone | Under 0.1 s |
| Frame rate | 53.8 FPS at MEDIUM | 60 FPS locked at the highest tier for 30 minutes; 120 Hz as an option |
| First frame | 2.2 s | Under 0.5 s |

### 6.2 What to do, in order

1. **Cut the triangle count** (R3 now, the kit later). Removes the vertex load and the overshading together.
2. **Follow vsync and draw on demand** (R5). Smoother, and the phone stays cool while you look at a still avatar.
3. **Compress textures and shrink the vertex** (R8). Less memory traffic per frame, room for 4K maps.
4. **Make each pixel cheaper.** Environment lookup instead of 16 samples (R7). Skin scattering only where there is skin. Bloom and occlusion at half resolution. An 11-11-10 float colour target instead of 16-bit RGBA where it is supported.
5. **Use the thermal forecast.** Step quality down before the system throttles and back up when there is room. Report each frame's work time through a performance-hint session so the scheduler keeps the render thread on a fast core. Declare the app a game and support game mode.
6. **Cache compiled shaders** so the first frame does not wait for them.
7. **Build the release for arm64 only**, with link-time optimisation.
8. **Then measure a native Vulkan path against GLES on the phone**, same content, and keep the faster one.

### 6.3 What not to assume

- **"Vulkan will make it fast."** On this phone OpenGL ES already runs on Vulkan through ANGLE, and one avatar was 25 to 29 draw calls in the recorded sessions, so the API is not where the time goes. Android's guidance is that Vulkan is the primary graphics API and that OpenGL ES is no longer under active feature development. Native Vulkan is worth having for what GLES cannot reach: variable-rate shading, ray queries, pipeline caches. Do it after the content change and decide by an A/B test on the phone.
- **"MSAA is nearly free."** True on the tile-based GPUs in most phones. The Xclipse 940 is built on AMD's RDNA 3 architecture. Measure it.
- **"Ray tracing."** The Xclipse 940 supports ray queries and ray-tracing pipelines. A published case study measured 29 ms a frame for ray-queried ambient occlusion and shadows in a 260 thousand triangle scene on this GPU. That is a photo mode for stills, not a 60 FPS feature.
- **"120 Hz."** Twice the GPU work for the same picture. Spend the budget on quality at 60 and offer 120 as a switch.
- **The emulator.** It draws in software. No performance claim can come from CI; every one needs your phone.

## 7. Stages and gates

Each stage ends with a build you install and a report from the phone. A stage is not done until the phone says so.

| Stage | What is built | Gate on the phone | Needs from you |
|---|---|---|---|
| 0 Repairs | R1 to R9, R11, R13 | A build installs over the previous one. The report carries the exit history and the generation time. The highest tier holds 60 FPS for ten minutes with the simplified mesh | The signing secret (R1); a test |
| 1 Kit body | The kit build in CI from CC0 assets; solver and assembler; the creator live | A slider changes the avatar within a tenth of a second. Side-by-side sheets of old and new. Memory and file size inside the budgets | A look at the sheets |
| 2 Skin, eyes, hair, light | Section 4.5, items 1 to 6; blink and eye motion | Your judgement against the Bodys references under the same light. 60 FPS for 30 minutes | Your verdict |
| 3 Scan-grade content | Head, skin and hair of the top tier in the same kit format | The licence record in `kit.json`; your judgement of a close-up still | The choice in section 9 |
| 4 Motion and voice | Captured clips, foot locking, spring bones, lip sync from the Voice page | A talking avatar with moving lips; no foot sliding in the walk | A test |
| 5 Photo mode and the neural tier | Accumulated stills with ray-traced shadows; optionally Gaussian avatars made on a server | A still you would take for a photograph | Whether a server step is acceptable |

Stages 0 and 1 need nothing bought. Stage 3 is where licences or a PC come in.

## 8. How real it can get

- **Your references are offline renders.** A path tracer spends seconds to minutes of a desktop GPU on one picture. The phone has 16 milliseconds.
- **Mesh and physically based shading on this phone**, with scan-grade content: stills at phone size under studio light can pass for photographs; close-ups in motion show it is real-time, mostly in hair edges, the mouth interior and skin at grazing angles. This is an estimate from what phones of this class render today, not a test of this GPU.
- **Neural avatars.** What published work shows:
  - TaoAvatar: full-body talking Gaussian avatars at 90 FPS on an Apple Vision Pro, built from multi-view capture of a real person.
  - LHM: an animatable Gaussian avatar from one picture in seconds on a server GPU. Its code is Apache-2.0; it is built on SMPL-X.
  - Mobile-GS: Gaussian scenes at 116 FPS on a Snapdragon 8 Gen 3 phone. Static scenes, a different GPU.
  - None of this has been measured on the Xclipse 940.
  - The trade: the result is a recording of one appearance. No sliders, no change of clothes or hair, lighting baked in, creation on a server.
  - It fits as an extra ("make an avatar from a picture"), not as the creator.

## 9. Decisions only you can make

1. **A signing key for test builds** (R1). I can generate a debug keystore and the workflow change; you add the secret in the repository settings. It ends the uninstall before every test.
2. **The top content tier.** Which of these do you have or want: (a) CC0 only; (b) your own Daz library, with Interactive Licences bought for what ships; (c) a Windows PC with Unreal Engine for MetaHuman export; (d) purchased scans. The kit format is the same for all four, so stage 1 can start before you decide.
3. **Online or offline.** The blueprint says the core must work offline. A photographic tier made on a server breaks that for that feature. Acceptable or not?
4. **Export.** If licensed content ships, Save may have to refuse to write that content into a GLB.
5. **The intro pictures and the icon** in your Dropbox: are they yours to ship? Still open.

## 10. Sources

Read on 2026-11-15.

- MakeHuman and MPFB assets are CC0; building your own character generator is encouraged: [static.makehumancommunity.org/mpfb/faq](https://static.makehumancommunity.org/mpfb/faq/build_other_chargen.html), [can I sell models](https://static.makehumancommunity.org/mpfb/faq/can_i_sell_models.html)
- MPFB 2.0.16, ARKit face units and viseme packs: [release notes](https://static.makehumancommunity.org/mpfb/releases/release_2016.html), [lip sync](https://static.makehumancommunity.org/mpfb/docs/lipsync/index.html)
- Anny, Apache-2.0: [NAVER LABS Europe](https://europe.naverlabs.com/blog/anny-a-free-to-use-3d-human-parametric-model-for-all-ages/)
- MHR, Apache-2.0: [github.com/facebookresearch/MHR](https://github.com/facebookresearch/MHR)
- ICT-FaceKit, MIT: [github.com/USC-ICT/ICT-FaceKit](https://github.com/USC-ICT/ICT-FaceKit)
- Blender Studio Human Base Meshes, CC0: [CG Channel](https://www.cgchannel.com/2023/06/download-blender-studios-free-human-base-meshes/)
- Quaternius Universal Animation Library, CC0: [quaternius.itch.io](https://quaternius.itch.io/universal-animation-library)
- MetaHuman licence since Unreal Engine 5.6: [CG Channel](https://www.cgchannel.com/2025/06/you-can-now-sell-metahumans-or-use-them-in-unity-or-godot/), [Epic Games](https://www.unrealengine.com/news/all-the-big-news-and-announcements-from-the-state-of-unreal-2025)
- Reallusion content in a customizer: [Reallusion forum](https://discussions.reallusion.com/t/clarification-on-eula-for-using-character-creator-with-custom-assets-and-edited-morphs-in-game-with-character-customization-system/14770)
- Daz Interactive Licence per product: [Daz forum](https://www.daz3d.com/forums/discussion/721771/answered-cost-for-interactive-license-for-original-genesis-figure), [Daz forum](https://www.daz3d.com/forums/discussion/379731/using-daz-studio-for-game-models)
- 3D Scan Store licence: [terms](https://www.3dscanstore.com/terms-and-conditions-licensing)
- Exynos 2400 and Xclipse 940: [nanoreview](https://nanoreview.net/en/soc/samsung-exynos-2400); SM-S921B: [deviceinfohw](http://www.deviceinfohw.ru/devices/item.php?item=96590)
- Ray tracing on the Xclipse 940: [ACM, Ray Tracing with Bindless Vulkan on Mobile Devices](https://dl.acm.org/doi/10.1145/3721239.3734103)
- Vulkan as Android's primary graphics API, ANGLE: [Android Developers](https://developer.android.com/games/develop/vulkan/overview)
- CPU and GPU headroom in Android 16: [Android Developers](https://developer.android.com/about/versions/16/features)
- Arm ASR, MIT: [Arm](https://developer.arm.com/community/arm-community-blogs/b/mobile-graphics-and-gaming-blog/posts/join-the-upscaling-revolution-with-arm-asr)
- TaoAvatar: [summary](https://www.emergentmind.com/papers/2503.17032); LHM: [github.com/aigc3d/LHM](https://github.com/aigc3d/LHM); Mobile-GS: [arXiv](https://arxiv.org/html/2603.11531v1)
