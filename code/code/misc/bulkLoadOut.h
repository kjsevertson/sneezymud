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

// Generate one loose bulk loot item -- a random armor slot, or a weapon.
// Unlike bulkLoadOut(), this neither equips the result nor buys its raw
// material from a commodity shop; the caller owns the returned object.
[[nodiscard]] TObj* bulkLoadOutItem(classIndT classInd, int level, race_t race);
