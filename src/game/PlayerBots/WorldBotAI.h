#ifndef WORLDBOT_H
#define WORLDBOT_H

#include "PlayerBotAI.h"
#include "PlayerBotMgr.h"
#include "SpellEntry.h"
#include "Spell.h"
#include "Player.h"
#include "Group.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "ObjectMgr.h"
#include "ItemEnchantmentMgr.h"
#include "Util.h"
#include "Utilities/Random.h"
#include "TradeData.h"

#define WB_UPDATE_INTERVAL 300
#define WB_UPDATE_MOVEMENT_INTERVAL 500

enum WorldBotMetrics {
    WB_METRIC_DISTANCE_TO_LEADER,
    WB_METRIC_DISTANCE_TO_TARGET_UNIT,
    WB_METRIC_DISTANCE_TO_TARGET_POS,
    WB_METRIC_MAX_LENGTH
};

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
        
        void SetTimerTracker(int32 value){
            m_updateTimer.Reset(value);
        }
        bool OnSessionLoaded(PlayerBotEntry* entry, WorldSession* sess) final;
        void OnPlayerLogin() final;
        void UpdateAI(uint32 diff) override;
        void OnPacketReceived(WorldPacket const* packet) override;
        void PopulateSpellData();
        void InitTalentsByRandomSpec();
        void GenerateGear();

        // groups and raids
        bool AreOthersOnSameTarget(ObjectGuid guid, bool checkMelee = true, bool checkSpells = true) const;
        Player* GetPartyLeader() const;
        Unit* GetMarkedTarget(RaidTargetIcon mark) const;
        
        // items
        void UseMount();
        void AddItemToInventory(uint32 itemId, uint32 count = 1);
        void AddAllClassSpellReagents();
        void AddRangedWeaponAmmo();
        bool UseItemEffect(Item* pItem);
        void CastEnchantSpell(uint32 id);
        inline uint32 GetGatherProfessionId() { return m_gatherProfession; }
        inline uint32 GetCraftProfessionId() { return m_craftProfession; }
        std::vector<const SpellEntry*> GetProffessionSpellsFilterByRegex(std::string filter) const;
        
        // movement
        void UpdateMovements();
        void FollowLeader();
        void SetLeader(Player* pLeader);
        void SetTravelJourney(std::vector<WorldBotConfig::TravelNode> travel, Position targetPos);
        void TravelToTarget(Unit* pTarget);
        void TravelToPoint(uint32 mapId, Position pos);
        void ClearTravelData(bool clearTargetPos = false);
        void FlushMetrics(){
            for (float& num: m_metrics)
                num = -1.0f;
        }
        
        // combat
        bool IsInDuel() const;
        bool CanTryToCastSpell(Unit const* pTarget, SpellEntry const* pSpellEntry) const;
        SpellCastResult DoCastSpell(Unit* pTarget, SpellEntry const* pSpellEntry);
        bool IsValidHostileTarget(Unit const* pTarget) const;
        bool CanUseCrowdControl(SpellEntry const* pSpellEntry, Unit* pTarget) const;
        Unit* SelectPartyAttackTarget() const;
        Unit* SelectAttackTarget(Player* pLeader) const;
        bool AttackStart(Unit* pVictim);
        
private:
    ShortTimeTracker m_updateTimer;
    Item* m_ridingPet = nullptr;
    std::unordered_map<std::string /*spell name*/, std::vector<const SpellEntry*> /*ranks*/> m_spellBook;
    std::vector<const SpellEntry*> m_professionBook;
    std::string m_currentSpec;
    uint32 m_gatherProfession = 0;
    uint32 m_craftProfession = 0;
    uint32 m_honorRank = 0;
    bool m_isInitialized = false;
    uint32 m_currentNodeIndex = 0;
    std::vector<WorldBotConfig::TravelNode> m_travelNodeVector;
    WorldBotConfig::TravelNode m_currentNode;
    uint32 m_travelNodePathNR = 0;
    std::vector<WorldBotConfig::TravelNodePath> m_travelNodePath;
    Position m_targetPos;
    std::vector<float> m_metrics {std::vector<float>(WB_METRIC_MAX_LENGTH, -1.0f)};

    Unit* m_primaryTarget = nullptr;

    std::vector<RaidTargetIcon> m_marksToCC;
    std::vector<RaidTargetIcon> m_marksToFocus;
    CombatBotRoles m_role = ROLE_INVALID;
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

    bool IsRandomEnchantRelevant(std::string suffx) const;
    bool AreItemStatsRelevant(const ItemPrototype* pProto) const;
    bool GenerateInventorySlotItem(
        std::map<uint32 /*slot*/, std::vector<ItemPrototype const*>>& itemsPerSlot,
        uint32 invSlot, bool isNeedToEquip /* true -- equip, false -- store */);
    void GenerateInventorySlotPermEnchant(Item*);
    void AddAllProfessionSpellReagents(uint32 id);
    void AddAllProfessionSpellTotems(uint32 id);
    void PopulateProffessionSpells();
    void GenerateBags();
    void GenerateMounts();

    void UpdateOutOfCombatAI();
    void UpdateOutOfCombatWarrior();
};


#endif