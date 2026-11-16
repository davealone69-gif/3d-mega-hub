/*
 * af_anim_face.cpp -- morph-target control, ARKit expressions, autonomous blink, visemes and the
 *                     lip-sync timeline.
 *
 * WHAT "DRIVES THE FACE" MEANS HERE
 * ---------------------------------
 * Three real sources, all of which end up in one flat morph-weight array:
 *   1. an imported glTF "weights" animation channel (af_anim_clip.cpp samples it into Pose::morphs);
 *   2. the engine's own 52 ARKit blendshape channels (af_rig.h kBlendshapeNames), which
 *      af_engine_bake_blendshapes already bakes into the generated avatar's morph targets;
 *   3. this file's expression / viseme / blink controllers, which write ARKit weights and let
 *      FaceController map them onto whatever the loaded asset actually has.
 *
 * THE HONESTY RULE
 * ----------------
 * FaceController is configured with the morph target names the asset REALLY has. A request for a
 * channel the asset does not provide is recorded in unbound() and returns false. Nothing pretends
 * to drive a target that is not there, and nothing silently succeeds: AnimatedMorphCube declares two
 * unnamed targets, so `applyExpression(Happy)` on it writes zero channels and says so, while
 * `setWeightAt(0, 1.0f)` writes one and reports it. Fox.glb has no morph targets at all, and its
 * FaceController reports morphCount() == 0 rather than inventing a face.
 *
 * LIP-SYNC SCOPE, STATED PLAINLY
 * ------------------------------
 * LipSyncTrack::buildFromKeys() plays an explicit viseme timeline, which is what a real phoneme
 * aligner would produce. buildFromText() is a deterministic spelling-to-viseme approximation at a
 * fixed syllable rate: it does not analyse audio, does not know the speaker's timing, and every UI
 * surface that offers it labels it approximate. There is no audio-driven viseme extraction in this
 * project and this file does not claim otherwise.
 */
#include "af_anim.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace af {
namespace anim {

/* ================================================================================== visemes ==== */

const char* visemeName(Viseme v) {
    switch (v) {
        case Viseme::Silence: return "sil";
        case Viseme::PP: return "PP";
        case Viseme::FF: return "FF";
        case Viseme::TH: return "TH";
        case Viseme::DD: return "DD";
        case Viseme::KK: return "KK";
        case Viseme::CH: return "CH";
        case Viseme::SS: return "SS";
        case Viseme::NN: return "NN";
        case Viseme::RR: return "RR";
        case Viseme::AA: return "AA";
        case Viseme::E: return "E";
        case Viseme::I: return "I";
        case Viseme::O: return "O";
        case Viseme::U: return "U";
        case Viseme::COUNT: return "<count>";
    }
    return "<invalid viseme>";
}

Viseme visemeForChar(char c) {
    /* Spelling -> mouth shape. English orthography is not phonetics, so this is an approximation by
       construction; it is grouped the way real-time viseme sets are grouped (plosive, fricative,
       vowel) rather than pretending to a per-phoneme accuracy it does not have. */
    switch (c) {
        case 'p': case 'P': case 'b': case 'B': case 'm': case 'M': return Viseme::PP;
        case 'f': case 'F': case 'v': case 'V': return Viseme::FF;
        case 't': case 'T': case 'd': case 'D': return Viseme::DD;
        case 'k': case 'K': case 'g': case 'G': return Viseme::KK;
        case 's': case 'S': case 'z': case 'Z': return Viseme::SS;
        case 'n': case 'N': case 'l': case 'L': return Viseme::NN;
        case 'r': case 'R': return Viseme::RR;
        case 'a': case 'A': return Viseme::AA;
        case 'e': case 'E': return Viseme::E;
        case 'i': case 'I': case 'y': case 'Y': return Viseme::I;
        case 'o': case 'O': return Viseme::O;
        case 'u': case 'U': case 'w': case 'W': return Viseme::U;
        case 'c': case 'C': return Viseme::CH;   /* 'c' is /s/ or /k/; CH is the safer neutral mouth */
        case 'h': case 'H': case 'j': case 'J':
        case 'q': case 'Q': case 'x': case 'X': return Viseme::CH;
        case ' ': case '\t': case '\n': case '\r':
        case '.': case ',': case ';': case ':': case '!': case '?': return Viseme::Silence;
        default: return Viseme::Silence;          /* digits and punctuation do not move the mouth */
    }
}

/* ================================================================================ expressions == */

const char* expressionName(Expression e) {
    switch (e) {
        case Expression::Neutral: return "Neutral";
        case Expression::Happy: return "Happy";
        case Expression::Sad: return "Sad";
        case Expression::Angry: return "Angry";
        case Expression::Surprised: return "Surprised";
        case Expression::Fear: return "Fear";
        case Expression::Disgust: return "Disgust";
        case Expression::COUNT: return "<count>";
    }
    return "<invalid expression>";
}

namespace {

/** Index of an ARKit channel name in af::kBlendshapeNames, or -1. */
int arkitIndex(const char* name) {
    for (int i = 0; i < kBlendshapeCount; ++i) {
        if (std::strcmp(kBlendshapeNames[i], name) == 0) return i;
    }
    return -1;
}

void setArkit(std::vector<float>* out, const char* name, float value) {
    const int i = arkitIndex(name);
    if (i >= 0 && i < static_cast<int>(out->size())) (*out)[static_cast<size_t>(i)] = value;
}

} /* namespace */

void expressionWeights(Expression e, float amount, std::vector<float>* out) {
    if (!out) return;
    out->assign(static_cast<size_t>(kBlendshapeCount), 0.0f);
    const float a = saturate(std::isfinite(amount) ? amount : 0.0f);
    switch (e) {
        case Expression::Neutral:
            break;
        case Expression::Happy:
            setArkit(out, "mouthSmileLeft", 0.72f * a);
            setArkit(out, "mouthSmileRight", 0.72f * a);
            setArkit(out, "cheekSquintLeft", 0.34f * a);
            setArkit(out, "cheekSquintRight", 0.34f * a);
            setArkit(out, "browInnerUp", 0.16f * a);
            setArkit(out, "eyeSquintLeft", 0.12f * a);
            setArkit(out, "eyeSquintRight", 0.12f * a);
            break;
        case Expression::Sad:
            setArkit(out, "mouthFrownLeft", 0.62f * a);
            setArkit(out, "mouthFrownRight", 0.62f * a);
            setArkit(out, "browInnerUp", 0.82f * a);
            setArkit(out, "eyeLookDownLeft", 0.22f * a);
            setArkit(out, "eyeLookDownRight", 0.22f * a);
            setArkit(out, "mouthPressLeft", 0.14f * a);
            setArkit(out, "mouthPressRight", 0.14f * a);
            break;
        case Expression::Angry:
            setArkit(out, "browDownLeft", 0.86f * a);
            setArkit(out, "browDownRight", 0.86f * a);
            setArkit(out, "noseSneerLeft", 0.44f * a);
            setArkit(out, "noseSneerRight", 0.44f * a);
            setArkit(out, "mouthPressLeft", 0.42f * a);
            setArkit(out, "mouthPressRight", 0.42f * a);
            setArkit(out, "eyeSquintLeft", 0.34f * a);
            setArkit(out, "eyeSquintRight", 0.34f * a);
            setArkit(out, "jawForward", 0.16f * a);
            break;
        case Expression::Surprised:
            setArkit(out, "eyeWideLeft", 0.90f * a);
            setArkit(out, "eyeWideRight", 0.90f * a);
            setArkit(out, "browInnerUp", 0.92f * a);
            setArkit(out, "browOuterUpLeft", 0.84f * a);
            setArkit(out, "browOuterUpRight", 0.84f * a);
            setArkit(out, "jawOpen", 0.55f * a);
            break;
        case Expression::Fear:
            setArkit(out, "eyeWideLeft", 0.68f * a);
            setArkit(out, "eyeWideRight", 0.68f * a);
            setArkit(out, "browInnerUp", 0.86f * a);
            setArkit(out, "browOuterUpLeft", 0.58f * a);
            setArkit(out, "browOuterUpRight", 0.58f * a);
            setArkit(out, "mouthStretchLeft", 0.46f * a);
            setArkit(out, "mouthStretchRight", 0.46f * a);
            setArkit(out, "jawOpen", 0.26f * a);
            break;
        case Expression::Disgust:
            setArkit(out, "noseSneerLeft", 0.80f * a);
            setArkit(out, "noseSneerRight", 0.80f * a);
            setArkit(out, "mouthUpperUpLeft", 0.54f * a);
            setArkit(out, "mouthUpperUpRight", 0.54f * a);
            setArkit(out, "browDownLeft", 0.36f * a);
            setArkit(out, "browDownRight", 0.36f * a);
            setArkit(out, "eyeSquintLeft", 0.30f * a);
            setArkit(out, "eyeSquintRight", 0.30f * a);
            setArkit(out, "cheekSquintLeft", 0.22f * a);
            setArkit(out, "cheekSquintRight", 0.22f * a);
            break;
        case Expression::COUNT:
            break;
    }
}

/*
 * Visemes as ARKit weights. The set covers the jaw and mouth channels only: brow, cheek and eye
 * channels are left at zero so that a viseme never fights an expression that is using them, and
 * FaceController::applyViseme merges with max() so the stronger of the two wins per channel.
 */
void visemeWeights(Viseme v, float amount, std::vector<float>* out) {
    if (!out) return;
    out->assign(static_cast<size_t>(kBlendshapeCount), 0.0f);
    const float a = saturate(std::isfinite(amount) ? amount : 0.0f);
    auto jaw = [&](float open, float close, float funnel, float pucker) {
        setArkit(out, "jawOpen", open * a);
        setArkit(out, "mouthClose", close * a);
        setArkit(out, "mouthFunnel", funnel * a);
        setArkit(out, "mouthPucker", pucker * a);
    };
    auto stretch = [&](float l, float r) {
        setArkit(out, "mouthStretchLeft", l * a);
        setArkit(out, "mouthStretchRight", r * a);
    };
    auto lowerDown = [&](float l, float r) {
        setArkit(out, "mouthLowerDownLeft", l * a);
        setArkit(out, "mouthLowerDownRight", r * a);
    };
    switch (v) {
        case Viseme::Silence: jaw(0.00f, 0.22f, 0.00f, 0.00f); break;
        case Viseme::PP:      jaw(0.00f, 0.86f, 0.00f, 0.00f);
                              setArkit(out, "mouthPressLeft", 0.30f * a);
                              setArkit(out, "mouthPressRight", 0.30f * a); break;
        case Viseme::FF:      jaw(0.09f, 0.00f, 0.00f, 0.00f);
                              setArkit(out, "mouthRollLower", 0.32f * a);
                              lowerDown(0.26f, 0.26f); break;
        case Viseme::TH:      jaw(0.13f, 0.00f, 0.00f, 0.00f);
                              setArkit(out, "tongueOut", 0.36f * a);
                              lowerDown(0.16f, 0.16f); break;
        case Viseme::DD:      jaw(0.29f, 0.00f, 0.00f, 0.00f);
                              setArkit(out, "tongueOut", 0.10f * a);
                              lowerDown(0.20f, 0.20f); break;
        case Viseme::KK:      jaw(0.33f, 0.00f, 0.00f, 0.00f);
                              setArkit(out, "tongueOut", 0.16f * a);
                              stretch(0.20f, 0.20f); break;
        case Viseme::CH:      jaw(0.18f, 0.00f, 0.22f, 0.46f); break;
        case Viseme::SS:      jaw(0.06f, 0.10f, 0.00f, 0.00f);
                              stretch(0.46f, 0.46f); break;
        case Viseme::NN:      jaw(0.23f, 0.00f, 0.00f, 0.00f);
                              setArkit(out, "tongueOut", 0.26f * a); break;
        case Viseme::RR:      jaw(0.21f, 0.00f, 0.06f, 0.36f);
                              setArkit(out, "tongueOut", 0.10f * a); break;
        case Viseme::AA:      jaw(0.76f, 0.00f, 0.00f, 0.00f);
                              lowerDown(0.36f, 0.36f); break;
        case Viseme::E:       jaw(0.36f, 0.00f, 0.00f, 0.00f);
                              stretch(0.46f, 0.46f); break;
        case Viseme::I:       jaw(0.18f, 0.00f, 0.00f, 0.00f);
                              stretch(0.62f, 0.62f); break;
        case Viseme::O:       jaw(0.52f, 0.00f, 0.46f, 0.40f); break;
        case Viseme::U:       jaw(0.22f, 0.00f, 0.36f, 0.76f); break;
        case Viseme::COUNT: break;
    }
}

/* =========================================================================== FaceController ==== */

namespace {

std::string lowerCopy(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) out.push_back((c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c);
    return out;
}

/** xorshift32: small, fast, and -- unlike a time-seeded RNG -- reproducible, which is what lets the
 *  host test assert an exact blink schedule instead of a statistical one. */
uint32_t nextRandom(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x ? x : 0x9E3779B9u;
    return *state;
}

float randomUnit(uint32_t* state) {
    return static_cast<float>(nextRandom(state) & 0xFFFFFFu) / 16777216.0f;
}

} /* namespace */

void FaceController::configure(int morphCount, const std::vector<std::string>& names) {
    const int n = std::max(0, morphCount);
    weights_.assign(static_cast<size_t>(n), 0.0f);
    names_.assign(static_cast<size_t>(n), std::string());
    for (int i = 0; i < n; ++i) {
        if (i < static_cast<int>(names.size())) names_[static_cast<size_t>(i)] = names[i];
    }
    arkitToMorph_.assign(static_cast<size_t>(kBlendshapeCount), -1);
    for (int a = 0; a < kBlendshapeCount; ++a) {
        const std::string target = lowerCopy(kBlendshapeNames[a]);
        for (int i = 0; i < n; ++i) {
            if (names_[static_cast<size_t>(i)].empty()) continue;
            if (lowerCopy(names_[static_cast<size_t>(i)]) == target) {
                arkitToMorph_[static_cast<size_t>(a)] = i;
                break;
            }
        }
    }
    unbound_.clear();
    blinkTimer_ = 0.0f;
    blinkPhase_ = 0.0f;
    blinking_ = false;
}

void FaceController::clear() {
    weights_.clear();
    names_.clear();
    arkitToMorph_.clear();
    unbound_.clear();
    blinkEnabled_ = false;
    blinking_ = false;
    blinkPhase_ = 0.0f;
    lipSyncEnabled_ = false;
}

bool FaceController::hasNamedTargets() const {
    for (const auto& n : names_) if (!n.empty()) return true;
    return false;
}

int FaceController::indexOf(const std::string& name) const {
    const std::string target = lowerCopy(name);
    for (size_t i = 0; i < names_.size(); ++i) {
        if (names_[i].empty()) continue;
        if (lowerCopy(names_[i]) == target) return static_cast<int>(i);
    }
    return -1;
}

void FaceController::noteUnbound(const std::string& name) {
    if (name.empty()) return;
    for (const auto& existing : unbound_) if (existing == name) return;
    unbound_.push_back(name);
}

bool FaceController::setWeight(const std::string& name, float w) {
    const int i = indexOf(name);
    if (i < 0) { noteUnbound(name); return false; }
    weights_[static_cast<size_t>(i)] = saturate(std::isfinite(w) ? w : 0.0f);
    return true;
}

bool FaceController::setWeightAt(int index, float w) {
    if (index < 0 || index >= static_cast<int>(weights_.size())) return false;
    weights_[static_cast<size_t>(index)] = saturate(std::isfinite(w) ? w : 0.0f);
    return true;
}

float FaceController::weightAt(int index) const {
    if (index < 0 || index >= static_cast<int>(weights_.size())) return 0.0f;
    return weights_[static_cast<size_t>(index)];
}

float FaceController::weightOf(const std::string& name) const {
    return weightAt(indexOf(name));
}

int FaceController::applyArkitWeights(const std::vector<float>& arkit, float amount) {
    const float a = saturate(std::isfinite(amount) ? amount : 0.0f);
    int written = 0;
    const int n = std::min(static_cast<int>(arkit.size()), kBlendshapeCount);
    for (int c = 0; c < n; ++c) {
        const float w = arkit[static_cast<size_t>(c)] * a;
        const int morph = (c < static_cast<int>(arkitToMorph_.size()))
                              ? arkitToMorph_[static_cast<size_t>(c)] : -1;
        if (morph < 0 || morph >= static_cast<int>(weights_.size())) {
            if (std::fabs(w) > 1e-4f) noteUnbound(kBlendshapeNames[c]);
            continue;
        }
        weights_[static_cast<size_t>(morph)] = saturate(w);
        ++written;
    }
    return written;
}

int FaceController::applyExpression(Expression e, float amount) {
    /* thread_local scratch: an expression is evaluated once per frame, and a 52-float heap
       allocation per frame on the render thread is exactly what this module's allocation policy
       forbids. expressionWeights() assigns into it, reusing the capacity after the first call. */
    thread_local std::vector<float> arkit;
    expressionWeights(e, amount, &arkit);
    return applyArkitWeights(arkit, 1.0f);
}

int FaceController::applyViseme(Viseme v, float amount) {
    thread_local std::vector<float> arkit;      /* see applyExpression: no per-frame allocation */
    visemeWeights(v, amount, &arkit);
    /* Merge with max() rather than overwriting: a viseme should open the jaw without erasing a
       smile that is already on the mouth corners, and taking the stronger of the two per channel is
       the standard way real-time rigs layer speech over expression. */
    int written = 0;
    const int n = std::min(static_cast<int>(arkit.size()), kBlendshapeCount);
    for (int c = 0; c < n; ++c) {
        const float w = arkit[static_cast<size_t>(c)];
        const int morph = (c < static_cast<int>(arkitToMorph_.size()))
                              ? arkitToMorph_[static_cast<size_t>(c)] : -1;
        if (morph < 0 || morph >= static_cast<int>(weights_.size())) {
            if (w > 1e-4f) noteUnbound(kBlendshapeNames[c]);
            continue;
        }
        float& slot = weights_[static_cast<size_t>(morph)];
        slot = saturate(w > slot ? w : slot);
        ++written;
    }
    return written;
}

void FaceController::setAll(float w) {
    const float v = saturate(std::isfinite(w) ? w : 0.0f);
    std::fill(weights_.begin(), weights_.end(), v);
}

void FaceController::resetWeights() {
    std::fill(weights_.begin(), weights_.end(), 0.0f);
    blinkPhase_ = 0.0f;
    blinking_ = false;
}

void FaceController::setBlinkEnabled(bool enabled) {
    blinkEnabled_ = enabled;
    if (!enabled) {
        blinking_ = false;
        blinkPhase_ = 0.0f;
        const int l = arkitIndex("eyeBlinkLeft");
        const int r = arkitIndex("eyeBlinkRight");
        if (l >= 0 && l < static_cast<int>(arkitToMorph_.size()) && arkitToMorph_[static_cast<size_t>(l)] >= 0) {
            weights_[static_cast<size_t>(arkitToMorph_[static_cast<size_t>(l)])] = 0.0f;
        }
        if (r >= 0 && r < static_cast<int>(arkitToMorph_.size()) && arkitToMorph_[static_cast<size_t>(r)] >= 0) {
            weights_[static_cast<size_t>(arkitToMorph_[static_cast<size_t>(r)])] = 0.0f;
        }
    }
}

void FaceController::setBlinkInterval(float minSeconds, float maxSeconds) {
    const float lo = std::max(0.05f, std::isfinite(minSeconds) ? minSeconds : 0.05f);
    const float hi = std::max(lo, std::isfinite(maxSeconds) ? maxSeconds : lo);
    blinkInterval_ = 0.5f * (lo + hi);
    blinkTimer_ = 0.0f;
    blinkNext_ = 0.0f;        /* dirty: the new interval applies to the very next blink */
}

void FaceController::setBlinkDuration(float seconds) {
    blinkDuration_ = std::isfinite(seconds) ? std::max(0.02f, seconds) : 0.14f;
}

/* Draw how long until the next blink. Called once per blink rather than once per frame: see
   updateBlink() for why that distinction is the whole point. */
void FaceController::drawNextBlink() {
    blinkNext_ = blinkInterval_ * (0.6f + 0.8f * randomUnit(&rng_));
    if (!(blinkNext_ > 0.0f)) blinkNext_ = blinkInterval_ > 0.0f ? blinkInterval_ : 3.5f;
}

void FaceController::setBlinkSeed(uint32_t seed) {
    rng_ = seed ? seed : 0x9E3779B9u;
    /* Restart the schedule too, so the seed actually determines the timeline. Weights are left as
       they are -- resetWeights() is the call that opens the eyes. */
    blinkTimer_ = 0.0f;
    blinkPhase_ = 0.0f;
    blinking_ = false;
    blinkNext_ = 0.0f;        /* dirty: redrawn on the next update, from this seed and interval */
}

void FaceController::triggerBlink() {
    blinking_ = true;
    blinkPhase_ = 0.0f;
}

bool FaceController::updateBlink(float dt) {
    if (!blinkEnabled_) return false;
    const int l = arkitIndex("eyeBlinkLeft");
    const int r = arkitIndex("eyeBlinkRight");
    const int lm = (l >= 0 && l < static_cast<int>(arkitToMorph_.size())) ? arkitToMorph_[static_cast<size_t>(l)] : -1;
    const int rm = (r >= 0 && r < static_cast<int>(arkitToMorph_.size())) ? arkitToMorph_[static_cast<size_t>(r)] : -1;
    if (lm < 0 && rm < 0) {
        /* No eye-blink target on this asset. The blink clock still runs so blinkPhase() is
           meaningful for a caller driving its own target, but nothing here is claimed to move. */
        noteUnbound("eyeBlinkLeft");
    }
    const float step = std::isfinite(dt) ? std::max(0.0f, dt) : 0.0f;
    if (!blinking_) {
        /* The interval is drawn ONCE per blink and then waited out. An earlier version redrew the
           threshold every frame and tested the accumulated time against it, which had two effects
           that both matter: it burned an RNG draw per frame per character for no reason, and because
           the timer only had to beat the SMALLEST threshold the dice ever produced, the realised rate
           was well above the configured one -- setBlinkInterval(2, 3) delivered about 16 blinks in
           30 s instead of about 12. Waiting out a single draw makes the setting mean what it says. */
        blinkTimer_ += step;
        /* Drawn here rather than in the setters: drawing eagerly meant the number of RNG draws a
           controller had consumed depended on the order the caller happened to set the seed and the
           interval in, so two controllers configured identically but in a different order blinked
           differently. Lazily drawing on the first update removes that order dependence. */
        if (!(blinkNext_ > 0.0f)) drawNextBlink();
        if (blinkTimer_ >= blinkNext_) {
            blinkTimer_ = 0.0f;
            blinking_ = true;
            blinkPhase_ = 0.0f;
        }
    }
    if (blinking_) {
        blinkPhase_ += step / std::max(1e-4f, blinkDuration_);
        if (blinkPhase_ >= 1.0f) {
            blinkPhase_ = 0.0f;
            blinking_ = false;
            blinkNext_ = 0.0f;        /* schedule the next one from the same seed stream */
        }
    }
    /* Triangle: close over the first half, open over the second. */
    const float closed = blinking_ ? (1.0f - std::fabs(2.0f * blinkPhase_ - 1.0f)) : 0.0f;
    if (lm >= 0 && lm < static_cast<int>(weights_.size())) weights_[static_cast<size_t>(lm)] = saturate(closed);
    if (rm >= 0 && rm < static_cast<int>(weights_.size())) weights_[static_cast<size_t>(rm)] = saturate(closed);
    return blinking_;
}

void FaceController::setLipSyncEnabled(bool enabled) { lipSyncEnabled_ = enabled; }

/* ============================================================================ LipSyncTrack ===== */

void LipSyncTrack::buildFromKeys(std::vector<VisemeKey> keys) {
    keys_ = std::move(keys);
    std::stable_sort(keys_.begin(), keys_.end(),
                     [](const VisemeKey& a, const VisemeKey& b) { return a.time < b.time; });
    duration_ = keys_.empty() ? 0.0f : keys_.back().time;
}

void LipSyncTrack::buildFromText(const std::string& text, float secondsPerViseme) {
    const float step = std::isfinite(secondsPerViseme) ? std::max(0.01f, secondsPerViseme) : 0.11f;
    std::vector<VisemeKey> keys;
    float t = 0.0f;
    for (char c : text) {
        const Viseme v = visemeForChar(c);
        keys.push_back({t, v, 1.0f});
        /* A gap between words is a real silence and gets twice the duration, so the mouth closes
           at a word boundary instead of running consonants together. */
        t += (v == Viseme::Silence) ? step * 2.0f : step;
    }
    if (!keys.empty()) keys.push_back({t, Viseme::Silence, 1.0f});
    buildFromKeys(std::move(keys));
}

void LipSyncTrack::buildForDuration(const std::string& text, float totalSeconds) {
    std::vector<char> spoken;
    spoken.reserve(text.size());
    for (char c : text) {
        if (visemeForChar(c) != Viseme::Silence) spoken.push_back(c);
    }
    if (spoken.empty() || !(totalSeconds > 0.0f) || !std::isfinite(totalSeconds)) {
        std::vector<VisemeKey> none;
        buildFromKeys(std::move(none));
        return;
    }
    /* Spread the visemes evenly across the requested duration. This is the honest shape of a
       text-to-speech lip-sync where the synthesiser's real per-phoneme timing is not observable:
       evenly spaced, and labelled as such by every caller. */
    const float step = totalSeconds / static_cast<float>(spoken.size());
    std::vector<VisemeKey> keys;
    keys.reserve(spoken.size() + 1);
    float t = 0.0f;
    for (char c : spoken) {
        keys.push_back({t, visemeForChar(c), 1.0f});
        t += step;
    }
    keys.push_back({totalSeconds, Viseme::Silence, 1.0f});
    buildFromKeys(std::move(keys));
}

void LipSyncTrack::clear() {
    keys_.clear();
    duration_ = 0.0f;
}

Viseme LipSyncTrack::sample(float t, float* outAmount) const {
    if (outAmount) *outAmount = 0.0f;
    if (keys_.empty()) return Viseme::Silence;
    if (!(t >= keys_.front().time)) return Viseme::Silence;
    /* Hold each key until the next one: a viseme is a mouth shape sustained over an interval, not an
       instantaneous event, so interpolating between two shapes would smear consonants. */
    size_t idx = 0;
    for (size_t i = 0; i < keys_.size(); ++i) {
        if (keys_[i].time <= t) idx = i;
        else break;
    }
    if (outAmount) *outAmount = saturate(keys_[idx].amount);
    return keys_[idx].viseme;
}

} /* namespace anim */
} /* namespace af */
