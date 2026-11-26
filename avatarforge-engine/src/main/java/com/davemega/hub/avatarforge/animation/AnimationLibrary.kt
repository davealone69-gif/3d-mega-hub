package com.davemega.hub.avatarforge.animation

/**
 * The animations of one model as a library: what each is, which kind it is, and a search.
 *
 * Pure Kotlin with no Android dependency, so any screen (or another app) can use it and it is
 * unit-tested on the JVM. The entries come from the renderer
 * ([com.davemega.hub.avatarforge.AvatarForgeSurfaceView.animationLibrary]).
 */
data class AnimationEntry(
    /** Index the renderer plays it by. */
    val index: Int,
    /** The name in the engine or in the file it was imported from. */
    val name: String,
    val durationSeconds: Float,
    /** The last pose is the first again, so it can repeat without a jump. */
    val looping: Boolean = true,
    /** Retargeted from a file the user added, not built into the engine. */
    val imported: Boolean = false,
    /** The clip moves the hips (a jump, a squat), not only turns joints. */
    val movesRoot: Boolean = false,
    /** The source walked away; the travel was taken out so it plays on the spot. */
    val travels: Boolean = false
) {
    val category: AnimationCategory get() = AnimationCategory.of(name)
    val label: String get() = AnimationLibrary.label(name)
}

/** The ten kinds an animation is filed under. */
enum class AnimationCategory(val label: String) {
    IDLE("Idle"),
    LOCOMOTION("Walk & run"),
    JUMP("Jump & turn"),
    GESTURE("Gesture"),
    DANCE("Dance"),
    EXERCISE("Exercise"),
    COMBAT("Combat"),
    SITTING("Sit & floor"),
    POSE("Pose"),
    OTHER("Other");

    companion object {
        /*
         * A name is filed by the first rule that matches, so the order is part of the rules:
         * "Sit-ups" and "Jumping jacks" are exercise before they are sitting or jumping, "hip hop"
         * is a dance before it is a hop, "Standing gesturing" is a gesture before it is standing.
         * A keyword matches at the start of a word only: "sit" files "sitting", not "position".
         */
        private val rules: List<Pair<AnimationCategory, List<String>>> = listOf(
            POSE to listOf("t pose", "a pose", "tpose", "apose", "pose", "bind", "rest"),
            EXERCISE to listOf(
                "squat", "pushup", "push up", "press up", "sit up", "situp", "jumping jack", "lunge", "plank",
                "crunch", "burpee", "stretch", "flex", "exercise", "workout", "knee lift", "lift"
            ),
            COMBAT to listOf("punch", "kick", "fight", "box", "block", "sword", "attack", "hit", "shoot", "slash", "dodge", "strike"),
            DANCE to listOf("dance", "dancing", "spin", "twirl", "groove", "samba", "salsa", "hip hop", "hiphop", "twist", "shuffle"),
            IDLE to listOf("idle", "breathe", "breathing"),
            SITTING to listOf("sit", "seat", "kneel", "crouch", "lie", "lying", "sleep", "floor"),
            LOCOMOTION to listOf("walk", "run", "jog", "sprint", "strut", "stride", "step", "march", "strafe", "crawl", "sneak"),
            JUMP to listOf("jump", "hop", "leap", "turn", "flip", "roll", "fall", "land"),
            GESTURE to listOf(
                "wave", "waving", "gestur", "point", "clap", "nod", "shrug", "salute", "bow", "talk", "hold", "hat",
                "arm", "hand", "cheer", "thumb", "look"
            ),
            IDLE to listOf("stand")
        )

        fun of(name: String): AnimationCategory {
            val words = " " + AnimationLibrary.words(name).joinToString(" ")
            for ((category, keywords) in rules) {
                if (keywords.any { words.contains(" $it") }) return category
            }
            return OTHER
        }
    }
}

class AnimationLibrary(entries: List<AnimationEntry>) {
    val entries: List<AnimationEntry> = entries.toList()

    val size: Int get() = entries.size
    fun isEmpty(): Boolean = entries.isEmpty()

    /** The categories that have at least one animation, in the order of [AnimationCategory]. */
    val categories: List<AnimationCategory> = AnimationCategory.values().filter { category -> this.entries.any { it.category == category } }

    val importedCount: Int get() = entries.count { it.imported }

    fun byIndex(index: Int): AnimationEntry? = entries.firstOrNull { it.index == index }

    /** By name: exactly, then without regard to case, then by the label shown to the user. */
    fun find(name: String): AnimationEntry? =
        entries.firstOrNull { it.name == name }
            ?: entries.firstOrNull { it.name.equals(name, ignoreCase = true) }
            ?: entries.firstOrNull { it.label.equals(name, ignoreCase = true) }

    /** The first of [names] this model has. */
    fun firstOf(vararg names: String): AnimationEntry? {
        for (name in names) find(name)?.let { return it }
        return null
    }

    fun inCategory(category: AnimationCategory?): List<AnimationEntry> =
        if (category == null) entries else entries.filter { it.category == category }

    /**
     * Every word of [query] has to occur in the name, the label or the category of an entry.
     * An empty query finds everything. [category] and [imported] narrow the result when given.
     */
    fun search(query: String, category: AnimationCategory? = null, imported: Boolean? = null): List<AnimationEntry> {
        val terms = query.lowercase().split(' ', '\t', '\n').filter { it.isNotBlank() }
        return entries.filter { entry ->
            (category == null || entry.category == category) &&
                (imported == null || entry.imported == imported) &&
                terms.all { term ->
                    entry.name.lowercase().contains(term) ||
                        entry.label.lowercase().contains(term) ||
                        entry.category.label.lowercase().contains(term)
                }
        }
    }

    companion object {
        val EMPTY = AnimationLibrary(emptyList())

        /** "turn_360" -> "Turn 360", "t_pose" -> "T pose"; a name with capitals or spaces is kept as its author wrote it. */
        fun label(name: String): String {
            val trimmed = name.trim()
            if (trimmed.isEmpty()) return "Animation"
            val authored = trimmed.any { it == ' ' || it.isUpperCase() }
            if (authored) return trimmed
            return trimmed.replace('_', ' ').replaceFirstChar { it.uppercase() }
        }

        /** The words of a name in lower case: split at anything that is not a letter or digit, and at aB. */
        fun words(name: String): List<String> {
            val spaced = StringBuilder()
            var previous = ' '
            for (c in name) {
                if (c.isUpperCase() && previous.isLowerCase()) spaced.append(' ')
                spaced.append(if (c.isLetterOrDigit()) c.lowercaseChar() else ' ')
                previous = c
            }
            return spaced.split(' ').filter { it.isNotEmpty() }
        }
    }
}
