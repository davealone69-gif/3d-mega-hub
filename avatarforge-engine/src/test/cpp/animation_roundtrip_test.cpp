/*
 * Animation round trip: generate -> export -> import -> evaluate -> skin.
 *
 * The app never shows the engine that generated an avatar. It exports a GLB, and the viewer
 * imports that file into a second engine. This test follows the same route and checks that the
 * avatar which comes out of the import can be animated, and that it moves exactly as the one that
 * went in:
 *
 *   1. the exported file carries one skin, its inverse bind matrices, four joints and weights per
 *      vertex, and one animation per clip of the library;
 *   2. the import adopts the skeleton: same joint count, names, parents and rest transforms;
 *   3. at rest every skin matrix is the identity (the mesh is exported in its bind pose);
 *   4. for several clips and times, the bind-pose vertices of the IMPORTED avatar, blended with
 *      the skin matrices from af_engine_evaluate_animation (the arithmetic a vertex shader does),
 *      land where the GENERATING engine's own CPU skinning puts them;
 *   5. the animation channels stored in the file hold the same rotations the engine plays, so
 *      "plays the library clip of that name" and "plays the exported clip" are the same thing;
 *   6. clips differ from one another and from the rest pose, time is clamped, and bad arguments
 *      are refused;
 *   7. a file whose skeleton is not this engine's rig, and a file with no skeleton, are imported
 *      as static meshes that report no animations.
 *
 * The avatar is built at draft quality (a few seconds); the skeleton, weights and clips are the
 * same code path as a full-quality build.
 */
#include "afcore.h"
#include "af_glb_read.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

int gFailures = 0;
void check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++gFailures;
}

int gWarnings = 0;
std::string gLastWarning;
void onLog(void*, int severity, const char* message) {
    if (severity >= AF_LOG_WARN) { ++gWarnings; gLastWarning = message ? message : ""; }
}

struct V3 { float x, y, z; };

/* What a vertex shader does: blend the bind-pose position with four skin matrices. */
V3 skinPosition(const af_vertex& v, const std::vector<float>& m, int joints) {
    V3 p{0, 0, 0};
    float total = 0.0f;
    for (int k = 0; k < 4; ++k) {
        const float w = v.weights[k];
        if (w <= 0.0f || v.joints[k] >= joints) continue;
        const float* a = &m[static_cast<size_t>(v.joints[k]) * 16];
        p.x += w * (a[0] * v.pos[0] + a[4] * v.pos[1] + a[8] * v.pos[2] + a[12]);
        p.y += w * (a[1] * v.pos[0] + a[5] * v.pos[1] + a[9] * v.pos[2] + a[13]);
        p.z += w * (a[2] * v.pos[0] + a[6] * v.pos[1] + a[10] * v.pos[2] + a[14]);
        total += w;
    }
    if (total > 1e-6f) { p.x /= total; p.y /= total; p.z /= total; } else { p = {v.pos[0], v.pos[1], v.pos[2]}; }
    return p;
}

}  // namespace

int main() {
    af_spec spec;
    af_spec_init(&spec);
    spec.sex = AF_SEX_FEMALE;
    spec.build = AF_BUILD_ATHLETIC;
    spec.hair_style = AF_HAIR_MEDIUM;
    spec.outfit = AF_OUTFIT_TSHIRT;
    spec.seed = 1337;
    spec.height_m = 1.75f;
    af_callbacks cb{};
    cb.on_log = onLog;

    /* ---- the generating engine ---- */
    af_engine* gen = nullptr;
    check(af_engine_create(&spec, &cb, &gen) == AF_OK && gen, "create generating engine");
    check(af_engine_set_quality(gen, AF_QUALITY_DRAFT) == AF_OK, "draft quality accepted");
    check(af_engine_run_all(gen) == AF_OK, "generate (draft)");
    const int J = af_engine_joint_count(gen);
    const int V = af_engine_vertex_count(gen);
    std::printf("INFO generated vertices=%d triangles=%d joints=%d library_animations=%d\n", V,
                af_engine_index_count(gen) / 3, J, af_engine_animation_count(gen));
    check(J > 50 && V > 1000, "generated avatar has a skeleton and a mesh");
    check(af_engine_animation_count(gen) == AF_CLIP_COUNT - 1, "generating engine lists every library clip");

    uint8_t* bytes = nullptr;
    size_t size = 0;
    check(af_engine_export_glb_to_memory(gen, &bytes, &size) == AF_OK && bytes && size > 1000, "export GLB to memory");

    /* ---- 1. what the file carries ---- */
    af::GlbAsset asset;
    std::string error;
    check(af::importGlb(bytes, size, &asset, &error) == AF_OK, "exported file parses");
    check(asset.skins.size() == 1 && static_cast<int>(asset.skins[0].joints.size()) == J, "file has one skin with every joint");
    check(asset.skins.size() == 1 && asset.skins[0].inverseBindMatrices.size() == static_cast<size_t>(J),
          "file has one inverse bind matrix per joint");
    check(static_cast<int>(asset.animations.size()) == AF_CLIP_COUNT - 1, "file has one animation per library clip");
    {
        size_t weighted = 0, total = 0;
        for (const auto& pool : asset.pools) {
            for (const af_vertex& v : pool) {
                ++total;
                const float sum = v.weights[0] + v.weights[1] + v.weights[2] + v.weights[3];
                if (std::fabs(sum - 1.0f) < 1e-3f) ++weighted;
            }
        }
        check(total == static_cast<size_t>(V) && weighted == total, "every vertex in the file carries normalised skin weights");
    }

    /* ---- 2. the import adopts the skeleton ---- */
    af_engine* imp = nullptr;
    af_spec blank;
    af_spec_init(&blank);
    check(af_engine_create(&blank, &cb, &imp) == AF_OK && imp, "create importing engine");
    gWarnings = 0;
    check(af_engine_import_glb_from_memory(imp, bytes, size) == AF_OK, "import the exported file");
    check(gWarnings == 0, "import of the engine's own file raises no warning");
    check(af_engine_joint_count(imp) == J && af_engine_skin_joint_count(imp) == J, "imported avatar keeps every joint");
    check(af_engine_vertex_count(imp) == V, "imported avatar keeps every vertex");
    const int A = af_engine_animation_count(imp);
    check(A == static_cast<int>(asset.animations.size()), "animation count comes from the file");
    {
        bool same = true;
        for (int j = 0; j < J && same; ++j) {
            af_joint_info a{}, b{};
            same = af_engine_joint_info(gen, j, &a) == AF_OK && af_engine_joint_info(imp, j, &b) == AF_OK &&
                   std::strcmp(a.name, b.name) == 0 && a.parent == b.parent;
            for (int k = 0; k < 3 && same; ++k) {
                /* The exporter grounds the model: the root's rest translation absorbs the offset. */
                if (a.parent < 0 && k == 1) continue;
                same = std::fabs(a.rest_translation[k] - b.rest_translation[k]) < 1e-5f;
            }
        }
        check(same, "joint names, parents and rest translations survive the round trip");
    }
    {
        bool namesOk = true;
        for (int i = 0; i < A && namesOk; ++i) {
            char name[64] = {0};
            namesOk = af_engine_animation_name(imp, i, name, sizeof(name)) > 0 &&
                      asset.animations[static_cast<size_t>(i)].name == name &&
                      af_engine_animation_duration(imp, i) > 0.0f &&
                      std::fabs(af_engine_animation_duration(imp, i) - asset.animations[static_cast<size_t>(i)].duration) < 1e-3f;
        }
        check(namesOk, "animation names and durations match the file, in the file's order");
    }

    const af_vertex* rest = af_engine_vertices_rest(imp);
    const af_vertex* genRest = af_engine_vertices_rest(gen);
    /* Ground offset applied by the exporter (a pure translation along Y). */
    const float dy = rest[0].pos[1] - genRest[0].pos[1];
    {
        double worst = 0.0;
        for (int i = 0; i < V; ++i) {
            worst = std::max(worst, static_cast<double>(std::fabs(rest[i].pos[0] - genRest[i].pos[0])));
            worst = std::max(worst, static_cast<double>(std::fabs(rest[i].pos[1] - genRest[i].pos[1] - dy)));
            worst = std::max(worst, static_cast<double>(std::fabs(rest[i].pos[2] - genRest[i].pos[2])));
        }
        std::printf("INFO bind pose: export ground offset %.6f m, worst vertex difference %.3g m\n", dy, worst);
        check(worst < 1e-5, "bind-pose vertices are identical up to the export's ground offset");
    }

    /* ---- 3. rest pose = identity skin matrices ---- */
    std::vector<float> m(static_cast<size_t>(J) * 16);
    check(af_engine_evaluate_animation(imp, -1, 0.0f, m.data(), J) == J, "evaluate rest pose");
    {
        double worst = 0.0;
        for (int j = 0; j < J; ++j) {
            for (int k = 0; k < 16; ++k) {
                const float expect = (k % 5 == 0) ? 1.0f : 0.0f;
                worst = std::max(worst, static_cast<double>(std::fabs(m[static_cast<size_t>(j) * 16 + k] - expect)));
            }
        }
        std::printf("INFO rest pose: worst deviation of a skin matrix from identity %.3g\n", worst);
        check(worst < 2e-5, "rest-pose skin matrices are the identity");
    }

    /* ---- 4. imported + evaluated + skinned == generating engine's own skinning ---- */
    double worstAll = 0.0, leastMotion = 1e9;
    int comparisons = 0;
    for (int i = 0; i < A; ++i) {
        char name[64] = {0};
        af_engine_animation_name(imp, i, name, sizeof(name));
        /* Library id of this animation in the generating engine: same name, same order as af_clip. */
        int clipId = -1;
        for (int id = 1; id < AF_CLIP_COUNT; ++id) {
            char n2[64] = {0};
            if (af_engine_clip_name(id, n2, sizeof(n2)) > 0 && std::strcmp(n2, name) == 0) { clipId = id; break; }
        }
        if (clipId < 0) { check(false, "every imported animation names a library clip"); continue; }
        const float duration = af_engine_animation_duration(imp, i);
        for (float f : {0.0f, 0.37f, 0.80f}) {
            const float t = duration * f;
            if (af_engine_apply_clip(gen, clipId, t) != AF_OK) { check(false, "generating engine applies the clip"); continue; }
            if (af_engine_evaluate_animation(imp, i, t, m.data(), J) != J) { check(false, "imported engine evaluates the clip"); continue; }
            const af_vertex* posed = af_engine_vertices(gen);
            double worst = 0.0, moved = 0.0;
            for (int v = 0; v < V; ++v) {
                const V3 p = skinPosition(rest[v], m, J);
                const double dx = p.x - posed[v].pos[0], dyy = p.y - (posed[v].pos[1] + dy), dz = p.z - posed[v].pos[2];
                worst = std::max(worst, std::sqrt(dx * dx + dyy * dyy + dz * dz));
                const double mx = p.x - rest[v].pos[0], my = p.y - rest[v].pos[1], mz = p.z - rest[v].pos[2];
                moved = std::max(moved, std::sqrt(mx * mx + my * my + mz * mz));
            }
            worstAll = std::max(worstAll, worst);
            if (f > 0.0f) leastMotion = std::min(leastMotion, moved);
            ++comparisons;
            std::printf("INFO clip %-9s t=%.3fs  worst difference %.3g m  largest displacement from bind pose %.3f m\n",
                        name, t, worst, moved);
        }
    }
    check(comparisons == A * 3 && worstAll < 2e-4,
          "imported avatar, skinned with the evaluated matrices, matches the generating engine (0.2 mm)");
    check(leastMotion > 0.01, "every clip moves the avatar by more than a centimetre");
    af_engine_apply_clip(gen, AF_CLIP_REST, 0.0f);

    /* ---- 5. the file's channels hold what the engine plays ---- */
    {
        double worst = 0.0;
        size_t compared = 0;
        std::vector<float> quats(static_cast<size_t>(J) * 4);
        af_pose pose{quats.data(), J};
        for (size_t ai = 0; ai < asset.animations.size(); ++ai) {
            const af::ImportedAnimation& anim = asset.animations[ai];
            int clipId = -1;
            for (int id = 1; id < AF_CLIP_COUNT; ++id) {
                char n2[64] = {0};
                if (af_engine_clip_name(id, n2, sizeof(n2)) > 0 && anim.name == n2) { clipId = id; break; }
            }
            if (clipId < 0) continue;
            for (const af::ImportedAnimationChannel& ch : anim.channels) {
                if (ch.path != 1) continue;   /* rotations */
                /* Node index == joint index in the engine's own files. */
                const int joint = ch.node;
                if (joint < 0 || joint >= J) continue;
                for (size_t k = 0; k < ch.times.size(); k += std::max<size_t>(1, ch.times.size() / 7)) {
                    /* The last key of a looping clip is its first frame again; the sampler wraps it. */
                    if (ch.times[k] >= anim.duration) continue;
                    if (af_engine_sample_clip(gen, clipId, ch.times[k], &pose) != AF_OK) continue;
                    const float* q = &quats[static_cast<size_t>(joint) * 4];
                    const af::Vec4& f = ch.values[k];
                    const float dot = q[0] * f.x + q[1] * f.y + q[2] * f.z + q[3] * f.w;
                    worst = std::max(worst, static_cast<double>(1.0f - std::fabs(dot)));
                    ++compared;
                }
            }
        }
        std::printf("INFO file channels vs engine playback: %zu keys compared, worst 1-|dot| %.3g\n", compared, worst);
        check(compared > 200 && worst < 1e-5, "rotation keys stored in the file equal the rotations the engine plays");
    }

    /* ---- 6. behaviour of the evaluation call ---- */
    {
        std::vector<float> a(m.size()), b(m.size());
        int idle = -1, walk = -1;
        for (int i = 0; i < A; ++i) {
            char name[64] = {0};
            af_engine_animation_name(imp, i, name, sizeof(name));
            if (std::strcmp(name, "idle") == 0) idle = i;
            if (std::strcmp(name, "walk") == 0) walk = i;
        }
        check(idle >= 0 && walk >= 0, "the file offers an idle clip and a walk clip");
        if (idle >= 0 && walk >= 0) {
            af_engine_evaluate_animation(imp, idle, 0.4f, a.data(), J);
            af_engine_evaluate_animation(imp, walk, 0.4f, b.data(), J);
            double diff = 0.0;
            for (size_t k = 0; k < a.size(); ++k) diff = std::max(diff, static_cast<double>(std::fabs(a[k] - b[k])));
            check(diff > 0.05, "idle and walk produce different poses at the same time");

            const float d = af_engine_animation_duration(imp, walk);
            af_engine_evaluate_animation(imp, walk, d * 0.25f, a.data(), J);
            af_engine_evaluate_animation(imp, walk, d * 0.75f, b.data(), J);
            diff = 0.0;
            for (size_t k = 0; k < a.size(); ++k) diff = std::max(diff, static_cast<double>(std::fabs(a[k] - b[k])));
            check(diff > 0.05, "seeking within a clip changes the pose");

            af_engine_evaluate_animation(imp, walk, -5.0f, a.data(), J);
            af_engine_evaluate_animation(imp, walk, 0.0f, b.data(), J);
            check(std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0, "time below zero is clamped to the start");
            af_engine_evaluate_animation(imp, walk, d + 100.0f, a.data(), J);
            af_engine_evaluate_animation(imp, walk, d, b.data(), J);
            check(std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0, "time beyond the end is clamped to the end");
            /* A looping clip ends where it starts: the loop boundary has no jump. */
            af_engine_evaluate_animation(imp, walk, 0.0f, a.data(), J);
            diff = 0.0;
            for (size_t k = 0; k < a.size(); ++k) diff = std::max(diff, static_cast<double>(std::fabs(a[k] - b[k])));
            std::printf("INFO walk: largest matrix difference between t=0 and t=duration %.3g\n", diff);
            check(diff < 0.02, "a looping clip's last pose equals its first");
            af_engine_evaluate_animation(imp, walk, std::nanf(""), a.data(), J);
            bool finite = true;
            for (float x : a) finite = finite && std::isfinite(x);
            check(finite, "a NaN time yields finite matrices");
        }
        check(af_engine_evaluate_animation(imp, A, 0.0f, m.data(), J) == AF_ERR_INVALID_ARG, "animation index out of range is refused");
        check(af_engine_evaluate_animation(imp, 0, 0.0f, m.data(), J - 1) == AF_ERR_INVALID_ARG, "too small an output buffer is refused");
        check(af_engine_evaluate_animation(imp, 0, 0.0f, nullptr, J) == AF_ERR_INVALID_ARG, "a null output buffer is refused");
        check(af_engine_evaluate_animation(nullptr, 0, 0.0f, m.data(), J) == AF_ERR_INVALID_ARG, "a null engine is refused");
        check(af_engine_animation_name(imp, A, nullptr, 0) == 0 && af_engine_animation_duration(imp, -1) == 0.0f,
              "names and durations of unknown animations are empty");

        /* The CPU call must really deform an imported avatar as well. */
        if (walk >= 0) {
            char name[64] = {0};
            int clipId = -1;
            for (int id = 1; id < AF_CLIP_COUNT; ++id) {
                if (af_engine_clip_name(id, name, sizeof(name)) > 0 && std::strcmp(name, "walk") == 0) { clipId = id; break; }
            }
            const float t = 0.4f;
            check(af_engine_apply_clip(imp, clipId, t) == AF_OK, "CPU apply_clip works on an imported avatar");
            af_engine_evaluate_animation(imp, walk, t, m.data(), J);
            const af_vertex* posed = af_engine_vertices(imp);
            double worst = 0.0, moved = 0.0;
            for (int v = 0; v < V; ++v) {
                const V3 p = skinPosition(rest[v], m, J);
                worst = std::max(worst, static_cast<double>(std::fabs(p.x - posed[v].pos[0]) + std::fabs(p.y - posed[v].pos[1]) + std::fabs(p.z - posed[v].pos[2])));
                moved = std::max(moved, static_cast<double>(std::fabs(posed[v].pos[0] - rest[v].pos[0])));
            }
            check(worst < 1e-5 && moved > 0.01, "CPU skinning of the imported avatar equals the evaluated matrices");
            char report[1024] = {0};
            check(af_engine_validate_deformed_mesh(imp, report, sizeof(report)) == AF_OK, "posed imported mesh validates");
            af_engine_apply_clip(imp, AF_CLIP_REST, 0.0f);
        }
    }

    /* ---- 7. files that cannot be animated say so ---- */
    {
        /* Same file, with every joint's name altered: the skeleton is no longer this engine's rig. */
        std::vector<uint8_t> foreign(bytes, bytes + size);
        uint32_t jsonLen = 0;
        std::memcpy(&jsonLen, foreign.data() + 12, 4);
        const char* key = "\"name\":\"";
        size_t renamed = 0;
        for (size_t i = 20; i + 9 < 20 + static_cast<size_t>(jsonLen); ++i) {
            if (std::memcmp(foreign.data() + i, key, 8) == 0) {
                uint8_t& c = foreign[i + 8];
                if (c >= 'a' && c <= 'z') { c = static_cast<uint8_t>(c - 'a' + 'A'); ++renamed; }
            }
        }
        af_engine* other = nullptr;
        af_engine_create(&blank, &cb, &other);
        gWarnings = 0;
        check(renamed > static_cast<size_t>(J) && af_engine_import_glb_from_memory(other, foreign.data(), foreign.size()) == AF_OK,
              "a file with a foreign skeleton still imports");
        check(af_engine_joint_count(other) == 0 && af_engine_animation_count(other) == 0, "a foreign skeleton is not adopted");
        check(gWarnings > 0, "the reason is reported as a warning");
        check(af_engine_evaluate_animation(other, 0, 0.0f, m.data(), J) == 0, "evaluating a static model writes no matrices");
        check(af_engine_apply_clip(other, AF_CLIP_IDLE, 0.0f) == AF_ERR_NOT_READY, "CPU apply_clip refuses a static model");
        std::printf("INFO foreign skeleton: %zu names altered, warning: %s\n", renamed, gLastWarning.c_str());
        af_engine_destroy(other);
    }
    {
        /* Same file again, keeping only the two most common joint names, "root" and "head". Sharing a
           couple of names with this rig does not make a skeleton this rig: small rigged test models
           in the owner's other repositories use exactly those names. Adopting such a file would
           offer the library's clips in place of the file's own animation. */
        std::vector<uint8_t> partial(bytes, bytes + size);
        uint32_t jsonLen = 0;
        std::memcpy(&jsonLen, partial.data() + 12, 4);
        const char* key = "\"name\":\"";
        size_t renamed = 0, kept = 0;
        for (size_t i = 20; i + 14 < 20 + static_cast<size_t>(jsonLen); ++i) {
            if (std::memcmp(partial.data() + i, key, 8) != 0) continue;
            const char* name = reinterpret_cast<const char*>(partial.data() + i + 8);
            if (std::memcmp(name, "root\"", 5) == 0 || std::memcmp(name, "head\"", 5) == 0) { ++kept; continue; }
            uint8_t& c = partial[i + 8];
            if (c >= 'a' && c <= 'z') { c = static_cast<uint8_t>(c - 'a' + 'A'); ++renamed; }
        }
        af_engine* other = nullptr;
        af_engine_create(&blank, &cb, &other);
        gWarnings = 0;
        check(kept >= 2 && renamed >= static_cast<size_t>(J) - 2 &&
              af_engine_import_glb_from_memory(other, partial.data(), partial.size()) == AF_OK,
              "a file sharing only two joint names still imports");
        check(af_engine_joint_count(other) == 0 && af_engine_animation_count(other) == 0,
              "a skeleton that shares only some joint names is not adopted");
        check(gWarnings > 0, "the partial match is reported as a warning");
        std::printf("INFO partial skeleton: %zu names kept, %zu altered, warning: %s\n", kept, renamed, gLastWarning.c_str());
        af_engine_destroy(other);
    }

    af_arena_free(bytes);
    af_engine_destroy(imp);
    af_engine_destroy(gen);
    std::printf("%s animation round trip: %d failure(s)\n", gFailures == 0 ? "RESULT PASS" : "RESULT FAIL", gFailures);
    return gFailures == 0 ? 0 : 1;
}
