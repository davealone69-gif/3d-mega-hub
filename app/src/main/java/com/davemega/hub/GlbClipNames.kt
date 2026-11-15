package com.davemega.hub

/**
 * The labels the viewer shows for the animation clips of a glTF file.
 *
 * A clip's name is whatever the file's author or exporter wrote, and it is optional: a file may
 * leave it out, repeat it, pad it with spaces or make it very long. The labels are what the user
 * picks a clip by, so each one must say something and no two may be the same:
 *  - spaces at the ends are dropped and runs of white space or control characters become one space;
 *  - a clip without a name is called "Clip N", N being its place in the file, counted from 1;
 *  - a name longer than [MAX_LENGTH] characters is cut and ends in an ellipsis;
 *  - a label that was already used gets " (2)", " (3)" and so on.
 * The list keeps the file's order and length, so a label's index is the clip's index.
 */
object GlbClipNames {
    const val MAX_LENGTH = 40

    fun displayNames(raw: List<String?>): List<String> {
        val taken = HashSet<String>()
        return raw.mapIndexed { index, name ->
            val base = shorten(clean(name)).ifEmpty { "Clip ${index + 1}" }
            var label = base
            var copy = 2
            while (!taken.add(label)) {
                label = "$base ($copy)"
                copy++
            }
            label
        }
    }

    private fun clean(name: String?): String {
        if (name == null) return ""
        val text = StringBuilder(name.length)
        var gap = false
        for (character in name) {
            if (character.isWhitespace() || character.isISOControl()) {
                gap = text.isNotEmpty()
            } else {
                if (gap) text.append(' ')
                gap = false
                text.append(character)
            }
        }
        return text.toString()
    }

    private fun shorten(name: String): String =
        if (name.length <= MAX_LENGTH) name else name.substring(0, MAX_LENGTH - 1).trimEnd() + "\u2026"
}
