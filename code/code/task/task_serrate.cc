//////////////////////////////////////////////////////////////////////////
//
// SneezyMUD - All rights reserved, SneezyMUD Coding Team
// task_serrate.cc
//
//////////////////////////////////////////////////////////////////////////

#include "comm.h"
#include "extern.h"
#include "room.h"
#include "being.h"
#include "handler.h"
#include "obj_tool.h"
#include "obj_base_weapon.h"
#include "materials.h"

namespace {
  // Filing teeth into an edge means fighting the material the whole way: soft
  // stock cuts easily and merely deforms when you slip, hard stock resists the
  // file and chips when forced.  Both rolls are a raw d100 against hardness,
  // read in opposite directions.
  TTool* heldFile(const TBeing* ch) {
    TTool* file = dynamic_cast<TTool*>(ch->heldInPrimHand());
    return (file && file->getToolType() == TOOL_FILE) ? file : nullptr;
  }

  TTool* roomWorkbench(const TBeing* ch) {
    if (!ch->roomp)
      return nullptr;

    for (StuffIter it = ch->roomp->stuff.begin(); it != ch->roomp->stuff.end();
         ++it) {
      TTool* tool = dynamic_cast<TTool*>(*it);
      if (tool && tool->getToolType() == TOOL_WORKBENCH)
        return tool;
    }
    return nullptr;
  }

  void stopSerrate(TBeing* ch) {
    act("You set down your work.", false, ch, nullptr, nullptr, TO_CHAR);
    act("$n sets down $s work.", true, ch, nullptr, nullptr, TO_ROOM);
    ch->stopTask();
  }

  // Returns DELETE_ITEM if the weapon filed itself apart.
  int serratePulse(TBeing* ch, TBaseWeapon* weapon) {
    const int hardness = material_nums[weapon->getMaterial()].hardness;

    if (::number(1, 100) > hardness) {
      weapon->addToCurSharp(-1);
      act("You draw the file across $p, raising a fresh tooth.", false, ch,
        weapon, nullptr, TO_CHAR);
    } else {
      weapon->addToCurSharp(-2);
      act("The file skips, and you flatten more of $p's edge than you meant.",
        false, ch, weapon, nullptr, TO_CHAR);

      // The same hardness that resisted the file is what chips under it.
      if (::number(1, 100) <= hardness) {
        act("A flake breaks away from $p.", false, ch, weapon, nullptr,
          TO_CHAR);
        if (IS_SET_DELETE(weapon->damageItem(1), DELETE_THIS))
          return DELETE_ITEM;
      }
    }

    if (weapon->getCurSharp() < 0)
      weapon->setCurSharp(0);

    return 0;
  }
}  // namespace

void TBeing::doSerrate(const char* arg) {
  if (!doesKnowSkill(SKILL_SERRATE)) {
    sendTo("You wouldn't know where to begin filing teeth into a blade.\n\r");
    return;
  }

  if (!arg || !*arg) {
    sendTo("Syntax: serrate <weapon>\n\r");
    return;
  }

  TTool* file = heldFile(this);
  if (!file) {
    sendTo("You need a file held in your primary hand for that.\n\r");
    return;
  }

  if (file->getToolUses() <= 0) {
    act("Your $o is completely worn out.", false, this, file, nullptr, TO_CHAR);
    return;
  }

  if (!roomWorkbench(this)) {
    sendTo("You need a workbench to clamp the work to.\n\r");
    return;
  }

  TThing* t = searchLinkedListVis(this, arg, stuff);
  if (!t) {
    sendTo("You aren't carrying that.\n\r");
    return;
  }

  TBaseWeapon* weapon = dynamic_cast<TBaseWeapon*>(t);
  if (!weapon) {
    act("$p has no edge to work.", false, this, t, nullptr, TO_CHAR);
    return;
  }

  if (!weapon->isSlashWeapon() && !weapon->isPierceWeapon()) {
    act("Teeth are for blades and points - $p is neither.", false, this, weapon,
      nullptr, TO_CHAR);
    return;
  }

  if (weapon->isSpiked()) {
    act("$p already carries a serrated edge.", false, this, weapon, nullptr,
      TO_CHAR);
    return;
  }

  if (weapon->getCurSharp() <= 0) {
    act("$p has no edge left to cut teeth into.", false, this, weapon, nullptr,
      TO_CHAR);
    return;
  }

  // One pass per ten points of edge there is to recut, so a finer weapon is a
  // longer job.
  const int passes = max(1, weapon->getMaxSharp() / 10);

  act("You clamp $p to the workbench and set to work with $P.", false, this,
    weapon, file, TO_CHAR);
  act("$n clamps $p to the workbench and sets to work with $P.", true, this,
    weapon, file, TO_ROOM);

  start_task(this, weapon, roomp, TASK_SERRATE, arg, passes, in_room, 0, 0, 0);
}

int task_serrate(TBeing* ch, cmdTypeT cmd, const char*, int pulse, TRoom*,
  TObj*) {
  TThing* t = nullptr;

  // Re-find the weapon by name each pulse rather than trusting a cached
  // pointer - it can be dropped, sold, or destroyed between updates.
  if (ch->isLinkdead() || (ch->in_room != ch->task->wasInRoom) ||
      (ch->getPosition() < POSITION_STANDING) ||
      !(t = searchLinkedListVis(ch, ch->task->orig_arg, ch->stuff))) {
    stopSerrate(ch);
    return false;  // returning false lets the command be interpreted
  }

  TBaseWeapon* weapon = dynamic_cast<TBaseWeapon*>(t);
  if (!weapon) {
    stopSerrate(ch);
    return false;
  }

  if (ch->utilityTaskCommand(cmd) || ch->nobrainerTaskCommand(cmd))
    return false;

  switch (cmd) {
    case CMD_TASK_CONTINUE: {
      TTool* file = heldFile(ch);
      if (!file) {
        ch->sendTo("You've nothing to file with any more.\n\r");
        ch->stopTask();
        return false;
      }

      if (!roomWorkbench(ch)) {
        ch->sendTo("There's no workbench here to steady the work.\n\r");
        ch->stopTask();
        return false;
      }

      ch->task->calcNextUpdate(pulse, 2 * Pulse::MOBACT);

      if (IS_SET_DELETE(serratePulse(ch, weapon), DELETE_ITEM)) {
        delete weapon;
        ch->stopTask();
        return false;
      }

      if (--ch->task->timeLeft <= 0) {
        weapon->addObjStat(ITEM_SPIKED);
        act("You file a last row of teeth into $p - it will bite now.", false,
          ch, weapon, nullptr, TO_CHAR);
        act("$n files a last row of teeth into $p.", true, ch, weapon, nullptr,
          TO_ROOM);
        ch->stopTask();

        // The job costs the file a single use, spent on finishing rather than
        // pass by pass, so an abandoned attempt costs nothing but the edge.
        file->addToToolUses(-1);
        if (file->getToolUses() <= 0) {
          act("Your $o is used up, and you set it aside.", false, ch, file,
            nullptr, TO_CHAR);
          act("$n's $o is worn out.", true, ch, file, nullptr, TO_ROOM);
          delete file;
        }
      }
      return false;
    }
    case CMD_ABORT:
    case CMD_STOP:
      act("You stop filing at $p.", false, ch, weapon, nullptr, TO_CHAR);
      act("$n stops filing at $p.", true, ch, weapon, nullptr, TO_ROOM);
      ch->stopTask();
      break;
    case CMD_TASK_FIGHTING:
      ch->sendTo("You can't keep filing while you're under attack!\n\r");
      ch->stopTask();
      break;
    default:
      if (cmd < MAX_CMD_LIST)
        warn_busy(ch);
      break;  // eat the command
  }
  return true;
}
