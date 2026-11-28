#ifndef AVATARFORGE_LIVE_RIG_H
#define AVATARFORGE_LIVE_RIG_H

#include "afcore.h"

#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace af {

/**
 * Process-local handoff cache that keeps the most recently generated rigged `af_engine`
 * (71 joints, 16 built-in animation clips, 52 ARKit blendshapes) available for immediate
 * real-time animation and facial expression playback in `AvatarForgeSurfaceView`.
 *
 * Ownership uses `std::shared_ptr<af_engine>` so a newly generated avatar never invalidates
 * an active render session until the old `AvatarForgeSurfaceView` tears down cleanly.
 */
class LiveRigRegistry {
public:
    static void stageGenerated(std::shared_ptr<af_engine> engine) {
        std::lock_guard<std::mutex> lock(mutex());
        state().staged = std::move(engine);
    }

    static void bindPublishedPath(const std::string& glbPath) {
        std::lock_guard<std::mutex> lock(mutex());
        if (state().staged && !glbPath.empty()) {
            state().published = std::move(state().staged);
            state().publishedPath = glbPath;
        }
        state().staged.reset();
    }

    static std::shared_ptr<af_engine> acquireForPath(const std::string& glbPath) {
        std::lock_guard<std::mutex> lock(mutex());
        if (state().published && !glbPath.empty() && state().publishedPath == glbPath) {
            return state().published;
        }
        return nullptr;
    }

    static void clear() {
        std::lock_guard<std::mutex> lock(mutex());
        state().staged.reset();
        state().published.reset();
        state().publishedPath.clear();
    }

private:
    struct State {
        std::shared_ptr<af_engine> staged;
        std::shared_ptr<af_engine> published;
        std::string publishedPath;
    };

    static std::mutex& mutex() {
        static std::mutex m;
        return m;
    }

    static State& state() {
        static State s;
        return s;
    }
};

} // namespace af

#endif /* AVATARFORGE_LIVE_RIG_H */
