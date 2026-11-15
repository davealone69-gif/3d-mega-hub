/*
 * host_avatar_render - look at what the engine holds, without a phone.
 *
 * Loads a GLB through the engine's own importer (the same af_engine_import_glb the Android app
 * calls), prints what the engine kept of it, and draws the engine's buffers with a small software
 * rasteriser. It exists because "the file exported and the importer returned OK" says nothing
 * about what an avatar looks like, and the only other way to find out was to install an APK.
 *
 * This is NOT the app's renderer: no shadows, no image-based lighting, no tone mapping. It shows
 * geometry, material regions and the baked base-colour texture - enough to see a torn eyelid or a
 * shirt baked as skin. Evidence from it must be labelled "host software render".
 *
 *   af_host_render --glb avatar.glb --report [--joints]
 *   af_host_render --glb avatar.glb --out sheet.png --height 900 \
 *        --tile 0,0.90,1.90 --tile 90,0.90,1.90 --tile 180,0.90,1.90 --tile 0,1.60,0.34
 *   af_host_render --glb avatar.glb --clip 3 --time 0.4 --out walk.png --tile 35,0.90,1.90
 *   af_host_render --generate --hair 5 --outfit 3 --footwear 2 --out preset.png --tile 0,0.90,1.90
 *        (generates with the given presets - numbers as in afcore.h - and draws the result
 *         straight from the generator's buffers; add --draft for the coarse test quality)
 *
 *   --tile yaw,centreY,extent[,centreX[,centreZ]]   one view; repeat for a sheet (joined left to right)
 *   --mode lit|clay|region|normal                   default lit
 *   --aspect w/h of each tile (default 0.5)   --height pixels (default 800)   --pitch degrees
 *   --clip N --time seconds                         pose with a library clip before drawing
 *                                                   (needs a rig: generated avatars, or AvatarForge GLBs)
 */
#include "afcore.h"
#include "af_png.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct V3 { float x, y, z; };
inline V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline V3 norm(V3 a) { const float l = std::sqrt(dot(a, a)); return l > 1e-20f ? a * (1.0f / l) : V3{0, 1, 0}; }

struct Tile { float yaw = 0, cy = 0.9f, extent = 1.9f, cx = 0, cz = 0; };

const float kRegionColour[16][3] = {
    {0.96f, 0.80f, 0.72f}, {0.30f, 0.62f, 0.95f}, {0.20f, 0.85f, 0.55f}, {0.92f, 0.36f, 0.42f},
    {0.98f, 0.96f, 0.90f}, {0.85f, 0.45f, 0.50f}, {0.42f, 0.28f, 0.16f}, {0.18f, 0.14f, 0.12f},
    {0.95f, 0.88f, 0.84f}, {0.72f, 0.72f, 0.74f}, {0.30f, 0.38f, 0.66f}, {0.50f, 0.40f, 0.30f},
    {0.80f, 0.74f, 0.42f}, {0.60f, 0.30f, 0.70f}, {0.35f, 0.70f, 0.70f}, {0.9f, 0.9f, 0.2f}};

inline float toLinear(float c) { return std::pow(std::max(c, 0.0f), 2.2f); }
inline float toSrgb(float c) { return std::pow(std::min(std::max(c, 0.0f), 1.0f), 1.0f / 2.2f); }

void renderTile(const af_vertex* verts, const uint32_t* idx, const af_submesh* subs, int subCount,
                const af_material* mats, int matCount, const af_texture* base, const Tile& t,
                const std::string& mode, float pitchDeg, int w, int h, std::vector<float>* rgbOut) {
    const int ss = 2, W = w * ss, H = h * ss;
    std::vector<float> depth(static_cast<size_t>(W) * H, 1e30f);
    std::vector<float> col(static_cast<size_t>(W) * H * 3);
    for (int i = 0; i < W * H; ++i) { col[i * 3] = 0.008f; col[i * 3 + 1] = 0.012f; col[i * 3 + 2] = 0.022f; }

    /* Camera: yaw 0 looks at the avatar's front (the avatar faces -Z), turning to its left as yaw grows. */
    const float yaw = t.yaw * 3.14159265f / 180.0f, pitch = pitchDeg * 3.14159265f / 180.0f;
    const float fov = 20.0f * 3.14159265f / 180.0f;
    const float dist = (t.extent * 0.5f) / std::tan(fov * 0.5f);
    const V3 centre{t.cx, t.cy, t.cz};
    const V3 back = norm(V3{-std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)});
    const V3 eye = centre + back * dist;
    const V3 fwd = back * -1.0f;
    const V3 right = norm(cross(fwd, V3{0, 1, 0}));
    const V3 up = cross(right, fwd);
    const float focal = (static_cast<float>(H) * 0.5f) / std::tan(fov * 0.5f);
    const V3 key = norm(right * -0.45f + up * 0.55f + back * 0.70f);
    const V3 fill = norm(right * 0.80f + up * 0.10f + back * 0.55f);

    for (int s = 0; s < subCount; ++s) {
        const af_submesh& sm = subs[s];
        const af_material* m = (sm.material_index >= 0 && sm.material_index < matCount) ? &mats[sm.material_index] : nullptr;
        const bool textured = mode == "lit" && m && m->has_base_color_tex && base && base->data && base->width > 0;
        for (int i = sm.index_offset; i + 2 < sm.index_offset + sm.index_count; i += 3) {
            const af_vertex* v[3] = {&verts[idx[i]], &verts[idx[i + 1]], &verts[idx[i + 2]]};
            float sx[3], sy[3], iz[3];
            bool behind = false;
            for (int k = 0; k < 3; ++k) {
                const V3 p = V3{v[k]->pos[0], v[k]->pos[1], v[k]->pos[2]} - eye;
                const float z = dot(p, fwd);
                if (z < 0.05f) { behind = true; break; }
                sx[k] = static_cast<float>(W) * 0.5f + dot(p, right) / z * focal;
                sy[k] = static_cast<float>(H) * 0.5f - dot(p, up) / z * focal;
                iz[k] = 1.0f / z;
            }
            if (behind) continue;
            const float area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);
            if (std::fabs(area) < 1e-9f) continue;
            const int x0 = std::max(0, static_cast<int>(std::floor(std::min({sx[0], sx[1], sx[2]}))));
            const int x1 = std::min(W - 1, static_cast<int>(std::ceil(std::max({sx[0], sx[1], sx[2]}))));
            const int y0 = std::max(0, static_cast<int>(std::floor(std::min({sy[0], sy[1], sy[2]}))));
            const int y1 = std::min(H - 1, static_cast<int>(std::ceil(std::max({sy[0], sy[1], sy[2]}))));
            for (int y = y0; y <= y1; ++y) {
                for (int x = x0; x <= x1; ++x) {
                    const float px = static_cast<float>(x) + 0.5f, py = static_cast<float>(y) + 0.5f;
                    const float b0 = ((sx[1] - px) * (sy[2] - py) - (sx[2] - px) * (sy[1] - py)) / area;
                    const float b1 = ((sx[2] - px) * (sy[0] - py) - (sx[0] - px) * (sy[2] - py)) / area;
                    const float b2 = 1.0f - b0 - b1;
                    if (b0 < 0.0f || b1 < 0.0f || b2 < 0.0f) continue;
                    const float invz = b0 * iz[0] + b1 * iz[1] + b2 * iz[2];
                    const float z = 1.0f / invz;
                    const size_t o = static_cast<size_t>(y) * W + x;
                    if (z >= depth[o]) continue;
                    depth[o] = z;
                    /* Perspective-correct weights. */
                    const float w0 = b0 * iz[0] * z, w1 = b1 * iz[1] * z, w2 = b2 * iz[2] * z;
                    V3 n = norm(V3{v[0]->normal[0] * w0 + v[1]->normal[0] * w1 + v[2]->normal[0] * w2,
                                   v[0]->normal[1] * w0 + v[1]->normal[1] * w1 + v[2]->normal[1] * w2,
                                   v[0]->normal[2] * w0 + v[1]->normal[2] * w1 + v[2]->normal[2] * w2});
                    if (dot(n, back) < 0.0f) n = n * -1.0f;      /* two-sided shading */
                    float a[3] = {0.62f, 0.62f, 0.62f};
                    if (mode == "region") {
                        const int r = (sm.region >= 0 && sm.region < 16) ? sm.region : 15;
                        for (int c = 0; c < 3; ++c) a[c] = toLinear(kRegionColour[r][c]);
                    } else if (mode == "normal") {
                        a[0] = n.x * 0.5f + 0.5f; a[1] = n.y * 0.5f + 0.5f; a[2] = n.z * 0.5f + 0.5f;
                    } else if (mode == "lit") {
                        for (int c = 0; c < 3; ++c) a[c] = m ? m->base_color_factor[c] : 0.62f;
                        if (textured) {
                            float u = v[0]->uv[0] * w0 + v[1]->uv[0] * w1 + v[2]->uv[0] * w2;
                            float tv = v[0]->uv[1] * w0 + v[1]->uv[1] * w1 + v[2]->uv[1] * w2;
                            u = std::min(std::max(u, 0.0f), 1.0f) * static_cast<float>(base->width) - 0.5f;
                            tv = std::min(std::max(tv, 0.0f), 1.0f) * static_cast<float>(base->height) - 0.5f;
                            const int ux = std::min(std::max(static_cast<int>(std::floor(u)), 0), base->width - 1);
                            const int uy = std::min(std::max(static_cast<int>(std::floor(tv)), 0), base->height - 1);
                            const int ux1 = std::min(ux + 1, base->width - 1), uy1 = std::min(uy + 1, base->height - 1);
                            const float fx = std::min(std::max(u - static_cast<float>(ux), 0.0f), 1.0f);
                            const float fy = std::min(std::max(tv - static_cast<float>(uy), 0.0f), 1.0f);
                            const int ch = base->channels;
                            for (int c = 0; c < 3; ++c) {
                                const auto tex = [&](int xx, int yy) {
                                    return toLinear(static_cast<float>(base->data[(static_cast<size_t>(yy) * base->width + xx) * ch + std::min(c, ch - 1)]) / 255.0f);
                                };
                                a[c] *= (tex(ux, uy) * (1 - fx) + tex(ux1, uy) * fx) * (1 - fy) +
                                        (tex(ux, uy1) * (1 - fx) + tex(ux1, uy1) * fx) * fy;
                            }
                        }
                    }
                    float light = 1.0f;
                    if (mode != "normal") {
                        const float hemi = 0.26f + 0.10f * (n.y * 0.5f + 0.5f);
                        light = hemi + 0.78f * std::max(0.0f, dot(n, key)) + 0.22f * std::max(0.0f, dot(n, fill));
                    }
                    col[o * 3] = a[0] * light; col[o * 3 + 1] = a[1] * light; col[o * 3 + 2] = a[2] * light;
                }
            }
        }
    }
    rgbOut->assign(static_cast<size_t>(w) * h * 3, 0.0f);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) for (int c = 0; c < 3; ++c) {
        float sum = 0.0f;
        for (int dy = 0; dy < ss; ++dy) for (int dx = 0; dx < ss; ++dx) sum += col[(static_cast<size_t>(y * ss + dy) * W + (x * ss + dx)) * 3 + c];
        (*rgbOut)[(static_cast<size_t>(y) * w + x) * 3 + c] = toSrgb(sum / static_cast<float>(ss * ss));
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::string glb, out, mode = "lit";
    std::vector<Tile> tiles;
    int height = 800, clip = -1;
    float aspect = 0.5f, pitch = 4.0f, clipTime = 0.0f;
    bool report = false, joints = false, generate = false, draft = false;
    int genSex = AF_SEX_FEMALE, genBuild = AF_BUILD_ATHLETIC, genHair = AF_HAIR_MEDIUM, genOutfit = AF_OUTFIT_TSHIRT, genFootwear = AF_FOOTWEAR_NONE;
    float genBust = -1.0f, genGlute = 0.0f, genMuscle = -1.0f, genFat = -1.0f, genMelanin = -1.0f, genStature = 1.75f, genAge = -1.0f;
    float genFace[8] = {0, 0, 0, 0, 0, 0, 0, 0}, genHairTone[3] = {0, 0, 0}, genEyeTone[3] = {0, 0, 0}, genClothTone[3] = {0, 0, 0};
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--glb") glb = next(); else if (a == "--out") out = next(); else if (a == "--mode") mode = next();
        else if (a == "--height") height = std::atoi(next().c_str()); else if (a == "--aspect") aspect = static_cast<float>(std::atof(next().c_str()));
        else if (a == "--pitch") pitch = static_cast<float>(std::atof(next().c_str()));
        else if (a == "--clip") clip = std::atoi(next().c_str()); else if (a == "--time") clipTime = static_cast<float>(std::atof(next().c_str()));
        else if (a == "--report") report = true; else if (a == "--joints") joints = true;
        else if (a == "--generate") generate = true; else if (a == "--draft") draft = true;
        else if (a == "--sex") genSex = std::atoi(next().c_str()); else if (a == "--build") genBuild = std::atoi(next().c_str());
        else if (a == "--hair") genHair = std::atoi(next().c_str()); else if (a == "--outfit") genOutfit = std::atoi(next().c_str());
        else if (a == "--footwear") genFootwear = std::atoi(next().c_str());
        /* The rest of the spec, so that every option of the creator can be drawn here. */
        else if (a == "--bust") genBust = static_cast<float>(std::atof(next().c_str()));
        else if (a == "--glute") genGlute = static_cast<float>(std::atof(next().c_str()));
        else if (a == "--muscle") genMuscle = static_cast<float>(std::atof(next().c_str()));
        else if (a == "--fat") genFat = static_cast<float>(std::atof(next().c_str()));
        else if (a == "--melanin") genMelanin = static_cast<float>(std::atof(next().c_str()));
        else if (a == "--stature") genStature = static_cast<float>(std::atof(next().c_str()));
        else if (a == "--age") genAge = static_cast<float>(std::atof(next().c_str()));
        else if (a == "--face") { const std::string v = next(); std::sscanf(v.c_str(), "%f,%f,%f,%f,%f,%f,%f,%f", &genFace[0], &genFace[1], &genFace[2], &genFace[3], &genFace[4], &genFace[5], &genFace[6], &genFace[7]); }
        else if (a == "--hairtone") { const std::string v = next(); std::sscanf(v.c_str(), "%f,%f,%f", &genHairTone[0], &genHairTone[1], &genHairTone[2]); }
        else if (a == "--eyetone") { const std::string v = next(); std::sscanf(v.c_str(), "%f,%f,%f", &genEyeTone[0], &genEyeTone[1], &genEyeTone[2]); }
        else if (a == "--clothtone") { const std::string v = next(); std::sscanf(v.c_str(), "%f,%f,%f", &genClothTone[0], &genClothTone[1], &genClothTone[2]); }
        else if (a == "--tile") {
            Tile t; const std::string s = next();
            std::sscanf(s.c_str(), "%f,%f,%f,%f,%f", &t.yaw, &t.cy, &t.extent, &t.cx, &t.cz);
            tiles.push_back(t);
        }
    }
    if (glb.empty() && !generate) {
        std::fprintf(stderr, "usage: af_host_render (--glb file.glb | --generate [--sex N --build N --hair N --outfit N --footwear N --draft\n"
                             "                         --bust F --glute F --muscle F --fat F --melanin F --stature M --age Y\n"
                             "                         --face a,b,c,d,e,f,g,h --hairtone r,g,b --eyetone r,g,b --clothtone r,g,b])\n"
                             "                      [--report] [--joints] [--out sheet.png --tile yaw,cy,extent ...]\n");
        return 2;
    }

    af_spec spec; af_spec_init(&spec);
    if (generate) {
        spec.sex = genSex; spec.build = genBuild; spec.hair_style = genHair; spec.outfit = genOutfit; spec.footwear = genFootwear;
        spec.height_m = genStature;
        if (genBust >= 0.0f) spec.bust = genBust;
        spec.glute = genGlute;
        if (genMuscle >= 0.0f) spec.muscle = genMuscle;
        if (genFat >= 0.0f) spec.body_fat = genFat;
        if (genMelanin >= 0.0f) spec.skin_melanin = genMelanin;
        if (genAge > 0.0f) spec.age_years = genAge;
        for (int c = 0; c < 8; ++c) spec.face[c] = genFace[c];
        for (int c = 0; c < 3; ++c) { spec.hair_tone[c] = genHairTone[c]; spec.eye_tone[c] = genEyeTone[c]; spec.cloth_tone[c] = genClothTone[c]; }
    }
    af_engine* e = nullptr;
    if (af_engine_create(&spec, nullptr, &e) != AF_OK || !e) { std::fprintf(stderr, "engine create failed\n"); return 1; }
    int rc = AF_OK;
    if (generate) {
        /* Straight from the generator's buffers: no export, no import. */
        if (draft) af_engine_set_quality(e, AF_QUALITY_DRAFT);
        rc = af_engine_run_all(e);
    } else {
        rc = af_engine_import_glb(e, glb.c_str());
    }
    const char* err = af_engine_last_error(e);
    if (rc != AF_OK) { std::printf("%s FAILED rc=%d: %s\n", generate ? "GENERATE" : "IMPORT", rc, err ? err : ""); af_engine_destroy(e); return 1; }
    if (clip >= 0) {
        const int crc = af_engine_apply_clip(e, clip, clipTime);
        char name[64] = {0}; af_engine_clip_name(clip, name, sizeof(name));
        std::printf("apply_clip %d (%s) t=%.3f -> rc=%d%s\n", clip, name, clipTime, crc, crc == AF_OK ? "" : " (no usable rig: drawn in the stored pose)");
    }
    const int vcount = af_engine_vertex_count(e), icount = af_engine_index_count(e);
    const af_vertex* verts = af_engine_vertices(e);
    const uint32_t* idx = af_engine_indices(e);
    const int subCount = af_engine_submesh_count(e), matCount = af_engine_material_count(e);
    const af_submesh* subs = af_engine_submeshes(e);
    const af_material* mats = af_engine_materials(e);
    af_texture base{}; const bool hasBase = af_engine_texture(e, AF_TEX_BASE_COLOR, &base) == AF_OK && base.data;

    if (report) {
        float mn[3] = {1e30f, 1e30f, 1e30f}, mx[3] = {-1e30f, -1e30f, -1e30f};
        int weighted = 0;
        for (int i = 0; i < vcount; ++i) {
            for (int c = 0; c < 3; ++c) { mn[c] = std::min(mn[c], verts[i].pos[c]); mx[c] = std::max(mx[c], verts[i].pos[c]); }
            if (verts[i].weights[0] + verts[i].weights[1] + verts[i].weights[2] + verts[i].weights[3] > 0.5f) ++weighted;
        }
        std::printf("%s: OK  last_error/warning: \"%s\"\n", generate ? "generate" : "import", err ? err : "");
        std::printf("vertices=%d triangles=%d submeshes=%d materials=%d\n", vcount, icount / 3, subCount, matCount);
        std::printf("bounds min=(%.3f,%.3f,%.3f) max=(%.3f,%.3f,%.3f)\n", mn[0], mn[1], mn[2], mx[0], mx[1], mx[2]);
        std::printf("engine joints=%d  skin joints=%d  animations=%d  vertices with skin weights=%d\n",
                    af_engine_joint_count(e), af_engine_skin_joint_count(e), af_engine_animation_count(e), weighted);
        for (int a = 0; a < af_engine_animation_count(e); ++a) {
            char name[64] = {0}; af_engine_animation_name(e, a, name, sizeof(name));
            std::printf("  animation %2d  %-12s %.3f s\n", a, name, af_engine_animation_duration(e, a));
        }
        std::printf("base-colour texture: %s", hasBase ? "" : "none\n");
        if (hasBase) std::printf("%dx%d, %d channels\n", base.width, base.height, base.channels);
        for (int s = 0; s < subCount; ++s) {
            const af_material* m = (subs[s].material_index >= 0 && subs[s].material_index < matCount) ? &mats[subs[s].material_index] : nullptr;
            std::printf("  submesh %d region=%-12s triangles=%-8d material=%-14s factor=(%.2f,%.2f,%.2f) baseTex=%d\n", s,
                        af_engine_region_name(subs[s].region), subs[s].index_count / 3, m ? m->name : "-",
                        m ? m->base_color_factor[0] : 0.f, m ? m->base_color_factor[1] : 0.f, m ? m->base_color_factor[2] : 0.f,
                        m ? m->has_base_color_tex : 0);
        }
    }
    if (joints) {
        /* The skeleton the engine holds, in its own order: index, name, parent index. */
        for (int j = 0; j < af_engine_joint_count(e); ++j) {
            af_joint_info info{};
            if (af_engine_joint_info(e, j, &info) == AF_OK) std::printf("joint %2d %-22s parent=%d flags=%d\n", j, info.name, info.parent, info.flags);
        }
    }
    if (!out.empty()) {
        if (tiles.empty()) tiles.push_back(Tile{});
        const int tw = std::max(16, static_cast<int>(static_cast<float>(height) * aspect)), gap = 4;
        const int W = static_cast<int>(tiles.size()) * tw + (static_cast<int>(tiles.size()) - 1) * gap;
        std::vector<uint8_t> img(static_cast<size_t>(W) * height * 3, 255);
        for (size_t t = 0; t < tiles.size(); ++t) {
            std::vector<float> rgb;
            renderTile(verts, idx, subs, subCount, mats, matCount, hasBase ? &base : nullptr, tiles[t], mode, pitch, tw, height, &rgb);
            for (int y = 0; y < height; ++y) for (int x = 0; x < tw; ++x) for (int c = 0; c < 3; ++c) {
                img[(static_cast<size_t>(y) * W + (t * (tw + gap) + x)) * 3 + c] =
                    static_cast<uint8_t>(rgb[(static_cast<size_t>(y) * tw + x) * 3 + c] * 255.0f + 0.5f);
            }
        }
        std::vector<uint8_t> png;
        if (!af::encodePng(img.data(), W, height, 3, &png)) { std::fprintf(stderr, "png encode failed: %s\n", af::encodePngLastError()); return 1; }
        FILE* f = std::fopen(out.c_str(), "wb");
        if (!f) { std::fprintf(stderr, "cannot write %s\n", out.c_str()); return 1; }
        std::fwrite(png.data(), 1, png.size(), f); std::fclose(f);
        std::printf("wrote %s (%dx%d, mode %s)\n", out.c_str(), W, height, mode.c_str());
    }
    af_engine_destroy(e);
    return 0;
}
