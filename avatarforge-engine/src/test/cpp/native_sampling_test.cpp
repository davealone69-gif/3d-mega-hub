#include "af_isosurface.h"
#include "af_prompt.h"

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

    std::puts("PASS: 40 repeated single-thread, parallel, and block-skipped sampling comparisons");
    return 0;
}
