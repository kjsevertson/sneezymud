//////////////////////////////////////////////////////////////////////////
//
// SneezyMUD - All rights reserved, SneezyMUD Coding Team
// mining.h - working ore out of rock, by pick or by blast
//
//////////////////////////////////////////////////////////////////////////

#pragma once

class TBeing;
class TRoom;
class TObj;
class TBaseWeapon;

// A pick held in an equipment slot, or nullptr. There is no tool type for one,
// so the test is a weapon answering to "pick".
[[nodiscard]] TBaseWeapon* getHeldPick(TBeing* ch);

// Hills, mountains and caves. Sewers run deeper than any of them and are
// deliberately excluded: depth alone should not make a drain worth quarrying.
[[nodiscard]] bool isMineableSector(const TRoom* rp);

// Levels below the surface, never negative.
[[nodiscard]] int getMineDepth(const TRoom* rp);

// The chance an attempt leaves the room barren: three fifths at the surface,
// ten points less every three levels down, never quite nothing.
[[nodiscard]] int getMinedOutChance(const TRoom* rp);

// What this rock holds, weighted by sector and gated by depth.
[[nodiscard]] int getOreMaterial(const TRoom* rp);

// Name a freshly cut lump for what it is: ore, rough stone, or an uncut gem.
void nameMinedThing(TObj* ore, int material);

// Build one chunk of what this room holds and leave it on the floor. Returns
// the chunk, or nullptr if the prototype could not be loaded.
TObj* makeOreChunk(TRoom* rp);

// A charge going off in rock shakes ore loose. Blast size is rolled down in
// steps of eight, each step its own chance at another chunk, so a big charge
// opens a seam and a small one scatters a little gravel.
void revealOreFromBlast(TBeing* ch, int trapLevel);
