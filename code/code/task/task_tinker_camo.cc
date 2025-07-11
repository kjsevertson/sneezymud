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

void stop_tinker_camo(TBeing* ch) {
    act("You stop applying camouflage.", FALSE, ch, 0, 0, TO_CHAR);
    act("$n stops applying camouflage.", TRUE, ch, 0, 0, TO_ROOM);
    ch->stopTask();
}

int task_tinker_camo(TBeing* ch, cmdTypeT cmd, const char* arg, int pulse, TRoom* rp, TObj* obj) {
    TOpenContainer* container = dynamic_cast<TOpenContainer*>(obj);
    int skill = ch->getSkillValue(SKILL_TINKER);

    // Basic validation
    if (!container) {
        ch->sendTo("The container you were working on seems to have vanished!\n\r");
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
                if (ch->bSuccess(skill, SKILL_TINKER)) {
                    ch->task->timeLeft -= 2;
                    ch->sendTo("You make good progress applying the camouflage.\n\r");
                    act("$n makes good progress camouflaging $p.", TRUE, ch, container, 0, TO_ROOM);
                } else {
                    ch->task->timeLeft += 1;
                    ch->sendTo("You struggle with applying the camouflage evenly.\n\r");
                    act("$n struggles with camouflaging $p.", TRUE, ch, container, 0, TO_ROOM);
                }
                return TRUE;
            } else {
                // Calculate visibility effect based on skill
                int visibility = std::min(6, (skill / 20)); // 120 skill = 6 max
                
                // Try to find and modify an existing CAN_BE_SEEN affect
                int i;
                for (i = 0; i < MAX_OBJ_AFFECT; i++) {
                    if (container->affected[i].location == APPLY_CAN_BE_SEEN) {
                        // Increase the existing modifier (negative values make objects harder to see)
                        container->affected[i].modifier -= visibility;
                        break;
                    }
                }
                
                // If no existing affect found, add a new one in the first empty slot
                if (i == MAX_OBJ_AFFECT) {
                    for (i = 0; i < MAX_OBJ_AFFECT; i++) {
                        if (container->affected[i].location == APPLY_NONE) {
                            container->affected[i].location = APPLY_CAN_BE_SEEN;
                            container->affected[i].modifier = -visibility; // Negative makes it harder to see
                            break;
                        }
                    }
                }
                
                // Add the SHADOWY flag to make it visually appear shadowy
                container->addObjStat(ITEM_SHADOWY);

                ch->sendTo("You successfully camouflage the container!\n\r");
                act("$n successfully camouflages $p.", TRUE, ch, container, 0, TO_ROOM);
                
                ch->gainTaskExp(0, 50.0);
                ch->doSave(SILENT_YES);
                ch->stopTask();
                return FALSE;
            }
            break;
        case CMD_ABORT:
        case CMD_STOP:
            stop_tinker_camo(ch);
            return FALSE;
        default:
            if (cmd < MAX_CMD_LIST)
                warn_busy(ch);
            break;
    }
    return TRUE;
}
