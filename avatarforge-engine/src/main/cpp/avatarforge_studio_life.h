#ifndef AVATARFORGE_STUDIO_LIFE_H
#define AVATARFORGE_STUDIO_LIFE_H

#include "render/af_render.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace af {

/**
 * Critically-damped exponential camera glide for cinematic preset transitions.
 * Shortest-arc yaw interpolation avoids 359° -> 1° spin-around artifacts.
 */
inline float wrapAngleDeltaDeg(float targetDeg, float currentDeg) {
    float d = std::fmod(targetDeg - currentDeg + 540.0f, 360.0f) - 180.0f;
    if (d < -180.0f) d += 360.0f;
    return d;
}

inline bool stepCameraGlide(CameraState* current, const CameraState& target, float dt) {
    if (!current || dt <= 0.0f) return false;
    const float alpha = 1.0f - std::exp(-8.5f * std::min(dt, 0.1f));
    const float dyaw = wrapAngleDeltaDeg(target.yawDeg, current->yawDeg);
    const float dpitch = target.pitchDeg - current->pitchDeg;
    const float ddist = target.distance - current->distance;
    const float dfov = target.fovDeg - current->fovDeg;
    const float dtx = target.targetX - current->targetX;
    const float dty = target.targetY - current->targetY;
    const float dtz = target.targetZ - current->targetZ;

    const bool settled =
        std::fabs(dyaw) < 0.05f &&
        std::fabs(dpitch) < 0.05f &&
        std::fabs(ddist) < 0.0008f &&
        std::fabs(dfov) < 0.05f &&
        std::fabs(dtx) < 0.0008f &&
        std::fabs(dty) < 0.0008f &&
        std::fabs(dtz) < 0.0008f;

    if (settled) {
        *current = target;
        return false;
    }

    current->yawDeg = std::fmod(current->yawDeg + dyaw * alpha + 360.0f, 360.0f);
    current->pitchDeg += dpitch * alpha;
    current->distance += ddist * alpha;
    current->fovDeg += dfov * alpha;
    current->targetX += dtx * alpha;
    current->targetY += dty * alpha;
    current->targetZ += dtz * alpha;
    return true;
}

/**
 * Living Digital Human Micro-Life Engine (52 ARKit blendshapes).
 * Combines:
 *  - Smooth exponential facial muscle morphing between user-selected expression presets
 *  - Physiological asymmetric autonomous blinking (65ms snap close / 155ms gentle open + occasional double blink)
 *  - Live camera eye-contact gaze tracking within natural human ocular limits (+/-42 deg yaw, +/-24 deg pitch)
 *  - Subtle ocular micro-saccades and warm living resting micro-expression
 */
struct StudioMicroLifeState {
    bool enabled = true;
    float elapsedSec = 0.0f;
    float nextBlinkSec = 2.6f;
    float blinkTimer = -1.0f;
    int blinkCount = 0;
    bool pendingDoubleBlink = false;
    int activeExpressionId = 0;
    std::array<float, 52> targetExpression{};
    std::array<float, 52> smoothedExpression{};
    std::array<float, 52> appliedWeights{};
    bool hasAppliedOnce = false;

    void setExpressionPreset(int expressionId) {
        activeExpressionId = expressionId;
        targetExpression.fill(0.0f);
        switch (expressionId) {
            case 1: // Smile
                targetExpression[43] = 0.85f; // mouthSmileLeft
                targetExpression[44] = 0.85f; // mouthSmileRight
                targetExpression[6]  = 0.45f; // cheekSquintLeft
                targetExpression[7]  = 0.45f; // cheekSquintRight
                targetExpression[18] = 0.25f; // eyeSquintLeft
                targetExpression[19] = 0.25f; // eyeSquintRight
                targetExpression[27] = 0.30f; // mouthDimpleLeft
                targetExpression[28] = 0.30f; // mouthDimpleRight
                break;
            case 2: // Blink
                targetExpression[8]  = 1.00f; // eyeBlinkLeft
                targetExpression[9]  = 1.00f; // eyeBlinkRight
                break;
            case 3: // Wink
                targetExpression[8]  = 1.00f; // eyeBlinkLeft
                targetExpression[6]  = 0.40f; // cheekSquintLeft
                targetExpression[43] = 0.65f; // mouthSmileLeft
                targetExpression[44] = 0.35f; // mouthSmileRight
                targetExpression[4]  = 0.35f; // browOuterUpRight
                break;
            case 4: // Surprised
                targetExpression[2]  = 0.85f; // browInnerUp
                targetExpression[3]  = 0.80f; // browOuterUpLeft
                targetExpression[4]  = 0.80f; // browOuterUpRight
                targetExpression[20] = 0.90f; // eyeWideLeft
                targetExpression[21] = 0.90f; // eyeWideRight
                targetExpression[24] = 0.72f; // jawOpen
                break;
            case 5: // Frown
                targetExpression[0]  = 0.80f; // browDownLeft
                targetExpression[1]  = 0.80f; // browDownRight
                targetExpression[29] = 0.75f; // mouthFrownLeft
                targetExpression[30] = 0.75f; // mouthFrownRight
                targetExpression[49] = 0.25f; // noseSneerLeft
                targetExpression[50] = 0.25f; // noseSneerRight
                break;
            case 6: // Gaze Left
                targetExpression[14] = 0.90f; // eyeLookOutLeft
                targetExpression[13] = 0.90f; // eyeLookInRight
                break;
            case 7: // Gaze Right
                targetExpression[12] = 0.90f; // eyeLookInLeft
                targetExpression[15] = 0.90f; // eyeLookOutRight
                break;
            default: // 0 = Neutral
                break;
        }
    }

    /**
     * Advances micro-life + smooth muscle interpolation by `dt` seconds.
     * `camRelYawDeg` is camera yaw relative to character's front-facing yaw (0 = dead front,
     * + = camera to character's left, - = camera to character's right).
     * Returns true if `outWeights` differs meaningfully from `appliedWeights`.
     */
    bool step(float dt, float camRelYawDeg, float camPitchDeg, std::array<float, 52>* outWeights) {
        if (!outWeights) return false;
        const float safeDt = std::clamp(dt, 0.001f, 0.1f);
        elapsedSec += safeDt;

        // Smooth facial muscle interpolation (~100ms half-life).
        const float muscleAlpha = 1.0f - std::exp(-11.0f * safeDt);
        for (size_t i = 0; i < 52; ++i) {
            smoothedExpression[i] += (targetExpression[i] - smoothedExpression[i]) * muscleAlpha;
            if (std::fabs(smoothedExpression[i] - targetExpression[i]) < 0.0005f) {
                smoothedExpression[i] = targetExpression[i];
            }
        }

        std::array<float, 52> composed = smoothedExpression;

        if (enabled) {
            // 1. Physiological asymmetric autonomous blinking (65ms close / 155ms open).
            float autoBlink = 0.0f;
            if (blinkTimer >= 0.0f) {
                blinkTimer += safeDt;
                constexpr float kCloseDur = 0.065f;
                constexpr float kHoldDur  = 0.025f;
                constexpr float kOpenDur  = 0.155f;
                constexpr float kTotalDur = kCloseDur + kHoldDur + kOpenDur;
                if (blinkTimer < kCloseDur) {
                    const float t = blinkTimer / kCloseDur;
                    autoBlink = t * t;
                } else if (blinkTimer < kCloseDur + kHoldDur) {
                    autoBlink = 1.0f;
                } else if (blinkTimer < kTotalDur) {
                    const float t = (blinkTimer - (kCloseDur + kHoldDur)) / kOpenDur;
                    autoBlink = 1.0f - (1.0f - (1.0f - t) * (1.0f - t));
                } else {
                    blinkTimer = -1.0f;
                    if (pendingDoubleBlink) {
                        pendingDoubleBlink = false;
                        nextBlinkSec = elapsedSec + 0.14f;
                    } else {
                        ++blinkCount;
                        // Natural irregular cadence between 3.1s and 5.3s; every 5th blink is a quick double-blink.
                        const float frac = std::fmod(static_cast<float>(blinkCount) * 0.6180339f, 1.0f);
                        nextBlinkSec = elapsedSec + 3.1f + frac * 2.2f;
                        pendingDoubleBlink = (blinkCount % 5 == 0);
                    }
                }
            } else if (elapsedSec >= nextBlinkSec) {
                blinkTimer = 0.0f;
            }

            // Do not fight explicit Blink (2) or Wink (3) presets.
            if (activeExpressionId != 2 && activeExpressionId != 3) {
                composed[8] = std::max(composed[8], autoBlink);
                composed[9] = std::max(composed[9], autoBlink);
            }

            // 2. Live camera eye-contact gaze tracking + subtle ocular micro-saccades.
            // Relax gaze smoothly back forward if user orbits behind character (> 55 deg).
            if (activeExpressionId != 6 && activeExpressionId != 7) {
                const float absYaw = std::fabs(camRelYawDeg);
                const float frontEngagement = std::clamp(1.0f - (absYaw - 42.0f) / 28.0f, 0.0f, 1.0f);
                const float saccadeYaw = 0.045f * std::sin(elapsedSec * 1.37f) * std::cos(elapsedSec * 0.53f);
                const float saccadePitch = 0.030f * std::sin(elapsedSec * 0.89f + 1.1f);
                const float normYaw = std::clamp((camRelYawDeg / 38.0f) * frontEngagement + saccadeYaw, -0.85f, 0.85f);
                const float normPitch = std::clamp((camPitchDeg / 26.0f) * frontEngagement + saccadePitch, -0.65f, 0.65f);

                if (normYaw > 0.0f) {
                    // Camera to character's left: left eye out [14], right eye in [13].
                    composed[14] = std::clamp(composed[14] + normYaw, 0.0f, 1.0f);
                    composed[13] = std::clamp(composed[13] + normYaw, 0.0f, 1.0f);
                } else if (normYaw < 0.0f) {
                    // Camera to character's right: left eye in [12], right eye out [15].
                    composed[12] = std::clamp(composed[12] - normYaw, 0.0f, 1.0f);
                    composed[15] = std::clamp(composed[15] - normYaw, 0.0f, 1.0f);
                }
                if (normPitch > 0.0f) {
                    composed[16] = std::clamp(composed[16] + normPitch, 0.0f, 1.0f); // eyeLookUpLeft
                    composed[17] = std::clamp(composed[17] + normPitch, 0.0f, 1.0f); // eyeLookUpRight
                } else if (normPitch < 0.0f) {
                    composed[10] = std::clamp(composed[10] - normPitch, 0.0f, 1.0f); // eyeLookDownLeft
                    composed[11] = std::clamp(composed[11] - normPitch, 0.0f, 1.0f); // eyeLookDownRight
                }
            }

            // 3. Subtle resting micro-warmth and breathing brow drift when Neutral.
            if (activeExpressionId == 0) {
                const float breathPhase = 0.5f + 0.5f * std::sin(elapsedSec * 1.45f);
                composed[43] = std::clamp(composed[43] + 0.055f + 0.020f * breathPhase, 0.0f, 1.0f);
                composed[44] = std::clamp(composed[44] + 0.055f + 0.020f * breathPhase, 0.0f, 1.0f);
                composed[2]  = std::clamp(composed[2] + 0.035f * breathPhase, 0.0f, 1.0f);
            }
        }

        float maxDelta = 0.0f;
        for (size_t i = 0; i < 52; ++i) {
            composed[i] = std::clamp(composed[i], 0.0f, 1.0f);
            maxDelta = std::max(maxDelta, std::fabs(composed[i] - appliedWeights[i]));
        }
        *outWeights = composed;
        if (!hasAppliedOnce || maxDelta > 0.002f) {
            appliedWeights = composed;
            hasAppliedOnce = true;
            return true;
        }
        return false;
    }
};

} // namespace af

#endif // AVATARFORGE_STUDIO_LIFE_H
