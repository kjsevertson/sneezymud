//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD - All rights reserved, SneezyMUD Coding Team
//      "task_bangle.cc" - Task implementation for the Bangle skill
//
//////////////////////////////////////////////////////////////////////////

#include "comm.h"
#include "handler.h"
#include "extern.h"
#include "room.h"
#include "being.h"
#include "augment.h"

// The clock is the item's structure, and only a landed roll moves it. A miss
// costs nothing but the pulse, unless the shaman is not thinking, in which
// case it costs the item a point of structure.
static void bangle_pulse(TBeing* ch) {
  TObj* obj = ch->task->obj;
  if (!obj) {
    ch->sendTo("You have nothing left to work on.\n\r");
    ch->stopTask();
    return;
  }

  // Physical work costs movement, charged before the roll so a spent
  // worker stops rather than landing one more pulse on fumes.
  if (augmentDrain(ch, SKILL_BANGLE, true)) {
    ch->stopTask();
    return;
  }

  if (!ch->bSuccess(SKILL_BANGLE)) {
    CF(SKILL_BANGLE);

    // Intelligence is what keeps a bad turn of the work off the piece itself.
    if (ch->isIntelligent()) {
      act("The work turns badly, and you set $p down before it costs you.",
        false, ch, obj, 0, TO_CHAR);
      return;
    }

    act("You crimp $p somewhere it should not have bent.", false, ch, obj, 0,
      TO_CHAR);

    int rc = obj->damageItem(1);
    if (IS_SET_DELETE(rc, DELETE_THIS)) {
      act("$p breaks apart under the work.", false, ch, obj, 0, TO_CHAR);
      act("$p breaks apart in $n's hands.", true, ch, obj, 0, TO_ROOM);
      // stopTask() clears the task's hold on the object, so it has to run
      // while the pointer is still good.
      ch->stopTask();
      delete obj;
      return;
    }

    return;
  }

  CS(SKILL_BANGLE);

  ch->task->timeLeft -= getAugmentTickAmount(ch, SKILL_BANGLE);

  if (ch->task->timeLeft > 0) {
    if (!::number(0, 2))
      act("You draw another piece of $p down small.", false, ch, obj, 0,
        TO_CHAR);
    return;
  }

  // The conversion lands at once, here. bangleFinish() replaces the object
  // outright, so the task has to let go of it first.
  ch->stopTask();
  bangleFinish(ch, obj);
}

int task_bangle(TBeing* ch, cmdTypeT cmd, const char*, int pulse, TRoom*, TObj*) {
  if (ch->isLinkdead() || (ch->getPosition() <= POSITION_SITTING)) {
    ch->stopTask();
    return false;
  }

  if (ch->utilityTaskCommand(cmd) || ch->nobrainerTaskCommand(cmd))
    return false;

  if (ch->in_room != ch->task->wasInRoom) {
    ch->sendTo("You wander off and lose your place in the work.\n\r");
    ch->stopTask();
    return false;
  }

  if (!ch->doesKnowSkill(SKILL_BANGLE)) {
    ch->sendTo("You've forgotten how to do this.\n\r");
    ch->stopTask();
    return false;
  }

  switch (cmd) {
    case CMD_TASK_CONTINUE:
      ch->task->calcNextUpdate(pulse, Pulse::MOBACT);
      bangle_pulse(ch);
      return false;
    case CMD_ABORT:
    case CMD_STOP:
      act("You set the work down, leaving the piece as it is.", false, ch, 0,
        0, TO_CHAR);
      act("$n sets down what $e was working on.", true, ch, 0, 0, TO_ROOM);
      ch->stopTask();
      break;
    case CMD_TASK_FIGHTING:
      ch->sendTo("You can't do fine work while under attack!\n\r");
      ch->stopTask();
      break;
    default:
      if (cmd < MAX_CMD_LIST)
        warn_busy(ch);
      break;
  }

  return true;
}
