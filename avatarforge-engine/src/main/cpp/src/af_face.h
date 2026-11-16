/*
 * af_face.h -- the face rig as data: what each of the 52 ARKit channels does to a point.
 *
 * A blendshape of this engine was always a formula, never a sculpt: an offset stamped onto the
 * face with a smooth falloff. Until now the formulas lived in one loop that filled 52 arrays of
 * per-vertex offsets, and those arrays were the only form the face rig had. They are far too large
 * to keep for a million-vertex avatar (52 x 12 bytes per vertex), so the importer dropped them and
 * the face of every avatar the app shows could not move.
 *
 * Here the formulas are data: a FaceRig is a short list of kernels (54 for the 52 channels), and it
 * can be evaluated anywhere - when the morph targets of a GLB are written, in a host test, and in
 * the vertex shader of the GLES renderer, which receives the active kernels as uniforms and
 * displaces the bind-pose vertex before it skins it. It needs no per-vertex storage at all, so it
 * survives the export and import of a file as a few hundred numbers in the GLB's `extras`.
 *
 * Kernel kinds (FaceKernel::flags):
 *   0                         offset = s * (dir + (p - centre) * scale), with
 *                             s = (1 - |q|)^2 for |q| < 1, q = (p - centre) / radii
 *   AF_FACE_KERNEL_MIRROR_X   as above, and dir.x takes the sign of p.x (the cheeks puff outwards)
 *   AF_FACE_KERNEL_BOX        s = 1 inside the box centre +- radii, 0 outside
 *   AF_FACE_KERNEL_JAW        the lower jaw: everything below the lip line, down to the chin, see
 *                             FaceRig::kernelWeight. It is what opens the mouth.
 *
 * The shader in af_shaders_gl.h (afFaceDelta) is a transcription of FaceRig::kernelDelta. A host
 * test evaluates both the packed uniform form and the rig and requires the same offsets.
 */
#ifndef AF_FACE_H
#define AF_FACE_H

#include "af_math.h"
#include <cstddef>
#include <vector>

namespace af {

enum : int {
    AF_FACE_KERNEL_MIRROR_X = 1,
    AF_FACE_KERNEL_JAW = 2,
    AF_FACE_KERNEL_BOX = 4,
};

struct FaceKernel {
    int   channel = 0;          /* index into kBlendshapeNames */
    Vec3  centre{0, 0, 0};
    Vec3  radii{1, 1, 1};
    Vec3  dir{0, 0, 0};         /* offset in metres at full weight where s = 1 */
    /* Stamp: a linear term, offset += s * (p - centre) * scale, per axis (the pucker spreads the
       lips about the mouth line). Jaw: x = half the width over which the lips part sharply,
       y = the height over which the cheeks beside the mouth follow the jaw, z = how the lip line
       curves away from the middle (cut = cutY + z * dx * dx). */
    Vec3  scale{0, 0, 0};
    int   flags = 0;
    float cutY = 0.0f;          /* jaw only: the lip line. Nothing at or above it moves. */
};

struct FaceRig {
    /* A point outside this box is not a face vertex and no kernel moves it. */
    Vec3 boxMin{0, 0, 0};
    Vec3 boxMax{0, 0, 0};
    std::vector<FaceKernel> kernels;      /* ordered by channel */

    bool empty() const { return kernels.empty(); }
    bool contains(const Vec3& p) const {
        return p.x > boxMin.x && p.x < boxMax.x && p.y > boxMin.y && p.y < boxMax.y &&
               p.z > boxMin.z && p.z < boxMax.z;
    }
    /** How much of a kernel reaches `p`: 0 to 1. */
    static float kernelWeight(const FaceKernel& k, const Vec3& p);
    /** The offset one kernel gives `p` at full weight. No box test. */
    static Vec3 kernelDelta(const FaceKernel& k, const Vec3& p);
    /** The offset channel `channel` gives `p` at weight 1; zero outside the face box. */
    Vec3 delta(int channel, const Vec3& p) const;
    /** The offset the given channel weights give `p` together. */
    Vec3 delta(const float* weights, int count, const Vec3& p) const;

    /** Moves the rig with the model: the exporter stands a file on y = 0. */
    void translate(const Vec3& t);

    /* The form a GLB carries in extras.faceRig: six numbers for the box, then
       kFloatsPerKernel for every kernel. */
    static constexpr int kFloatsPerKernel = 15;
    std::vector<float> pack() const;
    bool unpack(const float* values, size_t count);

    /*
     * The form the renderer uploads. For the given channel weights, writes the kernels that have
     * an effect, strongest channels first, each as four vec4:
     *   [centre.xyz, weight] [1/radii.xyz, flags] [dir.xyz, cutY] [scale.xyz, 0]
     * and returns how many were written (at most maxKernels). `outBox` receives the face box as
     * two vec4. A null rig or all-zero weights write nothing and return 0.
     */
    int packActive(const float* weights, int count, float* outKernels, int maxKernels, float* outBox) const;

    /** Evaluates the uploaded form exactly as the vertex shader does. */
    static Vec3 evaluatePacked(const float* kernels, int kernelCount, const float* box, const Vec3& p);
};

} // namespace af
#endif /* AF_FACE_H */
