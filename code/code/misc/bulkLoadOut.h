//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD++ - All rights reserved, SneezyMUD Coding Team
//
//      "bulkLoadOut.h" - Random equipment generation for humanoid mobs
//
//////////////////////////////////////////////////////////////////////////

#pragma once

#include <array>
#include <span>

#include "enum.h"
#include "obj_low.h"
#include "race.h"
#include "wearTemplate.h"

class TMonster;
class TObj;

// Classes for whom a tier is their ceiling: allowed(tier) - allowed(tier + 1).
// Armor tiers nest downward -- heavy-wearers wear every tier below -- so it is
// this marginal set, not the cumulative allowed set, that ties an item to a
// class. Ranger holds no slot, leaving Medium with cleric alone.
inline constexpr std::array tierClothingClasses{MAGE_LEVEL_IND,
  SHAMAN_LEVEL_IND};
inline constexpr std::array tierLightClasses{MONK_LEVEL_IND, THIEF_LEVEL_IND};
inline constexpr std::array tierMediumClasses{CLERIC_LEVEL_IND};
inline constexpr std::array tierHeavyClasses{WARRIOR_LEVEL_IND,
  DEIKHAN_LEVEL_IND};

// Tier_Jewelry short-circuits tier derivation before any flag check and so has
// no defining class; weapon tiers have none either. Both yield an empty span,
// which callers must handle.
[[nodiscard]] constexpr std::span<const classIndT> tierDefiningClasses(
  Tier tier) {
  switch (tier) {
    case Tier_Clothing:
      return tierClothingClasses;
    case Tier_Light:
      return tierLightClasses;
    case Tier_Medium:
      return tierMediumClasses;
    case Tier_Heavy:
      return tierHeavyClasses;
    default:
      return {};
  }
}

// Attempt to generate bulk loot equipment on a humanoid mob.
// For each empty equipment slot, rolls a 3% chance to generate
// a random item appropriate for the mob's class, level, and race.
// Should be called after all zone-reset equipment loading is complete.
void bulkLoadOut(TMonster* mob);

// Volume of a wearable in this slot at this race's size tier, or 0 for a slot
// or race with no entry. Bulk loot, Weave and Forge all size their output the
// same way: a per-slot human baseline scaled by the race's size modifier.
[[nodiscard]] int slotVolumeForRace(TemplateSlot slot, race_t race);

// The weight an item of this volume and material comes out at. Weight is
// derived, never set independently -- material density is what makes a silk
// wrap and a steel plate of the same size weigh differently.
[[nodiscard]] float weightForVolume(int volume, int material);

// The inverse: how much room a given weight of a material takes up. Smelt
// needs it because an ingot is created from an item's weight but has to carry
// an honest volume -- a pound of mithril is better than twice the bar that a
// pound of iron is, and Forge sizes gear by volume, not weight.
[[nodiscard]] int volumeForWeight(float weight, int material);

// The word for this race's size tier -- "tiny", "large" and so on -- or
// nullptr for human-sized races, which take no adjective, and for races with
// no size entry at all.
[[nodiscard]] const char* raceSizeName(race_t race);

// Everything that defines one weapon, gathered from the two tables bulk loot
// keeps it in: the physical spec (volume, sharpness, handedness) and the
// damage types it deals. Forge builds from this, so a forged dagger is the
// same object a dropped one is.
struct ForgeWeaponSpec {
    const char* name;
    int volume;
    int maxSharp;
    bool twoHanded;
    int type1;
    int freq1;
    int type2;
    int freq2;
};

// Look a weapon up by the name a player would type. False if there is no such
// weapon.
[[nodiscard]] bool findWeaponSpecByName(const char* name, ForgeWeaponSpec* out);

// Generate one loose bulk loot item -- a random armor slot, or a weapon.
// Unlike bulkLoadOut(), this neither equips the result nor buys its raw
// material from a commodity shop; the caller owns the returned object.
[[nodiscard]] TObj* bulkLoadOutItem(classIndT classInd, int level, race_t race);
