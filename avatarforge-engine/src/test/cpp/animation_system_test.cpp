/*
 * The animation system of the engine, checked on real data: a generated avatar, the file the app
 * would write for it, and two rigged GLB files from outside the engine.
 *
 *   usage: af_animation_system_test <directory with humanoid_motion_pack.glb and rigged_test_figure.glb>
 *
 * The avatar goes the way it goes in the app: generated, exported, and imported into a second
 * engine, which is the one the renderer shows. Everything below runs on that second engine unless
 * it says otherwise.
 *
 *   SKELETON AND CLIPS   every animation gives rigid, finite matrices; looping ones end where
 *                        they start; a squat keeps the feet on the floor; a walk alternates feet.
 *   SKIN WEIGHTS         no vertex is pulled by an arm and a leg; a shin vertex follows the leg.
 *   BLENDING AND LAYERS  weight 0 and 1 give the single clips; a crossfade lies between; a masked
 *                        layer moves the joints it covers and leaves the others to the base clip.
 *   RETARGETING          humanoid_motion_pack.glb (Mixamo names, T pose, centimetres, bone-aligned
 *                        joints, turned 25 degrees: tools/glb_figures/make_humanoid_pack.py) drives
 *                        the avatar: arms overhead, the LEFT knee lifted, a quarter turn to the
 *                        left, a hop of the right height, a stride whose travel is taken out.
 *                        rigged_test_figure.glb (seven joints) is refused with the reason.
 *   FACE                 the face rig survives the file; its packed form (what the vertex shader
 *                        evaluates) equals the engine's own morph offsets; jawOpen parts the lips;
 *                        channels add up; a file from elsewhere has no face rig.
 */
#include "afcore.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

int gFailures = 0;
void check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++gFailures;
}
void check(bool ok, const std::string& what) { check(ok, what.c_str()); }

void onLog(void*, int, const char*) {}

struct V3 {
    float x = 0, y = 0, z = 0;
    V3() = default;
    V3(float a, float b, float c) : x(a), y(b), z(c) {}
};
V3 operator-(const V3& a, const V3& b) { return V3(a.x - b.x, a.y - b.y, a.z - b.z); }
V3 operator+(const V3& a, const V3& b) { return V3(a.x + b.x, a.y + b.y, a.z + b.z); }
float length(const V3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

V3 transform(const float* m, const V3& p) {
    return V3(m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12], m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
              m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]);
}

V3 rotateByQuat(const float* q, const V3& v) {
    /* v + 2 * cross(q.xyz, cross(q.xyz, v) + q.w * v) */
    const V3 u(q[0], q[1], q[2]);
    const V3 c1(u.y * v.z - u.z * v.y + q[3] * v.x, u.z * v.x - u.x * v.z + q[3] * v.y, u.x * v.y - u.y * v.x + q[3] * v.z);
    const V3 c2(u.y * c1.z - u.z * c1.y, u.z * c1.x - u.x * c1.z, u.x * c1.y - u.y * c1.x);
    return V3(v.x + 2.0f * c2.x, v.y + 2.0f * c2.y, v.z + 2.0f * c2.z);
}

void multiplyQuat(const float* a, const float* b, float* out) {
    out[0] = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
    out[1] = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
    out[2] = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
    out[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
}

/* A general 4x4 inverse (column-major), for "this joint relative to its parent". */
bool invert(const float* m, float* out) {
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    const float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (std::fabs(det) < 1e-12f) return false;
    for (int i = 0; i < 16; ++i) out[i] = inv[i] / det;
    return true;
}

void multiply(const float* a, const float* b, float* out) {
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            out[c * 4 + r] = a[r] * b[c * 4] + a[4 + r] * b[c * 4 + 1] + a[8 + r] * b[c * 4 + 2] + a[12 + r] * b[c * 4 + 3];
        }
    }
}

float maxDifference(const float* a, const float* b, int count) {
    float worst = 0.0f;
    for (int i = 0; i < count; ++i) worst = std::max(worst, std::fabs(a[i] - b[i]));
    return worst;
}

std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

/* The model one engine holds, with what a test needs to ask about its joints. */
struct Model {
    af_engine* engine = nullptr;
    int joints = 0;
    std::vector<af_joint_info> info;
    std::vector<V3> rest;                 /* where each joint is in the bind pose */

    void read(af_engine* e) {
        engine = e;
        joints = af_engine_joint_count(e);
        info.assign(static_cast<size_t>(std::max(joints, 0)), af_joint_info{});
        rest.assign(info.size(), V3());
        std::vector<float> worldRot(info.size() * 4, 0.0f);
        for (int j = 0; j < joints; ++j) {
            af_engine_joint_info(e, j, &info[static_cast<size_t>(j)]);
            const af_joint_info& ji = info[static_cast<size_t>(j)];
            const V3 t(ji.rest_translation[0], ji.rest_translation[1], ji.rest_translation[2]);
            float* q = &worldRot[static_cast<size_t>(j) * 4];
            if (ji.parent < 0) {
                rest[static_cast<size_t>(j)] = t;
                std::memcpy(q, ji.rest_rotation, 4 * sizeof(float));
            } else {
                const float* pq = &worldRot[static_cast<size_t>(ji.parent) * 4];
                rest[static_cast<size_t>(j)] = rest[static_cast<size_t>(ji.parent)] + rotateByQuat(pq, t);
                multiplyQuat(pq, ji.rest_rotation, q);
            }
        }
    }
    int joint(const char* name) const {
        for (int j = 0; j < joints; ++j) if (std::strcmp(info[static_cast<size_t>(j)].name, name) == 0) return j;
        return -1;
    }
    int animation(const char* name) const {
        const int n = af_engine_animation_count(engine);
        for (int a = 0; a < n; ++a) {
            char buffer[64] = {0};
            af_engine_animation_name(engine, a, buffer, sizeof(buffer));
            if (std::strcmp(buffer, name) == 0) return a;
        }
        return -1;
    }
    std::vector<float> pose(int animation, float t) const {
        std::vector<float> m(static_cast<size_t>(joints) * 16, 0.0f);
        af_engine_evaluate_animation(engine, animation, t, m.data(), joints);
        return m;
    }
    std::vector<float> layers(const std::vector<af_animation_layer>& l) const {
        std::vector<float> m(static_cast<size_t>(joints) * 16, 0.0f);
        af_engine_evaluate_layers(engine, l.data(), static_cast<int32_t>(l.size()), m.data(), joints);
        return m;
    }
    /* Where a joint is under a pose: its skin matrix moves its bind position. */
    V3 at(const std::vector<float>& m, int j) const { return transform(&m[static_cast<size_t>(j) * 16], rest[static_cast<size_t>(j)]); }
    V3 at(const std::vector<float>& m, const char* name) const { return at(m, joint(name)); }
    /* The joint's own motion, with its parent's taken out. */
    void relative(const std::vector<float>& m, int j, float* out) const {
        const int parent = info[static_cast<size_t>(j)].parent;
        if (parent < 0) { std::memcpy(out, &m[static_cast<size_t>(j) * 16], 16 * sizeof(float)); return; }
        float inv[16];
        if (!invert(&m[static_cast<size_t>(parent) * 16], inv)) { std::memset(out, 0, 16 * sizeof(float)); return; }
        multiply(inv, &m[static_cast<size_t>(j) * 16], out);
    }
};

bool rigid(const std::vector<float>& m, int joints) {
    for (int j = 0; j < joints; ++j) {
        const float* a = &m[static_cast<size_t>(j) * 16];
        for (int k = 0; k < 16; ++k) if (!std::isfinite(a[k])) return false;
        const float det = a[0] * (a[5] * a[10] - a[9] * a[6]) - a[4] * (a[1] * a[10] - a[9] * a[2]) + a[8] * (a[1] * a[6] - a[5] * a[2]);
        if (std::fabs(det - 1.0f) > 2e-3f) return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: %s <directory with humanoid_motion_pack.glb and rigged_test_figure.glb>\n", argv[0]);
        return 2;
    }
    const std::string assets = argv[1];
    const std::vector<uint8_t> pack = readFile(assets + "/humanoid_motion_pack.glb");
    const std::vector<uint8_t> sevenJoints = readFile(assets + "/rigged_test_figure.glb");
    check(pack.size() > 1000 && sevenJoints.size() > 1000, "both test files are there");
    if (pack.empty() || sevenJoints.empty()) return 1;

    af_spec spec;
    af_spec_init(&spec);
    spec.sex = AF_SEX_FEMALE;
    spec.build = AF_BUILD_ATHLETIC;
    spec.hair_style = AF_HAIR_MEDIUM;
    spec.outfit = AF_OUTFIT_TSHIRT;
    spec.seed = 4242;
    spec.height_m = 1.70f;
    af_callbacks cb{};
    cb.on_log = onLog;

    af_engine* gen = nullptr;
    check(af_engine_create(&spec, &cb, &gen) == AF_OK && gen, "create the generating engine");
    af_engine_set_quality(gen, AF_QUALITY_DRAFT);
    check(af_engine_run_all(gen) == AF_OK, "generate an avatar (draft)");
    uint8_t* bytes = nullptr;
    size_t size = 0;
    check(af_engine_export_glb_to_memory(gen, &bytes, &size) == AF_OK && bytes && size > 1000, "export it");
    af_engine* shown = nullptr;
    check(af_engine_create(&spec, &cb, &shown) == AF_OK && shown, "create the engine the renderer would hold");
    check(af_engine_import_glb_from_memory(shown, bytes, size) == AF_OK, "import the exported file");

    Model m;
    m.read(shown);
    const int J = m.joints;
    const int builtIn = af_engine_animation_count(shown);
    std::printf("INFO joints=%d vertices=%d built-in animations=%d\n", J, af_engine_vertex_count(shown), builtIn);
    check(J > 50 && builtIn == AF_CLIP_COUNT - 1, "the imported avatar has its skeleton and every built-in clip");

    /* ================================================================ skeleton and clips ==== */
    {
        bool allRigid = true, loopsClose = true, infoSane = true;
        float worstLoop = 0.0f;
        for (int a = 0; a < builtIn; ++a) {
            af_animation_info info{};
            if (af_engine_animation_info(shown, a, &info) != AF_OK || info.duration <= 0.0f || info.frames < 2 || info.source != 0 ||
                info.name[0] == 0 || std::fabs(info.duration - af_engine_animation_duration(shown, a)) > 1e-6f) infoSane = false;
            for (int k = 0; k <= 4; ++k) {
                if (!rigid(m.pose(a, info.duration * static_cast<float>(k) / 4.0f), J)) allRigid = false;
            }
            if (info.looping) {
                const std::vector<float> first = m.pose(a, 0.0f), last = m.pose(a, info.duration);
                for (int j = 0; j < J; ++j) worstLoop = std::max(worstLoop, length(m.at(first, j) - m.at(last, j)));
            }
        }
        if (worstLoop > 0.002f) loopsClose = false;
        std::printf("INFO looping clips: the last pose is within %.2f mm of the first\n", worstLoop * 1000.0f);
        check(infoSane, "every built-in animation reports a name, a duration and its frames");
        check(allRigid, "every built-in animation gives finite, rigid joint matrices at five times");
        check(loopsClose, "every clip marked as looping ends where it starts (2 mm)");
        for (const char* name : {"idle", "walk", "run", "jump", "dance", "wave", "hold_hat"}) {
            check(m.animation(name) >= 0, std::string("built-in clip '") + name + "' is offered");
        }
        af_animation_info idle{}, jump{};
        af_engine_animation_info(shown, m.animation("idle"), &idle);
        af_engine_animation_info(shown, m.animation("jump"), &jump);
        check(idle.looping == 1 && jump.looping == 0 && jump.moves_root == 1, "idle loops; jump does not, and moves the root");

        /* Squat: the hips come down by more than a third of a metre and the feet stay put. */
        const int squat = m.animation("squat");
        const float squatMid = af_engine_animation_duration(shown, squat) * 0.5f;
        const std::vector<float> rest = m.pose(-1, 0.0f), bottom = m.pose(squat, squatMid);
        const float hipsDrop = m.at(rest, "hips").y - m.at(bottom, "hips").y;
        const float footL = std::fabs(m.at(bottom, "foot_L").y - m.at(rest, "foot_L").y);
        const float footR = std::fabs(m.at(bottom, "foot_R").y - m.at(rest, "foot_R").y);
        std::printf("INFO squat: hips drop %.3f m, ankles move %.1f mm and %.1f mm vertically\n", hipsDrop, footL * 1000.0f, footR * 1000.0f);
        check(hipsDrop > 0.33f, "squat lowers the hips by more than a third of a metre");
        check(footL < 0.03f && footR < 0.03f, "squat keeps both ankles within 3 cm of where they stand");

        /* Walk: half a cycle apart, the other foot is in front. The figure faces -Z. */
        const int walk = m.animation("walk");
        const float period = af_engine_animation_duration(shown, walk);
        float bestLead = 0.0f, bestTime = 0.0f;
        for (int k = 0; k < 20; ++k) {
            const float t = period * static_cast<float>(k) / 20.0f;
            const std::vector<float> p = m.pose(walk, t);
            const float lead = m.at(p, "foot_R").z - m.at(p, "foot_L").z;      /* > 0: the _L foot is in front */
            if (lead > bestLead) { bestLead = lead; bestTime = t; }
        }
        const std::vector<float> opposite = m.pose(walk, std::fmod(bestTime + period * 0.5f, period));
        const float otherLead = m.at(opposite, "foot_L").z - m.at(opposite, "foot_R").z;
        std::printf("INFO walk: one foot leads by %.3f m, half a cycle later the other by %.3f m\n", bestLead, otherLead);
        check(bestLead > 0.25f && otherLead > 0.25f && std::fabs(bestLead - otherLead) < 0.05f, "walk puts each foot in front in turn, by the same stride");
        std::vector<float> scratch(static_cast<size_t>(J) * 16, 0.0f);
        check(af_engine_evaluate_animation(shown, builtIn, 0.0f, scratch.data(), J) < 0 && af_engine_evaluate_animation(shown, 0, 0.0f, nullptr, J) < 0 &&
              af_engine_evaluate_animation(shown, 0, 0.0f, scratch.data(), J - 1) < 0, "an animation index past the end, a null output and a short output are refused");
    }

    /* ================================================================ skin weights ========== */
    {
        /* Which joints belong to an arm and which to a leg, by name. */
        std::vector<char> arm(static_cast<size_t>(J), 0), leg(static_cast<size_t>(J), 0);
        for (int j = 0; j < J; ++j) {
            const std::string name = m.info[static_cast<size_t>(j)].name;
            auto has = [&](const char* part) { return name.find(part) != std::string::npos; };
            if (has("upperarm") || has("forearm") || has("hand") || has("thumb") || has("index") || has("middle") || has("ring") || has("pinky")) arm[static_cast<size_t>(j)] = 1;
            if (has("thigh") || has("shin") || has("foot") || has("toe")) leg[static_cast<size_t>(j)] = 1;
        }
        const af_vertex* v = af_engine_vertices_rest(shown) ? af_engine_vertices_rest(shown) : af_engine_vertices(shown);
        const int V = af_engine_vertex_count(shown);
        int both = 0, unweighted = 0, shinVertices = 0, shinFollowsLeg = 0;
        const V3 knee = m.rest[static_cast<size_t>(m.joint("shin_L"))], ankle = m.rest[static_cast<size_t>(m.joint("foot_L"))];
        for (int i = 0; i < V && v; ++i) {
            float armWeight = 0.0f, legWeight = 0.0f, total = 0.0f;
            for (int k = 0; k < 4; ++k) {
                const int j = v[i].joints[k];
                if (j >= J) continue;
                total += v[i].weights[k];
                if (arm[static_cast<size_t>(j)]) armWeight += v[i].weights[k];
                if (leg[static_cast<size_t>(j)]) legWeight += v[i].weights[k];
            }
            if (total < 0.99f) ++unweighted;
            if (armWeight > 0.02f && legWeight > 0.02f) ++both;
            /* A vertex in the middle third of the _L shin, near its axis. */
            const float along = (v[i].pos[1] - ankle.y) / (knee.y - ankle.y);
            if (along > 0.35f && along < 0.65f && std::fabs(v[i].pos[0] - knee.x) < 0.07f && std::fabs(v[i].pos[2] - knee.z) < 0.09f) {
                ++shinVertices;
                if (legWeight > 0.98f) ++shinFollowsLeg;
            }
        }
        std::printf("INFO weights: %d vertices, %d with arm and leg weight, %d mid-shin vertices of which %d follow only the leg\n", V, both, shinVertices, shinFollowsLeg);
        check(v != nullptr && unweighted == 0, "every vertex has weights that add up to one");
        check(both == 0, "no vertex is pulled by an arm joint and a leg joint");
        check(shinVertices > 20 && shinFollowsLeg == shinVertices, "the vertices of the mid shin follow only leg joints");
    }

    /* ================================================================ blending and layers === */
    {
        const int walk = m.animation("walk"), run = m.animation("run"), wave = m.animation("wave");
        const std::vector<float> walkPose = m.pose(walk, 0.3f), runPose = m.pose(run, 0.2f), wavePose = m.pose(wave, 1.0f);
        const int n = J * 16;
        check(maxDifference(m.layers({{walk, 0.3f, 1.0f, AF_MASK_FULL_BODY}}).data(), walkPose.data(), n) < 1e-5f, "one layer at weight 1 is the clip itself");
        check(maxDifference(m.layers({{walk, 0.3f, 1.0f, AF_MASK_FULL_BODY}, {run, 0.2f, 0.0f, AF_MASK_FULL_BODY}}).data(), walkPose.data(), n) < 1e-5f,
              "a second layer at weight 0 changes nothing");
        check(maxDifference(m.layers({{walk, 0.3f, 1.0f, AF_MASK_FULL_BODY}, {run, 0.2f, 1.0f, AF_MASK_FULL_BODY}}).data(), runPose.data(), n) < 1e-5f,
              "a second layer at weight 1 replaces the first");
        const std::vector<float> half = m.layers({{walk, 0.3f, 1.0f, AF_MASK_FULL_BODY}, {run, 0.2f, 0.5f, AF_MASK_FULL_BODY}});
        check(rigid(half, J), "a crossfade at one half gives rigid matrices");
        {
            /* Between the two: each joint's own turn is half-way, so it is as far from one clip as from the other. */
            bool between = true;
            float worst = 0.0f;
            for (int j = 0; j < J; ++j) {
                float a[16], b[16], h[16];
                m.relative(walkPose, j, a); m.relative(runPose, j, b); m.relative(half, j, h);
                const float span = maxDifference(a, b, 16), toA = maxDifference(h, a, 16), toB = maxDifference(h, b, 16);
                if (span < 0.02f) continue;
                worst = std::max(worst, std::max(toA, toB) / span);
                if (toA > span * 0.85f || toB > span * 0.85f) between = false;
            }
            std::printf("INFO crossfade at 0.5: no joint is further than %.0f %% of the way from either clip\n", worst * 100.0f);
            check(between, "a crossfade at one half lies between the two clips, joint by joint");
        }
        /* A wave over a walk: the upper body waves, the rest walks. */
        const std::vector<float> mixed = m.layers({{walk, 0.3f, 1.0f, AF_MASK_FULL_BODY}, {wave, 1.0f, 1.0f, AF_MASK_UPPER_BODY}});
        float worstCovered = 0.0f, worstOther = 0.0f;
        int covered = 0;
        for (int j = 0; j < J; ++j) {
            float a[16], b[16];
            m.relative(mixed, j, a);
            if (af_engine_joint_mask_weight(shown, AF_MASK_UPPER_BODY, j) > 0.5f) {
                m.relative(wavePose, j, b);
                worstCovered = std::max(worstCovered, maxDifference(a, b, 16));
                ++covered;
            } else {
                m.relative(walkPose, j, b);
                worstOther = std::max(worstOther, maxDifference(a, b, 16));
            }
        }
        std::printf("INFO wave over walk: %d of %d joints covered; covered differ from the wave by %.2g, the others from the walk by %.2g\n",
                    covered, J, worstCovered, worstOther);
        check(covered > 10 && covered < J - 10, "the upper-body mask covers part of the skeleton");
        check(worstCovered < 2e-4f, "joints under the mask move as the layered clip says");
        check(worstOther < 2e-4f, "joints outside the mask move as the base clip says");
        check(length(m.at(mixed, "foot_L") - m.at(walkPose, "foot_L")) < 1e-4f && length(m.at(mixed, "foot_R") - m.at(walkPose, "foot_R")) < 1e-4f,
              "the feet are where the walk alone puts them");

        /* The masks themselves. */
        bool partition = true, armsSplit = true, headInUpper = true;
        for (int j = 0; j < J; ++j) {
            const float up = af_engine_joint_mask_weight(shown, AF_MASK_UPPER_BODY, j), low = af_engine_joint_mask_weight(shown, AF_MASK_LOWER_BODY, j);
            const float l = af_engine_joint_mask_weight(shown, AF_MASK_LEFT_ARM, j), r = af_engine_joint_mask_weight(shown, AF_MASK_RIGHT_ARM, j);
            const float arms = af_engine_joint_mask_weight(shown, AF_MASK_ARMS, j), head = af_engine_joint_mask_weight(shown, AF_MASK_HEAD, j);
            if (up + low != 1.0f || af_engine_joint_mask_weight(shown, AF_MASK_FULL_BODY, j) != 1.0f) partition = false;
            if (l + r > 1.0f || arms != std::max(l, r) || (arms > 0.5f && up < 0.5f)) armsSplit = false;
            if (head > 0.5f && up < 0.5f) headInUpper = false;
        }
        check(partition, "upper and lower body together are the whole skeleton, with no joint in both");
        check(armsSplit, "the two arm masks do not overlap, add up to the arms mask and lie in the upper body");
        check(headInUpper && af_engine_joint_mask_weight(shown, AF_MASK_HEAD, m.joint("head")) == 1.0f, "the head mask holds the head and lies in the upper body");
        check(af_engine_joint_mask_weight(shown, AF_MASK_LOWER_BODY, m.joint("hips")) == 1.0f && af_engine_joint_mask_weight(shown, AF_MASK_LOWER_BODY, m.joint("thigh_L")) == 1.0f &&
              af_engine_joint_mask_weight(shown, AF_MASK_LEFT_ARM, m.joint("upperarm_01_L")) == 1.0f && af_engine_joint_mask_weight(shown, AF_MASK_RIGHT_ARM, m.joint("hand_R")) == 1.0f,
              "hips and thigh are lower body; the _L upper arm and the _R hand are in their arm masks");
        std::vector<float> scratch(static_cast<size_t>(n), 0.0f);
        const af_animation_layer bad[1] = {{9999, 0.0f, 1.0f, AF_MASK_FULL_BODY}};
        check(af_engine_evaluate_layers(shown, nullptr, 1, scratch.data(), J) < 0 && af_engine_evaluate_layers(shown, bad, 1, scratch.data(), J) < 0 &&
              af_engine_evaluate_layers(nullptr, bad, 1, scratch.data(), J) < 0, "bad arguments to the layer evaluation are refused");
    }

    /* ================================================================ retargeting =========== */
    {
        /* Refusals first: nothing may change. */
        af_motion_import_report refused{};
        const int32_t rcSeven = af_engine_import_motions_from_memory(shown, sevenJoints.data(), sevenJoints.size(), &refused);
        std::printf("INFO seven-joint figure: rc=%d \"%s\"\n", rcSeven, refused.message);
        check(rcSeven == AF_ERR_UNSUPPORTED && refused.compatible == 0 && refused.animations_added == 0, "a seven-joint figure is refused as not a humanoid this rig can follow");
        check(std::strstr(refused.message, "missing") != nullptr && std::strstr(refused.message, "forearm") != nullptr, "the refusal names what is missing");
        check(af_engine_animation_count(shown) == builtIn, "a refused file adds nothing");
        af_motion_import_report broken{};
        std::vector<uint8_t> cut(pack.begin(), pack.begin() + static_cast<std::ptrdiff_t>(pack.size() / 3));
        check(af_engine_import_motions_from_memory(shown, cut.data(), cut.size(), &broken) < 0 && broken.message[0] != 0 && af_engine_animation_count(shown) == builtIn,
              "a truncated file is refused with a message and adds nothing");
        check(af_engine_import_motions_from_memory(shown, nullptr, 0, &broken) == AF_ERR_INVALID_ARG, "no data is refused");

        af_motion_import_report report{};
        const int32_t rc = af_engine_import_motions_from_memory(shown, pack.data(), pack.size(), &report);
        std::printf("INFO pack: rc=%d \"%s\"\n", rc, report.message);
        check(rc == AF_OK && report.compatible == 1, "the humanoid pack is accepted");
        check(report.source_joints == 27 && report.recognised_joints == 24, "24 of its 27 joints are recognised (the three end joints are not)");
        check(report.mapped_joints == 24, "24 joints of the avatar follow the file");
        check(report.animations_in_file == 5 && report.animations_added == 5 && af_engine_animation_count(shown) == builtIn + 5, "all five animations are added after the built-in ones");

        const int armsUp = m.animation("Arms Up"), kneeLift = m.animation("Knee Lift"), turnLeft = m.animation("Turn Left");
        const int hop = m.animation("Hop"), stride = m.animation("Stride Ahead");
        check(armsUp >= 0 && kneeLift >= 0 && turnLeft >= 0 && hop >= 0 && stride >= 0, "they keep the names the file gives them");
        if (armsUp < 0 || kneeLift < 0 || turnLeft < 0 || hop < 0 || stride < 0) return 1;
        bool allRigid = true;
        for (int a = builtIn; a < builtIn + 5; ++a) {
            af_animation_info info{};
            af_engine_animation_info(shown, a, &info);
            if (info.source != 1) allRigid = false;
            for (int k = 0; k <= 4; ++k) if (!rigid(m.pose(a, info.duration * static_cast<float>(k) / 4.0f), J)) allRigid = false;
        }
        check(allRigid, "every retargeted animation is marked as coming from a file and gives rigid matrices");

        const std::vector<float> rest = m.pose(-1, 0.0f);
        /* Arms Up: the file's rest is a T pose, the avatar's has the arms down. */
        {
            const std::vector<float> t0 = m.pose(armsUp, 0.0f), t1 = m.pose(armsUp, 1.0f);
            const float shoulderY = m.at(t0, "upperarm_01_L").y;
            const V3 l0 = m.at(t0, "hand_L"), r0 = m.at(t0, "hand_R"), l1 = m.at(t1, "hand_L"), r1 = m.at(t1, "hand_R");
            const float headY = m.at(t1, "head").y;
            std::printf("INFO Arms Up: at 0 s the hands are at x=%.2f and x=%.2f, %.0f mm and %.0f mm from shoulder height; at 1 s they are %.2f m and %.2f m above the head joint\n",
                        l0.x, r0.x, std::fabs(l0.y - shoulderY) * 1000.0f, std::fabs(r0.y - shoulderY) * 1000.0f, l1.y - headY, r1.y - headY);
            check(std::fabs(l0.y - shoulderY) < 0.06f && std::fabs(r0.y - shoulderY) < 0.06f && l0.x > 0.5f && r0.x < -0.5f,
                  "at its start the avatar stands in the file's T pose: hands at shoulder height, out to the sides");
            check(l1.y > headY + 0.2f && r1.y > headY + 0.2f, "at its peak both hands are well above the head");
            check(std::fabs(l1.x - m.at(t1, "upperarm_01_L").x) < 0.08f && std::fabs(r1.x - m.at(t1, "upperarm_01_R").x) < 0.08f, "and straight above the shoulders");
            check(length(m.at(t1, "foot_L") - m.at(rest, "foot_L")) < 0.01f, "while the feet stay where they stand");
        }
        /* Knee Lift: the file's LEFT leg. This avatar faces -Z, so its own left is -X. */
        {
            const std::vector<float> p = m.pose(kneeLift, 1.0f);
            const bool lIsOwnLeft = m.rest[static_cast<size_t>(m.joint("thigh_L"))].x < 0.0f;
            const char* own = lIsOwnLeft ? "_L" : "_R";
            const char* other = lIsOwnLeft ? "_R" : "_L";
            const V3 hip = m.at(p, (std::string("thigh") + own).c_str()), knee = m.at(p, (std::string("shin") + own).c_str()), ankle = m.at(p, (std::string("foot") + own).c_str());
            const V3 otherKnee = m.at(p, (std::string("shin") + other).c_str()), otherKneeRest = m.at(rest, (std::string("shin") + other).c_str());
            std::printf("INFO Knee Lift: the knee on the figure's left is %.0f mm from hip height and %.2f m in front of the hip; the ankle hangs %.2f m below it; the other knee moved %.1f mm\n",
                        std::fabs(knee.y - hip.y) * 1000.0f, hip.z - knee.z, knee.y - ankle.y, length(otherKnee - otherKneeRest) * 1000.0f);
            check(knee.x < 0.0f, "the leg that lifts is on the figure's own left");
            check(std::fabs(knee.y - hip.y) < 0.06f && hip.z - knee.z > 0.25f, "its knee comes up to hip height, in front of the hip");
            check(knee.y - ankle.y > 0.25f && std::fabs(ankle.z - knee.z) < 0.08f, "its shin hangs straight down from the knee");
            check(length(otherKnee - otherKneeRest) < 0.01f, "the other leg stays where it is");
        }
        /* Turn Left: a quarter turn. Facing -Z, turning left ends facing -X. */
        {
            af_animation_info info{};
            af_engine_animation_info(shown, turnLeft, &info);
            const std::vector<float> p = m.pose(turnLeft, info.duration);
            const V3 before = m.at(rest, "toe_L") - m.at(rest, "foot_L"), after = m.at(p, "toe_L") - m.at(p, "foot_L");
            const float beforeLen = std::sqrt(before.x * before.x + before.z * before.z), afterLen = std::sqrt(after.x * after.x + after.z * after.z);
            std::printf("INFO Turn Left: the toes point along (%.2f, %.2f) at rest and (%.2f, %.2f) at the end (x, z)\n",
                        before.x / beforeLen, before.z / beforeLen, after.x / afterLen, after.z / afterLen);
            check(before.z / beforeLen < -0.9f, "at rest the toes point along -Z, the way the avatar faces");
            check(after.x / afterLen < -0.97f, "after the turn they point along -X: a quarter turn to the figure's own left");
            check(info.looping == 0, "a clip that ends turned is not marked as looping");
        }
        /* Hop: 20 cm for the file's 84 cm legs, scaled to this avatar's legs. */
        {
            const char* side = "_L";
            const float leg = length(m.rest[static_cast<size_t>(m.joint((std::string("shin") + side).c_str()))] - m.rest[static_cast<size_t>(m.joint((std::string("thigh") + side).c_str()))]) +
                              length(m.rest[static_cast<size_t>(m.joint((std::string("foot") + side).c_str()))] - m.rest[static_cast<size_t>(m.joint((std::string("shin") + side).c_str()))]);
            const float expected = 0.20f * leg / 0.84f;
            const std::vector<float> p = m.pose(hop, 0.5f);
            const float rise = m.at(p, "hips").y - m.at(rest, "hips").y, footRise = m.at(p, "foot_L").y - m.at(rest, "foot_L").y;
            af_animation_info info{};
            af_engine_animation_info(shown, hop, &info);
            std::printf("INFO Hop: the avatar's leg is %.3f m, so 20 cm becomes %.3f m; the hips rise %.3f m and the ankle %.3f m\n", leg, expected, rise, footRise);
            check(std::fabs(rise - expected) < 0.015f && std::fabs(footRise - expected) < 0.015f, "the whole figure rises by the file's hop, scaled by the leg length");
            check(info.moves_root == 1 && info.looping == 1 && info.travels == 0, "the hop is reported as moving the root, looping, and not travelling");
            check(length(m.at(m.pose(hop, 0.0f), "hips") - m.at(rest, "hips")) < 0.005f, "and starts on the ground");
        }
        /* Stride Ahead: the hips walk a metre away in the file; here the figure stays on its spot. */
        {
            af_animation_info info{};
            af_engine_animation_info(shown, stride, &info);
            const std::vector<float> first = m.pose(stride, 0.0f), last = m.pose(stride, info.duration), quarter = m.pose(stride, 0.25f);
            const V3 travel = m.at(last, "hips") - m.at(first, "hips");
            const float apart = std::fabs(m.at(quarter, "foot_L").z - m.at(quarter, "foot_R").z);
            std::printf("INFO Stride Ahead: the hips end %.1f mm from where they start; at 0.25 s the ankles are %.2f m apart front to back\n", length(travel) * 1000.0f, apart);
            check(info.travels == 1, "a clip that walks away is reported as travelling");
            check(length(travel) < 0.02f, "its travel is taken out: the hips end where they start");
            check(apart > 0.3f, "while the legs still swing");
            check(std::strstr(report.message, "on the spot") != nullptr, "the report says a travelling animation plays on the spot");
        }
        /* A second import of the same file: the names are told apart. */
        {
            af_motion_import_report again{};
            check(af_engine_import_motions_from_memory(shown, pack.data(), pack.size(), &again) == AF_OK && af_engine_animation_count(shown) == builtIn + 10,
                  "the same file can be added a second time");
            check(m.animation("Arms Up 2") >= 0 && m.animation("Hop 2") >= 0, "and its animations are numbered so that no two share a name");
        }
        /* A retargeted clip under a built-in one: layers do not care where a clip came from. */
        {
            const int walk = m.animation("walk");
            const std::vector<float> mixed = m.layers({{walk, 0.3f, 1.0f, AF_MASK_FULL_BODY}, {armsUp, 1.0f, 1.0f, AF_MASK_ARMS}});
            const std::vector<float> walkPose = m.pose(walk, 0.3f);
            check(m.at(mixed, "hand_L").y > m.at(mixed, "head").y + 0.15f && length(m.at(mixed, "foot_L") - m.at(walkPose, "foot_L")) < 1e-4f,
                  "a retargeted clip layers over a built-in one: arms overhead while the legs walk");
        }
        /* The generating engine takes motions too, and a new import forgets them. */
        {
            af_motion_import_report onGen{};
            check(af_engine_import_motions_from_memory(gen, pack.data(), pack.size(), &onGen) == AF_OK && af_engine_animation_count(gen) == builtIn + 5,
                  "the engine that generated the avatar accepts the pack as well");
            check(af_engine_import_glb_from_memory(shown, bytes, size) == AF_OK && af_engine_animation_count(shown) == builtIn,
                  "importing a model again starts without the added motions");
            m.read(shown);
        }
        /* A model that is not this engine's: it has no skeleton the engine can move. */
        {
            af_engine* foreign = nullptr;
            af_engine_create(&spec, &cb, &foreign);
            af_motion_import_report r{};
            const bool imported = af_engine_import_glb_from_memory(foreign, pack.data(), pack.size()) == AF_OK;
            const int32_t rcForeign = af_engine_import_motions_from_memory(foreign, pack.data(), pack.size(), &r);
            std::printf("INFO motions onto a model from elsewhere: rc=%d \"%s\"\n", rcForeign, r.message);
            check(imported && rcForeign == AF_ERR_NOT_READY && r.message[0] != 0, "a model from elsewhere cannot take motions, and the report says why");
            check(af_engine_face_kernel_count(foreign) == 0, "a model from elsewhere has no face rig");
            float kernels[16 * 4], box[8];
            float weights[52];
            for (float& w : weights) w = 1.0f;
            check(af_engine_pack_face(foreign, weights, 52, kernels, 4, box) == 0, "so there is nothing to upload for its face");
            af_engine_destroy(foreign);
        }
    }

    /* ================================================================ face ================== */
    {
        char names[64][64];
        const int channels = af_engine_blendshape_names(names, 64);
        auto channel = [&](const char* name) {
            for (int c = 0; c < channels; ++c) if (std::strcmp(names[c], name) == 0) return c;
            return -1;
        };
        const int jawOpen = channel("jawOpen"), smileL = channel("mouthSmileLeft"), blinkL = channel("eyeBlinkLeft");
        check(channels == 52 && jawOpen >= 0 && smileL >= 0 && blinkL >= 0, "the engine names 52 face channels");
        const int kernelsGen = af_engine_face_kernel_count(gen), kernelsShown = af_engine_face_kernel_count(shown);
        std::printf("INFO face rig: %d kernels in the generating engine, %d after the file\n", kernelsGen, kernelsShown);
        check(kernelsGen > 52 && kernelsShown == kernelsGen, "the face rig survives export and import, kernel for kernel");

        const int V = af_engine_vertex_count(gen);
        std::vector<af_vertex> restVerts(af_engine_vertices_rest(gen), af_engine_vertices_rest(gen) + V);
        constexpr int kMax = 32;
        std::vector<float> packed(static_cast<size_t>(kMax) * 16, 0.0f);
        float box[8] = {0};

        /* The packed form against the engine's own morph offsets, every channel, every vertex. */
        float worst = 0.0f, largest = 0.0f;
        int movedChannels = 0, tooMany = 0;
        std::vector<float> weights(static_cast<size_t>(channels), 0.0f);
        for (int c = 0; c < channels; ++c) {
            std::fill(weights.begin(), weights.end(), 0.0f);
            weights[static_cast<size_t>(c)] = 1.0f;
            const int count = af_engine_pack_face(gen, weights.data(), channels, packed.data(), kMax, box);
            if (count >= kMax) ++tooMany;
            if (af_engine_apply_blendshapes(gen, weights.data(), channels) != AF_OK) { ++gFailures; break; }
            const af_vertex* posed = af_engine_vertices(gen);
            float channelLargest = 0.0f;
            for (int i = 0; i < V; ++i) {
                float d[3] = {0, 0, 0};
                af_face_evaluate_packed(packed.data(), count, box, restVerts[static_cast<size_t>(i)].pos, d);
                for (int k = 0; k < 3; ++k) {
                    const float engineDelta = posed[i].pos[k] - restVerts[static_cast<size_t>(i)].pos[k];
                    worst = std::max(worst, std::fabs(engineDelta - d[k]));
                    channelLargest = std::max(channelLargest, std::fabs(d[k]));
                }
            }
            if (channelLargest > 0.0005f) ++movedChannels;
            largest = std::max(largest, channelLargest);
        }
        std::fill(weights.begin(), weights.end(), 0.0f);
        af_engine_apply_blendshapes(gen, weights.data(), channels);
        std::printf("INFO face: %d of %d channels move a vertex by more than 0.5 mm; the largest offset is %.1f mm; packed form and engine differ by at most %.4f mm\n",
                    movedChannels, channels, largest * 1000.0f, worst * 1000.0f);
        check(tooMany == 0, "no single channel needs more kernels than the renderer uploads");
        check(worst < 2e-6f, "the packed form gives the engine's own offsets for every channel and vertex (2 micrometres)");
        check(movedChannels >= 48, "at least 48 of the 52 channels visibly move the face");

        /* jawOpen: the lower lip leaves the upper lip. */
        {
            std::fill(weights.begin(), weights.end(), 0.0f);
            weights[static_cast<size_t>(jawOpen)] = 1.0f;
            const int count = af_engine_pack_face(gen, weights.data(), channels, packed.data(), kMax, box);
            float cutY = 0.0f, centreX = 0.0f;
            bool jawKernel = false;
            for (int k = 0; k < count; ++k) {
                if ((static_cast<int>(packed[static_cast<size_t>(k) * 16 + 7] + 0.5f) & 2) == 0) continue;
                jawKernel = true;
                centreX = packed[static_cast<size_t>(k) * 16 + 0];
                cutY = packed[static_cast<size_t>(k) * 16 + 11];
            }
            float upperMoves = 0.0f, lowerDrop = 0.0f;
            int upper = 0, lower = 0;
            for (int i = 0; i < V; ++i) {
                const float* p = restVerts[static_cast<size_t>(i)].pos;
                if (std::fabs(p[0] - centreX) > 0.006f) continue;
                if (!(p[0] > box[0] && p[0] < box[4] && p[1] > box[1] && p[1] < box[5] && p[2] > box[2] && p[2] < box[6])) continue;
                float d[3] = {0, 0, 0};
                af_face_evaluate_packed(packed.data(), count, box, p, d);
                if (p[1] > cutY && p[1] < cutY + 0.006f) { ++upper; upperMoves = std::max(upperMoves, length(V3(d[0], d[1], d[2]))); }
                if (p[1] < cutY - 0.002f && p[1] > cutY - 0.008f) { ++lower; lowerDrop = std::max(lowerDrop, -d[1]); }
            }
            std::printf("INFO jawOpen: lip line at y=%.4f; %d vertices just above it move at most %.2f mm, %d just below it drop by up to %.1f mm\n",
                        cutY, upper, upperMoves * 1000.0f, lower, lowerDrop * 1000.0f);
            check(jawKernel, "jawOpen is driven by a jaw kernel");
            check(upper > 0 && lower > 0 && upperMoves < 0.0005f, "jawOpen leaves the upper lip where it is");
            check(lowerDrop > 0.010f, "jawOpen drops the lower lip by more than 10 mm: the mouth opens");
        }
        /* Several channels at once add up. */
        {
            std::fill(weights.begin(), weights.end(), 0.0f);
            weights[static_cast<size_t>(jawOpen)] = 0.6f;
            weights[static_cast<size_t>(smileL)] = 1.0f;
            weights[static_cast<size_t>(blinkL)] = 0.8f;
            const int count = af_engine_pack_face(gen, weights.data(), channels, packed.data(), kMax, box);
            std::vector<float> one(static_cast<size_t>(kMax) * 16, 0.0f);
            float worstSum = 0.0f;
            const int picks[3] = {jawOpen, smileL, blinkL};
            for (int i = 0; i < V; i += 3) {
                float together[3] = {0, 0, 0}, sum[3] = {0, 0, 0};
                af_face_evaluate_packed(packed.data(), count, box, restVerts[static_cast<size_t>(i)].pos, together);
                for (int c : picks) {
                    std::vector<float> single(static_cast<size_t>(channels), 0.0f);
                    single[static_cast<size_t>(c)] = weights[static_cast<size_t>(c)];
                    float oneBox[8];
                    const int n = af_engine_pack_face(gen, single.data(), channels, one.data(), kMax, oneBox);
                    float d[3] = {0, 0, 0};
                    af_face_evaluate_packed(one.data(), n, oneBox, restVerts[static_cast<size_t>(i)].pos, d);
                    for (int k = 0; k < 3; ++k) sum[k] += d[k];
                }
                for (int k = 0; k < 3; ++k) worstSum = std::max(worstSum, std::fabs(together[k] - sum[k]));
            }
            check(count >= 3 && worstSum < 2e-6f, "three channels at once give the sum of the three alone");
            /* All 52 at full weight cannot fit; the strongest are kept and the count says so. */
            for (float& w : weights) w = 1.0f;
            check(af_engine_pack_face(gen, weights.data(), channels, packed.data(), kMax, box) == kMax, "with every channel on, the upload is capped at the block size");
        }
        /* The file: the rig moved with the model when the exporter stood it on the floor. */
        {
            std::fill(weights.begin(), weights.end(), 0.0f);
            weights[static_cast<size_t>(jawOpen)] = 1.0f;
            weights[static_cast<size_t>(smileL)] = 0.5f;
            std::vector<float> packedShown(static_cast<size_t>(kMax) * 16, 0.0f);
            float boxShown[8] = {0};
            const int countGen = af_engine_pack_face(gen, weights.data(), channels, packed.data(), kMax, box);
            const int countShown = af_engine_pack_face(shown, weights.data(), channels, packedShown.data(), kMax, boxShown);
            const float lift = boxShown[1] - box[1];
            float worstFile = 0.0f;
            for (int i = 0; i < V; i += 2) {
                const float* p = restVerts[static_cast<size_t>(i)].pos;
                const float moved[3] = {p[0], p[1] + lift, p[2]};
                float a[3] = {0, 0, 0}, b[3] = {0, 0, 0};
                af_face_evaluate_packed(packed.data(), countGen, box, p, a);
                af_face_evaluate_packed(packedShown.data(), countShown, boxShown, moved, b);
                for (int k = 0; k < 3; ++k) worstFile = std::max(worstFile, std::fabs(a[k] - b[k]));
            }
            std::printf("INFO face rig in the file: moved %.4f m with the model; offsets differ from the generating engine's by at most %.4f mm\n", lift, worstFile * 1000.0f);
            check(countShown == countGen && countGen > 0 && worstFile < 5e-6f, "the face rig read back from the file gives the same offsets (5 micrometres)");
        }
        check(af_engine_pack_face(nullptr, weights.data(), channels, packed.data(), kMax, box) == 0 && af_engine_face_kernel_count(nullptr) == 0,
              "a null engine has no face rig");
    }

    af_arena_free(bytes);
    af_engine_destroy(shown);
    af_engine_destroy(gen);
    std::printf("%s (%d failure%s)\n", gFailures == 0 ? "ALL PASS" : "FAILED", gFailures, gFailures == 1 ? "" : "s");
    return gFailures == 0 ? 0 : 1;
}
