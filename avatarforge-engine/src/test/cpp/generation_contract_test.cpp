/*
 * generation_contract_test.cpp -- host regression for the GENERATION side of AvatarForge.
 *
 * Sibling tests already prove a generated avatar imports, deforms and exports (imported_glb_pbr_test
 * runs af_engine_create -> run_all -> deformation -> export). What nothing tested is the contract the
 * public header itself makes and that "generation works correctly" actually means:
 *
 *   DETERMINISM.  afcore.h: "identical af_spec + identical seed => byte-identical geometry."
 *     This test generates the SAME spec+seed twice and requires the two exported GLBs to be
 *     byte-identical, then generates a DIFFERENT seed and requires the output to differ - so the
 *     equality check cannot pass vacuously on constant output. Nondeterminism here (thread-reduction
 *     order, uninitialised memory leaking into the file, RNG not seeded from the spec) would surface
 *     as a named failure with the first divergent byte offset, instead of silently shipping avatars
 *     that differ run-to-run for the same prompt.
 *
 *   COMPLETENESS.  A generated avatar must carry the production rig and face exactly: 71 named joints
 *     (including the twist bones), all 52 ARKit-compatible blendshapes, every one of the five PBR
 *     texture slots populated, a non-empty validated mesh and materials, and a working authored clip
 *     library. These are asserted against the engine's OWN read-back (af_stats, af_engine_joint_info,
 *     af_engine_blendshape_names, af_engine_texture, af_engine_clip_duration), so a future change that
 *     quietly drops a joint set, a morph channel or a texture map fails here.
 *
 *   ROUND-TRIP.  The exported bytes must re-import through the engine's hardened reader
 *     (af_engine_import_glb_from_memory) with rc==AF_OK and an empty af_engine_last_error, adopting a
 *     71-joint skeleton - i.e. the file the app receives is one the app can read back.
 *
 * Nothing is mocked and nothing is synthesised to pass; every assertion reads real generated data.
 * COST NOTE: it runs the full pipeline up to three times (two identical seeds for the equality check,
 * one different seed for the inequality control), so it is the slowest host test and carries its own
 * generous ctest timeout. That cost is deliberate: determinism cannot be proven from a single sample.
 */
#include "afcore.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool cond, const char* msg) {
    ++g_checks;
    if (cond) {
        std::printf("PASS  %s\n", msg);
    } else {
        std::fprintf(stderr, "FAIL  %s\n", msg);
        ++g_failures;
    }
}

void checkf(bool cond, const char* fmt, double a, double b) {
    ++g_checks;
    if (cond) {
        std::printf("PASS  ");
        std::printf(fmt, a, b);
        std::printf("\n");
    } else {
        std::fprintf(stderr, "FAIL  ");
        std::fprintf(stderr, fmt, a, b);
        std::fprintf(stderr, "\n");
        ++g_failures;
    }
}

/* Deterministic spec: the canonical seed-1337 female/athletic/medium/jeans-jacket avatar, with every
   field pinned so two calls cannot drift. af_spec_init fills defaults (and struct_size); we then
   override. Tone arrays left at their documented sentinels except where pinning removes ambiguity. */
void fillSpec(af_spec* s, int32_t seed) {
    *s = af_spec{};
    af_spec_init(s);
    s->sex        = AF_SEX_FEMALE;
    s->build      = AF_BUILD_ATHLETIC;
    s->hair_style = AF_HAIR_MEDIUM;
    s->outfit     = AF_OUTFIT_JEANS_JACKET;
    s->footwear   = AF_FOOTWEAR_SNEAKER;
    s->seed       = seed;
    s->height_m   = 1.75f;
    s->age_years  = 28.0f;
    s->muscle     = 0.50f;
    s->body_fat   = 0.25f;
    s->bust       = 0.50f;
    s->skin_melanin = 0.30f;
    /* Explicit hair/cloth/gear tones so the result does not depend on any auto-derivation path. */
    s->hair_tone[0] = 0.10f; s->hair_tone[1] = 0.07f; s->hair_tone[2] = 0.05f;
    s->cloth_tone[0] = 0.20f; s->cloth_tone[1] = 0.30f; s->cloth_tone[2] = 0.55f;
    s->gear_tone[0] = 0.55f; s->gear_tone[1] = 0.55f; s->gear_tone[2] = 0.60f;
    s->cloth_roughness = -1.0f;   /* <0 => material default */
    s->gear_metalness  = -1.0f;
    std::snprintf(s->prompt, sizeof(s->prompt),
                  "athletic woman, medium hair, jeans jacket, sneaker");
}

double nowMs() {
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

/* Forward declarations: the objective texture-quality metrics are defined further down (beside the
   other file helpers) but are needed earlier, inside generateAndExport's inspection pass. */
double channelMean(const uint8_t* px, int w, int h, int ch, int c);
double channelStdDev(const uint8_t* px, int w, int h, int ch, int c, double mean);
double luminanceStdDev(const uint8_t* px, int w, int h, int ch);
void   decodedNormalStats(const uint8_t* px, int w, int h, int ch,
                          double* sdNx, double* sdNy, double* meanNz);

/* Runs the full pipeline for `seed` and writes the resulting GLB to `outPath`. When `inspect` is set,
   asserts the live-engine structural contract (Part B) before export. Returns true iff every stage
   succeeded; records wall-clock generation time into *elapsedMs when provided. */
bool generateAndExport(int32_t seed, const char* outPath, bool inspect, double* elapsedMs) {
    const double t0 = nowMs();
    af_spec spec;
    fillSpec(&spec, seed);
    af_engine* engine = nullptr;
    if (af_engine_create(&spec, nullptr, &engine) != AF_OK || engine == nullptr) {
        std::fprintf(stderr, "FAIL  af_engine_create (seed=%d)\n", (int)seed);
        ++g_failures; ++g_checks;
        return false;
    }

    const int32_t runRc = af_engine_run_all(engine);
    check(runRc == AF_OK, "af_engine_run_all completes the full generation pipeline");
    if (runRc != AF_OK) {
        std::fprintf(stderr, "      run_all rc=%d last_error=\"%s\"\n", (int)runRc,
                     af_engine_last_error(engine) ? af_engine_last_error(engine) : "");
        af_engine_destroy(engine);
        return false;
    }

    if (inspect) {
        /* ---- Part B: structural completeness, read back from the engine itself ---- */
        af_stats st{};
        check(af_engine_stats(engine, &st) == AF_OK, "af_engine_stats reports the generated avatar");
        check(st.vertex_count > 0 && st.triangle_count > 0 && st.index_count > 0,
              "generated mesh has vertices, indices and triangles");
        check(st.submesh_count >= 1, "generated mesh has at least one submesh");
        checkf(st.joint_count == 71,
               "generated rig has exactly 71 joints (got %.0f, expected 71)", (double)st.joint_count, 71.0);
        checkf(st.blendshape_count == 52,
               "generated face has all 52 ARKit blendshapes (got %.0f, expected 52)",
               (double)st.blendshape_count, 52.0);
        checkf(af_engine_joint_count(engine) == st.joint_count,
               "joint accessor agrees with af_stats (%.0f vs %.0f)",
               (double)af_engine_joint_count(engine), (double)st.joint_count);
        check(af_engine_material_count(engine) >= 1, "generated avatar has at least one material");

        /* Joint NAMES, including the twist bones whose absence would silently soften deformations. */
        std::string allJoints;
        bool jointReadOk = true;
        for (int32_t j = 0; j < st.joint_count; ++j) {
            af_joint_info ji{};
            if (af_engine_joint_info(engine, j, &ji) != AF_OK) { jointReadOk = false; break; }
            allJoints += ji.name;
            allJoints += ",";
        }
        check(jointReadOk, "every joint exposes readable info");
        const char* requiredJoints[] = {"hips", "head", "hand_L", "hand_R",
                                        "toe_L", "toe_R", "upperarm_twist_L", "thigh_twist_R"};
        bool missingJoint = false;
        for (const char* rj : requiredJoints) {
            if (allJoints.find(rj) == std::string::npos) {
                std::fprintf(stderr, "      missing expected joint name: %s\n", rj);
                missingJoint = true;
            }
        }
        check(!missingJoint, "rig contains hips/head/hands/toes and the twist bones by name");

        /* Blendshape NAMES: the 52 ARKit targets must actually be named, not just counted. */
        char bs[64][64];
        const int32_t bsCount = af_engine_blendshape_names(bs, 64);
        checkf(bsCount == 52, "af_engine_blendshape_names returns 52 (got %.0f)", (double)bsCount, 0.0);
        std::string allBs;
        for (int32_t i = 0; i < bsCount && i < 64; ++i) { allBs += bs[i]; allBs += ","; }
        const char* requiredBs[] = {"eyeBlinkLeft", "eyeLookUpLeft", "browDownLeft", "cheekPuff"};
        bool missingBs = false;
        for (const char* rb : requiredBs) {
            if (allBs.find(rb) == std::string::npos) {
                std::fprintf(stderr, "      missing expected ARKit blendshape: %s\n", rb);
                missingBs = true;
            }
        }
        check(!missingBs, "blendshapes include canonical ARKit names (blink/look/brow/cheek)");

        /* Every PBR slot populated by the bake. While verifying population we also extract OBJECTIVE
           quality metrics from the live pixels (Part B-QUALITY), so "the avatar looks flat/wrong" is a
           number each CI run records and a fix must measurably move -- not an opinion. */
        const int32_t slots[] = {AF_TEX_BASE_COLOR, AF_TEX_NORMAL, AF_TEX_METALLIC_ROUGHNESS,
                                 AF_TEX_OCCLUSION, AF_TEX_SUBSURFACE};
        const char* slotName[] = {"BASE_COLOR", "NORMAL", "METALLIC_ROUGHNESS", "OCCLUSION", "SUBSURFACE"};
        bool allTexturesOk = true;
        double baseLumSd = 0.0;                     /* tonal spread of the base-colour map          */
        double normalRawSdR = 0.0, normalRawSdG = 0.0; /* tangent-plane detail in the normal map    */
        double normalSdNx = 0.0, normalSdNy = 0.0, normalMeanNz = 0.0; /* decoded deviation + flatness */
        for (int i = 0; i < 5; ++i) {
            const int32_t slot = slots[i];
            af_texture t{};
            const int32_t trc = af_engine_texture(engine, slot, &t);
            if (trc != AF_OK || t.data == nullptr || t.width <= 0 || t.height <= 0 || t.channels <= 0) {
                std::fprintf(stderr, "      texture slot %s not populated (rc=%d w=%d h=%d ch=%d data=%p)\n",
                             slotName[i], (int)trc, t.width, t.height, t.channels, (const void*)t.data);
                allTexturesOk = false;
                continue;
            }
            const size_t level0Bytes = (size_t)t.width * (size_t)t.height * (size_t)t.channels;
            if ((uint64_t)level0Bytes > t.byte_size) {
                std::fprintf(stderr, "      texture slot %s: level0 bytes %zu exceed byte_size %llu; skipping metrics\n",
                             slotName[i], level0Bytes, (unsigned long long)t.byte_size);
                continue; /* populated but cannot be read safely; report, do not crash */
            }
            const int nCh = t.channels < 8 ? t.channels : 8;
            double mean[8] = {}, sd[8] = {};
            for (int c = 0; c < nCh; ++c) {
                mean[c] = channelMean(t.data, t.width, t.height, t.channels, c);
                sd[c]   = channelStdDev(t.data, t.width, t.height, t.channels, c, mean[c]);
            }
            std::printf("QUALITY %-18s %dx%dx%d ", slotName[i], t.width, t.height, t.channels);
            for (int c = 0; c < nCh; ++c) std::printf("c%d(mean=%.1f sd=%.2f) ", c, mean[c], sd[c]);
            std::printf("\n");
            if (slot == AF_TEX_BASE_COLOR && nCh >= 3) {
                baseLumSd = luminanceStdDev(t.data, t.width, t.height, t.channels);
            } else if (slot == AF_TEX_NORMAL && nCh >= 3) {
                normalRawSdR = sd[0];
                normalRawSdG = sd[1];
                decodedNormalStats(t.data, t.width, t.height, t.channels,
                                   &normalSdNx, &normalSdNy, &normalMeanNz);
            }
        }
        check(allTexturesOk, "all five procedural PBR texture slots are populated with real pixels");
        /* RATCHETED APPEARANCE BASELINES. These are regression floors, not aspirational targets: each
           sits safely below the magnitudes measured after commit f5fdcea (garments gained real woven +
           stitched relief), so any future change that flattens the surface back toward the old painted
           regime fails here loudly. They are stable because the spec+seed above is pinned, which makes
           the baked pixels -- and therefore these statistics -- byte-reproducible run to run.
             measured @ f5fdcea : rawSdR 9.56, rawSdG 5.39, sdNx 0.075, sdNy 0.042, meanNz 0.996
             previous (flat)    : rawSdR 4.14, rawSdG 2.14, sdNx 0.032, sdNy 0.017, meanNz ~0.999   */
        check(baseLumSd > 1.0,
              "base-colour map has tonal spread (not one flat tone): luminance stddev > 1.0 on 0-255");
        check(normalRawSdR > 6.0 && normalRawSdG > 4.0,
              "normal map keeps garment weave/stitch relief: R/G channel stddev > 6.0 / > 4.0");
        check(normalSdNx > 0.05 && normalSdNy > 0.03,
              "normal map deviates from flat in tangent space: decoded sdNx > 0.05 and sdNy > 0.03");
        check(normalMeanNz < 0.9988,
              "surface is not globally flat: normal mean Z < 0.9988 (relief present somewhere)");
        std::printf("DETAIL  baseColour lumSd=%.2f | normal raw(sdR=%.2f sdG=%.2f) decoded(sdNx=%.3f sdNy=%.3f meanNz=%.3f)\n",
                    baseLumSd, normalRawSdR, normalRawSdG, normalSdNx, normalSdNy, normalMeanNz);

        /* Authored clip library responds with finite, positive durations (REST excluded: it is 0). */
        const int32_t clips[] = {AF_CLIP_IDLE, AF_CLIP_WALK, AF_CLIP_JUMP, AF_CLIP_TURN_360, AF_CLIP_DANCE};
        bool clipsOk = true;
        for (int32_t c : clips) {
            const float d = af_engine_clip_duration(engine, c);
            if (!(std::isfinite(d) && d > 0.0f)) {
                std::fprintf(stderr, "      clip %d has bad duration %f\n", (int)c, (double)d);
                clipsOk = false;
            }
        }
        check(clipsOk, "idle/walk/jump/turn_360/dance clips have finite positive durations");

        /* Geometry sanity at rest: finite positions, valid weights/joints, bounds pass. */
        char report[2048] = {};
        check(af_engine_validate_deformed_mesh(engine, report, (int32_t)sizeof(report)) == AF_OK,
              "generated rest mesh passes finite-position / weight / joint / bounds validation");

        std::printf("STATS verts=%d tris=%d submeshes=%d joints=%d blendshapes=%d mats=%d "
                    "tex=%.2fMiB geo=%.2fMiB peakField=%.2fMiB\n",
                    st.vertex_count, st.triangle_count, st.submesh_count, st.joint_count,
                    st.blendshape_count, af_engine_material_count(engine),
                    (double)st.texture_bytes / 1048576.0, (double)st.geometry_bytes / 1048576.0,
                    (double)st.peak_field_bytes / 1048576.0);
    }

    /* Export at the canonical rest pose so the file does not encode a transient deformation. */
    check(af_engine_apply_clip(engine, AF_CLIP_REST, 0.0f) == AF_OK,
          "generated avatar returns to its rest pose before export");
    const int32_t expRc = af_engine_export_glb(engine, outPath);
    check(expRc == AF_OK, "generated avatar exports a self-contained GLB to disk");
    if (expRc != AF_OK) {
        std::fprintf(stderr, "      export_glb rc=%d last_error=\"%s\"\n", (int)expRc,
                     af_engine_last_error(engine) ? af_engine_last_error(engine) : "");
    }
    af_engine_destroy(engine);
    if (elapsedMs) *elapsedMs = nowMs() - t0;
    return expRc == AF_OK;
}

/* Compares two files byte-for-byte without holding them both in memory. Reports sizes and the first
   divergent offset so a determinism failure is diagnosable rather than mysterious. */
bool filesIdentical(const char* a, const char* b, long* sizeA, long* sizeB, long* firstDiff) {
    std::FILE* fa = std::fopen(a, "rb");
    std::FILE* fb = std::fopen(b, "rb");
    if (!fa || !fb) { if (fa) std::fclose(fa); if (fb) std::fclose(fb); return false; }
    std::fseek(fa, 0, SEEK_END); *sizeA = std::ftell(fa); std::fseek(fa, 0, SEEK_SET);
    std::fseek(fb, 0, SEEK_END); *sizeB = std::ftell(fb); std::fseek(fb, 0, SEEK_SET);
    if (*sizeA != *sizeB) { *firstDiff = -1; std::fclose(fa); std::fclose(fb); return false; }
    const size_t CHUNK = 1u << 20;
    std::vector<uint8_t> ba(CHUNK), bb(CHUNK);
    long offset = 0;
    bool same = true;
    for (;;) {
        const size_t na = std::fread(ba.data(), 1, CHUNK, fa);
        const size_t nb = std::fread(bb.data(), 1, CHUNK, fb);
        if (na != nb) { same = false; *firstDiff = offset + (long)(na < nb ? na : nb); break; }
        if (na == 0) break;
        if (std::memcmp(ba.data(), bb.data(), na) != 0) {
            for (size_t i = 0; i < na; ++i) {
                if (ba[i] != bb[i]) { *firstDiff = offset + (long)i; break; }
            }
            same = false;
            break;
        }
        offset += (long)na;
    }
    std::fclose(fa); std::fclose(fb);
    return same;
}

/* Reads an entire file into memory (used for the self-import round-trip). */
bool readFile(const char* path, std::vector<uint8_t>* out) {
    std::FILE* fp = std::fopen(path, "rb");
    if (!fp) return false;
    std::fseek(fp, 0, SEEK_END);
    const long sz = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);
    if (sz <= 0) { std::fclose(fp); return false; }
    out->resize((size_t)sz);
    const size_t got = std::fread(out->data(), 1, out->size(), fp);
    std::fclose(fp);
    return got == out->size();
}

/* ------------------------------------------------------------------------------------------
 * Objective texture-quality metrics over a LEVEL-0 8-bit image (width*height*channels bytes).
 * These exist so "the avatar looks flat/wrong" becomes a number every CI run records and a
 * future fix must measurably move -- the qualitative analogue of the byte-determinism check.
 * Nothing here renders; it reads the same engine-owned buffers af_engine_texture hands back.
 * -------------------------------------------------------------------------------------------- */
double channelMean(const uint8_t* px, int w, int h, int ch, int c) {
    if (!px || w <= 0 || h <= 0 || ch <= 0 || c < 0 || c >= ch) return 0.0;
    const size_t n = (size_t)w * (size_t)h;
    double s = 0.0;
    for (size_t i = 0; i < n; ++i) s += (double)px[i * (size_t)ch + c];
    return s / (double)n;
}

double channelStdDev(const uint8_t* px, int w, int h, int ch, int c, double mean) {
    if (!px || w <= 0 || h <= 0 || ch <= 0 || c < 0 || c >= ch) return 0.0;
    const size_t n = (size_t)w * (size_t)h;
    double v = 0.0;
    for (size_t i = 0; i < n; ++i) { const double d = (double)px[i * (size_t)ch + c] - mean; v += d * d; }
    return std::sqrt(v / (double)n);
}

/* Luminance standard deviation (Rec.709 weights) on a >=3-channel map: a measure of tonal spread. */
double luminanceStdDev(const uint8_t* px, int w, int h, int ch) {
    if (!px || w <= 0 || h <= 0 || ch < 3) return 0.0;
    const size_t n = (size_t)w * (size_t)h;
    std::vector<double> l(n);
    double m = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const uint8_t* p = px + i * (size_t)ch;
        l[i] = 0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2];
        m += l[i];
    }
    m /= (double)n;
    double v = 0.0;
    for (size_t i = 0; i < n; ++i) { const double d = l[i] - m; v += d * d; }
    return std::sqrt(v / (double)n);
}

/* Tangent-space normal maps encode xyz in [0,255]->[-1,+1]. A perfectly flat surface is uniform
   (128,128,255): its decoded x/y standard deviation is ~0. Reporting that deviation quantifies how
   much real micro-relief the bake actually produced (skin pores, cloth weave, seams), independent
   of which tangent convention the map uses, because we measure spread rather than sign. */
void decodedNormalStats(const uint8_t* px, int w, int h, int ch,
                        double* sdNx, double* sdNy, double* meanNz) {
    *sdNx = 0.0; *sdNy = 0.0; *meanNz = 0.0;
    if (!px || w <= 0 || h <= 0 || ch < 3) return;
    const size_t n = (size_t)w * (size_t)h;
    std::vector<double> vx(n), vy(n);
    double sx = 0.0, sy = 0.0, sz = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const uint8_t* p = px + i * (size_t)ch;
        const double dx = (double)p[0] / 255.0 * 2.0 - 1.0;
        const double dy = (double)p[1] / 255.0 * 2.0 - 1.0;
        const double dz = (double)p[2] / 255.0 * 2.0 - 1.0;
        vx[i] = dx; vy[i] = dy;
        sx += dx; sy += dy; sz += dz;
    }
    const double mx = sx / (double)n, my = sy / (double)n;
    double ex = 0.0, ey = 0.0;
    for (size_t i = 0; i < n; ++i) { ex += (vx[i] - mx) * (vx[i] - mx); ey += (vy[i] - my) * (vy[i] - my); }
    *sdNx = std::sqrt(ex / (double)n);
    *sdNy = std::sqrt(ey / (double)n);
    *meanNz = sz / (double)n;
}

} // namespace

int main(void) {
    const char* PATH_A = "/tmp/af_generation_contract_A.glb";
    const char* PATH_B = "/tmp/af_generation_contract_B.glb";
    const char* PATH_C = "/tmp/af_generation_contract_C.glb";

    std::printf("=== Generation contract: determinism + completeness + round-trip ===\n");

    /* ---- Part A: determinism ---- */
    double msA = 0, msB = 0, msC = 0;
    const bool okA = generateAndExport(1337, PATH_A, /*inspect=*/true, &msA);
    const bool okB = generateAndExport(1337, PATH_B, /*inspect=*/false, &msB);
    std::printf("TIMING  gen(seed=1337) #1 = %.0f ms, #2 = %.0f ms\n", msA, msB);
    check(okA && okB, "both identical-seed generations exported");

    long sizeA = 0, sizeB = 0, firstDiff = -2;
    const bool identical = okA && okB && filesIdentical(PATH_A, PATH_B, &sizeA, &sizeB, &firstDiff);
    checkf(identical,
           "DETERMINISM: identical spec+seed => byte-identical GLB (%.0f bytes vs %.0f bytes)",
           (double)sizeA, (double)sizeB);
    if (!identical) {
        std::fprintf(stderr, "      sizes %ld vs %ld, first divergent byte offset %ld\n",
                     sizeA, sizeB, firstDiff);
    }

    /* Negative control: a different seed must NOT reproduce the same bytes, or the equality check
       above could pass vacuously on constant output. */
    const bool okC = generateAndExport(1338, PATH_C, /*inspect=*/false, &msC);
    std::printf("TIMING  gen(seed=1338) control = %.0f ms\n", msC);
    long sizeC = 0, dummyFirst = -2;
    const bool cDiffersFromA = okA && okC && !filesIdentical(PATH_A, PATH_C, &sizeA, &sizeC, &dummyFirst);
    check(cDiffersFromA, "CONTROL: a different seed produces a different GLB (equality check is not vacuous)");

    /* ---- Round-trip: the exported file must re-import through the engine's own hardened reader ---- */
    std::vector<uint8_t> exportedBytes;
    if (okA && readFile(PATH_A, &exportedBytes)) {
        af_spec rs;
        fillSpec(&rs, 1); /* contents irrelevant; the importer adopts the FILE's rig */
        af_engine* reimport = nullptr;
        check(af_engine_create(&rs, nullptr, &reimport) == AF_OK && reimport != nullptr,
              "round-trip: fresh engine created to re-read the export");
        if (reimport) {
            const int32_t rc = af_engine_import_glb_from_memory(
                reimport, exportedBytes.data(), (int32_t)exportedBytes.size());
            const char* why = af_engine_last_error(reimport);
            const bool silent = (why == nullptr) || (*why == '\0');
            check(rc == AF_OK && silent,
                  "round-trip: our exported GLB re-imports with rc==AF_OK and an empty last_error");
            if (rc != AF_OK || !silent) {
                std::fprintf(stderr, "      re-import rc=%d last_error=\"%s\"\n", (int)rc,
                             silent ? "<none>" : why);
            }
            checkf(af_engine_joint_count(reimport) == 71,
                   "round-trip: imported skeleton adopts 71 joints (got %.0f)",
                   (double)af_engine_joint_count(reimport), 0.0);
            af_engine_destroy(reimport);
        }
    } else {
        check(false, "round-trip: could not read back the exported GLB for re-import");
    }

    /* ---- Part C: prompt parsing is deterministic (front door of generation) ---- */
    {
        af_spec defSpec;
        fillSpec(&defSpec, 1);
        af_engine* pe = nullptr;
        if (af_engine_create(&defSpec, nullptr, &pe) == AF_OK && pe) {
            const char* prompt = "tall athletic woman, braids, jeans jacket";
            af_spec o1{}, o2{};
            const int32_t r1 = af_engine_analyze_prompt(pe, prompt, &o1);
            const int32_t r2 = af_engine_analyze_prompt(pe, prompt, &o2);
            checkf(r1 == r2 && r1 >= 1,
                   "analyze_prompt is deterministic and recognises attributes (%.0f attributes)",
                   (double)r1, (double)r2);
            /* Curated field comparison (raw memcmp is unsafe because of struct padding). */
            const bool sameFields =
                o1.sex == o2.sex && o1.build == o2.build && o1.hair_style == o2.hair_style &&
                o1.outfit == o2.outfit && o1.footwear == o2.footwear &&
                o1.height_m == o2.height_m && o1.age_years == o2.age_years &&
                o1.muscle == o2.muscle && o1.body_fat == o2.body_fat && o1.bust == o2.bust;
            check(sameFields, "analyze_prompt yields identical spec fields for the same prompt");
            /* Transparency (not a hard gate): show how the prompt mapped, so parser behaviour is visible. */
            std::printf("PARSE   \"%s\" -> attrs=%d sex=%d build=%d hair=%d outfit=%d height=%.3f\n",
                        prompt, (int)r1, o1.sex, o1.build, o1.hair_style, o1.outfit, (double)o1.height_m);
            af_engine_destroy(pe);
        } else {
            check(false, "prompt-parse engine could not be created");
        }
    }

    /* Best-effort cleanup of the temporary artifacts. */
    std::remove(PATH_A); std::remove(PATH_B); std::remove(PATH_C);

    std::printf("=== GENERATION CONTRACT: %d checks, %d failure(s) ===\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
