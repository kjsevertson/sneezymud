//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD++ - All rights reserved, SneezyMUD Coding Team
//
//      "bulkLoadOut.h" - Random equipment generation for humanoid mobs
//
//////////////////////////////////////////////////////////////////////////

#pragma once

class TMonster;

// Attempt to generate bulk loot equipment on a humanoid mob.
// For each empty equipment slot, rolls a 3% chance to generate
// a random item appropriate for the mob's class, level, and race.
// Should be called after all zone-reset equipment loading is complete.
void bulkLoadOut(TMonster* mob);
