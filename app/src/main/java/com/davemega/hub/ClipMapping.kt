package com.davemega.hub

/**
 * Matches the animation core's clip indices to a GPU renderer's animation indices.
 *
 * Why this file exists: the app parses the same GLB twice, once with `af::importGlb` for the animation
 * core and once with Filament's gltfio for rendering. Each parser produces its own list of clips, and
 * the two lists are only *probably* in the same order. Assuming they are would be the kind of bug that
 * works on Fox.glb and silently plays the wrong animation on a file whose clips gltfio orders
 * differently -- or on one where our importer refused an unreadable channel and therefore has one fewer
 * clip than gltfio does.
 *
 * So the correspondence is decided by NAME first and by POSITION only as an explicitly reasoned
 * fallback, and every decision is recorded so the UI can show what it is relying on.
 *
 * This is pure Kotlin with no Filament or JNI types, which is deliberate: it is the one piece of the
 * renderer integration whose correctness can be pinned by a JVM unit test rather than by a device.
 */

/** How one clip was matched. Reported to the user; a POSITION match is weaker evidence than NAME. */
enum class ClipMatchMethod {
    /** Both parsers reported the same animation name. The strongest evidence. */
    NAME,
    /** Names were absent or generated on one side, so the clips were matched by document order. Only
     *  attempted when both parsers reported the same clip COUNT -- otherwise positional matching is
     *  meaningless and the clip is left unmatched. */
    POSITION,
    /** No defensible correspondence. The renderer cannot play this clip and the UI must say so rather
     *  than quietly playing something else. */
    UNMATCHED,
}

/**
 * @param nativeToRenderer native clip index -> renderer animation index, or -1 when unmatched.
 * @param methods how each entry was decided, indexed the same way.
 * @param notes human-readable explanation of every non-NAME decision, for the diagnostics panel.
 */
data class ClipMapping(
    val nativeToRenderer: IntArray,
    val methods: List<ClipMatchMethod>,
    val notes: List<String>,
) {
    /**
     * True when there is at least one native clip and every one of them found a renderer counterpart.
     *
     * The emptiness check is not decoration. `all` on an empty collection is vacuously true, so without
     * it a session that loaded NO clips at all would report itself as a complete mapping -- the exact
     * case where "everything matched" is least true. Found by ClipMappingTest on the first CI run.
     */
    val complete: Boolean get() = nativeToRenderer.isNotEmpty() && nativeToRenderer.all { it >= 0 }

    val matchedCount: Int get() = nativeToRenderer.count { it >= 0 }

    fun rendererIndexFor(nativeIndex: Int): Int =
        nativeToRenderer.getOrElse(nativeIndex) { -1 }

    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is ClipMapping) return false
        return nativeToRenderer.contentEquals(other.nativeToRenderer) &&
            methods == other.methods && notes == other.notes
    }

    override fun hashCode(): Int {
        var result = nativeToRenderer.contentHashCode()
        result = 31 * result + methods.hashCode()
        result = 31 * result + notes.hashCode()
        return result
    }
}

/**
 * A name that carries no information about identity, so it must not be used to match on.
 *
 * Our importer labels an animation the glTF document left unnamed as `<unnamed animation N>`, and
 * gltfio is free to report "" or its own generated label for the same entry. Those strings describe a
 * parser's numbering, not the asset, so two parsers' generated names agree only by luck.
 */
/* `<unnamed animation 0>`, `animation_2`, `anim-1`: a parser's own numbering, optionally bracketed.
   A name ending in non-digits ("animation_walk") is a real name the author chose and stays
   authoritative. */
private val GENERATED_CLIP_NAME =
    Regex("""^(?:<\s*unnamed[^>]*>|anim(?:ation)?[\s_-]*\d+)$""", RegexOption.IGNORE_CASE)

internal fun isAuthoritativeName(name: String?): Boolean {
    if (name.isNullOrBlank()) return false
    return !GENERATED_CLIP_NAME.matches(name.trim())
}

/**
 * Builds the correspondence between two parsers' clip lists.
 *
 * @param nativeNames clip names as the animation core sees them, in its own index order.
 * @param rendererNames animation names as the renderer sees them, in its own index order.
 */
fun mapClipIndices(nativeNames: List<String>, rendererNames: List<String>): ClipMapping {
    val notes = mutableListOf<String>()
    val nativeToRenderer = IntArray(nativeNames.size) { -1 }
    val methods = MutableList(nativeNames.size) { ClipMatchMethod.UNMATCHED }

    if (nativeNames.isEmpty()) {
        notes += "the animation core loaded no clips from this asset"
        return ClipMapping(nativeToRenderer, methods, notes)
    }
    if (rendererNames.isEmpty()) {
        notes += "the renderer reported no animations, so nothing can be applied"
        return ClipMapping(nativeToRenderer, methods, notes)
    }

    /* Index the renderer's clips by name. A duplicate name is ambiguous, so only the FIRST occurrence
       is indexed and the duplicate is reported -- matching onto the wrong one of two identically named
       clips is worse than reporting the ambiguity. */
    val byName = HashMap<String, Int>(rendererNames.size * 2)
    val duplicates = mutableListOf<String>()
    rendererNames.forEachIndexed { index, name ->
        if (!isAuthoritativeName(name)) return@forEachIndexed
        val key = name.trim()
        if (byName.putIfAbsent(key, index) != null) duplicates += key
    }
    if (duplicates.isNotEmpty()) {
        notes += "the renderer reports more than one animation named ${duplicates.distinct().joinToString(", ")}; " +
            "each was matched to its first occurrence"
    }

    val sameCount = nativeNames.size == rendererNames.size
    if (!sameCount) {
        notes += "the two parsers disagree on the clip count (core ${nativeNames.size}, renderer " +
            "${rendererNames.size}), so positional matching was disabled and only names were trusted"
    }

    val usedRenderer = BooleanArray(rendererNames.size)

    nativeNames.forEachIndexed { nativeIndex, name ->
        val matched = if (isAuthoritativeName(name)) byName[name.trim()] else null
        if (matched != null && !usedRenderer[matched]) {
            nativeToRenderer[nativeIndex] = matched
            methods[nativeIndex] = ClipMatchMethod.NAME
            usedRenderer[matched] = true
            return@forEachIndexed
        }
        /* Positional fallback, and only when both sides reported the same number of clips and that
           renderer slot is still free. Without the count agreement, position N in one list has no
           relationship to position N in the other. */
        if (sameCount && !usedRenderer[nativeIndex]) {
            nativeToRenderer[nativeIndex] = nativeIndex
            methods[nativeIndex] = ClipMatchMethod.POSITION
            usedRenderer[nativeIndex] = true
            notes += "'${if (name.isBlank()) "<blank>" else name}' has no matching renderer name; " +
                "matched by document position ${nativeIndex} because both parsers reported " +
                "${nativeNames.size} clip(s)"
        } else if (matched != null) {
            notes += "'$name' matched a renderer animation already claimed by another clip"
        } else {
            notes += "'${if (name.isBlank()) "<blank>" else name}' could not be matched: no renderer " +
                "animation has that name" + if (sameCount) "" else " and the clip counts differ"
        }
    }

    val unmatchedRenderer = rendererNames.indices.filter { !usedRenderer[it] }
    if (unmatchedRenderer.isNotEmpty()) {
        notes += "the renderer has ${unmatchedRenderer.size} animation(s) the core did not load: " +
            unmatchedRenderer.joinToString(", ") { "'${rendererNames[it]}'" }
    }
    return ClipMapping(nativeToRenderer, methods, notes)
}

/**
 * Decides whether two morph-target name lists are in the same order, which is the precondition for
 * pushing core-computed weights straight into the renderer.
 *
 * Morph weights are positional: `RenderableManager.setMorphWeights` takes an array indexed by target,
 * with no names on the wire. If the two parsers ordered the targets differently, every expression would
 * be applied to the wrong target -- a face that smiles with its eyelids. Requiring the names to agree
 * in order is cheap and turns that into a refusal instead of a wrong render.
 *
 * @return null when the lists are interchangeable, otherwise the reason they are not.
 */
fun verifyMorphTargetOrder(nativeNames: List<String>, rendererNames: List<String>): String? {
    if (nativeNames.isEmpty() || rendererNames.isEmpty()) {
        /* Nothing to compare. An empty list on either side means this asset has no named morph targets,
           which is not an ordering conflict -- the caller has no weights to push either way. */
        return null
    }
    if (nativeNames.size != rendererNames.size) {
        return "morph target counts differ (core ${nativeNames.size}, renderer ${rendererNames.size}), " +
            "so weights cannot be pushed positionally"
    }
    for (index in nativeNames.indices) {
        val a = nativeNames[index].trim()
        val b = rendererNames[index].trim()
        if (a.isNotEmpty() && b.isNotEmpty() && !a.equals(b, ignoreCase = true)) {
            return "morph target $index is '$a' to the core but '$b' to the renderer, so pushing weights " +
                "positionally would drive the wrong targets"
        }
    }
    return null
}
