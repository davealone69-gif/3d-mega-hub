/*
 * af_face.cpp -- evaluation and serialisation of the face rig. See af_face.h.
 */
#include "af_face.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace af {

namespace {

inline float smooth01(float x) {
    const float t = clampf(x, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/* The lip line is cut this sharply (metres) where the lips part. */
constexpr float kLipCutBand = 0.0008f;

} // namespace

float FaceRig::kernelWeight(const FaceKernel& k, const Vec3& p) {
    const Vec3 q((p.x - k.centre.x) / k.radii.x, (p.y - k.centre.y) / k.radii.y, (p.z - k.centre.z) / k.radii.z);
    if (k.flags & AF_FACE_KERNEL_JAW) {
        /*
         * The lower jaw. Everything from the lip line down to the chin moves as one piece; below
         * the chin the motion fades out over radii.y so the throat stays where it is. Sideways
         * and towards the back of the head it fades with the distance from the front of the
         * mouth, the way the real jaw moves least at its hinge.
         *
         * At the lip line the lower lip has to leave the upper lip, so between the corners of the
         * mouth the cut is sharp (kLipCutBand). Beside the mouth there is no gap to open - cheek
         * skin runs across the line - so there the motion fades in over scale.y instead, and the
         * cheek stretches.
         */
        const float rx = p.x - k.centre.x;
        const float cut = k.cutY + k.scale.z * rx * rx;
        if (!(p.y < cut)) return 0.0f;
        const float radial = std::sqrt(q.x * q.x + q.z * q.z);
        if (radial >= 1.0f) return 0.0f;
        const float sideways = 1.0f - smooth01((radial - 0.55f) / 0.45f);
        const float low = smooth01(q.y + 1.0f);
        const float beside = smooth01((std::fabs(rx) - k.scale.x) / (0.35f * k.scale.x));
        const float band = kLipCutBand + (k.scale.y - kLipCutBand) * beside;
        const float below = smooth01((cut - p.y) / band);
        return sideways * low * below;
    }
    if (k.flags & AF_FACE_KERNEL_BOX) {
        return (std::fabs(q.x) < 1.0f && std::fabs(q.y) < 1.0f && std::fabs(q.z) < 1.0f) ? 1.0f : 0.0f;
    }
    const float r2 = q.lengthSq();
    if (r2 >= 1.0f) return 0.0f;
    const float r = std::sqrt(r2);
    return (1.0f - r) * (1.0f - r);
}

Vec3 FaceRig::kernelDelta(const FaceKernel& k, const Vec3& p) {
    const float s = kernelWeight(k, p);
    if (s == 0.0f) return Vec3(0, 0, 0);
    if (k.flags & AF_FACE_KERNEL_JAW) return Vec3(s * k.dir.x, s * k.dir.y, s * k.dir.z);
    const float sx = (k.flags & AF_FACE_KERNEL_MIRROR_X) ? (p.x >= 0.0f ? 1.0f : -1.0f) : 1.0f;
    return Vec3(s * (k.dir.x * sx + (p.x - k.centre.x) * k.scale.x),
                s * (k.dir.y + (p.y - k.centre.y) * k.scale.y),
                s * (k.dir.z + (p.z - k.centre.z) * k.scale.z));
}

Vec3 FaceRig::delta(int channel, const Vec3& p) const {
    Vec3 d(0, 0, 0);
    if (!contains(p)) return d;
    for (const FaceKernel& k : kernels) {
        if (k.channel != channel) continue;
        d += kernelDelta(k, p);
    }
    return d;
}

Vec3 FaceRig::delta(const float* weights, int count, const Vec3& p) const {
    Vec3 d(0, 0, 0);
    if (!weights || !contains(p)) return d;
    for (const FaceKernel& k : kernels) {
        if (k.channel < 0 || k.channel >= count) continue;
        const float w = weights[k.channel];
        if (w == 0.0f) continue;
        d += kernelDelta(k, p) * w;
    }
    return d;
}

void FaceRig::translate(const Vec3& t) {
    boxMin += t;
    boxMax += t;
    for (FaceKernel& k : kernels) {
        k.centre += t;
        k.cutY += t.y;
    }
}

std::vector<float> FaceRig::pack() const {
    std::vector<float> out;
    out.reserve(6 + kernels.size() * kFloatsPerKernel);
    out.insert(out.end(), {boxMin.x, boxMin.y, boxMin.z, boxMax.x, boxMax.y, boxMax.z});
    for (const FaceKernel& k : kernels) {
        out.insert(out.end(), {static_cast<float>(k.channel), k.centre.x, k.centre.y, k.centre.z,
                               k.radii.x, k.radii.y, k.radii.z, k.dir.x, k.dir.y, k.dir.z,
                               k.scale.x, k.scale.y, k.scale.z, static_cast<float>(k.flags), k.cutY});
    }
    return out;
}

bool FaceRig::unpack(const float* values, size_t count) {
    boxMin = boxMax = Vec3(0, 0, 0);
    kernels.clear();
    if (!values || count < 6 || (count - 6) % kFloatsPerKernel != 0) return false;
    const size_t n = (count - 6) / kFloatsPerKernel;
    if (n == 0 || n > 4096) return false;
    for (size_t i = 0; i < count; ++i) if (!std::isfinite(values[i])) return false;
    std::vector<FaceKernel> read(n);
    for (size_t i = 0; i < n; ++i) {
        const float* v = values + 6 + i * kFloatsPerKernel;
        FaceKernel& k = read[i];
        k.channel = static_cast<int>(v[0]);
        if (k.channel < 0 || k.channel >= 52 || static_cast<float>(k.channel) != v[0]) return false;
        k.centre = Vec3(v[1], v[2], v[3]);
        k.radii = Vec3(v[4], v[5], v[6]);
        /* A radius of zero would divide by zero in every evaluation, on the CPU and in the shader. */
        if (!(k.radii.x > 1e-6f && k.radii.y > 1e-6f && k.radii.z > 1e-6f)) return false;
        k.dir = Vec3(v[7], v[8], v[9]);
        k.scale = Vec3(v[10], v[11], v[12]);
        k.flags = static_cast<int>(v[13]);
        if (k.flags < 0 || k.flags > 7 || static_cast<float>(k.flags) != v[13]) return false;
        if ((k.flags & AF_FACE_KERNEL_JAW) && !(k.scale.x > 1e-6f && k.scale.y > 1e-6f)) return false;
        k.cutY = v[14];
    }
    boxMin = Vec3(values[0], values[1], values[2]);
    boxMax = Vec3(values[3], values[4], values[5]);
    kernels = std::move(read);
    return true;
}

int FaceRig::packActive(const float* weights, int count, float* outKernels, int maxKernels, float* outBox) const {
    if (outBox) {
        const float box[8] = {boxMin.x, boxMin.y, boxMin.z, 0.0f, boxMax.x, boxMax.y, boxMax.z, 0.0f};
        std::memcpy(outBox, box, sizeof(box));
    }
    if (!weights || !outKernels || maxKernels <= 0 || kernels.empty()) return 0;
    /* Strongest channels first, so that when more kernels are active than the block holds the
       ones left out are the ones seen least. */
    struct Active { float weight; int kernel; };
    Active active[256];
    int n = 0;
    for (size_t i = 0; i < kernels.size() && n < 256; ++i) {
        const int c = kernels[i].channel;
        if (c < 0 || c >= count) continue;
        const float w = weights[c];
        if (!std::isfinite(w) || std::fabs(w) < 1e-4f) continue;
        active[n++] = {clampf(w, -1.0f, 1.5f), static_cast<int>(i)};
    }
    std::stable_sort(active, active + n, [](const Active& a, const Active& b) {
        return std::fabs(a.weight) > std::fabs(b.weight);
    });
    const int written = std::min(n, maxKernels);
    for (int i = 0; i < written; ++i) {
        const FaceKernel& k = kernels[static_cast<size_t>(active[i].kernel)];
        float* o = outKernels + static_cast<size_t>(i) * 16;
        o[0] = k.centre.x; o[1] = k.centre.y; o[2] = k.centre.z; o[3] = active[i].weight;
        o[4] = 1.0f / k.radii.x; o[5] = 1.0f / k.radii.y; o[6] = 1.0f / k.radii.z; o[7] = static_cast<float>(k.flags);
        o[8] = k.dir.x; o[9] = k.dir.y; o[10] = k.dir.z; o[11] = k.cutY;
        o[12] = k.scale.x; o[13] = k.scale.y; o[14] = k.scale.z; o[15] = 0.0f;
    }
    return written;
}

Vec3 FaceRig::evaluatePacked(const float* kernels, int kernelCount, const float* box, const Vec3& p) {
    /* Line for line what afFaceDelta() in the vertex shader does. */
    Vec3 delta(0, 0, 0);
    if (!kernels || !box || kernelCount <= 0) return delta;
    if (!(p.x > box[0] && p.y > box[1] && p.z > box[2] && p.x < box[4] && p.y < box[5] && p.z < box[6])) return delta;
    for (int i = 0; i < kernelCount; ++i) {
        const float* c = kernels + static_cast<size_t>(i) * 16;
        const float* r = c + 4;
        const float* d = c + 8;
        const float* e = c + 12;
        const Vec3 rel(p.x - c[0], p.y - c[1], p.z - c[2]);
        const Vec3 q(rel.x * r[0], rel.y * r[1], rel.z * r[2]);
        const int flags = static_cast<int>(r[3] + 0.5f);
        float s = 0.0f;
        Vec3 offset(d[0], d[1], d[2]);
        if (flags & AF_FACE_KERNEL_JAW) {
            const float cut = d[3] + e[2] * rel.x * rel.x;
            if (!(p.y < cut)) continue;
            const float radial = std::sqrt(q.x * q.x + q.z * q.z);
            if (radial >= 1.0f) continue;
            const float sideways = 1.0f - smooth01((radial - 0.55f) / 0.45f);
            /* centre.y - radii.y is where the motion has faded out; q.y is (p.y - centre.y) / radii.y. */
            const float low = smooth01(q.y + 1.0f);
            const float beside = smooth01((std::fabs(rel.x) - e[0]) / (0.35f * e[0]));
            const float band = kLipCutBand + (e[1] - kLipCutBand) * beside;
            const float below = smooth01((cut - p.y) / band);
            s = sideways * low * below;
        } else {
            if (flags & AF_FACE_KERNEL_BOX) {
                if (!(std::fabs(q.x) < 1.0f && std::fabs(q.y) < 1.0f && std::fabs(q.z) < 1.0f)) continue;
                s = 1.0f;
            } else {
                const float r2 = q.lengthSq();
                if (r2 >= 1.0f) continue;
                const float f = 1.0f - std::sqrt(r2);
                s = f * f;
            }
            const float sx = (flags & AF_FACE_KERNEL_MIRROR_X) ? (p.x >= 0.0f ? 1.0f : -1.0f) : 1.0f;
            offset = Vec3(d[0] * sx + rel.x * e[0], d[1] + rel.y * e[1], d[2] + rel.z * e[2]);
        }
        delta += offset * (c[3] * s);
    }
    return delta;
}

} // namespace af
