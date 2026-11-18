package com.davemega.hub

import java.util.Locale

/** A clip index is always the original glTF index, not its position in a filtered list. */
internal data class AnimationClip(
    val index: Int,
    val name: String,
    val durationSeconds: Float,
) {
    val isPlayable: Boolean get() = durationSeconds.isFinite() && durationSeconds > 0f

    val label: String
        get() = name.replace('_', ' ').trim().ifBlank { "Clip ${index + 1}" }
            .replaceFirstChar { if (it.isLowerCase()) it.titlecase(Locale.ROOT) else it.toString() }
}

internal enum class AnimationShortcut(val label: String) {
    IDLE("Idle"), WALK("Walk"), RUN("Run"), WAVE("Wave")
}

/** Null clips means the model is loading or unavailable; an empty list means it loaded without clips. */
internal data class AnimationPlaybackState(
    val clips: List<AnimationClip>? = null,
    val selectedIndex: Int = 0,
    val playing: Boolean = false,
    val looping: Boolean = true,
    val speed: Float = 1f,
    val positionSeconds: Float = 0f,
) {
    val currentClip: AnimationClip? get() = clips?.firstOrNull { it.index == selectedIndex }
    val previousClip: AnimationClip?
        get() = clips.orEmpty().lastOrNull { it.isPlayable && it.index < selectedIndex }
    val nextClip: AnimationClip?
        get() = clips.orEmpty().firstOrNull { it.isPlayable && it.index > selectedIndex }

    /** Match only actual, playable clips. An unrelated imported clip must never become a fake Walk button. */
    fun shortcut(shortcut: AnimationShortcut): AnimationClip? = clips.orEmpty().firstOrNull {
        it.isPlayable && it.name.trim().equals(shortcut.label, ignoreCase = true)
    }
}
