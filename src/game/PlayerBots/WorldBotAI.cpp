#include "WorldBotAI.h"

void WorldBotAI::UpdateAI(uint32 const diff){
    m_updateTimer.Update(diff);
    if (!m_updateTimer.Passed())
        return;
        
    m_updateTimer.Reset(WB_UPDATE_INTERVAL);
    
    if (!me->IsInWorld() || me->IsBeingTeleported())
        return;

    
}

void WorldBotAI::OnPacketRecieved(WorldPacket const* packet){
    switch (packet->GetOpcode()){
        case SMSG_NEW_WORLD:
        {
            if (!me)
                return;

            std::unique_ptr<WorldPacket> data = std::make_unique<WorldPacket>(MSG_MOVE_WORLDPORT_ACK);
            me->GetSession()->QueuePacket(std::move(data));
            break;
        }
        case MSG_MOVE_TELEPORT_ACK:
        {
            if (!me)
                return;

            std::unique_ptr<WorldPacket> data = std::make_unique<WorldPacket>(MSG_MOVE_TELEPORT_ACK);
            *data << me->GetObjectGuid();
#if SUPPORTED_CLIENT_BUILD > CLIENT_BUILD_1_9_4
            *data << me->GetLastCounterForMovementChangeType(TELEPORT);
#endif
            *data << uint32(time(nullptr));
            me->GetSession()->QueuePacket(std::move(data));
            break;
        }
    }
}

bool WorldBotAI::OnSessionLoaded(PlayerBotEntry* entry, WorldSession* sess)
{
    if (!m_race && !m_class)
    {
        sess->LoginPlayer(entry->playerGUID);
        return true;
    }

    return SpawnNewPlayer(sess, m_class, m_race, m_mapId, m_instanceId, m_x, m_y, m_z, m_o, nullptr);
}