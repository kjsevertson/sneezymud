//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD - All rights reserved, SneezyMUD Coding Team
//      "task_distill.cc" - Drawing the virtue out of a piece of jewelry
//
//////////////////////////////////////////////////////////////////////////

#include "comm.h"
#include "handler.h"
#include "extern.h"
#include "room.h"
#include "being.h"
#include "augment.h"

// The item is destroyed either way -- what a missed pulse costs is time, and
// the structure of the piece is the clock. Nothing is decided here; the whole
// deposit happens at the end, from what the item was still carrying.
static void distill_pulse(TBeing* ch) {
  TObj* obj = ch->task->obj;
  if (!obj) {
    ch->sendTo("There is nothing left in front of you.\n\r");
    ch->stopTask();
    return;
  }

  if (augmentDrain(ch, SKILL_DISTILL, false)) {
    ch->stopTask();
    return;
  }

  if (!ch->bSuccess(SKILL_DISTILL)) {
    CF(SKILL_DISTILL);

    if (!::number(0, 2))
      act("The virtue in $p slips away from you.", false, ch, obj, 0, TO_CHAR);

    return;
  }

  CS(SKILL_DISTILL);
  ch->task->timeLeft -= getAugmentTickAmount(ch, SKILL_DISTILL);

  if (ch->task->timeLeft > 0) {
    if (!::number(0, 2))
      act("Another thread of $p comes loose in your hands.", false, ch, obj, 0,
        TO_CHAR);
    return;
  }

  // stopTask() first: distillFinish() destroys the item the task is holding.
  ch->stopTask();
  distillFinish(ch, obj);
}

int task_distill(TBeing* ch, cmdTypeT cmd, const char*, int pulse, TRoom*,
  TObj*) {
  if (ch->isLinkdead() || (ch->getPosition() <= POSITION_SITTING)) {
    ch->stopTask();
    return false;
  }

  if (ch->utilityTaskCommand(cmd) || ch->nobrainerTaskCommand(cmd))
    return false;

  if (ch->in_room != ch->task->wasInRoom) {
    ch->sendTo("You wander away from the work.\n\r");
    ch->stopTask();
    return false;
  }

  if (!ch->doesKnowSkill(SKILL_DISTILL)) {
    ch->sendTo("You've forgotten how to do this.\n\r");
    ch->stopTask();
    return false;
  }

  switch (cmd) {
    case CMD_TASK_CONTINUE:
      ch->task->calcNextUpdate(pulse, Pulse::MOBACT);
      distill_pulse(ch);
      return false;
    case CMD_ABORT:
    case CMD_STOP:
      act("You let the work go, and $p is whole yet.", false, ch,
        ch->task ? ch->task->obj : 0, 0, TO_CHAR);
      ch->stopTask();
      break;
    case CMD_TASK_FIGHTING:
      ch->sendTo("You can't hold this together while under attack!\n\r");
      ch->stopTask();
      break;
    default:
      if (cmd < MAX_CMD_LIST)
        warn_busy(ch);
      break;
  }

  return true;
}
