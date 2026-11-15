/*
 * af_field.cpp -- implementation of the human body signed distance field.
 *
 * Construction order and why it matters:
 *   1. Skeleton chains (articulation points) are derived from the anthropometric landmarks
 *      first, because every primitive - capsule, muscle blob, nail - is anchored to them.
 *      This is also what guarantees the field and the rig agree by construction: the rig is
 *      later built from these exact same joint positions, so bind-pose skinning is exact.
 *   2. The trunk is a swept super-elliptical profile rather than a stack of capsules: that is
 *      the only way to get a believable waist-hip-bust rhythm with continuous curvature.
 *   3. Muscle/adipose blobs are unioned with a smooth-min radius that scales with the blob
 *      size, so a heavy build gets soft merges (fat is smooth) and a muscular build gets
 *      tighter merges (muscle boundaries are defined).
 *   4. Facial features are subtractive as well as additive (eye sockets, mouth line, nostril
 *      notches) because a photoreal face reads from its concavities far more than its bumps.
 *
 * Every primitive here is evaluated in metres; the field is approximately 1-Lipschitz, which
 * the extractor relies on for safe sphere-tracing and for skipping empty grid blocks.
 */
#include "af_field.h"
#include "af_mesh.h"

namespace af {

/* Thickness of an eyelid at its margin, as a fraction of the eyeball radius (about 1.6 mm). The
   lids are a shell of this thickness round the globe; the opening is cut through it. It was 0.22
   (2.7 mm), which left the eye at the bottom of a wall the mesh could not resolve. */
static const float kLidThickness = 0.13f;

/*
 * How far a layer that lies on the body (cloth, hair on the scalp) stands off the skin, given the
 * signed distance to the layer's edge (negative inside the layer).
 *
 * The layer reaches its full thickness `width` inside the edge and thins smoothly to nothing
 * towards it; outside the edge it is below the skin, so it does not exist. A layer used to keep its
 * full thickness up to the edge and end in a wall. A wall a few millimetres high is a step the
 * mesher cannot follow with triangles of that same size - every collar, hem and hairline came out
 * as a saw-tooth. With the layer feathered there is no step: the surface is smooth across the
 * edge, and the edge itself is drawn by the texture bake, texel by texel, from this same function.
 */
static float layerOffset(float edge, float thickness, float width) {
    const float s = -edge;
    /* Outside the edge the layer goes under the skin along a parabola, not a straight line: a
       change of slope at the edge shows in the body's surface as a ridge along every hem. */
    const float out = minf(s, 0.0f);
    return thickness * smoothstepf(0.0f, width, s) - 0.0010f - out * out * 250.0f;
}

/* =======================================================================================
 * Profile1D -- monotone cubic Hermite (Fritsch-Carlson tangents)
 * ======================================================================================= */
void Profile1D::build() {
    const size_t n = ys_.size();
    ms_.assign(n, 0.0f);
    if (n < 2) return;
    std::vector<float> delta(n - 1);
    for (size_t i = 0; i + 1 < n; ++i) {
        const float dy = ys_[i + 1] - ys_[i];
        delta[i] = (std::fabs(dy) < kEps) ? 0.0f : (vs_[i + 1] - vs_[i]) / dy;
    }
    ms_[0] = delta[0];
    ms_[n - 1] = delta[n - 2];
    for (size_t i = 1; i + 1 < n; ++i) {
        if (delta[i - 1] * delta[i] <= 0.0f) { ms_[i] = 0.0f; continue; }
        const float w1 = 2.0f * (ys_[i + 1] - ys_[i]) + (ys_[i] - ys_[i - 1]);
        const float w2 = (ys_[i + 1] - ys_[i]) + 2.0f * (ys_[i] - ys_[i - 1]);
        ms_[i] = (w1 + w2) / (w1 / delta[i - 1] + w2 / delta[i]);
    }
}

float Profile1D::eval(float y) const {
    const size_t n = ys_.size();
    if (n == 0) return 0.0f;
    if (n == 1) return vs_[0];
    if (y <= ys_[0]) return vs_[0];
    if (y >= ys_[n - 1]) return vs_[n - 1];
    size_t hi = 1;
    while (hi < n - 1 && ys_[hi] < y) ++hi;
    const size_t lo = hi - 1;
    const float h = ys_[hi] - ys_[lo];
    if (std::fabs(h) < kEps) return vs_[lo];
    const float t = (y - ys_[lo]) / h;
    const float t2 = t * t, t3 = t2 * t;
    return (2 * t3 - 3 * t2 + 1) * vs_[lo] + (t3 - 2 * t2 + t) * h * ms_[lo] +
           (-2 * t3 + 3 * t2) * vs_[hi] + (t3 - t2) * h * ms_[hi];
}

/* =======================================================================================
 * helpers
 * ======================================================================================= */
namespace {

/** Rotate `dir` by `angle` radians about the axis that turns it toward `target`. */
inline Vec3 rotateToward(const Vec3& dir, const Vec3& target, float angle) {
    Vec3 axis = dir.cross(target);
    if (axis.lengthSq() < 1e-10f) return dir;
    axis = axis.normalizedOr({1, 0, 0});
    const Quat q = Quat::fromAxisAngle(axis, angle);
    return q.rotate(dir).normalizedOr(dir);
}

/** Ellipsoid blob, the universal "add flesh here" primitive. */
inline float blob(const Vec3& p, const Vec3& centre, const Vec3& radii) {
    return sdEllipsoid(p - centre, radii);
}

/** Two-lobe blob mirrored across X, for paired anatomy (pecs, glutes, deltoids, ...). */
inline float blob2(const Vec3& p, float cx, float cy, float cz, const Vec3& radii) {
    const float a = blob(p, {cx, cy, cz}, radii);
    const float b = blob(p, {-cx, cy, cz}, radii);
    return minf(a, b);
}

} // namespace

/* =======================================================================================
 * Construction
 * ======================================================================================= */
BodyField::BodyField(const BodyMetrics& metrics) : m_(metrics), noise_(metrics.seed) {
    const float H = m_.height;
    const bool female = (m_.sex == AF_SEX_FEMALE);

    /* ---- neck: a short slightly-forward-leaning taper from neck base to jaw -------------- */
    const Vec3 neckBase(0.0f, m_.y.neckBaseY - 0.02f * H, m_.neck.centreZ);
    const Vec3 neckTop(0.0f, m_.y.jawY + 0.004f * H, m_.neck.centreZ - 0.010f * H);

    /* ---- torso profile control points (y, value) ---------------------------------------- */
    const float crotch = m_.y.crotchY;
    const float hipY = m_.y.hipY;
    const float waistY = m_.y.waistY;
    const float underbustY = m_.y.underbustY;
    const float bustY = m_.y.bustY;
    const float armpitY = m_.y.armpitY;
    const float shoulderY = m_.y.shoulderY;
    const float neckBaseY = m_.y.neckBaseY;

    /* rx (side-to-side half width) - note the pinch at the waist and the flare at the hips. */
    /* Below the hip line the trunk narrows to the pelvic floor and stops: from there down the
       width of the hips is carried by the two thighs. The profile used to stay almost as wide as
       the hips for 13 cm below the crotch, which is the solid block that sat between and around
       the upper thighs. */
    rxProfile_.add(crotch - 0.020f * H, m_.hip.rx * 0.34f);
    rxProfile_.add(crotch, m_.hip.rx * 0.58f);
    rxProfile_.add(lerpf(crotch, hipY, 0.45f), m_.hip.rx * 0.90f);
    rxProfile_.add(hipY, m_.hip.rx * 1.02f);
    rxProfile_.add(lerpf(hipY, waistY, 0.5f), lerpf(m_.hip.rx, m_.waist.rx, 0.45f) * 1.01f);
    rxProfile_.add(waistY, m_.waist.rx);
    rxProfile_.add(lerpf(waistY, underbustY, 0.5f), lerpf(m_.waist.rx, m_.underbust.rx, 0.55f));
    rxProfile_.add(underbustY, m_.underbust.rx);
    rxProfile_.add(bustY, m_.chest.rx * 0.98f);
    rxProfile_.add(armpitY, m_.chest.rx * 1.00f);
    rxProfile_.add(lerpf(armpitY, shoulderY, 0.6f), m_.chest.rx * 0.88f);
    rxProfile_.add(shoulderY, m_.chest.rx * 0.72f);
    rxProfile_.add(neckBaseY, m_.neck.rx * 1.05f);
    rxProfile_.build();

    /* rz (front-to-back half depth) - the lumbar curve and thoracic mass live here. */
    /* The hip girth is measured round the buttocks, so the pelvis itself is shallower than an
       ellipse of that girth: the rest of the depth is the gluteal mass added in torso(). Using the
       full ellipse AND the glutes made the hips a barrel that stood out in front as well. */
    rzProfile_.add(crotch - 0.020f * H, m_.hip.rz * 0.42f);
    rzProfile_.add(crotch, m_.hip.rz * 0.62f);
    rzProfile_.add(lerpf(crotch, hipY, 0.45f), m_.hip.rz * 0.80f);
    rzProfile_.add(hipY, m_.hip.rz * 0.86f);
    rzProfile_.add(lerpf(hipY, waistY, 0.5f), lerpf(m_.hip.rz * 0.86f, m_.waist.rz, 0.55f));
    rzProfile_.add(waistY, m_.waist.rz);
    rzProfile_.add(underbustY, m_.underbust.rz);
    rzProfile_.add(bustY, m_.bust > 0.05f && female ? m_.underbust.rz * 1.08f : m_.chest.rz * 0.99f);
    rzProfile_.add(armpitY, m_.chest.rz);
    rzProfile_.add(lerpf(armpitY, shoulderY, 0.6f), m_.chest.rz * 0.92f);
    rzProfile_.add(shoulderY, m_.chest.rz * 0.80f);
    rzProfile_.add(neckBaseY, m_.neck.rz * 1.02f);
    rzProfile_.build();

    /* cz (centre-line offset) - lumbar lordosis pulls the waist centre backwards, the bust
       pushes the chest centre forwards. Small numbers, big perceptual effect. */
    czProfile_.add(crotch, m_.hip.centreZ + 0.012f);
    czProfile_.add(hipY, m_.hip.centreZ + 0.010f);
    czProfile_.add(waistY, m_.waist.centreZ - 0.006f * H);
    czProfile_.add(bustY, m_.bustSeg.centreZ + (female ? 0.004f * H * m_.bust : 0.0f));
    czProfile_.add(shoulderY, m_.chest.centreZ + 0.002f * H);
    czProfile_.add(neckBaseY, m_.neck.centreZ);
    czProfile_.build();

    /* ---- chains (articulation points) ---------------------------------------------------- */
    for (int s = 0; s < 2; ++s) {
        const int side = (s == 0) ? 1 : -1;
        Chain& c = (side > 0) ? left_ : right_;
        c.side = side;
        const float sx = static_cast<float>(side);

        /* Arm: an A-pose, the standard rigging stance: arms clear of the torso so skinning, cloth
           and IK all behave. 12 degrees at the shoulder (a little more on a female figure, whose
           hips are wider), and the forearm nearly in line with the upper arm. It was 9 degrees
           with the forearm turned back to half of that, which hung the wrist against the hip:
           the hand and the trouser leg were one surface. */
        const float abduct = radians(12.0f * (female ? 1.15f : 1.0f));
        /* The biacromial breadth is measured between the outer edges of the acromia. The centre
           of the shoulder joint, which is what the arm turns about, lies inside that edge. */
        c.shoulder = {sx * m_.biacromialHalf * 0.87f, m_.y.shoulderY, -0.006f * H};
        const Vec3 upperDir(sx * std::sin(abduct), -std::cos(abduct), 0.02f);
        c.elbow = c.shoulder + upperDir * m_.upperArmLen;
        const Vec3 foreDir(sx * std::sin(abduct * 0.80f), -std::cos(abduct * 0.80f), -0.015f);
        c.wrist = c.elbow + foreDir * m_.forearmLen;

        /* Hand frame: fingers hang down, palm faces the thigh, index finger toward the front. */
        const Vec3 dirDown = (c.wrist - c.elbow).normalizedOr({0, -1, 0});
        const Vec3 palmNormal(-sx, 0.0f, 0.0f);              /* points at the body    */
        c.palmCentre = c.wrist + dirDown * (m_.handLength * 0.26f);
        /* `across` runs from the little finger towards the index finger and the thumb. A cross
           product is an axial vector, so mirroring the hand flips it: without the side sign the
           left hand was built with its thumb pointing backwards and the two hands were not mirror
           images of each other. With it, both thumbs point forward (-Z). */
        const Vec3 across = (palmNormal.cross(dirDown) * -sx).normalizedOr({0, 0, -1});

        const float spacing = m_.palmWidth * 0.245f;
        const Vec3 knuckleLineCentre = c.palmCentre + dirDown * (m_.handLength * 0.20f);
        for (int i = 0; i < 5; ++i) {
            if (i == 0) {
                /* Thumb. Its base (the carpometacarpal joint) is at the thumb-side edge of the
                   palm near the wrist. In the relaxed hand the thumb points down and forward,
                   about 40 degrees off the fingers, and turns a little towards the palm. */
                const Vec3 base = c.palmCentre + across * (m_.palmWidth * 0.40f) +
                                  palmNormal * (m_.palmThickness * 0.20f) -
                                  dirDown * (m_.handLength * 0.15f);
                const Vec3 dirMeta = (dirDown * 0.74f + across * 0.64f + palmNormal * 0.20f).normalizedOr(dirDown);
                const Vec3 dirTip = (dirDown * 0.86f + across * 0.46f + palmNormal * 0.22f).normalizedOr(dirDown);
                const float L = m_.fingerLength[0];
                c.fingerKnuckle[0] = base;
                c.fingerJoints[0][0] = base + dirMeta * (L * 0.46f);
                c.fingerJoints[0][1] = c.fingerJoints[0][0] + dirTip * (L * 0.30f);
                c.fingerJoints[0][2] = c.fingerJoints[0][1] + dirTip * (L * 0.24f);
                c.fingerTip[0] = c.fingerJoints[0][2];
                continue;
            }
            /* Index..little, centred on the palm: +1.5, +0.5, -0.5, -1.5 spacings. The row used
               to be centred on the middle finger, which hung the little finger off the palm. */
            const float acrossOffset = (2.5f - static_cast<float>(i)) * spacing;
            /* The knuckles lie on an arc, not a line: the index and little finger start higher. */
            const float arc = (i == 2 || i == 3) ? 0.0f : (i == 1 ? 0.035f : 0.075f);
            const Vec3 knuckle = knuckleLineCentre + across * acrossOffset -
                                 dirDown * (m_.handLength * arc) +
                                 palmNormal * (m_.palmThickness * 0.05f);
            const float L = m_.fingerLength[i];
            /* Relaxed hand: each joint a little more flexed than the last, curling towards the
               palm, and the outer fingers fanning slightly away from the middle one. */
            const Vec3 fan = across * ((2.5f - static_cast<float>(i)) * 0.045f);
            const Vec3 d1 = (rotateToward(dirDown, palmNormal, radians(10.0f)) + fan).normalizedOr(dirDown);
            const Vec3 d2 = rotateToward(d1, palmNormal, radians(14.0f));
            const Vec3 d3 = rotateToward(d2, palmNormal, radians(12.0f));
            c.fingerKnuckle[i] = knuckle;
            c.fingerJoints[i][0] = knuckle + d1 * (L * 0.44f);
            c.fingerJoints[i][1] = c.fingerJoints[i][0] + d2 * (L * 0.32f);
            c.fingerJoints[i][2] = c.fingerJoints[i][1] + d3 * (L * 0.24f);
            c.fingerTip[i] = c.fingerJoints[i][2];
            c.middleTip = c.fingerTip[2];
        }

        /* Legs: hip -> knee -> ankle, with a small natural genu valgum offset and the
           ankle sitting slightly behind the knee. Toe-out of 7 degrees matches a relaxed
           stance and gives the foot field a sensible axis. */
        c.hipJoint = {sx * m_.legSeparation, m_.y.hipY, 0.0f};
        c.knee = {sx * (m_.legSeparation + 0.004f), m_.y.kneeY, 0.006f};
        c.ankle = {sx * (m_.legSeparation + 0.006f), m_.y.ankleY, -0.004f};
        /*
         * The foot is modelled flat, in a frame hung from the ankle. Its floor is the real floor
         * except in heels, where the ankle stands heelLift higher and the frame is pitched about
         * it until the ball of the foot is back on the ground.
         */
        const float flatFloor = m_.y.soleY + m_.heelLift;
        c.heel = {c.ankle.x, flatFloor + m_.footHeight * 0.5f, c.ankle.z + m_.footHeelZ};
        c.ball = {c.ankle.x + sx * 0.008f, flatFloor + m_.footHeight * 0.42f,
                  c.ankle.z - m_.footLength * 0.52f};
        c.toeTip = {c.ball.x, c.ball.y, c.ball.z - m_.toeLength[1] - m_.toeRadius[1]};
        c.heelL = footLocalRaw(c.heel, c);
        c.ballL = footLocalRaw(c.ball, c);
        if (m_.heelLift > 0.0f) {
            /* Pitch: the point of the sole under the ball has to come down to the floor. */
            const float rise = c.ankle.y - m_.y.soleY;              /* ankle above the floor       */
            const float gs = rise - m_.heelLift;                    /* ankle above the foot's sole */
            const float zb = -c.ballL.z;                            /* ball in front of the ankle  */
            const float reach = std::sqrt(gs * gs + zb * zb);
            const float pitch = std::atan2(zb, gs) - std::acos(clampf(rise / maxf(reach, 1e-4f), -1.0f, 1.0f));
            heelSin_ = std::sin(pitch);
            heelCos_ = std::cos(pitch);
            heelPivotY_ = -gs * heelCos_ - zb * heelSin_;
            heelPivotZ_ = gs * heelSin_ - zb * heelCos_;
            heelShiftY_ = heelPivotY_ + gs;
            heelShiftZ_ = heelPivotZ_ + zb;
            const float norm = std::sqrt(heelSin_ * heelSin_ + (heelCos_ + 1.0f) * (heelCos_ + 1.0f));
            heelBisY_ = -heelSin_ / norm;
            heelBisZ_ = -(heelCos_ + 1.0f) / norm;
            const Vec3 toeL = footLocalRaw(c.toeTip, c);
            c.heel = footPosed(c.heelL, c);
            c.ball = footPosed(c.ballL, c);
            c.toeTip = footPosed(toeL, c);
        }
    }

    /* ---- face anchors ------------------------------------------------------------------- */
    const float headC = (m_.y.crownY + m_.y.chinY) * 0.5f;
    const float headH = m_.y.crownY - m_.y.chinY;
    const float zFront = -m_.head.rz * 0.98f;                      /* -Z is forward */
    /* Anchors on the face. Depths are fractions of zFront, the front of the head, so the features
       sit ON the facial surface that skull() builds: the nasion and the brow slightly behind it,
       the nose tip in front of it, the mouth and the chin stepping back below the nose. */
    noseBridge_ = {0.0f, m_.y.eyeY + headH * 0.012f, zFront * 0.945f};
    noseTip_ = {0.0f, m_.y.eyeY - headH * 0.185f, zFront * 1.225f};
    chin_ = {0.0f, m_.y.chinY + headH * 0.062f, zFront * 0.75f};
    browRidge_ = {0.0f, m_.y.eyeY + headH * 0.062f, zFront * 0.95f};
    mouthCentre_ = {0.0f, m_.mouthY, zFront * 0.925f};
    for (int s = 0; s < 2; ++s) {
        const float sx = (s == 0) ? 1.0f : -1.0f;
        /* Angle of the jaw: below the ear lobe, at the back of the face. */
        jawCorner_[s] = {sx * m_.jaw.rx * (female ? 0.66f : 0.80f), m_.y.chinY + headH * 0.20f, -m_.head.rz * 0.04f};
        cheek_[s] = {sx * m_.head.rx * 0.56f, m_.y.chinY + headH * 0.30f, zFront * 0.70f};
        /* One eye centre for everything: the field, the rig and the texture bake all read
           metrics.eyeZ. The field used its own depth, 11 mm in front of the one the iris was
           painted around. */
        eyeCentre_[s] = {sx * m_.eyeSeparation * 0.5f, m_.y.eyeY, m_.eyeZ};
        earCentre_[s] = {sx * m_.head.rx * 1.05f, m_.y.eyeY - headH * 0.055f, zFront * 0.10f};
    }
    (void)headC; (void)neckBase; (void)neckTop;
}

/* =======================================================================================
 * Torso
 * ======================================================================================= */
float BodyField::torsoProfileRx(float y) const { return maxf(rxProfile_.eval(y), 0.012f); }
float BodyField::torsoProfileRz(float y) const { return maxf(rzProfile_.eval(y), 0.012f); }
float BodyField::torsoProfileCz(float y) const { return czProfile_.eval(y); }

float BodyField::torso(const Vec3& p) const {
    const float yMin = m_.y.crotchY - 0.018f * m_.height;
    const float yMax = m_.y.neckBaseY + 0.010f;
    const float y = clampf(p.y, yMin, yMax);
    const float rx = torsoProfileRx(y);
    const float rz = torsoProfileRz(y);
    const float cz = torsoProfileCz(y);

    /* Super-elliptical cross section (exponent 2.45) - a pure ellipse makes chests and
       abdomens look like sausages; real torsos are flatter in front. */
    const float nx = p.x / rx;
    const float nz = (p.z - cz) / rz;
    const float e = 2.45f;
    const float k = std::pow(std::pow(std::fabs(nx), e) + std::pow(std::fabs(nz), e), 1.0f / e);
    /* Both terms are SIGNED distances and both are negative inside, which is what makes the
       union form below correct: inside the profile tube max() picks the nearest wall, and
       outside it picks the nearest of the radial surface and the two end caps. The final
       length() term rounds the cap rims, which stops the shoulders from ending in a hard
       band. (Using max() against a non-negative "vertical" term is the classic mistake here:
       it silently returns 0 everywhere inside the slab, i.e. an empty body.) */
    const float radial = (k - 1.0f) * minf(rx, rz);
    const float vertical = maxf(yMin - p.y, p.y - yMax);
    float d = minf(maxf(radial, vertical), 0.0f) +
              Vec3(maxf(radial, 0.0f), maxf(vertical, 0.0f), 0.0f).length();

    const float H = m_.height;
    const bool female = (m_.sex == AF_SEX_FEMALE);
    const float chestZ = -m_.chest.rz * 0.85f;
    const float mergeK = lerpf(0.030f, 0.014f, saturate(m_.muscle));

    /* --- chest: pectorals / bust ------------------------------------------------------- */
    if (female && m_.bust > 0.02f) {
        const float r = m_.chest.rx * (0.40f + 0.10f * m_.bust);
        const Vec3 radii(r * 0.92f, r * lerpf(0.85f, 1.05f, m_.bust), r * lerpf(0.82f, 1.02f, m_.bust));
        const float cx = m_.chest.rx * 0.44f;
        /* Centre the breast so that it projects from the FRONT of the rib cage (forward is -Z).
           The old expression evaluated to +62 mm, i.e. behind the spine: the breasts were two
           mounds on the back and the chest was flat. The projection grows with the bust value. */
        const float chestFrontZ = torsoProfileCz(m_.y.bustY) - torsoProfileRz(m_.y.bustY);
        const float projection = lerpf(0.018f, 0.062f, saturate(m_.bust)) * (H / 1.70f);
        const float cz2 = chestFrontZ + radii.z - projection;
        d = smin(d, blob2(p, cx, m_.y.bustY + 0.004f * H, cz2, radii), 0.026f);
    } else {
        const float pecR = m_.chest.rx * (0.42f + 0.10f * m_.muscle);
        const Vec3 radii(pecR, pecR * 0.72f, pecR * 0.52f);
        d = smin(d, blob2(p, m_.chest.rx * 0.46f, m_.y.bustY + 0.005f * H,
                          chestZ * 0.72f, radii), mergeK);
    }

    /* --- abdomen: rectus columns + transverse crease ----------------------------------- */
    const float abW = m_.waist.rx * (0.60f + 0.16f * m_.muscle);
    const float abH = (m_.y.waistY - m_.y.hipY) * 0.34f;
    /* The rectus columns stand proud of the abdomen only as far as the build's muscle says.
       They used to protrude on every body, as two lumps above the waistband. */
    const float abProud = saturate((m_.muscle - 0.45f) / 0.45f);
    const Vec3 abRadii(abW * 0.5f, abH, m_.waist.rz * (0.26f + 0.16f * abProud + m_.abDepth * 2.0f));
    d = smin(d, blob2(p, m_.waist.rx * 0.34f, (m_.y.waistY + m_.y.underbustY) * 0.5f - abH * 0.30f,
                      -m_.waist.rz * 0.72f, abRadii), 0.028f);

    /* --- obliques / serratus ----------------------------------------------------------- */
    if (m_.obliqueBulge > 1e-4f) {
        const Vec3 radii(m_.waist.rx * 0.30f, (m_.y.waistY - m_.y.hipY) * 0.28f, m_.waist.rz * 0.34f);
        d = smin(d, blob2(p, m_.waist.rx * 0.82f, lerpf(m_.y.waistY, m_.y.hipY, 0.35f),
                          -m_.waist.rz * 0.30f, radii), mergeK);
    }

    /* --- belly (adipose, heavy builds) -------------------------------------------------- */
    if (m_.bellyBulge > 1e-4f) {
        const Vec3 radii(m_.waist.rx * 0.82f, (m_.y.waistY - m_.y.hipY) * 0.52f, m_.waist.rz * 0.55f);
        d = smin(d, blob(p, {0.0f, lerpf(m_.y.hipY, m_.y.waistY, 0.55f), -m_.waist.rz * 0.95f}, radii), 0.035f);
    }

    /* --- glutes ------------------------------------------------------------------------ */
    {
        const float gz = m_.hip.rz * (0.50f + 0.10f * saturate(m_.bodyFat));
        const Vec3 radii(m_.hip.rx * 0.50f, m_.hip.rz * 0.60f, m_.hip.rz * 0.52f);
        d = smin(d, blob2(p, m_.hip.rx * 0.45f, m_.y.hipY - m_.hip.rz * 0.36f, gz, radii), 0.034f);
    }

    /* --- spinal furrow -------------------------------------------------------------------
       A shallow channel down the middle of the back, 3 mm deep and about 2 cm wide, following the
       back's own profile from above the hips to between the shoulder blades and fading out at
       both ends.

       It was a straight capsule of 10 mm radius between two fixed points near the back surface,
       cut with a 30 mm blend. The back is not straight, so the capsule broke the surface only
       along the middle third of its length - and there it cut a slot ten centimetres long with
       sharp ends, which read as a wound.

       Carving rule used everywhere in this file: to remove the region {shape < 0} from a field d,
       you need d = smax(d, -shape, k). Writing `smax(d, shape)` treats the *space outside* the
       shape as solid and carves the entire body away. */
    {
        const float yLow = m_.y.hipY + 0.035f * H;
        const float yHigh = m_.y.armpitY + 0.030f * H;
        if (p.y > yLow - 0.05f && p.y < yHigh + 0.05f && p.z > 0.0f && std::fabs(p.x) < 0.05f) {
            const float fade = smoothstepf(yLow, yLow + 0.07f, p.y) * (1.0f - smoothstepf(yHigh - 0.07f, yHigh, p.y));
            const float backZ = cz + rz;                       /* the profile's back surface here */
            /* A cylinder lying along the back, just touching it; the blend below is what cuts the
               channel (about a quarter of its radius deep). Towards the ends the cylinder is moved
               off the surface, further than the blend reaches, so the channel fades to nothing
               instead of stopping at an edge. */
            const float channelR = 0.0105f;
            const float standOff = (1.0f - fade) * 0.024f;
            const float dz = p.z - (backZ + channelR + standOff);
            const float channel = std::sqrt(p.x * p.x + dz * dz) - channelR;
            d = smax(d, -channel, 0.011f);
        }
    }

    /* --- trapezius --------------------------------------------------------------------- */
    for (int s = 0; s < 2; ++s) {
        const float sx = (s == 0) ? 1.0f : -1.0f;
        /* The shoulder girdle: trapezius and clavicle together, as one sloping mass from the side
           of the neck to the acromion. Its top surface IS the shoulder line, so it has to carry
           real thickness all the way out; a thin strand left a dip between neck and deltoid. */
        const Vec3 from(sx * m_.neck.rx * 0.70f, m_.y.neckBaseY - 0.010f * H, m_.neck.centreZ + 0.014f);
        const Vec3 to(sx * m_.biacromialHalf * 0.88f, m_.y.shoulderY + 0.010f * H, 0.002f);
        d = smin(d, sdTaperedCapsule(p, from, to, m_.neck.rx * 0.60f + m_.trapeziusBulge * 0.25f,
                                     m_.deltoidR * 0.50f + m_.trapeziusBulge * 0.10f), 0.040f);
    }

    /* --- latissimus / ribs (only visibly present on muscular builds) -------------------- */
    if (m_.latBulge > 1e-4f) {
        const Vec3 radii(m_.chest.rx * 0.34f, (m_.y.armpitY - m_.y.waistY) * 0.42f, m_.chest.rz * 0.46f);
        d = smin(d, blob2(p, m_.chest.rx * 0.80f, lerpf(m_.y.armpitY, m_.y.waistY, 0.40f),
                          m_.chest.rz * 0.30f, radii), mergeK);
    }

    /* (Two flattened ellipsoids labelled "iliac crest / hip bones" used to be added here, at the
       front of the pelvis at the level of the hip girth. The iliac crests are ten centimetres
       higher than that; at this level the blobs made a horizontal shelf across the lower belly,
       which every garment then inherited.) */

    /*
     * --- neck ------------------------------------------------------------------------------
     * BOTH domains carry the neck, because the domain cut plane (neckCutY) has to slice a real
     * tube in both of them; the two definitions are therefore identical, term for term, so that
     * the smooth union at the top of the build is a no-op inside the neck and a fillet only where
     * the neck meets the shoulders and the jaw.
     *
     * The tube runs up past the jaw and its cap is buried inside the skull, which is where a neck
     * actually ends. It is safe to do that because the oral shapes are applied to the FINAL union
     * (see BodyField::applyOralShapes) rather than inside the head's field: an earlier revision cut
     * the mouth out of the head alone and then smooth-unioned the trunk over it, so the 5 cm-wide
     * neck tube silently refilled the cavity and the avatar came out with a closed face no matter
     * how well the mouth itself was modelled.
     */
    {
        /* The top of the tube is behind the jaw, inside the skull. It used to lean FORWARD to a
           point under the mouth, which put the front of the neck level with the lips: the face
           had no chin and no jaw line, only a column from the shoulders up to the mouth. A throat
           lies three to four centimetres behind the chin. */
        const Vec3 neckBase(0.0f, m_.y.neckBaseY - 0.045f * H, m_.neck.centreZ);
        const Vec3 neckTop(0.0f, m_.y.jawY + 0.010f * H, m_.neck.centreZ + 0.016f);
        d = smin(d, sdTaperedCapsule(p, neckBase, neckTop, m_.neck.rx * 1.08f, m_.neck.rx * 0.88f), 0.030f);
        /* (Two capsules labelled "sterno-mastoid" used to run up the FRONT of the throat and
           meet under the chin. The muscle runs the other way - from the breastbone up and back
           to behind the ear - and what they made was a thick throat under a chin that then
           could not be seen.) */
    }

    /*
     * No surface noise here. This function used to end by displacing the body with ridged and
     * cellular noise ("striation", "pores") a fraction of a millimetre deep. Fine relief belongs in
     * the baked normal map, which has its own: in the distance field it cannot be meshed - its
     * features are smaller than any triangle - so all it contributed was a random error in every
     * vertex position and normal, and three noise evaluations to every one of the millions of
     * field samples a build takes.
     */
    return d;
}

/* =======================================================================================
 * Arms
 * ======================================================================================= */
float BodyField::armChain(const Vec3& p, const Chain& c) const {
    const float sx = static_cast<float>(c.side);
    /* Upper arm and forearm as tapered capsules; the elbow keeps a slightly larger radius
       so the joint does not pinch when the arm bends during animation. */
    float d = sdTaperedCapsule(p, c.shoulder, c.elbow, m_.upperArmR * 1.02f, m_.elbowR);
    d = smin(d, sdTaperedCapsule(p, c.elbow, c.wrist, m_.elbowR * 0.98f, m_.wristR), 0.016f);

    /* Deltoid - the single most important shape for reading a shoulder. It wraps the joint and
       hangs BELOW the acromion: its top is level with the shoulder line, not above it. The cap
       used to be centred above and outside the joint, so its top reached the height of the neck
       base and each shoulder was a ball standing beside the neck. */
    const Vec3 deltoidCentre = c.shoulder + Vec3(sx * m_.deltoidR * 0.10f, -m_.deltoidR * 0.18f, 0.0f);
    d = smin(d, blob(p, deltoidCentre,
                     Vec3(m_.deltoidR * 0.92f, m_.deltoidR * 1.20f, m_.deltoidR * 0.94f)), 0.034f);

    /* Biceps (anterior), triceps (posterior), brachioradialis (forearm mass). */
    const Vec3 upDir = (c.elbow - c.shoulder).normalizedOr({0, -1, 0});
    const Vec3 upMid = c.shoulder + upDir * (m_.upperArmLen * 0.46f);
    if (m_.bicepBulge > 1e-4f) {
        d = smin(d, blob(p, upMid + Vec3(0, 0, -m_.upperArmR * 0.62f),
                         Vec3(m_.upperArmR * 0.62f, m_.upperArmLen * 0.24f, m_.upperArmR * 0.44f + m_.bicepBulge)), 0.020f);
    }
    if (m_.tricepBulge > 1e-4f) {
        d = smin(d, blob(p, c.shoulder + upDir * (m_.upperArmLen * 0.44f) + Vec3(0, 0, m_.upperArmR * 0.60f),
                         Vec3(m_.upperArmR * 0.60f, m_.upperArmLen * 0.26f, m_.upperArmR * 0.42f + m_.tricepBulge)), 0.022f);
    }
    if (m_.brachioradialisBulge > 1e-4f) {
        const Vec3 foreDir = (c.wrist - c.elbow).normalizedOr({0, -1, 0});
        d = smin(d, blob(p, c.elbow + foreDir * (m_.forearmLen * 0.26f) + Vec3(0, 0, -m_.forearmR * 0.55f),
                         Vec3(m_.forearmR * 0.62f, m_.forearmLen * 0.22f, m_.forearmR * 0.52f + m_.brachioradialisBulge)), 0.018f);
    }
    /* (A trench used to be cut along the back of each forearm here, 9 mm deep from elbow to wrist,
       as "a cheap trick that makes an arm look anatomical". A forearm has no such trench.) */
    /* Wrist cap so the hand domain has a matching tube on its side of the cut plane. */
    d = smin(d, sdCapsule(p, c.wrist, c.wrist + (c.palmCentre - c.wrist) * 0.35f, m_.wristR * 0.98f), 0.010f);
    return d;
}

/* =======================================================================================
 * Legs
 * ======================================================================================= */
float BodyField::legChain(const Vec3& p, const Chain& c) const {
    const float sx = static_cast<float>(c.side);
    float d = sdTaperedCapsule(p, c.hipJoint, c.knee, m_.thighRx * 1.03f, m_.kneeR * 0.93f);
    d = smin(d, sdTaperedCapsule(p, c.knee, c.ankle, m_.calfRx * 0.98f, m_.ankleRx), 0.030f);

    const Vec3 thighDir = (c.knee - c.hipJoint).normalizedOr({0, -1, 0});
    const Vec3 kneeToAnkle = (c.ankle - c.knee).normalizedOr({0, -1, 0});
    const float legMerge = lerpf(0.034f, 0.022f, saturate(m_.muscle));

    /* Quads (anterior), hamstrings (posterior), adductors (medial). */
    if (m_.quadBulge > 1e-4f) {
        const Vec3 radii(m_.thighRx * 0.62f, m_.thighLen * 0.30f, m_.thighRz * 0.46f + m_.quadBulge);
        d = smin(d, blob(p, c.hipJoint + thighDir * (m_.thighLen * 0.42f) + Vec3(0, 0, -m_.thighRz * 0.52f), radii), legMerge);
    }
    if (m_.hamstringBulge > 1e-4f) {
        const Vec3 radii(m_.thighRx * 0.58f, m_.thighLen * 0.24f, m_.thighRz * 0.44f + m_.hamstringBulge);
        d = smin(d, blob(p, c.hipJoint + thighDir * (m_.thighLen * 0.34f) + Vec3(0, 0, m_.thighRz * 0.50f), radii), legMerge);
    }
    if (m_.adductorBulge > 1e-4f) {
        const Vec3 radii(m_.thighRx * 0.46f, m_.thighLen * 0.28f, m_.thighRz * 0.42f);
        d = smin(d, blob(p, c.hipJoint + thighDir * (m_.thighLen * 0.34f) - Vec3(sx * m_.thighRx * 0.55f, 0, 0), radii), legMerge);
    }

    /* Knee: patella in front, condyles on the sides, popliteal hollow behind. */
    d = smin(d, blob(p, c.knee + Vec3(0, m_.kneeR * 0.10f, -m_.kneeR * 0.50f),
                     Vec3(m_.kneeR * 0.56f, m_.kneeR * 0.70f, m_.kneeR * 0.40f)), 0.026f);
    {
        const float popliteal = sdEllipsoid(p - (c.knee + Vec3(0, 0, m_.kneeR * 0.85f)),
                                            Vec3(m_.kneeR * 0.52f, m_.kneeR * 0.62f, m_.kneeR * 0.34f));
        d = smax(d, -popliteal - m_.kneeR * 0.10f, 0.020f);
    }

    /* Gastrocnemius (two heads), soleus (low, wide), tibialis anterior. */
    if (m_.gastrocBulge > 1e-4f) {
        for (int h = 0; h < 2; ++h) {
            const float hx = (h == 0 ? 1.0f : -1.0f) * sx * m_.calfRx * 0.26f;
            const Vec3 radii(m_.calfRx * 0.44f, m_.shankLen * 0.24f, m_.calfRz * 0.42f + m_.gastrocBulge);
            d = smin(d, blob(p, c.knee + kneeToAnkle * (m_.shankLen * 0.30f) + Vec3(hx, 0, m_.calfRz * 0.52f), radii), 0.026f);
        }
    }
    if (m_.tibialisBulge > 1e-4f) {
        const Vec3 radii(m_.calfRx * 0.38f, m_.shankLen * 0.22f, m_.calfRz * 0.34f + m_.tibialisBulge);
        d = smin(d, blob(p, c.knee + kneeToAnkle * (m_.shankLen * 0.34f) + Vec3(-sx * m_.calfRx * 0.42f, 0, -m_.calfRz * 0.34f), radii), 0.022f);
    }
    /* Ankle bones: the visual tell of a real ankle. */
    for (int b = 0; b < 2; ++b) {
        const float bx = (b == 0 ? -1.0f : 1.0f) * m_.ankleRx * 0.78f;
        const float by = (b == 0 ? 0.012f : 0.006f);
        d = smin(d, blob(p, c.ankle + Vec3(bx, by, 0.004f),
                         Vec3(m_.ankleRx * 0.34f, m_.ankleRx * 0.34f, m_.ankleRx * 0.30f)), 0.014f);
    }
    /* Wrist cap for the foot domain. */
    d = smin(d, sdCapsule(p, c.ankle, c.ankle + (c.heel - c.ankle) * 0.35f, m_.ankleRx), 0.010f);
    return d;
}

/* =======================================================================================
 * Feet
 * ======================================================================================= */
/*
 * Where toe `i` (0 = big toe) starts and ends, in the foot's local frame: origin at the ankle,
 * toe-out already removed, forward = -Z, lateral = +side in X. One function, used by the foot
 * field and by the nail classifier, so the two cannot disagree.
 */
void BodyField::toeSegment(const Chain& c, int i, float ground, float ballZ, Vec3* from, Vec3* to) const {
    const float sx = static_cast<float>(c.side);
    static const float kAcross[5] = {-0.335f, -0.120f, 0.055f, 0.210f, 0.350f};   /* medial .. lateral */
    const float r = m_.toeRadius[i];
    const float x = sx * m_.footWidth * kAcross[i];
    /* The toe line slants back from the second toe to the little toe. */
    const float zStart = ballZ - m_.footLength * 0.040f + m_.footLength * 0.018f * static_cast<float>(i > 1 ? i - 1 : 0);
    *from = Vec3(x, ground + r * 0.98f, zStart + r * 0.4f);
    *to = Vec3(x * 1.03f, ground + r * 0.90f, zStart - m_.toeLength[i] + r);
}

float BodyField::footShape(const Vec3& p, const Chain& c) const {
    const float sx = static_cast<float>(c.side);
    /*
     * The foot is built in a frame anchored at the ANKLE, with toe-out expressed as a yaw about
     * the vertical through that point, so each foot stays under its own leg. Every lateral offset
     * below is relative to the ankle.
     */
    const Vec3 q = footLocal(p, c);
    const Vec3 heelL = c.heelL;
    const Vec3 ballL = c.ballL;

    const float L = m_.footLength, W = m_.footWidth, Hf = m_.footHeight;
    /* `ground` is the contact plane, ankle-relative. Every primitive is placed by its lowest
       point, and the sole is flattened onto this plane at the end. */
    const float soleGap = 0.002f;
    const float ground = footGround(c) + soleGap;
    const float ballZ = ballL.z;

    /*
     * Heel, instep, forefoot and the outer border, joined with wide blends so the foot is one
     * form. The earlier version stacked four ellipsoids with 10 mm blends between a "heel" and a
     * ball that were 54 mm apart (the heel offset had the wrong sign), which is the ringed lump
     * in front of the ankle; and it had no heel behind the ankle at all.
     */
    float d = sdEllipsoid(q - Vec3(0.0f, ground + Hf * 0.42f, heelL.z),
                          Vec3(W * 0.33f, Hf * 0.46f, L * 0.105f));
    {
        /* Instep: a low, wide ridge from the front of the ankle to the ball. The capsule is
           evaluated in a frame squashed in X, which gives it an elliptical section. */
        const float wide = 1.45f;
        const Vec3 qs(q.x / wide, q.y, q.z);
        const Vec3 a(0.0f, ground + Hf * 0.60f, -L * 0.03f);
        const Vec3 b(sx * 0.004f / wide, ground + Hf * 0.30f, ballZ + L * 0.02f);
        d = smin(d, sdTaperedCapsule(qs, a, b, Hf * 0.36f, Hf * 0.27f), 0.022f);
    }
    /* Forefoot: the ball, the widest part of the foot. */
    d = smin(d, sdEllipsoid(q - Vec3(sx * W * 0.03f, ground + Hf * 0.27f, ballZ + L * 0.035f),
                            Vec3(W * 0.50f, Hf * 0.30f, L * 0.125f)), 0.020f);
    /* Outer border: on the ground from the heel to the little toe. */
    d = smin(d, sdCapsule(q, Vec3(sx * W * 0.20f, ground + Hf * 0.17f, heelL.z * 0.5f),
                          Vec3(sx * W * 0.30f, ground + Hf * 0.16f, ballZ + L * 0.03f), Hf * 0.17f), 0.020f);
    /* Longitudinal arch: carved on the inner side only, between heel and ball. */
    {
        const float arch = sdEllipsoid(q - Vec3(-sx * W * 0.36f, ground - Hf * 0.02f,
                                                (heelL.z + ballZ) * 0.5f + L * 0.01f),
                                       Vec3(W * 0.30f, Hf * 0.30f, L * 0.17f));
        d = smax(d, -arch, 0.014f);
    }

    /* Toes: side by side, each its own chain, joined to one another with a hard minimum (they
       touch; a smooth union would web them) and then blended into the forefoot. */
    float toes = 1e9f;
    for (int i = 0; i < 5; ++i) {
        Vec3 from, to;
        toeSegment(c, i, ground, ballZ, &from, &to);
        const float tr = m_.toeRadius[i];
        float t = sdTaperedCapsule(q, from, to, tr * 1.06f, tr * 0.96f);
        /* Nail plate on top of the last phalanx. */
        t = smin(t, blob(q, to + Vec3(0.0f, tr * 0.62f, tr * 0.10f), Vec3(tr * 0.66f, tr * 0.30f, tr * 0.62f)), 0.002f);
        toes = minf(toes, t);
    }
    d = smin(d, toes, 0.008f);

    /* Ankle stump, at the local origin, so the foot and shin fields overlap across the joint. */
    d = smin(d, sdCapsule(q, Vec3(0, -0.025f, 0.0f), Vec3(0, 0.025f, 0.002f), m_.ankleRx * 1.02f), 0.016f);

    /* Flatten the sole onto the contact plane: smooth unions push the surface outward, and the
       sole has to be a plane to stand on. */
    d = smax(d, ground - q.y, 0.004f);
    return d;
}

/* =======================================================================================
 * Hands
 * ======================================================================================= */
float BodyField::handShape(const Vec3& p, const Chain& c) const {
    const float sx = static_cast<float>(c.side);
    const Vec3 dirDown = (c.wrist - c.elbow).normalizedOr({0, -1, 0});
    const Vec3 palmNormal(-sx, 0.0f, 0.0f);
    /* Same frame as the chain: `across` points from the little finger to the thumb (forward). */
    const Vec3 across = (palmNormal.cross(dirDown) * -sx).normalizedOr({0, 0, -1});

    /*
     * Palm: a flat slab with fully rounded edges, in the hand's own frame, a little narrower at
     * the wrist than at the knuckles. An ellipsoid (the previous shape) comes to a point at both
     * ends, so at the knuckle line it was a third of the width of the finger row and the fingers
     * hung off a ball; a box with a large corner radius (the shape before that) was 52 mm thick.
     */
    const Vec3 q = p - c.palmCentre;
    const float alongDown = q.dot(dirDown);
    const float alongAcross = q.dot(across);
    const float alongNormal = q.dot(palmNormal);
    const float halfLen = m_.handLength * 0.26f;
    const float edge = m_.palmThickness * 0.44f;
    const float widen = lerpf(0.84f, 1.0f, saturate((alongDown + halfLen) / maxf(2.0f * halfLen, 1e-4f)));
    float d = sdRoundBox(Vec3(alongAcross / widen, alongDown, alongNormal),
                         Vec3(maxf(m_.palmWidth * 0.5f - edge, 1e-4f), maxf(halfLen - edge * 0.5f, 1e-4f),
                              maxf(m_.palmThickness * 0.5f - edge, 1e-4f)),
                         edge);
    /* Thenar pad (base of the thumb) and hypothenar pad (little-finger side), on the palm side. */
    d = smin(d, blob(Vec3(alongAcross, alongDown, alongNormal),
                     Vec3(m_.palmWidth * 0.30f, -m_.handLength * 0.07f, m_.palmThickness * 0.22f),
                     Vec3(m_.palmWidth * 0.26f, m_.handLength * 0.15f, m_.palmThickness * 0.40f)), 0.012f);
    d = smin(d, blob(Vec3(alongAcross, alongDown, alongNormal),
                     Vec3(-m_.palmWidth * 0.30f, -m_.handLength * 0.05f, m_.palmThickness * 0.16f),
                     Vec3(m_.palmWidth * 0.18f, m_.handLength * 0.14f, m_.palmThickness * 0.34f)), 0.012f);

    /* Wrist: the stump the cut plane needs, tapering into the heel of the hand so that the round
       forearm becomes the flat palm without a step on the back of the hand. */
    d = smin(d, sdTaperedCapsule(p, c.wrist - dirDown * 0.02f, c.palmCentre - dirDown * (halfLen * 0.25f),
                                 m_.wristR, m_.palmThickness * 0.52f), 0.014f);

    /* Knuckle ridge: the heads of the metacarpals, which is what the fingers actually grow from. */
    d = smin(d, sdTaperedCapsule(p, c.fingerKnuckle[1], c.fingerKnuckle[4],
                                 m_.fingerRadius[1] * 1.12f, m_.fingerRadius[4] * 1.12f), 0.008f);
    d = smin(d, sdTaperedCapsule(p, c.fingerKnuckle[2], c.fingerKnuckle[3],
                                 m_.fingerRadius[2] * 1.14f, m_.fingerRadius[3] * 1.14f), 0.008f);

    /*
     * Fingers. Each finger is one smooth chain of three phalanges; DIFFERENT fingers are joined
     * with a hard minimum, because neighbouring fingers touch at the base and any smooth union
     * between them webs them into a mitten. The finished row is then blended into the knuckle
     * ridge, which gives the root of each finger a fillet without bridging the gaps.
     */
    float fingers = 1e9f;
    for (int i = 0; i < 5; ++i) {
        const Vec3 j0 = c.fingerKnuckle[i];
        const Vec3 j1 = c.fingerJoints[i][0];
        const Vec3 j2 = c.fingerJoints[i][1];
        const Vec3 j3 = c.fingerJoints[i][2];
        const bool thumb = (i == 0);
        const float r0 = m_.fingerRadius[i] * (thumb ? 1.16f : 1.06f);
        const float r1 = m_.fingerRadius[i] * (thumb ? 1.00f : 0.96f);
        const float r2 = m_.fingerRadius[i] * (thumb ? 0.94f : 0.88f);
        const float r3 = m_.fingerRadius[i] * (thumb ? 0.86f : 0.78f);
        float f = sdTaperedCapsule(p, j0, j1, r0, r1);
        f = smin(f, sdTaperedCapsule(p, j1, j2, r1, r2), 0.003f);
        f = smin(f, sdTaperedCapsule(p, j2, j3, r2, r3), 0.003f);
        /* Joint bulges, slight: enough to read as knuckles, not as beads. */
        f = smin(f, blob(p, j1, Vec3(r1 * 1.06f, r1 * 1.06f, r1 * 1.06f)), 0.003f);
        f = smin(f, blob(p, j2, Vec3(r2 * 1.05f, r2 * 1.05f, r2 * 1.05f)), 0.003f);
        /* Nail plate: a thin raised plate on the DORSAL side (away from the palm) of the tip. */
        const Vec3 along = (j3 - j2).normalizedOr(dirDown);
        f = smin(f, blob(p, j3 - palmNormal * (r3 * 0.62f) - along * (r3 * 0.25f),
                         Vec3(r3 * 0.62f, r3 * 0.62f, r3 * 0.62f) * 0.9f +
                         along.abs() * (r3 * 0.30f)), 0.002f);
        fingers = minf(fingers, f);
    }
    d = smin(d, fingers, 0.006f);
    return d;
}

/* =======================================================================================
 * Head and face
 * ======================================================================================= */
/* ==========================================================================================
 * Facial sub-shapes.
 *
 * Everything below is shared: headShape() unions them into the head surface and
 * classifyRegion() asks the same functions which material a surface point belongs to. The head
 * is otherwise one smooth mass, and the difference between "a face" and "a face with a mouth" is
 * entirely about what is carved out of it and what sits inside the cavity.
 * ========================================================================================== */

Vec3 BodyField::archCentre() const {
    /*
     * One anchor for the whole mouth. The dental arch, the oral cavity, the aperture and the
     * classifier are all expressed in terms of this point, because the first version of this code
     * placed the teeth from one formula and the void they live in from another, and the two ended
     * up 7 mm apart: every tooth was buried in the cheek and the mouth contained no geometry at
     * all. Anything that has to agree is derived from here, once.
     */
    const Vec3 m = mouthCentre_;
    const float archRz = m_.head.rz * 0.30f;
    const float band = m_.lipWidth * 0.15f;
    return Vec3(m.x, m.y, m.z + 0.002f + archRz + band + 0.003f);
}

float BodyField::lipsOutline(const Vec3& p) const {
    /* Negative inside the outline of the lips, in the plane of the face. Two ellipses - the same
       two whose solids make the lips' relief in lipsShape() - with a notch at the top for the
       bow of the upper lip. */
    if (std::fabs(p.z - mouthCentre_.z) > 0.016f) return 1.0f;
    const float lw = m_.lipWidth;
    const float upThick = lw * (m_.sex == AF_SEX_FEMALE ? 0.110f : 0.092f);
    const float loThick = lw * (m_.sex == AF_SEX_FEMALE ? 0.140f : 0.118f);
    const float dx = p.x - mouthCentre_.x;
    const float dy = p.y - mouthCentre_.y;
    const auto ellipse = [](float x, float y, float rx, float ry) {
        return (std::sqrt((x / rx) * (x / rx) + (y / ry) * (y / ry)) - 1.0f) * minf(rx, ry);
    };
    float upper = ellipse(dx, dy - upThick * 0.80f, lw * 0.50f, upThick);
    const float notchY = dy - upThick * 2.35f;
    upper = maxf(upper, upThick * 0.75f - std::sqrt(dx * dx + notchY * notchY));
    const float lower = ellipse(dx, dy + loThick * 0.78f, lw * 0.45f, loThick);
    return minf(upper, lower);
}

float BodyField::lipsShape(const Vec3& p) const {
    const float lw = m_.lipWidth;
    const Vec3 m = mouthCentre_;
    /* Semi-heights of the upper and lower vermilion. They were 8.4 and 11 mm, which stacked to a
       30 mm tall mouth; a full adult mouth is about 20 mm from the top of the upper lip to the
       bottom of the lower one. */
    const float upThick = lw * (m_.sex == AF_SEX_FEMALE ? 0.110f : 0.092f);
    const float loThick = lw * (m_.sex == AF_SEX_FEMALE ? 0.140f : 0.118f);
    /* Upper and lower lip are joined with a HARD minimum: the crease where the two convex forms
       meet is the lip line, and a smooth union would fill it in. */
    float lips = sdEllipsoid(p - Vec3(m.x, m.y + upThick * 0.80f, m.z + lw * 0.020f), Vec3(lw * 0.50f, upThick, lw * 0.18f));
    lips = minf(lips, sdEllipsoid(p - Vec3(m.x, m.y - loThick * 0.78f, m.z + lw * 0.030f), Vec3(lw * 0.45f, loThick, lw * 0.19f)));
    for (int side = 0; side < 2; ++side) {
        const float sx = (side == 0) ? 1.0f : -1.0f;
        lips = smin(lips, blob(p, Vec3(m.x + sx * lw * 0.50f, m.y - lw * 0.01f, m.z + lw * 0.05f),
                               Vec3(lw * 0.09f, lw * 0.07f, lw * 0.10f)), 0.006f);
    }
    return lips;
}

float BodyField::mouthAperture(const Vec3& p) const {
    /*
     * The opening itself: a 40 mm wide, 2.6 mm tall slot cut clean through the front wall of the
     * lips. This is what turns the vertical gap between the vermilion envelopes into a real hole
     * rather than a hidden pocket, and it is why the upper and lower lips read as separate forms
     * with a line between them: everything above the slot is the upper lip, everything below is
     * the lower lip, and the slot is the dark interior.
     */
    const Vec3 m = mouthCentre_;
    const float lw = m_.lipWidth;
    /*
     * Depth is 25% of the head's front-to-back radius, which puts the slot's leading face a few
     * millimetres in FRONT of the lip surface. That is deliberate and load-bearing: at 16% the slot
     * stopped short of the skin, the mouth was a closed pocket with a smooth face over it, and the
     * avatar had no mouth line at all. The slot must break the surface; everything above and below
     * it is then the upper and lower lip by construction.
     */
    const Vec3 c(m.x, m.y, m.z + 0.004f);
    return sdRoundBox(p - c, Vec3(m_.mouthWidth * 0.40f, lw * 0.012f, m_.head.rz * 0.25f), 0.0012f);
}

float BodyField::oralCavity(const Vec3& p) const {
    /*
     * The void behind the aperture, sized to CONTAIN the dental arch and the tongue with a small
     * clearance. An elliptical column in the horizontal plane, clipped vertically to a quarter of
     * the face height.
     *
     * The ellipse is not a stylistic choice. A box would have a flat front face, and a flat face
     * at the lip plane is 3-7 mm in front of the skin at the corners of the mouth (the head curves
     * away, the box does not), so the carve would punch holes in the cheeks. An ellipse whose
     * front is tangent to the arch retreats with x exactly as the cheek does, and never breaks the
     * surface except through the aperture.
     */
    const Vec3 c = archCentre();
    const float h = m_.y.eyeY - m_.y.chinY;
    const float archRx = m_.mouthWidth * 0.46f;
    const float archRz = m_.head.rz * 0.30f;
    const float band = m_.lipWidth * 0.15f;
    const float rx = archRx + band + 0.004f;
    const float rz = archRz + band + 0.006f;
    const float ex = (p.x - c.x) / rx;
    const float ez = (p.z - c.z) / rz;
    float cavity = (std::sqrt(ex * ex + ez * ez) - 1.0f) * minf(rx, rz);
    cavity = smax(cavity, std::fabs(p.y - c.y) - h * 0.125f, 0.004f);
    return cavity;
}

float BodyField::teethShape(const Vec3& p) const {
    /*
     * Dental arches: an elliptical band in the horizontal plane, closed at the front and open at
     * the back (a horseshoe), split at the occlusal plane into an upper and a lower arch.
     *
     * The band is the tooth row plus the alveolar bone around it; the split at the lip line is
     * where the incisal edges meet. Everything above the split and inside the band is the upper
     * arch: crowns in the lowest 6 mm, gums above them, roots disappearing into the maxilla. Same
     * mirrored below. The result is one connected shell per arch, closing on itself inside the
     * bone, which is what a skull actually looks like and what a watertight mesh needs.
     */
    const Vec3 c = archCentre();
    const float archRx = m_.mouthWidth * 0.46f;
    const float archRz = m_.head.rz * 0.30f;
    const float band = m_.lipWidth * 0.15f;
    const float crownH = (m_.y.eyeY - m_.y.chinY) * 0.052f;
    const float gumH = crownH * 1.7f;

    const float ex = (p.x - c.x) / archRx;
    const float ez = (p.z - c.z) / archRz;
    const float radial = (std::sqrt(ex * ex + ez * ez) - 1.0f) * minf(archRx, archRz);
    float arch = std::fabs(radial) - band;
    arch = smax(arch, p.z - (c.z + archRz * 0.45f), 0.002f);      /* open the back */

    /* At rest the teeth do not touch: there is a gap of a few millimetres behind the closed
       lips. Without it the incisal edges met exactly on the lip line and showed through the
       mouth opening as a white stripe. */
    const float restGap = crownH * 0.34f;
    float upper = smax(arch, (c.y + restGap) - p.y, 0.0015f);
    upper = smax(upper, p.y - (c.y + crownH + gumH), 0.0025f);
    float lower = smax(arch, p.y - (c.y - restGap), 0.0015f);
    lower = smax(lower, (c.y - crownH - gumH) - p.y, 0.0025f);
    return minf(upper, lower);
}

float BodyField::tongueShape(const Vec3& p) const {
    const Vec3 m = mouthCentre_;
    const float h = m_.y.eyeY - m_.y.chinY;
    const Vec3 c(m.x, m.y - h * 0.045f, m.z + m_.head.rz * 0.30f);
    float t = sdEllipsoid(p - c, Vec3(m_.mouthWidth * 0.33f, h * 0.030f, m_.head.rz * 0.30f));
    /* Tip, angled up towards the back of the upper incisors. */
    t = smin(t, blob(p, c + Vec3(0.0f, h * 0.012f, -m_.head.rz * 0.28f),
                     Vec3(m_.mouthWidth * 0.20f, h * 0.016f, m_.head.rz * 0.12f)), 0.004f);
    return t;
}

float BodyField::eyeballShape(const Vec3& p, int side) const {
    /*
     * Sclera sphere plus a corneal bulge on the front. The bulge matters for more than looks: the
     * renderer refracts through the cornea, and without a protruding anterior surface there is no
     * refraction volume to sample, only a flat disc painted with an iris.
     */
    const int idx = (side > 0) ? 0 : 1;
    const Vec3 e = eyeCentre_[static_cast<size_t>(idx)];
    const float r = m_.eyeRadius;
    /* The socket ceiling used to sit at the eyeball centre; lifting it by a fifth of the radius
       gives the lid a place to close onto and stops the sclera from being sliced flat on top. */
    float eyeball = (p - e).length() - r;
    /* The cornea: a dome over the iris, about half the globe's radius across and rising a little
       over a millimetre from it. It was an ellipsoid three quarters of the radius wide, pushed far
       enough forward to stand in front of the lids - a bead, not an eye. */
    const Vec3 corneaCentre = e + Vec3(0.0f, 0.0f, -r * 0.80f);
    eyeball = smin(eyeball, sdEllipsoid(p - corneaCentre, Vec3(r * 0.50f, r * 0.50f, r * 0.29f)), 0.004f);
    return eyeball;
}

float BodyField::eyeOpening(const Vec3& p, int side) const {
    return eyeOpening(p, side, nullptr);
}

float BodyField::eyeOpening(const Vec3& p, int side, bool* upperLid) const {
    /*
     * The opening between the lids, seen from the front: a lens with pointed corners. The line
     * between the corners lies a little below the centre of the eye and tilts up towards the
     * outside; the upper lid arches over it more than the lower lid sags under it, so the upper
     * lid covers the top of the iris and the lower lid just meets its bottom edge - which is how a
     * relaxed eye looks. (It was an ellipse a third of the radius high: a slot, with round ends.)
     *
     * faceFeatures() cuts this through the lids, and the classifier accepts a point as "eyeball"
     * only inside it - so the lid margin, which lies within the classifier's tolerance of the
     * globe, is skin and not sclera.
     */
    const int idx = (side > 0) ? 0 : 1;
    const float sx = (side > 0) ? 1.0f : -1.0f;
    const Vec3 e = eyeCentre_[static_cast<size_t>(idx)];
    const float r = m_.eyeRadius;
    const float lid = r * kLidThickness;
    const Vec3 q = p - (e + Vec3(sx * r * 0.04f, -r * 0.12f, -(r + lid) * 0.98f));
    const float slant = radians(m_.sex == AF_SEX_FEMALE ? 6.0f : 3.0f) * sx;
    const Vec3 qr(q.x * std::cos(slant) + q.y * std::sin(slant),
                  -q.x * std::sin(slant) + q.y * std::cos(slant), q.z);
    const float halfWidth = r * 1.14f;
    const float rise = (qr.y >= 0.0f) ? r * 0.48f : r * 0.36f;
    if (upperLid) *upperLid = (qr.y >= 0.0f);
    const float u = qr.x / halfWidth;
    /* |y| - rise * (1 - u^2), divided by the length of its gradient to approximate a distance. */
    const float slope = 2.0f * rise * std::fabs(u) / halfWidth;
    const float lens = (std::fabs(qr.y) - rise * (1.0f - u * u)) / std::sqrt(1.0f + slope * slope);
    const float depth = std::fabs(qr.z) - r * 0.66f;
    return maxf(lens, depth);
}

float BodyField::browShape(const Vec3& p) const {
    /*
     * Eyebrows: an arched band above each eye, thick at the nose end and tapering to a point at
     * the temple, a little higher and finer on female faces.
     *
     * The band is defined as seen from the front - a distance in X and Y only - and applies to the
     * front of the face. It used to be a pair of capsules at fixed positions in space, so whether a
     * brow showed depended on where the surface of the forehead happened to be: any change to the
     * skull buried half of each brow or left it standing off the face.
     *
     * Negative inside the band. faceFeatures() raises the skin by this band and the classifier
     * paints it, so the two cannot disagree.
     */
    if (p.z > -m_.head.rz * 0.40f) return 1.0f;
    const float r = m_.eyeRadius;
    const bool female = (m_.sex == AF_SEX_FEMALE);
    const float lift = female ? 1.10f : 1.0f;
    const float weight = female ? 0.85f : 1.0f;
    const Vec3 q(p.x, p.y, 0.0f);
    float band = 1e9f;
    for (int side = 0; side < 2; ++side) {
        const float sx = (side == 0) ? 1.0f : -1.0f;
        const Vec3 e = eyeCentre_[static_cast<size_t>(side)];
        const Vec3 inner(e.x - sx * r * 1.00f, e.y + r * 1.26f * lift, 0.0f);
        const Vec3 peak(e.x + sx * r * 0.62f, e.y + r * 1.50f * lift, 0.0f);
        const Vec3 outer(e.x + sx * r * 1.80f, e.y + r * 1.22f * lift, 0.0f);
        band = minf(band, sdTaperedCapsule(q, inner, peak, r * 0.25f * weight, r * 0.20f * weight));
        band = minf(band, sdTaperedCapsule(q, peak, outer, r * 0.20f * weight, r * 0.06f * weight));
    }
    return band;
}

float BodyField::skull(const Vec3& p) const {
    const float headH = m_.y.crownY - m_.y.chinY;
    const float headC = (m_.y.crownY + m_.y.chinY) * 0.5f;
    const bool female = (m_.sex == AF_SEX_FEMALE);
    const float zFront = -m_.head.rz * 0.98f;

    /*
     * The head is two masses, the way a head is drawn: the brain case, and the face hung from its
     * front, tapering to the chin. Everything else is a small addition to those two.
     *
     * The previous construction was one ellipsoid with a dozen landmark blobs and four carved
     * hollows (temples and cheeks). On a lean build every hollow was cut at full depth, and the
     * jaw-angle blobs sat on the cheeks in front of the ears, so the result was a skull with
     * lumps rather than a face.
     *
     * Brain case: its top is exactly crownY, because the crown is the stature landmark.
     */
    const Vec3 cranC(0.0f, headC + headH * 0.085f, m_.head.rz * 0.10f);
    float d = sdEllipsoid(p - cranC, Vec3(m_.head.rx * 1.03f, headH * 0.415f, m_.head.rz * 1.00f));
    /* Occiput. */
    d = smin(d, blob(p, {0.0f, headC - headH * 0.04f, m_.head.rz * 0.80f},
                     Vec3(m_.head.rx * 0.80f, headH * 0.20f, m_.head.rz * 0.34f)), 0.030f);

    /* Face mass: brow to chin, widest at the cheekbones, narrowing downwards. */
    /* It is an ellipsoid whose widest level is just under the eyes and whose lower end is the
       chin, so its outline from the front is the oval a face is: about 0.9 of the skull's width at
       the cheekbones, three quarters of that at the corners of the jaw. It used to be centred at
       the level of the nose and as wide as the jaw measurement all the way down, which gave every
       face the lower half of a cube. */
    const Vec3 faceC(0.0f, m_.y.chinY + headH * 0.43f, -m_.head.rz * 0.20f);
    {
        /* Below its widest level the oval narrows faster than an ellipse does - an egg, small end
           down - by a sixth on a female face and a tenth on a male one. */
        Vec3 q = p - faceC;
        const float ry = headH * 0.44f;
        const float narrowing = lerpf(1.0f, female ? 0.84f : 0.90f, smoothstepf(0.0f, 1.0f, -q.y / ry));
        q.x /= narrowing;
        d = smin(d, sdEllipsoid(q, Vec3(m_.head.rx * (female ? 0.89f : 0.94f), ry, m_.head.rz * 0.80f)), 0.036f);
    }
    /* Chin, and the lower border of the mandible running back to the angle of the jaw. */
    d = smin(d, blob(p, chin_, Vec3(m_.head.rx * (female ? 0.22f : 0.33f), headH * 0.058f,
                                    m_.head.rz * 0.25f)), 0.022f);
    for (int s = 0; s < 2; ++s) {
        const float sx = (s == 0) ? 1.0f : -1.0f;
        const Vec3 chinSide(sx * m_.head.rx * (female ? 0.17f : 0.24f), m_.y.chinY + headH * 0.045f, zFront * 0.66f);
        d = smin(d, sdTaperedCapsule(p, chinSide, jawCorner_[s], headH * (female ? 0.026f : 0.040f),
                                     headH * (female ? 0.030f : 0.050f)), 0.024f);
        /* Cheekbone and the soft cheek below it. */
        /* These two stand a few millimetres proud of the oval, no more. They used to stand one to
           two centimetres proud, which is a face with something in each cheek. */
        d = smin(d, blob(p, {sx * m_.head.rx * 0.70f, m_.y.eyeY - headH * 0.095f, zFront * 0.56f},
                         Vec3(m_.head.rx * 0.20f, headH * 0.055f, m_.head.rz * 0.20f)), 0.020f);
        d = smin(d, blob(p, Vec3(cheek_[s].x, cheek_[s].y, zFront * 0.58f),
                         Vec3(m_.head.rx * 0.24f, headH * 0.090f, m_.head.rz * 0.20f)), 0.020f);
    }
    /* Brow ridge: slight, and slighter still on female faces. */
    {
        const float amp = female ? 0.55f : 1.0f;
        d = smin(d, sdCapsule(p, {-m_.head.rx * 0.58f, browRidge_.y, browRidge_.z + m_.head.rz * 0.11f},
                              {m_.head.rx * 0.58f, browRidge_.y, browRidge_.z + m_.head.rz * 0.11f},
                              m_.head.rz * 0.10f * (0.75f + 0.25f * amp)), 0.024f);
    }
    /* Ears: flattened, slightly rotated discs with a concha cavity. */
    for (int s = 0; s < 2; ++s) {
        const float sx = (s == 0) ? 1.0f : -1.0f;
        const Vec3 e = earCentre_[s];
        const float tilt = radians(12.0f) * sx;
        const Vec3 rq(p.x - e.x, p.y - e.y, p.z - e.z);
        const float rx = rq.x * std::cos(tilt) - rq.y * std::sin(tilt);
        const float ry = rq.x * std::sin(tilt) + rq.y * std::cos(tilt);
        const Vec3 er(rx, ry, rq.z);
        float ear = sdEllipsoid(er, Vec3(m_.head.rx * 0.075f, m_.earLength * 0.52f, m_.head.rz * 0.30f));
        ear = smax(ear, -sdEllipsoid(er - Vec3(sx * m_.head.rx * 0.03f, 0, -m_.head.rz * 0.03f),
                                     Vec3(m_.head.rx * 0.055f, m_.earLength * 0.34f, m_.head.rz * 0.20f)) - 0.004f, 0.010f);
        d = smin(d, ear, 0.016f);
    }
    return d;
}

float BodyField::faceFeatures(const Vec3& p) const {
    const float headH = m_.y.crownY - m_.y.chinY;
    const bool female = (m_.sex == AF_SEX_FEMALE);
    float d = skull(p);

    /* Nose: dorsum from the nasion to the tip, the tip itself, the two alae, and the nostrils. */
    {
        const Vec3 bridge = noseBridge_;
        const Vec3 tip = noseTip_;
        const float w = m_.head.rx * (female ? 0.115f : 0.135f);
        float nose = sdTaperedCapsule(p, bridge, tip + Vec3(0, headH * 0.012f, w * 0.55f), w * 0.72f, w * 0.92f);
        nose = smin(nose, blob(p, tip + Vec3(0, 0, w * 0.30f), Vec3(w * 1.05f, w * 0.95f, w * 1.05f)), 0.006f);
        for (int s = 0; s < 2; ++s) {
            const float sx = (s == 0) ? 1.0f : -1.0f;
            nose = smin(nose, blob(p, tip + Vec3(sx * w * 1.30f, -headH * 0.012f, w * 1.45f),
                                   Vec3(w * 0.85f, w * 0.72f, w * 1.00f)), 0.006f);
            const float nostril = sdEllipsoid(p - (tip + Vec3(sx * w * 0.80f, -headH * 0.034f, w * 1.20f)),
                                              Vec3(w * 0.42f, w * 0.36f, w * 0.62f));
            nose = smax(nose, -nostril, 0.003f);
        }
        /* Columella down to the upper lip. */
        nose = smin(nose, sdCapsule(p, tip + Vec3(0, -headH * 0.020f, w * 0.75f),
                                    tip + Vec3(0, -headH * 0.045f, w * 1.90f), w * 0.36f), 0.004f);
        d = smin(d, nose, 0.014f);
    }

    /* The muzzle: the gentle forward swell of the upper jaw and teeth that the lips lie on. Without
       it the mouth sits in a flat or hollow band between the nose and the chin. */
    d = smin(d, blob(p, mouthCentre_ + Vec3(0.0f, headH * 0.018f, m_.head.rz * 0.20f),
                     Vec3(m_.head.rx * 0.42f, headH * 0.100f, m_.head.rz * 0.205f)), 0.016f);
    /* Lips: the shared vermilion envelope (see lipsShape) blended into the face. */
    d = smin(d, lipsShape(p), 0.008f);

    /*
     * Eyes. For each eye, in this order:
     *   1. a shallow dish in the face, so the eye sits under the brow rather than on a flat front;
     *   2. the lids: a thin shell around the eyeball, blended into brow and cheek;
     *   3. the opening, an almond cut through everything that is in front of the eyeball;
     *   4. the eyeball itself, last and with a hard minimum, so the lid margin stays a crisp edge.
     *
     * The previous eye was a deep socket carved 1.5 radii wide with two ellipsoid "lids" above and
     * below it. Nothing closed the corners, so the result was a small bead at the bottom of a pit.
     */
    const float r = m_.eyeRadius;
    const float lid = r * kLidThickness;
    for (int s = 0; s < 2; ++s) {
        const float sx = (s == 0) ? 1.0f : -1.0f;
        const Vec3 e = eyeCentre_[s];
        const float dish = sdEllipsoid(p - (e + Vec3(sx * r * 0.10f, r * 0.05f, -r * 0.62f)),
                                       Vec3(r * 1.70f, r * 1.12f, r * 0.80f));
        d = smax(d, -dish, 0.012f);
        d = smin(d, (p - e).length() - (r + lid), 0.007f);
    }
    for (int side : {-1, 1}) {
        d = smax(d, -eyeOpening(p, side), 0.0010f);
    }
    (void)female;
    /* Eyebrows: the skin inside the brow band stands 0.7 mm proud, fading over the band's edge.
       See browShape(). */
    d -= 0.0007f * (1.0f - smoothstepf(-0.0012f, 0.0012f, browShape(p)));

    /* Eyeballs, added LAST. The join with the lid margin is rounded over less than a millimetre:
       enough for the mesher to have a surface to follow round the base of the lid (a perfectly
       sharp inside corner is where its vertices were left standing as spikes), too little to
       blend the white of the eye into the lid. */
    for (int side : {-1, 1}) {
        d = smin(d, eyeballShape(p, side), 0.0008f);
    }

    /*
     * No surface noise here either. The face ended with cellular "pore" noise: cells 5 mm across,
     * up to 0.2 mm deep, on everything including the eyeballs. Once the face is meshed finely
     * enough to resolve a lid margin it resolves those cells too, and they appear as a mosaic of
     * tiles on the eye and as crosses on the brow and the lips. Skin texture is the normal map's
     * job.
     */
    return d;
}

/* =======================================================================================
 * Domain entry points
 * ======================================================================================= */
float BodyField::body(const Vec3& p) const {
    float d = torso(p);
    /*
     * Left and right limbs are unioned with a HARD min, never a smooth one. Two surfaces that
     * are merely close - the inner thighs, the two feet - would otherwise be bridged by a web:
     * smooth-min subtracts up to k/4 from the smaller distance, and with k comparable to the gap
     * the field turns negative in the empty space between them (measured: it produced a 6 mm
     * sheet of phantom geometry under the arches, which the floor-drop then read as the sole).
     * Same-side limb-to-torso blends keep a smooth union, where a real fillet is wanted and the
     * parts genuinely overlap.
     */
    const float dLeft = smin(smin(d, armChain(p, left_), armFillet(p, left_)), legChain(p, left_), 0.030f);
    const float dRight = smin(smin(d, armChain(p, right_), armFillet(p, right_)), legChain(p, right_), 0.030f);
    return minf(dLeft, dRight);
}

float BodyField::armFillet(const Vec3& p, const Chain& c) const {
    /*
     * Blend radius between an arm and the trunk: the full fillet at the shoulder and the armpit,
     * falling to zero (a hard union) by the lower third of the upper arm.
     *
     * A constant radius blended the WHOLE arm with the trunk. The forearm and hand hang within a
     * few centimetres of the hip, and a smooth minimum pulls both surfaces towards each other
     * across such a gap: the hip grew a bump opposite the wrist, large enough to come through the
     * trousers.
     */
    const Vec3 axis = (c.elbow - c.shoulder).normalizedOr({0.0f, -1.0f, 0.0f});
    const float along = (p - c.shoulder).dot(axis);
    return 0.034f * (1.0f - smoothstepf(m_.upperArmLen * 0.40f, m_.upperArmLen * 0.85f, along));
}

float BodyField::trunkAndLegs(const Vec3& p) const {
    /* The body without its arms: what a garment's trunk and trouser legs are an offset of. */
    const float t = torso(p);
    return minf(smin(t, legChain(p, left_), 0.030f), smin(t, legChain(p, right_), 0.030f));
}

float BodyField::skinUnion(const Vec3& p) const {
    /* The unclothed body: trunk and limbs with each hand and foot, then the head. Left and right
       are joined with a hard minimum (see body()). */
    const float dBody = body(p);
    const float dLeft = smin(smin(dBody, hand(p, 1), 0.006f), foot(p, 1), 0.006f);
    const float dRight = smin(smin(dBody, hand(p, -1), 0.006f), foot(p, -1), 0.006f);
    return smin(minf(dLeft, dRight), head(p), 0.012f);   /* neck blend */
}

void BodyField::applyOralShapes(float& d, const Vec3& p) const {
    /*
     * The mouth interior: an aperture through the lips, the cavity behind it, and the teeth and
     * tongue inside. It is built only when apparel().openMouth is set.
     *
     * By default the mouth is CLOSED and none of this exists. The opening is a slot about 3 mm
     * tall, which is at or below the finest extraction cell: whether the lattice resolved it was
     * a matter of phase, and where it half-resolved it the lips came out as torn, stretched
     * triangles with the teeth showing through as a white stripe. A closed mouth is the rest
     * pose of a face, it extracts as one clean surface, and it does not spend a tenth of the
     * mesh on teeth and tongue that nothing can see.
     */
    if (!apparel_.openMouth) return;
    d = smax(d, -mouthAperture(p), 0.0015f);
    d = smax(d, -oralCavity(p), 0.008f);
    d = smin(d, teethShape(p), 0.003f);
    d = smin(d, tongueShape(p), 0.004f);
}

float BodyField::head(const Vec3& p) const {
    /* The head domain ends where its neck stub does. Below that, a conservative distance is all
       the union needs, and the facial features do not have to be evaluated. */
    {
        const float lowest = m_.y.neckBaseY - 0.045f * m_.height - m_.neck.rx * 1.25f;
        if (p.y < lowest - 0.04f) return lowest - p.y;
    }
    float d = faceFeatures(p);
    /*
     * The neck: a tapered tube from the shoulders up to just BENEATH the chin. Two constraints
     * fight here and the second one is easy to miss.
     *   - it has to cross the neck cut plane (1.45 m) as a single tube, so the trunk's and head's
     *     surfaces meet on a clean ring;
     *   - it must NOT reach as high as the mouth. The neck's radius is ~5-6 cm and its axis is
     *     close to the body's midline, while the oral cavity sits 40 mm behind the lips at almost
     *     the same height. A capsule whose top sat at the jaw line (the previous revision) filled
     *     the whole mouth, so the cavity carve, the teeth and the tongue were all buried inside
     *     solid material and every one of those shapes silently produced no surface at all.
     * Capping the tube 10 mm below the chin satisfies both: it still crosses the 1.45 m plane, and
     * it stays clear of the cavity, which bottoms out 20 mm above the chin.
     */
    /* The same tube as the trunk's (see torso()): the two must be identical for the union across
       the neck to be seamless. Its front is now well behind the mouth, so it no longer has to stop
       short of the chin to keep out of the oral cavity. */
    const Vec3 neckBase(0.0f, m_.y.neckBaseY - 0.045f * m_.height, m_.neck.centreZ);
    const Vec3 neckTop(0.0f, m_.y.jawY + 0.010f * m_.height, m_.neck.centreZ + 0.016f);
    d = smin(d, sdTaperedCapsule(p, neckBase, neckTop, m_.neck.rx * 1.08f, m_.neck.rx * 0.88f), 0.012f);
    return d;
}

float BodyField::hand(const Vec3& p, int side) const {
    const Chain& c = chain(side);
    /* Everything of the hand lies within one hand length of the palm centre. */
    const float away = (p - c.palmCentre).length() - m_.handLength;
    if (away > 0.04f) return away;
    return handShape(p, c);
}

float BodyField::foot(const Vec3& p, int side) const {
    const Chain& c = chain(side);
    /* Everything of the foot lies within one foot length of the midfoot. */
    const float away = (p - (c.ankle + c.ball) * 0.5f).length() - m_.footLength * 0.80f;
    if (away > 0.04f) return away;
    return footShape(p, c);
}

/* =======================================================================================
 * Region classification
 * ======================================================================================= */
/* ==========================================================================================
 * Apparel.
 *
 * One rule generates every garment in the catalogue: a garment is the body's own distance field
 * inflated by a thickness and intersected with a domain. That is what lets the same code dress a
 * 1.62 m heavy build and a 1.86 m slim one identically well - the cloth follows the body because it
 * IS the body, offset.
 *
 * Three consequences worth stating, because they are what make this tractable:
 *   - a dressed avatar's skin under the cloth simply is not in the mesh (the inflated body
 *     contains the body, so the torso's surface is interior and the extractor never sees it),
 *     which is both correct and cheaper than modelling clothes over a full body;
 *   - the collar, hems, armholes and cuffs are the DOMAIN's boundaries, so a garment's outline is
 *     an intersection curve rather than modelled trim - crisp, and automatically correct;
 *   - the shells are solids, not membranes. A 4 mm membrane is thinner than the extraction lattice
 *     and would be sampled the way the mouth aperture was; a solid is robust at any lattice size.
 * ========================================================================================== */

float BodyField::armsCut(const Vec3& p, float thickness) const {
    /* The arms, inflated. Subtracted from a garment's band so a sleeveless garment has real
       armholes: the band covers the body's silhouette, and this punches the arms out of it. */
    const float t = thickness + 0.004f;
    return minf(armChain(p, left_) - t, armChain(p, right_) - t);
}

float BodyField::sleeveSolid(const Vec3& p, float thickness, float reach) const {
    /*
     * A sleeve: the inflated arm, limited to `reach` metres along the arm from the shoulder. The
     * limit is a sphere around the shoulder joint rather than a cut perpendicular to the arm's
     * axis, which is deliberate - a sleeve end is roughly perpendicular to the arm whatever the
     * pose, and a sphere keeps the boundary smooth and cheap. reach = 0 gives a shoulder strap.
     */
    float d = 1e9f;
    for (int side = -1; side <= 1; side += 2) {
        const Chain& c = chain(side);
        const float tube = armChain(p, chain(side)) - thickness;
        /* Clip the sleeve perpendicular to the upper arm. A sphere around the shoulder
           creates a large rounded pauldron at the sleeve root, even for a plain T-shirt.
           The axial plane keeps the sleeve attached while removing that bulbous cap. */
        const Vec3 armAxis = (c.elbow - c.shoulder).normalizedOr({0.0f, -1.0f, 0.0f});
        const float limit = (p - c.shoulder).dot(armAxis) - reach;
        d = minf(d, smax(tube, limit, 0.006f));
    }
    return d;
}

float BodyField::frontOpening(const Vec3& p) const {
    /*
     * The opening down the front of a garment that is worn open or has lapels: negative inside it,
     * where the layer underneath shows. A jacket is not a pullover in a darker colour - the shirt
     * and tie in the V of a suit, the T-shirt between the fronts of a denim jacket, are what the
     * eye reads the garment by.
     */
    const int o = apparel_.outfit;
    if (o != 3 && o != 4 && o != 9) return 1e9f;
    const float H = m_.height;
    /* The front of the body only. */
    const float front = p.z - (m_.chest.centreZ - m_.chest.rz * 0.20f);
    const float ax = std::fabs(p.x);
    if (o == 4) return maxf(ax - 0.040f, front);                 /* worn open, collar to hem */
    const float topY = m_.y.neckBaseY + 0.012f * H;
    const float apexY = (o == 3) ? lerpf(m_.y.waistY, m_.y.underbustY, 0.55f)
                                 : lerpf(m_.y.waistY, m_.y.underbustY, 0.95f);
    const float halfTop = m_.neck.rx * 1.08f + ((o == 3) ? 0.018f : 0.012f);
    const float w = halfTop * saturate((p.y - apexY) / maxf(1e-3f, topY - apexY));
    return maxf(maxf(ax - w, apexY - p.y), front);
}

float BodyField::upperShell(const Vec3& p, float thickness, float hemY, float topY, float neckDrop,
                            float sleeveReach) const {
    const float H = m_.height;
    /*
     * The band: everything between the hem and the collar. It used to be a super-elliptical
     * column fitted round the trunk's profile as well, whose job was to keep the arms out of the
     * garment. The garment is now an offset of the trunk alone (see below), so there is nothing to
     * keep out - and the column was tighter than the body at its corners, where it cut holes in the
     * cloth over the hip bones and the bust.
     */
    float domain = maxf(hemY - p.y, p.y - topY);

    /* Neckline: an opening around the neck, deeper at the front than at the back. */
    {
        const Vec3 nc(0.0f, m_.y.neckBaseY + 0.020f * H, m_.neck.centreZ - neckDrop * 0.55f);
        const float hole = sdEllipsoid(p - nc, Vec3(m_.neck.rx * 1.22f + neckDrop * 0.55f,
                                                   0.024f * H + neckDrop,
                                                   m_.neck.rz * 1.30f + neckDrop * 0.75f));
        domain = smax(domain, -hole, 0.006f);
    }

    /*
     * The garment is the body's own surface, moved outwards by the cloth's thickness and kept only
     * where the garment is. "Where the garment is" has two parts with different limits: on the
     * trunk, between the hem and the neckline; on each arm, as far as the sleeve reaches. A point
     * belongs to the trunk or to an arm according to which of the two it is nearer, and the two
     * parts overlap by a centimetre so that they are one piece of cloth.
     *
     * The body's surface here includes the fillet that joins arm and trunk at the armpit. Two
     * earlier constructions got that wrong in opposite ways. The first offset the whole body and
     * cut the arms out of the trunk's band, which removed the trunk's cloth wherever a hand hung
     * near it (holes at the hips). The second offset the trunk and each sleeve separately and
     * joined them with the armpit fillet - but below the end of a short sleeve there is no sleeve
     * to join, so the fillet of skin between arm and trunk stood out through the cloth behind
     * each armpit.
     */
    const float trunk = trunkAndLegs(p);
    float shell = 1e9f;
    for (int side = -1; side <= 1; side += 2) {
        const Chain& c = chain(side);
        const float arm = armChain(p, c);
        const float surface = smin(trunk, arm, armFillet(p, c));
        /* Positive on the arm's side of the surface halfway between arm and trunk. */
        const float towardsArm = (trunk - arm) * 0.5f;
        const Vec3 armAxis = (c.elbow - c.shoulder).normalizedOr({0.0f, -1.0f, 0.0f});
        const float sleeveEnd = (p - c.shoulder).dot(armAxis) - sleeveReach;
        /* Inside the sleeve the two parts overlap by a centimetre. Past its end there is no sleeve
           for the trunk's cloth to join, and there it must stop short of the halfway surface, or
           it wraps the inside of the bare arm wherever the arm hangs near the body. */
        const float overlap = lerpf(0.010f, -0.003f, smoothstepf(0.0f, 0.015f, sleeveEnd));
        const float onTrunk = smax(domain, towardsArm - overlap, 0.004f);
        const float onSleeve = smax(sleeveEnd, -towardsArm - 0.010f, 0.004f);
        /* Feathered at every edge - collar, hem, sleeve end. See layerOffset(). */
        shell = minf(shell, surface - layerOffset(minf(onTrunk, onSleeve), thickness,
                                                  clampf(thickness * 4.0f, 0.016f, 0.040f)));
    }
    return shell;
}

float BodyField::skirtShell(const Vec3& p, float topY, float hemY, float flare, float clearance) const {
    /*
     * A skirt: the space inside a column that is fitted at the waist, clears the hips and widens
     * to the hem - and outside the body. It has to be a volume of its own: an offset of the body
     * would wrap each leg separately.
     *
     * The column follows the trunk's own profile down to the hips, so there is no height at which
     * the body is wider than the skirt. (The first version started from a ring 16 % wider than the
     * waist; the hips and the seat stood out through it, a ledge all round at hip height.)
     */
    if (p.y > topY + 0.03f) return p.y - topY;
    if (p.y < hemY - 0.03f) return hemY - p.y;
    const float hipsY = lerpf(m_.y.crotchY, m_.y.hipY, 0.35f);
    const float yy = clampf(p.y, hipsY, topY);
    /* The skirt comes out from under the bodice: no clearance at its top edge, all of it 7 cm
       lower. At full clearance from the top it stood off the bodice as a ledge round the waist. */
    clearance *= smoothstepf(topY, topY - 0.07f, p.y);
    float rx = torsoProfileRx(yy) + clearance;
    float rz = torsoProfileRz(yy) + clearance + m_.gluteBulge * saturate((topY - yy) / maxf(1e-3f, topY - hipsY));
    const float cz = torsoProfileCz(yy);
    /* Never narrower than it was higher up: a skirt hangs from its widest point. */
    {
        const float wideX = maxf(m_.hip.rx, m_.waist.rx) + clearance;
        const float wideZ = maxf(m_.hip.rz, m_.waist.rz) + clearance + m_.gluteBulge;
        const float t = smoothstepf(topY, hipsY, p.y);
        rx = maxf(rx, lerpf(rx, wideX, t));
        rz = maxf(rz, lerpf(rz, wideZ, t));
    }
    /* The flare begins gently: a straight widening from the hips left a crease all round them. */
    const float below = std::pow(saturate((hipsY - p.y) / maxf(1e-3f, hipsY - hemY)), 1.45f);
    const float widen = 1.0f + (flare - 1.0f) * below;
    rx *= widen; rz *= widen;
    const float nx = p.x / rx, nz = (p.z - cz) / rz;
    const float e = 2.45f;
    const float k = std::pow(std::pow(std::fabs(nx), e) + std::pow(std::fabs(nz), e), 1.0f / e);
    const float column = maxf((k - 1.0f) * minf(rx, rz), maxf(hemY - p.y, p.y - topY));
    return smax(column, -body(p), 0.004f);
}

float BodyField::clothUpper(const Vec3& p) const {
    if (apparel_.outfit == 0) return 1e9f;
    const float H = m_.height;
    const float layer = minLayer();
    float thickness = 0.005f;        /* T-shirt                        */
    float hemY = m_.y.hipY - 0.02f * H;
    /* The garment reaches the base of the neck and the neckline is cut out of it. */
    const float topY = m_.y.neckBaseY + 0.012f * H;
    float neckDrop = 0.016f * H;     /* how far the front of the neckline dips below the neck base */
    float sleeveReach = 0.0f;
    float skirtHemY = 0.0f;          /* > 0: the garment has a skirt down to here */
    float skirtFlare = 1.0f;
    const float wristReach = m_.upperArmLen + m_.forearmLen;
    switch (apparel_.outfit) {
        case 1:  /* T-shirt   */ thickness = 0.005f; sleeveReach = m_.upperArmLen * 0.35f; break;
        case 2:  /* Full-length dress: fitted to mid-thigh, so whatever the skirt does not cover
                    is still the dress. */
                 thickness = 0.004f; hemY = m_.y.crotchY - m_.thighLen * 0.25f;
                 neckDrop = 0.050f * H; sleeveReach = 0.0f;
                 skirtHemY = m_.y.soleY + 0.010f * H; skirtFlare = 2.35f; break;
        case 3:  /* Suit jacket: covers the seat, sleeves to the wrist. */
                 thickness = maxf(0.014f, layer + 0.006f); sleeveReach = wristReach * 0.955f;
                 neckDrop = 0.010f * H; hemY = m_.y.crotchY + 0.012f * H; break;
        case 4:  /* Denim jacket: ends at the top of the hips. */
                 thickness = maxf(0.016f, layer + 0.007f); sleeveReach = wristReach * 0.945f;
                 neckDrop = 0.014f * H; hemY = lerpf(m_.y.hipY, m_.y.waistY, 0.45f); break;
        case 5:  /* Arming doublet under plate: close, long sleeves. */
                 thickness = 0.006f; sleeveReach = wristReach * 0.95f; neckDrop = 0.006f * H;
                 hemY = m_.y.hipY - 0.01f * H; break;
        case 6:  /* Tunic under leather armour. */
                 thickness = 0.007f; sleeveReach = wristReach * 0.93f; neckDrop = 0.020f * H;
                 hemY = m_.y.crotchY + 0.01f * H; break;
        case 7:  /* Spacesuit: one thick layer over everything to the wrist. */
                 thickness = maxf(0.030f, layer + 0.010f); sleeveReach = wristReach * 0.985f;
                 neckDrop = 0.004f * H; hemY = m_.y.crotchY + 0.01f * H; break;
        case 8:  /* Athletic  */ thickness = 0.0035f; sleeveReach = m_.upperArmLen * 0.22f;
                                 hemY = m_.y.hipY - 0.03f * H; break;
        case 9:  /* Coat: to the knee. */
                 thickness = maxf(0.018f, layer + 0.008f); sleeveReach = wristReach * 0.965f;
                 neckDrop = 0.012f * H; hemY = m_.y.crotchY - m_.thighLen * 0.20f;
                 skirtHemY = m_.y.kneeY - 0.015f * H; skirtFlare = 1.10f; break;
        default: return 1e9f;
    }
    thickness = maxf(thickness, layer);
    /* Well above the collar or below the hem there is no garment: a conservative distance. */
    {
        const float lowest = (skirtHemY > 0.0f) ? minf(hemY, skirtHemY) : hemY;
        if (p.y < lowest - 0.05f) return lowest - p.y - 0.01f;
        if (p.y > topY + 0.05f) return p.y - topY - 0.01f;
    }
    /* In the opening of a jacket the layer underneath shows: thinner, so the fronts stand proud. */
    {
        const float opening = frontOpening(p);
        if (opening < 0.02f) thickness = lerpf(layer, thickness, smoothstepf(0.0f, 0.008f, opening));
    }
    float d = upperShell(p, thickness, hemY, topY, neckDrop, sleeveReach);
    if (skirtHemY > 0.0f) {
        const bool coat = (apparel_.outfit == 9);
        d = minf(d, skirtShell(p, m_.y.waistY, skirtHemY, skirtFlare, coat ? thickness + 0.006f : 0.012f));
    }
    return d;
}

float BodyField::clothLower(const Vec3& p) const {
    if (apparel_.outfit == 0) return 1e9f;
    const float H = m_.height;
    float thickness = 0.0f;
    float hemY = 0.0f;
    switch (apparel_.outfit) {
        case 1:  thickness = 0.010f; hemY = m_.y.ankleY + 0.030f; break;             /* T-shirt: with jeans */
        case 2:  thickness = 0.004f; hemY = m_.y.soleY + 0.010f * H; break; /* full-length dress lining */
        case 3:  thickness = 0.010f; hemY = m_.y.ankleY + 0.035f; break;             /* trousers      */
        case 4:  thickness = 0.012f; hemY = m_.y.ankleY + 0.030f; break;             /* jeans         */
        case 5:  thickness = 0.006f; hemY = m_.y.ankleY + 0.020f; break;             /* hose          */
        case 6:  thickness = 0.009f; hemY = m_.y.ankleY + 0.030f; break;             /* breeches      */
        case 7:  thickness = maxf(0.028f, minLayer() + 0.010f); hemY = m_.y.ankleY + 0.040f; break;
        case 8:  thickness = 0.0035f; hemY = m_.y.kneeY + m_.shankLen * 0.18f; break; /* shorts      */
        case 9:  thickness = 0.010f; hemY = m_.y.ankleY + 0.035f; break;
        default: return 1e9f;
    }
    thickness = maxf(thickness, minLayer());
    const float topY = m_.y.waistY + 0.030f;
    if (p.y > topY + 0.05f) return p.y - topY - 0.01f;
    if (p.y < hemY - 0.05f) return hemY - p.y - 0.01f;

    /* From the hem up past the waist. The gap between the legs is air in the body's field, so the
       crotch forms by itself. */
    const float domain = maxf(hemY - p.y, p.y - topY);
    /* Trousers are an offset of the trunk and the legs. The arms are not part of that field, so a
       hand hanging beside the thigh no longer bites a hole in the cloth. */
    (void)H;
    /* Feathered at the waist and at each hem. See layerOffset(). */
    return trunkAndLegs(p) - layerOffset(domain, thickness, clampf(thickness * 4.0f, 0.016f, 0.040f));
}

Vec3 BodyField::footLocalRaw(const Vec3& p, const Chain& c) const {
    /* The foot's own frame, as footShape() builds it: origin at the ankle, toe-out removed,
       forward = -Z, lateral = +side in X. */
    const float yaw = radians(7.0f) * static_cast<float>(c.side);
    const float cs = std::cos(-yaw), sn = std::sin(-yaw);
    const Vec3 r = p - c.ankle;
    return Vec3(r.x * cs - r.z * sn, r.y, r.x * sn + r.z * cs);
}

Vec3 BodyField::footLocal(const Vec3& p, const Chain& c) const {
    /*
     * Where p is on the flat foot. Without heels that is the foot's own frame. In heels the foot
     * is pitched about the ankle and its toes are bent back to lie on the floor, so the pitch is
     * undone here: behind the ball by turning the point back, in front of it by sliding it back,
     * with the two blended across the plane that bisects the bend. Everything that shapes or
     * paints a foot or a shoe goes through this, which is why a shoe pitches with its foot.
     */
    const Vec3 q = footLocalRaw(p, c);
    if (m_.heelLift <= 0.0f) return q;
    const float turnedY = q.y * heelCos_ - q.z * heelSin_;
    const float turnedZ = q.y * heelSin_ + q.z * heelCos_;
    const float slidY = q.y - heelShiftY_;
    const float slidZ = q.z - heelShiftZ_;
    const float across = (q.y - heelPivotY_) * heelBisY_ + (q.z - heelPivotZ_) * heelBisZ_;
    const float w = smoothstepf(-0.014f, 0.014f, across);
    return Vec3(q.x, lerpf(turnedY, slidY, w), lerpf(turnedZ, slidZ, w));
}

Vec3 BodyField::footPosed(const Vec3& l, const Chain& c) const {
    /* The other way, for landmarks and joints: a point of the flat foot, in the world. */
    Vec3 q = l;
    if (m_.heelLift > 0.0f) {
        if (l.z < c.ballL.z) {
            q.y = l.y + heelShiftY_;
            q.z = l.z + heelShiftZ_;
        } else {
            q.y = l.y * heelCos_ + l.z * heelSin_;
            q.z = -l.y * heelSin_ + l.z * heelCos_;
        }
    }
    const float yaw = radians(7.0f) * static_cast<float>(c.side);
    const float cs = std::cos(yaw), sn = std::sin(yaw);
    return c.ankle + Vec3(q.x * cs - q.z * sn, q.y, q.x * sn + q.z * cs);
}

int BodyField::footwearKind() const {
    /* A spacesuit comes with its boots: bare feet under a pressure suit are not a spacesuit. */
    if (apparel_.footwear == 0 && apparel_.outfit == 7) return 5;
    return apparel_.footwear;
}

float BodyField::shoeTopY() const {
    switch (footwearKind()) {
        case 1: return m_.y.ankleY + 0.024f;                    /* sneaker: below the ankle bone */
        case 2: return m_.y.ankleY + m_.shankLen * 0.52f;       /* boot: mid-calf                */
        case 3: return m_.y.ankleY + m_.shankLen * 0.34f;       /* sabaton: over the ankle       */
        case 4: return m_.y.ankleY - 0.006f;                    /* court shoe: under the ankle   */
        case 5: return m_.y.ankleY + m_.shankLen * 0.40f;       /* suit boot                     */
        default: return 0.0f;
    }
}

float BodyField::shoeShape(const Vec3& p) const {
    const int kind = footwearKind();
    if (kind < 1 || kind > 5) return 1e9f;
    const float L = m_.footLength, W = m_.footWidth, Hf = m_.footHeight;
    float wall = 0.008f;        /* how far the upper stands off the foot                      */
    float shaft = 0.0f;         /* > 0: a shaft round the leg, standing this far off the leg  */
    float soleStep = 0.0f;      /* > 0: the sole stands this far proud of the upper           */
    float soleTop = 0.0f;       /* height of the sole's top edge above the ground             */
    switch (kind) {
        case 1: wall = 0.010f; soleStep = 0.006f; soleTop = 0.026f; break;
        case 2: wall = 0.010f; shaft = 0.012f; soleStep = 0.004f; soleTop = 0.018f; break;
        case 3: wall = 0.012f; shaft = 0.015f; break;
        case 4: wall = 0.005f; break;
        case 5: wall = 0.020f; shaft = 0.030f; soleStep = 0.006f; soleTop = 0.030f; break;
        default: break;
    }
    wall = maxf(wall, minLayer() * 0.75f);
    const float topY = shoeTopY();
    if (p.y > topY + 0.06f) return p.y - topY - 0.02f;

    float d = 1e9f;
    for (int side = -1; side <= 1; side += 2) {
        const Chain& c = chain(side);
        const float sx = static_cast<float>(side);
        /* Everything of a shoe lies within a foot length of the midfoot, or along its shaft. */
        if (shaft <= 0.0f) {
            const float away = (p - (c.ankle + c.ball) * 0.5f).length() - (L * 0.80f + 0.05f);
            if (away > 0.04f) { d = minf(d, away); continue; }
        } else if (std::fabs(p.x - c.ankle.x) > W + 0.12f) {
            d = minf(d, std::fabs(p.x - c.ankle.x) - W - 0.08f);
            continue;
        }
        const Vec3 q = footLocal(p, c);
        const float ground = footGround(c) + 0.002f;
        const float ballZ = c.ballL.z;

        /*
         * The last: the form a shoe is built on. It is the foot, inflated, with one rounded box
         * over all five toes. The shoe used to be the inflated foot alone, which kept every toe
         * as a separate bump: a sock with a sole, the same for every kind of footwear.
         */
        float last = footShape(p, c) - wall;
        {
            const Vec3 boxC(sx * W * 0.02f, ground + Hf * 0.30f, ballZ - L * 0.125f);
            const float toeBox = sdEllipsoid(q - boxC, Vec3(W * 0.52f + wall, Hf * 0.36f + wall, L * 0.205f + wall));
            last = smin(last, toeBox, 0.020f);
        }
        if (kind == 3) {
            /* Sabaton: a pointed toe, and the lames of the instep as shallow steps. */
            const Vec3 a(sx * W * 0.02f, ground + Hf * 0.26f, ballZ - L * 0.06f);
            const Vec3 b(sx * W * 0.02f, ground + 0.014f, ballZ - L * 0.44f);
            last = smin(last, sdTaperedCapsule(q, a, b, W * 0.42f, 0.011f), 0.016f);
            last -= 0.0025f * (0.5f + 0.5f * std::cos(q.z * (6.2831853f / 0.030f)));
        }
        /* The wrap round the ankle: the opening is a ring round the leg, not a hole in the top of
           the foot. The clip at topY cuts it into the collar. */
        const float wrap = (q - Vec3(0.0f, -Hf * 0.30f, 0.004f)).length() - (m_.ankleRx + wall + 0.008f);
        float solid = smin(last, wrap, 0.008f);
        if (kind == 1) {
            /* Sneaker: a padded tongue up the instep. */
            const Vec3 a(0.0f, ground + Hf * 0.78f, -L * 0.15f);
            const Vec3 b(0.0f, 0.026f, -(m_.ankleRx + 0.012f));
            solid = smin(solid, sdCapsule(q, a, b, 0.017f), 0.012f);
        }
        if (shaft > 0.0f) {
            /* A shaft: the leg, inflated, from the ankle to the top. */
            const float leg = legChain(p, c) - shaft;
            solid = smin(solid, smax(leg, (c.ankle.y - 0.030f) - p.y, 0.010f), 0.014f);
        }
        if (soleStep > 0.0f) {
            /* The sole: the lowest part of the shoe, a few millimetres proud of the upper. */
            solid = minf(solid, smax(solid - soleStep, q.y - (ground + soleTop), 0.003f));
        }
        /*
         * The shoe has to stand ON the floor. The foot's sole is already at the field's ground
         * level, so inflating it would push the sole through y = 0 and the avatar would sink into
         * its own floor plane. Clipping at y = 0 flattens the sole exactly where the ground is.
         */
        const bool raised = (kind == 4 && m_.heelLift > 0.0f);
        if (raised) {
            /*
             * A raised heel. The shoe is pitched with the foot (footLocal), so its sole is flattened
             * in the foot's frame, and the heel itself is a tapered block from under the heel bone
             * straight down to the floor.
             */
            solid = smax(solid, (ground - 0.006f) - q.y, 0.003f);
            const Vec3 l = footLocalRaw(p, c);
            const float gs = ground - 0.002f;
            const float seatY = gs * heelCos_ + c.heelL.z * heelSin_;
            const float seatZ = -gs * heelSin_ + c.heelL.z * heelCos_;
            const float floorY = m_.y.soleY - c.ankle.y;
            const Vec3 a(c.heelL.x, seatY + 0.012f, seatZ + 0.006f);
            const Vec3 b(c.heelL.x, floorY + 0.012f, seatZ - 0.002f);
            solid = smin(solid, sdTaperedCapsule(l, a, b, 0.022f, 0.0125f), 0.006f);
        }
        solid = smax(solid, -p.y, 0.0025f);
        if (kind == 4) {
            /* Court shoe: cut low over the instep, so the top of the foot shows. */
            const Vec3 cutC(0.0f, ground + Hf * 1.02f, -L * 0.16f);
            const float throat = sdEllipsoid(q - cutC, Vec3(W * 0.30f, Hf * 0.62f, L * 0.20f));
            solid = smax(solid, -throat, 0.004f);
        }
        /* The opening: level with the floor, or with the foot's own sole when it is pitched. */
        solid = smax(solid, raised ? (q.y - (topY - c.ankle.y)) : (p.y - topY), (shaft > 0.0f) ? 0.004f : 0.006f);
        d = minf(d, solid);
    }
    return d;
}

/* The pieces of a kit, so that the texture bake can tell a breastplate from a belt. */
enum GearPiece {
    GEAR_NONE = 0, GEAR_CUIRASS, GEAR_PAULDRON, GEAR_ARM, GEAR_COUTER, GEAR_FAULD, GEAR_TASSET,
    GEAR_CUISSE, GEAR_POLEYN, GEAR_GREAVE, GEAR_BELT, GEAR_SKIRT, GEAR_BRACER, GEAR_NECKRING,
    GEAR_CHESTBOX, GEAR_PACK, GEAR_GLOVE
};

float BodyField::gearShape(const Vec3& p) const { return gearField(p, nullptr); }

float BodyField::gearField(const Vec3& p, int* pieceOut) const {
    /*
     * Equipment: the parts of a kit that are not cloth. Plate armour is a harness - breastplate,
     * fauld and tassets, pauldrons, arm and leg defences - worn over dark arming clothes. Leather
     * armour is a jerkin with a belt and a skirt of straps, shoulder caps and bracers, over a
     * tunic. A spacesuit's equipment is its neck ring, chest unit, life-support pack and gloves.
     *
     * (Each of the three used to be the same four pieces - two shoulder domes, a chest disc and a
     * belt ring - on what was otherwise a shirt and trousers.)
     */
    if (pieceOut) *pieceOut = GEAR_NONE;
    const int o = apparel_.outfit;
    if (o != 5 && o != 6 && o != 7) return 1e9f;
    const float H = m_.height;
    const float layer = minLayer();
    const bool plate = (o == 5), leather = (o == 6), suit = (o == 7);
    float d = 1e9f;
    int piece = GEAR_NONE;
    const auto take = [&](float v, int id) { if (v < d) { d = v; piece = id; } };
    const float neckTop = m_.y.neckBaseY + 0.012f * H;
    const float plateT = maxf(plate ? 0.020f : 0.014f, layer + 0.006f);   /* over the trunk */
    const float limbT = maxf(plate ? 0.015f : 0.011f, layer + 0.005f);    /* over arms, legs */

    if (plate || leather) {
        const float skirtHem = m_.y.crotchY - m_.thighLen * 0.34f;
        if (p.y > (plate ? m_.y.crotchY : skirtHem) - 0.06f && p.y < neckTop + 0.08f) {
            /* Breastplate and backplate, or the jerkin: the trunk's surface moved outwards,
               without sleeves. */
            const float hem = plate ? (m_.y.waistY - 0.012f * H) : (m_.y.hipY + 0.012f * H);
            take(upperShell(p, plateT, hem, neckTop, plate ? 0.010f * H : 0.026f * H, 0.0f), GEAR_CUIRASS);
            if (plate) {
                /* Fauld: the hoops below the breastplate, over the hips. */
                const float dom = maxf((m_.y.crotchY + 0.012f * H) - p.y, p.y - (m_.y.waistY - 0.004f * H));
                take(trunkAndLegs(p) - layerOffset(dom, plateT + 0.003f, 0.020f), GEAR_FAULD);
            } else {
                /* A wide belt, and a skirt of straps to mid-thigh. */
                const float dom = std::fabs(p.y - m_.y.waistY) - 0.024f;
                take(trunkAndLegs(p) - layerOffset(dom, plateT + 0.005f, 0.012f), GEAR_BELT);
                take(skirtShell(p, m_.y.waistY - 0.010f, skirtHem, 1.08f, plateT + 0.004f), GEAR_SKIRT);
            }
        }
        for (int side = -1; side <= 1; side += 2) {
            const Chain& c = chain(side);
            const float sx = static_cast<float>(side);
            /* Pauldron: a shell over the deltoid, dropping over the upper arm. */
            {
                const float r = m_.deltoidR * (plate ? 1.36f : 1.12f) + limbT * 0.5f;
                const Vec3 pc(c.shoulder.x + sx * r * 0.22f, c.shoulder.y - r * 0.06f, c.shoulder.z);
                float pad = sdEllipsoid(p - pc, Vec3(r * 1.02f, r * (plate ? 1.00f : 0.82f), r * 1.00f));
                pad = smax(pad, (c.shoulder.y - r * (plate ? 0.72f : 0.40f)) - p.y, 0.008f);
                take(pad, GEAR_PAULDRON);
            }
            /* Arm defences. */
            if (p.y < c.shoulder.y + 0.05f && p.y > c.wrist.y - 0.06f && sx * p.x > 0.0f) {
                const float tube = armChain(p, c) - limbT;
                const Vec3 axisU = (c.elbow - c.shoulder).normalizedOr({0.0f, -1.0f, 0.0f});
                const Vec3 axisF = (c.wrist - c.elbow).normalizedOr({0.0f, -1.0f, 0.0f});
                const float u = (p - c.shoulder).dot(axisU);
                const float f = (p - c.elbow).dot(axisF);
                const float U = m_.upperArmLen, F = m_.forearmLen;
                if (plate) {
                    take(smax(tube, maxf(U * 0.44f - u, u - U * 0.86f), 0.004f), GEAR_ARM);       /* rerebrace */
                    take(smax(tube, maxf(F * 0.14f - f, f - F * 0.965f), 0.004f), GEAR_ARM);      /* vambrace  */
                    take((p - c.elbow).length() - (m_.elbowR + limbT + 0.008f), GEAR_COUTER);
                } else {
                    take(smax(tube, maxf(F * 0.32f - f, f - F * 0.955f), 0.004f), GEAR_BRACER);
                }
            }
            /* Leg defences. */
            if (plate && p.y < m_.y.crotchY + 0.02f && p.y > m_.y.ankleY - 0.02f && sx * p.x > -0.02f) {
                const float tube = legChain(p, c) - limbT;
                take(smax(tube, maxf((m_.y.kneeY + 0.075f) - p.y, p.y - (m_.y.crotchY - 0.012f)), 0.004f), GEAR_CUISSE);
                take(smax(tube, maxf((m_.y.ankleY + 0.030f) - p.y, p.y - (m_.y.kneeY - 0.065f)), 0.004f), GEAR_GREAVE);
                const float kr = m_.kneeR + limbT + 0.004f;
                take(sdEllipsoid(p - (c.knee + Vec3(0.0f, 0.004f, -m_.kneeR * 0.42f)), Vec3(kr * 0.98f, kr * 1.08f, kr * 0.84f)), GEAR_POLEYN);
            }
            if (plate) {
                /* Tassets: the plates that hang from the fauld over the front of each thigh. */
                const Vec3 hj = c.hipJoint;
                const float r = m_.thighRx * 1.20f + limbT * 0.5f;
                float t = sdEllipsoid(p - Vec3(hj.x, hj.y - m_.thighLen * 0.16f, hj.z - m_.thighRz * 0.40f),
                                      Vec3(r * 0.92f, r * 0.95f, r * 0.80f));
                t = smax(t, (hj.y - m_.thighLen * 0.40f) - p.y, 0.010f);
                take(t, GEAR_TASSET);
            }
        }
    }
    if (suit) {
        /* The neck ring the helmet locks onto. */
        {
            const float a = m_.neck.rx + 0.060f, b = m_.neck.rz + 0.064f;
            const float rho = std::sqrt((p.x / a) * (p.x / a) + ((p.z - m_.neck.centreZ) / b) * ((p.z - m_.neck.centreZ) / b));
            const float radial = (rho - 1.0f) * minf(a, b);
            const float dy = p.y - (m_.y.neckBaseY + 0.020f * H);
            take(std::sqrt(radial * radial + dy * dy) - 0.021f, GEAR_NECKRING);
        }
        /* Display and control unit on the front, life support pack on the back. */
        take(sdRoundBox(p - Vec3(0.0f, suitBoxY_, suitFrontZ_ - 0.016f), Vec3(0.088f, 0.058f, 0.032f), 0.010f), GEAR_CHESTBOX);
        take(sdRoundBox(p - Vec3(0.0f, m_.y.bustY + 0.004f * H, suitBackZ_ + 0.056f), Vec3(0.150f, 0.215f, 0.060f), 0.028f), GEAR_PACK);
        /* Gloves. */
        for (int side = -1; side <= 1; side += 2) {
            take(hand(p, side) - maxf(0.006f, layer * 0.75f), GEAR_GLOVE);
        }
    }
    if (pieceOut) *pieceOut = piece;
    return d;
}

static const float kBraidLength = 0.46f;   /* from the crown to the end of a plait */

void BodyField::setApparel(const ApparelOptions& a) {
    apparel_ = a;
    braidBeads_.clear();
    if (apparel_.outfit == 7) {
        /*
         * Where the suit's front and back are, at the heights of its two boxes: found once by
         * walking in from in front of and behind the body to the suit's surface. A box placed
         * from the chest's nominal depth disappeared into a full bust.
         */
        suitBoxY_ = lerpf(m_.y.underbustY, m_.y.waistY, 0.30f);
        const auto walk = [&](float y, float from, float dir) {
            float z = from;
            for (int i = 0; i < 64; ++i) {
                const float f = minf(skinUnion(Vec3(0.0f, y, z)), clothUpper(Vec3(0.0f, y, z)));
                if (f < 0.0005f) break;
                z += dir * f;
            }
            return z;
        };
        suitFrontZ_ = walk(suitBoxY_, -0.60f, 1.0f);
        suitBackZ_ = walk(m_.y.bustY, 0.60f, -1.0f);
    }
    if (apparel_.hair != 7) return;
    /*
     * The plaits are laid out once, here, because each bead has to rest ON whatever is under it -
     * neck, collarbone, chest, a jacket - and finding that surface for every sample of the field
     * would cost a body evaluation per bead per sample.
     *
     * A plait starts behind the ear lobe, comes forward over the shoulder and hangs down the front
     * of the chest. Its beads alternate left and right of the path, which is what a three-strand
     * plait looks like from outside.
     */
    const float headH = m_.y.crownY - m_.y.chinY;
    const float crown = m_.y.crownY;
    const float zc = m_.head.centreZ;
    const float endY = crown - kBraidLength;
    const auto dressed = [&](const Vec3& q) { return minf(skinUnion(q), minf(clothUpper(q), gearShape(q))); };
    for (int side = -1; side <= 1; side += 2) {
        const float sx = static_cast<float>(side);
        const Vec3 start(sx * m_.head.rx * 0.86f, earCentre_[0].y - m_.earLength * 0.62f, zc + m_.head.rz * 0.18f);
        const Vec3 over(sx * (m_.neck.rx + 0.045f), m_.y.neckBaseY + 0.010f, m_.neck.centreZ - m_.neck.rz * 0.55f);
        const Vec3 down(sx * (m_.neck.rx + 0.050f), endY, m_.chest.centreZ - m_.chest.rz * 0.90f);
        const float len1 = (over - start).length(), len2 = (down - over).length();
        const float total = len1 + len2;
        float along = 0.0f;
        int index = 0;
        while (along < total) {
            const float t = along / total;
            const float r = lerpf(0.020f, 0.0145f, t);
            Vec3 c = (along < len1) ? start.lerp(over, along / len1) : over.lerp(down, (along - len1) / len2);
            /* Rest the bead on the surface: move it out along the surface normal until it sits
               nine tenths of its radius clear. */
            for (int it = 0; it < 6; ++it) {
                const float f = dressed(c);
                if (f >= r * 0.85f) break;
                const float e = 0.004f;
                Vec3 n(dressed(c + Vec3(e, 0, 0)) - dressed(c - Vec3(e, 0, 0)),
                       dressed(c + Vec3(0, e, 0)) - dressed(c - Vec3(0, e, 0)),
                       dressed(c + Vec3(0, 0, e)) - dressed(c - Vec3(0, 0, e)));
                n.y = 0.0f;                                   /* keep the bead at its height */
                n = n.normalizedOr({0.0f, 0.0f, -1.0f});
                c = c + n * (r * 0.85f - f + 0.001f);
            }
            const float zig = ((index & 1) ? 1.0f : -1.0f) * r * 0.36f;
            BraidBead bead;
            bead.centre = c + Vec3(zig, 0.0f, 0.0f);
            bead.radii = Vec3(r * 0.92f, r * 0.98f, r * 0.80f);
            braidBeads_.push_back(bead);
            along += r * 1.05f;
            ++index;
        }
        /* The end of the plait: a tie and a short brush below it. */
        if (!braidBeads_.empty()) {
            const Vec3 last = braidBeads_.back().centre;
            BraidBead tuft;
            tuft.centre = Vec3(last.x, last.y - 0.030f, last.z);
            tuft.radii = Vec3(0.012f, 0.026f, 0.011f);
            braidBeads_.push_back(tuft);
        }
    }
}

float BodyField::hairShape(const Vec3& p) const {
    if (apparel_.hair == 0) return 1e9f;
    const float headH = m_.y.crownY - m_.y.chinY;
    const float crown = m_.y.crownY;
    float thickness = 0.008f;   /* depth of the layer lying on the scalp                    */
    float puff = 0.0f;          /* how far the outer silhouette stands off the skull        */
    float hang = 0.0f;          /* > 0: the mass falls to this height (world Y)             */
    float backFall = 0.0f;      /* > 0: a sheet continues down the back to this height      */
    float crest = 0.0f;         /* mohawk: restrict to a midline strip                      */
    float bun = 0.0f, tail = 0.0f, braids = 0.0f, afro = 0.0f;
    switch (apparel_.hair) {
        case 1: thickness = 0.004f; break;                                        /* buzz     */
        case 2: thickness = 0.010f; puff = 0.006f; break;                         /* short    */
        case 3: thickness = 0.012f; puff = 0.016f;                                /* medium   */
                hang = m_.y.neckBaseY + 0.014f * m_.height; break;
        case 4: thickness = 0.012f; puff = 0.018f;                                /* long     */
                hang = m_.y.neckBaseY + 0.006f * m_.height;
                backFall = crown - headH * 2.05f; break;
        case 5: thickness = 0.011f; puff = 0.004f; tail = 0.30f; break;           /* ponytail */
        case 6: thickness = 0.011f; puff = 0.004f; bun = 0.055f; break;           /* bun      */
        case 7: thickness = 0.010f; braids = kBraidLength; break;                 /* braids   */
        case 8: thickness = 0.012f; afro = 0.066f; break;                         /* afro     */
        case 9: thickness = 0.018f; puff = 0.002f; crest = 0.030f; break;         /* mohawk   */
        default: return 1e9f;
    }
    thickness = maxf(thickness, minLayer() * 0.75f);   /* hair may be a little thinner than cloth */

    /* Far from the head there is no hair. Return a conservative distance instead of evaluating
       the whole head field: this function is sampled at every lattice point of the body. */
    {
        float lowest = m_.y.chinY - headH * 0.15f;
        if (hang > 0.0f) lowest = minf(lowest, hang);
        if (backFall > 0.0f) lowest = minf(lowest, backFall);
        if (tail > 0.0f) lowest = minf(lowest, crown - tail - 0.05f);
        if (braids > 0.0f) lowest = minf(lowest, crown - braids - 0.05f);
        if (p.y < lowest - 0.04f) return lowest - p.y;
        if (p.y > crown + 0.16f) return p.y - (crown + 0.12f);
    }

    /*
     * The hairline, as a height that depends on the direction around the head.
     *
     * phi is the angle about the vertical axis through the skull: 0 at the nose, 90 degrees at
     * the ear, 180 at the back. A real hairline is not a plane: it crosses the forehead, drops
     * at the temple, dips in front of the ear, clears the ear, and falls to the nape behind it.
     * The previous revision used one tilted plane whose slope had the wrong sign: it kept the
     * hair in FRONT of the brow and removed it from the back of the skull, which is the visor
     * over the eyes and the bare occiput seen in every capture.
     */
    const float zc = m_.head.centreZ;
    const float phi = degrees(std::atan2(std::fabs(p.x), -(p.z - zc)));
    const float foreheadY = browRidge_.y + headH * 0.235f;
    const float templeY = browRidge_.y + headH * 0.125f;
    const float sideburnY = earCentre_[0].y - m_.earLength * 0.05f;
    const float overEarY = earCentre_[0].y + m_.earLength * 0.52f;
    const float behindEarY = earCentre_[0].y - m_.earLength * 0.40f;
    const float napeY = m_.y.chinY + headH * 0.10f;
    const auto hairlineAt = [&](float a) {
        if (a < 40.0f) return lerpf(foreheadY, templeY, smoothstepf(14.0f, 40.0f, a));
        if (a < 55.0f) return lerpf(templeY, sideburnY, smoothstepf(40.0f, 55.0f, a));
        if (a < 66.0f) return lerpf(sideburnY, overEarY, smoothstepf(55.0f, 66.0f, a));
        if (a < 102.0f) return overEarY;
        if (a < 120.0f) return lerpf(overEarY, behindEarY, smoothstepf(102.0f, 120.0f, a));
        return lerpf(behindEarY, napeY, smoothstepf(120.0f, 180.0f, a));
    };
    /* Lower edge of the hanging mass: the hairline in front, the hang height behind the temples. */
    const auto lowerEdgeAt = [&](float a) {
        const float l = hairlineAt(a);
        return (hang > 0.0f) ? lerpf(l, hang, smoothstepf(46.0f, 70.0f, a)) : l;
    };
    /*
     * Signed distance from p to an edge given as a height for each direction round the head:
     * negative above the edge (where the hair is), positive below it.
     *
     * `height - p.y` is not that distance - where the edge climbs steeply round the head (at the
     * temple it rises six centimetres in fifteen degrees) the value changes several times faster
     * than the position does. Nor is the same value divided by the edge's slope, which an earlier
     * revision used: that is right only at the edge itself, and far above a steep stretch of it
     * the result collapses towards zero, so every steep stretch of hairline cut a groove through
     * the hair all the way to the crown.
     *
     * What is wanted is the distance to the curve in the unrolled surface of a cylinder through
     * p: arc length one way, height the other. The nearest point of the curve cannot be further
     * round than the height difference, which bounds the search.
     */
    const float radius = maxf(0.03f, std::sqrt(p.x * p.x + (p.z - zc) * (p.z - zc)));
    const auto edgeDistance = [&](const auto& height) {
        const float dy = p.y - height(phi);
        float best = dy * dy;
        const float reachDeg = minf(degrees(std::fabs(dy) / radius), 75.0f);
        if (reachDeg > 0.05f) {
            /* The curve is taken as straight between samples and the distance to each piece is
               exact, so the result is continuous. (The distance to the samples alone is not smooth:
               it terraced the hair like a contour map.) */
            const int kSteps = 8;
            float prevArc = 0.0f, prevDh = 0.0f;
            for (int i = -kSteps; i <= kSteps; ++i) {
                const float offDeg = reachDeg * static_cast<float>(i) / static_cast<float>(kSteps);
                /* The edge is the same on both sides of the head, so directions past the nose or
                   past the back fold over. */
                float aDeg = phi + offDeg;
                if (aDeg < 0.0f) aDeg = -aDeg;
                if (aDeg > 180.0f) aDeg = 360.0f - aDeg;
                const float arc = radius * radians(offDeg);
                const float dh = height(aDeg) - p.y;
                if (i > -kSteps) {
                    /* Distance from the origin to the segment (prevArc, prevDh)-(arc, dh). */
                    const float ex = arc - prevArc, ey = dh - prevDh;
                    const float len2 = ex * ex + ey * ey;
                    const float t = (len2 > 1e-12f) ? clampf(-(prevArc * ex + prevDh * ey) / len2, 0.0f, 1.0f) : 0.0f;
                    const float cx = prevArc + ex * t, cy = prevDh + ey * t;
                    best = minf(best, cx * cx + cy * cy);
                }
                prevArc = arc; prevDh = dh;
            }
        }
        return (dy > 0.0f ? -1.0f : 1.0f) * std::sqrt(best);
    };
    const float lineCut = edgeDistance(hairlineAt);

    /* The layer on the scalp: the skull inflated by the hair's depth, kept above the hairline. */
    const float skinD = head(p);
    float d = skinD - layerOffset(lineCut, thickness, 0.030f);

    /*
     * Volume. The outer silhouette is a dome over the cranium that continues downwards as an
     * elliptical tube, and the mass is the WALL of that bell: a solid bell would fill the space
     * between the neck and the hair and read as a hood. The wall is open towards the face - the
     * same hairline bounds it there - and, for styles that hang, it falls behind the temples to
     * `hang`, so the hair frames the face instead of crossing it.
     *
     * This replaces a rounded box placed behind the skull, which was wider than the head and
     * appeared as two slabs beside it.
     */
    if (puff > 0.0f) {
        const float headC = (crown + m_.y.chinY) * 0.5f;
        const float ra = m_.head.rx * 1.03f + thickness + puff;
        const float rb = m_.head.rz * 1.10f + thickness + puff * 0.9f;
        const float ry = headH * 0.50f + thickness + puff * 0.55f;
        const Vec3 c(0.0f, headC, zc + m_.head.rz * 0.04f);
        const Vec3 q = p - c;
        float outer;
        if (q.y >= 0.0f) {
            outer = sdEllipsoid(q, Vec3(ra, ry, rb));
        } else {
            /* Below the widest point the bell narrows a little, the way hair gathers. */
            const float t = saturate(-q.y / maxf(1e-3f, headC - (hang > 0.0f ? hang : napeY)));
            const float a = ra * lerpf(1.0f, 0.93f, t);
            const float bz = rb * lerpf(1.0f, 0.88f, t);
            const float k = std::sqrt((q.x / a) * (q.x / a) + (q.z / bz) * (q.z / bz));
            outer = (k - 1.0f) * minf(a, bz);
        }
        const float wall = maxf(0.024f, thickness + puff * 0.8f);
        float inner;
        {
            const Vec3 ri(maxf(ra - wall, 0.01f), maxf(ry - wall, 0.01f), maxf(rb - wall, 0.01f));
            if (q.y >= 0.0f) {
                inner = sdEllipsoid(q, ri);
            } else {
                const float k = std::sqrt((q.x / ri.x) * (q.x / ri.x) + (q.z / ri.z) * (q.z / ri.z));
                inner = (k - 1.0f) * minf(ri.x, ri.z);
            }
        }
        /*
         * The space inside the bell is hollow only where it is clear of the skin by 8 mm. Hair that
         * hangs beside a face lies against the temple and leaves the cheek lower down; with a
         * hollow that was simply "inside the inner wall" the gap between hair and cheek closed to
         * nothing where the two met, and no mesh can follow a gap of zero width - it came out as a
         * row of spikes down the cheek. Now the hair lies on the skin until the gap is wide enough
         * to be a shape, and the gap ends in a rounded end.
         */
        float hollow = smax(inner, 0.008f - skinD, 0.006f);
        /* ...and only below the jaw, round the neck. On the head the hair is solid from the scalp
           to its outer surface: a cavity there has nothing to show, and where the hair thins
           towards the hairline it was laid open as a slot under a flap. */
        hollow = smax(hollow, p.y - (m_.y.jawY + 0.010f), 0.010f);
        float bell = smax(outer, -hollow, 0.006f);
        /* The free edge of the mass - below the jaw, where it hangs clear of the neck - is rounded
           over almost its whole thickness. */
        const float edgeCut = edgeDistance(lowerEdgeAt);
        bell = smax(bell, edgeCut, 0.026f);
        /*
         * Where the mass lies against the head - across the forehead, at the temples, down the
         * side of the face to the jaw - it may stand no further off the skin than a ramp that
         * starts from nothing at its edge. Hair grows out of a hairline; it does not begin as a
         * wall three centimetres high. (As a wall it was a visor over the forehead and a cheek
         * plate beside the face, and the foot of the wall was a crease the mesher drew as a
         * saw-tooth.) Below the jaw the limit is released: there the hair hangs free.
         */
        const float below = maxf(0.0f, (m_.y.jawY + 0.005f) - p.y);
        /* The limit applies near the edge only: from five centimetres in it rises without bound. */
        const float inward = maxf(0.0f, -edgeCut - 0.050f);
        /* ...and, for hair that hangs, on the face's side only: round the back the lower edge is
           a free edge, a hand's breadth from the neck. */
        const float behind = (hang > 0.0f) ? 0.30f * smoothstepf(60.0f, 76.0f, phi) : 0.0f;
        const float release = 40.0f * below * below + 2.0f * inward + 30.0f * inward * inward + behind;
        bell = smax(bell, skinD - (layerOffset(edgeCut, thickness + puff, 0.070f) + release), 0.008f);
        /* A hard union: near the edge the two are the same surface, and a smooth union of a
           surface with itself inflates it. */
        d = minf(d, bell);
    }

    /* Hair that continues down the back: a sheet lying on whatever is outermost there. */
    if (backFall > 0.0f && p.z > zc && p.y < hang + 0.03f) {
        const float surface = minf(body(p), clothUpper(p));
        const float halfW = m_.head.rx * lerpf(1.30f, 1.05f, saturate((hang - p.y) / maxf(1e-3f, hang - backFall)));
        float sheet = smax(surface - 0.016f, -(surface - 0.002f), 0.003f);
        sheet = smax(sheet, std::fabs(p.x) - halfW, 0.012f);
        sheet = smax(sheet, maxf(backFall - p.y, p.y - (hang + 0.02f)), 0.012f);
        sheet = smax(sheet, (zc + m_.head.rz * 0.25f) - p.z, 0.010f);
        d = smin(d, sheet, 0.008f);
    }

    /* A crest keeps only a strip along the midline; used by the mohawk. */
    if (crest > 0.0f) d = smax(d, std::fabs(p.x) - crest, 0.008f);

    /* Ponytail: a tapered rope from the crown, sweeping down and back. */
    if (tail > 0.0f) {
        const Vec3 a(0.0f, crown - headH * 0.12f, zc + m_.head.rz * 1.02f);
        const Vec3 b(0.0f, crown - tail, zc + m_.head.rz * 1.45f);
        d = smin(d, sdTaperedCapsule(p, a, b, m_.head.rx * 0.34f, m_.head.rx * 0.10f), 0.010f);
    }
    /* Bun: a flattened ball at the back of the crown. */
    if (bun > 0.0f) {
        const Vec3 c(0.0f, crown - headH * 0.16f, zc + m_.head.rz * 0.98f);
        d = smin(d, sdEllipsoid(p - c, Vec3(bun * 0.95f, bun * 0.80f, bun * 0.70f)), 0.010f);
    }
    /*
     * Afro: one rounded mass, wider than the head all round and standing well above the crown. It
     * grows out of the hairline: at the edge it stands no further off the skin than it is from the
     * edge, so it rises from the forehead instead of overhanging it. (It used to be the same bell
     * as the hanging styles with a larger offset, which came out as a lumpy bouffant.)
     */
    if (afro > 0.0f) {
        const float headC = (crown + m_.y.chinY) * 0.5f;
        const Vec3 c(0.0f, headC + headH * 0.15f, zc + m_.head.rz * 0.10f);
        const Vec3 radii(m_.head.rx + afro, headH * 0.42f + afro, m_.head.rz + afro * 0.95f);
        float mass = sdEllipsoid(p - c, radii);
        /* Feathered to nothing at the hairline itself, like the layer on the scalp. */
        const float rise = layerOffset(lineCut, thickness, 0.030f) + 1.8f * maxf(0.0f, -lineCut);
        mass = smax(mass, skinD - rise, 0.016f);
        d = minf(d, mass);
    }
    /* Braids: two plaits, each a chain of overlapping beads laid over the shoulder. */
    if (!braidBeads_.empty()) {
        float b = 1e9f;
        for (const BraidBead& bead : braidBeads_) {
            const Vec3 r = p - bead.centre;
            if (std::fabs(r.x) > 0.06f || std::fabs(r.y) > 0.06f || std::fabs(r.z) > 0.06f) {
                b = minf(b, maxf(maxf(std::fabs(r.x), std::fabs(r.y)), std::fabs(r.z)) - 0.03f);
                continue;
            }
            b = minf(b, sdEllipsoid(r, bead.radii));
        }
        d = smin(d, b, 0.005f);
    }
    return d;
}

float BodyField::apparelField(const Vec3& p) const {
    return apparelField(p, nullptr);
}

float BodyField::apparelField(const Vec3& p, float* hairOut) const {
    if (hairOut) *hairOut = 1e9f;
    if (!hasApparel()) return 1e9f;
    float d = minf(clothUpper(p), clothLower(p));
    d = minf(d, shoeShape(p));
    d = minf(d, gearShape(p));
    const float hair = hairShape(p);
    if (hairOut) *hairOut = hair;
    d = minf(d, hair);
    return d;
}

bool BodyField::boundsApparel(Vec3* lo, Vec3* hi) const {
    if (!hasApparel()) return false;
    /*
     * The apparel's extent, stated generously rather than derived: a coat hem, a long ponytail and
     * a boot all add a few centimetres to the body, and the only cost of a generous bound is a
     * slightly larger grid. The alternative - clipping a hem because the grid ended - is a
     * silhouette defect that no amount of downstream care can fix.
     */
    Vec3 blo, bhi, tlo, thi;
    boundsBody(&blo, &bhi);
    boundsHead(&tlo, &thi);
    blo = blo.minv(tlo); bhi = bhi.maxv(thi);
    for (int side = -1; side <= 1; side += 2) {
        boundsHand(side, &tlo, &thi);
        blo = blo.minv(tlo); bhi = bhi.maxv(thi);
        boundsFoot(side, &tlo, &thi);
        blo = blo.minv(tlo); bhi = bhi.maxv(thi);
    }
    const float padXZ = 0.16f;
    const float padY = 0.10f;
    *lo = Vec3(blo.x - padXZ, blo.y - padY, blo.z - padXZ);
    *hi = Vec3(bhi.x + padXZ, bhi.y + padY, bhi.z + padXZ);
    return true;
}

int BodyField::classifyRegion(const Vec3& p) const {
    /*
     * Material selection, in priority order. Each test asks the SAME shape function that built the
     * geometry, so a region label can never disagree with the surface it labels - the previous
     * revision re-derived the eyes from a radius test that disagreed with the field by a factor of
     * two at the socket rim, and the lips were missed entirely except for a single vertex.
     */
    /*
     * The tolerance on each test is not arbitrary: mesh vertices are projected onto the isosurface
     * to within about 0.2 mm but are not exactly on it, so a strict `d < 0` test classifies a
     * surface vertex as skin about half the time and the feature loses its material entirely (the
     * previous revision produced ZERO eye vertices that way). 1.5 mm is larger than the projection
     * error and smaller than the distance from these shapes to the surrounding skin.
     */
    const float kOnSurface = 0.0015f;
    /*
     * Apparel is tested first. Cloth, shoes, gear and hair all sit outside the skin, so a point on
     * one of their surfaces would otherwise be labelled as skin - the region labels drive the
     * material assignment in the texture bake, and a shirt baked as skin is exactly the kind of
     * silent failure this ordering prevents. Precedence runs outermost layer first: a pauldron
     * covers a jacket sleeve, and a boot covers a trouser hem.
     */
    {
        float nearest = 1e9f;
        int nearestRegion = -1;
        const auto test = [&](float d, int region) {
            if (d < nearest) { nearest = d; nearestRegion = region; }
            return d < kOnSurface;
        };
        /* A sabaton is plate: it takes the equipment material, like the rest of the harness. */
        const int shoeRegion = (footwearKind() == 3) ? REGION_GEAR : REGION_SHOE;
        if (footwearKind() != 0 && test(shoeShape(p), shoeRegion)) return shoeRegion;
        if (apparel_.hair != 0 && test(hairShape(p), REGION_HAIR)) return REGION_HAIR;
        if ((apparel_.outfit == 5 || apparel_.outfit == 6 || apparel_.outfit == 7) &&
            test(gearShape(p), REGION_GEAR)) return REGION_GEAR;
        if (apparel_.outfit != 0 && test(clothUpper(p), REGION_CLOTH_UPPER)) return REGION_CLOTH_UPPER;
        if (apparel_.outfit != 0 && test(clothLower(p), REGION_CLOTH_LOWER)) return REGION_CLOTH_LOWER;
        /*
         * Not within tolerance of any garment. That does not make the point skin: in a concave
         * crease - between the two trouser legs, under a collar, along a hairline - a mesh vertex
         * can sit a few millimetres off both surfaces, and labelling it skin by default painted
         * skin-coloured specks along every such crease. The nearer surface decides.
         */
        if (nearestRegion >= 0 && nearest < skinUnion(p)) return nearestRegion;
    }

    if (apparel_.openMouth) {
        if (teethShape(p) < kOnSurface) return REGION_TEETH;
        if (tongueShape(p) < kOnSurface) return REGION_TONGUE;
        /* The walls of the oral cavity are mucosa, not skin. */
        if (oralCavity(p) < kOnSurface * 0.5f) return REGION_TONGUE;
    }
    /* Lips: the outline of the vermilion as seen from the front. The surface there is a blend of
       the lips' own form with the face around it and can lie a couple of millimetres off the
       former, so "within tolerance of the lip shape" painted only a rim. */
    if (lipsOutline(p) < 0.0f) return REGION_LIPS;
    if (browShape(p) < 0.0f) return REGION_LASH;
    // eyeballShape accepts anatomical side values (+1 = left, -1 = right).
    // Passing loop indices 0/1 silently reversed the eye material assignment.
    for (int side : {-1, 1}) {
        /*
         * On the globe AND inside the opening between the lids. Both tolerances are a fraction of
         * a millimetre: mesh vertices now lie within a few hundredths of a millimetre of the
         * surface, and with the general 1.5 mm tolerance the whole lid margin - which stands
         * 1.6 mm off the globe - was painted as the white of the eye.
         */
        const float globe = eyeballShape(p, side);
        if (globe > 0.004f) continue;
        bool upper = false;
        const float opening = eyeOpening(p, side, &upper);
        if (globe < 0.00030f && opening < 0.00020f) {
            return (side > 0) ? REGION_EYE_L : REGION_EYE_R;
        }
        /* The lash line: the edge of the upper lid, and a thinner line along the lower one. It is
           what separates the eye from the face at any distance; without it the white of the eye
           runs straight into skin. */
        const float band = upper ? 0.0013f : 0.0006f;
        if (opening >= 0.0f && opening < band && globe < 0.0030f) return REGION_LASH;
    }
    /* Nails: dorsal side of each distal phalanx. */
    for (int side = -1; side <= 1; side += 2) {
        const Chain& c = chain(side);
        const Vec3 palmNormal(-static_cast<float>(side), 0.0f, 0.0f);
        for (int i = 0; i < 5; ++i) {
            const float r = m_.fingerRadius[i];
            const Vec3 tip = c.fingerTip[i];
            /* Dorsal side of the fingertip: away from the palm, i.e. against palmNormal. The
               test used the opposite sign and painted the nails on the finger pads. */
            if ((p - tip).length() < r * 1.35f && (p - tip).dot(palmNormal) < -r * 0.30f) return REGION_NAIL;
        }
        {
            /* Toenails: the top of the last phalanx of each toe, found with the same toe
               placement the foot field uses. */
            const Vec3 q = footLocal(p, c);
            const float ground = footGround(c) + 0.002f;
            const float ballZ = c.ballL.z;
            for (int i = 0; i < 5; ++i) {
                Vec3 from, to;
                toeSegment(c, i, ground, ballZ, &from, &to);
                const float r = m_.toeRadius[i];
                const Vec3 rel = q - (to + Vec3(0.0f, r * 0.62f, r * 0.10f));
                if (rel.length() < r * 0.95f && rel.y > -r * 0.10f) return REGION_NAIL;
            }
        }
    }
    return REGION_SKIN;
}

/* =======================================================================================
 * Parts of a garment
 * ======================================================================================= */
/*
 * Which part of its garment a surface point belongs to. The geometry gives each garment one
 * region and so one material; what makes a suit a suit and a sneaker a sneaker at the size of a
 * phone screen is the parts within it - the shirt and tie in the opening of the jacket, the white
 * sole, the laces. The texture bake asks this function for every texel, and it is written from the
 * same landmarks and the same local frames as the shapes above, so paint and form agree.
 */
int BodyField::apparelPart(const Vec3& p, int region) const {
    if (region == REGION_SHOE || (region == REGION_GEAR && footwearKind() == 3 && shoeShape(p) < 0.004f)) {
        const int kind = footwearKind();
        const int side = (p.x >= 0.0f) ? 1 : -1;
        const Chain& c = chain(side);
        const Vec3 q = footLocal(p, c);
        const float L = m_.footLength, W = m_.footWidth, Hf = m_.footHeight;
        const float ground = footGround(c) + 0.002f;
        const float ballZ = c.ballL.z;
        const float up = q.y - ground;                       /* height above the foot's own sole */
        switch (kind) {
            case 1: {
                if (up < 0.007f) return PART_OUTSOLE;
                if (up < 0.027f) return PART_SOLE;
                /* Rubber toe cap. */
                if (q.z < ballZ - L * 0.085f && up < Hf * 0.95f) return PART_TOECAP;
                /* Laces: bars across the tongue, between the toe cap and the collar. */
                if (std::fabs(q.x) < W * 0.20f && q.z < -L * 0.05f && q.z > ballZ - L * 0.03f && up > Hf * 0.55f) {
                    const float bar = std::fmod(std::fabs(q.z) + 100.0f, 0.016f);
                    return (bar < 0.0065f) ? PART_LACE : PART_ACCENT;
                }
                /* Collar lining at the opening. */
                if (p.y > shoeTopY() - 0.008f) return PART_ACCENT;
                return PART_BASE;
            }
            case 2: {
                if (up < 0.019f) return PART_OUTSOLE;
                /* A welt line above the sole, and a strap across the instep. */
                if (up < 0.024f) return PART_TRIM;
                if (p.y > shoeTopY() - 0.012f) return PART_TRIM;
                return PART_BASE;
            }
            case 3: {
                /* The joints between the lames: dark lines across the foot. */
                if (p.y > c.ankle.y + 0.012f) {
                    /* The shaft: hoops round the leg. */
                    const float band = std::fmod(p.y - c.ankle.y + 100.0f, 0.036f);
                    if (band < 0.0035f) return PART_TRIM;
                } else {
                    const float lame = std::fmod(std::fabs(q.z) + 100.0f + 0.015f, 0.030f);
                    if (up > 0.012f && q.z < -L * 0.02f && lame < 0.0035f) return PART_TRIM;
                }
                return PART_STEEL;
            }
            case 4: {
                /* The sole, and the tip of the heel. */
                if (up < -0.0045f && p.y > 0.012f) return PART_BASE;      /* the heel block */
                if (up < 0.004f) return PART_OUTSOLE;
                return PART_BASE;
            }
            case 5: {
                if (up < 0.010f) return PART_OUTSOLE;
                if (up < 0.031f) return PART_SOLE;
                /* Two fastening bands round the shaft. */
                const float h = p.y - c.ankle.y;
                if ((h > 0.045f && h < 0.060f) || p.y > shoeTopY() - 0.016f) return PART_TRIM;
                return PART_BASE;
            }
            default: return PART_BASE;
        }
    }
    const auto sq = [](float v) { return v * v; };
    const int o = apparel_.outfit;
    const float H = m_.height;
    if (region == REGION_GEAR) {
        int piece = GEAR_NONE;
        gearField(p, &piece);
        if (o == 5) {
            /* The joints between the hoops of the fauld, and the lower edge of each plate. */
            if (piece == GEAR_FAULD) {
                const float lame = std::fmod((m_.y.waistY - p.y) + 100.0f, 0.034f);
                return (lame < 0.004f) ? PART_TRIM : PART_BASE;
            }
            if (piece == GEAR_CUIRASS && p.y < m_.y.waistY + 0.004f * H) return PART_TRIM;
            if (piece == GEAR_PAULDRON) {
                const Chain& c = chain((p.x >= 0.0f) ? 1 : -1);
                const float drop = c.shoulder.y - p.y;
                if (drop > 0.0f && std::fmod(drop + 100.0f, 0.040f) < 0.004f) return PART_TRIM;
            }
            return PART_BASE;
        }
        if (o == 6) {
            if (piece == GEAR_BELT) {
                const bool front = p.z < m_.waist.centreZ;
                if (front && std::fabs(p.x) < 0.024f && std::fabs(p.y - m_.y.waistY) < 0.019f) return PART_METAL;   /* buckle */
                return PART_ACCENT;
            }
            if (piece == GEAR_SKIRT) {
                /* Straps: eighteen round the hips, with a gap between each. */
                const float turn = std::atan2(p.x, -(p.z - m_.hip.centreZ)) / 6.2831853f + 0.5f;
                const float strap = std::fmod(turn * 18.0f + 100.0f, 1.0f);
                if (strap < 0.10f) return PART_TRIM;
                /* A stud near the end of each strap. */
                const float hem = m_.y.crotchY - m_.thighLen * 0.34f;
                if (std::fabs(strap - 0.55f) < 0.14f && std::fabs(p.y - (hem + 0.030f)) < 0.008f) return PART_METAL;
                return PART_BASE;
            }
            if (piece == GEAR_CUIRASS) {
                /* Studs in rows, and a seam down the front. */
                const bool front = p.z < m_.chest.centreZ;
                if (front && std::fabs(p.x) < 0.0026f) return PART_TRIM;
                const float gx = std::fmod(std::fabs(p.x) + 0.0225f, 0.045f) - 0.0225f;
                const float gy = std::fmod(p.y + 100.0f, 0.050f) - 0.025f;
                if (sq(gx) + sq(gy) < sq(0.0055f)) return PART_METAL;
                return PART_BASE;
            }
            if (piece == GEAR_BRACER) {
                const Chain& c = chain((p.x >= 0.0f) ? 1 : -1);
                const Vec3 axisF = (c.wrist - c.elbow).normalizedOr({0.0f, -1.0f, 0.0f});
                const float f = (p - c.elbow).dot(axisF);
                return (std::fmod(f + 100.0f, 0.040f) < 0.005f) ? PART_ACCENT : PART_BASE;      /* straps */
            }
            return PART_BASE;
        }
        if (o == 7) {
            if (piece == GEAR_NECKRING) return PART_METAL;
            if (piece == GEAR_GLOVE) return PART_TRIM;
            if (piece == GEAR_CHESTBOX) {
                if (p.z > suitFrontZ_ - 0.040f) return PART_BASE;          /* its sides */
                const float bx = p.x, by = p.y - suitBoxY_;
                /* A dark panel with two knobs and an indicator. */
                if (std::fabs(bx) < 0.074f && std::fabs(by) < 0.044f) {
                    if (sq(bx + 0.045f) + sq(by + 0.012f) < sq(0.011f)) return PART_ACCENT;
                    if (sq(bx - 0.045f) + sq(by + 0.012f) < sq(0.011f)) return PART_METAL;
                    if (std::fabs(bx) < 0.030f && std::fabs(by - 0.020f) < 0.008f) return PART_BUTTON;
                    return PART_PANEL;
                }
                return PART_BASE;
            }
            if (piece == GEAR_PACK) {
                /* A frame line round the pack and a dark vent near its top. */
                const float bx = std::fabs(p.x), by = p.y - (m_.y.bustY + 0.004f * H);
                if (p.z > suitBackZ_ + 0.100f) {
                    if (std::fabs(bx - 0.118f) < 0.003f && std::fabs(by) < 0.180f) return PART_TRIM;
                    if (std::fabs(std::fabs(by) - 0.180f) < 0.003f && bx < 0.118f) return PART_TRIM;
                    if (bx < 0.070f && std::fabs(by - 0.120f) < 0.020f) return PART_PANEL;
                }
                return PART_BASE;
            }
            return PART_BASE;
        }
        return PART_BASE;
    }
    if (region == REGION_CLOTH_UPPER) {
        const float ax = std::fabs(p.x);
        const float opening = frontOpening(p);
        const bool front = p.z < m_.chest.centreZ - m_.chest.rz * 0.20f;
        /* Sleeve or trunk, and how far along the arm (measured as a sleeve's reach is). */
        const Chain& c = chain((p.x >= 0.0f) ? 1 : -1);
        const bool onSleeve = armChain(p, c) < trunkAndLegs(p);
        const Vec3 armAxis = (c.elbow - c.shoulder).normalizedOr({0.0f, -1.0f, 0.0f});
        const float along = (p - c.shoulder).dot(armAxis);
        const float wristReach = m_.upperArmLen + m_.forearmLen;
        switch (o) {
            case 3: {   /* suit: shirt and tie in the V, lapel edge, buttons, cuffs */
                const float topY = m_.y.neckBaseY + 0.012f * H;
                const float apexY = lerpf(m_.y.waistY, m_.y.underbustY, 0.55f);
                if (opening < 0.0f) {
                    const float t = saturate((topY - p.y) / maxf(1e-3f, topY - apexY));
                    const float half = (t < 0.12f) ? 0.017f : lerpf(0.011f, 0.025f, (t - 0.12f) / 0.88f);
                    return (ax < half) ? PART_TIE : PART_SHIRT;
                }
                if (opening < 0.0045f) return PART_TRIM;
                if (onSleeve) return (along > wristReach * 0.955f - 0.016f) ? PART_SHIRT : PART_BASE;
                if (front) {
                    if (p.y < apexY && ax < 0.0022f) return PART_TRIM;
                    for (int b = 0; b < 2; ++b) {
                        const float by = apexY - 0.030f - 0.075f * static_cast<float>(b);
                        if (sq(p.x - 0.014f) + sq(p.y - by) < sq(0.0085f)) return PART_BUTTON;
                    }
                    if (p.x > 0.050f && p.x < 0.100f && std::fabs(p.y - (m_.y.bustY + 0.022f * H)) < 0.0022f) return PART_TRIM;
                }
                return PART_BASE;
            }
            case 4: {   /* denim jacket: T-shirt between the fronts, stitching, flap pockets */
                if (opening < 0.0f) return PART_SHIRT;
                const float hemY = lerpf(m_.y.hipY, m_.y.waistY, 0.45f);
                if (onSleeve) {
                    return (std::fabs(along - (wristReach * 0.945f - 0.045f)) < 0.0022f) ? PART_TRIM : PART_BASE;
                }
                if (front) {
                    if (opening > 0.012f && opening < 0.0155f) return PART_TRIM;
                    if (p.x > 0.040f && p.x < 0.060f) {
                        const float u = std::fmod(p.y - hemY + 100.0f, 0.070f) - 0.035f;
                        if (sq(p.x - 0.0495f) + sq(u) < sq(0.0065f)) return PART_BUTTON;
                    }
                    const float flapY = m_.y.bustY + 0.030f * H;
                    if (ax > 0.060f && ax < 0.124f) {
                        if (std::fabs(p.y - flapY) < 0.0022f) return PART_TRIM;
                        const float v = flapY - 0.022f - 0.012f * (1.0f - std::fabs((ax - 0.092f) / 0.032f));
                        if (std::fabs(p.y - v) < 0.0022f) return PART_TRIM;
                        if (sq(ax - 0.092f) + sq(p.y - (flapY - 0.019f)) < sq(0.006f)) return PART_BUTTON;
                    }
                }
                if (std::fabs(p.y - (m_.y.bustY + 0.062f * H)) < 0.0022f) return PART_TRIM;     /* yoke      */
                if (std::fabs(p.y - (hemY + 0.036f)) < 0.0022f) return PART_TRIM;              /* waistband */
                return PART_BASE;
            }
            case 7: {   /* spacesuit: joint bearings, a stripe, the wrist rings */
                if (onSleeve) {
                    if (along > wristReach * 0.985f - 0.026f) return PART_METAL;
                    if (std::fabs(along - m_.upperArmLen) < 0.030f) return PART_TRIM;
                    if (along > 0.015f && along < 0.050f) return PART_TRIM;
                    if (along > m_.upperArmLen * 0.42f && along < m_.upperArmLen * 0.42f + 0.032f) return PART_ACCENT;
                    return PART_BASE;
                }
                if (std::fabs(p.y - m_.y.waistY) < 0.015f) return PART_TRIM;
                return PART_BASE;
            }
            case 9: {   /* coat: a dark layer in the V, the front edge, three buttons */
                if (opening < 0.0f) return PART_SHIRT;
                if (opening < 0.005f) return PART_TRIM;
                if (onSleeve) return PART_BASE;
                if (front) {
                    const float apexY = lerpf(m_.y.waistY, m_.y.underbustY, 0.95f);
                    if (p.y < apexY && std::fabs(p.x + 0.006f) < 0.0024f) return PART_TRIM;
                    for (int b = 0; b < 3; ++b) {
                        const float by = apexY - 0.055f - 0.105f * static_cast<float>(b);
                        if (sq(p.x - 0.024f) + sq(p.y - by) < sq(0.010f)) return PART_BUTTON;
                    }
                }
                return PART_BASE;
            }
            default: return PART_BASE;
        }
    }
    if (region == REGION_CLOTH_LOWER) {
        if (o == 7) {
            if (std::fabs(p.y - m_.y.kneeY) < 0.034f) return PART_TRIM;
            if (p.y > m_.y.kneeY + 0.150f && p.y < m_.y.kneeY + 0.182f) return PART_ACCENT;
            return PART_BASE;
        }
        if (o == 1 || o == 4) {
            /* Jeans: the waistband seam. */
            if (std::fabs(p.y - (m_.y.waistY - 0.008f)) < 0.0022f) return PART_TRIM;
        }
        return PART_BASE;
    }
    return PART_BASE;
}

/* =======================================================================================
 * Bounds
 * ======================================================================================= */
/*
 * Bounds philosophy: a domain is extracted on a dense grid, so its bounding box MUST fully
 * contain the surface. If it does not, the mesh is clipped by the grid wall and the extractor
 * legitimately reports boundary loops there - which the stitcher then cannot match against
 * the intended cut plane, leaving holes. The margins below are therefore derived from the
 * largest soft-tissue bulge and the widest rounding radius in each domain, not guessed.
 */
void BodyField::boundsBody(Vec3* mn, Vec3* mx) const {
    const float H = m_.height;
    const float armReach = m_.biacromialHalf + m_.deltoidR * 1.6f + m_.upperArmR * 1.5f;
    const float footExtent = m_.footLength + m_.footWidth;
    const float margin = 0.030f + 0.02f * H;
    mn->x = -armReach - margin;
    mx->x = armReach + margin;
    mn->y = -0.010f;
    mx->y = neckCutY();
    mn->z = -maxf(footExtent, m_.bustSeg.rz * 1.9f + m_.gluteBulge) - margin;
    mx->z = maxf(m_.footLength * 0.8f, m_.hip.rz * 1.8f + m_.gluteBulge) + margin;
}

void BodyField::boundsHead(Vec3* mn, Vec3* mx) const {
    /* Ears add ~0.6 head radii laterally, the nose ~1.35 forward, the occiput ~1.2 back. */
    const float pad = 0.030f;
    mn->x = -m_.head.rx * 1.75f - pad;
    mx->x = m_.head.rx * 1.75f + pad;
    mn->y = neckCutY() - 0.005f;
    mx->y = m_.y.crownY + 0.006f;
    mn->z = -m_.head.rz * 1.55f - pad;
    mx->z = m_.head.rz * 1.35f + pad;
}

void BodyField::boundsHand(int side, Vec3* mn, Vec3* mx) const {
    const Chain& c = chain(side);
    const float pad = 0.020f;
    const float widest = m_.fingerRadius[0] * 1.35f + m_.palmThickness * 0.5f;
    Vec3 lo(1e30f, 1e30f, 1e30f), hi(-1e30f, -1e30f, -1e30f);
    auto include = [&](const Vec3& v, float r) {
        lo = lo.minv(v - Vec3(r));
        hi = hi.maxv(v + Vec3(r));
    };
    include(c.wrist, m_.wristR * 1.2f);
    include(c.palmCentre, m_.palmWidth * 0.75f);
    for (int i = 0; i < 5; ++i) {
        include(c.fingerTip[i], m_.fingerRadius[i] * 1.6f);
        include(c.fingerKnuckle[i], m_.fingerRadius[i] * 1.6f);
        include(c.fingerJoints[i][0], widest);
        include(c.fingerJoints[i][1], widest);
    }
    *mn = lo - Vec3(pad);
    *mx = hi + Vec3(pad);
    /* The domain must straddle its cut plane so the extracted mesh has a ring exactly there. */
    mx->y = maxf(mx->y, wristCutY(side) + 0.006f);
    mn->y = minf(mn->y, wristCutY(side) - 0.05f);
}

void BodyField::boundsFoot(int side, Vec3* mn, Vec3* mx) const {
    const Chain& c = chain(side);
    const float pad = 0.020f;
    mn->x = c.ankle.x - (m_.footWidth * 1.10f + m_.ankleRx * 1.5f) - pad;
    mx->x = c.ankle.x + (m_.footWidth * 1.10f + m_.ankleRx * 1.5f) + pad;
    /* The sole sits a fixed gap above the ground plane; nothing may poke below it. */
    mn->y = m_.y.soleY - 0.008f;
    mx->y = ankleCutY(side) + 0.006f;
    mn->z = c.heel.z - m_.footLength * 0.30f - pad;
    mx->z = c.ankle.z + m_.footLength * 0.30f + pad;
}

} // namespace af
