//////////////////////////////////////////////////////////////////////////
//
// SneezyMUD - All rights reserved, SneezyMUD Coding Team
//
//////////////////////////////////////////////////////////////////////////

#include "handler.h"     // for act() and other handler functions
#include "extern.h"      // for external declarations
#include "being.h"       // for TBeing class
#include "obj_bag.h"     // for container-related functions
#include "obj_commodity.h" // for TCommodity class
#include "materials.h"   // for material_nums array
#include "low.h"         // for utility functions like number()
#include "room.h"        // for TRoom
#include "obj_tool.h"    // for TTool
#include "obj_base_container.h"

double getReinforceDifficultyScale(taskDiffT diff) {
    switch(diff) {
        case TASK_HOPELESS:    return 20.0;   // Most exp - hardest task
        case TASK_DANGEROUS:   return 35.0;
        case TASK_DIFFICULT:   return 50.0;
        case TASK_NORMAL:      return 65.0;
        case TASK_EASY:        return 75.0;
        case TASK_TRIVIAL:     return 80.0;   // Least exp - easiest task
        default:               return 65.0;    // Normal as default
    }
}

void stop_tinker_reinforce(TBeing* ch) {
    act("You stop reinforcing.", FALSE, ch, 0, 0, TO_CHAR);
    act("$n stops reinforcing.", TRUE, ch, 0, 0, TO_ROOM);
    ch->stopTask();
}

int task_tinker_reinforce(TBeing* ch, cmdTypeT cmd, const char* arg, int pulse, TRoom* rp, TObj* obj) {
    TOpenContainer* container = dynamic_cast<TOpenContainer*>(obj);
    taskDiffT difficulty;
    int skill = ch->getSkillValue(SKILL_TINKER);
    double skillMod = skill / 100.0;
    int bagWeight = container->getWeight();
    int bagUpsizeAmt = bagWeight*0.05; // Keep this - we'll use it for incremental increases
    int initialMaxStruct = container->getMaxStructPoints();
    int mats_needed = (int)(container->getWeight() * 0.1); // 10% of container weight
    
    // Calculate reinforcement bonuses based on skill
    double baseValue = max(0.0, min(50.0, (skillMod * 25) + ::number(-5, 5)));
    int structBonus = (int)((baseValue * 3.0 / 2.0) + 5);
    int maxStructBonus = structBonus + ::number(5, 10);
    
   
    // Calculate carry weight increase - 20-50% of current max based on skill
    float currentMaxCarry = container->carryWeightLimit();
    float carryIncrease = currentMaxCarry * (0.2 + (skillMod * 0.3));

    // Calculate AC bonus - negative because lower is better
    int acBonus = -(int)(structBonus * 0.5);  // AC bonus is half of struct bonus, made negative

    // Basic validation
    if (!container) {
        ch->sendTo("The container you were reinforcing seems to have vanished!\n\r");
        ch->stopTask();
        return FALSE;
    }

    // Adjust materials if monogrammed
    if (container->isMonogrammed()) {
        mats_needed = mats_needed / 4;
    }

    // Find material in inventory
    TCommodity* mat = NULL;
    for (StuffIter it = ch->stuff.begin(); it != ch->stuff.end(); ++it) {
        TCommodity* comm = dynamic_cast<TCommodity*>(*it);
        if (comm && comm->getMaterial() == container->getMaterial()) {
            mat = comm;
            break;
        }
    }

    // Check if we have enough materials
    if (!mat || mat->numUnits() < mats_needed) {
        ch->sendTo(format("You need %d units of %s to reinforce this container!\n\r") % 
                   mats_needed % material_nums[container->getMaterial()].mat_name);
        ch->stopTask();
        return FALSE;
    }

    // Set difficulty based on container's current weight
    int currentWeight = container->getWeight();
    if (currentWeight > 600)
        difficulty = TASK_HOPELESS;
    else if (currentWeight > 400)
        difficulty = TASK_DANGEROUS;
    else if (currentWeight > 200)
        difficulty = TASK_DIFFICULT;
    else if (currentWeight > 100)
        difficulty = TASK_NORMAL;
    else if (currentWeight > 50)
        difficulty = TASK_EASY;
    else
        difficulty = TASK_TRIVIAL;

    // Add difficulty modifier calculation
    double difficultyMod;
    switch(difficulty) {
        case TASK_HOPELESS:   difficultyMod = 0.50; break;  // 50
        case TASK_DANGEROUS:  difficultyMod = 0.70; break;  // 70
        case TASK_DIFFICULT:  difficultyMod = 0.80; break;  // 80
        case TASK_NORMAL:     difficultyMod = 0.90; break;  // 90
        case TASK_EASY:       difficultyMod = 1.00; break;  // 100
        case TASK_TRIVIAL:    difficultyMod = 1.10; break;  // 110
        default:              difficultyMod = 0.50; break;
    }

    int adjustedSkill = (int)(skill * difficultyMod);

    if (ch->isLinkdead() || (ch->in_room != ch->task->wasInRoom) ||
        (ch->getPosition() < POSITION_RESTING)) {
        ch->stopTask();
        return FALSE;
    }

    if (ch->utilityTaskCommand(cmd) || ch->nobrainerTaskCommand(cmd))
        return FALSE;

    switch (cmd) {
        case CMD_TASK_CONTINUE:
            if (ch->bSuccess(adjustedSkill, SKILL_TINKER)) {
                // Consume materials on success
                mat->setWeight(mat->getWeight() - (mats_needed / 10.0));
                
                // Apply incremental weight increase with each successful pulse
                container->setWeight(container->getWeight() + bagUpsizeAmt);
                
                // Apply all reinforcement effects
                container->setMaxStructPoints(initialMaxStruct + maxStructBonus);
                
                // Apply AC bonus using APPLY_ARMOR
                bool found = false;
                for (int i = 0; i < MAX_OBJ_AFFECT; i++) {
                    if (container->affected[i].location == APPLY_ARMOR) {
                        // AC is negative for better protection
                        container->affected[i].modifier += acBonus;
                        found = true;
                        break;
                    }
                }
                
                // If no APPLY_ARMOR found, add it to first empty slot
                if (!found) {
                    for (int i = 0; i < MAX_OBJ_AFFECT; i++) {
                        if (container->affected[i].location == APPLY_NONE) {
                            container->affected[i].location = APPLY_ARMOR;
                            container->affected[i].modifier = acBonus;
                            break;
                        }
                    }
                }
                
                container->setCarryWeightLimit(currentMaxCarry + (int)carryIncrease);
                
                ch->sendTo("You carefully reinforce the container, making it more durable.\n\r");
                ch->sendTo("The reinforcement adds some weight but increases protection and carrying capacity.\n\r");
                act("$n carefully reinforces $p.", TRUE, ch, container, 0, TO_ROOM);

                if (mat->numUnits() <= 0) {
                    ch->sendTo("You've run out of materials!\n\r");
                    delete mat;
                    ch->stopTask();
                    return FALSE;
                }

                container->updateBagDesc();

                // Add scaled final exp gain
                double scaleFactor = getReinforceDifficultyScale(difficulty);
                ch->gainTaskExp(0, scaleFactor);
                ch->doSave(SILENT_YES);
                ch->stopTask();
                return FALSE;
            } else {
                // Consume half materials on failure
                mat->setWeight(mat->getWeight() - (mats_needed / 20.0));
                ch->sendTo("You struggle with reinforcing the container.\n\r");
                act("$n struggles while reinforcing $p.", TRUE, ch, container, 0, TO_ROOM);

                if (mat->numUnits() <= 0) {
                    ch->sendTo("You've run out of materials!\n\r");
                    delete mat;
                    ch->stopTask();
                    return FALSE;
                }
                return TRUE;
            }
            break;
        case CMD_ABORT:
        case CMD_STOP:
            stop_tinker_reinforce(ch);
            return FALSE;
        default:
            if (cmd < MAX_CMD_LIST)
                warn_busy(ch);
            break;
    }
    return TRUE;
}
