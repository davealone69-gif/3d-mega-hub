package com.davemega.hub

import androidx.compose.runtime.saveable.listSaver
import com.davemega.hub.avatarforge.Build
import com.davemega.hub.avatarforge.Footwear
import com.davemega.hub.avatarforge.HairStyle
import com.davemega.hub.avatarforge.Outfit
import com.davemega.hub.avatarforge.Sex

/** Store only primitive values so the editor draft survives Android state restoration. */
internal val CharacterDraftSaver = listSaver<CharacterDraft, Any>(
    save = {
        listOf(it.presetId, it.sex.name, it.build.name, it.hair.name, it.outfit.name,
            it.footwear.name, it.heightCm, it.skin.name, it.hairColor.name)
    },
    restore = { values ->
        runCatching {
            CharacterDraft(
                presetId = values[0] as String,
                sex = Sex.valueOf(values[1] as String),
                build = Build.valueOf(values[2] as String),
                hair = HairStyle.valueOf(values[3] as String),
                outfit = Outfit.valueOf(values[4] as String),
                footwear = Footwear.valueOf(values[5] as String),
                heightCm = values[6] as Int,
                skin = SkinChoice.valueOf(values[7] as String),
                hairColor = HairColorChoice.valueOf(values[8] as String),
            )
        }.getOrNull()
    },
)
