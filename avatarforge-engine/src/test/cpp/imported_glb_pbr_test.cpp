/*
 * imported_glb_pbr_test.cpp
 *
 * Host regression suite covering foreign GLB import + renderer scene invariants:
 *   1. Degenerate (0,0,0) vertex normals are repaired to finite unit normals.
 *   2. Foreign material names ("", "Material.001") map to sm.region == -1 (AF_KIND_GENERIC),
 *      not REGION_SKIN (0).
 *   3. alphaMode == "OPAQUE" normalizes baseColor alpha to 255 so chart-border texels with
 *      alpha == 0 are not discarded by forward.frag.
 *   4. Importing a GLB after af_engine_run_all() clears stale procedural texture slots
 *      (normal/occlusion/subsurface) while populating referenced metallicRoughness & baseColor.
 *   5. Static scene-graph node transforms (translation + scale) are baked into imported vertices
 *      and submesh AABBs.
 *   6. buildCameraUbo anchors ground[0] at modelMin[1] so models centered around origin
 *      (Y in [-0.5, +0.5]) are not bisected by the ground plane.
 *   7. Optional CLI path argument (e.g. mylove.glb) runs live end-to-end WebP + normal + PBR
 *      checks against a real asset.
 */
#include "afcore.h"
#include "af_glb_read.h"
#include "af_png.h"
#include "af_scene.h"
#include "af_scene_bridge.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

int gFailures = 0;

void check(bool cond, const char* msg) {
    if (cond) {
        std::printf("PASS  %s\n", msg);
    } else {
        std::fprintf(stderr, "FAIL  %s\n", msg);
        ++gFailures;
    }
}

void appendU32(std::vector<uint8_t>* out, uint32_t v) {
    out->push_back(uint8_t(v & 0xFFu));
    out->push_back(uint8_t((v >> 8) & 0xFFu));
    out->push_back(uint8_t((v >> 16) & 0xFFu));
    out->push_back(uint8_t((v >> 24) & 0xFFu));
}

void appendF32(std::vector<uint8_t>* out, float f) {
    uint32_t bits = 0;
    std::memcpy(&bits, &f, sizeof(bits));
    appendU32(out, bits);
}

std::vector<uint8_t> buildGlb(const std::string& jsonText, const std::vector<uint8_t>& binChunk) {
    std::string paddedJson = jsonText;
    while (paddedJson.size() % 4u != 0u) paddedJson.push_back(' ');
    std::vector<uint8_t> paddedBin = binChunk;
    while (paddedBin.size() % 4u != 0u) paddedBin.push_back(0u);

    const uint32_t totalLen = uint32_t(12u + 8u + paddedJson.size() + 8u + paddedBin.size());
    std::vector<uint8_t> glb;
    glb.reserve(totalLen);
    appendU32(&glb, 0x46546C67u); // "glTF"
    appendU32(&glb, 2u);
    appendU32(&glb, totalLen);

    appendU32(&glb, uint32_t(paddedJson.size()));
    appendU32(&glb, 0x4E4F534Au); // "JSON"
    glb.insert(glb.end(), paddedJson.begin(), paddedJson.end());

    appendU32(&glb, uint32_t(paddedBin.size()));
    appendU32(&glb, 0x004E4942u); // "BIN\0"
    glb.insert(glb.end(), paddedBin.begin(), paddedBin.end());
    return glb;
}

std::vector<uint8_t> buildSyntheticForeignGlb() {
    // 2x2 RGBA baseColor PNG where texel (0,0) has alpha=0 despite alphaMode="OPAQUE"
    const uint8_t baseRgba[16] = {
        200,  40,  60,   0,   200,  40,  60, 255,
        200,  40,  60, 255,   200,  40,  60, 255
    };
    std::vector<uint8_t> basePng;
    af::encodePng(baseRgba, 2, 2, 4, &basePng);

    // 2x2 RGBA metallicRoughness PNG (G = roughness 180, B = metallic 64)
    const uint8_t mrRgba[16] = {
        0, 180, 64, 255,   0, 180, 64, 255,
        0, 180, 64, 255,   0, 180, 64, 255
    };
    std::vector<uint8_t> mrPng;
    af::encodePng(mrRgba, 2, 2, 4, &mrPng);

    std::vector<uint8_t> bin;
    // View 0: POSITION (3 verts in [-0.25, 0.25])
    const size_t posOff = bin.size();
    const float pos[9] = {-0.25f, -0.25f, 0.0f,  0.25f, -0.25f, 0.0f,  0.0f, 0.25f, 0.0f};
    for (float v : pos) appendF32(&bin, v);
    const size_t posLen = bin.size() - posOff;

    // View 1: NORMAL with vertex 0 deliberately (0,0,0) degenerate!
    const size_t normOff = bin.size();
    const float nrm[9] = {0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f};
    for (float v : nrm) appendF32(&bin, v);
    const size_t normLen = bin.size() - normOff;

    // View 2: TEXCOORD_0
    const size_t uvOff = bin.size();
    const float uvs[6] = {0.0f, 0.0f,  1.0f, 0.0f,  0.5f, 1.0f};
    for (float v : uvs) appendF32(&bin, v);
    const size_t uvLen = bin.size() - uvOff;

    // View 3: INDICES (uint32)
    const size_t idxOff = bin.size();
    appendU32(&bin, 0u);
    appendU32(&bin, 1u);
    appendU32(&bin, 2u);
    const size_t idxLen = bin.size() - idxOff;

    while (bin.size() % 4u != 0u) bin.push_back(0u);
    const size_t img0Off = bin.size();
    bin.insert(bin.end(), basePng.begin(), basePng.end());
    const size_t img0Len = basePng.size();

    while (bin.size() % 4u != 0u) bin.push_back(0u);
    const size_t img1Off = bin.size();
    bin.insert(bin.end(), mrPng.begin(), mrPng.end());
    const size_t img1Len = mrPng.size();

    // Node scales by 2.0 and translates Y by -0.25 -> final Y bounds [-0.75, +0.25]
    char jsonBuf[4096];
    std::snprintf(jsonBuf, sizeof(jsonBuf), R"({
      "asset": {"version": "2.0"},
      "scene": 0,
      "scenes": [{"nodes": [0]}],
      "nodes": [{"mesh": 0, "scale": [2.0, 2.0, 2.0], "translation": [0.0, -0.25, 0.0]}],
      "meshes": [{
        "primitives": [{
          "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
          "indices": 3,
          "material": 0
        }]
      }],
      "materials": [{
        "name": "",
        "alphaMode": "OPAQUE",
        "pbrMetallicRoughness": {
          "baseColorTexture": {"index": 0},
          "metallicRoughnessTexture": {"index": 1},
          "metallicFactor": 1.0,
          "roughnessFactor": 1.0
        }
      }],
      "textures": [{"source": 0}, {"source": 1}],
      "images": [
        {"bufferView": 4, "mimeType": "image/png"},
        {"bufferView": 5, "mimeType": "image/png"}
      ],
      "bufferViews": [
        {"buffer": 0, "byteOffset": %zu, "byteLength": %zu},
        {"buffer": 0, "byteOffset": %zu, "byteLength": %zu},
        {"buffer": 0, "byteOffset": %zu, "byteLength": %zu},
        {"buffer": 0, "byteOffset": %zu, "byteLength": %zu},
        {"buffer": 0, "byteOffset": %zu, "byteLength": %zu},
        {"buffer": 0, "byteOffset": %zu, "byteLength": %zu}
      ],
      "accessors": [
        {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", "min": [-0.25,-0.25,0.0], "max": [0.25,0.25,0.0]},
        {"bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3"},
        {"bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC2"},
        {"bufferView": 3, "componentType": 5125, "count": 3, "type": "SCALAR"}
      ],
      "buffers": [{"byteLength": %zu}]
    })",
    posOff, posLen, normOff, normLen, uvOff, uvLen, idxOff, idxLen,
    img0Off, img0Len, img1Off, img1Len, bin.size());

    return buildGlb(jsonBuf, bin);
}

std::vector<float> capturePositions(const af_engine* engine) {
    const int32_t count = af_engine_vertex_count(engine);
    const af_vertex* vertices = af_engine_vertices(engine);
    if (count <= 0 || !vertices) return {};
    std::vector<float> positions;
    positions.reserve(static_cast<size_t>(count) * 3u);
    for (int32_t i = 0; i < count; ++i) {
        positions.push_back(vertices[i].pos[0]);
        positions.push_back(vertices[i].pos[1]);
        positions.push_back(vertices[i].pos[2]);
    }
    return positions;
}

bool positionsDiffer(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.empty() || a.size() != b.size()) return false;
    float maxDelta = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        if (!std::isfinite(a[i]) || !std::isfinite(b[i])) return false;
        const float delta = std::fabs(a[i] - b[i]);
        if (delta > maxDelta) maxDelta = delta;
    }
    return maxDelta > 1e-5f;
}

bool fakeFallbackWebpDecoder(const uint8_t* data, std::size_t size,
                             af::DecodedImage* out, std::string*) {
    if (!data || size < 12 || !out) return false;
    out->width = 2;
    out->height = 2;
    out->rgba = {
        120, 200, 80, 255,   120, 200, 80, 255,
        120, 200, 80, 255,   120, 200, 80, 255
    };
    return true;
}

} // namespace

int main(int argc, char** argv) {
    af_spec spec{};
    af_spec_init(&spec);
    af_engine* engine = nullptr;
    check(af_engine_create(&spec, nullptr, &engine) == AF_OK && engine != nullptr,
          "af_engine_create succeeds");

    // Populate procedural textures first to ensure import clears unused slots.
    check(af_engine_run_all(engine) == AF_OK, "procedural af_engine_run_all populates all 5 texture slots");
    af_texture preNormal{};
    check(af_engine_texture(engine, AF_TEX_NORMAL, &preNormal) == AF_OK && preNormal.data != nullptr,
          "procedural normal texture is populated before import");

    // The generated avatar must move through its own skeleton, not just export a static mesh.
    const int32_t generatedJointCount = af_engine_joint_count(engine);
    check(generatedJointCount > 0, "generated avatar has a live skeleton");
    const float walkDuration = af_engine_clip_duration(engine, AF_CLIP_WALK);
    check(std::isfinite(walkDuration) && walkDuration > 0.0f, "built-in walk clip has a finite duration");
    check(af_engine_apply_clip(engine, AF_CLIP_REST, 0.0f) == AF_OK, "generated avatar accepts its rest pose");
    const std::vector<float> restPositions = capturePositions(engine);
    check(!restPositions.empty(), "generated rest pose exposes live vertex positions");

    check(af_engine_apply_clip(engine, AF_CLIP_WALK, walkDuration * 0.25f) == AF_OK,
          "generated avatar samples the first walk pose");
    const std::vector<float> walkPositionsA = capturePositions(engine);
    check(positionsDiffer(restPositions, walkPositionsA),
          "walk pose deforms generated vertex positions relative to rest pose");

    check(af_engine_apply_clip(engine, AF_CLIP_WALK, walkDuration * 0.75f) == AF_OK,
          "generated avatar samples a later walk pose");
    const std::vector<float> walkPositionsB = capturePositions(engine);
    check(positionsDiffer(walkPositionsA, walkPositionsB),
          "different walk timestamps produce different deformed geometry");
    char animationValidation[1024] = {};
    check(af_engine_validate_deformed_mesh(engine, animationValidation,
              static_cast<int32_t>(sizeof(animationValidation))) == AF_OK,
          "animated generated mesh passes finite-position, skin-weight, joint and bounds validation");

    check(af_engine_apply_clip(engine, AF_CLIP_REST, 0.0f) == AF_OK,
          "generated avatar returns to its rest pose before GLB export");
    uint8_t* generatedGlbBytes = nullptr;
    size_t generatedGlbSize = 0;
    const int32_t exportRc = af_engine_export_glb_to_memory(engine, &generatedGlbBytes, &generatedGlbSize);
    check(exportRc == AF_OK && generatedGlbBytes != nullptr && generatedGlbSize > 0,
          "generated avatar exports a non-empty self-contained GLB");
    if (exportRc == AF_OK && generatedGlbBytes != nullptr && generatedGlbSize > 0) {
        af::GlbAsset exportedAsset;
        std::string exportParseError;
        const int32_t parseRc = af::importGlb(generatedGlbBytes, generatedGlbSize,
                                               &exportedAsset, &exportParseError);
        check(parseRc == AF_OK, "exported generated-avatar GLB parses through the production reader");
        if (parseRc == AF_OK) {
            check(!exportedAsset.skins.empty(), "generated GLB contains its exported skeleton/skin");
            check(!exportedAsset.animations.empty(), "generated GLB contains animation clips");
            bool hasAnimationChannels = false;
            for (const auto& animation : exportedAsset.animations) {
                if (!animation.channels.empty()) {
                    hasAnimationChannels = true;
                    break;
                }
            }
            check(hasAnimationChannels, "generated GLB animation clips contain playable transform channels");
        }

        // Foreign GLB import is intentionally static today. Prove that a rigged export is not
        // silently advertised as animated after crossing the import boundary.
        const int32_t reimportRc = af_engine_import_glb_from_memory(engine, generatedGlbBytes, generatedGlbSize);
        check(reimportRc == AF_OK, "generated rigged GLB can be re-imported as foreign geometry");
        if (reimportRc == AF_OK) {
            check(af_engine_joint_count(engine) == 0,
                  "foreign GLB import does not falsely adopt the embedded skeleton");
        }
    }
    if (generatedGlbBytes != nullptr) af_arena_free(generatedGlbBytes);

    const std::vector<uint8_t> glb = buildSyntheticForeignGlb();
    check(af_engine_import_glb_from_memory(engine, glb.data(), glb.size()) == AF_OK,
          "af_engine_import_glb_from_memory imports foreign GLB");

    // 1. Degenerate normal repaired
    const int32_t vcount = af_engine_vertex_count(engine);
    const af_vertex* verts = af_engine_vertices(engine);
    check(vcount == 3 && verts != nullptr, "imported vertex count == 3");
    const float n0Len = std::sqrt(verts[0].normal[0]*verts[0].normal[0] +
                                  verts[0].normal[1]*verts[0].normal[1] +
                                  verts[0].normal[2]*verts[0].normal[2]);
    check(std::isfinite(n0Len) && std::fabs(n0Len - 1.0f) < 1e-4f && verts[0].normal[2] > 0.99f,
          "zero-length vertex normal repaired from triangle geometry");

    // 2. Static node world transform baked into positions & submesh AABB
    const af_submesh* sm = af_engine_submeshes(engine);
    check(af_engine_submesh_count(engine) == 1 && sm != nullptr, "imported submesh count == 1");
    check(std::fabs(sm[0].bounds_min[1] - (-0.75f)) < 1e-4f &&
          std::fabs(sm[0].bounds_max[1] - 0.25f) < 1e-4f,
          "node scale+translation baked into static imported submesh bounds");

    /* Exercise the same scene bridge the GLES backend calls, against the real imported submesh. */
    af_scene_bridge* bridge = af_scene_bridge_create();
    af::Camera sceneCamera;
    sceneCamera.position = af::Vec3(0.0f, 0.5f, 3.0f);
    sceneCamera.target = af::Vec3(0.0f, -0.25f, 0.0f);
    sceneCamera.aspect = 1.0f;
    const af::Mat4 sceneViewProjection = sceneCamera.viewProjection();
    const float sceneEye[3] = {
        sceneCamera.position.x, sceneCamera.position.y, sceneCamera.position.z
    };
    af_scene_draw sceneDraws[1]{};
    int32_t sceneDrawCount = 0, sceneCulledCount = 0;
    const int32_t sceneRc = bridge
        ? af_scene_bridge_update(bridge, engine, sceneViewProjection.m, sceneEye, 1.0f,
                                 sceneDraws, 1, &sceneDrawCount, &sceneCulledCount)
        : AF_ERR_OUT_OF_MEMORY;
    check(sceneRc == AF_OK && sceneDrawCount == 1 &&
          sceneDraws[0].instance == 0 && sceneDraws[0].index_count == 3,
          "imported GLB submesh reaches the renderer scene visibility draw list");
    af_scene_bridge_destroy(bridge);

    // 3. Foreign material maps to region -1 (AF_KIND_GENERIC), not REGION_SKIN
    check(sm[0].region == -1, "foreign unnamed material maps to region -1 (AF_KIND_GENERIC)");

    // 4. OPAQUE alpha normalization + MR texture populated + stale procedural slots cleared
    af_texture bc{}, mr{}, nm{}, sub{};
    check(af_engine_texture(engine, AF_TEX_BASE_COLOR, &bc) == AF_OK && bc.width == 2 && bc.data[3] == 255u,
          "OPAQUE baseColor forces alpha=255 on zero-alpha texels");
    check(af_engine_texture(engine, AF_TEX_METALLIC_ROUGHNESS, &mr) == AF_OK &&
          mr.width == 2 && mr.data[1] == 180u && mr.data[2] == 64u,
          "metallicRoughness texture slot populated with decoded G/B channels");
    check(af_engine_texture(engine, AF_TEX_NORMAL, &nm) != AF_OK,
          "unused normal texture slot cleared after procedural run");
    check(af_engine_texture(engine, AF_TEX_SUBSURFACE, &sub) != AF_OK,
          "unused subsurface texture slot cleared after procedural run");

    // 5. Fallback image decoder registration hook (used on Android API 26..29)
    af::setFallbackImageDecoder(&fakeFallbackWebpDecoder);
    af::setFallbackImageDecoder(nullptr);

    // 7. Optional real-file verification when passed on CLI (e.g. mylove.glb)
    if (argc > 1 && argv[1] && argv[1][0] != '\0') {
        check(af_engine_import_glb(engine, argv[1]) == AF_OK, "CLI real GLB asset imports cleanly");
        af_texture realBc{}, realMr{};
        check(af_engine_texture(engine, AF_TEX_BASE_COLOR, &realBc) == AF_OK && realBc.width > 0,
              "CLI real GLB baseColor decoded");
        check(af_engine_texture(engine, AF_TEX_METALLIC_ROUGHNESS, &realMr) == AF_OK && realMr.width > 0,
              "CLI real GLB metallicRoughness decoded");
    }

    af_engine_destroy(engine);
    if (gFailures == 0) {
        std::printf("ALL PASSED (0 failures)\n");
        return 0;
    }
    std::fprintf(stderr, "%d FAILURE(S)\n", gFailures);
    return 1;
}

