/*
 * avatarforge_animation_jni.cpp -- the JNI bridge to af::anim.
 *
 * This is the seam between the renderer-agnostic animation core and Android. It owns no animation
 * logic of its own: every call forwards to af::anim::Player, which is the same core the host test
 * suite exercises and the same core the GLES engine backend uses. That is deliberate. A second state
 * machine or a second viseme table written in Kotlin would be a second animation engine, and the two
 * would drift.
 *
 * What the bridge does own is lifetime and marshalling:
 *   - a session handle is a heap AnimationSession, validated by magic number before every use, so a
 *     stale Kotlin handle returns an error instead of dereferencing freed memory;
 *   - the imported GlbAsset is released as soon as the Player has copied what it needs, because the
 *     app holds a 128 MB ceiling on imported assets and a Fox.glb decode is not free;
 *   - the per-frame call writes into arrays the CALLER allocated. Allocating a Java array 60 times a
 *     second on the render thread is how an animation system ends up stuttering, so nativeAdvance
 *     takes the output buffers as arguments and returns only a status.
 *
 * Threading: a session is not internally synchronised. The Kotlin side confines each session to the
 * render thread (Choreographer callback) and treats the handle as a value, which is the same
 * ownership rule the Filament Engine itself uses.
 */
#include <jni.h>
#include <android/log.h>

#include "anim/af_anim.h"
#include "af_glb_read.h"
#include "afcore.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#define AF_ANIM_LOG_TAG "AvatarForgeAnim"

namespace {

using af::anim::Category;
using af::anim::Clip;
using af::anim::Expression;
using af::anim::Player;
using af::anim::RetargetMap;
using af::anim::Skeleton;

constexpr uint32_t kSessionMagic = 0x41464E49u;   /* "AFNI" */

/* One animation session: a Player, the library built from what it loaded, and the last error. */
struct AnimationSession {
    uint32_t magic = kSessionMagic;
    std::unique_ptr<Player> player;
    std::unique_ptr<af::anim::AnimationLibrary> library;
    std::string lastError;
    std::string source;
    std::string licence;
    /* Events fired during the most recent advance(), drained by nativeDrainEvents. Kept here rather
       than pushed through a callback so the render thread never re-enters the JVM. */
    std::vector<std::string> pendingEvents;
    std::vector<float> pendingEventValues;

    AnimationSession()
        : player(new Player()), library(new af::anim::AnimationLibrary()) {
        player->setEventListener([this](const std::string& name, float value, float /*time*/) {
            pendingEvents.push_back(name);
            pendingEventValues.push_back(value);
        });
    }
};

AnimationSession* session(jlong handle) {
    if (handle == 0) return nullptr;
    auto* s = reinterpret_cast<AnimationSession*>(static_cast<uintptr_t>(handle));
    /* A stale or corrupt handle must not be dereferenced into a wild pointer. Checking the magic
       catches the common cases: a session already destroyed, and a handle from another type. */
    return (s->magic == kSessionMagic) ? s : nullptr;
}

jstring toJString(JNIEnv* env, const std::string& s) {
    return env->NewStringUTF(s.c_str());
}

jstring toJStringOrNull(JNIEnv* env, const char* s) {
    return s ? env->NewStringUTF(s) : nullptr;
}

std::string fromJString(JNIEnv* env, jstring s) {
    if (!s) return std::string();
    const char* chars = env->GetStringUTFChars(s, nullptr);
    if (!chars) return std::string();
    std::string out(chars);
    env->ReleaseStringUTFChars(s, chars);
    return out;
}

void fail(AnimationSession* s, const std::string& msg) {
    if (s) s->lastError = msg;
    __android_log_print(ANDROID_LOG_WARN, AF_ANIM_LOG_TAG, "%s", msg.c_str());
}

/* Releases the decoded asset once the Player has taken what it needs. Skeleton::fromGlb and
   Clip::fromGlb both COPY (names, rest transforms, inverse bind matrices, keyframe times and values),
   so holding the GlbAsset afterwards would keep every decoded texture and buffer view alive for the
   life of the session -- on a 438 KB CesiumMan that is mostly an embedded PNG nobody will draw. */
void releaseAsset(af::GlbAsset* asset) {
    for (auto& pool : asset->pools) { std::vector<af_vertex>().swap(pool); }
    asset->pools.clear();
    for (auto& prim : asset->primitives) std::vector<uint32_t>().swap(prim.indices);
    asset->primitives.clear();
    for (auto& tex : asset->textures) { tex.rgba.clear(); tex.rgba.shrink_to_fit(); }
    asset->textures.clear();
    asset->nodes.clear();
    asset->skins.clear();
    asset->animations.clear();
    asset->meshes.clear();
    asset->materials.clear();
    asset->warnings.clear();
    asset->sceneRoots.clear();
    asset->extensionsUsed.clear();
}

} /* namespace */

extern "C" {

/* ================================================================ lifetime ================== */

JNIEXPORT jlong JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeCreate(JNIEnv*, jobject) {
    auto* s = new (std::nothrow) AnimationSession();
    if (!s) return 0;
    return static_cast<jlong>(reinterpret_cast<uintptr_t>(s));
}

JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeDestroy(JNIEnv*, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    if (!s) return;
    s->magic = 0;              /* invalidate before freeing, so a racing call sees a dead handle */
    delete s;
}

JNIEXPORT jstring JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeLastError(JNIEnv* env, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    return toJString(env, s ? s->lastError : std::string("animation session handle is not valid"));
}

JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeIsAlive(JNIEnv*, jobject, jlong handle) {
    return session(handle) != nullptr ? JNI_TRUE : JNI_FALSE;
}

/* ================================================================ loading =================== */

JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeLoadGlb(
        JNIEnv* env, jobject, jlong handle, jbyteArray bytes, jstring sourceLabel, jstring licence) {
    AnimationSession* s = session(handle);
    if (!s) return -1;
    if (!bytes) { fail(s, "loadGlb: no bytes"); return -2; }
    const jsize len = env->GetArrayLength(bytes);
    if (len <= 0) { fail(s, "loadGlb: empty asset"); return -3; }

    std::vector<uint8_t> buffer(static_cast<size_t>(len));
    env->GetByteArrayRegion(bytes, 0, len, reinterpret_cast<jbyte*>(buffer.data()));
    if (env->ExceptionCheck()) { env->ExceptionClear(); fail(s, "loadGlb: failed to read the byte array"); return -4; }

    af::GlbAsset asset;
    std::string importError;
    const int rc = af::importGlb(buffer.data(), buffer.size(), &asset, &importError);
    if (rc != AF_OK) {
        fail(s, "importGlb failed: " + (importError.empty() ? std::string("unknown error") : importError));
        return -5;
    }

    std::string loadError;
    if (!s->player->loadGlb(asset, &loadError)) {
        fail(s, "the animation core refused this asset: " + loadError);
        releaseAsset(&asset);
        return -6;
    }

    s->source = fromJString(env, sourceLabel);
    s->licence = fromJString(env, licence);

    /* The library is built from what actually loaded, never from a fixed list, so the picker cannot
       offer a category this character has no clip for. */
    s->library->clear();
    s->library->addFromPlayer(*s->player, s->source.empty() ? std::string("imported GLB") : s->source,
                              s->licence);
    releaseAsset(&asset);
    return 0;
}

/* Loads the same asset from a path instead of a Java byte array.

   nativeLoadGlb copies the entire array into a std::vector before parsing, so a generated avatar of
   79.77 MiB exists twice at once: once on the Dalvik heap, which the CI emulator caps at a 192 MiB
   growth limit, and once here. af::importGlbFile was already in the engine with the same contract as
   importGlb and nothing called it. Reading the file in C++ removes the second copy entirely and never
   touches the Java heap, which is the difference between an avatar that loads and an
   OutOfMemoryError. Return codes mirror nativeLoadGlb so the Kotlin side branches on one contract. */
JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeLoadGlbFile(
        JNIEnv* env, jobject, jlong handle, jstring path, jstring sourceLabel, jstring licence) {
    AnimationSession* s = session(handle);
    if (!s) return -1;
    if (!path) { fail(s, "loadGlbFile: no path"); return -2; }
    const std::string absolutePath = fromJString(env, path);
    if (absolutePath.empty()) { fail(s, "loadGlbFile: empty path"); return -3; }

    af::GlbAsset asset;
    std::string importError;
    const int rc = af::importGlbFile(absolutePath.c_str(), &asset, &importError);
    if (rc != AF_OK) {
        fail(s, "importGlbFile failed: " +
                (importError.empty() ? std::string("unknown error") : importError));
        return -5;
    }

    std::string loadError;
    if (!s->player->loadGlb(asset, &loadError)) {
        fail(s, "the animation core refused this asset: " + loadError);
        releaseAsset(&asset);
        return -6;
    }

    s->source = fromJString(env, sourceLabel);
    s->licence = fromJString(env, licence);

    /* Built from what actually loaded, exactly as nativeLoadGlb does, so the two entry points cannot
       drift into offering different clip lists for the same file. */
    s->library->clear();
    s->library->addFromPlayer(*s->player, s->source.empty() ? std::string("imported GLB") : s->source,
                              s->licence);
    releaseAsset(&asset);
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeLoadAuthoredLibrary(
        JNIEnv* env, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    if (!s) return -1;
    /* The 16 authored clips need no asset and no download; they bind to the engine's generated rig,
       so this only makes sense once a rig is attached. The library entries are added regardless,
       because they are real clips the engine ships -- but each is marked unbound (clipIndex -1) until
       a Player has a skeleton that can carry it. */
    s->library->addAuthoredLibrary();
    (void)env;
    return static_cast<jint>(s->library->count());
}

JNIEXPORT jobjectArray JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeLoadNotes(JNIEnv* env, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    const jclass stringClass = env->FindClass("java/lang/String");
    static const std::vector<std::string> kNoNotes;
    /* A dead handle returns an empty array rather than leaking a vector to satisfy a reference
       binding. Load notes are diagnostics; nothing is worth a leak per call. */
    const std::vector<std::string>& notes = s ? s->player->loadNotes() : kNoNotes;
    jobjectArray out = env->NewObjectArray(static_cast<jsize>(notes.size()), stringClass, nullptr);
    for (size_t i = 0; i < notes.size(); ++i) {
        env->SetObjectArrayElement(out, static_cast<jsize>(i), toJString(env, notes[i]));
    }
    return out;
}

/* ================================================================ introspection ============= */

JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeClipCount(JNIEnv*, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    return s ? s->player->clipCount() : 0;
}

JNIEXPORT jstring JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeClipName(JNIEnv* env, jobject, jlong handle, jint index) {
    AnimationSession* s = session(handle);
    const Clip* c = s ? s->player->clipAt(index) : nullptr;
    return c ? toJString(env, c->name) : nullptr;
}

JNIEXPORT jfloat JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeClipDuration(JNIEnv*, jobject, jlong handle, jint index) {
    AnimationSession* s = session(handle);
    const Clip* c = s ? s->player->clipAt(index) : nullptr;
    return c ? static_cast<jfloat>(c->duration) : 0.0f;
}

JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeClipCategory(JNIEnv*, jobject, jlong handle, jint index) {
    AnimationSession* s = session(handle);
    const Clip* c = s ? s->player->clipAt(index) : nullptr;
    return c ? static_cast<jint>(c->category) : static_cast<jint>(Category::Uncategorized);
}

JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeClipLooping(JNIEnv*, jobject, jlong handle, jint index) {
    AnimationSession* s = session(handle);
    const Clip* c = s ? s->player->clipAt(index) : nullptr;
    return (c && c->looping) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeClipChannelCount(JNIEnv*, jobject, jlong handle, jint index) {
    AnimationSession* s = session(handle);
    const Clip* c = s ? s->player->clipAt(index) : nullptr;
    return c ? static_cast<jint>(c->channels.size()) : 0;
}

JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeClipHasRootMotion(JNIEnv*, jobject, jlong handle, jint index) {
    AnimationSession* s = session(handle);
    const Clip* c = s ? s->player->clipAt(index) : nullptr;
    return (c && c->hasRootTranslation) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeClipIndex(JNIEnv* env, jobject, jlong handle, jstring name) {
    AnimationSession* s = session(handle);
    return s ? s->player->clipIndex(fromJString(env, name)) : -1;
}

JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeJointCount(JNIEnv*, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    return s ? s->player->skeleton().count() : 0;
}

JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeSkinSlotCount(JNIEnv*, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    return s ? s->player->skeleton().skinSlotCount() : 0;
}

JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeRolesMatched(JNIEnv*, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    return s ? s->player->skeleton().rolesMatched() : 0;
}

JNIEXPORT jstring JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeJointName(JNIEnv* env, jobject, jlong handle, jint index) {
    AnimationSession* s = session(handle);
    if (!s) return nullptr;
    const Skeleton& sk = s->player->skeleton();
    if (index < 0 || index >= sk.count()) return nullptr;
    return toJString(env, sk.names[static_cast<size_t>(index)]);
}

JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeMorphCount(JNIEnv*, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    return s ? s->player->morphCount() : 0;
}

JNIEXPORT jstring JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeMorphName(JNIEnv* env, jobject, jlong handle, jint index) {
    AnimationSession* s = session(handle);
    if (!s) return nullptr;
    const std::vector<std::string>& names = s->player->morphNames();
    if (index < 0 || index >= static_cast<jint>(names.size())) return nullptr;
    return toJString(env, names[static_cast<size_t>(index)]);
}

/* ================================================================ transport ================= */

JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativePlayIndex(JNIEnv*, jobject, jlong handle, jint index) {
    AnimationSession* s = session(handle);
    if (!s) return JNI_FALSE;
    const bool ok = s->player->play(static_cast<int>(index));
    if (!ok) fail(s, "play: no clip at index " + std::to_string(index));
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativePlay(JNIEnv* env, jobject, jlong handle, jstring name) {
    AnimationSession* s = session(handle);
    if (!s) return JNI_FALSE;
    const std::string clip = fromJString(env, name);
    const bool ok = s->player->play(clip);
    if (!ok) fail(s, "play: no clip named '" + clip + "'");
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativePause(JNIEnv*, jobject, jlong handle) {
    if (AnimationSession* s = session(handle)) s->player->pause();
}

JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeResume(JNIEnv*, jobject, jlong handle) {
    if (AnimationSession* s = session(handle)) s->player->resume();
}

JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeStop(JNIEnv*, jobject, jlong handle) {
    if (AnimationSession* s = session(handle)) s->player->stop();
}

JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeSeek(JNIEnv*, jobject, jlong handle, jfloat seconds) {
    if (AnimationSession* s = session(handle)) s->player->seek(seconds);
}

JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeSetSpeed(JNIEnv*, jobject, jlong handle, jfloat speed) {
    AnimationSession* s = session(handle);
    if (!s) return JNI_FALSE;
    const float before = s->player->speed();
    s->player->setSpeed(speed);
    /* setSpeed refuses a negative or non-finite speed rather than applying it, so the caller learns
       the truth by comparing what it asked for with what the player now has. */
    return (s->player->speed() != before || std::fabs(before - speed) < 1e-6f) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jfloat JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeSpeed(JNIEnv*, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    return s ? s->player->speed() : 0.0f;
}

JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeSetLoop(JNIEnv*, jobject, jlong handle, jboolean loop) {
    if (AnimationSession* s = session(handle)) s->player->setLoop(loop == JNI_TRUE);
}

JNIEXPORT jfloat JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativePosition(JNIEnv*, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    return s ? s->player->position() : 0.0f;
}

JNIEXPORT jfloat JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeCurrentDuration(JNIEnv*, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    return s ? s->player->currentDuration() : 0.0f;
}

JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeIsPlaying(JNIEnv*, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    return (s && s->player->isPlaying()) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeCurrentClip(JNIEnv*, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    return s ? s->player->currentClip() : -1;
}

/* ================================================================ blending ================== */

JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeCrossfadeToIndex(
        JNIEnv*, jobject, jlong handle, jint index, jfloat seconds) {
    AnimationSession* s = session(handle);
    if (!s) return JNI_FALSE;
    const bool ok = s->player->crossfadeTo(static_cast<int>(index), seconds);
    if (!ok) fail(s, "crossfadeTo: no clip at index " + std::to_string(index));
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeCrossfadeTo(
        JNIEnv* env, jobject, jlong handle, jstring name, jfloat seconds) {
    AnimationSession* s = session(handle);
    if (!s) return JNI_FALSE;
    const std::string clip = fromJString(env, name);
    const bool ok = s->player->crossfadeTo(clip, seconds);
    if (!ok) fail(s, "crossfadeTo: no clip named '" + clip + "'");
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeSetCrossfadeEnabled(
        JNIEnv*, jobject, jlong handle, jboolean enabled) {
    if (AnimationSession* s = session(handle)) s->player->setCrossfadeEnabled(enabled == JNI_TRUE);
}

JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeBuildStateMachine(
        JNIEnv*, jobject, jlong handle, jfloat crossfadeSeconds, jboolean allToAll) {
    AnimationSession* s = session(handle);
    if (!s) return -1;
    if (!s->player->buildStateMachine(crossfadeSeconds)) {
        fail(s, "buildStateMachine: the player has no clips to build states from");
        return -1;
    }
    if (allToAll) s->player->stateMachine().addAllTransitions(crossfadeSeconds);
    return s->player->stateMachine().stateCount();
}

JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeAddTransition(
        JNIEnv* env, jobject, jlong handle, jstring from, jstring to, jfloat seconds) {
    AnimationSession* s = session(handle);
    if (!s) return JNI_FALSE;
    const bool ok = s->player->stateMachine().addTransition(fromJString(env, from), fromJString(env, to), seconds);
    if (!ok) fail(s, "addTransition: unknown state name");
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeSetStateMachineDriven(
        JNIEnv*, jobject, jlong handle, jboolean driven) {
    if (AnimationSession* s = session(handle)) s->player->setStateMachineDriven(driven == JNI_TRUE);
}

JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeRequestState(
        JNIEnv* env, jobject, jlong handle, jstring name) {
    AnimationSession* s = session(handle);
    if (!s) return JNI_FALSE;
    const std::string state = fromJString(env, name);
    const bool ok = s->player->requestState(state);
    /* requestState refuses an undeclared edge instead of snapping. That refusal has to reach the UI,
       or a button that does nothing looks like a bug rather than a rule. */
    if (!ok) fail(s, "requestState: '" + state + "' is not a declared state, or no transition to it "
                     "was declared from the current one");
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeCurrentStateName(JNIEnv* env, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    if (!s) return nullptr;
    return toJString(env, s->player->stateMachine().currentStateName());
}

/* ---- layers ---- */

JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeAddLayer(
        JNIEnv*, jobject, jlong handle, jint clipIndex, jfloat weight, jboolean additive) {
    AnimationSession* s = session(handle);
    if (!s) return -1;
    const int layer = s->player->addLayer(clipIndex, weight, additive == JNI_TRUE);
    if (layer < 0) fail(s, "addLayer: no clip at index " + std::to_string(clipIndex));
    return layer;
}

JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeLayerCount(JNIEnv*, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    return s ? s->player->layerCount() : 0;
}

JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeSetLayerClip(
        JNIEnv*, jobject, jlong handle, jint layer, jint clipIndex) {
    AnimationSession* s = session(handle);
    return (s && s->player->setLayerClip(layer, clipIndex)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeSetLayerWeight(
        JNIEnv*, jobject, jlong handle, jint layer, jfloat weight) {
    AnimationSession* s = session(handle);
    return (s && s->player->setLayerWeight(layer, weight)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeSetLayerEnabled(
        JNIEnv*, jobject, jlong handle, jint layer, jboolean enabled) {
    AnimationSession* s = session(handle);
    return (s && s->player->setLayerEnabled(layer, enabled == JNI_TRUE)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeSetLayerAdditive(
        JNIEnv*, jobject, jlong handle, jint layer, jboolean additive) {
    AnimationSession* s = session(handle);
    return (s && s->player->setLayerAdditive(layer, additive == JNI_TRUE)) ? JNI_TRUE : JNI_FALSE;
}

/* which: 0 = upper body, 1 = lower body, 2 = everything. Masks come from the skeleton's own roles,
   so "upper body" means this character's arms, spine and head rather than a hardcoded joint range. */
JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeSetLayerMask(
        JNIEnv*, jobject, jlong handle, jint layer, jint which) {
    AnimationSession* s = session(handle);
    if (!s) return -1;
    const Skeleton& sk = s->player->skeleton();
    af::anim::BoneMask mask = (which == 1) ? af::anim::BoneMask::lowerBody(sk)
                            : (which == 2) ? af::anim::BoneMask::fromRole(sk, af::anim::Role::Hips, true)
                                           : af::anim::BoneMask::upperBody(sk);
    if (!s->player->setLayerMask(layer, mask)) return -1;
    return mask.includedCount();
}

JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeAddEvent(
        JNIEnv* env, jobject, jlong handle, jint clipIndex, jfloat time, jstring name, jfloat value) {
    AnimationSession* s = session(handle);
    if (!s) return JNI_FALSE;
    const bool ok = s->player->addEvent(clipIndex, time, fromJString(env, name), value);
    if (!ok) fail(s, "addEvent: no clip at index " + std::to_string(clipIndex));
    return ok ? JNI_TRUE : JNI_FALSE;
}

/* ================================================================ face / lip-sync =========== */

JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeSetExpression(
        JNIEnv*, jobject, jlong handle, jint expressionId, jfloat amount) {
    AnimationSession* s = session(handle);
    if (!s) return -1;
    if (expressionId < 0 || expressionId >= static_cast<jint>(Expression::COUNT)) return -1;
    return s->player->face().applyExpression(static_cast<Expression>(expressionId), amount);
}

JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeSetMorphWeight(
        JNIEnv* env, jobject, jlong handle, jstring name, jfloat value) {
    AnimationSession* s = session(handle);
    if (!s) return JNI_FALSE;
    const bool ok = s->player->face().setWeight(fromJString(env, name), value);
    /* A refused channel is reported by the face controller itself, so the UI can say "this character
       has no jawOpen" instead of silently doing nothing. */
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeSetMorphWeightAt(
        JNIEnv*, jobject, jlong handle, jint index, jfloat value) {
    AnimationSession* s = session(handle);
    return (s && s->player->face().setWeightAt(index, value)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jfloat JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeMorphWeightAt(JNIEnv*, jobject, jlong handle, jint index) {
    AnimationSession* s = session(handle);
    return s ? s->player->face().weightAt(index) : 0.0f;
}

JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeResetFace(JNIEnv*, jobject, jlong handle) {
    if (AnimationSession* s = session(handle)) s->player->face().resetWeights();
}

JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeSetBlinkEnabled(
        JNIEnv*, jobject, jlong handle, jboolean enabled) {
    if (AnimationSession* s = session(handle)) s->player->face().setBlinkEnabled(enabled == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeSetBlinkInterval(
        JNIEnv*, jobject, jlong handle, jfloat minSeconds, jfloat maxSeconds) {
    if (AnimationSession* s = session(handle)) s->player->face().setBlinkInterval(minSeconds, maxSeconds);
}

/* Starts a blink now instead of waiting for the schedule. void in the core: there is nothing to
   refuse, since a face with no eyelid targets simply records the request as unbound. */
JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeTriggerBlink(JNIEnv*, jobject, jlong handle) {
    if (AnimationSession* s = session(handle)) s->player->face().triggerBlink();
}

JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeSpeak(
        JNIEnv* env, jobject, jlong handle, jstring text, jfloat totalSeconds) {
    AnimationSession* s = session(handle);
    if (!s) return -1;
    const std::string utterance = fromJString(env, text);
    s->player->speak(utterance, totalSeconds);
    const int keys = s->player->lipSync().keyCount();
    /* An empty utterance or a negative duration produces no keys. Reporting 0 rather than "ok" is the
       difference between a lip-sync control that works and one that pretends to. */
    if (keys == 0) fail(s, "speak: '" + utterance + "' produced no viseme keys");
    return keys;
}

JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeStopSpeaking(JNIEnv*, jobject, jlong handle) {
    if (AnimationSession* s = session(handle)) s->player->stopSpeaking();
}

JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeIsSpeaking(JNIEnv*, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    return (s && s->player->isSpeaking()) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jobjectArray JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeUnboundFaceChannels(JNIEnv* env, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    const jclass stringClass = env->FindClass("java/lang/String");
    static const std::vector<std::string> kNoChannels;
    const std::vector<std::string>& unbound = s ? s->player->face().unbound() : kNoChannels;
    jobjectArray out = env->NewObjectArray(static_cast<jsize>(unbound.size()), stringClass, nullptr);
    for (size_t i = 0; i < unbound.size(); ++i) {
        env->SetObjectArrayElement(out, static_cast<jsize>(i), toJString(env, unbound[i]));
    }
    return out;
}

/* ================================================================ retargeting =============== */

/* Returns [compatible(0/1), coverage, scale, mappedJoints, dstJoints] plus the textual report via
   nativeRetargetReport, so the UI can show WHY two characters are or are not compatible. */
JNIEXPORT jfloatArray JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeCheckCompatibility(
        JNIEnv* env, jobject, jlong handle, jlong otherHandle) {
    AnimationSession* dst = session(handle);
    AnimationSession* src = session(otherHandle);
    jfloatArray out = env->NewFloatArray(5);
    jfloat values[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    if (dst && src) {
        RetargetMap report;
        const bool ok = dst->player->checkCompatibility(src->player->skeleton(), &report);
        values[0] = (ok && report.compatible) ? 1.0f : 0.0f;
        values[1] = report.coverage;
        values[2] = report.scale;
        values[3] = static_cast<jfloat>(report.mappedJoints());
        values[4] = static_cast<jfloat>(dst->player->skeleton().count());
        dst->lastError.clear();
        for (const std::string& note : report.notes) dst->pendingEvents.push_back("note:" + note);
        for (const std::string& missing : report.missingRoles) dst->pendingEvents.push_back("missing:" + missing);
    } else {
        fail(dst ? dst : src, "checkCompatibility: one of the two session handles is not valid");
    }
    env->SetFloatArrayRegion(out, 0, 5, values);
    return out;
}

JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeRetarget(
        JNIEnv* env, jobject, jlong handle, jlong sourceHandle, jint sourceClipIndex, jstring newName) {
    AnimationSession* dst = session(handle);
    AnimationSession* src = session(sourceHandle);
    if (!dst || !src) return -1;
    const Clip* sourceClip = src->player->clipAt(sourceClipIndex);
    if (!sourceClip) {
        fail(dst, "retarget: the source session has no clip at index " + std::to_string(sourceClipIndex));
        return -1;
    }
    std::string error;
    const int newIndex = dst->player->retarget(src->player->skeleton(), *sourceClip, &error);
    if (newIndex < 0) {
        fail(dst, "retarget refused: " + error);
        return -1;
    }
    const std::string label = fromJString(env, newName);
    if (!label.empty()) {
        /* retarget() names the baked clip after its provenance; a caller-supplied label wins so the
           UI can say "Fox Walk on CesiumMan" rather than "animation_0 (retargeted)". */
        Clip* baked = const_cast<Clip*>(dst->player->clipAt(newIndex));
        if (baked) baked->name = label;
    }
    return newIndex;
}

/* ================================================================ per frame ================= */

/* Advances every clock and writes what the renderer has to apply into the CALLER's arrays.
 *   outFrame[0] = clipA index (-1 when nothing is loaded)
 *   outFrame[1] = clipA time in seconds
 *   outFrame[2] = clipB index (-1 when not blending)
 *   outFrame[3] = clipB time in seconds
 *   outFrame[4] = blend alpha in FILAMENT's convention: the weight of clipA, so 1 = clipA alone and
 *                 0 = clipB alone. Animator.applyCrossFade(previousIndex, previousTime, alpha) takes
 *                 exactly this. Getting it backwards does not crash; it plays every fade in reverse.
 *                 See af::anim::CrossFadeCommand, which the host test suite pins.
 *   outFrame[5] = root motion X this frame, [6] = Y, [7] = Z
 *   outFrame[8] = last advance() cost in microseconds, [9] = moving average
 *   outMorphs   = one weight per morph target, in the order nativeMorphName reports
 * Returns the number of morph weights written, or a negative code on error. */
JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeAdvance(
        JNIEnv* env, jobject, jlong handle, jfloat dt, jfloatArray outFrame, jfloatArray outMorphs) {
    AnimationSession* s = session(handle);
    if (!s) return -1;
    if (!outFrame || env->GetArrayLength(outFrame) < 10) return -2;

    s->player->advance(dt);

    /* The blend triple comes from the core, not from an expression written here. The alpha convention
       is easy to invert, and inverting it does not crash -- it silently plays every transition
       backwards, fading out of the clip the user asked for. CrossFadeCommand::from is the single place
       that decides, and the host test suite pins it against Filament's documented convention. */
    const af::anim::CrossFadeCommand cf = s->player->crossFadeCommand();
    const af::anim::RootMotion& rm = s->player->rootMotion();
    const Player::Stats& stats = s->player->stats();
    jfloat frame[10];
    frame[0] = static_cast<jfloat>(cf.currentIndex);
    frame[1] = cf.currentTime;
    frame[2] = static_cast<jfloat>(cf.previousIndex);
    frame[3] = cf.previousTime;
    frame[4] = cf.alpha;
    frame[5] = rm.accumulated.x;
    frame[6] = rm.accumulated.y;
    frame[7] = rm.accumulated.z;
    frame[8] = static_cast<jfloat>(stats.lastAdvanceMicros);
    frame[9] = static_cast<jfloat>(stats.avgAdvanceMicros);
    env->SetFloatArrayRegion(outFrame, 0, 10, frame);

    const std::vector<float>& morphs = s->player->currentPose().morphs;
    jint written = 0;
    if (outMorphs && !morphs.empty()) {
        const jsize capacity = env->GetArrayLength(outMorphs);
        written = static_cast<jint>(std::min(static_cast<size_t>(capacity), morphs.size()));
        if (written > 0) env->SetFloatArrayRegion(outMorphs, 0, written, morphs.data());
    }
    return written;
}

/* The clock the crossfade is fading FROM. Filament's applyCrossFade takes
   (previousIndex, previousTime, alpha), so the renderer needs the source clip's own time, which is not
   the current position -- during a fade the two run independently. nativeAdvance reports the index and
   the alpha; this reports the time. */
JNIEXPORT jfloat JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativePreviousTime(JNIEnv*, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    if (!s) return 0.0f;
    return s->player->previousPosition();
}

/* The blend weight of the INCOMING clip: Filament's applyCrossFade alpha. 1 when nothing is blending. */
JNIEXPORT jfloat JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeCrossfadeAlpha(JNIEnv*, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    return s ? s->player->crossFadeCommand().alpha : 1.0f;
}

JNIEXPORT jfloat JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeTransitionProgress(JNIEnv*, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    return s ? s->player->state().transitionProgress : 1.0f;
}

JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeIsTransitioning(JNIEnv*, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    return (s && s->player->state().transitioning) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jfloatArray JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeConsumeRootMotion(JNIEnv* env, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    jfloatArray out = env->NewFloatArray(3);
    jfloat v[3] = {0.0f, 0.0f, 0.0f};
    if (s) {
        const af::Vec3 d = s->player->rootMotion().consume();
        v[0] = d.x; v[1] = d.y; v[2] = d.z;
    }
    env->SetFloatArrayRegion(out, 0, 3, v);
    return out;
}

JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeSetRootMotionEnabled(
        JNIEnv*, jobject, jlong handle, jboolean enabled) {
    if (AnimationSession* s = session(handle)) s->player->rootMotion().enabled = (enabled == JNI_TRUE);
}

JNIEXPORT jdoubleArray JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeStats(JNIEnv* env, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    jdoubleArray out = env->NewDoubleArray(7);
    jdouble v[7] = {0, 0, 0, 0, 0, 0, 0};
    if (s) {
        const Player::Stats& st = s->player->stats();
        v[0] = st.lastAdvanceMicros;
        v[1] = st.avgAdvanceMicros;
        v[2] = st.maxAdvanceMicros;
        v[3] = static_cast<jdouble>(st.frames);
        v[4] = static_cast<jdouble>(st.posesSampled);
        v[5] = st.jointCount;
        v[6] = st.channelCount;
    }
    env->SetDoubleArrayRegion(out, 0, 7, v);
    return out;
}

JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeResetStats(JNIEnv*, jobject, jlong handle) {
    if (AnimationSession* s = session(handle)) s->player->resetStats();
}

/* ================================================================ events ==================== */

/* Drains the events fired since the last call, as a flat [name, value, name, value, ...] array of
 * strings with the value formatted. Empty when nothing fired, which is the common case: no allocation
 * on a frame with no events. */
JNIEXPORT jobjectArray JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeDrainEvents(JNIEnv* env, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    const jclass stringClass = env->FindClass("java/lang/String");
    static const std::vector<std::string> kEmpty;
    if (!s) return env->NewObjectArray(0, stringClass, nullptr);

    const size_t count = s->pendingEvents.size();
    jobjectArray out = env->NewObjectArray(static_cast<jsize>(count * 2), stringClass, nullptr);
    for (size_t i = 0; i < count; ++i) {
        env->SetObjectArrayElement(out, static_cast<jsize>(i * 2), toJString(env, s->pendingEvents[i]));
        char value[32];
        std::snprintf(value, sizeof(value), "%.6g", s->pendingEventValues[i]);
        env->SetObjectArrayElement(out, static_cast<jsize>(i * 2 + 1), env->NewStringUTF(value));
    }
    s->pendingEvents.clear();
    s->pendingEventValues.clear();
    return out;
}

/* ================================================================ library =================== */

JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeLibraryCount(JNIEnv*, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    return s ? s->library->count() : 0;
}

/* One library entry as [name, categoryId, duration, looping, authored, source, licence, clipIndex].
   The category crosses as its NUMERIC id, not its label: the label is presentation ("Facial
   expressions") and parsing it back into an enum on the Kotlin side would be a string match that
   breaks the first time a label is reworded. Kotlin maps the id with AnimationCategory.fromNativeId
   and asks nativeCategoryName for display text. */
JNIEXPORT jobjectArray JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeLibraryEntry(JNIEnv* env, jobject, jlong handle, jint index) {
    AnimationSession* s = session(handle);
    const jclass stringClass = env->FindClass("java/lang/String");
    jobjectArray out = env->NewObjectArray(8, stringClass, nullptr);
    const af::anim::LibraryEntry* e = s ? s->library->at(index) : nullptr;
    if (!e) return out;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.6g", e->duration);
    char categoryBuf[8];
    std::snprintf(categoryBuf, sizeof(categoryBuf), "%d", static_cast<int>(e->category));
    const char* fields[8] = {
        e->name.c_str(),
        categoryBuf,
        buf,
        e->looping ? "1" : "0",
        e->authored ? "1" : "0",
        e->source.c_str(),
        e->licence.c_str(),
        nullptr
    };
    char indexBuf[16];
    std::snprintf(indexBuf, sizeof(indexBuf), "%d", e->clipIndex);
    fields[7] = indexBuf;
    for (int i = 0; i < 8; ++i) {
        env->SetObjectArrayElement(out, i, toJStringOrNull(env, fields[i]));
    }
    return out;
}

JNIEXPORT jintArray JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeLibrarySearch(
        JNIEnv* env, jobject, jlong handle, jstring query, jint category) {
    AnimationSession* s = session(handle);
    std::vector<int> hits;
    if (s) {
        const Category c = (category >= 0 && category < static_cast<jint>(Category::COUNT))
                               ? static_cast<Category>(category) : Category::COUNT;
        hits = s->library->search(fromJString(env, query), c);
    }
    jintArray out = env->NewIntArray(static_cast<jsize>(hits.size()));
    if (!hits.empty()) env->SetIntArrayRegion(out, 0, static_cast<jsize>(hits.size()), hits.data());
    return out;
}

JNIEXPORT jintArray JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeLibraryHistogram(JNIEnv* env, jobject, jlong handle) {
    AnimationSession* s = session(handle);
    const std::vector<int> hist = s ? s->library->categoryHistogram()
                                    : std::vector<int>(static_cast<size_t>(Category::COUNT), 0);
    jintArray out = env->NewIntArray(static_cast<jsize>(hist.size()));
    if (!hist.empty()) env->SetIntArrayRegion(out, 0, static_cast<jsize>(hist.size()), hist.data());
    return out;
}

/* ================================================================ metadata ================== */

JNIEXPORT jstring JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeCategoryName(JNIEnv* env, jobject, jint category) {
    if (category < 0 || category >= static_cast<jint>(Category::COUNT)) return nullptr;
    return toJStringOrNull(env, af::anim::categoryName(static_cast<Category>(category)));
}

JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeCategoryCount(JNIEnv*, jobject) {
    return static_cast<jint>(Category::COUNT);
}

JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeCategoryForClipName(JNIEnv* env, jobject, jstring name) {
    return static_cast<jint>(af::anim::categoryForClipName(fromJString(env, name)));
}

JNIEXPORT jstring JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeExpressionName(JNIEnv* env, jobject, jint id) {
    if (id < 0 || id >= static_cast<jint>(Expression::COUNT)) return nullptr;
    return toJStringOrNull(env, af::anim::expressionName(static_cast<Expression>(id)));
}

JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeExpressionCount(JNIEnv*, jobject) {
    return static_cast<jint>(Expression::COUNT);
}

JNIEXPORT jstring JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeVisemeName(JNIEnv* env, jobject, jint id) {
    if (id < 0 || id >= static_cast<jint>(af::anim::Viseme::COUNT)) return nullptr;
    return toJStringOrNull(env, af::anim::visemeName(static_cast<af::anim::Viseme>(id)));
}

JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeVisemeForChar(JNIEnv* env, jobject, jstring text, jint at) {
    const std::string s = fromJString(env, text);
    if (at < 0 || at >= static_cast<jint>(s.size())) return 0;
    return static_cast<jint>(af::anim::visemeForChar(s[static_cast<size_t>(at)]));
}

/* The bone palette, column-major, one Mat4 per skin slot: exactly what
   RenderableManager.setBonesAsMatrices wants. Written into the caller's buffer; returns the number of
   matrices written. The GLES backend uses the same numbers through af_engine instead. */
JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_AnimationNative_nativeBonePalette(
        JNIEnv* env, jobject, jlong handle, jfloatArray outMatrices) {
    AnimationSession* s = session(handle);
    if (!s || !outMatrices) return -1;
    static thread_local std::vector<af::Mat4> palette;
    palette.clear();
    s->player->skeleton().buildPalette(s->player->currentPose(), &palette);
    const jsize capacity = env->GetArrayLength(outMatrices);
    const jint count = static_cast<jint>(std::min(static_cast<size_t>(capacity / 16), palette.size()));
    if (count > 0) {
        env->SetFloatArrayRegion(outMatrices, 0, count * 16,
                                 reinterpret_cast<const jfloat*>(palette.data()));
    }
    return count;
}

} /* extern "C" */
