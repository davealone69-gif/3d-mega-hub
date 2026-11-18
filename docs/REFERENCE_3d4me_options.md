# 3D4ME avatar customisation options — harvested reference

Status: REFERENCE (copied from a donor project, mapped to this repository). Recorded 2026-11-18.

## Provenance, licensing, scope

- Source donor: `davealone69-gif/3d4me` (cloned depth-1 for this audit; not modified). Display name **3D4ME**; Expo slug / package `3d4me`; Android/iOS app id `space.manus.figural.*`.
- Authoritative files copied here: `shared/character.ts` (the zod option schema + labelled choice sets + numeric ranges/defaults), `shared/avatar3d.ts` (the Three.js builder + colour swatches + PBR + proportions), `components/figural/StudioControls.tsx` (picker/attestation UI + theme palette), `docs/model-capabilities.md`.
- Licence to preserve when redistributing derived code: **Three.js (MIT)** and **expo-three (MIT)**. No third-party human models are bundled by that project; its avatar is original procedural geometry. Preserve those notices if any of its constants are ported.
- Its server/drizzle/oauth code is **scaffolding only** (on-device editor; cloud routers unused). Nothing here depends on it.
- Adult handling: that project gates adult mode behind an explicit four-point attestation (`isFictionalAdult`, `isAtLeastTwentyOne`, `hasRightsAndConsent`, `noMinorOrNonConsensualReplica`) and a minimum age of 21 in the schema. This matches, and does not relax, this repository's own adult-confirmation rule: do not expose unclothed/adult paths in the Hub UI until the visible-pixel/runtime gate is fixed (see `docs/PROJECT_REUSE_AUDIT_2026-11-15.md`).

Sections 1–4 below are FACTS copied from the donor. Sections 5–6 are THIS repository's analysis (mapping to AvatarForge and the work plan), clearly labelled.

---

## 1. Choice sets (id -> label), with colour hexes where applicable

### Skin tone (named swatch -> sRGB hex)
| id | label | hex |
|---|---|---|
| porcelain | Porcelain | #F1D5C2 |
| warm-beige | Warm beige | #C99378 |
| olive | Olive | #A87455 |
| golden-brown | Golden brown | #9C6247 |
| deep-brown | Deep brown | #623E32 |
| ebony | Ebony | #392721 |
Plus a free `skinHex` (custom). Default skinHex `#C99378` (= warm-beige).

### Hair colour (named -> hex)
| id | label | hex |
|---|---|---|
| black | Black | #17161A |
| dark-brown | Dark brown | #382720 |
| chestnut | Chestnut | #75452F |
| blonde | Blonde | #C4A361 |
| auburn | Auburn | #853F2D |
| silver | Silver | #AEB2BD |
Plus free `hairHex`. Default `#382720` (= dark-brown).

### Eye colour (named -> hex)
| id | label | hex |
|---|---|---|
| brown | Brown | #634530 |
| hazel | Hazel | #85723E |
| green | Green | #4B805F |
| blue | Blue | #577DA4 |
| grey | Grey | #87909C |

### Body build (id -> label, and builder scale factor)
| id | label | factor |
|---|---|---|
| slender | Slender | 0.88 |
| athletic | Athletic | 0.98 |
| curvy | Curvy | 1.08 |
| strong | Strong | 1.12 |
| soft | Soft | 1.10 |
The builder multiplies shoulder/chest/waist/hip by `factor * <width slider>`; it additionally scales whole silhouette by 0.92 (slender) / 1.08 (strong) / 1.0 (others).

### Face shape (id -> label, and {x, y, cheek} multipliers)
| id | label | x | y | cheek |
|---|---|---|---|---|
| oval | Oval | 1.00 | 1.03 | 1.00 |
| heart | Heart | 1.04 | 1.02 | 1.04 |
| square | Square | 1.04 | 0.98 | 0.94 |
| diamond | Diamond | 0.97 | 1.06 | 1.08 |
| round | Round | 1.06 | 0.95 | 1.08 |

### Hair style (id -> label; builder strand parameters)
| id | label | strands | length scale | wave | strand radius | notes |
|---|---|---|---|---|---|---|
| sleek-bob | Sleek bob | 52 | 1.0 | 0.012 | 0.007 | tips clamped >= y1.70 |
| long-waves | Long waves | 52 | 1.0 | 0.028 | 0.007 | |
| curly-volume | Curly volume | 68 | 1.0 | 0.050 | 0.011 | thicker strands |
| braided-updo | Braided updo | 36 | 1.0 | 0.012 | 0.007 | points raised to y~1.92/1.99 |
| short-crop | Short crop | 52 | 0.33 | 0.012 | 0.007 | tips clamped to y1.78 |
Hair length overall = `(0.10 + hairLength * 0.51)`; cap scaled by `hairVolume` (radii 0.17/0.11/0.155 * volume). Strands skip the central front parting (sin(angle)>0.68 && |cos|<0.75).

### Clothing STYLE (geometry built by the model)
ids: `tailored`, `casual`, `editorial`, `formal-dress`, `swimwear`, `lingerie`. Default `editorial`.
Behaviour: non-adult forces `lingerie`->`editorial`; with `adultMode`, `swimwear`/`lingerie` become cropped (top starts higher at y1.13 vs 0.79; no sleeves; legs bare to ~0.10). `formal-dress` (non-cropped) builds a skirt loft instead of trouser legs. `tailored`/`editorial` add three metal buttons (y 1.3/1.2/1.1) and a belt clasp (#C9A66B).

### Clothing MATERIAL (fabric PBR)
| id | roughness | metalness |
|---|---|---|
| cotton | 0.90 | 0.00 |
| silk | 0.32 | 0.02 |
| satin | 0.22 | 0.05 |
| denim | 0.84 | 0.00 |
| leather | 0.40 | 0.02 |
| latex | 0.12 | 0.08 |
Default `silk`. Fabric gets clearcoat 0.22 / clearcoatRoughness = its roughness. Metal trim/clasp/button uses #C9A66B, roughness 0.27, metalness 0.7. Plus free `clothingHex`. Default `#393452`.

### Render-presentation sets (NOT geometry; stage/lighting/camera/output choices)
- wardrobe: tailored-evening, mature-casual, fashion-editorial, fantasy-formal, resort-editorial
- expression: confident, serene, playful, intense, soft-smile
- lighting: cinematic-studio, golden-hour, neon-noir, soft-window, editorial-flash
- cameraAngle: eye-level, low-angle, three-quarter, close-portrait, full-body
- sceneBackground: studio-cyclorama, luxury-interior, night-city, sunset-terrace, minimal-abstract
- outputBackground: studio, transparent
- renderViews: front(0), side(pi/2), rear(pi), three-quarter(pi/4)

## 2. Numeric sliders (min .. max, default)
| field | min | max | default |
|---|---|---|---|
| age (int) | 21 | 80 | 28 |
| bodyHeight | 0.82 | 1.18 | 1.00 |
| shoulderWidth | 0.75 | 1.30 | 1.00 |
| chestWidth | 0.72 | 1.32 | 1.00 |
| waistWidth | 0.70 | 1.30 | 1.00 |
| hipWidth | 0.72 | 1.35 | 1.00 |
| armLength | 0.80 | 1.20 | 1.00 |
| legLength | 0.80 | 1.20 | 1.00 |
| faceWidth | 0.78 | 1.22 | 1.00 |
| jawWidth | 0.70 | 1.25 | 1.00 |
| eyeSize | 0.75 | 1.30 | 1.00 |
| eyeSpacing | 0.80 | 1.25 | 1.00 |
| noseSize | 0.75 | 1.25 | 1.00 |
| hairLength | 0.15 | 1.00 | 0.62 |
| hairVolume | 0.65 | 1.40 | 1.00 |
Other scalars: `identitySeed` (string 8..100, default "figural-character-anchor"); `adultMode` (bool, default false); `skinHex`/`hairHex`/`clothingHex` (#rrggbb).
Defaults assembled: athletic / oval / warm-beige / long-waves / dark-brown / hazel / fashion-editorial / confident / cinematic-studio / three-quarter / studio-cyclorama / studio.

## 3. Base-material PBR defaults (MeshPhysicalMaterial)
| surface | colour | roughness | metalness | clearcoat | clearcoatRoughness |
|---|---|---|---|---|---|
| generic material() | (given) | 0.72 | 0.00 | 0.10 | 0.66 |
| skin | skin swatch | 0.58 | 0.00 | 0.08 | 0.52 |
| hair | hair swatch | 0.38 | 0.02 | 0.26 | 0.36 |
| eye white | #F2EADF | 0.23 | 0.00 | - | - |
| iris | eye swatch | 0.18 | 0.08 | - | - |
| pupil/dark | #211618 | 0.23 | 0.00 | - | - |
| lips | #995F5A | 0.42 | 0.00 | - | - |
| metal trim | #C9A66B | 0.27 | 0.70 | - | - |

## 4. Geometry proportion recipe (metres, Y-up; root.scale.y = bodyHeight)
Torso = loft through rings (y; rx; rz), rz near-constant ~0.13-0.17:
(0.73; 0.235*hip; 0.137) (0.83; 0.31*hip; 0.17) (0.98; 0.275*waist; 0.158) (1.12; 0.225*waist; 0.132) (1.29; 0.29*chest; 0.17) (1.44; 0.315*chest; 0.167) (1.57; 0.31*shoulder; 0.132).
Neck capsule (0,1.54,0)->(0,1.72,0) r0.095. Head sphere c(0,1.795,0) s(0.156*faceW*shapeX, 0.208*shapeY, 0.153). Jaw sphere c(0,1.68,0.024) s(0.132*jawW,0.105,0.133). Cheeks at (±0.097,1.765,0.079) s(0.061*cheek,...). Eye stack at x=±0.065*eyeSpacing, y1.827-1.829: socket(z0.130,s~0.047*eyeSize), white(z0.151,0.034), iris(z0.164,0.014 wide/0.017 tall), pupil(z0.17,0.006). Brows capsule y~1.87. Ears at (±0.154,1.79,0.006) s(0.035,0.064,0.032). Nose bridge (0,1.826,0.129)->(0,1.757,0.176) r0.019*nose; tip (0,1.755,0.179) s~0.029*nose. Lips (0,1.716,0.143) s(0.051,0.012,0.018).
Arms: shoulder (±0.32*shoulder,1.48,0) -> elbow (±0.43,1.08*armLen,0.01) -> wrist (±0.45,0.82*armLen,0.045) -> hand (±0.46,0.72*armLen,0.06); upper-arm r0.066, forearm r0.045; hand s(0.044,0.073,0.028); FOUR finger capsules per hand.
Legs: hip (±0.135*hip,0.77,0) -> knee (±0.15,0.43*legLen,0) -> ankle (±0.155,0.065*legLen,0) -> foot (±0.155,0.045*legLen,0.083); thigh r0.105*hip, shin r0.061; foot s(0.071,0.045,0.13).

Capability metadata embedded in GLB userData under keys `figural` (legacy) and `3d4me`:
meshEditableBySettings = [bodyHeight, bodyBuild, shoulderWidth, chestWidth, waistWidth, hipWidth, armLength, legLength, faceShape, faceWidth, jawWidth, eyeSize, eyeSpacing, noseSize, skinHex, hairStyle, hairLength, hairVolume, hairHex, clothingStyle, clothingMaterial, clothingHex, adultMode]; materials = [skin, hair, eyes, fabric, metal-trim]; contentMode = adult-opt-in|standard. Export validates the GLB header (magic 0x46546C67, version 2, total-length field).

UI theme palette (from StudioControls): panel #17142A, surface #211C39, line #302B4B, ink #EFEAFF, muted #A8A1C4, lavender #B7A3FF, lavenderDeep #7D65DD, mint #80E6C4. Picker primitive = wrapping "ChoiceChips"; adult gate = "AttestationRow" checkboxes.

---

## 5. ANALYSIS: mapping to AvatarForge (this repository)

Legend: HAVE = already in engine/spec; MAP = present under different names; GAP = missing, worth adding.

| 3D4ME option | AvatarForge today | status |
|---|---|---|
| sex (male/female variety) | `af_spec.sex` AF_SEX_{unspecified,male,female}; ANSUR male+female anthropometry exist | MAP/partial — path exists but UNVERIFIED end-to-end; creator cards were female-only |
| bodyBuild (5) | AF_BUILD_{average,slim,athletic,muscular,heavy,curvy} (+ muscle/body_fat) | MAP — slender≈slim, strong≈muscular, soft≈heavy/average |
| faceShape (5 named) | seven numeric face-measurement offsets, no named presets | GAP — add named shape presets that set the offsets (use 3D4ME {x,y,cheek} factors) |
| skinTone (6 named + hex) | `skin_melanin` 0-1 continuous; `skin_tone[3]` exists in struct but HAD NO control | GAP(control) — expose a 6-swatch picker + custom; reuse 3D4ME hexes as the palette |
| hairColour (6 + hex) | `hair_tone[3]` implemented (12 tiles per prior work) | HAVE/MAP — adopt the 6 curated hexes as named defaults |
| eyeColour (5 + hex) | `evaluateEye()` colours eyes procedurally; no `af_spec` eye-colour field/control surfaced | GAP — add `eye_tone[3]` (or eye preset index) to spec + wire to evaluateEye; owner-requested |
| clothingStyle (6) | AF_OUTFIT_* (incl JEANS_JACKET; a gown/dress exists) | MAP/partial — add tailored/casual/editorial/formal-dress/swimwear mapping; lingerie gated by adult rule |
| clothingMaterial (6 fabrics + PBR) | single `cloth_roughness`; no fabric presets | GAP — add fabric presets -> roughness/metalness/sheen (values above) |
| expression (5) | 52 ARKit blendshapes EXIST + apply API; no authored presets/UI | GAP(feature) — add expression presets as blendshape weight vectors (confident/serene/playful/intense/soft-smile) |
| lighting / camera / background / views (render) | renderer has environment/camera; app-layer concern | APP work, not generator — track separately |
| 14 numeric sliders | height_m, muscle, body_fat, bust, age_years + internal anthropometry | PARTIAL — finer explicit widths (shoulder/chest/waist/hip/arm/leg/jaw/eye/nose) are extension candidates |
| adultMode + attestation | Outfit.NONE adult path exists but is NOT exposed (per reuse audit) | KEEP gated; adopt the four-point attestation text when/if exposed |
| CYBORG variant | none anywhere | NEW — no organic donor provides this; derive from owner's Drive/Dropbox cyborg scrap, built on the engine's metallic GEAR region + emissive panels + plate geometry |

## 6. ANALYSIS: prioritised plan for "male, lots of females, cyborgs"

Sequenced so each step is host-verifiable like the texture-quality work already landed:

P0 — Colour identity controls (owner explicitly named eye colour):
  a) Add `eye_tone[3]` to `af_spec` and wire `evaluateEye` iris colour to it; ship the 5-eye / 6-skin / 6-hair curated hex palettes as named presets (constants) selectable from spec/prompt. Measure via the quality harness (iris channel histogram differs per preset; determinism preserved).
  b) Fabric/material presets (cotton/silk/satin/denim/leather/latex -> roughness/metalness/sheen) driven from spec; measure MR-map distribution shifts per fabric.
P1 — Variety:
  c) Named FACE-SHAPE presets (oval/heart/square/diamond/round) setting the seven offsets; assert distinct facial landmark spreads per shape.
  d) EXPRESSION presets from the existing 52 blendshapes (five weight vectors); assert non-zero morph weights applied/restored.
  e) MALE pass: exercise AF_SEX_MALE end-to-end, verify masculine landmarks/proportions differ measurably from female (bounds/landmark deltas), fix anomalies.
  f) Female VARIETY: multiple faces/bodies beyond the single stylised face (combine faceShape x build x skin/eye/hair palettes => large combinatorial cast), each deterministic per seed.
P2 — Cyborg variant (new content):
  g) Define a cybernetic look on top of the existing rig: metallic plating via the GEAR material (hammered-steel base -> brushed/anodised), emissive seam/panel lines (need an emissive term in the shader), partial-replacement regions (e.g., one cyber arm/eye), decal panel lines. Ground the aesthetic in the owner's `/avatar/cyborgs`, `/avatar/armor`, Drive `Avatar/cyborg` scrap, and the ink.glb/mylove detail targets (run `tools/glb_quality_targets_probe.py` on them; agent cannot fetch those binaries, owner runs locally). Verify via normal/emissive map statistics + CI render.

Cross-cutting honesty: the agent sandbox cannot download the owner's binary scrap (egress limited to GitHub/npm/PyPI) and cannot render pixels; structural/parameter facts are copied here verbatim, visual fidelity work needs owner-run probes/renders as evidence. No adult/unclothed exposure is added by any of the above; gating policy is unchanged.
