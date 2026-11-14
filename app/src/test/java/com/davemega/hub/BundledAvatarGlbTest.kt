package com.davemega.hub

import com.davemega.hub.avatarforge.GlbValidator
import java.io.File
import org.junit.Assert.assertTrue
import org.junit.Test

class BundledAvatarGlbTest {
    @Test
    fun bundledAvatarIsCompatibleWithFilamentMorphRequirements() {
        val asset = File("src/main/assets/models/female.glb")
        assertTrue("Bundled avatar asset is missing at ${asset.absolutePath}", asset.isFile)
        GlbValidator.validate(asset)
    }
}
