#pragma once

#include "discipline.h"
#include "skills.h"

class CDLooting : public CDiscipline {
  public:
    CSkill skCounterSteal;
    CSkill skPlant;
    CSkill skResourcefulness;
    CSkill skScrutiny;
    CSkill skJam;
    CSkill skKeycut;

    virtual CDLooting* cloneMe() { return new CDLooting(*this); }

  private:
};

int detectSecret(TBeing*);

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

// Jam: wedge a small piercing weapon into a closed door to bind it shut.
// Base difficulty of the resulting lock, before the thief's level and
// learnedness are folded in.
inline constexpr int kJamBaseDifficulty = 25;
// task_picklock treats lock_difficulty >= 100 as unpickable; stay under it.
inline constexpr int kJamMaxDifficulty = 99;
// Pulses of work before the single success roll is made.
inline constexpr int kJamPulses = 3;

short jamLockDifficulty(const TBeing*);
void applyJam(TBeing*, dirTypeT, short);

// Keycut: cast a short-lived wax duplicate of a door's key.
// decay_time ticks once per Pulse::MUDHOUR, which is 2.4 real minutes, so
// this is the coarsest unit available -- roughly five real minutes.
inline constexpr int kWaxKeyDecay = 2;
// Pulses of work before the single success roll is made.
inline constexpr int kKeycutPulses = 4;
// A perfectly skilled thief wastes nothing; an unskilled one needs twice the
// wax. Expressed as a percentage multiplier against learnedness.
inline constexpr int kKeycutWasteFactor = 200;
// One full mud day: Pulse::SECS_PER_MUD_DAY is 24 mud hours, and a mud hour
// is 2.4 real minutes, so this works out to roughly one real hour. The
// cooldown starts on any completed attempt, success or failure -- the pairing
// is what gates the skill, since neither alone is much of a brake.
inline constexpr int kKeycutCooldownHours = 24;

class TCommodity;

int keycutWaxUnits(const TBeing*, float keyWeight);
TCommodity* findWax(TBeing*);
bool consumeWax(TBeing*, int units);
void installKeycutCooldown(TBeing*);
