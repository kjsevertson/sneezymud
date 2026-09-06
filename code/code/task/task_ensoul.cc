//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD - All rights reserved, SneezyMUD Coding Team
//      "task_ensoul.cc" - Calling a soul into a powerstone
//
//////////////////////////////////////////////////////////////////////////

#include "comm.h"
#include "handler.h"
#include "extern.h"
#include "room.h"
#include "being.h"
#include "augment.h"

// Ensoul is paid for in lifeforce rather than movement: the shaman is spending
// themselves, not their back. The draw is DeadRepair's per-pulse rate scaled by
// the stone's carats, which makes a big opal enormously expensive -- fifty
// carats is five hundred successes at fifty times the rate -- and that expense
// is the whole reason a Level 10 soulstone is rare.
//
// The carat count rides in task->status.
static void ensoul_pulse(TBeing* ch) {
  TObj* obj = ch->task->obj;
  if (!obj) {
    ch->sendTo("The stone is gone from your hands.\n\r");
    ch->stopTask();
    return;
  }

  int carats = max(1, static_cast<int>(ch->task->status));

  ch->addToLifeforce(-(::number(15, 30) * carats));

  if (ch->getLifeforce() < 30) {
    act("You have nothing left to give $p, and the call fails.", false, ch, obj,
      0, TO_CHAR);
    act("$n sags, and the stone in $s hands goes dull.", true, ch, obj, 0,
      TO_ROOM);
    ch->stopTask();
    return;
  }

  if (!ch->bSuccess(SKILL_ENSOUL)) {
    CF(SKILL_ENSOUL);

    if (!::number(0, 2))
      act("Your call goes out and finds nothing to answer it.", false, ch, obj,
        0, TO_CHAR);

    return;
  }

  CS(SKILL_ENSOUL);
  ch->task->timeLeft -= getAugmentTickAmount(ch, SKILL_ENSOUL);

  if (ch->task->timeLeft > 0) {
    if (!::number(0, 2))
      act("Something moves toward $p, and you hold it there.", false, ch, obj,
        0, TO_CHAR);
    return;
  }

  // stopTask() first: ensoulFinish() destroys the opal the task is holding.
  ch->stopTask();
  ensoulFinish(ch, obj, carats);
}

int task_ensoul(TBeing* ch, cmdTypeT cmd, const char*, int pulse, TRoom*,
  TObj*) {
  if (ch->isLinkdead() || (ch->getPosition() <= POSITION_SITTING)) {
    ch->stopTask();
    return false;
  }

  if (ch->utilityTaskCommand(cmd) || ch->nobrainerTaskCommand(cmd))
    return false;

  if (ch->in_room != ch->task->wasInRoom) {
    ch->sendTo("You lose the thread of the call.\n\r");
    ch->stopTask();
    return false;
  }

  if (!ch->doesKnowSkill(SKILL_ENSOUL)) {
    ch->sendTo("You've forgotten how to do this.\n\r");
    ch->stopTask();
    return false;
  }

  switch (cmd) {
    case CMD_TASK_CONTINUE:
      ch->task->calcNextUpdate(pulse, Pulse::MOBACT);
      ensoul_pulse(ch);
      return false;
    case CMD_ABORT:
    case CMD_STOP:
      act("You let the call go, and the stone is only a stone.", false, ch, 0,
        0, TO_CHAR);
      act("$n stops chanting.", true, ch, 0, 0, TO_ROOM);
      ch->stopTask();
      break;
    case CMD_TASK_FIGHTING:
      ch->sendTo("You can't hold a call while under attack!\n\r");
      ch->stopTask();
      break;
    default:
      if (cmd < MAX_CMD_LIST)
        warn_busy(ch);
      break;
  }

  return true;
}
