//////////////////////////////////////////////////////////////////////////
//
// SneezyMUD - All rights reserved, SneezyMUD Coding Team
//
//////////////////////////////////////////////////////////////////////////

#include <stdio.h>

#include "handler.h"
#include "extern.h"
#include "room.h"
#include "being.h"
#include "disease.h"
#include "combat.h"
#include "disc_looting.h"
#include "obj_trap.h"
#include "obj_portal.h"
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
// Make the function return void since you don't actually need a return value from it
void reclaimTrapComps (TBeing* thief, sstring trap_type, TTrap* trap) {
  int item1, item2, item3;

  if (trap_type == "fire") {
    item1 = Obj::ST_FLINT;
    item2 = Obj::ST_SULPHUR;
    item3 = Obj::ST_BAG;
  } else if (trap_type == "explosive") {
    item1 = Obj::ST_FLINT;
    item2 = Obj::ST_SULPHUR;
    item3 = Obj::ST_HYDROGEN;
  } else if (trap_type == "poison") {
    if (trap->isGrenade() || trap->isLandMine()) {
      item1 = Obj::ST_CANISTER;
      item2 = Obj::ST_SPRING;
      item3 = Obj::ST_CON_POISON;
    } else {
      item1 = Obj::ST_NEEDLE;
      item2 = Obj::ST_SPRING;
      item3 = Obj::ST_POISON;
    }
  } else if (trap_type == "sleep") {
    item1 = Obj::ST_NOZZLE;
    item2 = Obj::ST_GAS;
    item3 = Obj::ST_HOSE;
  } else if (trap_type == "acid") {     
    if (trap->isGrenade()|| trap->isLandMine()) {
      item1 = Obj::ST_CANISTER;
      item2 = Obj::ST_SPRING;
      item3 = Obj::ST_ACID_VIAL;
    } else {
      item1 = Obj::ST_NOZZLE;
      item2 = Obj::ST_ACID_VIAL;
      item3 = Obj::ST_BELLOWS;
    }
  } else if (trap_type == "spore") {     
    if (trap->isGrenade()|| trap->isLandMine()) {
      item1 = Obj::ST_CANISTER;
      item2 = Obj::ST_SPRING;
      item3 = Obj::ST_FUNGUS;
    } else {
      item1 = Obj::ST_FUNGUS;
      item2 = Obj::ST_NOZZLE;
      item3 = Obj::ST_BELLOWS;
    }
  } else if (trap_type == "spike") {
    item1 = Obj::ST_SPIKE;
    item2 = Obj::ST_SPRING;
    item3 = Obj::ST_TRIPWIRE;
  } else if (trap_type == "bolt") {
      item1 = Obj::ST_TUBING;
      item2 = Obj::ST_CGAS;
      item3 = Obj::ST_BOLTS;
  } else if (trap_type == "blade") {
    item1 = Obj::ST_RAZOR_BLADE;
    item2 = Obj::ST_SPRING;
    item3 = Obj::ST_TRIPWIRE;
  } else if (trap_type == "disk") {
    item1 = Obj::ST_RAZOR_DISK;
    item2 = Obj::ST_SPRING;
    item3 = Obj::ST_CANISTER;
  } else if (trap_type == "hammer") {
    item1 = Obj::ST_CONCRETE;
    item2 = Obj::ST_WEDGE;
    item3 = Obj::ST_TRIPWIRE;
  } else if (trap_type == "pebble") {
    item1 = Obj::ST_TUBING;
    item2 = Obj::ST_CGAS;
    item3 = Obj::ST_PEBBLES;
  } else if (trap_type == "frost") {
    item1 = Obj::ST_NOZZLE;
    item2 = Obj::ST_HOSE;
    item3 = Obj::ST_FROST;
  } else if (trap_type == "teleport") {     
    if (trap->isGrenade()|| trap->isLandMine()) {
      item1 = Obj::ST_PENTAGRAM;
      item2 = Obj::ST_CRYSTALINE;
      item3 = Obj::ST_BLINK;
    } else {
      item1 = Obj::ST_PENTAGRAM;
      item2 = Obj::ST_TRIPWIRE;
      item3 = Obj::ST_BLINK;
    }
  } else if (trap_type == "power") {     
    if (trap->isGrenade()|| trap->isLandMine()) {
      item1 = Obj::ST_PENTAGRAM;
      item2 = Obj::ST_CRYSTALINE;
      item3 = Obj::ST_ATHANOR;
    } else {
      item1 = Obj::ST_PENTAGRAM;
      item2 = Obj::ST_TRIPWIRE;
      item3 = Obj::ST_ATHANOR;
    }
  } else {
    return;
  } 

  // Can just pass VIRTUAL instead of REAL here to let read_object call 'real_object()' internally
  TObj* comp1 = read_object(item1, VIRTUAL);
  TObj* comp2 = read_object(item2, VIRTUAL);
  TObj* comp3 = read_object(item3, VIRTUAL);
  
  if (!comp1 || !comp2 || !comp3) {
    // This will just cause the function to fail silently if read_object doesn't return a valid object
    return;
  }
  
  *thief += *comp1;
  *thief += *comp2;
  *thief += *comp3;
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

int TObj::disarmMe(TBeing* thief) {
  thief->sendTo("I don't think that's a trap.\n\r");
  return FALSE;
}

int TTrap::disarmMe(TBeing* thief) {  
  if (getTrapCharges() <= 0) {
    thief->sendTo("That trap is already disarmed.\n\r");
    return false;
  }

  sstring trap_type = trap_types[getTrapDamType()];

  if (thief->bSuccess(SKILL_DISARM_TRAP)) {
    thief->sendTo(format("Click.  You disarm the %s trap.\n\r") % trap_type);
    act("$n disarms $p.", false, thief, this, nullptr, TO_ROOM);

    setTrapCharges(0);

    if ((isGrenade() && thief->doesKnowSkill(SKILL_SET_TRAP_GREN)) ||
        (isLandMine() && thief->doesKnowSkill(SKILL_SET_TRAP_MINE)) || 
        (!isGrenade() && !isLandMine() && thief->doesKnowSkill(SKILL_SET_TRAP_CONT)) {
      reclaimTrapComps(thief, trap_type, this);
    }

    return true;
  } 

  thief->sendTo("Click. (whoops)\n\r");
  act("$n tries to disarm $p.", false, thief, this, nullptr, TO_ROOM);

  int rc = thief->triggerTrap(this);
  if (IS_SET_DELETE(rc, DELETE_THIS)) {
    return DELETE_VICT;
  }

  return true; 
}

int disarmTrapObj(TBeing* thief, TObj* trap) {
  int rc;
  rc = trap->disarmMe(thief);
  if (IS_SET_DELETE(rc, DELETE_VICT)) {
    return DELETE_THIS;
  }
  return FALSE;
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
    if (thief->doesKnowSkill(SKILL_SET_TRAP_DOOR)) {
      reclaimTrapComps(thief, trap_type, nullptr);
    }
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
