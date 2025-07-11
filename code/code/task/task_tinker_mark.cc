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

void stop_tinker_mark(TBeing* ch) {
    act("You stop monogramming.", FALSE, ch, 0, 0, TO_CHAR);
    act("$n stops monogramming.", TRUE, ch, 0, 0, TO_ROOM);
    ch->stopTask();
}

int task_tinker_mark(TBeing* ch, cmdTypeT cmd, const char* arg, int pulse, TRoom* rp, TObj* obj) {
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
                    ch->sendTo("You make good progress on the monogram.\n\r");
                    act("$n makes good progress monogramming $p.", TRUE, ch, container, 0, TO_ROOM);
                } else {
                    ch->task->timeLeft += 1;
                    ch->sendTo("You struggle with the delicate monogramming work.\n\r");
                    act("$n struggles with monogramming $p.", TRUE, ch, container, 0, TO_ROOM);
                }
                return TRUE;
            } else {
                // Task complete - mark the container
                container->action_description = format("This is the personalized object of %s.") % ch->getName();
                
                // Add a slight boost to container's properties
                container->setMaxStructPoints(container->getMaxStructPoints() * 1.1);
                container->setStructPoints(container->getStructPoints() * 1.1);

                ch->sendTo(format("You successfully monogram %s with your mark!\n\r") % 
                          container->getName());
                act("$n successfully monograms $p with a personal mark.", TRUE, ch, container, 0, TO_ROOM);
                
                // Final exp gain
                ch->gainTaskExp(0, 25.0);
                ch->doSave(SILENT_YES);
                ch->stopTask();
                return FALSE;
            }
            break;
        case CMD_ABORT:
        case CMD_STOP:
            stop_tinker_mark(ch);
            return FALSE;
        default:
            if (cmd < MAX_CMD_LIST)
                warn_busy(ch);
            break;
    }
    return TRUE;
}
