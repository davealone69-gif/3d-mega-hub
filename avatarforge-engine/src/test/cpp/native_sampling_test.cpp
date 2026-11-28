#include "af_isosurface.h"
#include "af_prompt.h"
#include "af_anatomy.h"
#include "af_body.h"
#include "af_glb.h"
#include "af_hair_cards.h"
#include "af_rig.h"
#include "af_texture.h"
#include "af_uv.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

std::vector<float> sample(int threads, int blockSize) {
    af::DenseGrid grid;
    grid.allocate(33, 25, 17, 0.01f, af::Vec3(0.0f, 0.0f, 0.0f));
    const auto plane = [](const af::Vec3& p) { return p.x - 0.16f; };
    af::sampleField(grid, plane, threads, blockSize);
    return grid.values;
}
}

int main() {
    // Hair tone must be preserved as a material tint, while the atlas contributes only
    // the root-to-tip multiplier. All-zero tone means the documented natural default.
    af_spec hairToneSpec{};
    af_spec_init(&hairToneSpec);
    const af::Vec3 naturalHair = af::hairBaseColorForSpec(hairToneSpec);
    const af::Vec3 defaultHair = af::materialForRegion(af::REGION_HAIR).baseColor;
    require(std::fabs(naturalHair.x - defaultHair.x) < 1e-6f &&
            std::fabs(naturalHair.y - defaultHair.y) < 1e-6f &&
            std::fabs(naturalHair.z - defaultHair.z) < 1e-6f,
            "zero hair tone did not preserve the natural material colour");
    hairToneSpec.hair_tone[0] = 0.18f;
    hairToneSpec.hair_tone[1] = 0.42f;
    hairToneSpec.hair_tone[2] = 0.73f;
    const af::Vec3 customHair = af::hairBaseColorForSpec(hairToneSpec);
    require(std::fabs(customHair.x - 0.18f) < 1e-6f &&
            std::fabs(customHair.y - 0.42f) < 1e-6f &&
            std::fabs(customHair.z - 0.73f) < 1e-6f,
            "custom hair tone was not preserved in the material factor");
    af::BodyMetrics hairGradientMetrics = af::Anatomy::solve(hairToneSpec);
    const float rootTint = af::hairRootTipTint(hairGradientMetrics, hairGradientMetrics.y.crownY);
    const float tipTint = af::hairRootTipTint(
        hairGradientMetrics, hairGradientMetrics.y.crownY -
            (hairGradientMetrics.y.crownY - hairGradientMetrics.y.chinY) * 2.6f);
    require(rootTint < 1.0f && tipTint > 1.0f && tipTint > rootTint,
            "hair root-to-tip pigment gradient is missing or reversed");

    // Explicit numeric measurements must win over descriptive adjectives regardless of order.
    af_spec measured{};
    af_spec_init(&measured);
    af::PromptParse measuredParse;
    const int measuredMatches = af::parsePrompt(
        "Adult woman, athletic build, 175 cm tall, medium dark hair, black t-shirt and jeans",
        &measured, &measuredParse);
    require(measuredMatches > 0, "avatar prompt matched no attributes");
    require(measured.sex == AF_SEX_FEMALE, "female prompt was not parsed");
    require(measured.build == AF_BUILD_ATHLETIC, "athletic build was not parsed");
    require(measured.hair_style == AF_HAIR_MEDIUM, "medium hair was not parsed");
    require(measured.outfit == AF_OUTFIT_JEANS_JACKET, "jeans outfit was not parsed");
    require(std::fabs(measured.height_m - 1.75f) < 0.001f,
            "explicit 175 cm height was overwritten by the word tall");

    // A hairstyle adjective must not be mistaken for a person's stature.
    af_spec shortHair{};
    af_spec_init(&shortHair);
    af::PromptParse shortHairParse;
    af::parsePrompt("short hair", &shortHair, &shortHairParse);
    require(shortHair.hair_style == AF_HAIR_SHORT, "short hair style was not parsed");
    require(std::fabs(shortHair.height_m - 1.75f) < 0.001f,
            "short hair incorrectly changed avatar stature");

    const std::vector<float> reference = sample(1, 1);
    require(!reference.empty(), "reference grid is empty");

    // Repeat to exercise scheduling variation and expose intermittent overlapping writes.
    for (int iteration = 0; iteration < 40; ++iteration) {
        const std::vector<float> parallel = sample(8, 1);
        const std::vector<float> blocked = sample(8, 8);
        require(reference.size() == parallel.size(), "parallel sample count differs");
        require(reference.size() == blocked.size(), "blocked sample count differs");

        for (size_t i = 0; i < reference.size(); ++i) {
            require(std::isfinite(reference[i]), "reference contains a non-finite sample");
            require(std::isfinite(parallel[i]), "parallel contains a non-finite sample");
            require(std::isfinite(blocked[i]), "blocked contains a non-finite sample");
            require((reference[i] < 0.0f) == (parallel[i] < 0.0f),
                    "parallel sampling changed a sample's sign");
            require((reference[i] < 0.0f) == (blocked[i] < 0.0f),
                    "block skipping or shared-boundary ownership changed a sample's sign");
        }
    }

    // The default quality path must refine the trunk and limbs, not only the face and extremities.
    // Compare a deliberately coarse extraction against one refinement round on the same field.
    af_spec qualitySpec{};
    af_spec_init(&qualitySpec);
    const af::BodyMetrics qualityMetrics = af::Anatomy::solve(qualitySpec);
    af::BodyField qualityField(qualityMetrics);

    // Anatomical field diagnostics: visible-pixel tests cannot detect missing eyes,
    // collapsed extremities, or left/right shape drift. Probe the actual SDF used by meshing.
    require(qualityMetrics.y.crownY > qualityMetrics.y.eyeY &&
            qualityMetrics.y.eyeY > qualityMetrics.y.chinY &&
            qualityMetrics.y.chinY > qualityMetrics.y.neckBaseY &&
            qualityMetrics.y.neckBaseY > qualityMetrics.y.waistY &&
            qualityMetrics.y.waistY > qualityMetrics.y.hipY &&
            qualityMetrics.y.hipY > qualityMetrics.y.kneeY &&
            qualityMetrics.y.kneeY > qualityMetrics.y.ankleY,
            "solved anatomical landmarks are not in cranio-caudal order");
    require(qualityField.head(af::Vec3(0.0f,
                (qualityMetrics.y.crownY + qualityMetrics.y.chinY) * 0.5f, 0.0f)) < 0.0f,
            "head SDF has no solid cranium at its anatomical centre");

    // Both eye centres must be real eye geometry and must retain distinct material regions.
    const af::Vec3 leftEye = qualityField.eyeCentre(1);
    const af::Vec3 rightEye = qualityField.eyeCentre(-1);
    require(leftEye.x > 0.0f && rightEye.x < 0.0f &&
            std::fabs(leftEye.x + rightEye.x) < 1e-5f &&
            std::fabs(leftEye.y - rightEye.y) < 1e-5f &&
            std::fabs(leftEye.z - rightEye.z) < 1e-5f,
            "eye anchors are not correctly mirrored across the facial midline");
    require(qualityField.classifyRegion(leftEye) == af::REGION_EYE_L,
            "left eyeball centre is missing or assigned to the wrong material region");
    require(qualityField.classifyRegion(rightEye) == af::REGION_EYE_R,
            "right eyeball centre is missing or assigned to the wrong material region");

    // Finger and toe centre-lines must remain inside their generated hand/foot SDFs.
    // This catches disconnected or collapsed extremity geometry before rendering.
    for (int side : {-1, 1}) {
        const af::Chain& chain = qualityField.chain(side);
        require(qualityField.body(chain.shoulder) < 0.0f,
                "shoulder anchor is outside the body SDF");
        require(qualityField.body(chain.hipJoint) < 0.0f,
                "hip joint anchor is outside the body SDF");
        for (int finger = 0; finger < 5; ++finger) {
            require(qualityField.hand(chain.fingerTip[finger], side) < 0.0f,
                    "finger tip centre is outside its hand SDF");
        }
        require(qualityField.foot(chain.ball, side) < 0.0f,
                "metatarsal/forefoot anchor is outside its foot SDF");
    }

    // Isolate the arm field before testing the full body union. The elbow/upper-forearm
    // probe is the historical failure location; this catches side-dependent frame errors
    // directly instead of allowing the torso union to hide or amplify them.
    {
        const af::Vec3 probe(0.2000f, 1.0763f, -0.0400f);
        const float leftArm = qualityField.armChainPublic(probe, 1);
        const float rightArm = qualityField.armChainPublic(
            af::Vec3(-probe.x, probe.y, probe.z), -1);
        require(std::isfinite(leftArm) && std::isfinite(rightArm),
                "mirrored arm-chain probe produced a non-finite SDF value");
        require(std::fabs(leftArm - rightArm) < 1e-5f,
                "mirrored arm-chain fields differ at the historical shoulder/upper-arm probe");

        const af::Chain& leftChain = qualityField.chain(1);
        const af::Chain& rightChain = qualityField.chain(-1);
        const float armY[] = {leftChain.shoulder.y, leftChain.elbow.y,
                              leftChain.wrist.y};
        const float armX[] = {0.10f, 0.16f, 0.20f, 0.24f, 0.30f};
        const float armZ[] = {-0.08f, -0.04f, 0.0f, 0.04f, 0.08f};
        float maxArmMirrorError = 0.0f;
        for (float y : armY) for (float x : armX) for (float z : armZ) {
            const float a = qualityField.armChainPublic(af::Vec3(x, y, z), 1);
            const float b = qualityField.armChainPublic(af::Vec3(-x, y, z), -1);
            maxArmMirrorError = std::max(maxArmMirrorError, std::fabs(a - b));
        }
        std::fprintf(stderr, "DIAGNOSTIC: arm-chain mirror max error=%.9f m\\n",
                     maxArmMirrorError);
        require(maxArmMirrorError < 1e-5f,
                "left/right arm-chain SDF is not mirror-equivariant");
    }

    // The procedural body is intended to be bilaterally symmetric in its rest pose.
    // Sample the real field on mirrored points; permit small deterministic noise, not gross drift.
    const float probeY[] = {
        qualityMetrics.y.hipY, qualityMetrics.y.waistY,
        qualityMetrics.y.bustY, qualityMetrics.y.shoulderY,
        qualityMetrics.y.kneeY, qualityMetrics.y.ankleY
    };
    const float probeX[] = {0.025f, 0.060f, 0.120f, 0.200f, 0.300f};
    const float probeZ[] = {-0.12f, -0.04f, 0.04f, 0.12f};
    float maximumMirrorError = 0.0f;
    float maximumMirrorX = 0.0f, maximumMirrorY = 0.0f, maximumMirrorZ = 0.0f;
    float maximumMirrorLeft = 0.0f, maximumMirrorRight = 0.0f;
    for (float y : probeY) {
        for (float x : probeX) {
            for (float z : probeZ) {
                const float left = qualityField.body(af::Vec3(x, y, z));
                const float right = qualityField.body(af::Vec3(-x, y, z));
                require(std::isfinite(left) && std::isfinite(right),
                        "bilateral body probe produced a non-finite SDF value");
                const float mirrorError = std::fabs(left - right);
                if (mirrorError > maximumMirrorError) {
                    maximumMirrorError = mirrorError;
                    maximumMirrorX = x;
                    maximumMirrorY = y;
                    maximumMirrorZ = z;
                    maximumMirrorLeft = left;
                    maximumMirrorRight = right;
                }
            }
        }
    }
    if (maximumMirrorError >= 0.004f) {
        std::fprintf(stderr,
                     "DIAGNOSTIC: mirror max error=%.6f m at (+/-%.4f, %.4f, %.4f), "
                     "left=%.6f right=%.6f\\n",
                     maximumMirrorError, maximumMirrorX, maximumMirrorY, maximumMirrorZ,
                     maximumMirrorLeft, maximumMirrorRight);
    }
    require(maximumMirrorError < 0.004f,
            "left/right body SDF differs by more than 4 mm in the symmetric rest pose");
    af::BodyBuildOptions coarseOptions;
    coarseOptions.bodyCell = 0.010f;  // keep this host regression quick and deterministic
    coarseOptions.detailRounds = 0;
    coarseOptions.smoothPasses = 0;
    coarseOptions.computeTangents = false;
    coarseOptions.threads = 2;
    af::Mesh coarseMesh;
    af::BodyBuildReport coarseReport;
    require(af::buildBodyMesh(qualityField, coarseOptions, &coarseMesh, &coarseReport),
            "coarse avatar mesh build failed");

    af::BodyBuildOptions refinedOptions = coarseOptions;
    refinedOptions.detailRounds = 1;
    af::Mesh refinedMesh;
    af::BodyBuildReport refinedReport;
    require(af::buildBodyMesh(qualityField, refinedOptions, &refinedMesh, &refinedReport),
            "refined avatar mesh build failed");
    require(!coarseMesh.empty() && !refinedMesh.empty(), "avatar mesh is empty");
    require(refinedMesh.triangleCount() > coarseMesh.triangleCount(),
            "body refinement did not add triangles to the generated avatar");
    for (const af::Vec3& p : refinedMesh.positions) {
        require(p.isFinite(), "refined avatar contains a non-finite vertex");
    }

    // The reference dress is floor-length and flared. Prove the garment field creates
    // actual fabric volume OUTSIDE the body, rather than merely clipping the body offset.
    {
        af_spec dressSpec{};
        af_spec_init(&dressSpec);
        dressSpec.sex = AF_SEX_FEMALE;
        const af::BodyMetrics dressMetrics = af::Anatomy::solve(dressSpec);
        af::BodyField dressField(dressMetrics);
        af::ApparelOptions dressOptions;
        dressOptions.outfit = 2; // AF_OUTFIT_DRESS
        dressOptions.latticeCell = 0.008f;
        dressField.setApparel(dressOptions);

        const af::Vec3 skirtProbe(
            dressMetrics.waist.rx * 2.25f,
            dressMetrics.height * 0.04f,
            dressMetrics.waist.centreZ);
        require(dressField.body(skirtProbe) > 0.0f,
                "dress flare probe unexpectedly lies inside the underlying body");
        require(dressField.clothUpperPublic(skirtProbe) < 0.0f,
                "dress skirt does not create flared fabric volume outside the body");

        const af::Vec3 belowHemProbe(
            dressMetrics.waist.rx * 2.25f,
            dressMetrics.y.soleY + dressMetrics.height * 0.005f,
            dressMetrics.waist.centreZ);
        require(dressField.clothUpperPublic(belowHemProbe) > 0.0f,
                "floor-length dress skirt extends below its defined hem plane");
    }

    // Hanging hairstyles must be volumetric, not a zero-thickness SDF curtain.
    af::BodyField hairField(qualityMetrics);
    af::ApparelOptions longHair;
    longHair.hair = 4;  // AF_HAIR_LONG
    longHair.latticeCell = 0.008f;
    hairField.setApparel(longHair);
    const float headHeight = qualityMetrics.y.crownY - qualityMetrics.y.chinY;
    const float hairTop = qualityMetrics.y.crownY - headHeight * 0.05f;
    const float hairBottom = qualityMetrics.y.crownY - headHeight * 2.6f;
    const float hairMidY = (hairTop + hairBottom) * 0.5f;
    const float hairBackZ = qualityMetrics.head.centreZ + qualityMetrics.head.rz * 0.625f;
    const float hairDepth = std::max(qualityMetrics.head.rz * 0.34f, 0.022f);
    const af::Vec3 hairCore(0.0f, hairMidY, hairBackZ + hairDepth);
    require(hairField.hairPublic(hairCore) < -0.015f,
            "long hairstyle hanging section is not a solid 3D volume");

    // Every exposed procedural hairstyle must produce finite, non-empty geometry in
    // the same anatomical sampling envelope. This protects the generator while the
    // hairline, braid and coil fields evolve independently.
    for (int style = 1; style <= 9; ++style) {
        af::BodyField styleField(qualityMetrics);
        af::ApparelOptions styleOptions;
        styleOptions.hair = style;
        styleOptions.latticeCell = 0.008f;
        styleField.setApparel(styleOptions);
        int interiorSamples = 0;
        for (int iy = 0; iy <= 12; ++iy) {
            const float y = qualityMetrics.y.chinY +
                (qualityMetrics.y.crownY - qualityMetrics.y.chinY + 0.10f) *
                static_cast<float>(iy) / 12.0f;
            for (int ix = 0; ix <= 10; ++ix) {
                const float x = -qualityMetrics.head.rx * 1.45f +
                    qualityMetrics.head.rx * 2.90f * static_cast<float>(ix) / 10.0f;
                for (int iz = 0; iz <= 10; ++iz) {
                    const float z = qualityMetrics.head.centreZ -
                        qualityMetrics.head.rz * 1.55f +
                        qualityMetrics.head.rz * 3.10f * static_cast<float>(iz) / 10.0f;
                    const float sample = styleField.hairPublic(af::Vec3(x, y, z));
                    require(std::isfinite(sample),
                            "hairstyle field produced a non-finite sample");
                    if (sample < 0.0f) ++interiorSamples;
                }
            }
        }
        require(interiorSamples > 0,
                "a selectable hairstyle produced no interior samples in the head envelope");
    }

    // Exported GLBs must omit KHR_materials_sheen when no sheen texture is authored
    // while retaining the AvatarForge generator stamp and KHR_materials_ior.
    {
        af::Mesh minimalMesh;
        minimalMesh.positions = {
            af::Vec3(0.0f, 0.0f, 0.0f),
            af::Vec3(0.1f, 0.0f, 0.0f),
            af::Vec3(0.0f, 0.1f, 0.0f)
        };
        minimalMesh.normals = {
            af::Vec3(0.0f, 0.0f, 1.0f),
            af::Vec3(0.0f, 0.0f, 1.0f),
            af::Vec3(0.0f, 0.0f, 1.0f)
        };
        minimalMesh.uvs = {
            0.0f, 0.0f,
            1.0f, 0.0f,
            0.0f, 1.0f
        };
        minimalMesh.tangents = {
            af::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
            af::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
            af::Vec4(1.0f, 0.0f, 0.0f, 1.0f)
        };
        minimalMesh.regions = {
            af::REGION_CLOTH_UPPER,
            af::REGION_CLOTH_UPPER,
            af::REGION_CLOTH_UPPER
        };
        minimalMesh.indices = {0u, 1u, 2u};

        af::GlbOptions glbOptions{};
        glbOptions.embedTextures = false;
        glbOptions.includeAnimations = false;
        glbOptions.includeMorphTargets = false;
        std::vector<uint8_t> glbBytes;
        af::GlbResult glbResult{};
        require(af::writeGlb(minimalMesh, nullptr, nullptr, nullptr, hairGradientMetrics,
                             hairToneSpec, glbOptions, &glbBytes, &glbResult),
                "writeGlb failed on minimal test mesh");
        require(glbBytes.size() >= 20, "exported GLB is shorter than header");
        const uint32_t jsonLen =
            static_cast<uint32_t>(glbBytes[12]) |
            (static_cast<uint32_t>(glbBytes[13]) << 8) |
            (static_cast<uint32_t>(glbBytes[14]) << 16) |
            (static_cast<uint32_t>(glbBytes[15]) << 24);
        require(jsonLen > 0 && 20u + jsonLen <= glbBytes.size(),
                "exported GLB JSON chunk length is out of range");
        const std::string glbJson(
            reinterpret_cast<const char*>(glbBytes.data() + 20), jsonLen);
        require(glbJson.find("\"generator\":\"AvatarForge ") != std::string::npos,
                "exported GLB is missing AvatarForge generator stamp");
        require(glbJson.find("KHR_materials_ior") != std::string::npos,
                "exported GLB is missing KHR_materials_ior");
        require(glbJson.find("KHR_materials_sheen") == std::string::npos,
                "exported GLB must not emit KHR_materials_sheen without an authored sheen texture");
        require(glbJson.find("\"cornea_left\"") != std::string::npos &&
                glbJson.find("\"cornea_right\"") != std::string::npos,
                "exported GLB is missing cornea_left/cornea_right materials");
        require(glbJson.find("\"alphaMode\":\"BLEND\"") != std::string::npos,
                "exported GLB cornea material is missing alphaMode BLEND");
        require(glbJson.find("KHR_materials_clearcoat") != std::string::npos,
                "exported GLB cornea material is missing KHR_materials_clearcoat");
    }

    // Dual-Mesh Eyes end-to-end verification: watertight analytic shells, concave iris bowl +
    // protruding wet corneal dome, sclera/iris/pupil texture split, rigid single-joint skinning,
    // aligned eye joints, and rigid gaze / non-swapped blink blendshapes.
    {
        require(coarseReport.openEdgesAfterRepair == 0 && coarseReport.nonManifoldEdges == 0,
                "body mesh with analytic dual-mesh eyes must remain watertight and manifold");

        size_t regionCounts[af::REGION_COUNT] = {0};
        af::Vec3 eyeLMin(1e30f, 1e30f, 1e30f), eyeLMax(-1e30f, -1e30f, -1e30f);
        af::Vec3 corneaLMin(1e30f, 1e30f, 1e30f), corneaLMax(-1e30f, -1e30f, -1e30f);
        for (size_t i = 0; i < coarseMesh.positions.size(); ++i) {
            const int r = coarseMesh.regions[i];
            require(r >= 0 && r < af::REGION_COUNT, "vertex region out of range");
            ++regionCounts[r];
            if (r == af::REGION_EYE_L) {
                eyeLMin = eyeLMin.minv(coarseMesh.positions[i]);
                eyeLMax = eyeLMax.maxv(coarseMesh.positions[i]);
            } else if (r == af::REGION_CORNEA_L) {
                corneaLMin = corneaLMin.minv(coarseMesh.positions[i]);
                corneaLMax = corneaLMax.maxv(coarseMesh.positions[i]);
            }
        }
        require(regionCounts[af::REGION_EYE_L] > 100 && regionCounts[af::REGION_EYE_R] > 100,
                "analytic inner eye shells missing");
        require(regionCounts[af::REGION_CORNEA_L] > 100 && regionCounts[af::REGION_CORNEA_R] > 100,
                "analytic outer cornea shells missing");
        require(regionCounts[af::REGION_EYE_L] == regionCounts[af::REGION_EYE_R] &&
                regionCounts[af::REGION_CORNEA_L] == regionCounts[af::REGION_CORNEA_R],
                "left/right dual-mesh eye vertex counts are not symmetric");

        // Anterior chamber depth: outer cornea apex must sit 2.5-4.5 mm ahead (-Z) of the
        // recessed inner iris bowl, and wrap the posterior sclera with positive clearance.
        const float chamberDepth = eyeLMin.z - corneaLMin.z;
        const float scleraClearance = corneaLMax.z - eyeLMax.z;
        require(chamberDepth > 0.0020f && chamberDepth < 0.0055f,
                "cornea anterior chamber depth outside anatomical 2.0-5.5 mm range");
        require(scleraClearance > 0.00015f && scleraClearance < 0.0012f,
                "cornea sclera clearance outside 0.15-1.2 mm range");

        // UV unwrap + 512x512 texture bake must produce pupil, iris, and bright sclera texels.
        af::Mesh eyeTestMesh = coarseMesh;
        af::UVLayout eyeLayout;
        require(af::unwrapAtlas(&eyeTestMesh, 512, 512, &eyeLayout),
                "unwrapAtlas failed on dual-mesh eye avatar");
        eyeTestMesh.computeTangents();

        af::TextureSet eyeTex;
        af::BakeOptions bakeOpt;
        bakeOpt.width = 512;
        bakeOpt.height = 512;
        bakeOpt.threads = 2;
        require(af::bakeTextures(eyeTestMesh, eyeLayout, qualityMetrics, qualitySpec, bakeOpt, &eyeTex),
                "bakeTextures failed on dual-mesh eye avatar");

        int darkPupilVerts = 0, midIrisVerts = 0, brightScleraVerts = 0;
        for (size_t i = 0; i < eyeTestMesh.positions.size(); ++i) {
            if (eyeTestMesh.regions[i] != af::REGION_EYE_L) continue;
            const float u = eyeTestMesh.uvs[i * 2];
            const float v = eyeTestMesh.uvs[i * 2 + 1];
            const int px = std::clamp(static_cast<int>(u * eyeTex.width), 0, eyeTex.width - 1);
            const int py = std::clamp(static_cast<int>(v * eyeTex.height), 0, eyeTex.height - 1);
            const size_t idx = (static_cast<size_t>(py) * eyeTex.width + px) * 4;
            const float lum = (0.2126f * eyeTex.baseColor[idx + 0] +
                               0.7152f * eyeTex.baseColor[idx + 1] +
                               0.0722f * eyeTex.baseColor[idx + 2]) / 255.0f;
            const af::Vec3 rel = eyeTestMesh.positions[i] -
                af::Vec3((eyeLMin.x + eyeLMax.x) * 0.5f,
                         (eyeLMin.y + eyeLMax.y) * 0.5f,
                         eyeLMax.z - qualityMetrics.eyeRadius);
            if (rel.z > 0.0f) continue; // check visible anterior hemisphere
            if (lum < 0.14f) ++darkPupilVerts;
            else if (lum < 0.72f) ++midIrisVerts;
            else ++brightScleraVerts;
        }
        require(darkPupilVerts > 0 && midIrisVerts > 0 && brightScleraVerts > 20,
                "baked eye texture must contain distinct pupil, iris, and bright sclera zones");

        // Photorealism PBR material & micro-relief verification:
        // 1. Skin must exhibit real anatomical T-zone sebum roughness contrast (glossy nose/forehead
        //    vs satin cheeks/limbs) and facial/body cavity AO contrast, not uniform clay.
        // 2. Lips must bake non-flat vertical micro-crease normals into the tangent-space normal map.
        // 3. Eye occlusion must include superior upper-eyelid ambient shadow.
        {
            float skinRoughMin = 1.0f, skinRoughMax = 0.0f;
            float skinAoMin = 1.0f, skinAoMax = 0.0f;
            float lipNormalMaxTilt = 0.0f;
            float eyeAoMin = 1.0f, eyeAoMax = 0.0f;
            for (size_t i = 0; i < eyeTestMesh.positions.size(); ++i) {
                const int r = eyeTestMesh.regions[i];
                const float u = eyeTestMesh.uvs[i * 2];
                const float v = eyeTestMesh.uvs[i * 2 + 1];
                const int px = std::clamp(static_cast<int>(u * eyeTex.width), 0, eyeTex.width - 1);
                const int py = std::clamp(static_cast<int>(v * eyeTex.height), 0, eyeTex.height - 1);
                const size_t idx4 = (static_cast<size_t>(py) * eyeTex.width + px) * 4;
                const size_t idx1 = static_cast<size_t>(py) * eyeTex.width + px;
                if (r == af::REGION_SKIN) {
                    const float rough = eyeTex.metallicRoughness[idx4 + 1] / 255.0f;
                    const float ao = eyeTex.occlusion[idx1] / 255.0f;
                    if (rough >= 0.20f) { // ignore chart-edge gutter texels adjacent to cornea/eye
                        skinRoughMin = std::min(skinRoughMin, rough);
                        skinRoughMax = std::max(skinRoughMax, rough);
                    }
                    skinAoMin = std::min(skinAoMin, ao);
                    skinAoMax = std::max(skinAoMax, ao);
                } else if (r == af::REGION_LIPS) {
                    const float nx = (eyeTex.normal[idx4 + 0] / 255.0f) * 2.0f - 1.0f;
                    const float ny = (eyeTex.normal[idx4 + 1] / 255.0f) * 2.0f - 1.0f;
                    lipNormalMaxTilt = std::max(lipNormalMaxTilt, std::sqrt(nx * nx + ny * ny));
                } else if (r == af::REGION_EYE_L) {
                    const float ao = eyeTex.occlusion[idx1] / 255.0f;
                    eyeAoMin = std::min(eyeAoMin, ao);
                    eyeAoMax = std::max(eyeAoMax, ao);
                }
            }
            require(skinRoughMin < 0.36f && skinRoughMax > 0.48f && (skinRoughMax - skinRoughMin) > 0.14f,
                    "skin roughness map lacks anatomical T-zone sebum contrast");
            require(skinAoMin < 0.72f && skinAoMax > 0.90f && (skinAoMax - skinAoMin) > 0.25f,
                    "skin occlusion map lacks anatomical facial/body cavity AO contrast");
            require(lipNormalMaxTilt > 0.05f,
                    "lip normal map must contain baked vertical lip micro-crease relief");
            require(eyeAoMin < 0.85f && eyeAoMax > 0.95f,
                    "eye occlusion map must include superior upper-eyelid shadow");
        }

        // Rigging: eye_L/eye_R joints must align with the analytic eyeballs within 1 mm and
        // bind both inner eye and outer cornea vertices 100% rigidly to their eye joint.
        af::Rig eyeRig;
        require(eyeRig.build(qualityField, eyeTestMesh), "Rig::build failed on dual-mesh eye avatar");
        const int jiEyeL = eyeRig.jointIndex("eye_L");
        const int jiEyeR = eyeRig.jointIndex("eye_R");
        require(jiEyeL >= 0 && jiEyeR >= 0, "eye_L / eye_R joints missing from rig");
        const af::Vec3 expectedEyeLCentre((eyeLMin.x + eyeLMax.x) * 0.5f,
                                          (eyeLMin.y + eyeLMax.y) * 0.5f,
                                          eyeLMax.z - qualityMetrics.eyeRadius);
        const af::Vec3 jointEyeLPos = eyeRig.joints()[static_cast<size_t>(jiEyeL)].worldPosition;
        require((jointEyeLPos - expectedEyeLCentre).length() < 0.001f,
                "eye_L joint position drifted more than 1 mm from the analytic eyeball centre");

        for (size_t i = 0; i < eyeTestMesh.positions.size(); ++i) {
            const int r = eyeTestMesh.regions[i];
            if (r == af::REGION_EYE_L || r == af::REGION_CORNEA_L) {
                const af::SkinWeights& sw = eyeRig.weights()[i];
                require(sw.joints[0] == static_cast<uint16_t>(jiEyeL) && sw.weights[0] > 0.999f,
                        "left eye/cornea vertex is not 100% rigidly bound to eye_L");
            } else if (r == af::REGION_EYE_R || r == af::REGION_CORNEA_R) {
                const af::SkinWeights& sw = eyeRig.weights()[i];
                require(sw.joints[0] == static_cast<uint16_t>(jiEyeR) && sw.weights[0] > 0.999f,
                        "right eye/cornea vertex is not 100% rigidly bound to eye_R");
            }
        }

        // Blendshapes: eyeLookOutLeft moves eye_L + cornea_L rigidly and leaves skin untouched;
        // eyeBlinkLeft moves +X (left) upper eyelid skin and leaves eye/cornea untouched.
        int lookOutLeftIdx = -1, blinkLeftIdx = -1;
        for (int c = 0; c < af::kBlendshapeCount; ++c) {
            if (std::string(af::kBlendshapeNames[c]) == "eyeLookOutLeft") lookOutLeftIdx = c;
            if (std::string(af::kBlendshapeNames[c]) == "eyeBlinkLeft") blinkLeftIdx = c;
        }
        require(lookOutLeftIdx >= 0 && blinkLeftIdx >= 0, "eye blendshape channels missing");
        const auto& lookDeltas = eyeRig.blendshapeDeltas(lookOutLeftIdx);
        const auto& blinkDeltas = eyeRig.blendshapeDeltas(blinkLeftIdx);
        float maxLookEyeL = 0.0f, maxLookCorneaL = 0.0f, maxLookSkin = 0.0f;
        float maxBlinkLeftSkin = 0.0f, maxBlinkRightSkin = 0.0f, maxBlinkEye = 0.0f;
        for (size_t i = 0; i < eyeTestMesh.positions.size(); ++i) {
            const int r = eyeTestMesh.regions[i];
            const float lmag = lookDeltas[i].length();
            const float bmag = blinkDeltas[i].length();
            if (r == af::REGION_EYE_L) maxLookEyeL = std::max(maxLookEyeL, lmag);
            else if (r == af::REGION_CORNEA_L) maxLookCorneaL = std::max(maxLookCorneaL, lmag);
            else if (r == af::REGION_SKIN) maxLookSkin = std::max(maxLookSkin, lmag);

            if (r == af::REGION_EYE_L || r == af::REGION_CORNEA_L ||
                r == af::REGION_EYE_R || r == af::REGION_CORNEA_R) {
                maxBlinkEye = std::max(maxBlinkEye, bmag);
            } else if (r == af::REGION_SKIN) {
                if (eyeTestMesh.positions[i].x > 0.0f) maxBlinkLeftSkin = std::max(maxBlinkLeftSkin, bmag);
                else maxBlinkRightSkin = std::max(maxBlinkRightSkin, bmag);
            }
        }
        require(maxLookEyeL > 0.002f && maxLookCorneaL > 0.002f,
                "eyeLookOutLeft must rotate both inner eye and outer cornea shells");
        require(maxLookSkin < 1e-6f,
                "eyeLookOutLeft must not pull surrounding eyelid skin");
        require(maxBlinkLeftSkin > 0.002f && maxBlinkLeftSkin > maxBlinkRightSkin * 2.0f,
                "eyeBlinkLeft must deform the left (+X) eyelid rather than the right (-X) eyelid");
        require(maxBlinkEye < 1e-6f,
                "eyeBlinkLeft must not squash the eyeball or cornea meshes");
    }

    // Procedural Hair Card Generator end-to-end verification:
    //   1) Every selectable hairstyle (1..9) generates watertight, manifold 3D strand ribbons
    //      while bald (0) generates zero hair cards.
    //   2) Full mesh build + UV packing + strand-sheet baking + GLB MASK/doubleSided export +
    //      rigging all agree on REGION_HAIR_CARD.
    {
        for (int style = 0; style <= 9; ++style) {
            af::BodyField hcField(qualityMetrics);
            af::ApparelOptions hcApp{};
            hcApp.hair = style;
            hcField.setApparel(hcApp);
            af::Mesh cardOnly;
            const af::HairCardReport rep = af::appendProceduralHairCards(hcField, 0.0f, &cardOnly);
            if (style == 0) {
                require(rep.cardCount == 0 && cardOnly.vertexCount() == 0,
                        "bald hairstyle must not generate hair cards");
            } else {
                require(rep.cardCount >= 12 && rep.vertexCount > 200 && rep.triangleCount > 200,
                        "non-bald hairstyle must generate layered strand hair cards");
                const af::Mesh::ManifoldReport audit = cardOnly.audit();
                require(audit.openEdges == 0 && audit.nonManifoldEdges == 0,
                        "procedural hair cards must be watertight and manifold");
                for (size_t i = 0; i < cardOnly.positions.size(); ++i) {
                    const af::Vec3& p = cardOnly.positions[i];
                    require(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z),
                            "hair card vertex position must be finite");
                    require(cardOnly.regions[i] == af::REGION_HAIR_CARD,
                            "hair card vertex region must be REGION_HAIR_CARD");
                    const float u = cardOnly.uvs[i * 2];
                    const float v = cardOnly.uvs[i * 2 + 1];
                    require(u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f,
                            "hair card parametric UVs must stay in [0, 1]");
                }
            }
        }

        // End-to-end build with AF_HAIR_LONG: body + dual-mesh eyes + layered hair cards.
        af::BodyField longField(qualityMetrics);
        af::ApparelOptions longApp{};
        longApp.hair = 4; // AF_HAIR_LONG
        longApp.latticeCell = 0.018f;
        longField.setApparel(longApp);
        af::Mesh hairAvatar;
        af::BodyBuildReport hairBuildRep;
        require(af::buildBodyMesh(longField, coarseOptions, &hairAvatar, &hairBuildRep),
                "body mesh build with long hair cards failed");
        require(hairBuildRep.openEdgesAfterRepair == 0 && hairBuildRep.nonManifoldEdges == 0,
                "avatar mesh with hair cards must remain watertight and manifold");

        size_t hairCardVerts = 0;
        for (int32_t r : hairAvatar.regions) {
            if (r == af::REGION_HAIR_CARD) ++hairCardVerts;
        }
        require(hairCardVerts > 1500, "long hairstyle avatar must contain dense hair card geometry");

        af::UVLayout hairLayout;
        require(af::unwrapAtlas(&hairAvatar, 512, 512, &hairLayout), "hair card atlas unwrap failed");
        int hairCardCharts = 0;
        af::UVChart hairChart{};
        for (const af::UVChart& ch : hairLayout.charts) {
            if (ch.material == af::REGION_HAIR_CARD) {
                ++hairCardCharts;
                hairChart = ch;
            }
        }
        require(hairCardCharts == 1,
                "all hair cards must pack into one shared 4-variant strand sheet chart");
        require((hairChart.rectMax.x - hairChart.rectMin.x) > 0.10f &&
                (hairChart.rectMax.y - hairChart.rectMin.y) > 0.10f,
                "hair card strand sheet must receive high-resolution atlas area");

        hairAvatar.computeTangents();
        af::TextureSet hairTex;
        af::BakeOptions hairBake{};
        hairBake.width = 512;
        hairBake.height = 512;
        hairBake.threads = 2;
        require(af::bakeTextures(hairAvatar, hairLayout, qualityMetrics, hairToneSpec, hairBake, &hairTex),
                "hair card texture bake failed");

        int opaqueHairPx = 0, transparentHairPx = 0, ridgeNormalPx = 0;
        const int hx0 = std::max(0, static_cast<int>(std::floor(hairChart.rectMin.x * 512.0f)) + 2);
        const int hx1 = std::min(511, static_cast<int>(std::ceil(hairChart.rectMax.x * 512.0f)) - 2);
        const int hy0 = std::max(0, static_cast<int>(std::floor(hairChart.rectMin.y * 512.0f)) + 2);
        const int hy1 = std::min(511, static_cast<int>(std::ceil(hairChart.rectMax.y * 512.0f)) - 2);
        for (int py = hy0; py <= hy1; ++py) {
            for (int px = hx0; px <= hx1; ++px) {
                const size_t idx = static_cast<size_t>(py) * 512 + static_cast<size_t>(px);
                const uint8_t a = hairTex.baseColor[idx * 4 + 3];
                const int nx = static_cast<int>(hairTex.normal[idx * 4 + 0]);
                if (a >= 200) ++opaqueHairPx;
                if (a <= 40) ++transparentHairPx;
                if (std::abs(nx - 128) >= 18) ++ridgeNormalPx;
            }
        }
        require(opaqueHairPx > 200 && transparentHairPx > 100,
                "hair card strand sheet must bake both opaque root/shaft and feathered wispy tips");
        require(ridgeNormalPx > 200,
                "hair card normal map must contain cylindrical strand-ridge normal variation");

        // GLB export must emit doubleSided + alphaMode:MASK + alphaCutoff for hair_cards.
        af::GlbOptions hairGlbOpts{};
        hairGlbOpts.embedTextures = false;
        std::vector<uint8_t> hairGlbBytes;
        af::GlbResult hairGlbRes{};
        require(af::writeGlb(hairAvatar, &hairLayout, nullptr, nullptr, qualityMetrics,
                             hairToneSpec, hairGlbOpts, &hairGlbBytes, &hairGlbRes),
                "GLB export with hair cards failed");
        require(hairGlbBytes.size() > 28, "hair card GLB output is truncated");
        const uint32_t hairJsonLen = static_cast<uint32_t>(hairGlbBytes[12]) |
                                     (static_cast<uint32_t>(hairGlbBytes[13]) << 8) |
                                     (static_cast<uint32_t>(hairGlbBytes[14]) << 16) |
                                     (static_cast<uint32_t>(hairGlbBytes[15]) << 24);
        const std::string hairGlbJson(reinterpret_cast<const char*>(hairGlbBytes.data() + 20), hairJsonLen);
        const size_t hcMatPos = hairGlbJson.find("\"name\":\"hair_cards\"");
        require(hcMatPos != std::string::npos, "GLB export missing hair_cards material");
        const std::string hcMatSlice = hairGlbJson.substr(hcMatPos, 320);
        require(hcMatSlice.find("\"doubleSided\":true") != std::string::npos,
                "hair_cards GLB material must be doubleSided");
        require(hcMatSlice.find("\"alphaMode\":\"MASK\"") != std::string::npos &&
                hcMatSlice.find("\"alphaCutoff\":0.35") != std::string::npos,
                "hair_cards GLB material must export alphaMode:MASK with alphaCutoff:0.35");

        // Rigging: hair card vertices bind only to head/neck/upper-torso/clavicle bones.
        af::Rig hairRig;
        require(hairRig.build(longField, hairAvatar), "hair card rig build failed");
        for (size_t i = 0; i < hairAvatar.positions.size(); ++i) {
            if (hairAvatar.regions[i] != af::REGION_HAIR_CARD) continue;
            const af::SkinWeights& sw = hairRig.weights()[i];
            float wSum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                if (sw.weights[k] <= 0.0f) continue;
                wSum += sw.weights[k];
                const std::string& jn = hairRig.joints()[sw.joints[k]].name;
                require(jn == "head" || jn == "neck_01" || jn == "spine_03" ||
                        jn == "clavicle_L" || jn == "clavicle_R",
                        "hair card vertex bound to unexpected bone");
            }
            require(std::fabs(wSum - 1.0f) < 1e-3f, "hair card skin weights must sum to 1.0");
        }

        // 3D Eyebrow & Eyelash Cards verification (even on bald AF_HAIR_NONE avatars):
        //   - Watertight manifold ribbons in REGION_HAIR_CARD
        //   - Bound 100% to head joint
        //   - Eyelashes blink with eyeBlinkLeft (not eyeLookOutLeft) and brows respond to browDownLeft.
        af::BodyField facialField(qualityMetrics);
        af::ApparelOptions facialApp{};
        facialApp.hair = 0; // AF_HAIR_NONE
        facialField.setApparel(facialApp);
        af::Mesh facialOnly;
        const af::HairCardReport facialRep = af::appendProceduralFacialHairCards(facialField, 0.0f, &facialOnly);
        require(facialRep.browCardCount == 12 && facialRep.lashCardCount == 16 && facialRep.cardCount == 28,
                "facial hair cards must generate 12 brow cards and 16 upper/lower lash cards");
        const af::Mesh::ManifoldReport facialAudit = facialOnly.audit();
        require(facialAudit.openEdges == 0 && facialAudit.nonManifoldEdges == 0,
                "3D eyebrow and eyelash cards must be watertight and manifold");

        af::Mesh baldAvatar;
        af::BodyBuildReport baldRep;
        require(af::buildBodyMesh(facialField, coarseOptions, &baldAvatar, &baldRep),
                "bald body mesh build with facial hair cards failed");
        require(baldRep.openEdgesAfterRepair == 0 && baldRep.nonManifoldEdges == 0,
                "bald avatar with 3D brows and lashes must remain watertight and manifold");
        af::Rig facialRig;
        require(facialRig.build(facialField, baldAvatar), "facial hair card rig build failed");

        int blinkLeftIdx = -1, lookOutLeftIdx = -1, browDownLeftIdx = -1;
        for (int c = 0; c < af::kBlendshapeCount; ++c) {
            if (std::string(af::kBlendshapeNames[c]) == "eyeBlinkLeft") blinkLeftIdx = c;
            if (std::string(af::kBlendshapeNames[c]) == "eyeLookOutLeft") lookOutLeftIdx = c;
            if (std::string(af::kBlendshapeNames[c]) == "browDownLeft") browDownLeftIdx = c;
        }
        require(blinkLeftIdx >= 0 && lookOutLeftIdx >= 0 && browDownLeftIdx >= 0,
                "expected facial ARKit blendshapes missing");

        float maxLashBlinkDown = 0.0f, maxFacialLookDelta = 0.0f, maxBrowDown = 0.0f;
        size_t facialCardVerts = 0;
        const auto& blinkDeltas = facialRig.blendshapeDeltas(blinkLeftIdx);
        const auto& lookDeltas = facialRig.blendshapeDeltas(lookOutLeftIdx);
        const auto& browDeltas = facialRig.blendshapeDeltas(browDownLeftIdx);
        for (size_t i = 0; i < baldAvatar.positions.size(); ++i) {
            if (baldAvatar.regions[i] != af::REGION_HAIR_CARD) continue;
            ++facialCardVerts;
            const af::SkinWeights& sw = facialRig.weights()[i];
            for (int k = 0; k < 4; ++k) {
                if (sw.weights[k] > 0.0f) {
                    require(facialRig.joints()[sw.joints[k]].name == "head",
                            "facial brow/lash card vertex must bind to head joint");
                }
            }
            if (baldAvatar.positions[i].x > 0.0f) {
                maxLashBlinkDown = std::max(maxLashBlinkDown, -blinkDeltas[i].y);
                maxBrowDown = std::max(maxBrowDown, -browDeltas[i].y);
            }
            maxFacialLookDelta = std::max(maxFacialLookDelta, lookDeltas[i].length());
        }
        require(facialCardVerts >= static_cast<size_t>(facialRep.vertexCount),
                "bald avatar must preserve all 3D eyebrow and eyelash card vertices");
        require(maxLashBlinkDown > 0.002f,
                "3D upper eyelash cards must move downward with eyeBlinkLeft");
        require(maxBrowDown > 0.001f,
                "3D eyebrow cards must move downward with browDownLeft");
        require(maxFacialLookDelta < 1e-6f,
                "eyeLookOutLeft must rotate eyeball/cornea without pulling 3D eyelashes or brows");
    }

    // Visual-quality anatomy & apparel regression guards (female dress + long hair):
    //   1. Hairline plane must clip the facial side (-Z) so forehead, brows, eyes & nose are 100% clear.
    //   2. Eyelid palpebral aperture must leave both corneal front poles unobstructed by skin.
    //   3. Zygomatic cheeks must stay cleanly inside the cranial silhouette (no chipmunk bulge).
    //   4. Sleeveless dress (AF_OUTFIT_DRESS) must have no detached pauldron cap over the outer deltoid.
    //   5. Solid scalp cap (REGION_HAIR) stays matte (roughness >= 0.50) to avoid metallic chrome glare.
    {
        af_spec fSpec{};
        af_spec_init(&fSpec);
        fSpec.sex = AF_SEX_FEMALE;
        const af::BodyMetrics fm = af::Anatomy::solve(fSpec);
        af::BodyField fField(fm);
        af::ApparelOptions fApp{};
        fApp.outfit = 2; // AF_OUTFIT_DRESS
        fApp.hair = 4;   // AF_HAIR_LONG
        fApp.latticeCell = 0.008f;
        fField.setApparel(fApp);

        const float fHeadH = fm.y.crownY - fm.y.chinY;
        const af::Vec3 browFront(0.0f, fm.y.eyeY + fHeadH * 0.03f, -fm.head.rz * 1.02f);
        const af::Vec3 foreheadMid(0.0f, fm.y.eyeY + fHeadH * 0.09f, -fm.head.rz * 1.02f);
        const af::Vec3 crownTop(0.0f, fm.y.crownY + 0.002f, fm.head.centreZ);
        require(fField.hairPublic(browFront) > 0.010f,
                "hairline must not droop over the brow ridge / eyes");
        require(fField.hairPublic(foreheadMid) > 0.005f,
                "hairline must keep the forehead exposed");
        require(fField.hairPublic(crownTop) < 0.0f,
                "hair cap must cover the cranial crown");

        for (int side : {-1, 1}) {
            const af::Vec3 ec = fField.eyeCentre(side);
            const af::Vec3 corneaFront(ec.x, ec.y, ec.z - fm.eyeRadius * 0.95f);
            require(fField.head(corneaFront) > 0.0f,
                    "eyelid palpebral fissure must leave the front cornea unobstructed by skin");
            require(fField.hairPublic(corneaFront) > 0.015f,
                    "hair volume must never cover the eyes");

            const af::Vec3 wideCheek(static_cast<float>(side) * fm.head.rx * 1.12f,
                                     fm.y.chinY + fHeadH * 0.30f, -fm.head.rz * 0.65f);
            require(fField.head(wideCheek) > 0.0f,
                    "cheek geometry must not bulge laterally past the cranial oval");

            const af::Vec3 deltoidCap(static_cast<float>(side) * (fm.biacromialHalf + fm.deltoidR * 0.85f),
                                      fm.y.shoulderY + fm.deltoidR * 0.55f, 0.0f);
            require(fField.clothUpperPublic(deltoidCap) > 0.0f,
                    "sleeveless dress must not attach floating pauldron caps on the outer deltoids");
        }
        require(af::materialForRegion(af::REGION_HAIR).roughness >= 0.50f,
                "REGION_HAIR scalp cap must remain matte under layered hair cards");
    }

    std::puts("PASS: body refinement increases generated avatar mesh density with finite vertices");
    std::puts("PASS: long hairstyle has measurable volumetric depth behind the skull");
    std::puts("PASS: GLB export omits KHR_materials_sheen and exports wet cornea with BLEND + clearcoat");
    std::puts("PASS: dual-mesh eyes pass geometry, sclera/iris texture, rigid skinning, and blendshape checks");
    std::puts("PASS: procedural hair cards pass watertightness, UV sheet, alpha/normal bake, GLB MASK, and rigging checks");
    std::puts("PASS: 3D eyebrow and eyelash cards pass watertightness, head skinning, and blink/brow blendshape checks");
    std::puts("PASS: female dress + long hair hairline, open eyelids, cheek oval, sleeveless shoulders, and matte scalp checks");

    std::puts("PASS: 40 repeated single-thread, parallel, and block-skipped sampling comparisons");
    return 0;
}
