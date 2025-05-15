//////////////////////////////////////////////////////////////////////////
//
// SneezyMUD - All rights reserved, SneezyMUD Coding Team
//
//////////////////////////////////////////////////////////////////////////

#include "handler.h"    // for act() and other utility functions
#include "room.h"      // for TRoom
#include "being.h"     // for TBeing
#include "extern.h"    // for external declarations
#include "obj_base_weapon.h"  // for TBaseWeapon
#include "obj_tool.h"  // for TTool
#include "obj_general_weapon.h"
#include "skills.h"    // for SKILL_HARVEST_REAGENTS
#include "combat.h"    // for damage functions
#include "spelltask.h" // for spell-related constants and functions
#include "disc_thief_poisons.h"
#include "obj_base_corpse.h"

// task-specific constants
const int MAX_HARVESTS = 5;  // maximum number of successful harvests per task


void stop_harvest(TBeing* ch) {
  if (ch->getPosition() >= POSITION_RESTING) {
    act("You stop harvesting reagents and look about.", FALSE, ch, 0, 0, TO_CHAR);
    act("$n stops harvesting reagents and looks about.", FALSE, ch, 0, 0, TO_ROOM);
  }
  ch->stopTask();
}


int harvestReagentPulse(TBeing* ch) {
  int learning = ch->getSkillValue(SKILL_HARVEST_REAGENTS);
  TGenWeapon* weapon = dynamic_cast<TGenWeapon*>(ch->heldInPrimHand());
  TTool* tool = dynamic_cast<TTool*>(ch->heldInSecHand());

  // Immortal instant success
  if (ch->isImmortal()) {
    ch->sendTo("You instantly harvest reagents in a god-like manner.\n\r");
    act("$n becomes a blur and instantly harvests reagents.", FALSE, ch, 0, 0, TO_ROOM);
    ch->task->flags = MAX_HARVESTS;  // Maximum harvests
    return TRUE;
  }

  // basic tasky safechecking
  if (ch->isLinkdead() || (ch->in_room != ch->task->wasInRoom) || learning == 0) {
    act("You cease your reagent harvesting activities.", FALSE, ch, 0, 0, TO_CHAR);
    act("$n stops $s reagent harvesting activities.", TRUE, ch, 0, 0, TO_ROOM);
    stop_harvest(ch);
    return FALSE;
  }

  // Check for required tools
  if (!weapon || !weapon->isSlashWeapon() || !weapon->isPierceWeapon()) {
    ch->sendTo("You need to hold a bladed weapon in your primary hand to harvest reagents!\n\r");
    stop_harvest(ch);
    return FALSE;
  }

  // Continue harvesting
  if (ch->task->timeLeft > 0 && ch->task->flags < MAX_HARVESTS) {
    // Skill-based fatigue
    ch->addToMove(::number(-1, -10 + learning / 10));
    if (ch->getMove() < 5) {
      act("You are much too tired to continue harvesting.", FALSE, ch, 0, 0, TO_CHAR);
      act("$n stops harvesting, looking exhausted.", TRUE, ch, 0, 0, TO_ROOM);
      stop_harvest(ch);
      return TRUE;
    }

    act("You carefully continue harvesting reagents.", FALSE, ch, 0, 0, TO_CHAR);
    act("$n carefully harvests reagents from the surrounding area.", FALSE, ch, 0, 0, TO_ROOM);
    ch->gainTaskExp(0, 50.0);
    ch->doSave(SILENT_YES);

    // Progress based on skill level
    if (!::number(0, 200 / learning))
      ch->task->timeLeft--;

    if (ch->bSuccess(SKILL_HARVEST_REAGENTS)) {
      
      CS(SKILL_HARVEST_REAGENTS);
      ch->task->flags++;
      
      // Create and give harvested reagent
      int reagentVnum = get_toxic_plant_vnum(ch->roomp->getSectorType());
      if (reagentVnum > 0) {
        TObj* reagent = read_object(reagentVnum, VIRTUAL);
        if (reagent) {
          *ch += *reagent;
          act("You carefully harvest $p!", FALSE, ch, reagent, 0, TO_CHAR);
          act("$n carefully harvests $p.", FALSE, ch, reagent, 0, TO_ROOM);
        }
      }
      
      // Dull weapon periodically (similar to butchering)
      if (weapon->getCurSharp() > 2 && (ch->task->flags % 10) == 0) {
        weapon->addToCurSharp(-1);
        act("Your $o looks a bit duller after the harvest.", FALSE, ch, weapon, 0, TO_CHAR);        
        act("$n's $o looks a bit duller after the harvest.", FALSE, ch, weapon, 0, TO_ROOM);
      }

      // Handle tool usage (similar to sharpening/dulling tasks)
      if (tool) {
        tool->addToToolUses(-1);
        if (tool->getToolUses() <= 0) {
          act("Your $o breaks due to overuse.", FALSE, ch, tool, 0, TO_CHAR);
          act("$n looks startled as $e breaks $P while harvesting.", FALSE, ch, 0, tool, TO_ROOM);
          ch->stopTask();
          delete tool;
          return DELETE_THIS;
        }
      }
      
      // Handle dangerous reagents
      if (!ch->isImmune(IMMUNE_POISON, WEAR_BODY)) {
        ch->sendTo("You nick yourself on a poisonous plant!\n\r");
        act("$n winces in pain while handling the reagents.", FALSE, ch, 0, 0, TO_ROOM);
        
        // Apply poison effect
        affectedData aff;
        aff.type = SPELL_POISON;
        aff.level = 30;
        aff.duration = 3 * Pulse::UPDATES_PER_MUDHOUR;
        aff.modifier = -25;
        aff.location = APPLY_STR;
        aff.bitvector = AFF_POISON;
        ch->affectTo(&aff);
        
        ch->reconcileDamage(ch, 5, SKILL_HARVEST_REAGENTS);
        if (ch->isDead())
          return DELETE_THIS;
      }

      // Grant experience based on success
      ch->gainTaskExp(0, 15.0);
      ch->doSave(SILENT_YES);
    } else {
      // Handle failure
      CF(SKILL_HARVEST_REAGENTS);
      
      // Critical failure check
      if (!ch->bSuccess(learning, SKILL_HARVEST_REAGENTS) && 
          !critFail(ch, SKILL_HARVEST_REAGENTS)) {
        
        // Severe weapon damage
        if (weapon->getCurSharp() > 4) {
          weapon->addToCurSharp(-3);
          act("You badly damage your $o on a tough plant!", FALSE, ch, weapon, 0, TO_CHAR);
          act("$n's $o looks significantly duller after a mishap.", FALSE, ch, weapon, 0, TO_ROOM);
        }
        
        // Chance of tool damage
        if (tool && !::number(0, 2)) {
          tool->addToToolUses(-2);
          if (tool->getToolUses() <= 0) {
            act("Your $o breaks from misuse!", FALSE, ch, tool, 0, TO_CHAR);
            act("$n breaks $P while harvesting.", FALSE, ch, 0, tool, TO_ROOM);
            ch->stopTask();
            delete tool;
            return DELETE_THIS;
          }
        }
        
        // Chance of injury from toxic plants
        if (!ch->isImmune(IMMUNE_POISON, WEAR_BODY) && !::number(0, 2)) {
          act("You badly cut yourself on a poisonous plant!", FALSE, ch, 0, 0, TO_CHAR);
          act("$n cuts $mself badly while harvesting!", FALSE, ch, 0, 0, TO_ROOM);
          
          // More severe poison effect
          affectedData aff;
          aff.type = SPELL_POISON;
          aff.level = 40;
          aff.duration = 5 * Pulse::UPDATES_PER_MUDHOUR;
          aff.modifier = -35;
          aff.location = APPLY_STR;
          aff.bitvector = AFF_POISON;
          ch->affectTo(&aff);
          
          ch->reconcileDamage(ch, 15, SKILL_HARVEST_REAGENTS);
          if (ch->isDead())
            return DELETE_THIS;
        }
      } else {
        // Regular failure - just dull weapon
        if (weapon->getCurSharp() && !::number(0, 10)) {
          if (weapon->getCurSharp() > 4) {
            weapon->addToCurSharp(-3);
            act("You accidentally strike something hard, dulling your $o!", FALSE, ch, weapon, 0, TO_CHAR);
            act("$n's $o looks a bit duller after a mishap.", FALSE, ch, weapon, 0, TO_ROOM);
          }
        }
      }
    }
  }
  return FALSE;
}

int task_harvestReagents(TBeing* ch, cmdTypeT cmd, const char* arg, int pulse, TRoom* room, TObj* obj) {
  TGenWeapon* weapon;
  TTool* tool;
  int rc;

  // Basic sanity checks
  if (ch->isLinkdead() || (ch->in_room < 0) ||
      (ch->getPosition() < POSITION_STANDING)) {
    stop_harvest(ch);
    return FALSE;
  }

  if (ch->utilityTaskCommand(cmd) || ch->nobrainerTaskCommand(cmd))
    return FALSE;

  // Tool validation
  weapon = dynamic_cast<TGenWeapon*>(ch->heldInPrimHand());
  tool = dynamic_cast<TTool*>(ch->heldInSecHand());

  if (!weapon || !weapon->isSlashWeapon() || !weapon->isPierceWeapon()) {
    ch->sendTo("You need to hold a bladed weapon in your primary hand to harvest reagents!\n\r");
    stop_harvest(ch);
    return FALSE;
  }

  if (!tool || (tool->getToolType() != TOOL_FORCEPS && 
                tool->getToolType() != TOOL_TONGS)) {
    ch->sendTo("You need forceps or tongs in your off hand to safely handle the reagents.\n\r");
    stop_harvest(ch);
    return FALSE;
  }

  if (weapon->getCurSharp() < 50) {
    ch->sendTo("Your weapon is too dull to harvest reagents.\n\r");
    stop_harvest(ch);
    return FALSE;
  }

  switch (cmd) {
    case CMD_TASK_CONTINUE:
      // Control pulse timing like other gathering tasks
      ch->task->calcNextUpdate(pulse, Pulse::MOBACT);
      rc = harvestReagentPulse(ch);
      return rc;

    case CMD_ABORT:
    case CMD_STOP:
      act("You carefully stop your harvesting activities.", FALSE, ch, 0, 0, TO_CHAR);
      act("$n carefully stops $s harvesting activities.", TRUE, ch, 0, 0, TO_ROOM);
      stop_harvest(ch);
      break;

    case CMD_TASK_FIGHTING:
      ch->sendTo("You are unable to continue harvesting while under attack!\n\r");
      act("$n is interrupted from harvesting by combat!", TRUE, ch, 0, 0, TO_ROOM);
      stop_harvest(ch);
      break;

    default:
      if (cmd < MAX_CMD_LIST)
        warn_busy(ch);
      break;  // eat the command
  }
  
  return TRUE;
}
