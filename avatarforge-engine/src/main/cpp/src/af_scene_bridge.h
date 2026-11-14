#ifndef AF_SCENE_BRIDGE_H
#define AF_SCENE_BRIDGE_H

#include "afcore.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct af_scene_bridge af_scene_bridge;

typedef struct af_scene_draw {
    int32_t instance;
    uint64_t index_offset;
    uint64_t index_count;
    float sort_depth;
} af_scene_draw;

af_scene_bridge* af_scene_bridge_create(void);
void af_scene_bridge_destroy(af_scene_bridge* bridge);

/* Builds visibility from the exact column-major view-projection matrix uploaded to the GPU.
   Returns a per-submesh draw list. The caller supplies capacity >= af_engine_submesh_count(). */
int32_t af_scene_bridge_update(af_scene_bridge* bridge, const af_engine* engine,
                               const float view_projection[16], const float eye[3],
                               float quality, af_scene_draw* out_draws, int32_t capacity,
                               int32_t* out_count, int32_t* out_culled);

#ifdef __cplusplus
}
#endif
#endif
