//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD - All rights reserved, SneezyMUD Coding Team
//      "task_transmute.cc" - Changing what a thing is made of
//
//////////////////////////////////////////////////////////////////////////

#include "comm.h"
#include "handler.h"
#include "extern.h"
#include "room.h"
#include "being.h"
#include "augment.h"

// The change lands at the end; a missed pulse costs only time. The roll is
// what carries the whole cost of the work: ten for every family between what
// the piece is and what it is being asked to become, plus the rarity of the
// target, less half the rarity of the starting material when the two are in
// the same family.
//
// The target material rides in task->status.
// Nothing changes until the end. Every pulse is one more mark in a tally, and
// what decides the outcome is the share of them that landed -- so a working
// that goes badly early can still be pulled round, and one that goes well can
// still fail.
//
// The opal was spent before the first pulse and is not coming back. What it
// bought rides in orig_arg as a flat modifier, since the stone itself is gone
// and the discount cannot be recomputed from it. The target material is in
// status, the tallies in flags.
static void transmute_pulse(TBeing* ch) {
  TObj* obj = ch->task->obj;
  if (!obj) {
    ch->sendTo("There is nothing left in front of you.\n\r");
    ch->stopTask();
    return;
  }

  if (augmentDrain(ch, SKILL_TRANSMUTE, false)) {
    ch->stopTask();
    return;
  }

  int mod = ch->task->orig_arg ? convertTo<int>(ch->task->orig_arg) : 0;
  int hits = smeltHits(ch->task->flags);
  int misses = smeltMisses(ch->task->flags);

  if (!ch->bSuccess(mod, SKILL_TRANSMUTE)) {
    CF(SKILL_TRANSMUTE);
    ch->task->flags = smeltTally(hits, misses + 1);

    if (!::number(0, 2))
      act("$p resists you, and stays what it was.", false, ch, obj, 0, TO_CHAR);

    return;
  }

  CS(SKILL_TRANSMUTE);
  ch->task->flags = smeltTally(hits + 1, misses);
  ch->task->timeLeft -= getAugmentTickAmount(ch, SKILL_TRANSMUTE);

  if (ch->task->timeLeft > 0) {
    if (!::number(0, 2))
      act("Something in $p gives a little further.", false, ch, obj, 0,
        TO_CHAR);
    return;
  }

  unsigned short material = static_cast<unsigned short>(ch->task->status);
  int finalHits = smeltHits(ch->task->flags);
  int finalMisses = smeltMisses(ch->task->flags);
  ch->stopTask();
  transmuteFinish(ch, obj, material, finalHits, finalMisses);
}

int task_transmute(TBeing* ch, cmdTypeT cmd, const char*, int pulse, TRoom*,
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

  if (!ch->doesKnowSkill(SKILL_TRANSMUTE)) {
    ch->sendTo("You've forgotten how to do this.\n\r");
    ch->stopTask();
    return false;
  }

  switch (cmd) {
    case CMD_TASK_CONTINUE:
      ch->task->calcNextUpdate(pulse, Pulse::MOBACT);
      transmute_pulse(ch);
      return false;
    case CMD_ABORT:
    case CMD_STOP:
      act("You let go, and it settles back into what it was.", false, ch, 0, 0,
        TO_CHAR);
      ch->stopTask();
      break;
    case CMD_TASK_FIGHTING:
      ch->sendTo("You can't hold this together while under attack!\n\r");
      ch->stopTask();
      break;
    default:
      if (cmd < MAX_CMD_LIST)
        warn_busy(ch);
      break;
  }

  return true;
}
