//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD - All rights reserved, SneezyMUD Coding Team
//      "task_tailor.cc" - Cutting a cloth piece to a different size
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
// Cloth's half of resizing. Same shape as the forge's: nothing degrades, a
// miss costs the pulse and the movement, and the piece's own structure is the
// clock. Cloth is lighter work, so the drain runs at the lower rate.
static void tailor_pulse(TBeing* ch) {
  TObj* obj = ch->task->obj;
  if (!obj) {
    ch->sendTo("There is nothing laid out in front of you.\n\r");
    ch->stopTask();
    return;
  }

  if (augmentDrain(ch, SKILL_TAILOR, false)) {
    ch->stopTask();
    return;
  }

  if (!ch->bSuccess(SKILL_TAILOR)) {
    CF(SKILL_TAILOR);

    if (!::number(0, 2))
      act("The cloth will not fall the way you want it to.", false, ch, obj, 0,
        TO_CHAR);

    return;
  }

  CS(SKILL_TAILOR);
  ch->task->timeLeft -= getAugmentTickAmount(ch, SKILL_TAILOR);

  if (ch->task->timeLeft > 0) {
    if (!::number(0, 2))
      act("You take in another seam of $p.", false, ch, obj, 0, TO_CHAR);
    return;
  }

  race_t race = static_cast<race_t>(ch->task->status);
  ch->stopTask();
  tailorFinish(ch, obj, race);
}

int task_tailor(TBeing* ch, cmdTypeT cmd, const char*, int pulse, TRoom*,
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

  if (!ch->doesKnowSkill(SKILL_TAILOR)) {
    ch->sendTo("You've forgotten how to do this.\n\r");
    ch->stopTask();
    return false;
  }

  switch (cmd) {
    case CMD_TASK_CONTINUE:
      ch->task->calcNextUpdate(pulse, Pulse::MOBACT);
      tailor_pulse(ch);
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
