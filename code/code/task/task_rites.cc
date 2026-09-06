//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD - All rights reserved, SneezyMUD Coding Team
//      "task_rites.cc" - Saying the rites over a corpse, for Rites
//
//////////////////////////////////////////////////////////////////////////

#include "comm.h"
#include "handler.h"
#include "extern.h"
#include "room.h"
#include "being.h"
#include "augment.h"
#include "obj_vial.h"
#include "obj_symbol.h"

// Attune's shape, over a body rather than a symbol: seated, symbol in hand,
// holy water spent as the work goes. Skill decides only whether the ritual
// lands -- how much it yields is the stone's Level against the corpse's, and
// nothing about the cleric enters that.
static void rites_pulse(TBeing* ch) {
  TObj* corpse = ch->task->obj;
  if (!corpse) {
    ch->sendTo("The body is gone from before you.\n\r");
    ch->stopTask();
    return;
  }

  if (ch->getPosition() > POSITION_SITTING) {
    ch->sendTo("You rise, and the rites break off.\n\r");
    ch->stopTask();
    return;
  }

  if (!dynamic_cast<TSymbol*>(ch->equipment[ch->getPrimaryHold()])) {
    ch->sendTo("Your hands are empty of your symbol, and the words stop.\n\r");
    ch->stopTask();
    return;
  }

  ch->addToMove(-1);
  if (ch->getMove() < 10) {
    act("You are much too tired to keep the rites going.", false, ch, corpse, 0,
      TO_CHAR);
    act("$n falters, and stops praying.", true, ch, corpse, 0, TO_ROOM);
    ch->stopTask();
    return;
  }

  // Water goes on the body as the words go over it, a unit at a time.
  TVial* water = findHolyWater(ch);
  if (!water) {
    ch->sendTo("Your holy water is gone, and the rites cannot go on.\n\r");
    ch->stopTask();
    return;
  }

  if (!::number(0, 2)) {
    act("You anoint $p from $P.", false, ch, corpse, water, TO_CHAR);
    water->addToDrinkUnits(-1);
  }

  if (!ch->bSuccess(SKILL_RITES)) {
    CF(SKILL_RITES);

    if (!::number(0, 2))
      act("The words come out wrong, and nothing answers them.", false, ch,
        corpse, 0, TO_CHAR);

    return;
  }

  CS(SKILL_RITES);
  ch->task->timeLeft -= getAugmentTickAmount(ch, SKILL_RITES);

  if (ch->task->timeLeft > 0) {
    if (!::number(0, 2))
      act("You feel something in $p begin to loosen.", false, ch, corpse, 0,
        TO_CHAR);
    return;
  }

  ch->stopTask();
  ritesFinish(ch, corpse);
}

int task_rites(TBeing* ch, cmdTypeT cmd, const char*, int pulse, TRoom*,
  TObj*) {
  if (ch->isLinkdead()) {
    ch->stopTask();
    return false;
  }

  if (ch->utilityTaskCommand(cmd) || ch->nobrainerTaskCommand(cmd))
    return false;

  if (ch->in_room != ch->task->wasInRoom) {
    ch->sendTo("You leave the body behind, and the rites with it.\n\r");
    ch->stopTask();
    return false;
  }

  if (!ch->doesKnowSkill(SKILL_RITES)) {
    ch->sendTo("You've forgotten the words.\n\r");
    ch->stopTask();
    return false;
  }

  switch (cmd) {
    case CMD_TASK_CONTINUE:
      ch->task->calcNextUpdate(pulse, Pulse::MOBACT);
      rites_pulse(ch);
      return false;
    case CMD_ABORT:
    case CMD_STOP:
      act("You let the words trail off.", false, ch, 0, 0, TO_CHAR);
      act("$n stops praying.", true, ch, 0, 0, TO_ROOM);
      ch->stopTask();
      break;
    case CMD_TASK_FIGHTING:
      ch->sendTo("You can't hold a rite together while under attack!\n\r");
      ch->stopTask();
      break;
    default:
      if (cmd < MAX_CMD_LIST)
        warn_busy(ch);
      break;
  }

  return true;
}
