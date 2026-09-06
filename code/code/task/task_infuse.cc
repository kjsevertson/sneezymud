//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD - All rights reserved, SneezyMUD Coding Team
//      "task_infuse.cc" - Writing an essence into a piece of gear
//
//////////////////////////////////////////////////////////////////////////

#include "comm.h"
#include "handler.h"
#include "extern.h"
#include "room.h"
#include "being.h"
#include "augment.h"

// Nothing is decided here: the write happens at the end, and a missed pulse
// costs only time. The piece's structure is the clock, so writing into a
// breastplate is longer work than into a ring.
static void infuse_pulse(TBeing* ch) {
  TObj* obj = ch->task->obj;
  if (!obj) {
    ch->sendTo("There is nothing left in front of you.\n\r");
    ch->stopTask();
    return;
  }

  if (augmentDrain(ch, SKILL_INFUSE, false)) {
    ch->stopTask();
    return;
  }

  if (!ch->bSuccess(SKILL_INFUSE)) {
    CF(SKILL_INFUSE);

    if (!::number(0, 2))
      act("The virtue will not take to $p.", false, ch, obj, 0, TO_CHAR);

    return;
  }

  CS(SKILL_INFUSE);
  ch->task->timeLeft -= getAugmentTickAmount(ch, SKILL_INFUSE);

  if (ch->task->timeLeft > 0) {
    if (!::number(0, 2))
      act("Another part of it settles into $p.", false, ch, obj, 0, TO_CHAR);
    return;
  }

  // The essence's name lives in orig_arg and stopTask() frees it, so take a
  // copy before letting the task go.
  sstring essenceName(ch->task->orig_arg ? ch->task->orig_arg : "");
  ch->stopTask();
  infuseFinish(ch, obj, essenceName.c_str());
}

int task_infuse(TBeing* ch, cmdTypeT cmd, const char*, int pulse, TRoom*,
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

  if (!ch->doesKnowSkill(SKILL_INFUSE)) {
    ch->sendTo("You've forgotten how to do this.\n\r");
    ch->stopTask();
    return false;
  }

  switch (cmd) {
    case CMD_TASK_CONTINUE:
      ch->task->calcNextUpdate(pulse, Pulse::MOBACT);
      infuse_pulse(ch);
      return false;
    case CMD_ABORT:
    case CMD_STOP:
      act("You let the work go, and nothing of it takes.", false, ch, 0, 0,
        TO_CHAR);
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
