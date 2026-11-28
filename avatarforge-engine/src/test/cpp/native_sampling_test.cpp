#include "af_isosurface.h"
#include "af_prompt.h"
#include "af_anatomy.h"
#include "af_body.h"
#include "af_texture.h"

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
    // "t-shirt and jeans" is the T-shirt outfit, which the engine builds with jeans as its lower
    // garment. It used to parse as the jeans-and-JACKET outfit, so the avatar came out in a jacket
    // nobody asked for.
    require(measured.outfit == AF_OUTFIT_TSHIRT, "\"t-shirt and jeans\" must stay the T-shirt outfit");
    {
        af_spec jacket{};
        af_spec_init(&jacket);
        af::PromptParse jacketParse;
        af::parsePrompt("Adult man in a denim jacket and jeans", &jacket, &jacketParse);
        require(jacket.outfit == AF_OUTFIT_JEANS_JACKET, "a jeans jacket prompt was not parsed as the jacket outfit");
    }
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
    // The material of an eye is asked where it can be seen: on the surface, straight in front of
    // the eye's centre. (This used to ask at the centre of the globe, which is inside solid
    // material; the classifier now answers "eye" only on the visible surface between the lids,
    // because that is the only place the answer is used.)
    const auto visibleEyePoint = [&](const af::Vec3& centre) {
        af::Vec3 p(centre.x, centre.y, centre.z - 0.040f);
        for (int i = 0; i < 4000; ++i) {
            const float d = qualityField.skinUnion(p);
            if (d < 0.00002f) break;
            p.z += (d > 0.00002f ? d * 0.5f : 0.00001f);
        }
        return p;
    };
    const af::Vec3 leftEyeSurface = visibleEyePoint(leftEye);
    const af::Vec3 rightEyeSurface = visibleEyePoint(rightEye);
    require(leftEyeSurface.z < leftEye.z && leftEyeSurface.z > leftEye.z - 0.030f,
            "no surface was found in front of the left eye");
    require(qualityField.classifyRegion(leftEyeSurface) == af::REGION_EYE_L,
            "the visible surface of the left eye is missing or assigned to the wrong material region");
    require(qualityField.classifyRegion(rightEyeSurface) == af::REGION_EYE_R,
            "the visible surface of the right eye is missing or assigned to the wrong material region");

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
    // Long hair hangs down the back. Somewhere behind the neck, between the nape and the shoulder
    // blades, there must be hair with real thickness: at least 8 mm of it (4 mm deep at its
    // middle). The probe searches for it rather than assuming where it is - this check used to
    // sample one fixed point inside the 60 mm slab the hair was once made of, which was wider than
    // the head and has been replaced by hair that follows the head and the back.
    const float hairMidY = qualityMetrics.y.crownY - headHeight * 1.30f;
    float deepestHair = 1.0f;
    for (float z = qualityMetrics.head.centreZ; z < qualityMetrics.head.centreZ + 0.30f; z += 0.002f) {
        deepestHair = std::min(deepestHair, hairField.hairPublic(af::Vec3(0.0f, hairMidY, z)));
    }
    require(deepestHair < -0.004f,
            "long hairstyle hanging section is not a solid 3D volume");

    // Every exposed procedural hairstyle must produce finite, non-empty geometry in
    // the same anatomical sampling envelope. This protects the generator while the
    // hairline, braid and coil fields evolve independently.
    for (int style = 1; style <= 13; ++style) {
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

    // The shape controls appended to af_spec: bust, glute, the face offsets, and the curvy build.
    {
        af_spec base{};
        af_spec_init(&base);
        require(base.glute == 0.0f && base.eye_tone[0] == 0.0f && base.eye_tone[1] == 0.0f && base.eye_tone[2] == 0.0f,
                "af_spec_init does not leave the appended fields at their default");
        for (int i = 0; i < AF_FACE_PARAM_COUNT; ++i) {
            require(base.face[i] == 0.0f, "af_spec_init does not leave the face offsets at zero");
        }
        base.sex = AF_SEX_FEMALE;
        base.height_m = 1.70f;

        // The body's surface along a line through the body, found by walking in from outside.
        const auto surfaceZ = [](const af::BodyField& field, float x, float y, float fromZ, float step) {
            float z = fromZ;
            for (int i = 0; i < 700; ++i) {
                if (field.body(af::Vec3(x, y, z)) < 0.0f) break;
                z += step;
            }
            return z;
        };

        // Bust: a larger value stands further forward (forward is -Z), over the whole range.
        float previousFront = 1.0f, flatFront = 0.0f, fullFront = 0.0f;
        const float bustValues[5] = {0.04f, 0.24f, 0.45f, 0.70f, 1.00f};
        for (int i = 0; i < 5; ++i) {
            af_spec spec = base;
            spec.bust = bustValues[i];
            const af::BodyMetrics m = af::Anatomy::solve(spec);
            require(std::fabs(m.bust - bustValues[i]) < 1e-6f, "af_spec.bust did not reach the anatomy");
            const af::BodyField field(m);
            float front = 1.0f;
            // The apex moves down and outwards with size, so take the furthest-forward point of a patch.
            for (int iy = -6; iy <= 2; ++iy) {
                for (int ix = 4; ix <= 9; ++ix) {
                    front = std::min(front, surfaceZ(field, m.chest.rx * 0.08f * static_cast<float>(ix),
                                                     m.y.bustY + 0.010f * static_cast<float>(iy), -0.45f, 0.001f));
                }
            }
            require(front < previousFront - 0.004f, "a larger af_spec.bust did not stand further forward");
            previousFront = front;
            if (i == 0) flatFront = front;
            if (i == 4) fullFront = front;
        }
        require(flatFront - fullFront > 0.045f, "the bust range from flat to full is under 45 mm");

        // Glute: a larger value stands further back, and the default lies between the two ends.
        float previousBack = -1.0f, smallBack = 0.0f, largeBack = 0.0f;
        const float gluteValues[5] = {-0.90f, -0.45f, 0.0f, 0.50f, 1.00f};
        for (int i = 0; i < 5; ++i) {
            af_spec spec = base;
            spec.glute = gluteValues[i];
            const af::BodyMetrics m = af::Anatomy::solve(spec);
            require(std::fabs(m.glute - gluteValues[i]) < 1e-6f, "af_spec.glute did not reach the anatomy");
            const af::BodyField field(m);
            float back = -1.0f;
            for (int iy = -5; iy <= 3; ++iy) {
                back = std::max(back, surfaceZ(field, m.hip.rx * 0.45f,
                                               m.y.hipY - m.hip.rz * 0.36f + 0.012f * static_cast<float>(iy), 0.45f, -0.001f));
            }
            require(back > previousBack + 0.004f, "a larger af_spec.glute did not stand further back");
            previousBack = back;
            if (i == 0) smallBack = back;
            if (i == 4) largeBack = back;
        }
        require(largeBack - smallBack > 0.030f, "the glute range is under 30 mm");

        // Both are for adults: under 18 the engine uses its defaults whatever was asked for.
        af_spec young = base;
        young.age_years = 17.0f;
        young.bust = 1.0f;
        young.glute = 1.0f;
        const af::BodyMetrics youngMetrics = af::Anatomy::solve(young);
        require(std::fabs(youngMetrics.bust - 0.40f) < 1e-6f && youngMetrics.glute == 0.0f,
                "bust or glute was honoured for an age below 18");
        af_spec adult = young;
        adult.age_years = 18.0f;
        const af::BodyMetrics adultMetrics = af::Anatomy::solve(adult);
        require(adultMetrics.bust > 0.99f && adultMetrics.glute > 0.99f,
                "bust or glute was not honoured for an adult");

        // A measured bust girth still decides when the caller gives one.
        af_spec measuredBust = base;
        measuredBust.bust = 0.04f;
        measuredBust.chest_cm = 104.0f;
        measuredBust.underbust_cm = 76.0f;
        require(af::Anatomy::solve(measuredBust).bust > 0.30f,
                "a measured bust girth no longer decides the bust");

        // Face offsets: clamped, never non-finite, and each end of each one still gives a head.
        af_spec wild = base;
        wild.face[AF_FACE_NOSE_WIDTH] = 9.0f;
        wild.face[AF_FACE_LIP_FULLNESS] = -9.0f;
        wild.face[AF_FACE_JAW_WIDTH] = std::nanf("");
        const af::BodyMetrics wildMetrics = af::Anatomy::solve(wild);
        require(wildMetrics.face[AF_FACE_NOSE_WIDTH] == 1.0f && wildMetrics.face[AF_FACE_LIP_FULLNESS] == -1.0f &&
                wildMetrics.face[AF_FACE_JAW_WIDTH] == 0.0f,
                "face offsets are not clamped to -1..+1 with non-finite values dropped");
        for (int sign = -1; sign <= 1; sign += 2) {
            af_spec spec = base;
            for (int i = 0; i < AF_FACE_PARAM_COUNT; ++i) spec.face[i] = static_cast<float>(sign);
            const af::BodyMetrics m = af::Anatomy::solve(spec);
            const af::BodyField field(m);
            int inside = 0;
            for (int iy = 0; iy <= 14; ++iy) {
                for (int ix = -7; ix <= 7; ++ix) {
                    for (int iz = -8; iz <= 4; ++iz) {
                        const af::Vec3 q(m.head.rx * 0.16f * static_cast<float>(ix),
                                         m.y.chinY + (m.y.crownY - m.y.chinY) * static_cast<float>(iy) / 14.0f,
                                         m.head.rz * 0.16f * static_cast<float>(iz));
                        const float d = field.head(q);
                        require(std::isfinite(d), "a face offset at its limit produced a non-finite head field");
                        if (d < 0.0f) ++inside;
                    }
                }
            }
            require(inside > 200, "a face offset at its limit left almost no head");
        }
        // The widest nose is wider than the narrowest, measured at the wings.
        {
            float halfWidth[2] = {0.0f, 0.0f};
            for (int k = 0; k < 2; ++k) {
                af_spec spec = base;
                spec.face[AF_FACE_NOSE_WIDTH] = k == 0 ? -1.0f : 1.0f;
                const af::BodyMetrics m = af::Anatomy::solve(spec);
                const af::BodyField field(m);
                const float headH = m.y.crownY - m.y.chinY;
                const float y = m.y.eyeY - headH * 0.197f;          // the level of the wings
                const float z = -m.head.rz * 0.98f * 1.10f;         // a plane through them
                float x = 0.0f;
                while (x < 0.05f && field.head(af::Vec3(x, y, z)) < 0.0f) x += 0.0005f;
                halfWidth[k] = x;
            }
            require(halfWidth[1] > halfWidth[0] + 0.0015f, "the nose-width offset does not change the width of the nose");
        }

        // Curvy: a smaller waist-to-hip ratio than the average build, on the same frame.
        af_spec curvy = base;
        curvy.build = AF_BUILD_CURVY;
        const af::BodyMetrics curvyMetrics = af::Anatomy::solve(curvy);
        const af::BodyMetrics averageMetrics = af::Anatomy::solve(base);
        require(curvyMetrics.waist.rx / curvyMetrics.hip.rx < averageMetrics.waist.rx / averageMetrics.hip.rx - 0.05f,
                "the curvy build does not have a smaller waist-to-hip ratio than the average build");

        // Swimwear: cloth over the bust and over the pelvis, none at the waist; trunks only on a male body.
        {
            af_spec swim = base;
            swim.outfit = AF_OUTFIT_SWIMWEAR;
            const af::BodyMetrics m = af::Anatomy::solve(swim);
            af::BodyField field(m);
            af::ApparelOptions options;
            options.outfit = AF_OUTFIT_SWIMWEAR;
            options.latticeCell = 0.008f;
            field.setApparel(options);
            // A point two millimetres outside the skin, in front of the body at a given height.
            const auto outsideFront = [&](float x, float y) {
                return af::Vec3(x, y, surfaceZ(field, x, y, -0.45f, 0.001f) - 0.002f);
            };
            const af::Vec3 onBust = outsideFront(m.chest.rx * 0.45f, m.y.bustY);
            const af::Vec3 onWaist = outsideFront(0.0f, m.y.waistY);
            const af::Vec3 onPelvis = outsideFront(0.0f, m.y.crotchY + 0.035f * m.height);
            require(field.clothUpperPublic(onBust) < 0.0f, "swimwear does not cover the bust");
            require(field.clothUpperPublic(onWaist) > 0.0f && field.clothLowerPublic(onWaist) > 0.0f,
                    "swimwear covers the waist; it is a two-piece");
            require(field.clothLowerPublic(onPelvis) < 0.0f, "swimwear does not cover the pelvis");

            af_spec trunks = swim;
            trunks.sex = AF_SEX_MALE;
            const af::BodyMetrics mm = af::Anatomy::solve(trunks);
            af::BodyField maleField(mm);
            maleField.setApparel(options);
            require(maleField.clothUpperPublic(af::Vec3(mm.chest.rx * 0.45f, mm.y.bustY, -mm.chest.rz)) > 1e6f,
                    "male swimwear has a top");
            const float mx = mm.legSeparation;
            const float my = mm.y.crotchY - mm.thighLen * 0.10f;
            float mz = -0.45f;
            for (int i = 0; i < 700 && maleField.body(af::Vec3(mx, my, mz)) >= 0.0f; ++i) mz += 0.001f;
            require(maleField.clothLowerPublic(af::Vec3(mx, my, mz - 0.002f)) < 0.0f,
                    "male swimwear does not cover the top of the thigh");
        }
    }
    std::puts("PASS: bust, glute, face offsets and the curvy build change the body as specified; bust and glute are adult-only; swimwear covers what it should");

    std::puts("PASS: body refinement increases generated avatar mesh density with finite vertices");
    std::puts("PASS: long hairstyle has measurable volumetric depth behind the skull");

    std::puts("PASS: 40 repeated single-thread, parallel, and block-skipped sampling comparisons");
    return 0;
}
