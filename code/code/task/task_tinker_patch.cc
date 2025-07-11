//////////////////////////////////////////////////////////////////////////
//
// SneezyMUD - All rights reserved, SneezyMUD Coding Team
//
//////////////////////////////////////////////////////////////////////////

#include "extern.h"
#include "skills.h"
#include "being.h"
#include "obj_base_container.h"
#include "obj_open_container.h"
#include "obj_commodity.h"
#include "handler.h"
#include "room.h"
#include "materials.h"
#include "obj_tool.h"
#include "spells.h"


void stop_tinker_patch(TBeing* ch) {
  act("You stop patching.", FALSE, ch, 0, 0, TO_CHAR);
  act("$n stops patching.", TRUE, ch, 0, 0, TO_ROOM);
  ch->stopTask();
}

int task_tinker_patch(TBeing* ch, cmdTypeT cmd, const char* arg, int pulse, TRoom*, TObj* obj) {
  TOpenContainer* container = dynamic_cast<TOpenContainer*>(obj);
  int mats_needed = 0;
  int mat_vnum = 0;
  TCommodity* mat = NULL;
  int skill = 0;
  float repair_quality = 0;
  int repair_amount = 0;

  // Basic validation
  if (!container) {
    ch->sendTo("The object you were patching seems to have vanished!\n\r");
    stop_tinker_patch(ch);
    return FALSE;
  }

  // Basic task checks
  if (ch->isLinkdead() || (ch->in_room != ch->task->wasInRoom) ||
      (ch->getPosition() < POSITION_RESTING)) {
    stop_tinker_patch(ch);
    return FALSE;
  }

  if (ch->utilityTaskCommand(cmd) || ch->nobrainerTaskCommand(cmd))
    return FALSE;

  switch (cmd) {
    case CMD_TASK_CONTINUE:
      // Parse stored task args
      sscanf(ch->task->orig_arg, "%d %d", &mats_needed, &mat_vnum);

      // Find material in inventory
      for (StuffIter it = ch->stuff.begin(); it != ch->stuff.end(); ++it) {
        TCommodity* comm = dynamic_cast<TCommodity*>(*it);
        if (comm && comm->objVnum() == mat_vnum) {
          mat = comm;
          break;
        }
      }

      if (!mat || mat->numUnits() < mats_needed) {
        ch->sendTo("You've run out of materials!\n\r");
        stop_tinker_patch(ch);
        return FALSE;
      }

      // Do repair check
      skill = ch->getSkillValue(SKILL_TINKER);

      if (ch->bSuccess(skill, SKILL_TINKER)) {
        // Consume materials
        mat->setWeight(mat->getWeight() - (mats_needed / 10.0));
        if (mat->numUnits() <= 0)
          delete mat;

        // Calculate repair amount based on skill
        repair_quality = 0.5 + ((float)skill / 200.0); // 50-100% effectiveness
        repair_amount = (int)((container->getMaxStructPoints() - container->getStructPoints()) * repair_quality);

        container->addToStructPoints(repair_amount);

        ch->sendTo("You successfully patch the container.\n\r");
        act("$n successfully patches $p.", TRUE, ch, container, 0, TO_ROOM);
        stop_tinker_patch(ch);
        return TRUE;
      } else {
        // Consume half materials
        mat->setWeight(mat->getWeight() - (mats_needed / 20.0));
        if (mat->numUnits() <= 0)
          delete mat;

        ch->sendTo("You fumble with the patching process and damage the container further!\n\r");
        act("$n fumbles while patching $p, making it worse.", TRUE, ch, container, 0, TO_ROOM);

        // Damage the container
        int damage = ::number(1, 3);  // Remove 1-3 structure points on failure
        container->addToStructPoints(-damage);

        // Check if we destroyed it
        if (container->getStructPoints() <= 0) {
          ch->sendTo("You've completely ruined it!\n\r");
          act("$n completely ruins $p!", TRUE, ch, container, 0, TO_ROOM);
          if (!container->makeScraps()) {
            delete container;
            return DELETE_THIS;
          }
        }

        // Chance to prick yourself
        if (::number(1, 100) < 25) {  // 25% chance of injury
          ch->sendTo("You prick yourself with the needle!\n\r");
          int dam = ::number(1, 5);
          
          if (ch->reconcileDamage(ch, dam, DAMAGE_TRAP_PIERCE) == -1)
            return DELETE_THIS;
        }

        stop_tinker_patch(ch);
        return FALSE;
      }

    case CMD_ABORT:
    case CMD_STOP:
      stop_tinker_patch(ch);
      return FALSE;

    default:
      if (cmd < MAX_CMD_LIST)
        warn_busy(ch);
      break;
  }
  return TRUE;
}
