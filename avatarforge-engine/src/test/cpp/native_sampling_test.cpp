#include "af_isosurface.h"
#include "af_prompt.h"
#include "af_anatomy.h"
#include "af_body.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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

    std::puts("PASS: body refinement increases generated avatar mesh density with finite vertices");
    std::puts("PASS: long hairstyle has measurable volumetric depth behind the skull");

    std::puts("PASS: 40 repeated single-thread, parallel, and block-skipped sampling comparisons");
    return 0;
}
