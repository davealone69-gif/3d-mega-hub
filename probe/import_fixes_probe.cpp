/* Host probe for the two import-path fixes:
 *   F3: node.world transforms must bake for skinned imports too (skins are never applied,
 *       so the mesh node transform is the only placement the file provides).
 *   F4: alphaMode OPAQUE must pin base_color_factor[3] to 1 (glTF 2.0 sec 3.9.2).
 * Builds one tiny GLB with: a skinned primitive on a node translated +10 in X, an OPAQUE
 * material carrying baseColorFactor alpha 0.0, and a BLEND material carrying alpha 0.5. */
#include <afcore.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static void putFloat(std::vector<uint8_t>& b, float v) {
    uint8_t tmp[4];
    std::memcpy(tmp, &v, 4);
    b.insert(b.end(), tmp, tmp + 4);
}

static std::string buildGlb() {
    /* ---- binary buffer: 3 verts + 3 normals + 3 u16 indices + identity IBM ---- */
    std::vector<uint8_t> bin;
    const float positions[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    const float normals[9] = {0, 0, 1, 0, 0, 1, 0, 0, 1};
    for (int i = 0; i < 9; ++i) putFloat(bin, positions[i]);
    for (int i = 0; i < 9; ++i) putFloat(bin, normals[i]);
    const uint16_t indices[3] = {0, 1, 2};
    const uint8_t* idxBytes = reinterpret_cast<const uint8_t*>(indices);
    bin.insert(bin.end(), idxBytes, idxBytes + 6);
    while (bin.size() % 4 != 0) bin.push_back(0);
    const size_t ibmOffset = bin.size();
    const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    for (int i = 0; i < 16; ++i) putFloat(bin, identity[i]);

    const std::string json = R"JSON(
{"asset":{"version":"2.0"},
 "scene":0,
 "scenes":[{"nodes":[0]}],
 "nodes":[
   {"name":"meshNode","mesh":0,"skin":0,"translation":[10,0,0]},
   {"name":"joint0"}],
 "skins":[{"joints":[1],"inverseBindMatrices":3}],
 "meshes":[{"primitives":[
   {"attributes":{"POSITION":0,"NORMAL":1},"indices":2,"material":0},
   {"attributes":{"POSITION":0,"NORMAL":1},"indices":2,"material":1}]}],
 "materials":[
   {"name":"opaqueZeroAlpha","alphaMode":"OPAQUE",
    "pbrMetallicRoughness":{"baseColorFactor":[0.8,0.7,0.6,0.0]}},
   {"name":"blendHalfAlpha","alphaMode":"BLEND",
    "pbrMetallicRoughness":{"baseColorFactor":[0.5,0.4,0.3,0.5]}}],
 "accessors":[
   {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
   {"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},
   {"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"},
   {"bufferView":3,"componentType":5126,"count":1,"type":"MAT4"}],
 "bufferViews":[
   {"buffer":0,"byteOffset":0,"byteLength":36},
   {"buffer":0,"byteOffset":36,"byteLength":36},
   {"buffer":0,"byteOffset":72,"byteLength":6},
   {"buffer":0,"byteOffset":)JSON" + std::to_string(ibmOffset) + R"JSON(,"byteLength":64}],
 "buffers":[{"byteLength":)JSON" + std::to_string(bin.size()) + R"JSON(}]}
)JSON";

    std::string jsonPadded = json;
    while (jsonPadded.size() % 4 != 0) jsonPadded.push_back(' ');
    std::string binPadded(reinterpret_cast<const char*>(bin.data()), bin.size());
    while (binPadded.size() % 4 != 0) binPadded.push_back('\0');

    std::vector<uint8_t> glb;
    auto put32 = [&](uint32_t v) {
        glb.push_back(uint8_t(v & 0xff));
        glb.push_back(uint8_t((v >> 8) & 0xff));
        glb.push_back(uint8_t((v >> 16) & 0xff));
        glb.push_back(uint8_t((v >> 24) & 0xff));
    };
    put32(0x46546C67);                      /* 'glTF' */
    put32(2);
    put32(uint32_t(12 + 8 + jsonPadded.size() + 8 + binPadded.size()));
    put32(uint32_t(jsonPadded.size()));
    put32(0x4E4F534A);                      /* 'JSON' */
    glb.insert(glb.end(), jsonPadded.begin(), jsonPadded.end());
    put32(uint32_t(binPadded.size()));
    put32(0x004E4942);                      /* 'BIN' */
    glb.insert(glb.end(), binPadded.begin(), binPadded.end());

    const char* path = "/tmp/import_fixes_probe.glb";
    FILE* f = std::fopen(path, "wb");
    if (!f) return "";
    std::fwrite(glb.data(), 1, glb.size(), f);
    std::fclose(f);
    return path;
}

static int failures = 0;
static void check(bool ok, const char* what) {
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

int main() {
    const std::string path = buildGlb();
    if (path.empty()) {
        std::printf("FAIL: could not write probe GLB\n");
        return 1;
    }

    af_spec spec{};
    af_spec_init(&spec);
    af_engine* engine = nullptr;
    const int32_t created = af_engine_create(&spec, nullptr, &engine);
    check(created == AF_OK && engine != nullptr, "af_engine_create");

    const int32_t imported = af_engine_import_glb(engine, path.c_str());
    if (imported != AF_OK) {
        std::printf("FAIL: import rc=%d (%s)\n", int(imported),
                    af_engine_last_error(engine) ? af_engine_last_error(engine) : "?");
        return 1;
    }
    std::printf("import ok: verts=%d indices=%d submeshes=%d materials=%d joints=%d\n",
                af_engine_vertex_count(engine), af_engine_index_count(engine),
                af_engine_submesh_count(engine), af_engine_material_count(engine),
                af_engine_joint_count(engine));

    /* ---- F3: the skinned mesh node's +10 X translation must be baked ---- */
    const af_submesh* sub = af_engine_submeshes(engine);
    const int subCount = af_engine_submesh_count(engine);
    check(subCount == 2, "both primitives imported as submeshes");
    for (int i = 0; i < subCount; ++i) {
        std::printf("  submesh[%d] bounds_min=(%.3f,%.3f,%.3f) bounds_max=(%.3f,%.3f,%.3f)\n", i,
                    sub[i].bounds_min[0], sub[i].bounds_min[1], sub[i].bounds_min[2],
                    sub[i].bounds_max[0], sub[i].bounds_max[1], sub[i].bounds_max[2]);
    }
    bool bakeOk = true;
    for (int i = 0; i < subCount; ++i) {
        if (sub[i].bounds_min[0] < 9.9f || sub[i].bounds_max[0] < 10.9f) bakeOk = false;
    }
    check(bakeOk, "F3: skinned import baked the mesh node world translation (+10 X)");

    /* ---- F4: OPAQUE alpha pinned to 1, BLEND alpha preserved ---- */
    const af_material* mats = af_engine_materials(engine);
    const int matCount = af_engine_material_count(engine);
    check(matCount == 2, "both materials imported");
    for (int i = 0; i < matCount; ++i) {
        std::printf("  material[%d] '%s' base_color_factor=(%.2f,%.2f,%.2f,%.2f)\n", i, mats[i].name,
                    mats[i].base_color_factor[0], mats[i].base_color_factor[1],
                    mats[i].base_color_factor[2], mats[i].base_color_factor[3]);
    }
    bool alphaOk = true;
    for (int i = 0; i < matCount; ++i) {
        if (std::strcmp(mats[i].name, "opaqueZeroAlpha") == 0 &&
            std::fabs(mats[i].base_color_factor[3] - 1.0f) > 1e-5f) alphaOk = false;
        if (std::strcmp(mats[i].name, "blendHalfAlpha") == 0 &&
            std::fabs(mats[i].base_color_factor[3] - 0.5f) > 1e-5f) alphaOk = false;
    }
    check(alphaOk, "F4: OPAQUE alpha forced to 1.0 and BLEND alpha 0.5 preserved");

    af_engine_destroy(engine);
    std::printf(failures == 0 ? "\nALL CHECKS PASSED\n" : "\n%d CHECK(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
