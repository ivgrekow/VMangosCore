#ifndef WORLDBOT_H
#define WORLDBOT_H

#include "PlayerBotAI.h"
#include "PlayerBotMgr.h"
#include "SpellEntry.h"
#include "Player.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "ObjectMgr.h"
#include "ItemEnchantmentMgr.h"

#define WB_UPDATE_INTERVAL 1000


class WorldBotAI : public PlayerBotAI
{
public:
    WorldBotAI(Player* pLeader,
        uint8 race, 
        uint8 _class, 
        uint8 level, 
        uint32 mapId, 
        uint32 instanceId,
        float x, float y, float z, float o) : PlayerBotAI(nullptr), m_leader{pLeader}, m_race{race}, m_class{_class},
        m_level{level}, m_mapId{mapId}, m_instanceId{instanceId}, m_x{x}, m_y{y}, m_z{z}, m_o{o}
        {
            m_updateTimer.Reset(2000);
        }
        
        bool OnSessionLoaded(PlayerBotEntry* entry, WorldSession* sess) final;
        void OnPlayerLogin() final;
        void UpdateAI(uint32 diff);
        void OnPacketRecieved(WorldPacket const* packet);
        void PopulateSpellData();
        void InitTalentsByRandomSpec();
        void GenerateGear();
        
private:
    ShortTimeTracker m_updateTimer;
    std::unordered_map<std::string /*spell name*/, std::vector<const SpellEntry*> /*ranks*/> m_spellBook;
    std::string m_currentSpec;
    uint32 m_honorRank = 0;
    bool m_isInitialized = false;

    Player* m_leader = nullptr;
    uint8 m_race = 0;
    uint8 m_class = 0;
    uint8 m_level = 0;
    uint32 m_mapId = 0;
    uint32 m_instanceId = 0;
    float m_x = 0.0f;
    float m_y = 0.0f;
    float m_z = 0.0f;
    float m_o = 0.0f;

    bool IsRandomEnchantRelevant(std::string suffx);
    bool IsItemStatsRelevant(const ItemPrototype* pProto);
};


#endif