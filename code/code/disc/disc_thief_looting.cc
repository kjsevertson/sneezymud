#include <stdio.h>

#include <algorithm>

#include "handler.h"
#include "extern.h"
#include "room.h"
#include "being.h"
#include "obj_general_weapon.h"
#include "obj_commodity.h"
#include "materials.h"
#include "disease.h"
#include "combat.h"
#include "disc_thief_looting.h"
#include "obj_trap.h"
#include "obj_portal.h"
#include "obj_trap_component.h"
#include "trap.h"
#include "low.h"

int TBeing::doSearch(const char* argument) {
  int rc;

  if (!doesKnowSkill(SKILL_SEARCH)) {
    sendTo("You are not trained in how to recognize secret passages!\n\r");
    return FALSE;
  }

  if (riding) {
    sendTo("You cannot search while riding.\n\r");
    return FALSE;
  }
  for (; isspace(*argument); argument++)
    ;

  if (!*argument) {
    sendTo("You begin searching for secret exits.\n\r");
    act("$n begins searching the walls for something.", FALSE, this, 0, 0,
      TO_ROOM);
    start_task(this, NULL, NULL, TASK_SEARCH, "", 0, in_room, 1, 0, 4);
  } else {
    for (rc = 0; rc < MAX_DIR; rc++) {
      if (is_abbrev(argument, dirs[rc])) {
        start_task(this, NULL, NULL, TASK_SEARCH, "", 0, in_room, 1, rc + 100,
          4);
        return TRUE;
      }
    }
    // there's probably a better way to do this
    if (!strcmp(argument, "ne")) {
      start_task(this, NULL, NULL, TASK_SEARCH, "", 0, in_room, 1,
        DIR_NORTHEAST + 100, 4);
      return TRUE;
    } else if (!strcmp(argument, "nw")) {
      start_task(this, NULL, NULL, TASK_SEARCH, "", 0, in_room, 1,
        DIR_NORTHWEST + 100, 4);
      return TRUE;
    } else if (!strcmp(argument, "se")) {
      start_task(this, NULL, NULL, TASK_SEARCH, "", 0, in_room, 1,
        DIR_SOUTHEAST + 100, 4);
      return TRUE;
    } else if (!strcmp(argument, "sw")) {
      start_task(this, NULL, NULL, TASK_SEARCH, "", 0, in_room, 1,
        DIR_SOUTHWEST + 100, 4);
      return TRUE;
    }

    sendTo("You look and look, but cannot seem to find that direction.\n\r");
  }
  return TRUE;
}

int detectSecret(TBeing* thief) {
  int j;
  roomDirData* fdd;
  char buf[128];
  int move_cost;

  move_cost = 30;

  *buf = '\0';

  if (thief->getMove() < move_cost) {
    thief->sendTo("You are too tired to search.  Maybe later...\n\r");
    return FALSE;
  }
  if (thief->riding) {
    thief->sendTo("You can't search while mounted.\n\r");
    return FALSE;
  }
  int bKnown = thief->getSkillValue(SKILL_SEARCH);

  if (thief->doesKnowSkill(SKILL_SEARCH))
    thief->learnFromDoingUnusual(LEARN_UNUSUAL_NORM_LEARN, SKILL_SEARCH, 5);

  for (j = 0; j < 10; j++) {
    if ((fdd = thief->roomp->dir_option[j])) {
      if (((j < 4) || (j > 5))) {
        sprintf(buf, "$n searches the %s wall for secret doors.", dirs[j]);
        act(buf, FALSE, thief, 0, 0, TO_ROOM);
      } else if (j == 4)
        act("$n searches the ceiling for secret doors.", FALSE, thief, 0, 0,
          TO_ROOM);
      else
        act("$n searches the $g for secret doors.", FALSE, thief, 0, 0,
          TO_ROOM);

      if (!IS_SET(fdd->condition, EXIT_SECRET) ||
          !IS_SET(fdd->condition, EXIT_CLOSED) ||
          fdd->keyword == "_unique_door_")
        continue;

      if (thief->bSuccess(bKnown, SKILL_SEARCH)) {
        thief->sendTo(format("Secret door found %s! Door is named %s.\n\r") %
                      dirs[j] %
                      (!fdd->keyword.empty() ? fname(fdd->keyword)
                                             : "NO NAME. TELL A GOD"));
        sprintf(buf, "$n exclaims, \"Look %s! A SECRET door named %s!\"\n\r",
          dirs[j],
          (!fdd->keyword.empty() ? fname(fdd->keyword).c_str()
                                 : "NO NAME. TELL A GOD"));
        act(buf, FALSE, thief, 0, 0, TO_ROOM);
        thief->setMove(max(0, (thief->getMove() - 30)));
        thief->gainTaskExp(SKILL_SEARCH, bKnown, 1.0, true);

        return TRUE;
      }
    }
  }
  thief->sendTo("No secret doors found in this area.\n\r");
  act("$n searches and searches, but comes up empty.", FALSE, thief, 0, 0,
    TO_ROOM);
  thief->setMove(max(0, (thief->getMove() - 30)));
  return TRUE;
}

int TBeing::disarmTrap(const char* arg, TObj* tp) {
  int rc;
  TObj* trap;
  char type[256], dir[256];
  dirTypeT door;

  if (!doesKnowSkill(SKILL_DISARM_TRAP)) {
    sendTo("You know nothing about removing traps.\n\r");
    return FALSE;
  }

  argument_interpreter(arg, type, cElements(type), dir, cElements(dir));

  if ((trap = tp) || (trap = get_obj_vis_accessible(this, type))) {
    rc = disarmTrapObj(this, trap);
    if (IS_SET_DELETE(rc, DELETE_ITEM)) {
      delete trap;
      trap = NULL;
    }
    if (rc)
      addSkillLag(SKILL_DISARM_TRAP, rc);

    if (IS_SET_DELETE(rc, DELETE_THIS))
      return DELETE_THIS;

    return FALSE;
  } else if ((door = findDoor(type, dir, DOOR_INTENT_OPEN, SILENT_YES)) >= 0) {
    rc = disarmTrapDoor(this, door);
    if (rc)
      addSkillLag(SKILL_DISARM_TRAP, rc);

    if (IS_SET_DELETE(rc, DELETE_THIS))
      return DELETE_THIS;

    return FALSE;
  } else {
    // needed for "disarm elite weapon"
    sendTo(format("You can't find \"%s\" here.\n\r") % arg);
    return FALSE;
  }

  return FALSE;
}

// Salvage a disarmed trap's crafting components into the thief's inventory.
// Sources the reagents from the shared trapComponents() table (same one the
// trap gating and flavor messages use), rolls recovery per component, and
// reads each recovered object into the thief. `targ` picks the recipe for the
// target the trap was set against. Pass the TTrap for mines and grenades (to
// also yield the casing); pass nullptr for flag-based traps (doors, containers,
// portals).
bool reclaimTrapComps(TBeing* thief, sstring trap_type, trap_targ_t targ,
  TTrap* trap) {
  // Source the reagents for the same target the trap was set against, so
  // reclaim returns exactly what the set path consumed.
  int i1 = 0, i2 = 0, i3 = 0;
  if (!trapComponents(trap_type.c_str(), targ, i1, i2, i3))
    return false;

  std::vector<int> components = {i1, i2, i3};
  if (trap) {
    if (trap->isTrapEffectType(TRAP_EFF_THROW))
      components.push_back(Obj::ST_CASE_GRENADE);
    else if (trap->isTrapEffectType(TRAP_EFF_MOVE))
      components.push_back(Obj::ST_CASE_MINE);
  }

  int recovery_chance = 50 + (thief->getSkillValue(SKILL_DISARM_TRAP) / 2);
  size_t recovered = 0;
  for (int vnum : components) {
    if (::number(1, 100) > recovery_chance)
      continue;
    if (TObj* comp = read_object(vnum, VIRTUAL)) {
      // Each salvaged reagent comes back as a single charge (one trap's
      // worth); auto-merge folds it into any matching stack the thief carries.
      if (auto* tc = dynamic_cast<TTrapComponent*>(comp))
        tc->setTrapComponentCharges(1);
      act("You carefully recover $p from the trap.", false, thief, comp,
        nullptr, TO_CHAR);
      *thief += *comp;
      recovered++;
    }
  }

  if (recovered == 0) {
    act("You were unable to salvage any components from the trap.", false,
      thief, nullptr, nullptr, TO_CHAR);
    return false;
  }
  if (recovered < components.size())
    act("You managed to salvage some components from the trap.", false, thief,
      nullptr, nullptr, TO_CHAR);
  else
    act("You successfully recovered all components from the trap!", false,
      thief, nullptr, nullptr, TO_CHAR);
  return true;
}

int TObj::disarmMe(TBeing* thief) {
  thief->sendTo("I don't think that's a trap.\n\r");
  return FALSE;
}

int TTrap::disarmMe(TBeing* thief) {
  int rc;
  char trap_type[80];
  int bKnown = thief->getSkillValue(SKILL_DISARM_TRAP);

  if (getTrapCharges() <= 0) {
    thief->sendTo("That trap is already disarmed.\n\r");
    return FALSE;
  }

  strcpy(trap_type, trap_types[getTrapDamType()].c_str());

  if (thief->bSuccess(bKnown, SKILL_DISARM_TRAP)) {
    thief->sendTo(format("Click.  You disarm the %s trap.\n\r") % trap_type);
    act("$n disarms $p.", FALSE, thief, this, 0, TO_ROOM);
    setTrapCharges(0);
    // Salvage components if the thief knows how to build this trap type
    bool canSalvage = (isTrapEffectType(TRAP_EFF_THROW) &&
                        thief->doesKnowSkill(SKILL_SET_TRAP_GREN)) ||
                      (isTrapEffectType(TRAP_EFF_MOVE) &&
                        thief->doesKnowSkill(SKILL_SET_TRAP_MINE));
    if (canSalvage)
      reclaimTrapComps(thief, trap_type,
        isTrapEffectType(TRAP_EFF_THROW) ? TRAP_TARG_GRENADE : TRAP_TARG_MINE,
        this);
    else
      act(
        "You lack the knowledge to salvage components from this type of "
        "trap.",
        false, thief, nullptr, nullptr, TO_CHAR);
    // Destroy the spent standalone trap object (mine/grenade).
    return DELETE_ITEM;
  } else {
    thief->sendTo("Click. (whoops)\n\r");
    act("$n tries to disarm $p.", FALSE, thief, this, 0, TO_ROOM);
    rc = thief->triggerTrap(this);
    if (IS_SET_DELETE(rc, DELETE_THIS)) {
      return DELETE_VICT;
    }
    return TRUE;
  }
}

int disarmTrapObj(TBeing* thief, TObj* trap) {
  int rc = trap->disarmMe(thief);
  int ret = 0;
  // The thief (passed as the victim) may die, and/or the trap object may need
  // deleting (a spent mine/grenade, or a portal that detonated on a botch).
  if (IS_SET_DELETE(rc, DELETE_VICT))
    ret |= DELETE_THIS;
  if (IS_SET_DELETE(rc, DELETE_ITEM))
    ret |= DELETE_ITEM;
  return ret;
}

int disarmTrapDoor(TBeing* thief, dirTypeT door) {
  int learnedness;
  int rc;
  roomDirData *exitp, *back = NULL;
  TRoom* rp;
  char buf[256], doorbuf[80], trap_type[80];

  exitp = thief->exitDir(door);
  strcpy(doorbuf, fname(exitp->keyword).c_str());

  if (!IS_SET(exitp->condition, EXIT_TRAPPED)) {
    thief->sendTo(format("I don't think the %s is trapped.\n\r") % doorbuf);
    return FALSE;
  }

  int bKnown = thief->getSkillValue(SKILL_DISARM_TRAP);

  strcpy(trap_type, trap_types[exitp->trap_info].c_str());
  learnedness = min((int)MAX_SKILL_LEARNEDNESS, 2 * bKnown);

  if (thief->bSuccess(learnedness, SKILL_DISARM_TRAP)) {
    thief->sendTo(format("Click.  You disarm the %s trap in the %s.\n\r") %
                  trap_type % doorbuf);
    sprintf(buf, "$n disarms the %s trap in the %s.", trap_type, doorbuf);
    act(buf, FALSE, thief, 0, 0, TO_ROOM);
    REMOVE_BIT(exitp->condition, EXIT_TRAPPED);
    if ((rp = real_roomp(exitp->to_room)) &&
        (back = rp->dir_option[rev_dir(door)])) {
      REMOVE_BIT(back->condition, EXIT_TRAPPED);
    }
    // Salvage components if the thief knows how to set door traps
    if (thief->doesKnowSkill(SKILL_SET_TRAP_DOOR))
      reclaimTrapComps(thief, trap_type, TRAP_TARG_DOOR, nullptr);
    else
      act(
        "You lack the knowledge to salvage components from this type of "
        "trap.",
        false, thief, nullptr, nullptr, TO_CHAR);
    return TRUE;
  } else {
    thief->sendTo("Click. (whoops)\n\r");
    sprintf(buf, "$n tries to disarm the trap in the %s.", doorbuf);
    act(buf, FALSE, thief, 0, 0, TO_ROOM);
    rc = thief->triggerDoorTrap(door);
    if (IS_SET_ONLY(rc, DELETE_THIS)) {
      return DELETE_THIS;
    }
    return TRUE;
  }
}

int TThing::detectMe(TBeing* thief) const { return FALSE; }

int TPortal::detectMe(TBeing* thief) const {
  int bKnown = thief->getSkillValue(SKILL_DETECT_TRAP);

  if (!isPortalFlag(EXIT_TRAPPED))
    return FALSE;

  // opening a trapped portal
  if (thief->bSuccess(bKnown, SKILL_DETECT_TRAP)) {
    CS(SKILL_DETECT_TRAP);
    return TRUE;
  } else {
    CF(SKILL_DETECT_TRAP);
    return FALSE;
  }
}

int TTrap::detectMe(TBeing* thief) const {
  int bKnown = thief->getSkillValue(SKILL_DETECT_TRAP);

  // randomly seen when in room
  // reduced detection rate
  if (thief->bSuccess(bKnown / 10 + 1, SKILL_DETECT_TRAP))
    return TRUE;
  else
    return FALSE;
}

// returns TRUE if trap detected
int detectTrapObj(TBeing* thief, const TThing* trap) {
  return trap->detectMe(thief);
}

int detectTrapDoor(TBeing* thief, int) {
  int bKnown = thief->getSkillValue(SKILL_DETECT_TRAP);

  if (thief->bSuccess(bKnown / 3 + 1, SKILL_DETECT_TRAP))
    return TRUE;
  else
    return FALSE;
}

// The lock a successful jam leaves behind. Held below 100 because at 100 or
// above task_picklock stops rolling and declares the lock "totally impossible
// to pick" -- a far stronger effect than this skill is meant to have.
short jamLockDifficulty(const TBeing* thief) {
  int diff = (kJamBaseDifficulty + thief->getSkillLevel(SKILL_JAM)) *
             thief->getSkillValue(SKILL_JAM) / 100;

  return static_cast<short>(std::clamp(diff, 0, kJamMaxDifficulty));
}

// A blade wedged into a keyhole binds it from both sides, so mirror the
// state onto the reverse exit the way doLock() does.
void applyJam(TBeing* thief, dirTypeT door, short diff) {
  roomDirData* exitp = thief->exitDir(door);
  if (!exitp)
    return;

  SET_BIT(exitp->condition, EXIT_LOCKED);
  exitp->lock_difficulty = diff;

  TRoom* rp = real_roomp(exitp->to_room);
  roomDirData* back = nullptr;
  if (rp && (back = rp->dir_option[rev_dir(door)]) &&
      back->to_room == thief->in_room) {
    SET_BIT(back->condition, EXIT_LOCKED);
    back->lock_difficulty = diff;
  }
}

int TBeing::doJam(const char* argument) {
  if (!doesKnowSkill(SKILL_JAM)) {
    sendTo("You wouldn't know how to wedge a blade into anything.\n\r");
    return false;
  }

  char objName[MAX_INPUT_LENGTH], dirName[MAX_INPUT_LENGTH];
  argument_interpreter(argument, objName, cElements(objName), dirName,
    cElements(dirName));

  if (!*objName || !*dirName) {
    sendTo("Syntax: jam <weapon> <direction>\n\r");
    return false;
  }

  dirTypeT door = getDirFromChar(dirName);
  if (door == DIR_NONE) {
    sendTo("That's not a direction.\n\r");
    return false;
  }

  roomDirData* exitp = exitDir(door);
  if (!exitp || exitp->door_type == DOOR_NONE) {
    sendTo("There's no door that way to jam.\n\r");
    return false;
  }
  if (IS_SET(exitp->condition, EXIT_DESTROYED)) {
    sendTo(
      format("The %s has been destroyed; there's nothing left to jam.\n\r") %
      exitp->getName());
    return false;
  }
  if (IS_SET(exitp->condition, EXIT_CAVED_IN)) {
    sendTo("It's caved in.  A blade isn't going to add much.\n\r");
    return false;
  }
  if (!IS_SET(exitp->condition, EXIT_CLOSED)) {
    sendTo(format("You have to close the %s first.\n\r") % exitp->getName());
    return false;
  }
  // Deliberate: a jam only ever creates a lock, it never reinforces one. This
  // also keeps the skill from overwriting a builder's lock_difficulty on a
  // door that is legitimately locked.
  if (IS_SET(exitp->condition, EXIT_LOCKED)) {
    sendTo(format("The %s is already locked.\n\r") % exitp->getName());
    return false;
  }

  TBeing* dummy = nullptr;
  TObj* obj = nullptr;
  if (!generic_find(objName, FIND_OBJ_INV, this, &dummy, &obj) || !obj) {
    sendTo(format("You aren't carrying anything called '%s'.\n\r") % objName);
    return false;
  }

  TGenWeapon* weapon = dynamic_cast<TGenWeapon*>(obj);
  if (!weapon || !weapon->canBackstab()) {
    act("$p is too clumsy a thing to wedge into a keyhole.", false, this, obj,
      nullptr, TO_CHAR);
    return false;
  }
  if (obj->isMonogrammed()) {
    act("Leaving $p behind with your name on it rather defeats the point.",
      false, this, obj, nullptr, TO_CHAR);
    return false;
  }

  if (task)
    stopTask();

  act("You begin working $p into the keyhole of the $T.", false, this, obj,
    reinterpret_cast<const TThing*>(exitp->getName().c_str()), TO_CHAR);
  act("$n begins working $p into the keyhole of the $T.", true, this, obj,
    reinterpret_cast<const TThing*>(exitp->getName().c_str()), TO_ROOM);

  learnFromDoingUnusual(LEARN_UNUSUAL_NORM_LEARN, SKILL_JAM, 8);

  // start_task's last argument is an absolute pulse, not an interval -- the
  // task sweep fires when pulse >= nextUpdate. Zero means the first pulse of
  // work lands on the next sweep; jam_pulse spaces the rest by MOBACT.
  start_task(this, weapon, nullptr, TASK_JAM, "", kJamPulses, in_room, door, 0,
    0);

  return true;
}

// Commodities carry ten units per point of weight, so a key's prototype
// weight converts directly. A clumsy caster spoils more of the pour.
int keycutWaxUnits(const TBeing* thief, float keyWeight) {
  int base = std::max(1, static_cast<int>(keyWeight * 10.0f));
  int waste = kKeycutWasteFactor - thief->getSkillValue(SKILL_KEYCUT);

  return std::max(1, base * waste / 100);
}

TCommodity* findWax(TBeing* thief) {
  for (StuffIter it = thief->stuff.begin(); it != thief->stuff.end(); ++it) {
    TCommodity* tc = dynamic_cast<TCommodity*>(*it);
    if (tc && tc->getMaterial() == MAT_WAX)
      return tc;
  }

  return nullptr;
}

// Returns false if the thief no longer has enough wax -- it can be dropped or
// sold while the task is still running.
bool consumeWax(TBeing* thief, int units) {
  TCommodity* wax = findWax(thief);
  if (!wax || wax->numUnits() < units)
    return false;

  wax->setWeight(wax->getWeight() - (units / 10.0));
  if (wax->numUnits() <= 0)
    delete wax;

  return true;
}

// AFFECT_SKILL_ATTEMPT is the codebase's cooldown carrier: the skill it gates
// rides in the modifier, and checkForSkillAttempt() matches on it.  Same shape
// as smite and forage.
void installKeycutCooldown(TBeing* thief) {
  affectedData cd;
  cd.type = AFFECT_SKILL_ATTEMPT;
  cd.duration = kKeycutCooldownHours * Pulse::UPDATES_PER_MUDHOUR;
  cd.location = APPLY_NONE;
  cd.modifier = SKILL_KEYCUT;
  cd.bitvector = 0;
  thief->affectTo(&cd, -1);
}

int TBeing::doKeycut(const char* argument) {
  if (!doesKnowSkill(SKILL_KEYCUT)) {
    sendTo("You wouldn't know how to take an impression of a lock.\n\r");
    return false;
  }

  if (checkForSkillAttempt(SKILL_KEYCUT)) {
    sendTo("Your hands are still too unsteady to work a lock that finely.\n\r");
    return false;
  }

  sstring dirName, ignored;
  argument_interpreter(sstring(argument), dirName, ignored);
  if (dirName.empty()) {
    sendTo("Syntax: keycut <direction>\n\r");
    return false;
  }

  dirTypeT door = getDirFromChar(dirName.c_str());
  if (door == DIR_NONE) {
    sendTo("That's not a direction.\n\r");
    return false;
  }

  roomDirData* exitp = exitDir(door);
  if (!exitp || exitp->door_type == DOOR_NONE) {
    sendTo("There's no door that way to take an impression of.\n\r");
    return false;
  }
  if (IS_SET(exitp->condition, EXIT_DESTROYED) ||
      IS_SET(exitp->condition, EXIT_CAVED_IN)) {
    sendTo("There's no lock left there worth copying.\n\r");
    return false;
  }
  // < 0 is no keyhole at all; 0 is a lock builders left keyless, to be picked
  // rather than opened. Neither has a key worth casting.
  if (exitp->key <= 0) {
    sendTo(format("There's no key fitting the %s for you to copy.\n\r") %
           exitp->getName());
    return false;
  }

  // A key vnum that resolves to nothing is builder data we can't work with.
  int rnum = real_object(exitp->key);
  if (rnum < 0) {
    sendTo("You can't make any sense of that lock.\n\r");
    vlogf(LOG_LOW, format("keycut: exit key vnum %d in room %d has no object") %
                     exitp->key % in_room);
    return false;
  }

  int units = keycutWaxUnits(this, obj_index[rnum].weight);
  TCommodity* wax = findWax(this);
  if (!wax || wax->numUnits() < units) {
    sendTo(format("You need %d units of wax to cast that key, and you have "
                  "%d.\n\r") %
           units % (wax ? wax->numUnits() : 0));
    return false;
  }

  if (task)
    stopTask();

  act("You begin working wax into the keyhole of the $T.", false, this, 0,
    reinterpret_cast<const TThing*>(exitp->getName().c_str()), TO_CHAR);
  act("$n crouches by the $T and begins working at the lock.", true, this, 0,
    reinterpret_cast<const TThing*>(exitp->getName().c_str()), TO_ROOM);

  learnFromDoingUnusual(LEARN_UNUSUAL_NORM_LEARN, SKILL_KEYCUT, 8);

  // flags carries the wax cost so the pulse doesn't have to recompute it
  // against a learnedness that may have risen mid-task.
  start_task(this, nullptr, nullptr, TASK_KEYCUT, "", kKeycutPulses, in_room,
    door, units, 0);

  return true;
}
