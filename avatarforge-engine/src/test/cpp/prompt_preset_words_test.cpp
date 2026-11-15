// Every hair, outfit and footwear preset must be reachable from the prompt box.
//
// The prompt parser is a keyword parser. The master inventory
// (docs/AVATAR_ASSETS_ANIMATION_MASTER.html) says which words select which preset; this test is
// the evidence for that column. It analyses one plain prompt per preset through the public C
// entry point the app uses and compares the resolved preset number.
#include "afcore.h"

#include <cstdio>
#include <cstring>

namespace {

enum Field { HAIR, OUTFIT, FOOTWEAR };

struct Case {
    const char* prompt;
    Field field;
    int expected;
    const char* presetName;
};

const Case kCases[] = {
    /* Hair styles. */
    {"bald man", HAIR, AF_HAIR_NONE, "none"},
    {"woman with a buzz cut", HAIR, AF_HAIR_BUZZ, "buzz"},
    {"woman with short hair", HAIR, AF_HAIR_SHORT, "short"},
    {"woman with medium hair", HAIR, AF_HAIR_MEDIUM, "medium"},
    {"woman with long hair", HAIR, AF_HAIR_LONG, "long"},
    {"woman with a ponytail", HAIR, AF_HAIR_PONYTAIL, "ponytail"},
    {"woman with her hair in a bun", HAIR, AF_HAIR_BUN, "bun"},
    {"woman with braids", HAIR, AF_HAIR_BRAIDS, "braids"},
    {"woman with an afro", HAIR, AF_HAIR_AFRO, "afro"},
    {"man with a mohawk", HAIR, AF_HAIR_MOHAWK, "mohawk"},
    {"woman with a bob", HAIR, AF_HAIR_BOB, "bob"},
    {"woman with bangs", HAIR, AF_HAIR_BANGS, "bangs"},
    {"woman with a fringe", HAIR, AF_HAIR_BANGS, "bangs (fringe)"},
    {"woman with curls", HAIR, AF_HAIR_CURLY, "curly"},
    /* Outfits. */
    {"woman in a t-shirt", OUTFIT, AF_OUTFIT_TSHIRT, "t-shirt"},
    {"woman in a black t-shirt and jeans", OUTFIT, AF_OUTFIT_TSHIRT, "t-shirt with jeans"},
    {"woman in a dress", OUTFIT, AF_OUTFIT_DRESS, "dress"},
    {"man in a suit", OUTFIT, AF_OUTFIT_SUIT, "suit"},
    {"woman in a denim jacket", OUTFIT, AF_OUTFIT_JEANS_JACKET, "jeans jacket"},
    {"knight in plate armour", OUTFIT, AF_OUTFIT_ARMOR_PLATE, "plate armour"},
    {"ranger in leather armour", OUTFIT, AF_OUTFIT_ARMOR_LEATHER, "leather armour"},
    {"astronaut in a spacesuit", OUTFIT, AF_OUTFIT_SPACESUIT, "spacesuit"},
    {"woman in sportswear", OUTFIT, AF_OUTFIT_ATHLETIC, "athletic"},
    {"man in a long coat", OUTFIT, AF_OUTFIT_COAT, "coat"},
    {"woman in a bikini", OUTFIT, AF_OUTFIT_SWIMWEAR, "swimwear"},
    {"man in swimwear", OUTFIT, AF_OUTFIT_SWIMWEAR, "swimwear"},
    /* Footwear. */
    {"woman in a t-shirt wearing sneakers", FOOTWEAR, AF_FOOTWEAR_SNEAKER, "sneaker"},
    {"woman in a coat wearing boots", FOOTWEAR, AF_FOOTWEAR_BOOT, "boot"},
    {"knight in plate armour with sabatons", FOOTWEAR, AF_FOOTWEAR_SABATON, "sabaton"},
    {"woman in a dress wearing heels", FOOTWEAR, AF_FOOTWEAR_HEEL, "heel"},
};

const char* fieldName(Field f) {
    return f == HAIR ? "hair_style" : f == OUTFIT ? "outfit" : "footwear";
}

}  // namespace

int main() {
    af_spec spec{};
    af_spec_init(&spec);
    af_engine* engine = nullptr;
    if (af_engine_create(&spec, nullptr, &engine) != AF_OK || !engine) {
        std::printf("FAIL: af_engine_create\n");
        return 1;
    }

    int failures = 0;
    for (const Case& c : kCases) {
        af_spec resolved{};
        const int32_t matched = af_engine_analyze_prompt(engine, c.prompt, &resolved);
        const int got = c.field == HAIR ? static_cast<int>(resolved.hair_style)
                      : c.field == OUTFIT ? static_cast<int>(resolved.outfit)
                                          : static_cast<int>(resolved.footwear);
        const bool ok = matched >= 0 && got == c.expected;
        std::printf("%s  %-9s %-18s \"%s\" -> %s=%d (expected %d, matched=%d)\n",
                    ok ? "ok  " : "FAIL", fieldName(c.field), c.presetName, c.prompt,
                    fieldName(c.field), got, c.expected, static_cast<int>(matched));
        if (!ok) ++failures;
    }
    af_engine_destroy(engine);

    const int total = static_cast<int>(sizeof(kCases) / sizeof(kCases[0]));
    std::printf("%d of %d preset prompts resolved as expected\n", total - failures, total);
    return failures == 0 ? 0 : 1;
}
