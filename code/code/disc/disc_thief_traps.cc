#include "handler.h"
#include "extern.h"
#include "obj_bag.h"
#include "room.h"
#include "games.h"
#include "being.h"
#include "disc_thief_traps.h"
#include "obj_tool.h"
#include "obj_commodity.h"
#include "low.h"
#include "spec_objs.h"
#include "materials.h"    // for material_nums
#include "obj_spellbag.h"


void TObj::pickMe(TBeing* thief) {
  act("$p: That's not a container.", false, thief, this, 0, TO_CHAR);
}

int TBeing::doPick(const char* argument) {
  char type[80], dir[80];
  int rc;

  // Gin Game Command, not Thief skill
  if (gGin.check(this)) {
    gGin.draw(this, argument);
    return TRUE;
  }
  if (!doesKnowSkill(SKILL_PICK_LOCK)) {
    sendTo("You know nothing about picking locks.\n\r");
    return FALSE;
  }
  // Thief skill
  argument_interpreter(argument, type, cElements(type), dir, cElements(dir));

  if (!*type) {
    sendTo("Pick what?\n\r");
    return FALSE;
  }
  rc = pickLocks(this, argument, type, dir);
  return rc;
}

int TThing::pickWithMe(TBeing* thief, const char*, const char*, const char*) {
  thief->sendTo(
    "You need to hold a lock pick in your primary hand in order to pick "
    "locks.\n\r");
  return FALSE;
}

int TTool::pickWithMe(TBeing* thief, const char* argument, const char* type,
  const char* dir) {
  dirTypeT door;
  roomDirData* exitp = NULL;
  TObj* obj;
  TBeing* victim;

  if ((getToolType() != TOOL_LOCKPICK) || (getToolUses() <= 0)) {
    thief->sendTo(
      "You need to hold a lock pick in your primary hand in order to pick "
      "locks.\n\r");
    return FALSE;
  }
  int bKnown = thief->getSkillValue(SKILL_PICK_LOCK);

  // moved door check before obj check as "pick gate s" seemed to
  // pick up objs with "s" in the name, not sure why gate was ignored though
  if ((door = thief->findDoor(type, dir, DOOR_INTENT_UNLOCK, SILENT_YES)) >=
      MIN_DIR) {
    exitp = thief->exitDir(door);
    if (exitp->door_type == DOOR_NONE)
      thief->sendTo("That's absurd.\n\r");

    if (!IS_SET(exitp->condition, EXIT_CLOSED))
      thief->sendTo("You realize that the door is already open.\n\r");
    else if (exitp->key < 0)
      thief->sendTo("You can't seem to spot any lock to pick.\n\r");
    else if (!IS_SET(exitp->condition, EXIT_LOCKED))
      thief->sendTo("Oh.. it wasn't locked at all.\n\r");
    else {
      act("$n begins fiddling with a lock.", FALSE, thief, 0, 0, TO_ROOM);
      act("You begin fiddling with a lock.", FALSE, thief, 0, 0, TO_CHAR);
      thief->learnFromDoingUnusual(LEARN_UNUSUAL_NORM_LEARN, SKILL_PICK_LOCK,
        8);

      // silly, but what if they sit down and pick the lock...
      if (thief->task)
        thief->stopTask();

      start_task(thief, NULL, NULL, TASK_PICKLOCKS, "", 0, thief->in_room, door,
        0, 120 - bKnown);
    }
  } else if (generic_find(argument, FIND_OBJ_INV | FIND_OBJ_ROOM, thief,
               &victim, &obj)) {
    obj->pickMe(thief);
  } else
    thief->sendTo("You don't see that here.\n\r");

  return TRUE;
}

int pickLocks(TBeing* thief, const char* argument, const char* type,
  const char* dir) {
  TThing* pick;

  if (!thief->doesKnowSkill(SKILL_PICK_LOCK)) {
    thief->sendTo("You don't know to pick locks!\n\r");
    return FALSE;
  }
  if (!(pick = thief->heldInPrimHand())) {
    thief->sendTo(
      "You need to hold a lock pick in your primary hand in order to pick "
      "locks.\n\r");
    return FALSE;
  }
  pick->pickWithMe(thief, argument, type, dir);
  return TRUE;
}

int TBeing::doTinker(const char* argument) {
  char type[80], obj_name[80], mod_type[80];
  TObj* obj;
  
  if (!doesKnowSkill(SKILL_TINKER)) {
    sendTo("You know nothing about tinkering with objects.\n\r");
    return FALSE;
  }

  sstring remaining = argument;
  remaining = one_argument(remaining.c_str(), type, cElements(type));
  remaining = one_argument(remaining.c_str(), obj_name, cElements(obj_name));
  remaining = one_argument(remaining.c_str(), mod_type, cElements(mod_type));

  if (!*type || !*obj_name || !*mod_type) {
    sendTo("Syntax: tinker <bag/bottle> <object> <modification>\n\r");
    return FALSE;
  }

  TThing* t_obj = searchLinkedListVis(this, obj_name, stuff);
  obj = dynamic_cast<TObj *>(t_obj);
  if (!obj) {
    sendTo("You don't have that object.\n\r");
    return FALSE;
  }

  if (is_abbrev(type, "bag")) {
    return tinkerBag(this, obj, mod_type);
  } else if (is_abbrev(type, "junk")) {
    return tinkerJunk(this, obj, mod_type);
  }

  sendTo("You can only tinker with bags or junk currently.\n\r");
  return FALSE;
}

int TBeing::tinkerBag(TBeing* ch, TObj* obj, const char* mod_type) {
  // First, check if it's a basic container
  TBaseContainer* baseContainer = dynamic_cast<TBaseContainer*>(obj);
  if (!baseContainer) {
    ch->sendTo("That's not actually a bag.\n\r");
    return FALSE;
  }

  // Then check if it's the specific type we need
  TOpenContainer* container = dynamic_cast<TOpenContainer*>(obj);
  if (!container) {
    ch->sendTo("That's not the right kind of bag.\n\r");
    return FALSE;
  }

  if (obj->itemType() != ITEM_BAG) {
    ch->sendTo("You can only tinker with bags.\n\r");
    return FALSE;
  }

  // Check if they have learned this modification
  if (!ch->doesKnowSkill(SKILL_TINKER)) {
    ch->sendTo("You haven't learned the tinkering skill.\n\r");
    return FALSE;
  }

  int tinkerLearnedness = ch->getSkillValue(SKILL_TINKER);

  // Check if the bag is made of an appropriate material
  int mat = container->getMaterial();
  switch(mat) {
    // Basic materials - no special requirements
    case MAT_CLOTH:
    case MAT_SILK:
    case MAT_TOUGH_CLOTH:
    case MAT_STRING:
    case MAT_HEMP:
    case MAT_WOOL:
    case MAT_FUR:
    case MAT_FUR_CAT:
    case MAT_FUR_DOG:
    case MAT_FUR_RABBIT:
    case MAT_FEATHERED:
    case MAT_FISHSCALE:
    case MAT_PAPER:
    case MAT_CARDBOARD:
    case MAT_WOOD:
      break;

    // Advanced materials - require 50 learnedness
    case MAT_LEATHER:
    case MAT_TOUGH_LEATHER:
    case MAT_SOFT_LEATHER:
    case MAT_DWARF_LEATHER:
    case MAT_OGRE_HIDE:
      if (tinkerLearnedness < 50) {
        ch->sendTo("You need at least 50 learnedness in Tinker to work with leather and hide materials.\n\r");
        return FALSE;
      }
      break;

    // Expert materials - require 90 learnedness
    case MAT_DRAGON_SCALE:
      if (tinkerLearnedness < 90) {
        ch->sendTo("You need at least 90 learnedness in Tinker to work with dragon scale materials.\n\r");
        return FALSE;
      }
      break;

    default:
      ch->sendTo("This bag's material cannot be modified with tinkering.\n\r");
      return FALSE;
  }

  // Route to specific modification functions
  if (is_abbrev(mod_type, "fuse")) {
    return tinkerBagFuse(ch, container);
  } else if (is_abbrev(mod_type, "stretch")) {
    return tinkerBagStretch(ch, container);
  } else if (is_abbrev(mod_type, "straps")) {
    return tinkerBagStraps(ch, container);
  } else if (is_abbrev(mod_type, "belt")) {
    return tinkerBagBelt(ch, container);
  } else if (is_abbrev(mod_type, "reinforce")) {
    return tinkerBagReinforce(ch, container);
  } else if (is_abbrev(mod_type, "patch")) {
    return tinkerBagPatch(ch, container);
  }

  ch->sendTo("Valid bag modifications: fuse, stretch, straps, belt, reinforce, patch\n\r");
  return FALSE;
}

int TBeing::tinkerBagFuse(TBeing* ch, TOpenContainer* container) {
  if (!container->isContainerFlag(CONT_TRAPPED)) {
    ch->sendTo("This container needs to be trapped before you can add a fuse.\n\r");
    return FALSE;
  }

  if (container->isContainerFlag(CONT_TRAPPED)) {
    ch->sendTo("This container already has a fuse mechanism.\n\r");
    return FALSE;
  }

  int skill = ch->getSkillValue(SKILL_TINKER);
  if (!ch->bSuccess(skill, SKILL_TINKER)) {
    ch->sendTo("You fumble with the delicate mechanism.\n\r");
    if (::number(0, 1)) {
      ch->sendTo("Your mistake triggers the trap!\n\r");
      ch->triggerContTrap(container);
      return TRUE;
    }
    return FALSE;
  }

  container->spec = 169;  // tinkerBagFuse
  ch->sendTo("You carefully install a fuse mechanism into the container.\n\r");
  act("$n tinkers with $p, installing something inside it.", TRUE, ch, container, 0,
    TO_ROOM);

  return TRUE;
}

int TBeing::tinkerBagStretch(TBeing* ch, TOpenContainer* container) {
  // Basic validation
  if (!container) {
    ch->sendTo("What are you trying to stretch?\n\r");
    return FALSE;
  }

  // Check if they have the skill
  if (!ch->doesKnowSkill(SKILL_TINKER)) {
    ch->sendTo("You don't know how to stretch containers.\n\r");
    return FALSE;
  }

  // Check restricted vnums first
  int vnum = container->objVnum();
  if (vnum >= 18613 || vnum <= 18615 || vnum == 31888 || vnum == 13887) {
    return tinkerBagExplode(ch, container);
  }

  // Check if the bag is at maximum capacity
  if (container->getCarriedVolume() >= 60000) {
    ch->sendTo("This container cannot be stretched any further.\n\r");
    return FALSE;
  }

  // Check structural integrity
  if (container->getStructPoints() < (container->getMaxStructPoints() * 0.2)) {
    ch->sendTo("This container is too damaged to be stretched safely.\n\r");
    return FALSE;
  }

  // Check if it's a spellbag (which shouldn't be stretchable)
  TSpellBag* spellbag = dynamic_cast<TSpellBag*>(container);
  if (spellbag) {
    ch->sendTo("Spell bags cannot be stretched.\n\r");
    return FALSE;
  }

  return start_task(ch, container, NULL, TASK_TINKER_STRETCH, "", 0, ch->in_room, 1, 0, 40);
}

int TBeing::tinkerBagExplode(TBeing* ch, TObj* obj) {
  ch->sendTo("\n\r\n\r<r>As you attempt to modify the bag, something goes CATASTROPHICALLY wrong!<1>\n\r");
  act("\n\r<r>$n's tinkering triggers a MASSIVE explosion!<1>", TRUE, ch, 0, 0, TO_ROOM);
  
  // Using similar damage calculation to blazeOfGlory
  int dam = min(30000, ch->hitLimit() * 100);  // This is MASSIVE damage
  
  TRoom* rm = real_roomp(ch->in_room);
  if (!rm) {
    vlogf(LOG_PROC, "Explosion in room : Room::NOWHERE. (tinkerBagExplode() disc_thief_traps.cc)");
    return FALSE;
  }

  // Damage everyone in the room
  for (StuffIter it = rm->stuff.begin(); it != rm->stuff.end();) {
    TThing* t = *(it++);
    TBeing* v = dynamic_cast<TBeing*>(t);
    if (!v)
      continue;
    int rc = v->objDamage(DAMAGE_TRAP_TNT, dam, obj);
    if (IS_SET_ONLY(rc, DELETE_THIS)) {
      delete v;
      v = NULL;
    }
  }
  delete obj;
  return TRUE;  // obj is deleted
}

int TBeing::tinkerJunk(TBeing* ch, TObj* obj, const char* mod_type) {
  TTrash* trash = dynamic_cast<TTrash*>(obj);
  if (!trash) {
    ch->sendTo("That's not actually junk.\n\r");
    return FALSE;
  }

  if (is_abbrev(mod_type, "reclaim")) {
    return tinkerJunkReclaim(ch, trash);
  }

  ch->sendTo("Valid junk modifications: reclaim\n\r");
  return FALSE;
}

int TBeing::tinkerJunkReclaim(TBeing* ch, TTrash* trash) {
  int skill = ch->getSkillValue(SKILL_TINKER);
  if (!ch->bSuccess(skill, SKILL_TINKER)) {
    ch->sendTo("You fumble with the delicate process.\n\r");
    delete trash;
    return FALSE;
  }

  TObj* obj = read_object(Obj::GENERIC_COMMODITY, VIRTUAL);
  TCommodity* commod = dynamic_cast<TCommodity*>(obj);
  if (!commod) {
    ch->sendTo("Error creating commodity.\n\r");
    delete obj;
    return FALSE;
  }

  commod->setWeight(trash->getWeight() / 2.0);
  commod->setMaterial(trash->getMaterial());

  *ch += *commod;

  ch->sendTo("You carefully reclaim usable material from the junk.\n\r");
  act("$n tinkers with $p, salvaging usable materials.", TRUE, ch, trash, 0, TO_ROOM);

  delete trash;
  return TRUE;
}

int TBeing::tinkerBagStraps(TBeing* ch, TOpenContainer* container) {
  // Basic validation
  if (!container) {
    ch->sendTo("What are you trying to add straps to?\n\r");
    return FALSE;
  }

  // Check if they have the skill
  if (!ch->doesKnowSkill(SKILL_TINKER)) {
    ch->sendTo("You don't know how to add straps to containers.\n\r");
    return FALSE;
  }

  // Check restricted vnums first
  int vnum = container->objVnum();
  if (vnum >= 18613 || vnum <= 18615 || vnum == 31888 || vnum == 13887) {
    return tinkerBagExplode(ch, container);
  }

  // Check if it already has back straps
  if (container->canWear(ITEM_WEAR_BACK)) {
    ch->sendTo("This bag already has back straps.\n\r");
    return FALSE;
  }

  // Check if it's a spellbag (which shouldn't be modified)
  TSpellBag* spellbag = dynamic_cast<TSpellBag*>(container);
  if (spellbag) {
    ch->sendTo("Spell bags cannot be modified with straps.\n\r");
    return FALSE;
  }

  return start_task(ch, container, NULL, TASK_TINKER_STRAP, "", 0, ch->in_room, 1, 0, 40);
}

int TBeing::tinkerBagReinforce(TBeing* ch, TOpenContainer* container) {
  // Basic validation
  if (!container) {
    ch->sendTo("What are you trying to reinforce?\n\r");
    return FALSE;
  }

  // Check if they have the skill
  if (!ch->doesKnowSkill(SKILL_TINKER)) {
    ch->sendTo("You don't know how to reinforce containers.\n\r");
    return FALSE;
  }

  // Check restricted vnums first
  int vnum = container->objVnum();
  if (vnum >= 18613 || vnum <= 18615 || vnum == 31888 || vnum == 13887) {
    return tinkerBagExplode(ch, container);
  }

  // Check if it's already at max reinforcement
  if (container->getStructPoints() >= container->getMaxStructPoints()) {
    ch->sendTo("This container is already fully reinforced.\n\r");
    return FALSE;
  }

  // Check if it's a spellbag (which shouldn't be modified)
  TSpellBag* spellbag = dynamic_cast<TSpellBag*>(container);
  if (spellbag) {
    ch->sendTo("Spell bags cannot be reinforced.\n\r");
    return FALSE;
  }

  return start_task(ch, container, NULL, TASK_TINKER_REINFORCE, "", 0, ch->in_room, 1, 0, 40);
}

int TBeing::tinkerBagPatch(TBeing* ch, TOpenContainer* container) {
  // Initial checks
  if (!container) {
    ch->sendTo("What are you trying to patch?\n\r");
    return FALSE;
  }

  // Check if they have the skill
  if (!ch->doesKnowSkill(SKILL_TINKER)) {
    ch->sendTo("You don't know how to patch containers.\n\r");
    return FALSE;
  }

  // Check restricted vnums first
  int vnum = container->objVnum();
  if (vnum >= 18613 || vnum <= 18615 || vnum == 31888 || vnum == 13887) {
    return tinkerBagExplode(ch, container);
  }

  if (container->getStructPoints() >= container->getMaxStructPoints()) {
    ch->sendTo("This bag doesn't need any repairs.\n\r");
    return FALSE;
  }

  // Check if it's a spellbag (which shouldn't be modified)
  TSpellBag* spellbag = dynamic_cast<TSpellBag*>(container);
  if (spellbag) {
    ch->sendTo("Spell bags cannot be patched.\n\r");
    return FALSE;
  }

  // Find materials in inventory
  TCommodity* mat = NULL;
  for (StuffIter it = ch->stuff.begin(); it != ch->stuff.end(); ++it) {
    TCommodity* comm = dynamic_cast<TCommodity*>(*it);
    if (comm && comm->getMaterial() == container->getMaterial()) {
      mat = comm;
      break;
    }
  }

  if (!mat) {
    act(format("You need some %s to patch $p.") % 
        material_nums[container->getMaterial()].mat_name,
        FALSE, ch, container, 0, TO_CHAR);
    return FALSE;
  }

  // Store material vnum in task arg for the task implementation to use
  sstring task_arg = format("%d") % mat->objVnum();

  // Show start messages
  ch->sendTo("You begin carefully patching the container.\n\r");
  act("$n begins carefully patching $p.", TRUE, ch, container, 0, TO_ROOM);

  // Start the task - let the task implementation handle material calculations and consumption
  return start_task(ch, container, NULL, TASK_TINKER_PATCH, task_arg.c_str(), 0, ch->in_room, 1, 0, 40);
}

int TBeing::tinkerBagBelt(TBeing* ch, TOpenContainer* container) {
  // Basic validation
  if (!container) {
    ch->sendTo("What are you trying to add a belt loop to?\n\r");
    return FALSE;
  }

  // Check if they have the skill
  if (!ch->doesKnowSkill(SKILL_TINKER)) {
    ch->sendTo("You don't know how to add belt loops to containers.\n\r");
    return FALSE;
  }

  // Check restricted vnums first
  int vnum = container->objVnum();
  if (vnum >= 18613 || vnum <= 18615 || vnum == 31888 || vnum == 13887) {
    return tinkerBagExplode(ch, container);
  }

  // Check if it already has waist wear
  if (container->canWear(ITEM_WEAR_WAIST)) {
    ch->sendTo("This bag already has belt loops.\n\r");
    return FALSE;
  }

  // Check if it's a spellbag (which shouldn't be modified)
  TSpellBag* spellbag = dynamic_cast<TSpellBag*>(container);
  if (spellbag) {
    ch->sendTo("Spell bags cannot be modified with belt loops.\n\r");
    return FALSE;
  }

  // Check if the bag is at maximum capacity
  if (container->getCarriedVolume() >= 5000) {
    ch->sendTo("This container is too large to be modified with a belt loop.\n\r");
    return FALSE;
  }

  // Check structural integrity
  if (container->getStructPoints() < (container->getMaxStructPoints() * 0.2)) {
    ch->sendTo("This container is too damaged to be modified.\n\r");
    return FALSE;
  }

  ch->sendTo("You begin carefully adding a belt loop to the container.\n\r");
  act("$n begins carefully adding a belt loop to $p.", TRUE, ch, container, 0, TO_ROOM);

  return start_task(ch, container, NULL, TASK_TINKER_BELT, "", 0, ch->in_room, 1, 0, 40);
}

int TBeing::tinkerBagMark(TBeing* ch, TOpenContainer* container) {
  // Basic validation
  if (!container) {
    ch->sendTo("What are you trying to monogram?\n\r");
    return FALSE;
  }

  // Check if they have the skill
  if (!ch->doesKnowSkill(SKILL_TINKER)) {
    ch->sendTo("You don't know how to monogram containers.\n\r");
    return FALSE;
  }

  // Check restricted vnums first
  int vnum = container->objVnum();
  if (vnum >= 18613 || vnum <= 18615 || vnum == 31888 || vnum == 13887) {
    return tinkerBagExplode(ch, container);
  }

  // Check if it's already monogrammed
  if (container->isMonogrammed()) {
    ch->sendTo("This container is already monogrammed.\n\r");
    return FALSE;
  }

  // Check if it's a spellbag (which shouldn't be modified)
  TSpellBag* spellbag = dynamic_cast<TSpellBag*>(container);
  if (spellbag) {
    ch->sendTo("Spell bags cannot be monogrammed.\n\r");
    return FALSE;
  }

  // Check structural integrity
  if (container->getStructPoints() < (container->getMaxStructPoints() * 0.2)) {
    ch->sendTo("This container is too damaged to be monogrammed.\n\r");
    return FALSE;
  }

  ch->sendTo("You begin carefully monogramming your mark on the container.\n\r");
  act("$n begins carefully marking $p with a personal monogram.", TRUE, ch, container, 0, TO_ROOM);

  return start_task(ch, container, NULL, TASK_TINKER_MARK, "", 0, ch->in_room, 1, 0, 40);
}

int TBeing::tinkerBagCamo(TBeing* ch, TOpenContainer* container) {
  // Basic validation
  if (!container) {
    ch->sendTo("What are you trying to camouflage?\n\r");
    return FALSE;
  }

  // Check if they have the skill
  if (!ch->doesKnowSkill(SKILL_TINKER)) {
    ch->sendTo("You don't know how to camouflage containers.\n\r");
    return FALSE;
  }

  // Check restricted vnums first
  int vnum = container->objVnum();
  if (vnum >= 18613 || vnum <= 18615 || vnum == 31888 || vnum == 13887) {
    return tinkerBagExplode(ch, container);
  }

  // Check if it's a spellbag (which shouldn't be modified)
  TSpellBag* spellbag = dynamic_cast<TSpellBag*>(container);
  if (spellbag) {
    ch->sendTo("Spell bags cannot be camouflaged.\n\r");
    return FALSE;
  }

  // Check structural integrity
  if (container->getStructPoints() < (container->getMaxStructPoints() * 0.2)) {
    ch->sendTo("This container is too damaged to be camouflaged.\n\r");
    return FALSE;
  }

  // Find ghostly material in inventory
  TCommodity* mat = NULL;
  for (StuffIter it = ch->stuff.begin(); it != ch->stuff.end(); ++it) {
    TCommodity* comm = dynamic_cast<TCommodity*>(*it);
    if (comm && comm->getMaterial() == MAT_GHOSTLY) {
      mat = comm;
      break;
    }
  }

  if (!mat) {
    ch->sendTo("You need some ghostly material to camouflage this container.\n\r");
    return FALSE;
  }

  ch->sendTo("You begin carefully applying ghostly material to the container.\n\r");
  act("$n begins carefully applying a strange substance to $p.", TRUE, ch, container, 0, TO_ROOM);

  // Store material vnum in task arg for the task implementation to use
  sstring task_arg = format("%d") % mat->objVnum();

  return start_task(ch, container, NULL, TASK_TINKER_CAMO, task_arg.c_str(), 0, ch->in_room, 1, 0, 40);
}

