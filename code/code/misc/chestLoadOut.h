//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD++ - All rights reserved, SneezyMUD Coding Team
//
//      "chestLoadOut.h" - Random loot generation for zone containers
//
//////////////////////////////////////////////////////////////////////////

#pragma once

class zoneData;
class TMonster;

// Fill this zone's containers with random loot. Runs on boot and on timed
// resets alike.
//
// Room containers are not recreated between resets: runResetCmdB re-uses one
// still sitting in the room, and runResetCmdO only runs the loader at boot,
// with later resets just re-locking what is already there. So the same chest
// object survives every reset, and filling it repeatedly would stack payloads
// rather than replace them. tryFillContainer therefore fills only containers
// that are currently empty.
//
// Call after the zone's reset commands have finished, so the zone's average
// mob level is complete.
void chestLoadOut(zoneData& zone);

// Fill any container this mob is carrying or wearing, scaled to the mob's own
// level rather than the zone average.
//
// This needs no empty-container check the way chestLoadOut does: runResetCmdM
// creates a fresh mob through read_mobile each time and returns early when the
// count is already at max, so a surviving mob is never re-used and its bag is
// new every time. Call once per mob, after its G/E commands have run.
void mobBagLoadOut(TMonster* mob);
