package com.davemega.hub

import org.junit.Assert.assertEquals
import org.junit.Test

class HubStartPageTest {
    @Test
    fun defaultsToHomeForNormalLaunch() {
        assertEquals(HubStartPage.HOME, resolveHubStartPage(openAvatarStudio = false))
    }

    @Test
    fun opensAvatarStudioWhenSmokeRouteIsRequested() {
        assertEquals(HubStartPage.AVATAR, resolveHubStartPage(openAvatarStudio = true))
    }
}
