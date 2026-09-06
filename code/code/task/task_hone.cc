//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD - All rights reserved, SneezyMUD Coding Team
//      "task_hone.cc" - Working a weapon past what the anvil could give it
//
//////////////////////////////////////////////////////////////////////////

#include "comm.h"
#include "handler.h"
#include "extern.h"
#include "room.h"
#include "being.h"
#include "augment.h"
#include "obj_general_weapon.h"

// The anvil stops at six sevenths of a level-35 smith, which is 30. Everything
// above that is honed in a point at a time: ten landed pulses to each, up to
// the smith's own level plus a point for every twenty of the skill they know.
//
// Difficulty is the metal's, so a fine blade resists being made finer.
static void hone_pulse(TBeing* ch) {
  TGenWeapon* blade = dynamic_cast<TGenWeapon*>(ch->task->obj);
  if (!blade) {
    ch->sendTo("There is nothing on the stone.\n\r");
    ch->stopTask();
    return;
  }

  if (augmentDrain(ch, SKILL_FORGE, true)) {
    ch->stopTask();
    return;
  }

  int ceiling = getHoneMax(ch);
  if (blade->getWeapDamLvl() >= ceiling) {
    act("$p will take no more from you.", false, ch, blade, 0, TO_CHAR);
    ch->stopTask();
    return;
  }

  if (!ch->bSuccess(getForgeRollMod(ch, blade->getMaterial()), SKILL_FORGE)) {
    CF(SKILL_FORGE);

    if (!::number(0, 2))
      act("The stone skips, and nothing is gained.", false, ch, blade, 0,
        TO_CHAR);

    return;
  }

  CS(SKILL_FORGE);
  ch->task->timeLeft--;

  if (ch->task->timeLeft > 0) {
    if (!::number(0, 2))
      act("You draw the stone along $p again.", false, ch, blade, 0, TO_CHAR);
    return;
  }

  // Ten landed pulses bought one point. Take it and start the next ten, so
  // honing runs on until the ceiling or an interruption.
  blade->setWeapDamLvl(blade->getWeapDamLvl() + 1);
  ch->task->timeLeft = 10;

  act("$p takes a keener edge.", false, ch, blade, 0, TO_CHAR);
  ch->sendTo(format("It strikes at level %d now.\n\r") %
             blade->getWeapDamLvl());

  if (blade->getWeapDamLvl() >= ceiling) {
    act("$p is everything you know how to make it.", false, ch, blade, 0,
      TO_CHAR);
    act("$n finishes honing $p.", true, ch, blade, 0, TO_ROOM);
    ch->stopTask();
  }
}

int task_hone(TBeing* ch, cmdTypeT cmd, const char*, int pulse, TRoom*, TObj*) {
  if (ch->isLinkdead() || (ch->getPosition() <= POSITION_SITTING)) {
    ch->stopTask();
    return false;
  }

  if (ch->utilityTaskCommand(cmd) || ch->nobrainerTaskCommand(cmd))
    return false;

  if (ch->in_room != ch->task->wasInRoom) {
    ch->sendTo("You wander away from the stone.\n\r");
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
      hone_pulse(ch);
      return false;
    case CMD_ABORT:
    case CMD_STOP:
      act("You set the stone down.", false, ch, 0, 0, TO_CHAR);
      act("$n sets down $s whetstone.", true, ch, 0, 0, TO_ROOM);
      ch->stopTask();
      break;
    case CMD_TASK_FIGHTING:
      ch->sendTo("You can't hone an edge while under attack!\n\r");
      ch->stopTask();
      break;
    default:
      if (cmd < MAX_CMD_LIST)
        warn_busy(ch);
      break;
  }

  return true;
}
