#include "handler.h"
#include "extern.h"
#include "being.h"
#include "obj_bag.h"
#include "obj_commodity.h"
#include "materials.h"
#include "low.h"
#include "room.h"
#include "obj_tool.h"
#include "obj_base_container.h"

void stop_tinker_belt(TBeing* ch) {
    act("You stop adding belt loops.", FALSE, ch, 0, 0, TO_CHAR);
    act("$n stops adding belt loops.", TRUE, ch, 0, 0, TO_ROOM);
    ch->stopTask();
}

int task_tinker_belt(TBeing* ch, cmdTypeT cmd, const char* arg, int pulse, TRoom* rp, TObj* obj) {
    TOpenContainer* container = dynamic_cast<TOpenContainer*>(obj);
    int skill = ch->getSkillValue(SKILL_TINKER);
    
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

    // Basic validation
    if (!container) {
        ch->sendTo("The container you were working on seems to have vanished!\n\r");
        ch->stopTask();
        return FALSE;
    }

    // Check if it already has belt loops
    if (container->canWear(ITEM_WEAR_WAIST)) {
        ch->sendTo("This bag already has belt loops.\n\r");
        ch->stopTask();
        return FALSE;
    }

    // Check current wear flags - only allow TAKE and HOLD
    unsigned int value = container->obj_flags.wear_flags;
    REMOVE_BIT(value, ITEM_WEAR_TAKE);
    REMOVE_BIT(value, ITEM_WEAR_HOLD);
    if (value != 0) {
        ch->sendTo("This container already has other wear locations - it can't be modified for belt wear.\n\r");
        ch->stopTask();
        return FALSE;
    }

    if (ch->isLinkdead() || (ch->in_room != ch->task->wasInRoom) ||
        (ch->getPosition() < POSITION_RESTING)) {
        ch->stopTask();
        return FALSE;
    }

    if (ch->utilityTaskCommand(cmd) || ch->nobrainerTaskCommand(cmd))
        return FALSE;

    switch (cmd) {
        case CMD_TASK_CONTINUE:
            if (ch->task->timeLeft > 0) {
                // Check if we have enough materials
                if (!mat || mat->numUnits() < mats_needed) {
                    ch->sendTo(format("You need at least %d units of %s to add belt loops to this container!\n\r") % 
                               mats_needed % material_nums[container->getMaterial()].mat_name);
                    ch->stopTask();
                    return FALSE;
                }

                if (ch->bSuccess(skill, SKILL_TINKER)) {
                    // Success reduces time and consumes materials
                    ch->task->timeLeft -= 2;
                    mat->setWeight(mat->getWeight() - (mats_needed / 10.0));
                    ch->sendTo("You make good progress adding the belt loops.\n\r");
                    act("$n makes good progress adding belt loops to $p.", TRUE, ch, container, 0, TO_ROOM);
                } else {
                    // Failure increases time and consumes half materials
                    ch->task->timeLeft += 1;
                    mat->setWeight(mat->getWeight() - (mats_needed / 20.0));
                    ch->sendTo("You struggle with attaching the belt loops.\n\r");
                    act("$n struggles with attaching belt loops to $p.", TRUE, ch, container, 0, TO_ROOM);
                }

                if (mat->numUnits() <= 0) {
                    ch->sendTo("You've run out of materials!\n\r");
                    delete mat;
                    ch->stopTask();
                    return FALSE;
                }
                return TRUE;
            } else {
                // Task complete - add waist wear flag
                SET_BIT(container->obj_flags.wear_flags, ITEM_WEAR_WAIST);
                
                // Check and modify AC
                bool found = false;
                int currentAC = 0;
                
                // Search for existing AC
                for (int i = 0; i < MAX_OBJ_AFFECT; i++) {
                    if (container->affected[i].location == APPLY_ARMOR) {
                        found = true;
                        currentAC = container->affected[i].modifier;
                        
                        // If AC is better than -120 (lower is better), add -20
                        if (currentAC > -120) {
                            container->affected[i].modifier -= 20;
                        }
                        break;
                    }
                }
                
                // If no AC found, add -50 to first empty slot
                if (!found) {
                    for (int i = 0; i < MAX_OBJ_AFFECT; i++) {
                        if (container->affected[i].location == APPLY_NONE) {
                            container->affected[i].location = APPLY_ARMOR;
                            container->affected[i].modifier = -50;
                            break;
                        }
                    }
                }

                ch->sendTo("You successfully add belt loops to the container!\n\r");
                act("$n successfully adds belt loops to $p.", TRUE, ch, container, 0, TO_ROOM);
                
                container->updateBagDesc();

                // Final exp gain
                ch->gainTaskExp(0, 50.0);
                ch->doSave(SILENT_YES);
                ch->stopTask();
                return FALSE;
            }
            break;
        case CMD_ABORT:
        case CMD_STOP:
            stop_tinker_belt(ch);
            return FALSE;
        default:
            if (cmd < MAX_CMD_LIST)
                warn_busy(ch);
            break;
    }
    return TRUE;
}