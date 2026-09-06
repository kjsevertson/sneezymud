//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD - All rights reserved, SneezyMUD Coding Team
//      "task_weave.cc" - Task implementation for the Weave skill
//
//////////////////////////////////////////////////////////////////////////

#include "comm.h"
#include "handler.h"
#include "extern.h"
#include "room.h"
#include "being.h"
#include "augment.h"

// The crucible's twin: a miss costs the piece nothing, since it is coming
// apart either way. What the misses buy is a worse grade of skein, and both
// tallies ride in the task's flags word exactly as they do for smelting.
static void weave_pulse(TBeing* ch) {
  TObj* obj = ch->task->obj;
  if (!obj) {
    ch->sendTo("There is nothing left in your hands.\n\r");
    ch->stopTask();
    return;
  }

  int hits = smeltHits(ch->task->flags);
  int misses = smeltMisses(ch->task->flags);

  // Physical work costs movement, charged before the roll so a spent
  // worker stops rather than landing one more pulse on fumes.
  if (augmentDrain(ch, SKILL_WEAVE, true)) {
    ch->stopTask();
    return;
  }

  if (!ch->bSuccess(SKILL_WEAVE)) {
    CF(SKILL_WEAVE);
    ch->task->flags = smeltTally(hits, misses + 1);

    if (!::number(0, 2))
      act("The thread snarls, and you lose the run of it.", false, ch, obj, 0,
        TO_CHAR);

    return;
  }

  CS(SKILL_WEAVE);
  ch->task->flags = smeltTally(hits + 1, misses);
  ch->task->timeLeft -= getAugmentTickAmount(ch, SKILL_WEAVE);

  if (ch->task->timeLeft > 0) {
    if (!::number(0, 2))
      act("Another clean length comes off $p.", false, ch, obj, 0, TO_CHAR);
    return;
  }

  // stopTask() first: weaveFinish() destroys the object the task is holding.
  int finalHits = smeltHits(ch->task->flags);
  int finalMisses = smeltMisses(ch->task->flags);
  ch->stopTask();
  weaveFinish(ch, obj, finalHits, finalMisses);
}

int task_weave(TBeing* ch, cmdTypeT cmd, const char*, int pulse, TRoom*,
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

  if (!ch->doesKnowSkill(SKILL_WEAVE)) {
    ch->sendTo("You've forgotten how to do this.\n\r");
    ch->stopTask();
    return false;
  }

  switch (cmd) {
    case CMD_TASK_CONTINUE:
      ch->task->calcNextUpdate(pulse, Pulse::MOBACT);
      weave_pulse(ch);
      return false;
    case CMD_ABORT:
    case CMD_STOP:
      act("You set the work down, half unpicked.", false, ch, 0, 0, TO_CHAR);
      act("$n sets down a half-unpicked tangle.", true, ch, 0, 0, TO_ROOM);
      ch->stopTask();
      break;
    case CMD_TASK_FIGHTING:
      ch->sendTo("You can't work fibre while under attack!\n\r");
      ch->stopTask();
      break;
    default:
      if (cmd < MAX_CMD_LIST)
        warn_busy(ch);
      break;
  }

  return true;
}
