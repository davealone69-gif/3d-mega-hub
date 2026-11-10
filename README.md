# 3D Mega Hub

**Android-first avatar studio with an integrated AI assistant and a real 3D runtime.**

This repository is the integrated Android product host. Existing repositories remain independent donor/source projects until their code, dependencies, licences, tests, and build status have been checked. No donor is considered integrated merely because its files or documentation were copied.

## Product goals

- Render and manage customizable 3D avatars on Android.
- Integrate an AI assistant that can run on-device where a compatible model/runtime fits the target device; make network/provider backends optional rather than silently required.
- Support avatar voice, expressions, animation, and lip-sync through explicit interfaces.
- Provide avatar import/export, a persistent library, generation-job status, resource controls, and useful diagnostics.
- Prioritize the Samsung SM-S921B / Exynos 2400 target and a reliable OpenGL ES path. Vulkan remains optional until verified on the actual device.
- Ship a reproducible Android build, with an AAB as the primary release artifact and a signed APK for installation/testing. Never commit signing secrets.


## Current capability split (2026-11-10)

### GLB import and display

The user reports that the current 3D-Mega-Hub working build displays the supplied mylove.glb
correctly on the phone. Treat this as **user-confirmed for that tested fixture/build**, not as proof
that every GLB variant, renderer path or device is covered. Preserve the successful viewing path
while generation work proceeds independently.

Reference fixture facts: 9,287,272 bytes; GLB 2.0; 163,756 vertices; 144,073 triangles; one mesh
and one material; two 2048 × 2048 WebP textures; normals and UVs; two nodes; no skeleton and no
animations. It is therefore a static display benchmark, not a rigged-avatar benchmark.

### Avatar generation

Generation is a separate, **PARTIAL / needs visual-quality work** capability owned by AvatarForge's
procedural engine. Its host records demonstrate substantial geometry and rig output, but do not
establish that generated faces, anatomy, proportions, skin, hair, clothing or materials match the
reference model visually. The reference GLB's vertex count is already lower than one recorded
AvatarForge generated fixture, so polygon count alone is not the bottleneck.

The reference GLB has no skeleton or animation and the available inspection record does not provide
trustworthy anthropometric tape measurements. Do not infer chest/waist/hip/height values from polygon
counts or invent an af_spec. First extract metric bounds, then derive and validate measurable
cross-sections/landmarks. Compare silhouette, face, UVs, textures and material response side by side.

The user's comparison set is in Dropbox /Model: generated_avatar_diagnostic.png and two
Screenshot_20261110_*.jpg captures. Visual scoring and numeric geometry extraction remain pending.

**Repository boundary:** 3D-Mega-Hub owns the integrated app and GLB viewing workflow; AvatarForge
owns the standalone generator/engine. Improve generation in AvatarForge and consume its proven
outputs in the Hub. Do not duplicate the generator inside the Hub or destabilize the working GLB
viewer to chase generator quality.

## Blueprint and audit records

- [Master blueprint](docs/MASTER_BLUEPRINT.md): product architecture, renderer decision, integration phases and release gates.
- [Integration plan](docs/INTEGRATION_PLAN.md): staged work checklist.
- [Donor baseline](docs/DONOR_BASELINE.md): inspected branch commits, CI evidence, known blockers and licence questions.

## Initial donor map (candidates, not yet integrated)

| Source repository | Candidate contribution | Required before adoption |
|---|---|---|
| [AvatarForge](https://github.com/davealone69-gif/AvatarForge) | Native avatar kernel, C ABI, Kotlin SDK, rigging/blendshapes and diagnostics | Select intended branch by exact SHA; compile native and Android targets; verify renderer on device; clarify source licence |
| [3DDD](https://github.com/davealone69-gif/3DDD) | Filament viewport, glTF/GLB, materials, skinning, animation and export | Resolve current CI failures; inspect dependency/licence compatibility and regression coverage |
| [Aaa](https://github.com/davealone69-gif/Aaa) | SDK/provider boundaries, model management, jobs, resource governance and persistence patterns | Verify working implementations and API contracts |
| [Grok-Girls](https://github.com/davealone69-gif/Grok-Girls) | Renderer regression cases and optional provider/voice adapters | Compare branch commits; keep external server integration optional |
| [Aura-Studio-Avatar](https://github.com/davealone69-gif/Aura-Studio-Avatar) | Avatar library/designer and management patterns | Verify source, build, tests, and licence |

This is a starting map from prior audits, not a claim that every repository or branch has been fully re-audited. Heavy 3D generation models are not assumed to run locally on the phone; their requirements and any remote-provider boundary must be explicit.

## Engineering rules

1. Preserve all donor repositories and their branches.
2. Inspect the current default branch and recent commits before choosing source code.
3. Prefer tested, minimal integrations over wholesale merges.
4. Do not ship mock AI, fake controls, placeholder success states, or dummy services.
5. Show unavailable capabilities as unavailable, with actionable diagnostics.
6. Keep secrets out of source control. Do not weaken main history with force pushes.
7. A green CI build is not a green device build.

## Verification labels

- **REAL + VERIFIED**: implementation exists and the stated test was run successfully.
- **UNVERIFIED**: code or documentation exists, but the required test has not been run.
- **PARTIAL**: some parts work; named gaps remain.
- **MISSING**: not implemented.
- **BLOCKED**: a concrete dependency or access limitation prevents the next check.

## Current baseline

- Repository created and writable: **verified**.
- Product scope and donor shortlist: **documented; source integration not yet verified**.
- Android project compilation: **not yet verified in this repository**.
- On-device renderer, embedded LLM, voice/lip-sync: **not yet verified in this repository**.
- Release signing and installable artifacts: **not yet configured or verified**.
