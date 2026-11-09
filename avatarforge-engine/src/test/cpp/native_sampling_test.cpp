#include "af_isosurface.h"

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
    const std::vector<float> reference = sample(1, 1);
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

    std::puts("PASS: single-thread, parallel, and block-skipped sampling agree");
    return 0;
}
