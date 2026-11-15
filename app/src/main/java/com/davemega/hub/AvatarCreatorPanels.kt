package com.davemega.hub

import androidx.annotation.DrawableRes
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.davemega.hub.avatarforge.FaceParam
import com.davemega.hub.avatarforge.Footwear
import com.davemega.hub.avatarforge.HairStyle
import com.davemega.hub.avatarforge.Outfit
import com.davemega.hub.avatarforge.Sex
import kotlin.math.pow

/**
 * The avatar creator's own look: a light, rounded sheet with one strong accent, after the
 * "Create Your Avatar" layout the owner supplied as the guide. It is applied to the creator only;
 * the rest of the app keeps its theme.
 */
val CreatorColours = lightColorScheme(
    primary = Color(0xFF6A3DF0),
    onPrimary = Color.White,
    primaryContainer = Color(0xFFE9E1FF),
    onPrimaryContainer = Color(0xFF22005D),
    secondary = Color(0xFFFF2FA6),
    onSecondary = Color.White,
    background = Color(0xFFF6F2FF),
    onBackground = Color(0xFF1B1B2A),
    surface = Color.White,
    onSurface = Color(0xFF1B1B2A),
    surfaceVariant = Color(0xFFF1EEF9),
    onSurfaceVariant = Color(0xFF5B5870),
    outline = Color(0xFFCFC8E6),
    error = Color(0xFFB3261E)
)

/** The pastel wash behind the creator. */
val CreatorBackdrop: Brush = Brush.linearGradient(
    listOf(Color(0xFFF3E9FF), Color(0xFFE6FBF4), Color(0xFFE8F0FF))
)

/**
 * The creator's dark look: near-black with white text and one red accent, after the dark picture
 * creator the owner sent as a reference and the owner's own artwork (red smoke on black). The
 * layout is the same; the header has the switch between this and the light look.
 */
val CreatorColoursDark = darkColorScheme(
    primary = Color(0xFFE5263F),
    onPrimary = Color.White,
    primaryContainer = Color(0xFF5A0F1B),
    onPrimaryContainer = Color(0xFFFFDADD),
    secondary = Color(0xFFFF6A3D),
    onSecondary = Color.White,
    background = Color(0xFF0D0D0F),
    onBackground = Color(0xFFF4F4F6),
    surface = Color(0xFF17171B),
    onSurface = Color(0xFFF4F4F6),
    surfaceVariant = Color(0xFF26262C),
    onSurfaceVariant = Color(0xFFB9B9C4),
    outline = Color(0xFF3C3C46),
    error = Color(0xFFFF8A80)
)

/** The backdrop of the dark look: black, with a little of the accent at the top. */
val CreatorBackdropDark: Brush = Brush.verticalGradient(
    listOf(Color(0xFF1C0C10), Color(0xFF0D0D0F), Color(0xFF0D0D0F))
)

/** The colour of the stage the renderer draws on, so the frame round the viewport matches it. */
val CreatorStage = Color(0xFF1C2230)

/**
 * The sections of the creator, in the order the rail lists them. Each one is a set of controls
 * that exists in the engine today. Look, Body and Hair carry the choices of the owner's picture
 * reference (ethnicity; body type, breast size, butt size; hair colour and style); Face has the
 * face proportions and the eye colour. The first guide also shows Accessories and Background;
 * the engine has no options for those yet, so they are not listed rather than shown empty.
 */
enum class CreatorCategory(val label: String, val glyph: String) {
    LOOK("Look", "\uD83C\uDF0D"),
    BODY("Body", "\uD83D\uDCAA"),
    HAIR("Hair", "\u2702\uFE0F"),
    FACE("Face", "\uD83D\uDE42"),
    SKIN("Skin", "\uD83D\uDCA7"),
    OUTFIT("Outfit", "\uD83D\uDC55"),
    SHOES("Shoes", "\uD83D\uDC5F"),
    MOTION("Motion", "\u25B6\uFE0F")
}

/**
 * A picture for every preset: a render of what the engine makes for it, drawn by this project's
 * host tool at the quality the app generates at. They are previews of the preset on the default
 * body, not of the avatar being built.
 */
object PresetThumbnails {
    @DrawableRes
    fun hair(style: HairStyle): Int = when (style) {
        HairStyle.NONE -> R.drawable.preset_hair_none
        HairStyle.BUZZ -> R.drawable.preset_hair_buzz
        HairStyle.SHORT -> R.drawable.preset_hair_short
        HairStyle.MEDIUM -> R.drawable.preset_hair_medium
        HairStyle.LONG -> R.drawable.preset_hair_long
        HairStyle.PONYTAIL -> R.drawable.preset_hair_ponytail
        HairStyle.BUN -> R.drawable.preset_hair_bun
        HairStyle.BRAIDS -> R.drawable.preset_hair_braids
        HairStyle.AFRO -> R.drawable.preset_hair_afro
        HairStyle.MOHAWK -> R.drawable.preset_hair_mohawk
        HairStyle.BOB -> R.drawable.preset_hair_bob
        HairStyle.BANGS -> R.drawable.preset_hair_bangs
        HairStyle.CURLY -> R.drawable.preset_hair_curly
        HairStyle.BOX_BRAIDS -> R.drawable.preset_hair_box_braids
    }

    @DrawableRes
    fun ethnicity(ethnicity: Ethnicity): Int = when (ethnicity) {
        Ethnicity.WHITE -> R.drawable.preset_look_white
        Ethnicity.ASIAN -> R.drawable.preset_look_asian
        Ethnicity.BLACK -> R.drawable.preset_look_black
        Ethnicity.LATINA -> R.drawable.preset_look_latina
        Ethnicity.SOUTH_ASIAN -> R.drawable.preset_look_south_asian
        Ethnicity.MIDDLE_EASTERN -> R.drawable.preset_look_middle_eastern
    }

    @DrawableRes
    fun bodyType(type: BodyType): Int = when (type) {
        BodyType.SLIM -> R.drawable.preset_body_slim
        BodyType.ATHLETIC -> R.drawable.preset_body_athletic
        BodyType.AVERAGE -> R.drawable.preset_body_average
        BodyType.CURVY -> R.drawable.preset_body_curvy
        BodyType.PLUS_SIZE -> R.drawable.preset_body_plus_size
        BodyType.MUSCULAR -> R.drawable.preset_body_muscular
    }

    @DrawableRes
    fun bust(size: BustSize): Int = when (size) {
        BustSize.FLAT -> R.drawable.preset_bust_flat
        BustSize.SMALL -> R.drawable.preset_bust_small
        BustSize.AVERAGE -> R.drawable.preset_bust_average
        BustSize.BIG -> R.drawable.preset_bust_big
        BustSize.HUGE -> R.drawable.preset_bust_huge
    }

    @DrawableRes
    fun glute(size: GluteSize): Int = when (size) {
        GluteSize.SMALL -> R.drawable.preset_glute_small
        GluteSize.SLIM -> R.drawable.preset_glute_slim
        GluteSize.ATHLETIC -> R.drawable.preset_glute_athletic
        GluteSize.BIG -> R.drawable.preset_glute_big
        GluteSize.HUGE -> R.drawable.preset_glute_huge
    }

    /** Null for the unclothed base, which the creator does not offer. */
    fun outfit(outfit: Outfit): Int? = when (outfit) {
        Outfit.NONE -> null
        Outfit.TSHIRT -> R.drawable.preset_outfit_tshirt
        Outfit.DRESS -> R.drawable.preset_outfit_dress
        Outfit.SUIT -> R.drawable.preset_outfit_suit
        Outfit.JEANS_JACKET -> R.drawable.preset_outfit_jeans_jacket
        Outfit.ARMOR_PLATE -> R.drawable.preset_outfit_armor_plate
        Outfit.ARMOR_LEATHER -> R.drawable.preset_outfit_armor_leather
        Outfit.SPACESUIT -> R.drawable.preset_outfit_spacesuit
        Outfit.ATHLETIC -> R.drawable.preset_outfit_athletic
        Outfit.COAT -> R.drawable.preset_outfit_coat
        Outfit.SWIMWEAR -> R.drawable.preset_outfit_swimwear
    }

    @DrawableRes
    fun footwear(footwear: Footwear): Int = when (footwear) {
        Footwear.NONE -> R.drawable.preset_footwear_none
        Footwear.SNEAKER -> R.drawable.preset_footwear_sneaker
        Footwear.BOOT -> R.drawable.preset_footwear_boot
        Footwear.SABATON -> R.drawable.preset_footwear_sabaton
        Footwear.HEEL -> R.drawable.preset_footwear_heel
    }
}

/** Names as the creator shows them. */
object CreatorLabels {
    fun hair(style: HairStyle): String = when (style) {
        HairStyle.NONE -> "Bald"
        HairStyle.BUZZ -> "Buzz cut"
        HairStyle.SHORT -> "Short"
        HairStyle.MEDIUM -> "Shoulder"
        HairStyle.LONG -> "Straight"
        HairStyle.PONYTAIL -> "Ponytail"
        HairStyle.BUN -> "Bun"
        HairStyle.BRAIDS -> "Twin braids"
        HairStyle.AFRO -> "Afro"
        HairStyle.MOHAWK -> "Mohawk"
        HairStyle.BOB -> "Bob"
        HairStyle.BANGS -> "Bangs"
        HairStyle.CURLY -> "Curly"
        HairStyle.BOX_BRAIDS -> "Braids"
    }

    /** The styles in the order the creator lists them: the eight of the owner's reference first. */
    val hairOrder: List<HairStyle> = listOf(
        HairStyle.LONG, HairStyle.BANGS, HairStyle.BOX_BRAIDS, HairStyle.CURLY,
        HairStyle.BUN, HairStyle.PONYTAIL, HairStyle.BOB, HairStyle.NONE,
        HairStyle.MEDIUM, HairStyle.SHORT, HairStyle.BUZZ, HairStyle.BRAIDS, HairStyle.AFRO, HairStyle.MOHAWK
    )

    fun face(param: FaceParam): String = when (param) {
        FaceParam.NOSE_WIDTH -> "Nose width"
        FaceParam.NOSE_BRIDGE -> "Nose bridge"
        FaceParam.LIP_FULLNESS -> "Lips"
        FaceParam.EYE_TILT -> "Eye tilt"
        FaceParam.EYE_OPENNESS -> "Eye opening"
        FaceParam.CHEEKBONES -> "Cheekbones"
        FaceParam.JAW_WIDTH -> "Jaw"
    }

    fun outfit(outfit: Outfit): String = when (outfit) {
        Outfit.NONE -> "None"
        Outfit.TSHIRT -> "T-shirt"
        Outfit.DRESS -> "Gown"
        Outfit.SUIT -> "Suit"
        Outfit.JEANS_JACKET -> "Denim jacket"
        Outfit.ARMOR_PLATE -> "Plate armour"
        Outfit.ARMOR_LEATHER -> "Leather armour"
        Outfit.SPACESUIT -> "Spacesuit"
        Outfit.ATHLETIC -> "Sportswear"
        Outfit.COAT -> "Coat"
        Outfit.SWIMWEAR -> "Swimwear"
    }

    fun footwear(footwear: Footwear): String = when (footwear) {
        Footwear.NONE -> "Barefoot"
        Footwear.SNEAKER -> "Sneakers"
        Footwear.BOOT -> "Boots"
        Footwear.SABATON -> "Sabatons"
        Footwear.HEEL -> "Heels"
    }

    /** "turn_360" -> "Turn 360", "t_pose" -> "T pose". */
    fun animation(name: String): String =
        name.replace('_', ' ').trim().replaceFirstChar { it.uppercase() }.ifBlank { "Animation" }
}

/** A linear-light colour component as the screen should show it. */
fun linearToDisplay(component: Float): Float = component.coerceIn(0f, 1f).pow(1f / 2.2f)

/** The engine's colours are linear RGB; a swatch has to show them the way the render will look. */
fun AppearanceCatalogue.Colour.displayColour(): Color =
    Color(linearToDisplay(red), linearToDisplay(green), linearToDisplay(blue))

/**
 * Skin as the engine tints it at a melanin value, closely enough for a swatch: a blend from a pale
 * to a deep brown. The render is the authority; this only tells the swatches apart.
 */
fun skinSwatch(melanin: Float): Color {
    val t = melanin.coerceIn(0f, 1f)
    fun mix(a: Float, b: Float) = a + (b - a) * t
    return Color(mix(0.98f, 0.24f), mix(0.84f, 0.14f), mix(0.74f, 0.09f))
}

/** The rail of sections beside the viewport. */
@Composable
fun CreatorRail(
    selected: CreatorCategory,
    onSelect: (CreatorCategory) -> Unit,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier
            .clip(RoundedCornerShape(24.dp))
            .background(MaterialTheme.colorScheme.surface.copy(alpha = 0.86f))
            .padding(6.dp),
        verticalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        CreatorCategory.entries.forEach { category ->
            val active = category == selected
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f)
                    .clip(RoundedCornerShape(18.dp))
                    .background(if (active) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surfaceVariant)
                    .clickable { onSelect(category) }
                    .padding(vertical = 4.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.Center
            ) {
                Text(category.glyph, fontSize = 18.sp)
                Text(
                    category.label,
                    style = MaterialTheme.typography.labelSmall,
                    fontWeight = if (active) FontWeight.Bold else FontWeight.Medium,
                    color = if (active) MaterialTheme.colorScheme.onPrimary else MaterialTheme.colorScheme.onSurface,
                    maxLines = 1
                )
            }
        }
    }
}

/** One preset: its picture, its name, and a ring with a tick when it is the chosen one. */
@Composable
fun PresetCard(
    label: String,
    @DrawableRes image: Int,
    selected: Boolean,
    enabled: Boolean,
    width: Dp,
    imageHeight: Dp,
    onClick: () -> Unit
) {
    val ring = if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outline
    Column(
        modifier = Modifier
            .width(width)
            .clip(RoundedCornerShape(16.dp))
            .background(MaterialTheme.colorScheme.surface)
            .border(if (selected) 3.dp else 1.dp, ring, RoundedCornerShape(16.dp))
            .clickable(enabled = enabled, onClick = onClick)
            .padding(4.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Box {
            Image(
                painter = painterResource(image),
                contentDescription = label,
                contentScale = ContentScale.Crop,
                modifier = Modifier
                    .fillMaxWidth()
                    .height(imageHeight)
                    .clip(RoundedCornerShape(12.dp))
                    .background(CreatorStage)
            )
            if (selected) {
                Box(
                    modifier = Modifier
                        .align(Alignment.TopEnd)
                        .padding(4.dp)
                        .size(20.dp)
                        .clip(CircleShape)
                        .background(MaterialTheme.colorScheme.primary),
                    contentAlignment = Alignment.Center
                ) {
                    Text("\u2713", color = MaterialTheme.colorScheme.onPrimary, fontSize = 12.sp, fontWeight = FontWeight.Bold)
                }
            }
        }
        Text(
            label,
            style = MaterialTheme.typography.labelMedium,
            color = MaterialTheme.colorScheme.onSurface,
            fontWeight = if (selected) FontWeight.Bold else FontWeight.Normal,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
            textAlign = TextAlign.Center,
            modifier = Modifier.fillMaxWidth().padding(top = 4.dp, bottom = 2.dp)
        )
    }
}

/**
 * A preset as a picture with its name written over the bottom of it, the way the owner's picture
 * reference draws them. A ring and a tick mark the chosen one.
 */
@Composable
fun PictureCard(
    label: String,
    @DrawableRes image: Int,
    selected: Boolean,
    enabled: Boolean,
    imageHeight: Dp,
    modifier: Modifier = Modifier,
    onClick: () -> Unit
) {
    val shape = RoundedCornerShape(14.dp)
    Box(
        modifier = modifier
            .height(imageHeight)
            .clip(shape)
            .background(CreatorStage)
            .border(
                if (selected) 3.dp else 1.dp,
                if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outline,
                shape
            )
            .clickable(enabled = enabled, onClick = onClick)
    ) {
        Image(
            painter = painterResource(image),
            contentDescription = label,
            contentScale = ContentScale.Crop,
            modifier = Modifier.fillMaxSize()
        )
        Box(
            modifier = Modifier
                .align(Alignment.BottomCenter)
                .fillMaxWidth()
                .background(Brush.verticalGradient(listOf(Color.Transparent, Color(0xE6000000))))
                .padding(start = 3.dp, end = 3.dp, top = 16.dp, bottom = 5.dp),
            contentAlignment = Alignment.Center
        ) {
            Text(
                label,
                color = Color.White,
                style = MaterialTheme.typography.labelMedium,
                fontWeight = FontWeight.SemiBold,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
                textAlign = TextAlign.Center
            )
        }
        if (selected) {
            Box(
                modifier = Modifier
                    .align(Alignment.TopEnd)
                    .padding(4.dp)
                    .size(20.dp)
                    .clip(CircleShape)
                    .background(MaterialTheme.colorScheme.primary),
                contentAlignment = Alignment.Center
            ) {
                Text("\u2713", color = MaterialTheme.colorScheme.onPrimary, fontSize = 12.sp, fontWeight = FontWeight.Bold)
            }
        }
    }
}

/**
 * Colours as tiles, the chosen one wider and carrying its name: the hair colours of the picture
 * reference. A colour of all zeros is the engine's own choice and is drawn as "Auto".
 */
@Composable
fun ColourTileRow(
    colours: List<AppearanceCatalogue.Colour>,
    selected: Int,
    enabled: Boolean,
    onSelect: (Int) -> Unit
) {
    Row(
        modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        colours.forEachIndexed { index, colour ->
            val auto = colour.red == 0f && colour.green == 0f && colour.blue == 0f
            val active = index == selected
            val base = if (auto) MaterialTheme.colorScheme.surfaceVariant else colour.displayColour()
            // A lighter band across the middle, like the sheen on a lock of hair.
            val sheen = Color(
                red = base.red + (1f - base.red) * 0.28f,
                green = base.green + (1f - base.green) * 0.28f,
                blue = base.blue + (1f - base.blue) * 0.28f
            )
            val shape = RoundedCornerShape(10.dp)
            Box(
                modifier = Modifier
                    .width(if (active || auto) 92.dp else 50.dp)
                    .height(46.dp)
                    .clip(shape)
                    .background(Brush.verticalGradient(listOf(base, if (auto) base else sheen, base)))
                    .border(
                        if (active) 3.dp else 1.dp,
                        if (active) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outline,
                        shape
                    )
                    .clickable(enabled = enabled) { onSelect(index) },
                contentAlignment = Alignment.Center
            ) {
                if (active || auto) {
                    val dark = auto || colour.red + colour.green + colour.blue < 0.9f
                    Text(
                        colour.label,
                        style = MaterialTheme.typography.labelMedium,
                        fontWeight = FontWeight.SemiBold,
                        color = if (auto) MaterialTheme.colorScheme.onSurfaceVariant else if (dark) Color.White else Color.Black,
                        maxLines = 1
                    )
                }
            }
        }
    }
}

/** A row of colour dots. A colour of all zeros is the engine's own choice and is drawn as "Auto". */
@Composable
fun SwatchRow(
    colours: List<AppearanceCatalogue.Colour>,
    selected: Int,
    enabled: Boolean,
    onSelect: (Int) -> Unit
) {
    Row(
        modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
        horizontalArrangement = Arrangement.spacedBy(10.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        colours.forEachIndexed { index, colour ->
            val auto = colour.red == 0f && colour.green == 0f && colour.blue == 0f
            val active = index == selected
            Box(
                modifier = Modifier
                    .size(44.dp)
                    .clip(CircleShape)
                    .border(
                        if (active) 3.dp else 1.dp,
                        if (active) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outline,
                        CircleShape
                    )
                    .padding(5.dp)
                    .clip(CircleShape)
                    .background(if (auto) MaterialTheme.colorScheme.surfaceVariant else colour.displayColour())
                    .clickable(enabled = enabled) { onSelect(index) },
                contentAlignment = Alignment.Center
            ) {
                if (auto) {
                    Text("Auto", fontSize = 9.sp, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 1)
                } else if (active) {
                    val dark = colour.red + colour.green + colour.blue < 0.9f
                    Text("\u2713", fontSize = 14.sp, fontWeight = FontWeight.Bold, color = if (dark) Color.White else Color.Black)
                }
            }
        }
    }
}

/** A round button drawn over the stage: turn, zoom, reset. */
@Composable
fun StageButton(glyph: String, onClick: () -> Unit) {
    Box(
        modifier = Modifier
            .size(38.dp)
            .clip(CircleShape)
            .background(Color.White.copy(alpha = 0.86f))
            .clickable(onClick = onClick),
        contentAlignment = Alignment.Center
    ) {
        Text(glyph, fontSize = 18.sp, fontWeight = FontWeight.Bold, color = Color(0xFF1B1B2A))
    }
}

/** A pill that is filled when chosen: used for sex, build, speed and the animation list. */
@Composable
fun CreatorChip(label: String, selected: Boolean, enabled: Boolean = true, onClick: () -> Unit) {
    Box(
        modifier = Modifier
            .clip(RoundedCornerShape(50))
            .background(if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surfaceVariant)
            .border(1.dp, if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outline, RoundedCornerShape(50))
            .clickable(enabled = enabled, onClick = onClick)
            .padding(horizontal = 14.dp, vertical = 8.dp),
        contentAlignment = Alignment.Center
    ) {
        Text(
            label,
            style = MaterialTheme.typography.labelLarge,
            color = if (selected) MaterialTheme.colorScheme.onPrimary else MaterialTheme.colorScheme.onSurface,
            maxLines = 1
        )
    }
}

@Composable
private fun PanelTitle(text: String) {
    Text(text, style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold, color = MaterialTheme.colorScheme.onSurface)
}

@Composable
private fun PanelCaption(text: String) {
    Text(text, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
}

@Composable
private fun ScrollRow(content: @Composable () -> Unit) {
    Row(
        modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) { content() }
}

/**
 * Look: the six starting points of the picture reference. Choosing one sets the skin tone, the
 * hair and eye colour and the face; each of those can be changed afterwards in its own section.
 */
@Composable
fun LookPanel(selection: AppearanceSelection, enabled: Boolean, onChange: (AppearanceSelection) -> Unit) {
    PanelTitle("Ethnicity")
    Ethnicity.entries.chunked(3).forEach { rowItems ->
        Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            rowItems.forEach { item ->
                PictureCard(
                    label = item.labelFor(selection.sex),
                    image = PresetThumbnails.ethnicity(item),
                    selected = selection.ethnicity == item,
                    enabled = enabled,
                    imageHeight = 132.dp,
                    modifier = Modifier.weight(1f)
                ) { onChange(item.applyTo(selection)) }
            }
        }
    }
    PanelCaption(
        "A starting point. It sets the skin tone, the hair and eye colour and the face; change any of them afterwards in Skin, Hair and Face."
    )
}

/** Hair: the colours as tiles, then the fourteen styles as pictures. */
@Composable
fun HairPanel(selection: AppearanceSelection, enabled: Boolean, onChange: (AppearanceSelection) -> Unit) {
    PanelTitle("Hair color")
    ColourTileRow(AppearanceCatalogue.hairColours, selection.hairColour, enabled) { onChange(selection.copy(hairColour = it)) }
    PanelTitle("Hair style")
    ScrollRow {
        CreatorLabels.hairOrder.forEach { style ->
            PictureCard(
                label = CreatorLabels.hair(style),
                image = PresetThumbnails.hair(style),
                selected = selection.hairStyle == style,
                enabled = enabled,
                imageHeight = 118.dp,
                modifier = Modifier.width(88.dp)
            ) { onChange(selection.copy(hairStyle = style)) }
        }
    }
}

/** Body: sex, body type, breast size, butt size and height. */
@Composable
fun BodyPanel(selection: AppearanceSelection, enabled: Boolean, onChange: (AppearanceSelection) -> Unit) {
    PanelTitle("Body type")
    ScrollRow {
        Sex.entries.forEach { sex ->
            CreatorChip(AppearanceCatalogue.label(sex.name), selection.sex == sex, enabled) { onChange(selection.copy(sex = sex)) }
        }
    }
    ScrollRow {
        BodyType.entries.forEach { type ->
            PictureCard(
                label = type.label,
                image = PresetThumbnails.bodyType(type),
                selected = selection.bodyType == type,
                enabled = enabled,
                imageHeight = 112.dp,
                modifier = Modifier.width(66.dp)
            ) { onChange(selection.copy(bodyType = type)) }
        }
    }
    if (selection.sex != Sex.MALE) {
        PanelTitle("Breast size")
        ScrollRow {
            BustSize.entries.forEach { size ->
                PictureCard(
                    label = size.label,
                    image = PresetThumbnails.bust(size),
                    selected = selection.bust == size,
                    enabled = enabled,
                    imageHeight = 96.dp,
                    modifier = Modifier.width(66.dp)
                ) { onChange(selection.copy(bust = size)) }
            }
        }
    }
    PanelTitle("Butt size")
    ScrollRow {
        GluteSize.entries.forEach { size ->
            PictureCard(
                label = size.label,
                image = PresetThumbnails.glute(size),
                selected = selection.glute == size,
                enabled = enabled,
                imageHeight = 96.dp,
                modifier = Modifier.width(66.dp)
            ) { onChange(selection.copy(glute = size)) }
        }
    }
    PanelCaption("Height: ${"%.2f".format(selection.heightM)} m")
    Slider(
        value = selection.heightM.coerceIn(AppearanceCatalogue.MIN_HEIGHT_M, AppearanceCatalogue.MAX_HEIGHT_M),
        onValueChange = { onChange(selection.copy(heightM = it)) },
        valueRange = AppearanceCatalogue.MIN_HEIGHT_M..AppearanceCatalogue.MAX_HEIGHT_M,
        enabled = enabled
    )
    PanelCaption("The pictures show each choice on the default female body. Every avatar made here is an adult.")
}

/** Face: the eye colour, and one slider for each face proportion the engine can change. */
@Composable
fun FacePanel(selection: AppearanceSelection, enabled: Boolean, onChange: (AppearanceSelection) -> Unit) {
    PanelTitle("Eye color")
    ColourTileRow(AppearanceCatalogue.eyeColours, selection.eyeColour, enabled) { onChange(selection.copy(eyeColour = it)) }
    PanelTitle("Face")
    FaceParam.entries.forEach { param ->
        val value = selection.face.getOrElse(param.index) { 0f }
        PanelCaption("${CreatorLabels.face(param)}: ${"%+.0f".format(value * 100f)}")
        Slider(
            value = value.coerceIn(-1f, 1f),
            onValueChange = { onChange(selection.withFace(param, it)) },
            valueRange = -1f..1f,
            enabled = enabled
        )
    }
    PanelCaption("0 is the engine's default face. A look chosen under Look sets these; they can be moved from there.")
}

/** Skin: six tones to tap and a slider for anything between them. */
@Composable
fun SkinPanel(selection: AppearanceSelection, enabled: Boolean, onChange: (AppearanceSelection) -> Unit) {
    PanelTitle("Skin tone")
    val stops = listOf(0.08f, 0.22f, 0.35f, 0.52f, 0.70f, 0.90f)
    Row(horizontalArrangement = Arrangement.spacedBy(10.dp), verticalAlignment = Alignment.CenterVertically) {
        stops.forEach { stop ->
            val active = kotlin.math.abs(selection.skinMelanin - stop) < 0.035f
            Box(
                modifier = Modifier
                    .size(44.dp)
                    .clip(CircleShape)
                    .border(
                        if (active) 3.dp else 1.dp,
                        if (active) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outline,
                        CircleShape
                    )
                    .padding(5.dp)
                    .clip(CircleShape)
                    .background(skinSwatch(stop))
                    .clickable(enabled = enabled) { onChange(selection.copy(skinMelanin = stop)) }
            )
        }
    }
    PanelCaption("Tone: ${"%.0f".format(selection.skinMelanin * 100f)} %")
    Slider(
        value = selection.skinMelanin.coerceIn(0f, 1f),
        onValueChange = { onChange(selection.copy(skinMelanin = it)) },
        valueRange = 0f..1f,
        enabled = enabled
    )
}

/** Outfit: the ten outfits as pictures, then the clothing colours. */
@Composable
fun OutfitPanel(selection: AppearanceSelection, enabled: Boolean, onChange: (AppearanceSelection) -> Unit) {
    PanelTitle("Outfit")
    ScrollRow {
        AppearanceCatalogue.outfits.forEach { outfit ->
            val image = PresetThumbnails.outfit(outfit)
            if (image != null) {
                PresetCard(
                    label = CreatorLabels.outfit(outfit),
                    image = image,
                    selected = selection.outfit == outfit,
                    enabled = enabled,
                    width = 86.dp,
                    imageHeight = 178.dp
                ) { onChange(selection.copy(outfit = outfit)) }
            }
        }
    }
    PanelCaption("Clothing colour. \"Auto\" keeps each outfit's own colours.")
    SwatchRow(AppearanceCatalogue.clothColours, selection.clothColour, enabled) { onChange(selection.copy(clothColour = it)) }
}

/** Shoes: the five kinds of footwear as pictures. */
@Composable
fun ShoesPanel(selection: AppearanceSelection, enabled: Boolean, onChange: (AppearanceSelection) -> Unit) {
    PanelTitle("Shoes")
    ScrollRow {
        Footwear.entries.forEach { footwear ->
            PresetCard(
                label = CreatorLabels.footwear(footwear),
                image = PresetThumbnails.footwear(footwear),
                selected = selection.footwear == footwear,
                enabled = enabled,
                width = 110.dp,
                imageHeight = 84.dp
            ) { onChange(selection.copy(footwear = footwear)) }
        }
    }
}

/** A different avatar each time, keeping the sex and height the user chose. */
fun randomAppearance(current: AppearanceSelection, random: kotlin.random.Random = kotlin.random.Random.Default): AppearanceSelection {
    val look = Ethnicity.entries[random.nextInt(Ethnicity.entries.size)]
    return look.applyTo(current).copy(
        bodyType = BodyType.entries[random.nextInt(BodyType.entries.size)],
        bust = BustSize.entries[random.nextInt(BustSize.entries.size)],
        glute = GluteSize.entries[random.nextInt(GluteSize.entries.size)],
        hairStyle = HairStyle.entries[random.nextInt(HairStyle.entries.size)],
        outfit = AppearanceCatalogue.outfits[random.nextInt(AppearanceCatalogue.outfits.size)],
        footwear = Footwear.entries[random.nextInt(Footwear.entries.size)],
        clothColour = random.nextInt(AppearanceCatalogue.clothColours.size)
    )
}
