//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD - All rights reserved, SneezyMUD Coding Team
//      "task_keycut.cc" - Task implementation for the Keycut skill
//
//////////////////////////////////////////////////////////////////////////

#include "comm.h"
#include "handler.h"
#include "extern.h"
#include "room.h"
#include "being.h"
#include "obj_commodity.h"
#include "materials.h"
#include "disc_thief_looting.h"

// The wax copy keeps the original's item index, so obj_index[].virt still
// matches the door's key vnum and keyCheck() accepts it. Only the strung
// name, the material and the decay timer differ from the real thing.
static void keycut_make_key(TBeing* ch, roomDirData* exitp) {
  TObj* key = read_object(exitp->key, VIRTUAL);
  if (!key) {
    ch->sendTo("Your casting collapses into a shapeless lump.\n\r");
    return;
  }

  // swapToStrung() populates the strung fields from the prototype, so read
  // the original wording back off the object afterwards rather than before.
  key->swapToStrung();
  sstring original = key->shortDescr;

  // Keep the original keywords so the copy answers to the same words the real
  // key does, and add our own so it can be singled out from it.
  key->name = key->name + " wax casting";
  key->shortDescr = format("a wax casting of %s") % original;
  key->setDescr(
    format("A wax casting of %s lies here, already softening.") % original);

  key->setMaterial(MAT_WAX);
  key->obj_flags.decay_time = kWaxKeyDecay;

  *ch += *key;

  act("You peel $p free of the lock.", false, ch, key, nullptr, TO_CHAR);
  act("$n works something free of the lock and pockets it.", true, ch, nullptr,
    nullptr, TO_ROOM);
}

// The exit is re-checked every pulse: the door can be destroyed, or its lock
// changed, while the thief is still working. Returns nullptr after messaging
// and stopping the task if the attempt can no longer continue.
static roomDirData* keycut_recheck_exit(TBeing* ch) {
  dirTypeT door = static_cast<dirTypeT>(ch->task->status);
  roomDirData* exitp = ch->exitDir(door);

  if (!exitp || exitp->door_type == DOOR_NONE ||
      IS_SET(exitp->condition, EXIT_DESTROYED) ||
      IS_SET(exitp->condition, EXIT_CAVED_IN)) {
    ch->sendTo("There's no longer a lock there to copy.\n\r");
    ch->stopTask();
    return nullptr;
  }
  if (exitp->key <= 0) {
    ch->sendTo("There's no longer a key that fits this lock.\n\r");
    ch->stopTask();
    return nullptr;
  }

  return exitp;
}

static void keycut_pulse(TBeing* ch) {
  roomDirData* exitp = keycut_recheck_exit(ch);
  if (!exitp)
    return;

  if (--ch->task->timeLeft > 0) {
    if (!::number(0, 1))
      ch->sendTo("You work the wax carefully into the wards of the lock.\n\r");
    return;
  }

  // Wax is spent whether or not the casting takes, so consume it before the
  // roll. A thief who has ditched the wax mid-task gets nothing either way.
  if (!consumeWax(ch, ch->task->flags)) {
    ch->sendTo("You no longer have enough wax to finish the casting.\n\r");
    ch->stopTask();
    return;
  }

  // Any completed attempt starts the cooldown, won or lost. Charging it only
  // on success would let a thief retry until the roll lands, and the wax cost
  // alone is not enough of a brake on a skill that opens unpickable doors.
  installKeycutCooldown(ch);

  if (!ch->bSuccess(SKILL_KEYCUT)) {
    CF(SKILL_KEYCUT);
    ch->sendTo(
      "The wax tears as you draw it out, and the impression is ruined.\n\r");
    act("$n curses at a ruined lump of wax.", true, ch, 0, 0, TO_ROOM);
    ch->stopTask();
    return;
  }

  CS(SKILL_KEYCUT);
  keycut_make_key(ch, exitp);
  ch->stopTask();
}

int task_keycut(TBeing* ch, cmdTypeT cmd, const char*, int pulse, TRoom*,
  TObj*) {
  if (ch->isLinkdead() || (ch->getPosition() <= POSITION_SITTING)) {
    ch->stopTask();
    return false;
  }

  if (ch->utilityTaskCommand(cmd) || ch->nobrainerTaskCommand(cmd))
    return false;

  if (ch->in_room != ch->task->wasInRoom) {
    ch->sendTo("You've wandered away from the lock.\n\r");
    ch->stopTask();
    return false;
  }

  if (!ch->doesKnowSkill(SKILL_KEYCUT)) {
    ch->sendTo("You've forgotten how to do this.\n\r");
    ch->stopTask();
    return false;
  }

  switch (cmd) {
    case CMD_TASK_CONTINUE:
      ch->task->calcNextUpdate(pulse, Pulse::MOBACT);
      keycut_pulse(ch);
      return false;
    case CMD_ABORT:
    case CMD_STOP:
      act("You give up on the casting and scrape the wax away.", false, ch, 0,
        0, TO_CHAR);
      act("$n stops fiddling with the lock.", false, ch, 0, 0, TO_ROOM);
      ch->stopTask();
      break;
    case CMD_TASK_FIGHTING:
      ch->sendTo("You can't work a lock while under attack!\n\r");
      ch->stopTask();
      break;
    default:
      if (cmd < MAX_CMD_LIST)
        warn_busy(ch);
      break;
  }

  return true;
}
