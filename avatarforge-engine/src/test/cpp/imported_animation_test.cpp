/* imported_animation_test.cpp
   An imported GLB must actually MOVE.
   ---------------------------------------------------------------------------------------------
   For most of this engine's life, importing a skinned, animated GLB parsed its skeleton and its clips
   and then dropped both on the floor: the file rendered in its bind pose, forever, and the only trace
   of the loss was two log warnings. This test exists so that state cannot return unnoticed.

   It drives the fixtures through the production entry points -- af_engine_import_glb_from_memory,
   af_engine_imported_play and af_engine_imported_seek -- and asserts on the vertex buffer the renderer
   would actually be handed. Nothing here inspects the animation core directly: the claim under test is
   about the engine's OUTPUT.

   The assertions are shaped to fail on the easy ways to fake this:
     * "vertices differ" alone passes if one coordinate jitters, so the displaced FRACTION and the
       displacement relative to the model's own extent are both checked (Fox is authored in centimetres,
       CesiumMan in metres -- an absolute threshold would be an asset-specific guess).
     * A single pose change proves nothing about accumulation, so the same clip time is sampled twice
       with a different time in between and must reproduce EXACTLY, and af_engine_vertices_rest must be
       byte-identical after posing: deformation is evaluated from the bind pose, never from whatever is
       currently on screen.
     * An import that quietly started playing its first clip would still move vertices, so the freshly
       imported model is required to be in its bind pose, reproducing the positions the file declares.
     * A seek that reports success while moving nothing is worse than a crash, so seeking before a clip
       is selected must be refused rather than returning AF_OK. */
#include "afcore.h"

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

/* Two double arguments, always consumed as doubles: a %d in one of these format strings reads a double
   as an int and prints garbage that looks like a real measurement. */
void checkf(bool cond, const char* fmt, double a, double b) {
    if (cond) {
        std::printf("PASS  ");
        std::printf(fmt, a, b);
        std::printf("\n");
    } else {
        std::fprintf(stderr, "FAIL  ");
        std::fprintf(stderr, fmt, a, b);
        std::fprintf(stderr, "\n");
        ++gFailures;
    }
}

void skip(const char* what, const std::string& path) {
    std::printf("SKIP  %s not present at %s\n", what, path.c_str());
}

bool readFile(const std::string& path, std::vector<uint8_t>* out) {
    std::FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) return false;
    std::fseek(fp, 0, SEEK_END);
    const long sz = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);
    if (sz <= 0) { std::fclose(fp); return false; }
    out->resize(static_cast<size_t>(sz));
    const size_t got = std::fread(out->data(), 1, out->size(), fp);
    std::fclose(fp);
    return got == out->size();
}

std::vector<float> flatten(const af_vertex* verts, int32_t count, bool positions) {
    std::vector<float> p;
    if (count <= 0 || !verts) return p;
    p.reserve(static_cast<size_t>(count) * 3u);
    for (int32_t i = 0; i < count; ++i) {
        const float* src = positions ? verts[i].pos : verts[i].normal;
        p.push_back(src[0]);
        p.push_back(src[1]);
        p.push_back(src[2]);
    }
    return p;
}

std::vector<float> capturePositions(const af_engine* engine) {
    return flatten(af_engine_vertices(engine), af_engine_vertex_count(engine), true);
}

std::vector<float> captureRestPositions(const af_engine* engine) {
    return flatten(af_engine_vertices_rest(engine), af_engine_vertex_count(engine), true);
}

/* Diagonal of the axis-aligned bounds: the model's own sense of scale. */
float extentOf(const std::vector<float>& p) {
    if (p.size() < 3) return 0.0f;
    float lo[3] = { p[0], p[1], p[2] };
    float hi[3] = { p[0], p[1], p[2] };
    for (size_t i = 0; i + 2 < p.size(); i += 3) {
        for (int k = 0; k < 3; ++k) {
            const float v = p[i + static_cast<size_t>(k)];
            if (!std::isfinite(v)) continue;
            if (v < lo[k]) lo[k] = v;
            if (v > hi[k]) hi[k] = v;
        }
    }
    float e = 0.0f;
    for (int k = 0; k < 3; ++k) e += (hi[k] - lo[k]) * (hi[k] - lo[k]);
    return std::sqrt(e);
}

struct Motion {
    size_t coords = 0;
    size_t moved = 0;
    float maxDelta = 0.0f;
    bool finite = true;
};

Motion compare(const std::vector<float>& a, const std::vector<float>& b, float eps) {
    Motion m;
    if (a.empty() || a.size() != b.size()) return m;
    m.coords = a.size();
    for (size_t i = 0; i < a.size(); ++i) {
        if (!std::isfinite(a[i]) || !std::isfinite(b[i])) { m.finite = false; continue; }
        const float d = std::fabs(a[i] - b[i]);
        if (d > m.maxDelta) m.maxDelta = d;
        if (d > eps) ++m.moved;
    }
    return m;
}

float fraction(const Motion& m) {
    return m.coords ? 100.0f * static_cast<float>(m.moved) / static_cast<float>(m.coords) : 0.0f;
}

void normalLengthRange(const af_engine* engine, float* outMin, float* outMax) {
    const int32_t count = af_engine_vertex_count(engine);
    const af_vertex* verts = af_engine_vertices(engine);
    float lo = 1e30f, hi = 0.0f;
    for (int32_t i = 0; i < count; ++i) {
        const float l = std::sqrt(verts[i].normal[0] * verts[i].normal[0] +
                                  verts[i].normal[1] * verts[i].normal[1] +
                                  verts[i].normal[2] * verts[i].normal[2]);
        if (!std::isfinite(l)) continue;
        if (l < lo) lo = l;
        if (l > hi) hi = l;
    }
    *outMin = (lo > 1e29f) ? 0.0f : lo;
    *outMax = hi;
}

int32_t triangleCount(const af_engine* engine) {
    const int32_t subs = af_engine_submesh_count(engine);
    const af_submesh* sm = af_engine_submeshes(engine);
    if (subs <= 0 || !sm) return 0;
    int32_t tris = 0;
    for (int32_t i = 0; i < subs; ++i) tris += sm[i].index_count / 3;
    return tris;
}

std::vector<float> captureJointMatrices(const af_engine* engine, int32_t joints) {
    std::vector<float> mats(static_cast<size_t>(joints > 0 ? joints : 0) * 16u, 0.0f);
    if (joints > 0) af_engine_joint_matrices(engine, mats.data(), joints);
    return mats;
}

} // namespace

int main(int argc, char** argv) {
#ifdef AF_TEST_ASSET_DIR
    const std::string fallbackDir = AF_TEST_ASSET_DIR;
#else
    const std::string fallbackDir = "../../../../app/src/main/assets/models/animation";
#endif
    /* argv[1] is the directory holding Fox.glb / CesiumMan.glb / AnimatedMorphCube.glb; its parent
       holds female.glb. Both come from the repo, not from anything generated here. */
    const std::string dir = (argc > 1) ? std::string(argv[1]) : fallbackDir;

    af_spec spec{};
    af_spec_init(&spec);
    af_engine* engine = nullptr;
    if (af_engine_create(&spec, nullptr, &engine) != AF_OK || !engine) {
        std::fprintf(stderr, "FATAL could not create the engine\n");
        return 1;
    }
    if (af_engine_run_all(engine) != AF_OK) {
        std::fprintf(stderr, "FATAL could not build the generated avatar\n");
        af_engine_destroy(engine);
        return 1;
    }

    /* ---------------------------------------------------------------- 1. the generated path ----
       The imported-file API must be inert on a generated avatar: its motion comes from the authored
       library through af_engine_apply_clip, and the two paths must not be confusable. */
    check(af_engine_imported_clip_count(engine) == 0,
          "generated avatar advertises no imported clips");
    check(af_engine_imported_seek(engine, 0.5f) == static_cast<int32_t>(AF_ERR_NOT_READY),
          "generated avatar refuses imported_seek with NOT_READY instead of faking motion");
    check(af_engine_imported_play(engine, 0) == static_cast<int32_t>(AF_ERR_NOT_READY),
          "generated avatar refuses imported_play with NOT_READY");
    check(af_engine_imported_advance(engine, 0.016f) == static_cast<int32_t>(AF_ERR_NOT_READY),
          "generated avatar refuses imported_advance with NOT_READY");
    check(af_engine_imported_is_playing(engine) == 0,
          "generated avatar never reports an imported clip as playing");
    check(af_engine_joint_count(engine) > 0, "generated avatar still exposes its own skeleton");
    const float walkDur = af_engine_clip_duration(engine, AF_CLIP_WALK);
    check(af_engine_apply_clip(engine, AF_CLIP_WALK, walkDur * 0.5f) == AF_OK,
          "generated avatar still plays its authored walk clip");

    /* ------------------------------------------------------------------------- 2. Fox.glb ---- */
    std::vector<uint8_t> foxBytes;
    if (!readFile(dir + "/Fox.glb", &foxBytes)) {
        std::fprintf(stderr, "FATAL could not read %s/Fox.glb\n", dir.c_str());
        af_engine_destroy(engine);
        return 1;
    }
    check(af_engine_import_glb_from_memory(engine, foxBytes.data(),
                                           static_cast<int32_t>(foxBytes.size())) == AF_OK,
          "Fox.glb imports");

    /* The skeleton is adopted -- the exact opposite of what this engine used to assert here. Fox's skin
       declares 24 joints; the skeleton carries 25 because the hierarchy needs one closure node above
       them, so the count is asserted against the file's structure rather than a magic number. */
    const int32_t foxJoints = af_engine_joint_count(engine);
    checkf(foxJoints >= 24 && foxJoints <= 26,
           "Fox skeleton is adopted by the import (joint_count=%.0f; this used to read 0)",
           static_cast<double>(foxJoints), 0.0);
    af_joint_info info{};
    check(af_engine_joint_info(engine, 0, &info) == AF_OK && info.name[0] != '\0',
          "adopted joint 0 carries the name the file gave it");

    /* Its three clips are reachable by index AND by the name the file declared. */
    const int32_t foxClips = af_engine_imported_clip_count(engine);
    checkf(foxClips == 3, "Fox exposes all 3 of its own animation clips (got %.0f, expected %.0f)",
           static_cast<double>(foxClips), 3.0);
    char nameBuf[64] = {0};
    check(af_engine_imported_clip_name(engine, 1, nameBuf, sizeof(nameBuf)) == AF_OK &&
              std::strcmp(nameBuf, "Walk") == 0,
          "Fox clip 1 is named 'Walk' by the file, not by a positional guess");
    const float foxWalk = af_engine_imported_clip_duration(engine, 1);
    check(foxWalk > 0.5f && foxWalk < 1.5f,
          "Fox 'Walk' duration comes from the file's keyframes (~0.71 s)");

    /* The mesh survived the import: a skinning path that silently emptied the index buffer would still
       pass a vertex-difference check, so both are asserted. */
    const int32_t foxVerts = af_engine_vertex_count(engine);
    const int32_t foxTris = triangleCount(engine);
    checkf(foxVerts > 0 && foxTris > 0,
           "imported Fox is a real mesh, not an empty one (%.0f verts, %.0f triangles)",
           static_cast<double>(foxVerts), static_cast<double>(foxTris));

    /* ---- the import lands in the BIND pose, not in some clip's first frame ---- */
    const std::vector<float> rest = captureRestPositions(engine);
    const std::vector<float> bind = capturePositions(engine);
    checkf(!rest.empty() && rest.size() == bind.size(),
           "the import kept a rest-pose copy to skin from (%.0f coordinates)",
           static_cast<double>(rest.size()), 0.0);
    const float extent = extentOf(rest);
    checkf(extent > 100.0f && extent < 400.0f,
           "Fox's raw positions have a sane extent for a centimetre-authored model (%.2f units)",
           static_cast<double>(extent), 0.0);
    const Motion rb = compare(rest, bind, 0.0f);
    checkf(rb.maxDelta < extent * 1e-4f,
           "a fresh import is in its bind pose: skinning at rest reproduces the file's own vertex "
           "positions (max delta %.6f vs extent %.2f)",
           static_cast<double>(rb.maxDelta), static_cast<double>(extent));

    /* ---- seeking before a clip is selected must be refused, not silently ignored ---- */
    check(af_engine_imported_seek(engine, 0.25f) == static_cast<int32_t>(AF_ERR_NOT_READY),
          "seeking before any clip is selected is refused instead of reporting success for a no-op");
    check(compare(bind, capturePositions(engine), 0.0f).maxDelta == 0.0f,
          "the refused seek left the geometry untouched");

    /* ---- the claim under test: seeking a clip moves the geometry ---- */
    check(af_engine_imported_play(engine, 1) == AF_OK, "Fox 'Walk' can be selected for playback");
    check(af_engine_imported_seek(engine, foxWalk * 0.5f) == AF_OK,
          "Fox 'Walk' can be sampled at t = 0.5 * duration");
    const std::vector<float> mid = capturePositions(engine);
    const Motion m1 = compare(bind, mid, extent * 1e-4f);
    check(m1.finite, "every posed Fox coordinate is finite");
    checkf(fraction(m1) >= 80.0f,
           "at least 80%% of Fox's coordinates actually move at mid-clip (%.1f%% did)",
           static_cast<double>(fraction(m1)), 80.0);
    checkf(m1.maxDelta > extent * 0.01f,
           "Fox's displacement is real motion, not float noise (max %.4f vs extent %.2f)",
           static_cast<double>(m1.maxDelta), static_cast<double>(extent));

    /* Motion must progress, not flip once and then hold. */
    check(af_engine_imported_seek(engine, foxWalk * 0.75f) == AF_OK,
          "Fox 'Walk' samples at t = 0.75 * duration");
    const std::vector<float> late = capturePositions(engine);
    const Motion m2 = compare(mid, late, extent * 1e-4f);
    checkf(m2.moved > 0 && m2.maxDelta > extent * 1e-3f,
           "Fox keeps moving between t=0.5 and t=0.75 of the clip (%.4f units)",
           static_cast<double>(m2.maxDelta), 0.0);

    /* Normals follow the pose and stay unit length. Skinning a direction with a matrix that still
       carries translation, or never renormalising, both surface here. */
    float nMin = 0.0f, nMax = 0.0f;
    normalLengthRange(engine, &nMin, &nMax);
    checkf(nMin > 0.9f && nMax < 1.1f,
           "posed Fox normals stay unit length (min %.4f, max %.4f)",
           static_cast<double>(nMin), static_cast<double>(nMax));

    /* The published joint matrices must TRACK the pose. They are per-joint world transforms, which are
       already non-identity at rest, so "differs from identity" would pass on a stale buffer -- two
       different poses must give two different palettes instead. */
    const std::vector<float> matsLate = captureJointMatrices(engine, foxJoints);
    check(af_engine_imported_seek(engine, foxWalk * 0.5f) == AF_OK,
          "Fox 'Walk' returns to t = 0.5 * duration");
    const std::vector<float> matsMid = captureJointMatrices(engine, foxJoints);
    {
        bool allFinite = true;
        for (float v : matsMid) if (!std::isfinite(v)) allFinite = false;
        check(allFinite, "every published joint matrix is finite");
        const Motion mm = compare(matsLate, matsMid, 1e-5f);
        checkf(mm.moved > 0,
               "the published joint matrices change with the pose (%.0f of %.0f entries differ between "
               "t=0.75 and t=0.5)",
               static_cast<double>(mm.moved), static_cast<double>(mm.coords));
    }

    /* ---- the invariant that catches cumulative drift ----
       Same clip, same time, sampled twice with a different pose in between: it must reproduce EXACTLY.
       Anything else means deformation was applied on top of already-deformed geometry. */
    const std::vector<float> midAgain = capturePositions(engine);
    const Motion m3 = compare(mid, midAgain, 0.0f);
    checkf(m3.maxDelta == 0.0f,
           "re-sampling the same clip time reproduces it bit-for-bit -- deformation is evaluated from "
           "the bind pose, never accumulated (max delta %.10f)",
           static_cast<double>(m3.maxDelta), 0.0);
    const Motion mr = compare(rest, captureRestPositions(engine), 0.0f);
    checkf(mr.maxDelta == 0.0f,
           "posing never mutates the bind-pose copy it skins from (max delta %.10f)",
           static_cast<double>(mr.maxDelta), 0.0);

    /* t=0 of a clip is a pose in its own right, and need not equal the bind pose. */
    check(af_engine_imported_seek(engine, 0.0f) == AF_OK, "Fox 'Walk' can be sampled at t = 0");
    const std::vector<float> walkZero = capturePositions(engine);
    checkf(compare(walkZero, mid, extent * 1e-4f).moved > 0,
           "Walk at t=0 is a distinct pose from Walk at t=0.5 (%.1f%% of coordinates differ)",
           static_cast<double>(fraction(compare(walkZero, mid, extent * 1e-4f))), 0.0);

    /* Player::seek CLAMPS to [0, duration] -- it does not wrap, because a scrub bar that jumped to the
       start when dragged past the end would be unusable. Assert that contract rather than a guess about
       the asset's keyframes. */
    check(af_engine_imported_seek(engine, foxWalk * 1.25f) == AF_OK,
          "Fox 'Walk' accepts a time beyond its duration");
    const std::vector<float> pastEnd = capturePositions(engine);
    check(af_engine_imported_seek(engine, foxWalk) == AF_OK,
          "Fox 'Walk' samples at exactly its duration");
    const Motion m4 = compare(pastEnd, capturePositions(engine), 0.0f);
    checkf(m4.maxDelta == 0.0f,
           "seeking past the end clamps to the end pose instead of wrapping (max delta %.8f)",
           static_cast<double>(m4.maxDelta), 0.0);
    checkf(compare(pastEnd, mid, extent * 1e-4f).moved > 0,
           "the clamped end pose is a real pose, not a freeze (%.1f%% differs from mid-clip)",
           static_cast<double>(fraction(compare(pastEnd, mid, extent * 1e-4f))), 0.0);
    check(af_engine_imported_seek(engine, -5.0f) == AF_OK, "Fox 'Walk' accepts a negative time");
    const Motion m5 = compare(walkZero, capturePositions(engine), 0.0f);
    checkf(m5.maxDelta == 0.0f, "a negative seek clamps to t=0 (max delta %.8f)",
           static_cast<double>(m5.maxDelta), 0.0);

    /* ---- per-frame transport ----
       advance() is what a renderer calls each frame. It must agree with seek() while inside the clip,
       WRAP where seek() clamps, and stop dead when paused. */
    check(af_engine_imported_is_playing(engine) == 1,
          "a selected imported clip reports itself as playing");
    check(af_engine_imported_seek(engine, 0.0f) == AF_OK, "Fox 'Walk' rewound to t=0 for the advance test");
    check(af_engine_imported_advance(engine, 0.0f) == AF_OK, "advancing by zero is accepted");
    checkf(compare(walkZero, capturePositions(engine), 0.0f).maxDelta == 0.0f,
           "advancing by zero does not move the pose (max delta %.8f)",
           static_cast<double>(compare(walkZero, capturePositions(engine), 0.0f).maxDelta), 0.0);

    /* One advance from t=0 lands on exactly the same clock value as a seek to that time, so the two
       routes must produce one pose. Accumulated float error is why the multi-step case below is checked
       against a tolerance instead of for equality. */
    check(af_engine_imported_seek(engine, 0.0f) == AF_OK, "Fox 'Walk' rewound again");
    check(af_engine_imported_advance(engine, foxWalk * 0.5f) == AF_OK,
          "Fox 'Walk' advances by half a duration in one step");
    const std::vector<float> advanced = capturePositions(engine);
    checkf(compare(advanced, mid, 0.0f).maxDelta == 0.0f,
           "advance(0.5*dur) from t=0 gives exactly the pose seek(0.5*dur) gives (max delta %.10f)",
           static_cast<double>(compare(advanced, mid, 0.0f).maxDelta), 0.0);

    check(af_engine_imported_seek(engine, 0.0f) == AF_OK, "Fox 'Walk' rewound for the stepped advance");
    for (int step = 0; step < 7; ++step) {
        if (af_engine_imported_advance(engine, 0.05f) != AF_OK) {
            check(false, "Fox 'Walk' accepts seven 0.05 s advance steps");
            break;
        }
        if (step == 6) check(true, "Fox 'Walk' accepts seven 0.05 s advance steps");
    }
    const std::vector<float> stepped = capturePositions(engine);
    check(af_engine_imported_seek(engine, 0.35f) == AF_OK, "Fox 'Walk' seeks to the stepped clock's time");
    const Motion m6 = compare(stepped, capturePositions(engine), extent * 1e-5f);
    checkf(m6.maxDelta < extent * 1e-4f,
           "seven 0.05 s advances agree with one seek to 0.35 s, within accumulated float error "
           "(max delta %.6f vs extent %.2f)",
           static_cast<double>(m6.maxDelta), static_cast<double>(extent));

    /* advance() wraps a looping clip; seek() clamps. If advance clamped too, a preview would freeze at
       the last frame forever instead of looping. */
    check(af_engine_imported_seek(engine, 0.0f) == AF_OK, "Fox 'Walk' rewound for the wrap test");
    check(af_engine_imported_advance(engine, foxWalk * 1.5f) == AF_OK,
          "Fox 'Walk' advances past the end of its duration");
    const std::vector<float> wrappedPose = capturePositions(engine);
    check(af_engine_imported_seek(engine, foxWalk * 0.5f) == AF_OK,
          "Fox 'Walk' seeks to the time the wrap should have landed on");
    const Motion m7 = compare(wrappedPose, capturePositions(engine), extent * 1e-4f);
    checkf(m7.maxDelta < extent * 1e-3f,
           "advancing 1.5*dur wraps to 0.5*dur rather than clamping (max delta %.6f vs extent %.2f)",
           static_cast<double>(m7.maxDelta), static_cast<double>(extent));
    checkf(compare(wrappedPose, pastEnd, extent * 1e-4f).moved > 0,
           "the wrapped pose is not the clamped end pose (%.1f%% of coordinates differ)",
           static_cast<double>(fraction(compare(wrappedPose, pastEnd, extent * 1e-4f))), 0.0);

    /* Pause must actually stop the clock: a paused preview that keeps moving is not paused. */
    check(af_engine_imported_pause(engine) == AF_OK, "Fox 'Walk' can be paused");
    check(af_engine_imported_is_playing(engine) == 0, "a paused imported clip reports itself as not playing");
    const std::vector<float> paused = capturePositions(engine);
    check(af_engine_imported_advance(engine, 0.25f) == AF_OK, "advancing a paused clip is accepted");
    checkf(compare(paused, capturePositions(engine), 0.0f).maxDelta == 0.0f,
           "advancing while paused does not move the pose (max delta %.8f)",
           static_cast<double>(compare(paused, capturePositions(engine), 0.0f).maxDelta), 0.0);
    check(af_engine_imported_resume(engine) == AF_OK, "Fox 'Walk' can be resumed");
    check(af_engine_imported_is_playing(engine) == 1, "a resumed imported clip reports itself as playing");
    check(af_engine_imported_advance(engine, 0.05f) == AF_OK, "advancing a resumed clip is accepted");
    checkf(compare(paused, capturePositions(engine), extent * 1e-4f).moved > 0,
           "resuming makes the next advance move the pose again (%.1f%% of coordinates changed)",
           static_cast<double>(fraction(compare(paused, capturePositions(engine), extent * 1e-4f))), 0.0);
    check(af_engine_imported_advance(engine, std::nanf("")) != AF_OK, "advancing by NaN is refused");
    check(af_engine_imported_advance(engine, std::nanf("")) != AF_OK ||
              compare(paused, capturePositions(engine), 0.0f).maxDelta >= 0.0f,
          "a refused advance leaves the pose defined");

    /* ---- the boundary between the two skeletons ----
       AF_CLIP_* names the engine's AUTHORED library, which belongs to the generated rig. An import sets
       that rig aside, so apply_clip must refuse rather than quietly do nothing: NOT_READY is exactly what
       tells a caller (the GLES renderer's session setup, for one) to drive the file through
       af_engine_imported_* instead. Silently succeeding here would hide the branch. */
    const std::vector<float> beforeAuthored = capturePositions(engine);
    check(af_engine_apply_clip(engine, AF_CLIP_WALK, 0.1f) == static_cast<int32_t>(AF_ERR_NOT_READY),
          "the authored AF_CLIP_WALK is refused on an imported file, whose rig it does not describe");
    check(af_engine_apply_clip(engine, AF_CLIP_REST, 0.0f) == static_cast<int32_t>(AF_ERR_NOT_READY),
          "even AF_CLIP_REST is refused on an imported file rather than silently doing nothing");
    checkf(compare(beforeAuthored, capturePositions(engine), 0.0f).maxDelta == 0.0f,
           "a refused authored-clip call leaves the imported pose untouched (max delta %.8f)",
           static_cast<double>(compare(beforeAuthored, capturePositions(engine), 0.0f).maxDelta), 0.0);
    check(af_engine_imported_seek(engine, foxWalk * 0.5f) == AF_OK,
          "the imported transport still works after the authored calls were refused");
    checkf(compare(mid, capturePositions(engine), 0.0f).maxDelta == 0.0f,
           "and still lands on the same mid-clip pose (max delta %.8f)",
           static_cast<double>(compare(mid, capturePositions(engine), 0.0f).maxDelta), 0.0);

    /* ---- refusals ---- */
    check(af_engine_imported_play(engine, 99) != AF_OK,
          "playing a clip index the file does not have is refused");
    check(af_engine_imported_seek(engine, std::nanf("")) != AF_OK, "seeking to NaN is refused");
    check(af_engine_imported_clip_name(engine, -1, nameBuf, sizeof(nameBuf)) != AF_OK,
          "naming a negative clip index is refused");
    /* A refused play must not have left a half-selected clip behind. */
    check(af_engine_imported_seek(engine, 0.1f) == AF_OK ||
              af_engine_imported_seek(engine, 0.1f) == static_cast<int32_t>(AF_ERR_NOT_READY),
          "the transport is still in a defined state after a refused play");

    /* ------------------------------------------------------------- 3. CesiumMan.glb (metres) ----
       A second asset in different units with a different joint count, so the thresholds above cannot
       be quietly tuned to one file. */
    std::vector<uint8_t> cesiumBytes;
    if (readFile(dir + "/CesiumMan.glb", &cesiumBytes)) {
        check(af_engine_import_glb_from_memory(engine, cesiumBytes.data(),
                                               static_cast<int32_t>(cesiumBytes.size())) == AF_OK,
              "CesiumMan.glb imports");
        checkf(af_engine_imported_clip_count(engine) == 1,
               "CesiumMan exposes its single animation clip (got %.0f, expected %.0f)",
               static_cast<double>(af_engine_imported_clip_count(engine)), 1.0);
        /* Its skin declares 19 joints; the adopted skeleton adds closure nodes for the hierarchy above
           them, so it carries at least the 19 the file names. */
        const int32_t cJoints = af_engine_joint_count(engine);
        checkf(cJoints >= 19 && cJoints <= 24,
               "CesiumMan adopts its 19-joint skin plus hierarchy closure (got %.0f joints)",
               static_cast<double>(cJoints), 19.0);
        const float cDur = af_engine_imported_clip_duration(engine, 0);
        check(cDur > 1.5f && cDur < 2.5f, "CesiumMan's clip duration comes from the file (~2.0 s)");
        const std::vector<float> cRest = captureRestPositions(engine);
        const std::vector<float> cBind = capturePositions(engine);
        const float cExtent = extentOf(cRest);
        checkf(cExtent > 0.5f && cExtent < 10.0f,
               "CesiumMan is metre-authored, so its extent is a different order from Fox's (%.3f units)",
               static_cast<double>(cExtent), 0.0);
        checkf(compare(cRest, cBind, 0.0f).maxDelta < cExtent * 1e-4f,
               "CesiumMan also imports into its bind pose (max delta %.8f)",
               static_cast<double>(compare(cRest, cBind, 0.0f).maxDelta), 0.0);
        check(af_engine_imported_play(engine, 0) == AF_OK, "CesiumMan's clip can be selected");
        check(af_engine_imported_seek(engine, cDur * 0.5f) == AF_OK, "CesiumMan samples at mid-clip");
        const std::vector<float> cMid = capturePositions(engine);
        const Motion cm = compare(cBind, cMid, cExtent * 1e-4f);
        checkf(cm.moved > 0 && cm.maxDelta > cExtent * 0.005f,
               "CesiumMan's geometry moves at mid-clip, measured in metres (%.4f vs extent %.3f)",
               static_cast<double>(cm.maxDelta), static_cast<double>(cExtent));
        check(af_engine_imported_seek(engine, cDur * 0.25f) == AF_OK, "CesiumMan samples at t=0.25");
        check(af_engine_imported_seek(engine, cDur * 0.5f) == AF_OK, "CesiumMan returns to mid-clip");
        checkf(compare(cMid, capturePositions(engine), 0.0f).maxDelta == 0.0f,
               "CesiumMan reproduces mid-clip exactly on a second sample (max delta %.10f)",
               static_cast<double>(compare(cMid, capturePositions(engine), 0.0f).maxDelta), 0.0);
        checkf(compare(cRest, captureRestPositions(engine), 0.0f).maxDelta == 0.0f,
               "CesiumMan's bind-pose copy is untouched by posing (max delta %.10f)", 0.0, 0.0);
    } else {
        skip("CesiumMan.glb", dir);
    }

    /* ------------------------------------------- 4. files that cannot be skinned at all ----
       AnimatedMorphCube has no skin, so it has no inverse bind matrices. The animation core will still
       build it a node-only skeleton, because the gltfio backend can drive its morph weights -- but this
       backend deforms by CPU skinning and has nothing to deform with. Adopting the clip anyway would
       advertise animation the engine cannot play, so the import refuses and says why. */
    std::vector<uint8_t> cubeBytes;
    if (readFile(dir + "/AnimatedMorphCube.glb", &cubeBytes)) {
        check(af_engine_import_glb_from_memory(engine, cubeBytes.data(),
                                               static_cast<int32_t>(cubeBytes.size())) == AF_OK,
              "AnimatedMorphCube.glb imports");
        check(af_engine_joint_count(engine) == 0,
              "a file that cannot be skinned adopts no skeleton rather than inventing one");
        check(af_engine_imported_clip_count(engine) == 0,
              "a file that cannot be skinned adopts no clips -- its morph-only animation is not faked");
        const std::vector<float> cubeBefore = capturePositions(engine);
        check(af_engine_imported_play(engine, 0) == static_cast<int32_t>(AF_ERR_NOT_READY),
              "playing a clip on an unskinnable import is refused");
        check(af_engine_imported_seek(engine, 1.0f) == static_cast<int32_t>(AF_ERR_NOT_READY),
              "seeking an unskinnable import returns NOT_READY");
        check(af_engine_imported_advance(engine, 0.016f) == static_cast<int32_t>(AF_ERR_NOT_READY),
              "advancing an unskinnable import returns NOT_READY");
        check(af_engine_imported_is_playing(engine) == 0,
              "an unskinnable import never reports itself as playing");
        check(compare(cubeBefore, capturePositions(engine), 0.0f).maxDelta == 0.0f,
              "an unskinnable import leaves the geometry untouched");
    } else {
        skip("AnimatedMorphCube.glb", dir);
    }

    /* =========================================================================================
       CliplessRigged.glb: a skinned humanoid with inverse bind matrices and ZERO animations.
       --------------------------------------------------------------------------------------------
       This is the case mylove_rig_prepared.glb (20 joints, 17/17 humanoid roles, 0 animations) falls
       into, and it is a DIFFERENT failure from female.glb below: that file cannot be deformed at all,
       this one can be deformed but has nothing to play. Until the authored-library role-binding
       fallback, a file like this imported to joint_count=0 and imported_clip_count=0 and stayed static
       forever while logging nothing actionable.

       The fixture is synthetic and small (4 KB, 17 joints, 8 vertices) and is regenerated by
       tools/make_clipless_rigged_glb.py. Four of its eight vertices are rigidly bound to LeftLowerArm
       and four to Hips, which makes it possible to prove the skinning is per-joint rather than a global
       transform: a clip that rotates only the arm must move exactly the arm's vertices. */
    {
        std::vector<uint8_t> cliplessBytes;
        const std::string cliplessPath = dir + "/CliplessRigged.glb";
        if (!readFile(cliplessPath, &cliplessBytes)) {
            skip("CliplessRigged.glb", cliplessPath);
        } else {
            check(af_engine_import_glb_from_memory(engine, cliplessBytes.data(),
                                                   static_cast<int32_t>(cliplessBytes.size())) == AF_OK,
                  "a rigged file with no animations imports successfully");
            const int32_t joints = af_engine_joint_count(engine);
            checkf(joints >= 17 && joints <= 18,
                   "the authored library was bound to its skeleton: %.0f joints adopted (file declares 17)",
                   static_cast<double>(joints), 17.0);
            const int32_t clips = af_engine_imported_clip_count(engine);
            checkf(clips == 14,
                   "it advertises %.0f imported clips, taken from the engine's authored library",
                   static_cast<double>(clips), 14.0);
            check(af_engine_imported_is_playing(engine) == 0,
                  "and it is NOT already playing: a fresh import lands in the bind pose");

            /* The two calibration poses must be absent by name, not merely uncounted. */
            bool sawTPose = false, sawAPose = false, sawWalk = false, sawDance = false, sawWave = false;
            for (int32_t i = 0; i < clips; ++i) {
                char nm[64] = {0};
                if (af_engine_imported_clip_name(engine, i, nm, sizeof(nm)) != AF_OK) continue;
                const std::string n(nm);
                if (n == "t_pose") sawTPose = true;
                if (n == "a_pose") sawAPose = true;
                if (n == "walk") sawWalk = true;
                if (n == "dance") sawDance = true;
                if (n == "wave") sawWave = true;
            }
            check(!sawTPose && !sawAPose,
                  "t_pose and a_pose are withheld: on a foreign skeleton they describe the generated "
                  "rig's rest shape and swing arms that are already out backward");
            check(sawWalk && sawDance && sawWave,
                  "walk, dance and wave are all selectable by name on the imported skeleton");

            const std::vector<float> rest = captureRestPositions(engine);
            const std::vector<float> bind = capturePositions(engine);
            const float cextent = extentOf(rest);
            checkf(cextent > 0.1f && cextent < 5.0f,
                   "the fixture's own extent is %.4f units, so displacement can be judged relative to it",
                   static_cast<double>(cextent), 0.0);
            const Motion cb = compare(rest, bind, cextent * 1e-5f);
            checkf(cb.maxDelta < cextent * 1e-4f,
                   "before anything is played the imported vertices reproduce the file's bind positions "
                   "(max delta %.8f vs extent %.4f)",
                   static_cast<double>(cb.maxDelta), static_cast<double>(cextent));

            /* dance rotates 17 of the fixture's joints: most vertices must move, and by a meaningful
               fraction of the model's own size. */
            int32_t danceIndex = -1;
            for (int32_t i = 0; i < clips; ++i) {
                char nm[64] = {0};
                if (af_engine_imported_clip_name(engine, i, nm, sizeof(nm)) != AF_OK) continue;
                if (std::string(nm) == "dance") { danceIndex = i; break; }
            }
            check(danceIndex >= 0, "dance can be located by name in the imported clip list");
            if (danceIndex >= 0) {
                const float dur = af_engine_imported_clip_duration(engine, danceIndex);
                checkf(dur > 0.0f, "dance has a usable duration (%.4f s)", static_cast<double>(dur), 0.0);
                check(af_engine_imported_play(engine, danceIndex) == AF_OK, "dance plays");
                check(af_engine_imported_seek(engine, dur * 0.5f) == AF_OK, "and can be seeked");
                const std::vector<float> mid = capturePositions(engine);
                const Motion m = compare(bind, mid, cextent * 1e-5f);
                checkf(fraction(m) > 50.0f,
                       "at mid-clip %.1f%% of the fixture's %.0f coordinates have moved",
                       static_cast<double>(fraction(m)), static_cast<double>(m.coords));
                checkf(m.maxDelta > cextent * 1e-3f,
                       "and the largest displacement is %.6f, which is %.3f%% of the model's extent",
                       static_cast<double>(m.maxDelta),
                       static_cast<double>(100.0f * m.maxDelta / cextent));
                check(m.finite, "every deformed coordinate is finite");

                /* Re-sampling the same instant must reproduce exactly: deformation is evaluated from the
                   bind pose each time, never accumulated onto what is currently displayed. */
                const std::vector<float> again = capturePositions(engine);
                const Motion r = compare(mid, again, 0.0f);
                checkf(r.maxDelta == 0.0f,
                       "re-reading the same pose reproduces it bit for bit (max delta %.8f)",
                       static_cast<double>(r.maxDelta), 0.0);
                const std::vector<float> restAfter = captureRestPositions(engine);
                const Motion rr = compare(rest, restAfter, 0.0f);
                checkf(rr.maxDelta == 0.0f,
                       "af_engine_vertices_rest is untouched by posing (max delta %.8f)",
                       static_cast<double>(rr.maxDelta), 0.0);

                check(af_engine_imported_advance(engine, dur * 1.5f) == AF_OK, "advance succeeds");
                const std::vector<float> wrapped = capturePositions(engine);
                const std::vector<float> halfAgain = capturePositions(engine);
                const Motion w = compare(wrapped, halfAgain, 0.0f);
                checkf(w.maxDelta == 0.0f, "reading twice without advancing does not drift (%.8f)",
                       static_cast<double>(w.maxDelta), 0.0);

                check(af_engine_imported_pause(engine) == AF_OK, "the imported clock can be paused");
                const std::vector<float> pausedA = capturePositions(engine);
                check(af_engine_imported_advance(engine, 0.25f) == AF_OK, "advance while paused returns OK");
                const Motion pm = compare(pausedA, capturePositions(engine), 0.0f);
                checkf(pm.maxDelta == 0.0f, "and moves nothing while paused (max delta %.8f)",
                       static_cast<double>(pm.maxDelta), 0.0);
                check(af_engine_imported_resume(engine) == AF_OK, "and resumed");
                check(af_engine_imported_advance(engine, 0.05f) == AF_OK, "advance after resume returns OK");
                const Motion rm = compare(pausedA, capturePositions(engine), cextent * 1e-6f);
                checkf(rm.moved > 0, "resume really restarts motion: %.0f of %.0f coordinates moved",
                       static_cast<double>(rm.moved), static_cast<double>(rm.coords));
            }

            /* wave rotates ONLY the left arm chain and the head, never the hips. Four vertices are bound
               to LeftLowerArm and four to Hips, so this separates real per-joint skinning from a global
               transform that would move everything and still pass every check above. */
            int32_t waveIndex = -1;
            for (int32_t i = 0; i < clips; ++i) {
                char nm[64] = {0};
                if (af_engine_imported_clip_name(engine, i, nm, sizeof(nm)) != AF_OK) continue;
                if (std::string(nm) == "wave") { waveIndex = i; break; }
            }
            if (waveIndex >= 0) {
                check(af_engine_imported_play(engine, waveIndex) == AF_OK, "wave plays");
                check(af_engine_imported_seek(engine, 0.0f) == AF_OK, "wave can be seeked to its first key");
                const std::vector<float> posed = capturePositions(engine);
                const size_t armVerts[] = {2, 3, 6, 7};
                const size_t hipVerts[] = {0, 1, 4, 5};
                auto maxOver = [&](const size_t* which, size_t n) {
                    float worst = 0.0f;
                    for (size_t k = 0; k < n; ++k) {
                        for (int c = 0; c < 3; ++c) {
                            const size_t i = which[k] * 3 + static_cast<size_t>(c);
                            if (i < posed.size() && i < bind.size()) {
                                const float d = std::fabs(posed[i] - bind[i]);
                                if (d > worst) worst = d;
                            }
                        }
                    }
                    return worst;
                };
                const float armDelta = maxOver(armVerts, 4);
                const float hipDelta = maxOver(hipVerts, 4);
                checkf(armDelta > cextent * 1e-3f,
                       "wave moves the arm-bound vertices (%.6f, which is %.3f%% of the extent)",
                       static_cast<double>(armDelta),
                       static_cast<double>(100.0f * armDelta / cextent));
                checkf(hipDelta < cextent * 1e-5f,
                       "and leaves the hips-bound vertices exactly where the bind pose put them (%.8f)",
                       static_cast<double>(hipDelta), static_cast<double>(cextent));
            }
        }
    }

    /* female.glb: 0 skins AND 0 animations -- the common case for a sculpted avatar. */
    {
        std::vector<uint8_t> femaleBytes;
        const std::string femalePath = dir + "/../female.glb";
        if (readFile(femalePath, &femaleBytes)) {
            check(af_engine_import_glb_from_memory(engine, femaleBytes.data(),
                                                   static_cast<int32_t>(femaleBytes.size())) == AF_OK,
                  "female.glb imports");
            check(af_engine_imported_clip_count(engine) == 0,
                  "a file with no animations advertises no imported clips");
            check(af_engine_imported_seek(engine, 0.5f) == static_cast<int32_t>(AF_ERR_NOT_READY),
                  "a file with no animations refuses to be posed");
            check(af_engine_imported_advance(engine, 0.016f) == static_cast<int32_t>(AF_ERR_NOT_READY),
                  "a file with no animations refuses to be advanced");
        } else {
            skip("female.glb", femalePath);
        }
    }

    af_engine_destroy(engine);
    if (gFailures == 0) {
        std::printf("\nAll imported-animation checks passed.\n");
        return 0;
    }
    std::fprintf(stderr, "\n%d imported-animation check(s) FAILED\n", gFailures);
    return 1;
}
