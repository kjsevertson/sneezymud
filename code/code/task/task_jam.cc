//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD - All rights reserved, SneezyMUD Coding Team
//      "task_jam.cc" - Task implementation for the Jam skill
//
//////////////////////////////////////////////////////////////////////////

#include "comm.h"
#include "handler.h"
#include "extern.h"
#include "room.h"
#include "being.h"
#include "obj_general_weapon.h"
#include "disc_thief_looting.h"

// The exit is re-checked every pulse rather than cached: the door can be
// opened, locked or destroyed by anyone else in either room while the thief
// is still working. Returns nullptr (after messaging and stopping the task)
// if the attempt can no longer continue.
static roomDirData* jam_recheck_exit(TBeing* ch) {
  dirTypeT door = static_cast<dirTypeT>(ch->task->status);
  roomDirData* exitp = ch->exitDir(door);

  if (!exitp || exitp->door_type == DOOR_NONE ||
      IS_SET(exitp->condition, EXIT_DESTROYED) ||
      IS_SET(exitp->condition, EXIT_CAVED_IN)) {
    ch->sendTo("There's no longer a door there to jam.\n\r");
    ch->stopTask();
    return nullptr;
  }
  if (!IS_SET(exitp->condition, EXIT_CLOSED)) {
    act("Someone opens the $T, and your work comes to nothing.", false, ch, 0,
      reinterpret_cast<const TThing*>(exitp->getName().c_str()), TO_CHAR);
    ch->stopTask();
    return nullptr;
  }
  if (IS_SET(exitp->condition, EXIT_LOCKED)) {
    act("The $T is locked now; there's no point wedging it.", false, ch, 0,
      reinterpret_cast<const TThing*>(exitp->getName().c_str()), TO_CHAR);
    ch->stopTask();
    return nullptr;
  }

  return exitp;
}

static void jam_pulse(TBeing* ch, TGenWeapon* weapon) {
  roomDirData* exitp = jam_recheck_exit(ch);
  if (!exitp)
    return;

  if (--ch->task->timeLeft > 0) {
    if (!::number(0, 1))
      act("You lever $p deeper into the gap.", false, ch, weapon, nullptr,
        TO_CHAR);
    return;
  }

  // One roll decides the whole attempt. Rolling per pulse would make failure
  // unreachable, since a failed jam costs the thief nothing but time.
  if (!ch->bSuccess(SKILL_JAM)) {
    CF(SKILL_JAM);
    act("$p works loose and clatters to the ground.  The $T sits as it was.",
      false, ch, weapon,
      reinterpret_cast<const TThing*>(exitp->getName().c_str()), TO_CHAR);
    act("$n swears quietly and works $p back out of the $T.", true, ch, weapon,
      reinterpret_cast<const TThing*>(exitp->getName().c_str()), TO_ROOM);
    ch->stopTask();
    return;
  }

  CS(SKILL_JAM);
  applyJam(ch, static_cast<dirTypeT>(ch->task->status), jamLockDifficulty(ch));

  act("$p snaps off flush with the keyhole, binding the $T shut.", false, ch,
    weapon, reinterpret_cast<const TThing*>(exitp->getName().c_str()), TO_CHAR);
  act("$n snaps $p off flush with the keyhole of the $T.", true, ch, weapon,
    reinterpret_cast<const TThing*>(exitp->getName().c_str()), TO_ROOM);

  // stopTask() first: it clears the task-object flag, so the ~TObj sweep of
  // character_list that cancels tasks holding this object is skipped.
  ch->stopTask();
  --(*weapon);
  delete weapon;
}

int task_jam(TBeing* ch, cmdTypeT cmd, const char*, int pulse, TRoom*, TObj*) {
  if (ch->isLinkdead() || (ch->getPosition() <= POSITION_SITTING)) {
    ch->stopTask();
    return false;
  }

  if (ch->utilityTaskCommand(cmd) || ch->nobrainerTaskCommand(cmd))
    return false;

  if (ch->in_room != ch->task->wasInRoom) {
    ch->sendTo("You've wandered away from the door.\n\r");
    ch->stopTask();
    return false;
  }

  if (!ch->doesKnowSkill(SKILL_JAM)) {
    ch->sendTo("You've forgotten how to do this.\n\r");
    ch->stopTask();
    return false;
  }

  switch (cmd) {
    case CMD_TASK_CONTINUE: {
      TGenWeapon* weapon = dynamic_cast<TGenWeapon*>(ch->task->obj);
      // ~TObj cancels tasks holding a destroyed object, but the thief can
      // still have put it down or handed it off since the last pulse.
      if (!weapon || ((weapon->equippedBy != ch) && (weapon->parent != ch))) {
        ch->sendTo("You no longer have anything to jam the door with.\n\r");
        ch->stopTask();
        return false;
      }
      ch->task->calcNextUpdate(pulse, Pulse::MOBACT);
      jam_pulse(ch, weapon);
      return false;
    }
    case CMD_ABORT:
    case CMD_STOP:
      act("You give up on jamming the door.", false, ch, 0, 0, TO_CHAR);
      act("$n stops fiddling with the door.", false, ch, 0, 0, TO_ROOM);
      ch->stopTask();
      break;
    case CMD_TASK_FIGHTING:
      ch->sendTo("You can't jam a door while under attack!\n\r");
      ch->stopTask();
      break;
    default:
      if (cmd < MAX_CMD_LIST)
        warn_busy(ch);
      break;
  }

  return true;
}
