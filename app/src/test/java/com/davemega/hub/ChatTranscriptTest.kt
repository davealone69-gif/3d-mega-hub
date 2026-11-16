package com.davemega.hub

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class ChatTranscriptTest {
    @Test
    fun mapsRolesOldestFirst() {
        val messages = ChatTranscript.messages(
            listOf(
                ChatTurn(true, "hello"),
                ChatTurn(false, "hi, how can I help?"),
                ChatTurn(true, "describe a coat")
            )
        )

        assertEquals(3, messages.length())
        assertEquals("user", messages.getJSONObject(0).getString("role"))
        assertEquals("hello", messages.getJSONObject(0).getString("content"))
        assertEquals("assistant", messages.getJSONObject(1).getString("role"))
        assertEquals("user", messages.getJSONObject(2).getString("role"))
    }

    @Test
    fun blankLinesAreDropped() {
        val kept = ChatTranscript.trimmed(listOf(ChatTurn(true, "   "), ChatTurn(true, "real"), ChatTurn(false, "")))

        assertEquals(1, kept.size)
        assertEquals("real", kept.single().text)
        assertEquals(0, ChatTranscript.messages(emptyList()).length())
    }

    @Test
    fun longMessagesAreTruncatedButKept() {
        val huge = "x".repeat(ChatTranscript.MAX_MESSAGE_CHARS + 5_000)

        val kept = ChatTranscript.trimmed(listOf(ChatTurn(true, huge)))

        assertEquals(1, kept.size)
        assertEquals(ChatTranscript.MAX_MESSAGE_CHARS, kept.single().text.length)
    }

    @Test
    fun oldestTurnsAreDroppedOnceTheCapIsReached() {
        val turns = (1..(ChatTranscript.MAX_TURNS + 5)).map { ChatTurn(true, "message $it") }

        val kept = ChatTranscript.trimmed(turns)

        assertEquals(ChatTranscript.MAX_TURNS, kept.size)
        assertEquals("message 6", kept.first().text)
        assertEquals("message ${ChatTranscript.MAX_TURNS + 5}", kept.last().text)
    }

    @Test
    fun characterBudgetStopsTheTranscriptGrowingForever() {
        val turns = listOf(
            ChatTurn(true, "a".repeat(10_000)),
            ChatTurn(false, "b".repeat(10_000)),
            ChatTurn(true, "c".repeat(10_000)),
            ChatTurn(false, "newest")
        )

        val kept = ChatTranscript.trimmed(turns)
        val total = kept.sumOf { it.text.length }

        assertTrue("kept $total characters", total <= ChatTranscript.MAX_TOTAL_CHARS + ChatTranscript.MAX_MESSAGE_CHARS)
        assertEquals("newest", kept.last().text)
    }

    @Test
    fun newestMessageSurvivesEvenWhenItAloneExceedsTheBudget() {
        val only = ChatTurn(true, "z".repeat(ChatTranscript.MAX_MESSAGE_CHARS))

        val kept = ChatTranscript.trimmed(listOf(only))

        assertEquals(1, kept.size)
    }

    @Test
    fun describeReportsTheContextBeingSent() {
        assertEquals("No conversation history yet.", ChatTranscript.describe(emptyList()))

        val text = ChatTranscript.describe(listOf(ChatTurn(true, "abc"), ChatTurn(false, "de")))

        assertTrue(text, text.startsWith("Sending 2 message(s)"))
        assertTrue(text, text.contains("5 characters"))
    }
}
