package com.davemega.hub

internal const val EXTRA_OPEN_AVATAR_STUDIO = "com.davemega.hub.extra.OPEN_AVATAR_STUDIO"
internal const val EXTRA_IMPORT_GLB_URL = "com.davemega.hub.extra.IMPORT_GLB_URL"

internal enum class HubStartPage {
    HOME,
    AVATAR
}

internal fun resolveHubStartPage(openAvatarStudio: Boolean): HubStartPage =
    if (openAvatarStudio) HubStartPage.AVATAR else HubStartPage.HOME

