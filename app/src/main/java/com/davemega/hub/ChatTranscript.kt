package com.davemega.hub

import org.json.JSONArray
import org.json.JSONObject

/** One line of the chat transcript. Mirrors the UI's own line type. */
data class ChatTurn(val fromUser: Boolean, val text: String)

/**
 * Turns the on-screen transcript into the `/api/chat` message array.
 *
 * The app previously sent a single user message per request, so the local model had no memory of
 * the conversation and answered every follow-up as if it were the first. Sending bounded history is
 * the feature fix; the bounds are what keep a long-running chat from producing an unbounded request
 * body or from silently exceeding the server's context window.
 *
 * Pure Kotlin plus `org.json`, so the trimming rules are covered by JVM unit tests rather than only
 * being observable on a device.
 */
object ChatTranscript {
    /** Newest turns kept for context. */
    const val MAX_TURNS = 12

    /** Total transcript characters allowed in one request body. */
    const val MAX_TOTAL_CHARS = 24_000

    /** Per-message ceiling, so one pasted wall of text cannot crowd out the conversation. */
    const val MAX_MESSAGE_CHARS = 8_000

    /**
     * Drops blank lines, truncates over-long messages, keeps the newest [MAX_TURNS] turns within
     * [MAX_TOTAL_CHARS], and always returns them oldest-first.
     */
    fun trimmed(turns: List<ChatTurn>): List<ChatTurn> {
        val cleaned = turns
            .filter { it.text.isNotBlank() }
            .map { it.copy(text = it.text.take(MAX_MESSAGE_CHARS)) }
        val kept = ArrayList<ChatTurn>()
        var used = 0
        for (turn in cleaned.asReversed()) {
            if (kept.size >= MAX_TURNS) break
            if (kept.isNotEmpty() && used + turn.text.length > MAX_TOTAL_CHARS) break
            kept.add(turn)
            used += turn.text.length
        }
        kept.reverse()
        return kept
    }

    /** `[{role: user|assistant, content: ...}]`, ready to put in the request body. */
    fun messages(turns: List<ChatTurn>): JSONArray {
        val array = JSONArray()
        for (turn in trimmed(turns)) {
            array.put(
                JSONObject()
                    .put("role", if (turn.fromUser) "user" else "assistant")
                    .put("content", turn.text)
            )
        }
        return array
    }

    /** Human-readable summary of what the next request will carry, for the status line. */
    fun describe(turns: List<ChatTurn>): String {
        val kept = trimmed(turns)
        if (kept.isEmpty()) return "No conversation history yet."
        val chars = kept.sumOf { it.text.length }
        return "Sending ${kept.size} message(s), $chars characters of context."
    }
}
