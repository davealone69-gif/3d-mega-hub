package com.davemega.hub

internal const val EXTRA_OPEN_AVATAR_STUDIO = "com.davemega.hub.extra.OPEN_AVATAR_STUDIO"

internal enum class HubStartPage {
    HOME,
    AVATAR
}

internal fun resolveHubStartPage(openAvatarStudio: Boolean): HubStartPage =
    if (openAvatarStudio) HubStartPage.AVATAR else HubStartPage.HOME
