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

#define WB_UPDATE_INTERVAL 1000
#define WB_UPDATE_MOVEMENT_INTERVAL 500

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
        void AddAllSpellReagents();
        void AddRangedWeaponAmmo();
        bool UseItemEffect(Item* pItem);
        
        // movement
        void UpdateMovements();
        void FollowLeader();
        void TravelToTarget(Unit* pTarget);
        void TravelToPoint(uint32 mapId, Position pos);
        
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
    std::vector<const SpellEntry*> m_proffessionBook;
    std::string m_currentSpec;
    uint32 m_honorRank = 0;
    bool m_isInitialized = false;
    ShortTimeTracker m_movementTimer;
    std::vector<WorldBotConfig::TravelNode> m_travelPath;

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
    void PopulateProffessionSpells();
    void GenerateBags();
    void GenerateMounts();

    void UpdateOutOfCombatAI();
    void UpdateOutOfCombatWarrior();
};


#endif