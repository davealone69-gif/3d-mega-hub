// The UI presets pass both an explicit spec and a prompt through AvatarForge's parser.
// Verify that parsing the actual UI prompts does not silently change their intended features.
#include "af_prompt.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

bool near(float a, float b) { return std::fabs(a - b) < 0.001f; }
}

int main() {
    af_spec scout{};
    af_spec_init(&scout);
    scout.sex = AF_SEX_FEMALE;
    scout.build = AF_BUILD_ATHLETIC;
    scout.hair_style = AF_HAIR_PONYTAIL;
    scout.outfit = AF_OUTFIT_ARMOR_LEATHER;
    scout.footwear = AF_FOOTWEAR_BOOT;
    scout.height_m = 1.75f;
    scout.seed = 4101;
    scout.hair_tone[0] = 0.045f;
    scout.gear_tone[0] = 0.16f;
    const int scoutMatches = af::parsePrompt(
        "Adult woman, athletic build, 175 cm tall, ponytail, leather armor and boots",
        &scout, nullptr);
    require(scoutMatches > 0, "Scout prompt matched no attributes");
    require(scout.sex == AF_SEX_FEMALE && scout.build == AF_BUILD_ATHLETIC,
            "Scout sex or build changed during prompt parsing");
    require(scout.hair_style == AF_HAIR_PONYTAIL && scout.outfit == AF_OUTFIT_ARMOR_LEATHER &&
            scout.footwear == AF_FOOTWEAR_BOOT, "Scout appearance changed during prompt parsing");
    require(near(scout.height_m, 1.75f) && scout.seed == 4101,
            "Scout height or seed changed during prompt parsing");
    require(near(scout.hair_tone[0], 0.045f) && near(scout.gear_tone[0], 0.16f),
            "Scout explicit colours changed during prompt parsing");

    af_spec sentinel{};
    af_spec_init(&sentinel);
    sentinel.sex = AF_SEX_MALE;
    sentinel.build = AF_BUILD_ATHLETIC;
    sentinel.hair_style = AF_HAIR_SHORT;
    sentinel.outfit = AF_OUTFIT_ARMOR_PLATE;
    sentinel.footwear = AF_FOOTWEAR_BOOT;
    sentinel.height_m = 1.82f;
    sentinel.seed = 4102;
    sentinel.hair_tone[0] = 0.045f;
    sentinel.gear_tone[0] = 0.12f;
    const int sentinelMatches = af::parsePrompt(
        "Adult man, athletic build, 182 cm tall, short hair, plate armor and boots",
        &sentinel, nullptr);
    require(sentinelMatches > 0, "Sentinel prompt matched no attributes");
    require(sentinel.sex == AF_SEX_MALE && sentinel.build == AF_BUILD_ATHLETIC,
            "Sentinel sex or build changed during prompt parsing");
    require(sentinel.hair_style == AF_HAIR_SHORT && sentinel.outfit == AF_OUTFIT_ARMOR_PLATE &&
            sentinel.footwear == AF_FOOTWEAR_BOOT, "Sentinel appearance changed during prompt parsing");
    require(near(sentinel.height_m, 1.82f) && sentinel.seed == 4102,
            "Sentinel height or seed changed during prompt parsing");
    require(near(sentinel.hair_tone[0], 0.045f) && near(sentinel.gear_tone[0], 0.12f),
            "Sentinel explicit colours changed during prompt parsing");

    // Exercise every option exposed by the editor, including combinations whose words can
    // collide in the native parser (e.g. "medium hair", "athletic" and "plate armor").
    struct Choice { const char* words; int value; };
    const Choice people[] = {{"woman", AF_SEX_FEMALE}, {"man", AF_SEX_MALE}};
    const Choice builds[] = {{"average", AF_BUILD_AVERAGE}, {"slim", AF_BUILD_SLIM},
        {"athletic", AF_BUILD_ATHLETIC}, {"muscular", AF_BUILD_MUSCULAR}, {"heavy", AF_BUILD_HEAVY}};
    const Choice hair[] = {{"bald", AF_HAIR_NONE}, {"short hair", AF_HAIR_SHORT},
        {"medium hair", AF_HAIR_MEDIUM}, {"long hair", AF_HAIR_LONG},
        {"ponytail", AF_HAIR_PONYTAIL}, {"bun", AF_HAIR_BUN}, {"braids", AF_HAIR_BRAIDS}};
    const Choice outfits[] = {{"leather armor", AF_OUTFIT_ARMOR_LEATHER},
        {"plate armor", AF_OUTFIT_ARMOR_PLATE}, {"jacket and jeans", AF_OUTFIT_JEANS_JACKET},
        {"spacesuit", AF_OUTFIT_SPACESUIT}, {"t-shirt", AF_OUTFIT_TSHIRT}};
    const Choice shoes[] = {{"boots", AF_FOOTWEAR_BOOT}, {"sneakers", AF_FOOTWEAR_SNEAKER}};
    int checked = 0;
    for (const auto& person : people) for (const auto& build : builds)
    for (const auto& hairstyle : hair) for (const auto& outfit : outfits)
    for (const auto& footwear : shoes) {
        af_spec spec{};
        af_spec_init(&spec);
        spec.sex = person.value;
        spec.build = build.value;
        spec.hair_style = hairstyle.value;
        spec.outfit = outfit.value;
        spec.footwear = footwear.value;
        spec.height_m = 1.68f;
        spec.skin_melanin = 0.75f;
        spec.hair_tone[0] = 0.56f;
        const std::string prompt = std::string("Adult ") + person.words + ", " + build.words +
            " build, 168 cm tall, " + hairstyle.words + ", " + outfit.words + " and " + footwear.words;
        af::parsePrompt(prompt.c_str(), &spec, nullptr);
        if (spec.sex != person.value || spec.build != build.value || spec.hair_style != hairstyle.value ||
            spec.outfit != outfit.value || spec.footwear != footwear.value || !near(spec.height_m, 1.68f) ||
            !near(spec.skin_melanin, 0.75f) || !near(spec.hair_tone[0], 0.56f)) {
            std::fprintf(stderr, "FAIL: editor prompt changed an explicit setting: %s\n", prompt.c_str());
            return 1;
        }
        ++checked;
    }
    std::printf("Character preset prompt regression passed (%d editor combinations).\n", checked);
    return 0;
}
