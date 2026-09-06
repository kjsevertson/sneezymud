//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD - All rights reserved, SneezyMUD Coding Team
//      "task_smelt.cc" - Task implementation for the Smelt skill
//
//////////////////////////////////////////////////////////////////////////

#include "comm.h"
#include "handler.h"
#include "extern.h"
#include "room.h"
#include "being.h"
#include "augment.h"

// Smelt is the one augment task where a miss costs the item nothing -- it is
// going in the fire either way. What the misses buy is a worse grade of ingot:
// both tallies ride in the task's flags word, and their ratio is the grade.
static void smelt_pulse(TBeing* ch) {
  TObj* obj = ch->task->obj;
  if (!obj) {
    ch->sendTo("There is nothing left in the fire.\n\r");
    ch->stopTask();
    return;
  }

  int hits = smeltHits(ch->task->flags);
  int misses = smeltMisses(ch->task->flags);

  // Physical work costs movement, charged before the roll so a spent
  // worker stops rather than landing one more pulse on fumes.
  if (augmentDrain(ch, SKILL_SMELT, true)) {
    ch->stopTask();
    return;
  }

  if (!ch->bSuccess(SKILL_SMELT)) {
    CF(SKILL_SMELT);
    ch->task->flags = smeltTally(hits, misses + 1);

    if (!::number(0, 2))
      act("The melt runs dirty, and you skim what you can off $p.", false, ch,
        obj, 0, TO_CHAR);

    return;
  }

  CS(SKILL_SMELT);
  ch->task->flags = smeltTally(hits + 1, misses);
  ch->task->timeLeft -= getAugmentTickAmount(ch, SKILL_SMELT);

  if (ch->task->timeLeft > 0) {
    if (!::number(0, 2))
      act("Another run of clean metal comes off $p.", false, ch, obj, 0,
        TO_CHAR);
    return;
  }

  // stopTask() first: smeltFinish() destroys the object the task is holding.
  int finalHits = smeltHits(ch->task->flags);
  int finalMisses = smeltMisses(ch->task->flags);
  ch->stopTask();
  smeltFinish(ch, obj, finalHits, finalMisses);
}

int task_smelt(TBeing* ch, cmdTypeT cmd, const char*, int pulse, TRoom*,
  TObj*) {
  if (ch->isLinkdead() || (ch->getPosition() <= POSITION_SITTING)) {
    ch->stopTask();
    return false;
  }

  if (ch->utilityTaskCommand(cmd) || ch->nobrainerTaskCommand(cmd))
    return false;

  if (ch->in_room != ch->task->wasInRoom) {
    ch->sendTo("You wander away from the fire.\n\r");
    ch->stopTask();
    return false;
  }

  if (!ch->doesKnowSkill(SKILL_SMELT)) {
    ch->sendTo("You've forgotten how to do this.\n\r");
    ch->stopTask();
    return false;
  }

  switch (cmd) {
    case CMD_TASK_CONTINUE:
      ch->task->calcNextUpdate(pulse, Pulse::MOBACT);
      smelt_pulse(ch);
      return false;
    case CMD_ABORT:
    case CMD_STOP:
      act("You pull the work out of the fire, half done.", false, ch, 0, 0,
        TO_CHAR);
      act("$n pulls $s work out of the fire.", true, ch, 0, 0, TO_ROOM);
      ch->stopTask();
      break;
    case CMD_TASK_FIGHTING:
      ch->sendTo("You can't work a crucible while under attack!\n\r");
      ch->stopTask();
      break;
    default:
      if (cmd < MAX_CMD_LIST)
        warn_busy(ch);
      break;
  }

  return true;
}
