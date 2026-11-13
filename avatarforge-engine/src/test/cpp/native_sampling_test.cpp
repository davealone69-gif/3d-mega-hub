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
