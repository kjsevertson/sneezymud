//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD - All rights reserved, SneezyMUD Coding Team
//      "task_bolster.cc" - Raising a piece toward its tier's ceiling
//
//////////////////////////////////////////////////////////////////////////

#include "comm.h"
#include "handler.h"
#include "extern.h"
#include "room.h"
#include "being.h"
#include "augment.h"
#include "obj_soulstone.h"
#include "obj_base_clothing.h"

// Bolster has no clock. It runs until the piece reaches the highest its tier
// and the wearer's level allow, until the stone runs dry, or until something
// stops it -- the same way sharpening simply halts at maximum sharpness.
//
// Every attempt costs charges whether or not it lands. A failure path that
// costs nothing would allow infinite retries and bypass the difficulty
// entirely, so the charge is spent first and the roll happens after.
static void bolster_pulse(TBeing* ch) {
  TObj* obj = ch->task->obj;
  TBaseClothing* clothing = dynamic_cast<TBaseClothing*>(obj);

  if (!obj || !clothing) {
    ch->sendTo("The work is gone from your hands.\n\r");
    ch->stopTask();
    return;
  }

  TSoulstone* stone = findSoulstone(ch);
  if (!stone) {
    ch->sendTo("Your soulstone is gone, and the work stops.\n\r");
    ch->stopTask();
    return;
  }

  Tier tier = getWearableTier(clothing);
  double ceiling = getBolsterMax(ch, tier);
  double level = clothing->armorLevel(ARMOR_LEV_REAL);

  if (level >= ceiling) {
    act("$p will take nothing more from you.", false, ch, obj, 0, TO_CHAR);
    ch->stopTask();
    return;
  }

  int cost = getBolsterChargeCost(level, stone->getSoulLevel());

  if (!stone->spendCharges(cost)) {
    act("$p goes cold in your hand; there is nothing left in it.", false, ch,
      stone, 0, TO_CHAR);
    ch->stopTask();
    return;
  }

  // The band is re-read every attempt, so the work gets harder as the piece
  // gets better and the last stretch of a high-tier piece is the worst of it.
  int mod = getBolsterBandMod(level, ceiling);

  if (!ch->bSuccess(mod, SKILL_BOLSTER)) {
    CF(SKILL_BOLSTER);
    act("The charge goes into $p and nothing comes of it.", false, ch, obj, 0,
      TO_CHAR);
    return;
  }

  CS(SKILL_BOLSTER);

  // One armorLevel per landed attempt, which moves AC and structure together.
  clothing->setDefArmorLevel(static_cast<float>(min(level + 1.0, ceiling)));

  act("$p answers, and holds a little more than it did.", false, ch, obj, 0,
    TO_CHAR);

  if (clothing->armorLevel(ARMOR_LEV_REAL) >= ceiling) {
    act("$p is everything you could make of it now.", false, ch, obj, 0,
      TO_CHAR);
    act("$n finishes work on $p.", true, ch, obj, 0, TO_ROOM);
    ch->stopTask();
  }
}

int task_bolster(TBeing* ch, cmdTypeT cmd, const char*, int pulse, TRoom*,
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

  if (!ch->doesKnowSkill(SKILL_BOLSTER)) {
    ch->sendTo("You've forgotten how to do this.\n\r");
    ch->stopTask();
    return false;
  }

  switch (cmd) {
    case CMD_TASK_CONTINUE:
      ch->task->calcNextUpdate(pulse, Pulse::MOBACT);
      bolster_pulse(ch);
      return false;
    case CMD_ABORT:
    case CMD_STOP:
      act("You set the work aside.", false, ch, 0, 0, TO_CHAR);
      act("$n sets aside what $e was working on.", true, ch, 0, 0, TO_ROOM);
      ch->stopTask();
      break;
    case CMD_TASK_FIGHTING:
      ch->sendTo("You can't keep at this while under attack!\n\r");
      ch->stopTask();
      break;
    default:
      if (cmd < MAX_CMD_LIST)
        warn_busy(ch);
      break;
  }

  return true;
}
