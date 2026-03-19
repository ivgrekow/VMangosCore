#include "WorldBotAI.h"

void WorldBotAI::UpdateAI(uint32 diff){
    m_updateTimer.Update(diff);
    if (!m_updateTimer.Passed())
        return;
        
    m_updateTimer.Reset(WB_UPDATE_INTERVAL);
    
    if (!me->IsInWorld() || me->IsBeingTeleported())
        return;

    
}