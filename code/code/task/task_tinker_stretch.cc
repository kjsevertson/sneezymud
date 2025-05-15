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

double getStretchDifficultyScale(taskDiffT diff) {
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

void stop_tinker_stretch(TBeing* ch) {
    act("You stop stretching.", FALSE, ch, 0, 0, TO_CHAR);
    act("$n stops stretching.", TRUE, ch, 0, 0, TO_ROOM);
    ch->stopTask();
}

int task_tinker_stretch(TBeing* ch, cmdTypeT cmd, const char* arg, int pulse, TRoom* rp, TObj* obj) {
    TOpenContainer* container = dynamic_cast<TOpenContainer*>(obj);
    taskDiffT difficulty;
    int skill = ch->getSkillValue(SKILL_TINKER);
    int bagSpace = container->getCarriedVolume();
    int bagUpsizeAmt = container->getVolume()*0.05;
    int stretchAmount = bagSpace*0.05;
    int initialStruct = container->getStructPoints(); 
    int initialMaxStruct = container->getMaxStructPoints();
    int stoppingStruct = initialStruct*0.2;
    
    // Calculate materials needed
    int mats_needed = (int)(container->getWeight() * 0.1); // 10% of container weight
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
        ch->sendTo(format("You need %d units of %s to stretch this container!\n\r") % 
                   mats_needed % material_nums[container->getMaterial()].mat_name);
        ch->stopTask();
        return FALSE;
    }

    // Basic validation
    if (!container) {
        ch->sendTo("The container you were stretching seems to have vanished!\n\r");
        ch->stopTask();
        return FALSE;
    }

    // Set difficulty based on container's current capacity
    bagSpace = container->getCarriedVolume();
    if (bagSpace > 60000)
        difficulty = TASK_HOPELESS;
    else if (bagSpace > 40000)
        difficulty = TASK_DANGEROUS;
    else if (bagSpace > 20000)
        difficulty = TASK_DIFFICULT;
    else if (bagSpace > 10000)
        difficulty = TASK_NORMAL;
    else if (bagSpace > 5000)
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
                container->addToCarriedVolume(stretchAmount);
                container->setVolume(container->getVolume() + bagUpsizeAmt);
                container->setStructPoints(initialStruct*0.9);
                container->setMaxStructPoints(initialMaxStruct*0.9);
                ch->sendTo("You carefully work on stretching the container.\n\r");
                act("$n carefully works on stretching $p.", TRUE, ch, container, 0, TO_ROOM);

                if (mat->numUnits() <= 0) {
                    ch->sendTo("You've run out of materials!\n\r");
                    delete mat;
                    ch->stopTask();
                    return FALSE;
                }

                if (container->getStructPoints() <= stoppingStruct) {
                    ch->sendTo("You've stretched the container as much as you can for now.\n\r");
                    act("$n has stretched $p as much as it can be stretched for now.", TRUE, ch, container, 0, TO_ROOM);
                    
                    // Add scaled final exp gain
                    double scaleFactor = getStretchDifficultyScale(difficulty);
                    ch->gainTaskExp(0, scaleFactor);
                    ch->doSave(SILENT_YES);
                    ch->stopTask();
                    return TRUE;
                }
                return TRUE;
            } else {
                // Consume half materials on failure
                mat->setWeight(mat->getWeight() - (mats_needed / 20.0));
                container->setStructPoints(initialStruct-(::number(1,5)));
                ch->sendTo("You struggle with stretching the container.\n\r");
                act("$n struggles while stretching $p.", TRUE, ch, container, 0, TO_ROOM);

                if (mat->numUnits() <= 0) {
                    ch->sendTo("You've run out of materials!\n\r");
                    delete mat;
                    ch->stopTask();
                    return FALSE;
                }

                if (container->getStructPoints() <= stoppingStruct) {
                    ch->sendTo("You've nearly ruined the bag and must stop.\n\r");
                    act("$n has nearly ruined $p and must stop.", TRUE, ch, container, 0, TO_ROOM);
                    ch->stopTask();
                    return TRUE;
                }
            }
            break;
        case CMD_ABORT:
        case CMD_STOP:
            stop_tinker_stretch(ch);
            return FALSE;
        default:
            if (cmd < MAX_CMD_LIST)
                warn_busy(ch);
            break;
    }
    return TRUE;
}
