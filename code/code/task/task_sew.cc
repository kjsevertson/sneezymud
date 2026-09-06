//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD - All rights reserved, SneezyMUD Coding Team
//      "task_sew.cc" - Task implementation for sewing a piece from thread
//
//////////////////////////////////////////////////////////////////////////

#include "comm.h"
#include "handler.h"
#include "extern.h"
#include "room.h"
#include "being.h"
#include "augment.h"

// The forge's mirror in cloth. The piece exists from the first pulse at the
// level projected for it, and misses only take away: a bad job still leaves a
// real garment carrying less of what the thread remembered.
//
// The skein's grade rides in task->status and the running penalty in
// task->flags, as a percentage of the projection.
static void sew_pulse(TBeing* ch) {
  TObj* obj = ch->task->obj;
  if (!obj) {
    ch->sendTo("There is nothing laid out in front of you.\n\r");
    ch->stopTask();
    return;
  }

  if (augmentDrain(ch, SKILL_SEW, false)) {
    ch->stopTask();
    return;
  }

  int quality = max(1, min(5, static_cast<int>(ch->task->status)));

  if (!ch->bSuccess(getFibreRollMod(ch, obj->getMaterial()), SKILL_SEW)) {
    CF(SKILL_SEW);

    // Poor metal punishes a miss harder: a flawless bar loses one percent, a
    // crude one up to five.
    int cost = ::number(1, 6 - quality);
    ch->task->flags += cost;
    reduceOneApply(obj);

    // The same clumsy hand that cost the piece costs the skein it came from.
    spoilLeftoverThread(ch, ch->task->orig_arg, cost);

    act("The stitch pulls wrong, and $p is the worse for it.", false, ch, obj,
      0, TO_CHAR);
    return;
  }

  CS(SKILL_SEW);
  ch->task->timeLeft -= getAugmentTickAmount(ch, SKILL_SEW);

  if (ch->task->timeLeft > 0) {
    if (!::number(0, 2))
      act("You work another seam into $p.", false, ch, obj, 0, TO_CHAR);
    return;
  }

  int penalty = ch->task->flags;
  ch->stopTask();
  forgeFinish(ch, obj, penalty);
}

int task_sew(TBeing* ch, cmdTypeT cmd, const char*, int pulse, TRoom*,
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

  if (!ch->doesKnowSkill(SKILL_SEW)) {
    ch->sendTo("You've forgotten how to do this.\n\r");
    ch->stopTask();
    return false;
  }

  switch (cmd) {
    case CMD_TASK_CONTINUE:
      ch->task->calcNextUpdate(pulse, Pulse::MOBACT);
      sew_pulse(ch);
      return false;
    case CMD_ABORT:
    case CMD_STOP:
      act("You set down your needle, leaving the work half done.", false, ch,
        0, 0, TO_CHAR);
      act("$n sets down $s needle.", true, ch, 0, 0, TO_ROOM);
      ch->stopTask();
      break;
    case CMD_TASK_FIGHTING:
      ch->sendTo("You can't sew while under attack!\n\r");
      ch->stopTask();
      break;
    default:
      if (cmd < MAX_CMD_LIST)
        warn_busy(ch);
      break;
  }

  return true;
}
