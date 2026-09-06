//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD - All rights reserved, SneezyMUD Coding Team
//      "task_resize.cc" - Reworking a finished piece to a different size
//
//////////////////////////////////////////////////////////////////////////

#include "comm.h"
#include "handler.h"
#include "extern.h"
#include "room.h"
#include "being.h"
#include "augment.h"
#include "race.h"

// The piece exists from the first pulse, made at the level the smith projected
// for it. Nothing here can raise that; misses only take it away. A bad forge
// still leaves real armor, carrying less of what the metal remembered and less
// of the shape the smith intended.
//
// The bar's grade rides in task->status and the running penalty in
// task->flags, as a percentage of the projection.
// Reshaping, not making: there is no ingot grade to answer to and nothing to
// degrade. A miss costs the pulse and the movement, and that is all. The
// piece's own structure is the clock, so a breastplate is a longer job than a
// wristlet.
static void resize_pulse(TBeing* ch) {
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

  if (!ch->bSuccess(getForgeRollMod(ch, obj->getMaterial()), SKILL_FORGE)) {
    CF(SKILL_FORGE);

    if (!::number(0, 2))
      act("The metal will not move the way you want it to.", false, ch, obj, 0,
        TO_CHAR);

    return;
  }

  CS(SKILL_FORGE);
  ch->task->timeLeft -= getAugmentTickAmount(ch, SKILL_FORGE);

  if (ch->task->timeLeft > 0) {
    if (!::number(0, 2))
      act("You draw another part of $p closer to the shape you want.", false,
        ch, obj, 0, TO_CHAR);
    return;
  }

  race_t race = static_cast<race_t>(ch->task->status);
  ch->stopTask();
  resizeFinish(ch, obj, race);
}

int task_resize(TBeing* ch, cmdTypeT cmd, const char*, int pulse, TRoom*,
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
      resize_pulse(ch);
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
