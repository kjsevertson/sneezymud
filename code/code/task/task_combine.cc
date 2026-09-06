//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD - All rights reserved, SneezyMUD Coding Team
//      "task_combine.cc" - Folding one ingot into another, under Forge
//
//////////////////////////////////////////////////////////////////////////

#include "comm.h"
#include "handler.h"
#include "extern.h"
#include "room.h"
#include "being.h"
#include "augment.h"
#include "obj_ingot.h"

// The donor bar is the work: the task holds it, and its structure is the
// clock, so folding in a big bar of a hard metal is a long job. The target is
// looked up by name each pulse -- it is only ever in inventory, and a bar that
// leaves ends the work rather than corrupting it.
static TIngot* combineTarget(TBeing* ch) {
  if (!ch->task || !ch->task->orig_arg)
    return nullptr;

  return dynamic_cast<TIngot*>(
    searchLinkedListVis(ch, ch->task->orig_arg, ch->stuff));
}

static void combine_pulse(TBeing* ch) {
  TObj* donor = ch->task->obj;
  TIngot* target = combineTarget(ch);

  if (!donor || !target) {
    ch->sendTo("You no longer have both bars to hand.\n\r");
    ch->stopTask();
    return;
  }

  int hits = smeltHits(ch->task->flags);
  int misses = smeltMisses(ch->task->flags);

  // The metal's own difficulty against the smith's depth in the advanced
  // discipline. Common stock reads easier than baseline; legendary metal
  // fights every pulse.
  // Physical work costs movement, charged before the roll so a spent
  // worker stops rather than landing one more pulse on fumes.
  if (augmentDrain(ch, SKILL_FORGE, true)) {
    ch->stopTask();
    return;
  }

  if (!ch->bSuccess(getForgeRollMod(ch, donor->getMaterial()), SKILL_FORGE)) {
    CF(SKILL_FORGE);
    ch->task->flags = smeltTally(hits, misses + 1);

    if (!::number(0, 2))
      act("The two bars refuse each other, and you lose the heat.", false, ch,
        donor, 0, TO_CHAR);

    return;
  }

  CS(SKILL_FORGE);
  ch->task->flags = smeltTally(hits + 1, misses);
  ch->task->timeLeft -= getAugmentTickAmount(ch, SKILL_FORGE);

  if (ch->task->timeLeft > 0) {
    if (!::number(0, 2))
      act("You fold another course of $p into the other bar.", false, ch, donor,
        0, TO_CHAR);
    return;
  }

  // stopTask() first: combineFinish() destroys the donor the task is holding.
  int finalHits = smeltHits(ch->task->flags);
  int finalMisses = smeltMisses(ch->task->flags);
  ch->stopTask();
  combineFinish(ch, target, donor, finalHits, finalMisses);
}

int task_combine(TBeing* ch, cmdTypeT cmd, const char*, int pulse, TRoom*,
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

  if (!ch->doesKnowSkill(SKILL_FORGE)) {
    ch->sendTo("You've forgotten how to do this.\n\r");
    ch->stopTask();
    return false;
  }

  switch (cmd) {
    case CMD_TASK_CONTINUE:
      ch->task->calcNextUpdate(pulse, Pulse::MOBACT);
      combine_pulse(ch);
      return false;
    case CMD_ABORT:
    case CMD_STOP:
      act("You draw both bars out of the fire, still two.", false, ch, 0, 0,
        TO_CHAR);
      act("$n draws two bars out of the fire.", true, ch, 0, 0, TO_ROOM);
      ch->stopTask();
      break;
    case CMD_TASK_FIGHTING:
      ch->sendTo("You can't work a forge while under attack!\n\r");
      ch->stopTask();
      break;
    default:
      if (cmd < MAX_CMD_LIST)
        warn_busy(ch);
      break;
  }

  return true;
}
