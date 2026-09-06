//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD - All rights reserved, SneezyMUD Coding Team
//      "task_strip.cc" - Task implementation for the Strip skill
//
//////////////////////////////////////////////////////////////////////////

#include "comm.h"
#include "handler.h"
#include "extern.h"
#include "room.h"
#include "being.h"
#include "augment.h"

// The clock is the item's structure, and only a landed roll moves it. A miss
// costs nothing but the pulse, unless the thief is not thinking, in which case
// it costs the item a point of structure.
static void strip_pulse(TBeing* ch) {
  TObj* obj = ch->task->obj;
  if (!obj) {
    ch->sendTo("You have nothing left to work on.\n\r");
    ch->stopTask();
    return;
  }

  // Physical work costs movement, charged before the roll so a spent
  // worker stops rather than landing one more pulse on fumes.
  if (augmentDrain(ch, SKILL_STRIP, false)) {
    ch->stopTask();
    return;
  }

  if (!ch->bSuccess(SKILL_STRIP)) {
    CF(SKILL_STRIP);

    // Intelligence is what keeps a slipped cut off the piece itself.
    if (ch->isIntelligent()) {
      act("Your blade skids off $p without biting.", false, ch, obj, 0,
        TO_CHAR);
      return;
    }

    act("Your blade bites into $p in the wrong place.", false, ch, obj, 0,
      TO_CHAR);

    int rc = obj->damageItem(1);
    if (IS_SET_DELETE(rc, DELETE_THIS)) {
      act("$p comes apart in your hands.", false, ch, obj, 0, TO_CHAR);
      act("$p comes apart in $n's hands.", true, ch, obj, 0, TO_ROOM);
      // stopTask() clears the task's hold on the object, so it has to run
      // while the pointer is still good.
      ch->stopTask();
      delete obj;
      return;
    }

    return;
  }

  CS(SKILL_STRIP);

  ch->task->timeLeft -= getAugmentTickAmount(ch, SKILL_STRIP);

  if (ch->task->timeLeft > 0) {
    if (!::number(0, 2))
      act("You work another seam of $p loose.", false, ch, obj, 0, TO_CHAR);
    return;
  }

  // The whole demotion lands at once, here: flags, AC and structure together.
  // stripFinish() may replace the object, so the task has to let go of it first.
  ch->stopTask();
  stripFinish(ch, obj);
}

int task_strip(TBeing* ch, cmdTypeT cmd, const char*, int pulse, TRoom*, TObj*) {
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

  if (!ch->doesKnowSkill(SKILL_STRIP)) {
    ch->sendTo("You've forgotten how to do this.\n\r");
    ch->stopTask();
    return false;
  }

  switch (cmd) {
    case CMD_TASK_CONTINUE:
      ch->task->calcNextUpdate(pulse, Pulse::MOBACT);
      strip_pulse(ch);
      return false;
    case CMD_ABORT:
    case CMD_STOP:
      act("You stop cutting, leaving the piece as it is.", false, ch, 0, 0,
        TO_CHAR);
      act("$n stops cutting away at what $e was working on.", true, ch, 0, 0,
        TO_ROOM);
      ch->stopTask();
      break;
    case CMD_TASK_FIGHTING:
      ch->sendTo("You can't pick a seam apart while under attack!\n\r");
      ch->stopTask();
      break;
    default:
      if (cmd < MAX_CMD_LIST)
        warn_busy(ch);
      break;
  }

  return true;
}
