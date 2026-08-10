//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD++ - All rights reserved, SneezyMUD Coding Team
//
//      "bulkLoadOut.h" - Random equipment generation for humanoid mobs
//
//////////////////////////////////////////////////////////////////////////

#pragma once

#include "enum.h"
#include "race.h"

class TMonster;
class TObj;

// Attempt to generate bulk loot equipment on a humanoid mob.
// For each empty equipment slot, rolls a 3% chance to generate
// a random item appropriate for the mob's class, level, and race.
// Should be called after all zone-reset equipment loading is complete.
void bulkLoadOut(TMonster* mob);

// Generate one loose bulk loot item -- a random armor slot, or a weapon.
// Unlike bulkLoadOut(), this neither equips the result nor buys its raw
// material from a commodity shop; the caller owns the returned object.
[[nodiscard]] TObj* bulkLoadOutItem(classIndT classInd, int level, race_t race);
