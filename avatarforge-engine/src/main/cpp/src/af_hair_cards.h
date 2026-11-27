/*
 * af_hair_cards.h -- procedural layered strand hair card generator.
 *
 * Generates watertight, outward-wound, collision-cleared 3D tapered strand ribbons
 * anchored to the skull and hairline for every selectable AvatarForge hairstyle
 * (AF_HAIR_BUZZ through AF_HAIR_MOHAWK). Each card carries continuous parametric
 * root-to-tip UVs across 4 distinct strand-sheet variants so texture baking, glTF
 * alpha-mask export, and Kajiya-Kay anisotropic shading follow the true strand flow.
 */
#ifndef AF_HAIR_CARDS_H
#define AF_HAIR_CARDS_H

#include "af_field.h"
#include "af_mesh.h"

namespace af {

/** Number of distinct strand-card texture strips packed side-by-side in the hair card UV tile. */
constexpr int kHairCardVariants = 4;

struct HairCardReport {
    int cardCount = 0;
    int vertexCount = 0;
    int triangleCount = 0;
};

/**
 * Appends layered watertight hair strand cards to `mesh` in REGION_HAIR_CARD.
 * `floorShiftY` matches the vertical floor drop already applied to `mesh->positions`.
 * Does nothing when `field.apparel().hair == 0` (bald).
 */
HairCardReport appendProceduralHairCards(const BodyField& field, float floorShiftY, Mesh* mesh);

} // namespace af

#endif /* AF_HAIR_CARDS_H */
