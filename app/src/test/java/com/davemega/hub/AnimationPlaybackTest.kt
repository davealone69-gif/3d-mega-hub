package com.davemega.hub

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class AnimationPlaybackTest {
    @Test
    fun shortcutsOnlyPointAtMatchingPlayableClips() {
        val state = AnimationPlaybackState(clips = listOf(
            AnimationClip(0, "idle", 2f),
            AnimationClip(1, "breathe", 1f),
            AnimationClip(2, "WALK", 1.2f),
            AnimationClip(3, "run", 0f),
            AnimationClip(4, "wave", 0.8f),
            AnimationClip(5, "WalkCycle", 2f),
        ))
        assertEquals(0, state.shortcut(AnimationShortcut.IDLE)?.index)
        assertEquals(2, state.shortcut(AnimationShortcut.WALK)?.index)
        assertNull(state.shortcut(AnimationShortcut.RUN))
        assertEquals(4, state.shortcut(AnimationShortcut.WAVE)?.index)
        assertNull(AnimationPlaybackState(clips = listOf(AnimationClip(0, "WalkCycle", 1f)))
            .shortcut(AnimationShortcut.WALK))
    }

    @Test
    fun unnamedAndUnderscoredClipsHaveReadableLabels() {
        assertEquals("Clip 3", AnimationClip(2, " ", 1f).label)
        assertEquals("T pose", AnimationClip(0, "t_pose", 1f).label)
        assertEquals("Turn 360", AnimationClip(1, "turn_360", 1f).label)
        assertEquals("WalkCycle", AnimationClip(2, "WalkCycle", 1f).label)
    }

    @Test
    fun navigationSkipsInvalidDurationsWithoutChangingOriginalIndices() {
        val clips = listOf(
            AnimationClip(0, "idle", 1f), AnimationClip(1, "bad", 0f),
            AnimationClip(2, "walk", 2f), AnimationClip(3, "bad", Float.NaN),
            AnimationClip(4, "wave", 1f), AnimationClip(5, "bad", Float.POSITIVE_INFINITY),
        )
        val state = AnimationPlaybackState(clips = clips, selectedIndex = 2)
        assertEquals("Walk", state.currentClip?.label)
        assertEquals(0, state.previousClip?.index)
        assertEquals(4, state.nextClip?.index)
        assertFalse(clips[1].isPlayable)
        assertFalse(clips[3].isPlayable)
        assertFalse(clips[5].isPlayable)
        assertTrue(clips[4].isPlayable)
        assertNull(state.copy(selectedIndex = 0).previousClip)
        assertNull(state.copy(selectedIndex = 4).nextClip)
    }

    @Test
    fun loadingAndStaticModelsHaveNoShortcuts() {
        for (state in listOf(AnimationPlaybackState(), AnimationPlaybackState(clips = emptyList()))) {
            assertNull(state.currentClip)
            assertNull(state.shortcut(AnimationShortcut.IDLE))
            assertNull(state.previousClip)
            assertNull(state.nextClip)
        }
    }
}
