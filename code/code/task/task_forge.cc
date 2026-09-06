//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD - All rights reserved, SneezyMUD Coding Team
//      "task_forge.cc" - Task implementation for forging a piece of armor
//
//////////////////////////////////////////////////////////////////////////

#include "comm.h"
#include "handler.h"
#include "extern.h"
#include "room.h"
#include "being.h"
#include "augment.h"

// The piece exists from the first pulse, made at the level the smith projected
// for it. Nothing here can raise that; misses only take it away. A bad forge
// still leaves real armor, carrying less of what the metal remembered and less
// of the shape the smith intended.
//
// The bar's grade rides in task->status and the running penalty in
// task->flags, as a percentage of the projection.
static void forge_pulse(TBeing* ch) {
  TObj* obj = ch->task->obj;
  if (!obj) {
    ch->sendTo("There is nothing on the anvil.\n\r");
    ch->stopTask();
    return;
  }

  if (augmentDrain(ch, SKILL_FORGE, true)) {
    ch->stopTask();
    return;
  }

  int quality = max(1, min(5, static_cast<int>(ch->task->status)));

  if (!ch->bSuccess(getForgeRollMod(ch, obj->getMaterial()), SKILL_FORGE)) {
    CF(SKILL_FORGE);

    // Poor metal punishes a miss harder: a flawless bar loses one percent, a
    // crude one up to five.
    int cost = ::number(1, 6 - quality);
    ch->task->flags += cost;
    reduceOneApply(obj);

    // The same clumsy heat that cost the piece costs the bar it came from.
    spoilLeftoverMetal(ch, ch->task->orig_arg, cost);

    act("The hammer falls wrong, and $p is the worse for it.", false, ch, obj,
      0, TO_CHAR);
    return;
  }

  CS(SKILL_FORGE);
  ch->task->timeLeft -= getAugmentTickAmount(ch, SKILL_FORGE);

  if (ch->task->timeLeft > 0) {
    if (!::number(0, 2))
      act("You work another course into $p.", false, ch, obj, 0, TO_CHAR);
    return;
  }

  int penalty = ch->task->flags;
  ch->stopTask();
  forgeFinish(ch, obj, penalty);
}

int task_forge(TBeing* ch, cmdTypeT cmd, const char*, int pulse, TRoom*,
  TObj*) {
  if (ch->isLinkdead() || (ch->getPosition() <= POSITION_SITTING)) {
    ch->stopTask();
    return false;
  }

  if (ch->utilityTaskCommand(cmd) || ch->nobrainerTaskCommand(cmd))
    return false;

  if (ch->in_room != ch->task->wasInRoom) {
    ch->sendTo("You wander away from the anvil.\n\r");
    ch->stopTask();
    return false;
  }

  if (!ch->doesKnowSkill(SKILL_FORGE)) {
    ch->sendTo("You've forgotten how to do this.\n\r");
    ch->stopTask();
    return false;
  }

  switch (cmd) {
    case CMD_TASK_CONTINUE:
      ch->task->calcNextUpdate(pulse, Pulse::MOBACT);
      forge_pulse(ch);
      return false;
    case CMD_ABORT:
    case CMD_STOP:
      act("You set down your hammer, leaving the work half done.", false, ch, 0,
        0, TO_CHAR);
      act("$n sets down $s hammer.", true, ch, 0, 0, TO_ROOM);
      ch->stopTask();
      break;
    case CMD_TASK_FIGHTING:
      ch->sendTo("You can't work an anvil while under attack!\n\r");
      ch->stopTask();
      break;
    default:
      if (cmd < MAX_CMD_LIST)
        warn_busy(ch);
      break;
  }

  return true;
}
