#include "WorldBotAI.h"

void WorldBotAI::UpdateAI(uint32 diff){
    m_updateTimer.Update(diff);
    if (m_updateTimer.Passed())
        m_updateTimer.Reset(WB_UPDATE_INTERVAL);
    else
        return;

    
}