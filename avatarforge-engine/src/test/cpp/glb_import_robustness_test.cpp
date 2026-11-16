// Robustness test for the GLB importer, the app's entry point for files it did not create.
//
// It feeds deterministic mutations of a valid GLB to af_engine_import_glb_from_memory(): truncated
// files, corrupted header and chunk lengths, extreme numbers in the JSON, flipped JSON and binary
// bytes. Every call must return, with success or a clean error. For every accepted file the index
// buffer and the submesh ranges must stay inside the data the renderer would read.
//
// In the normal regression build this catches crashes, hangs and broken ranges. Build the engine
// and this file with -fsanitize=address,undefined to turn out-of-bounds reads, overflows and leaks
// into failures as well (see BUILD_AND_VERIFY.md).
//
// Usage: af_glb_import_robustness_test <valid.glb> [iterations] [seed]
#include "afcore.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> readFile(const char* path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

uint32_t readU32(const std::vector<uint8_t>& b, size_t at) {
    return uint32_t(b[at]) | (uint32_t(b[at + 1]) << 8) | (uint32_t(b[at + 2]) << 16) | (uint32_t(b[at + 3]) << 24);
}

void writeU32(std::vector<uint8_t>& b, size_t at, uint32_t v) {
    b[at] = uint8_t(v); b[at + 1] = uint8_t(v >> 8); b[at + 2] = uint8_t(v >> 16); b[at + 3] = uint8_t(v >> 24);
}

struct Tally { long ok = 0; long rejected = 0; };

int importOnce(const std::vector<uint8_t>& bytes, Tally* tally) {
    af_spec spec{};
    af_spec_init(&spec);
    af_engine* engine = nullptr;
    if (af_engine_create(&spec, nullptr, &engine) != AF_OK || !engine) {
        std::printf("af_engine_create failed\n");
        return 1;
    }
    const int32_t rc = af_engine_import_glb_from_memory(engine, bytes.empty() ? nullptr : bytes.data(), bytes.size());
    if (rc == AF_OK) {
        ++tally->ok;
        // Touch what a renderer would read, so a successful import with bad ranges is caught too.
        const af_vertex* v = af_engine_vertices(engine);
        const int32_t vc = af_engine_vertex_count(engine);
        const uint32_t* idx = af_engine_indices(engine);
        const int32_t ic = af_engine_index_count(engine);
        volatile float sink = 0.0f;
        for (int32_t i = 0; i < vc; ++i) sink = sink + v[i].pos[0];
        for (int32_t i = 0; i < ic; ++i) {
            if (idx[i] >= uint32_t(vc)) {
                std::printf("import accepted an index out of range: %u >= %d\n", idx[i], vc);
                af_engine_destroy(engine);
                return 2;
            }
        }
        const af_submesh* sm = af_engine_submeshes(engine);
        const int32_t sc = af_engine_submesh_count(engine);
        for (int32_t i = 0; i < sc; ++i) {
            const int64_t first = sm[i].index_offset, count = sm[i].index_count;
            if (first < 0 || count < 0 || first + count > ic) {
                std::printf("import accepted a submesh range outside the index buffer\n");
                af_engine_destroy(engine);
                return 3;
            }
        }
    } else {
        ++tally->rejected;
    }
    af_engine_destroy(engine);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: %s <valid.glb> [iterations] [seed]\n", argv[0]);
        return 64;
    }
    /* The importer logs every accessor it reads to stderr. Thousands of imports would bury the
       result, so stderr is discarded; this test reports on stdout. */
    if (!std::freopen("/dev/null", "w", stderr)) std::printf("note: stderr could not be silenced\n");
    const std::vector<uint8_t> seedFile = readFile(argv[1]);
    const long iterations = argc > 2 ? std::atol(argv[2]) : 2000;
    const unsigned seed = argc > 3 ? unsigned(std::atol(argv[3])) : 1337u;
    if (seedFile.size() < 28) {
        std::printf("seed file is too small to be a GLB\n");
        return 65;
    }
    const uint32_t jsonLength = readU32(seedFile, 12);
    const size_t jsonStart = 20;
    const size_t jsonEnd = jsonStart + jsonLength;
    if (jsonEnd > seedFile.size()) {
        std::printf("seed file has a broken JSON chunk\n");
        return 66;
    }

    Tally baseline;
    if (int rc = importOnce(seedFile, &baseline)) return rc;
    std::printf("seed file: %zu bytes, JSON chunk %u bytes, unmodified import %s\n",
                seedFile.size(), jsonLength, baseline.ok ? "accepted" : "rejected");

    std::mt19937 rng(seed);
    auto pick = [&](size_t lo, size_t hi) { return std::uniform_int_distribution<size_t>(lo, hi)(rng); };
    static const char* kExtremes[] = {"0", "-1", "1", "2147483647", "2147483648", "4294967295",
                                      "4294967296", "9999999999999", "1e30", "-1e30", "0.5", "65535", "65536"};
    static const uint32_t kWords[] = {0u, 1u, 4u, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu, 0xFFFFFFFCu, 0x4E4F534Au, 0x004E4942u};

    const char* kNames[] = {"truncate", "header length", "json chunk length", "bin chunk header",
                            "json number", "json byte flip", "bin byte flip", "json token swap"};
    Tally perKind[8];
    for (long it = 0; it < iterations; ++it) {
        std::vector<uint8_t> m = seedFile;
        const int kind = int(it % 8);
        switch (kind) {
        case 0:   /* cut the file anywhere, header included */
            m.resize(pick(0, m.size() - 1));
            break;
        case 1:   /* total length in the header */
            writeU32(m, 8, kWords[pick(0, 8)] + uint32_t(pick(0, 64)));
            break;
        case 2:   /* JSON chunk length or type */
            writeU32(m, pick(0, 1) ? 12 : 16, kWords[pick(0, 8)] + uint32_t(pick(0, 64)));
            break;
        case 3:   /* BIN chunk header, when there is one */
            if (jsonEnd + 8 <= m.size()) writeU32(m, jsonEnd + (pick(0, 1) ? 0 : 4), kWords[pick(0, 8)] + uint32_t(pick(0, 64)));
            break;
        case 4: { /* replace one numeric token in the JSON with an extreme value */
            std::vector<size_t> starts;
            for (size_t i = jsonStart; i < jsonEnd; ++i) {
                const bool digit = m[i] >= '0' && m[i] <= '9';
                const bool prevDigit = i > jsonStart && ((m[i - 1] >= '0' && m[i - 1] <= '9') || m[i - 1] == '.' || m[i - 1] == '-' || m[i - 1] == 'e');
                if (digit && !prevDigit) starts.push_back(i);
            }
            if (starts.empty()) break;
            const size_t s = starts[pick(0, starts.size() - 1)];
            size_t e = s;
            while (e < jsonEnd && ((m[e] >= '0' && m[e] <= '9') || m[e] == '.' || m[e] == 'e' || m[e] == '-' || m[e] == '+')) ++e;
            const std::string rep = kExtremes[pick(0, 12)];
            std::vector<uint8_t> out(m.begin(), m.begin() + long(s));
            out.insert(out.end(), rep.begin(), rep.end());
            out.insert(out.end(), m.begin() + long(e), m.end());
            /* keep the container consistent so the mutation reaches the accessor validation */
            const long delta = long(rep.size()) - long(e - s);
            const long pad = ((4 - ((long(jsonLength) + delta) % 4)) % 4 + 4) % 4;
            out.insert(out.begin() + long(jsonEnd) + delta, size_t(pad), uint8_t(' '));
            writeU32(out, 12, uint32_t(long(jsonLength) + delta + pad));
            writeU32(out, 8, uint32_t(out.size()));
            m.swap(out);
            break;
        }
        case 5:   /* flip a few bytes of JSON */
            for (size_t n = pick(1, 6); n > 0; --n) m[pick(jsonStart, jsonEnd - 1)] ^= uint8_t(1u << pick(0, 7));
            break;
        case 6:   /* flip a few bytes of the binary chunk */
            if (jsonEnd + 9 < m.size()) for (size_t n = pick(1, 32); n > 0; --n) m[pick(jsonEnd + 8, m.size() - 1)] = uint8_t(pick(0, 255));
            break;
        default: { /* overwrite a short stretch of JSON with structural characters */
            static const char kStructural[] = "{}[],:\"0";
            const size_t at = pick(jsonStart, jsonEnd - 1);
            for (size_t n = 0, len = pick(1, 12); n < len && at + n < jsonEnd; ++n) m[at + n] = uint8_t(kStructural[pick(0, 7)]);
            break;
        }
        }
        if (int rc = importOnce(m, &perKind[kind])) {
            std::printf("FAILED at iteration %ld (%s)\n", it, kNames[kind]);
            return rc;
        }
    }
    long ok = 0, rejected = 0;
    for (int k = 0; k < 8; ++k) {
        std::printf("  %-18s accepted %5ld  rejected %5ld\n", kNames[k], perKind[k].ok, perKind[k].rejected);
        ok += perKind[k].ok; rejected += perKind[k].rejected;
    }
    std::printf("PASS: %ld mutated files (seed %u): every import returned, %ld were accepted with index and submesh "
                "ranges inside their buffers, %ld were rejected with an error\n",
                iterations, seed, ok, rejected);
    return 0;
}
