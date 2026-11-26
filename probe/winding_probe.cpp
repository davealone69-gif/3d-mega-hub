/* Probe: determine whether the engine's generated mesh winding matches the
   vertex-normal convention expected by glFrontFace(GL_CCW) + glCullFace(GL_BACK).
   For a triangle ordered CCW as seen from its normal side:
       dot(cross(v1 - v0, v2 - v0), n) > 0
   If most triangles violate this, backface culling erases the whole model. */
#include "afcore.h"

#include <cmath>
#include <cstdio>
#include <cstring>

static void analyze(const char* label, const af_vertex* verts, int vcount,
                    const uint32_t* idx, int icount) {
    if (!verts || !idx || vcount <= 0 || icount < 3) {
        std::printf("%s: no geometry\n", label);
        return;
    }
    int agree = 0, disagree = 0, degenerate = 0;
    double signedVolume = 0.0;
    for (int t = 0; t + 2 < icount; t += 3) {
        const uint32_t i0 = idx[t], i1 = idx[t + 1], i2 = idx[t + 2];
        if (i0 >= (uint32_t)vcount || i1 >= (uint32_t)vcount || i2 >= (uint32_t)vcount) continue;
        const float* p0 = verts[i0].pos; const float* p1 = verts[i1].pos; const float* p2 = verts[i2].pos;
        const float e1x = p1[0]-p0[0], e1y = p1[1]-p0[1], e1z = p1[2]-p0[2];
        const float e2x = p2[0]-p0[0], e2y = p2[1]-p0[1], e2z = p2[2]-p0[2];
        const float fx = e1y*e2z - e1z*e2y;
        const float fy = e1z*e2x - e1x*e2z;
        const float fz = e1x*e2y - e1y*e2x;
        const float area2 = fx*fx + fy*fy + fz*fz;
        if (area2 < 1e-20f) { ++degenerate; continue; }
        const float nx = verts[i0].normal[0] + verts[i1].normal[0] + verts[i2].normal[0];
        const float ny = verts[i0].normal[1] + verts[i1].normal[1] + verts[i2].normal[1];
        const float nz = verts[i0].normal[2] + verts[i1].normal[2] + verts[i2].normal[2];
        const float d = fx*nx + fy*ny + fz*nz;
        if (d >= 0.0f) ++agree; else ++disagree;
        signedVolume += (double)p0[0] * (double)fx / 6.0;
    }
    std::printf("%s: verts=%d tris=%d agreeWithNormals(CCW)=%d disagree(CW)=%d degenerate=%d signedVolume=%.4f\n",
                label, vcount, icount / 3, agree, disagree, degenerate, signedVolume);
    if (disagree > agree) {
        std::printf("  => WINDING IS CW RELATIVE TO NORMALS: every face is culled with GL_CCW+GL_BACK!\n");
    } else {
        std::printf("  => winding CCW-consistent with normals (visible with GL_CCW+GL_BACK culling)\n");
    }
    /* alpha of vertex colours */
    int zeroAlpha = 0;
    for (int i = 0; i < vcount; ++i) if (verts[i].color[3] <= 0.0f) ++zeroAlpha;
    std::printf("  vertex colors with alpha<=0: %d/%d\n", zeroAlpha, vcount);
}

int main() {
    af_spec spec{};
    af_spec_init(&spec);
    spec.seed = 1337;
    af_engine* e = nullptr;
    int rc = af_engine_create(&spec, nullptr, &e);
    if (rc != AF_OK || !e) { std::printf("engine create failed %d\n", rc); return 1; }

    af_spec resolved{};
    const char* prompt = "Adult woman, athletic build, 175 cm tall, medium dark hair, black t-shirt and jeans";
    af_engine_analyze_prompt(e, prompt, &resolved);
    rc = af_engine_run_all(e);
    if (rc != AF_OK) { std::printf("run_all failed %d: %s\n", rc, af_engine_last_error(e)); return 1; }
    af_engine_apply_clip(e, AF_CLIP_REST, 0.0f);

    analyze("GENERATED (live engine mesh)", af_engine_vertices(e), af_engine_vertex_count(e),
            af_engine_indices(e), af_engine_index_count(e));

    /* export + re-import round trip (this is what the app's diagnostic path does) */
    const char* path = "/tmp/af_probe.glb";
    rc = af_engine_export_glb(e, path);
    std::printf("export rc=%d\n", rc);
    af_engine* e2 = nullptr;
    af_spec spec2{};
    af_spec_init(&spec2);
    af_engine_create(&spec2, nullptr, &e2);
    rc = af_engine_import_glb(e2, path);
    std::printf("import rc=%d (%s)\n", rc, rc == AF_OK ? "ok" : af_engine_last_error(e2));
    if (rc == AF_OK) {
        analyze("RE-IMPORTED (export->import)", af_engine_vertices(e2), af_engine_vertex_count(e2),
                af_engine_indices(e2), af_engine_index_count(e2));
        const int n = af_engine_submesh_count(e2);
        const af_submesh* sm = af_engine_submeshes(e2);
        for (int i = 0; i < n; ++i) {
            std::printf("  submesh %d: region=%d tris=%d bounds=(%.3f,%.3f,%.3f)-(%.3f,%.3f,%.3f)\n",
                        i, sm[i].region, sm[i].index_count / 3,
                        sm[i].bounds_min[0], sm[i].bounds_min[1], sm[i].bounds_min[2],
                        sm[i].bounds_max[0], sm[i].bounds_max[1], sm[i].bounds_max[2]);
        }
    }
    af_engine_destroy(e2);
    af_engine_destroy(e);
    return 0;
}
