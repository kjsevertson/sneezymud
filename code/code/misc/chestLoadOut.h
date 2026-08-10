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
// rather than replace them.
//
// Rather than skip any container holding something -- which would exclude most
// of the key-locked chests the fill tiers are aimed at -- the odds taper as a
// container fills. Each level has a capacity; the fill chance is scaled by the
// fraction of it still free, so a container fills at full rate when empty,
// slows as it approaches the ceiling, and stops there. A fill never carries it
// past that ceiling. Emptying one restores its odds, which keeps a looted chest
// worth revisiting without letting an untouched one grow without bound.
//
// Call after the zone's reset commands have finished, so the zone's average
// mob level is complete.
void chestLoadOut(zoneData& zone);

// Fill any container this mob is carrying or wearing, scaled to the mob's own
// level rather than the zone average.
//
// The taper matters less here: runResetCmdM creates a fresh mob through
// read_mobile each time and returns early when the count is already at max, so
// a surviving mob is never re-used and its bag starts empty every time. Call
// once per mob, after its G/E commands have run.
void mobBagLoadOut(TMonster* mob);
