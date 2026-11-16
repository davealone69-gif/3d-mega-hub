/*
 * af_anim_library.cpp -- the searchable animation library.
 *
 * The catalogue is built FROM loaded clips, never from a hard-coded list. That distinction is the
 * whole point of this file: an animation picker that lists "Dancing" because the specification asked
 * for a Dancing category, on a character that has no dance clip, is a placeholder button with extra
 * steps. AnimationLibrary::addFromPlayer() reads what a Player actually loaded and records where it
 * came from and under what licence; addAuthoredLibrary() adds the 16 clips af_rig.cpp really ships.
 * A category with nothing in it is simply empty in the histogram, and search() returns nothing.
 *
 * Categories are assigned from the clip's own name by categoryForClipName() (af_anim_clip.cpp),
 * which is why Fox's "Survey" lands in Idle and "Walk" in Walking without anybody having to label
 * them by hand -- and why an unnamed clip stays Uncategorized rather than being guessed at.
 */
#include "af_anim.h"

#include <algorithm>
#include <cmath>

namespace af {
namespace anim {

namespace {

std::string lower(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) out.push_back((c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c);
    return out;
}

} /* namespace */

void AnimationLibrary::clear() { entries_.clear(); }

void AnimationLibrary::addEntry(LibraryEntry entry) {
    entries_.push_back(std::move(entry));
}

void AnimationLibrary::addFromPlayer(const Player& player, const std::string& source,
                                     const std::string& licence) {
    for (int i = 0; i < player.clipCount(); ++i) {
        const Clip* clip = player.clipAt(i);
        if (!clip) continue;
        LibraryEntry e;
        e.name = clip->name.empty() ? ("clip_" + std::to_string(i)) : clip->name;
        e.category = clip->category;
        e.duration = clip->duration;
        e.looping = clip->looping;
        e.authored = clip->authored;
        e.source = source;
        e.licence = licence;
        e.clipIndex = i;
        entries_.push_back(std::move(e));
    }
}

void AnimationLibrary::addAuthoredLibrary() {
    /* The engine's own library. No asset, no download, no licence question: af_rig.cpp is this
       project's source and the key data in it is hand-authored. */
    const std::vector<af::Clip>& library = af::builtinClips();
    for (size_t i = 0; i < library.size(); ++i) {
        const af::Clip& c = library[i];
        LibraryEntry e;
        e.name = c.name;
        e.category = categoryForClipName(c.name);
        e.duration = c.duration;
        e.looping = c.looping;
        e.authored = true;
        e.source = "AvatarForge authored clip library (af_rig.cpp)";
        e.licence = "Project source; hand-authored key data, no third-party asset";
        /* -1: an authored entry is not bound to a loaded Player until one calls loadAuthored(). The
           UI must not offer it as playable on a character whose skeleton cannot bind it. */
        e.clipIndex = -1;
        entries_.push_back(std::move(e));
    }
}

const LibraryEntry* AnimationLibrary::at(int index) const {
    if (index < 0 || index >= static_cast<int>(entries_.size())) return nullptr;
    return &entries_[static_cast<size_t>(index)];
}

int AnimationLibrary::indexOf(const std::string& name) const {
    for (size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].name == name) return static_cast<int>(i);
    }
    return -1;
}

std::vector<int> AnimationLibrary::byCategory(Category c) const {
    std::vector<int> out;
    if (c == Category::COUNT) {
        for (size_t i = 0; i < entries_.size(); ++i) out.push_back(static_cast<int>(i));
        return out;
    }
    for (size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].category == c) out.push_back(static_cast<int>(i));
    }
    return out;
}

std::vector<int> AnimationLibrary::search(const std::string& query, Category category) const {
    std::vector<int> out;
    const std::string q = lower(query);
    const std::string catName = lower(categoryName(category));
    for (size_t i = 0; i < entries_.size(); ++i) {
        const LibraryEntry& e = entries_[i];
        if (category != Category::COUNT && e.category != category) continue;
        if (q.empty()) { out.push_back(static_cast<int>(i)); continue; }
        /* Match the clip name, its category label, and its source, so "fox" finds everything from
           Fox.glb and "run" finds both a clip called Run and everything categorised Running. */
        if (lower(e.name).find(q) != std::string::npos ||
            catName.find(q) != std::string::npos ||
            lower(e.source).find(q) != std::string::npos) {
            out.push_back(static_cast<int>(i));
        }
    }
    return out;
}

std::vector<int> AnimationLibrary::categoryHistogram() const {
    std::vector<int> hist(static_cast<size_t>(Category::COUNT), 0);
    for (const LibraryEntry& e : entries_) {
        const int c = static_cast<int>(e.category);
        if (c >= 0 && c < static_cast<int>(Category::COUNT)) hist[static_cast<size_t>(c)] += 1;
    }
    return hist;
}

} /* namespace anim */
} /* namespace af */
