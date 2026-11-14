/*
 * AvatarForge host evidence probe (not part of the app; built only for host test builds).
 *
 * It runs the same native sequence the Android JNI layer runs for "Generate avatar"
 *   create -> set_memory_budget -> analyze_prompt -> run_all -> apply_clip(REST) ->
 *   validate_deformed_mesh -> export_glb
 * and then measures the result, so statements in the README about orientation, mirror symmetry
 * and export/import round-trips can be reproduced instead of being taken on trust:
 *
 *   - bounds, height, vertex/triangle/joint/submesh counts
 *   - which way the figure faces (feet, face regions, nose-versus-occiput cap width)
 *   - vertex-weighted and area-weighted PCA of the XZ footprint
 *   - mirror symmetry about X = 0, including a scan over candidate yaw angles
 *   - which side the engine's "left"/"right" joints, regions and blendshapes are on
 *   - what the production renderer's import path sees after export -> import
 *
 * Build:  cmake -S avatarforge-engine/src/main/cpp -B build/host -DBUILD_TESTING=ON && cmake --build build/host
 * Run:    build/host/af_host_probe --sex female --out /tmp/avatar.glb --extended
 *         build/host/af_host_probe --analyze /path/to/any.glb --extended
 */
#include "afcore.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

static double nowSec() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}
static double gStart = 0.0;
static int gLastStage = -1;
static bool gVerbose = false;
static void onProgress(void*, int stage, float, const char* message) {
    if (stage != gLastStage) {
        gLastStage = stage;
        std::printf("  [%7.2fs] stage %d %s\n", nowSec() - gStart, stage, message ? message : "");
        std::fflush(stdout);
    }
}
static void onLog(void*, int severity, const char* message) {
    if (severity >= AF_LOG_WARN || gVerbose)
        std::printf("  [%7.2fs] log(%d): %s\n", nowSec() - gStart, severity, message ? message : "");
}

struct Key { long long x, y, z; bool operator==(const Key& o) const { return x==o.x && y==o.y && z==o.z; } };
struct KeyHash { size_t operator()(const Key& k) const {
    return std::hash<long long>()(k.x * 73856093LL ^ k.y * 19349663LL ^ k.z * 83492791LL); } };


struct MirrorResult { size_t matched = 0, unmatched = 0; double meanMm = 0, maxMm = 0; };
// Rotates every referenced vertex by yawDeg about +Y (right-handed: x' = c x + s z, z' = -s x + c z),
// then counts vertices whose X-mirror image has a neighbour within tolMeters.
static MirrorResult mirrorTest(const af_vertex* v, int32_t vcount, const std::vector<uint8_t>& used,
                               double yawDeg, double tolMeters, std::vector<uint8_t>* unmatchedFlags) {
    const double c = std::cos(yawDeg * M_PI / 180.0), s = std::sin(yawDeg * M_PI / 180.0);
    std::vector<float> p(static_cast<size_t>(vcount) * 3);
    for (int32_t i = 0; i < vcount; ++i) {
        const double x = v[i].pos[0], y = v[i].pos[1], z = v[i].pos[2];
        p[3 * i] = float(c * x + s * z); p[3 * i + 1] = float(y); p[3 * i + 2] = float(-s * x + c * z);
    }
    const double cell = tolMeters;
    std::unordered_map<Key, std::vector<int32_t>, KeyHash> grid;
    grid.reserve(static_cast<size_t>(vcount));
    auto keyOf = [&](double x, double y, double z) { return Key{ (long long)std::floor(x / cell), (long long)std::floor(y / cell), (long long)std::floor(z / cell) }; };
    for (int32_t i = 0; i < vcount; ++i) if (used[i]) grid[keyOf(p[3 * i], p[3 * i + 1], p[3 * i + 2])].push_back(i);
    MirrorResult r; double sum = 0;
    if (unmatchedFlags) unmatchedFlags->assign(static_cast<size_t>(vcount), 0);
    for (int32_t i = 0; i < vcount; ++i) {
        if (!used[i]) continue;
        const double mxp = -double(p[3 * i]), myp = p[3 * i + 1], mzp = p[3 * i + 2];
        const Key k = keyOf(mxp, myp, mzp);
        double best = 1e30;
        for (long long ax = -1; ax <= 1; ++ax) for (long long ay = -1; ay <= 1; ++ay) for (long long az = -1; az <= 1; ++az) {
            auto it = grid.find(Key{k.x + ax, k.y + ay, k.z + az});
            if (it == grid.end()) continue;
            for (int32_t j : it->second) {
                const double dx = p[3 * j] - mxp, dy = p[3 * j + 1] - myp, dz = p[3 * j + 2] - mzp;
                best = std::min(best, std::sqrt(dx * dx + dy * dy + dz * dz));
            }
        }
        if (best <= tolMeters) { ++r.matched; sum += best; r.maxMm = std::max(r.maxMm, best * 1000.0); }
        else { ++r.unmatched; if (unmatchedFlags) (*unmatchedFlags)[i] = 1; }
    }
    r.meanMm = r.matched ? 1000.0 * sum / double(r.matched) : 0.0;
    return r;
}

static void extendedSymmetry(const af_engine* engine, const af_vertex* v, int32_t vcount, const uint32_t* idx, int32_t icount,
                             const std::vector<uint8_t>& used) {
    // Left/right vertex census: an exact mirror-symmetric tessellation would give equal counts.
    size_t left = 0, right = 0, onPlane = 0;
    for (int32_t i = 0; i < vcount; ++i) { if (!used[i]) continue; if (v[i].pos[0] < -1e-6f) ++left; else if (v[i].pos[0] > 1e-6f) ++right; else ++onPlane; }
    std::printf("vertex census: x<0 %zu | x>0 %zu | on plane %zu | left-right difference %lld\n", left, right, onPlane, (long long)left - (long long)right);

    // Area-weighted PCA (triangle area x centroid), which removes vertex-density bias.
    double A = 0, ax = 0, az = 0;
    for (int32_t t = 0; t + 2 < icount; t += 3) {
        const float* a = v[idx[t]].pos; const float* b = v[idx[t + 1]].pos; const float* c = v[idx[t + 2]].pos;
        const double ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2], wx = c[0] - a[0], wy = c[1] - a[1], wz = c[2] - a[2];
        const double nx = uy * wz - uz * wy, ny = uz * wx - ux * wz, nz = ux * wy - uy * wx;
        const double area = 0.5 * std::sqrt(nx * nx + ny * ny + nz * nz);
        A += area; ax += area * (a[0] + b[0] + c[0]) / 3.0; az += area * (a[2] + b[2] + c[2]) / 3.0;
    }
    ax /= A; az /= A;
    double sxx = 0, szz = 0, sxz = 0;
    for (int32_t t = 0; t + 2 < icount; t += 3) {
        const float* a = v[idx[t]].pos; const float* b = v[idx[t + 1]].pos; const float* c = v[idx[t + 2]].pos;
        const double ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2], wx = c[0] - a[0], wy = c[1] - a[1], wz = c[2] - a[2];
        const double nx = uy * wz - uz * wy, ny = uz * wx - ux * wz, nz = ux * wy - uy * wx;
        const double area = 0.5 * std::sqrt(nx * nx + ny * ny + nz * nz);
        const double dx = (a[0] + b[0] + c[0]) / 3.0 - ax, dz = (a[2] + b[2] + c[2]) / 3.0 - az;
        sxx += area * dx * dx; szz += area * dz * dz; sxz += area * dx * dz;
    }
    std::printf("surface area %.6f m^2; AREA-weighted PCA major axis in XZ: %+.5f deg (area centroid x=%+.6f z=%+.6f)\n",
                A, 0.5 * std::atan2(2.0 * sxz, sxx - szz) * 180.0 / M_PI, ax, az);

    // Which yaw makes the figure most mirror-symmetric about X = 0? Tight 1 mm tolerance for a sharp peak.
    const double yaws[] = { -6.04720, -4.5, -3.0, -2.0, -1.0, -0.5, -0.25, 0.0, 0.25, 0.5, 1.0, 2.0, 3.0, 4.5, 6.04720 };
    std::printf("mirror-symmetry scan (rotate by yaw about +Y, then test X -> -X, tolerance 1.0 mm):\n");
    double bestYaw = 0, bestFrac = -1;
    for (double yaw : yaws) {
        const MirrorResult r = mirrorTest(v, vcount, used, yaw, 0.001, nullptr);
        const double frac = 100.0 * double(r.matched) / double(r.matched + r.unmatched);
        std::printf("  yaw %+8.4f deg -> matched %6.2f%%  mean residual %.3f mm\n", yaw, frac, r.meanMm);
        if (frac > bestFrac) { bestFrac = frac; bestYaw = yaw; }
    }
    std::printf("  => most symmetric at yaw %+.4f deg (%.2f%% matched at 1 mm)\n", bestYaw, bestFrac);

    // Where do the unmatched vertices (4 mm criterion, raw axes) live?
    std::vector<uint8_t> flags;
    const MirrorResult raw = mirrorTest(v, vcount, used, 0.0, 0.004, &flags);
    std::printf("per-region unmatched vertices at 4 mm, raw axes (total unmatched %zu):\n", raw.unmatched);
    const int32_t sn = af_engine_submesh_count(engine);
    const af_submesh* sm = af_engine_submeshes(engine);
    for (int32_t s = 0; s < sn; ++s) {
        size_t n = 0, un = 0, l = 0, r = 0; double cov = 0;
        std::vector<uint8_t> inRegion(static_cast<size_t>(vcount), 0);
        for (int32_t k = sm[s].index_offset; k < sm[s].index_offset + sm[s].index_count; ++k) inRegion[idx[k]] = 1;
        for (int32_t i = 0; i < vcount; ++i) {
            if (!inRegion[i]) continue; ++n; if (flags[i]) ++un;
            if (v[i].pos[0] < 0) ++l; else ++r;
            cov += double(v[i].pos[0]) * double(v[i].pos[2]);
        }
        std::printf("  region %-10s vertices %7zu (x<0 %7zu | x>=0 %7zu)  unmatched %6zu (%.2f%%)  sum(x*z)=%+.5f\n",
                    af_engine_region_name(sm[s].region), n, l, r, un, n ? 100.0 * un / n : 0.0, cov);
    }
}

static void orientationCues(const af_engine* engine) {
    const int32_t vcount = af_engine_vertex_count(engine);
    const af_vertex* v = af_engine_vertices_rest(engine) ? af_engine_vertices_rest(engine) : af_engine_vertices(engine);
    const uint32_t* idx = af_engine_indices(engine);
    const int32_t sn = af_engine_submesh_count(engine);
    const af_submesh* sm = af_engine_submeshes(engine);
    std::printf("\n-- orientation cues --\n");
    for (int32_t s = 0; s < sn; ++s) {
        std::vector<uint8_t> in(static_cast<size_t>(vcount), 0);
        for (int32_t k = sm[s].index_offset; k < sm[s].index_offset + sm[s].index_count; ++k) in[idx[k]] = 1;
        double cx = 0, cy = 0, cz = 0; size_t n = 0;
        for (int32_t i = 0; i < vcount; ++i) if (in[i]) { cx += v[i].pos[0]; cy += v[i].pos[1]; cz += v[i].pos[2]; ++n; }
        if (n) std::printf("  region %-10s centroid x=%+.5f y=%.5f z=%+.5f (n=%zu)\n", af_engine_region_name(sm[s].region), cx / n, cy / n, cz / n, n);
    }
    // Nose versus occiput: around eye/nose height take vertices within 8 mm of each Z extreme and
    // report how wide (in X) that cap is. A nose tip is a narrow cap; the back of the skull is broad.
    double mnY = 1e30, mxY = -1e30;
    for (int32_t i = 0; i < vcount; ++i) { mnY = std::min(mnY, double(v[i].pos[1])); mxY = std::max(mxY, double(v[i].pos[1])); }
    const double H = mxY - mnY;
    for (double band : {0.905, 0.915, 0.925}) {
        double zmin = 1e30, zmax = -1e30;
        for (int32_t i = 0; i < vcount; ++i) { const double f = (v[i].pos[1] - mnY) / H; if (std::fabs(f - band) < 0.005) { zmin = std::min(zmin, double(v[i].pos[2])); zmax = std::max(zmax, double(v[i].pos[2])); } }
        double wMinLo = 1e30, wMinHi = -1e30, wMaxLo = 1e30, wMaxHi = -1e30;
        for (int32_t i = 0; i < vcount; ++i) {
            const double f = (v[i].pos[1] - mnY) / H; if (std::fabs(f - band) >= 0.005) continue;
            if (v[i].pos[2] < zmin + 0.008) { wMinLo = std::min(wMinLo, double(v[i].pos[0])); wMinHi = std::max(wMinHi, double(v[i].pos[0])); }
            if (v[i].pos[2] > zmax - 0.008) { wMaxLo = std::min(wMaxLo, double(v[i].pos[0])); wMaxHi = std::max(wMaxHi, double(v[i].pos[0])); }
        }
        std::printf("  head band Y/H=%.3f: -Z cap at z=%+.4f is %.1f mm wide in X; +Z cap at z=%+.4f is %.1f mm wide in X\n",
                    band, zmin, 1000.0 * (wMinHi - wMinLo), zmax, 1000.0 * (wMaxHi - wMaxLo));
    }
    const int32_t jc = af_engine_joint_count(engine);
    if (jc > 0) {
        std::vector<float> m(static_cast<size_t>(jc) * 16);
        const int32_t got = af_engine_joint_matrices(engine, m.data(), jc);
        std::printf("  joints=%d (matrices returned %d). Named side joints, world translation column:\n", jc, got);
        for (int32_t j = 0; j < jc; ++j) {
            af_joint_info info{};
            if (af_engine_joint_info(engine, j, &info) != AF_OK) continue;
            const std::string name = info.name;
            const bool side = name.find("hand") != std::string::npos || name.find("eye") != std::string::npos ||
                              name.find("foot") != std::string::npos || name.find("clavicle") != std::string::npos;
            if (!side) continue;
            if (name.find("thumb") != std::string::npos || name.find("index") != std::string::npos || name.find("middle") != std::string::npos ||
                name.find("ring") != std::string::npos || name.find("pinky") != std::string::npos) continue;
            std::printf("    %-22s parent=%2d rest_local=(%+.4f,%+.4f,%+.4f) matrix_t=(%+.4f,%+.4f,%+.4f)\n", info.name, info.parent,
                        info.rest_translation[0], info.rest_translation[1], info.rest_translation[2], m[16 * j + 12], m[16 * j + 13], m[16 * j + 14]);
        }
    }
    char names[64][64] = {};
    const int32_t bn = af_engine_blendshape_names(names, 64);
    std::printf("  blendshapes=%d first names:", bn);
    for (int32_t i = 0; i < bn && i < 6; ++i) std::printf(" %s", names[i]);
    std::printf("\n");
    // Which side does eyeBlinkLeft move? Apply only that channel and compare the deformed buffer with rest.
    for (const char* channel : {"eyeBlinkLeft", "eyeBlinkRight", "mouthSmileLeft"}) {
        int which = -1;
        for (int32_t i = 0; i < bn; ++i) if (std::strcmp(names[i], channel) == 0) which = i;
        if (which < 0) { std::printf("  blendshape %s not present\n", channel); continue; }
        std::vector<float> w(static_cast<size_t>(bn), 0.0f); w[which] = 1.0f;
        af_engine* mutableEngine = const_cast<af_engine*>(engine);
        const int32_t r = af_engine_apply_blendshapes(mutableEngine, w.data(), bn);
        const af_vertex* d = af_engine_vertices(engine);
        const af_vertex* rest = af_engine_vertices_rest(engine);
        double sx = 0, moved = 0; size_t n = 0;
        for (int32_t i = 0; i < vcount; ++i) {
            const double dx = d[i].pos[0] - rest[i].pos[0], dy = d[i].pos[1] - rest[i].pos[1], dz = d[i].pos[2] - rest[i].pos[2];
            const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dist > 0.0005) { sx += rest[i].pos[0] * dist; moved += dist; ++n; }
        }
        std::printf("  blendshape %-15s apply=%d moved %zu vertices; displacement-weighted mean X of moved vertices = %+.5f m => %s side\n",
                    channel, r, n, moved > 0 ? sx / moved : 0.0, moved > 0 ? ((sx / moved) > 0 ? "+X" : "-X") : "n/a");
        std::vector<float> zero(static_cast<size_t>(bn), 0.0f);
        af_engine_apply_blendshapes(mutableEngine, zero.data(), bn);
    }
}

static bool gExtended = false;
static void measure(const char* label, const af_engine* engine, bool rest) {
    const int32_t vcount = af_engine_vertex_count(engine);
    const int32_t icount = af_engine_index_count(engine);
    const af_vertex* v = rest && af_engine_vertices_rest(engine) ? af_engine_vertices_rest(engine)
                                                                 : af_engine_vertices(engine);
    const uint32_t* idx = af_engine_indices(engine);
    std::printf("\n== %s ==\n", label);
    std::printf("vertices=%d indices=%d triangles=%d joints=%d submeshes=%d\n", vcount, icount, icount / 3,
                af_engine_joint_count(engine), af_engine_submesh_count(engine));
    if (vcount <= 0 || !v) return;

    // Unique triangle-referenced vertices.
    std::vector<uint8_t> used(static_cast<size_t>(vcount), 0);
    for (int32_t i = 0; i < icount; ++i) used[idx[i]] = 1;
    size_t referenced = 0;
    double mn[3] = {1e30, 1e30, 1e30}, mx[3] = {-1e30, -1e30, -1e30};
    double cx = 0, cz = 0;
    for (int32_t i = 0; i < vcount; ++i) {
        if (!used[i]) continue;
        ++referenced;
        for (int a = 0; a < 3; ++a) { mn[a] = std::min(mn[a], double(v[i].pos[a])); mx[a] = std::max(mx[a], double(v[i].pos[a])); }
        cx += v[i].pos[0]; cz += v[i].pos[2];
    }
    cx /= double(referenced); cz /= double(referenced);
    const double H = mx[1] - mn[1];
    std::printf("triangle-referenced vertices=%zu\n", referenced);
    std::printf("bounds X[%.6f,%.6f] Y[%.6f,%.6f] Z[%.6f,%.6f]  H=%.6f m\n", mn[0], mx[0], mn[1], mx[1], mn[2], mx[2], H);
    std::printf("XZ centroid of referenced vertices: x=%+.6f z=%+.6f\n", cx, cz);

    // PCA of the XZ vertex cloud (unweighted, per referenced vertex).
    double sxx = 0, szz = 0, sxz = 0;
    for (int32_t i = 0; i < vcount; ++i) {
        if (!used[i]) continue;
        const double dx = v[i].pos[0] - cx, dz = v[i].pos[2] - cz;
        sxx += dx * dx; szz += dz * dz; sxz += dx * dz;
    }
    const double theta = 0.5 * std::atan2(2.0 * sxz, sxx - szz); // major-axis angle, atan2(z,x) convention
    std::printf("PCA major axis in XZ (angle from +X toward +Z, modulo 180): %+.5f deg  (var_x=%.6g var_z=%.6g cov_xz=%.6g)\n",
                theta * 180.0 / M_PI, sxx / referenced, szz / referenced, sxz / referenced);

    // Front direction evidence: where do the feet point? Compare the Z centroid of the lowest
    // 4% of the figure (feet) with the Z centroid of the shank slice (20%..30% of height).
    double footZ = 0, shankZ = 0; size_t footN = 0, shankN = 0;
    for (int32_t i = 0; i < vcount; ++i) {
        if (!used[i]) continue;
        const double f = (v[i].pos[1] - mn[1]) / H;
        if (f < 0.04) { footZ += v[i].pos[2]; ++footN; }
        if (f > 0.20 && f < 0.30) { shankZ += v[i].pos[2]; ++shankN; }
    }
    if (footN && shankN) {
        footZ /= footN; shankZ /= shankN;
        std::printf("feet Z centroid=%+.6f  shank Z centroid=%+.6f  => toes point toward %sZ (delta=%+.6f m)\n",
                    footZ, shankZ, (footZ - shankZ) < 0 ? "-" : "+", footZ - shankZ);
    }
    // Second, independent front cue: the head slice (88%..97% of height). The face (nose) side has
    // the larger |z| reach from the head's own centroid on a human head? Not reliable with hair, so
    // report it only as raw numbers.
    double headZmin = 1e30, headZmax = -1e30, headZc = 0; size_t headN = 0;
    for (int32_t i = 0; i < vcount; ++i) {
        if (!used[i]) continue;
        const double f = (v[i].pos[1] - mn[1]) / H;
        if (f > 0.88 && f < 0.97) { headZmin = std::min(headZmin, double(v[i].pos[2])); headZmax = std::max(headZmax, double(v[i].pos[2])); headZc += v[i].pos[2]; ++headN; }
    }
    if (headN) std::printf("head slice z-range [%+.6f,%+.6f] centroid %+.6f (n=%zu)\n", headZmin, headZmax, headZc / headN, headN);

    // Mirror symmetry across the YZ plane (x -> -x): nearest-neighbour search on a hash grid.
    const double tol = 0.004; // the provisional 4 mm criterion recorded in the README
    const double cell = tol;
    std::unordered_map<Key, std::vector<int32_t>, KeyHash> grid;
    grid.reserve(referenced * 2);
    auto keyOf = [&](double x, double y, double z) { return Key{ (long long)std::floor(x / cell), (long long)std::floor(y / cell), (long long)std::floor(z / cell) }; };
    for (int32_t i = 0; i < vcount; ++i) if (used[i]) grid[keyOf(v[i].pos[0], v[i].pos[1], v[i].pos[2])].push_back(i);
    size_t matched = 0, unmatched = 0; double maxResidual = 0, sumResidual = 0;
    double worst[3] = {0, 0, 0}; double worstUnmatchedDist = 0; double worstUnmatched[3] = {0, 0, 0};
    for (int32_t i = 0; i < vcount; ++i) {
        if (!used[i]) continue;
        const double mxp = -double(v[i].pos[0]), myp = v[i].pos[1], mzp = v[i].pos[2];
        const Key k = keyOf(mxp, myp, mzp);
        double best = 1e30;
        for (long long ax = -1; ax <= 1; ++ax) for (long long ay = -1; ay <= 1; ++ay) for (long long az = -1; az <= 1; ++az) {
            auto it = grid.find(Key{k.x + ax, k.y + ay, k.z + az});
            if (it == grid.end()) continue;
            for (int32_t j : it->second) {
                const double dx = v[j].pos[0] - mxp, dy = v[j].pos[1] - myp, dz = v[j].pos[2] - mzp;
                best = std::min(best, std::sqrt(dx * dx + dy * dy + dz * dz));
            }
        }
        if (best <= tol) { ++matched; sumResidual += best; if (best > maxResidual) { maxResidual = best; worst[0] = v[i].pos[0]; worst[1] = v[i].pos[1]; worst[2] = v[i].pos[2]; } }
        else { ++unmatched; if (best < 1e29 && best > worstUnmatchedDist) { worstUnmatchedDist = best; worstUnmatched[0] = v[i].pos[0]; worstUnmatched[1] = v[i].pos[1]; worstUnmatched[2] = v[i].pos[2]; } }
    }
    std::printf("mirror X->-X nearest-vertex test, tolerance %.1f mm: matched=%zu (%.3f%%) unmatched=%zu (%.3f%%) mean residual=%.4f mm max matched residual=%.4f mm at (%.4f,%.4f,%.4f)\n",
                tol * 1000.0, matched, 100.0 * matched / referenced, unmatched, 100.0 * unmatched / referenced,
                matched ? 1000.0 * sumResidual / matched : 0.0, 1000.0 * maxResidual, worst[0], worst[1], worst[2]);
    std::printf("  NOTE: nearest-neighbour (many-to-one) test, NOT the one-to-one maximum-cardinality matcher.\n");

    const int32_t sn = af_engine_submesh_count(engine);
    const af_submesh* sm = af_engine_submeshes(engine);
    int foreign = 0;
    for (int32_t i = 0; i < sn; ++i) if (sm[i].region < 0) ++foreign;
    std::printf("submesh regions: %d total, %d foreign(region<0) => production JNI camera yaw = %s\n", sn, foreign,
                (sn > 0 && foreign == sn) ? "0 deg (camera on +Z, looks toward -Z)" : "180 deg (camera on -Z, looks toward +Z)");
    for (int32_t i = 0; i < sn && i < 40; ++i)
        std::printf("  submesh %2d region=%3d (%s) tris=%d\n", i, sm[i].region, af_engine_region_name(sm[i].region), sm[i].index_count / 3);
    if (gExtended) extendedSymmetry(engine, v, vcount, idx, icount, used);
}

int main(int argc, char** argv) {
    std::string out = "af_host_probe_out.glb", prompt, sex = "female", analyzeOnly;
    int seed = -1; long budgetMb = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--out") out = next(); else if (a == "--prompt") prompt = next(); else if (a == "--sex") sex = next();
        else if (a == "--seed") seed = std::atoi(next().c_str()); else if (a == "--budget-mb") budgetMb = std::atol(next().c_str());
        else if (a == "--verbose") gVerbose = true; else if (a == "--extended") gExtended = true;
        else if (a == "--analyze") analyzeOnly = next();
    }
    if (!analyzeOnly.empty()) {
        af_spec blank0; af_spec_init(&blank0);
        af_callbacks cb0{}; cb0.on_log = onLog;
        af_engine* a = nullptr; gStart = nowSec();
        if (af_engine_create(&blank0, &cb0, &a) != AF_OK || !a) { std::printf("create failed\n"); return 2; }
        const int32_t ir = af_engine_import_glb(a, analyzeOnly.c_str());
        std::printf("import_glb(%s)=%d %s\n", analyzeOnly.c_str(), ir, ir == AF_OK ? "" : af_engine_last_error(a));
        if (ir == AF_OK) measure("IMPORTED GLB", a, false);
        af_engine_destroy(a);
        return ir == AF_OK ? 0 : 6;
    }
    af_spec spec; af_spec_init(&spec);
    if (sex == "female") spec.sex = AF_SEX_FEMALE; else if (sex == "male") spec.sex = AF_SEX_MALE;
    if (seed >= 0) spec.seed = seed;
    if (!prompt.empty()) std::snprintf(spec.prompt, sizeof(spec.prompt), "%s", prompt.c_str());
    std::printf("engine %s abi %d | spec: sex=%d build=%d hair=%d outfit=%d footwear=%d seed=%d height=%.3f | budget=%ld MB (0=default) | prompt='%s'\n",
                af_engine_version(), af_engine_abi_version(), spec.sex, spec.build, spec.hair_style, spec.outfit, spec.footwear, spec.seed, spec.height_m, budgetMb, prompt.c_str());
    af_callbacks cb{}; cb.on_progress = onProgress; cb.on_log = onLog;
    af_engine* e = nullptr;
    gStart = nowSec();
    int32_t r = af_engine_create(&spec, &cb, &e);
    if (r != AF_OK || !e) { std::printf("create failed %d\n", r); return 2; }
    if (budgetMb > 0) { r = af_engine_set_memory_budget(e, uint64_t(budgetMb) * 1024ull * 1024ull); if (r != AF_OK) { std::printf("budget failed %d\n", r); return 2; } }
    af_spec parsed{};
    const int32_t matched = af_engine_analyze_prompt(e, prompt.c_str(), &parsed);
    std::printf("analyze_prompt matched=%d -> sex=%d build=%d hair=%d outfit=%d footwear=%d seed=%d height=%.3f\n", matched, parsed.sex, parsed.build, parsed.hair_style, parsed.outfit, parsed.footwear, parsed.seed, parsed.height_m);
    const double t0 = nowSec();
    r = af_engine_run_all(e);
    const double t1 = nowSec();
    if (r != AF_OK) { std::printf("run_all failed %d: %s\n", r, af_engine_last_error(e)); return 3; }
    std::printf("run_all OK in %.2f s\n", t1 - t0);
    r = af_engine_apply_clip(e, AF_CLIP_REST, 0.0f);
    if (r != AF_OK) { std::printf("apply_clip failed %d: %s\n", r, af_engine_last_error(e)); return 3; }
    char report[2048] = {};
    r = af_engine_validate_deformed_mesh(e, report, sizeof(report));
    std::printf("validate_deformed_mesh=%d report: %s\n", r, report);
    const double t2 = nowSec();
    r = af_engine_export_glb(e, out.c_str());
    const double t3 = nowSec();
    if (r != AF_OK) { std::printf("export failed %d: %s\n", r, af_engine_last_error(e)); return 4; }
    af_stats st{}; af_engine_stats(e, &st);
    std::printf("export OK in %.2f s -> %s\n", t3 - t2, out.c_str());
    std::printf("stats: vertices=%d indices=%d triangles=%d submeshes=%d joints=%d blendshapes=%d texture_bytes=%llu geometry_bytes=%llu peak_field_bytes=%llu\n",
                st.vertex_count, st.index_count, st.triangle_count, st.submesh_count, st.joint_count, st.blendshape_count,
                (unsigned long long)st.texture_bytes, (unsigned long long)st.geometry_bytes, (unsigned long long)st.peak_field_bytes);
    std::printf("stage ms: fit=%.0f field=%.0f iso=%.0f garment=%.0f hair=%.0f uv=%.0f texture=%.0f rig=%.0f export=%.0f\n",
                st.fit_anatomy_ms, st.field_ms, st.isosurface_ms, st.garment_ms, st.hair_ms, st.uv_ms, st.texture_ms, st.rig_ms, st.export_ms);
    measure("GENERATED (engine frame, rest pose)", e, true);
    orientationCues(e);
    af_engine_destroy(e);

    // Round trip through the same import entry point the production renderer uses.
    af_spec blank; af_spec_init(&blank);
    af_engine* imp = nullptr;
    r = af_engine_create(&blank, &cb, &imp);
    if (r != AF_OK || !imp) { std::printf("create(import) failed %d\n", r); return 5; }
    const double t4 = nowSec();
    r = af_engine_import_glb(imp, out.c_str());
    const double t5 = nowSec();
    std::printf("\nimport_glb=%d in %.2f s (%s)\n", r, t5 - t4, r == AF_OK ? "ok" : af_engine_last_error(imp));
    if (r == AF_OK) {
        if (af_engine_joint_count(imp) > 0) {
            const int32_t pr = af_engine_apply_clip(imp, AF_CLIP_REST, 0.0f);
            std::printf("apply_clip(REST) on imported = %d\n", pr);
        }
        measure("RE-IMPORTED from the exported GLB (what the production GLES route renders)", imp, false);
    }
    af_engine_destroy(imp);
    return 0;
}
