/*
 * af_retarget.h -- playing another skeleton's animations on this engine's rig.
 *
 * An avatar this engine makes has its own skeleton (71 joints, its own names, an A-like rest
 * pose, facing -Z). Motion made elsewhere - a Mixamo clip, a game character's dance - is written
 * for a different skeleton: other names, other proportions, usually a T pose, usually facing +Z.
 * Retargeting turns such a clip into poses of this rig:
 *
 *   1. COMPATIBILITY. The file's joints are recognised by name (Mixamo, Unreal, Blender and VRM
 *      spellings, with or without a prefix). A humanoid needs hips, a spine, a head or neck and
 *      both arms and legs down to forearm and shin; a file without them is refused with the list
 *      of what is missing.
 *   2. SIDES AND FACING are taken from geometry, not from the words "left" and "right": the toes
 *      say where the figure faces, and a joint belongs to the side it stands on. (This engine's
 *      own "_L" joints are on +X of a figure facing -Z; a name match would cross the arms.)
 *   3. REST-POSE ALIGNMENT. For every mapped bone the rotation that turns this rig's rest bone
 *      onto the file's rest bone is found once (arms down to arms out, for a T pose).
 *   4. PER FRAME, each mapped joint takes the WORLD rotation its source joint has made since the
 *      source's rest pose, applied on top of that alignment, and is then expressed relative to
 *      its parent. Joints the file has no counterpart for keep still relative to their parent.
 *      The hips also take the source's travel, scaled by the ratio of the leg lengths.
 *
 * The result is a set of sampled clips in the engine's own form (one rotation per joint per
 * frame, hip offsets), which play through the same code as the built-in clips.
 */
#ifndef AF_RETARGET_H
#define AF_RETARGET_H

#include "af_glb_read.h"
#include "af_rig.h"
#include <string>
#include <vector>

namespace af {

struct RetargetedClip {
    std::string name;
    float duration = 0.0f;
    bool  looping = false;       /* the last pose is the first pose again */
    bool  travels = false;       /* the source walked away; the drift was taken out */
    int   frames = 0;
    std::vector<float> quats;    /* frames * joints * 4, local rotations of the target rig */
    std::vector<float> offsets;  /* frames * joints * 3, or empty when nothing is translated */
};

struct RetargetReport {
    int  sourceJoints = 0;       /* joints of the file's skeleton */
    int  recognisedJoints = 0;   /* of those, how many were recognised by name */
    int  mappedJoints = 0;       /* joints of this rig that follow a joint of the file */
    int  animationsInFile = 0;
    int  animationsAdded = 0;
    bool compatible = false;
    std::string message;         /* what was done, or why nothing was */
};

/**
 * Retargets every animation of `source` onto `target`. Returns false, with the reason in
 * `report->message`, when the file has no skeleton this rig can follow or no animation. `source`
 * must have its world transforms resolved (importGlb does that).
 */
bool retargetAnimations(const GlbAsset& source, const Rig& target, float sampleHz,
                        std::vector<RetargetedClip>* outClips, RetargetReport* report);

} // namespace af
#endif /* AF_RETARGET_H */
