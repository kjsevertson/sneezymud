#pragma once

#include "discipline.h"
#include "skills.h"

class CDLooting : public CDiscipline {
  public:
    CSkill skCounterSteal;
    CSkill skPlant;
    CSkill skResourcefulness;
    CSkill skScrutiny;

    virtual CDLooting* cloneMe() { return new CDLooting(*this); }

  private:
};

int detectSecret(TBeing*);

// Random potion/component/symbol/tool appropriate to the given class bits,
// costing no more than moneyCeiling. Returns nullptr if nothing affordable
// turned up. Caller owns the result.
TObj* generateStealLoot(unsigned short classBits, double moneyCeiling);

int disarmTrapObj(TBeing*, TObj*);
int disarmTrapDoor(TBeing*, dirTypeT);

class TTrap;
// Salvage trap components on a successful disarm. `targ` selects the recipe for
// the target the trap was set against (door/cont/mine/grenade). Pass the TTrap
// for mines/grenades (yields the casing); pass nullptr for flag-based traps
// (doors, containers, portals).
bool reclaimTrapComps(TBeing*, sstring, trap_targ_t, TTrap*);

int detectTrapObj(TBeing*, const TThing*);
int detectTrapDoor(TBeing*, int);
