#ifndef _PLAYERBOTMGR_H
#define _PLAYERBOTMGR_H

#include "Common.h"
#include "Policies/Singleton.h"
#include "Database/DatabaseEnv.h"
#include "PlayerBotAI.h"
#include "BattleGroundDefines.h"
#include "PathFinder.h"

#include <vector>
#include <memory>
#include <regex>

class PlayerBotAI;
class WorldSession;
class Player;

enum PlayerBotAutoEquip
{
    PLAYER_BOT_AUTO_EQUIP_STARTING_GEAR = 0,
    PLAYER_BOT_AUTO_EQUIP_RANDOM_GEAR = 1,
    PLAYER_BOT_AUTO_EQUIP_PREMADE_GEAR = 2,
};

enum PlayerBotState
{
    PB_STATE_OFFLINE,
    PB_STATE_LOADING,
    PB_STATE_ONLINE
};

struct PlayerBotEntry
{
    uint64 playerGUID;
    std::string name;
    uint32 accountId;

    uint32 chance;
    uint8 state; //Online, in queue or offline
    bool isChatBot; // bot des joueurs en discussion via le site.
    bool customBot; // Enabled even if PlayerBot system disabled (AutoTesting system for example)
    bool requestRemoval;
    std::unique_ptr<PlayerBotAI> ai;

    PlayerBotEntry(uint64 guid, uint32 account, uint32 chance_): playerGUID(guid), accountId(account), chance(chance_), state(PB_STATE_OFFLINE), isChatBot(false), customBot(false), requestRemoval(false), ai(nullptr)
    {}
    PlayerBotEntry(): playerGUID(0), accountId(0), chance(100.0f), state(PB_STATE_OFFLINE), isChatBot(false), customBot(false), requestRemoval(false), ai(nullptr)
    {}
};

struct PlayerBotStats
{
    /* Stats */
    uint32 onlineCount;
    uint32 loadingCount;
    uint32 totalBots;
    uint32 onlineChat;

    /* Config */
    uint32 confMaxOnline;
    uint32 confMinOnline;
    uint32 confRandomBotsRefresh;
    uint32 confUpdateDiff;

    PlayerBotStats() 
    : onlineCount(0), loadingCount(0), totalBots(0), onlineChat(0),
    confMaxOnline(0), confMinOnline(0), confRandomBotsRefresh(0), confUpdateDiff(0) {}
};

struct WorldBotConfig {
    float gearQuality;

    struct TravelNode {
        uint32 id; // prim key
        std::string name;
        uint32 map_id;
        float x, y, z;
        bool linked;

        TravelNode(){
            this->id = 0;
            this->name = "";
            this->map_id = 0;
            this->x = 0.0f;
            this->y = 0.0f;
            this->z = 0.0f;
            this->linked = false;
        }

        TravelNode(uint32 id, std::string name, uint32 map_id, float x, float y, float z, bool linked){
            this->id = id;
            this->name = name;
            this->map_id = map_id;
            this->x = x;
            this->y = y;
            this->z = z;
            this->linked = linked;
        }

        void operator=(const TravelNode& other){
            id = other.id;
            name = other.name;
            map_id = other.map_id;
            x = other.x;
            y = other.y;
            z = other.z;
            linked = other.linked;
        }

        bool operator==(const TravelNode& other) const {
            if (id == other.id &&
                name == other.name &&
                map_id == other.map_id &&
                x == other.x && y == other.y && z == other.z &&
                linked == other.linked)
                return true;
            else
                return false;
        }
        bool operator!=(const TravelNode& other) const {
            if (id != other.id ||
                name != other.name ||
                map_id != other.map_id ||
                x != other.x || y != other.y || z != other.z ||
                linked != other.linked)
                return true;
            else
                return false;
        }
    };

    struct TravelNodeLink {
        uint32 node_id = 0; // prim key
        uint32 to_node_id = 0; // prim key
        uint8 type = 0;
        uint32 object = 0;
        float distance = 0.0f;
        float swim_distance = 0.0f;
        float extra_cost = 0.0f;
        uint8 calculated = 0;
        uint8 max_creature0 = 0;
        uint8 max_creature1 = 0;
        uint8 max_creature2 = 0;

        TravelNodeLink() = default;
    };

    struct TravelNodePath {
        uint32 node_id = 0; // prim key
        uint32 to_node_id = 0; // prim key
        uint32 nr = 0; // prim key
        uint32 map_id = 0;
        float x = 0.0f, y = 0.0f, z = 0.0f;

        TravelNodePath() = default;

        Position GetPosition() const{
            return Position(x, y, z, 10.0f);
        }
    };

    struct TaxiFlightNodeInfo{
        uint32 id = 0;
        uint32 mount_creature_id1 = 0;
        uint32 mount_creature_id2 = 0;

        TaxiFlightNodeInfo() = default;
    };

    std::vector<TravelNode> travelNode;
    std::vector<TravelNodeLink> travelNodeLink;
    std::vector<TravelNodePath> travelNodePath;
    std::vector<TaxiFlightNodeInfo> taxiNodesInfo;

    uint32 currentNodeId = 0;
    uint32 previousNodeId = 0;
    std::vector<TravelNode> recordNodes;
    std::vector<TravelNodeLink> recordNodeLinks;
    std::vector<TravelNodePath> recordNodePaths;
    uint32 currentPathNodeIndex = 0;
};

enum TravelNodePathType {
    PATH_TYPE_NONE,
    PATH_TYPE_WALK,
    PATH_TYPE_AREATRIGGER,
    PATH_TYPE_TRANSPORT,
    PATH_TYPE_FLIGHTPATH,
    PATH_TYPE_TELEPORTSPELL,
    PATH_TYPE_STATICPORTAL,
    PATH_TYPE_MAX
};

class PlayerBotMgr
{
    public:
        PlayerBotMgr();
        ~PlayerBotMgr();

        void LoadConfig();
        void Load();

        void Update(uint32 diff);
        bool AddOrRemoveBot();

        bool AddBot(PlayerBotAI* ai);
        bool AddBot(uint32 playerGuid, bool chatBot = false, PlayerBotAI* pAI = nullptr);
        bool DeleteBot(std::map<uint64, std::shared_ptr<PlayerBotEntry>>::iterator iter);
        bool DeleteBot(uint32 playerGuid);

        bool AddRandomBot();
        bool DeleteRandomBot();

        void AddBattleBot(BattleGroundQueueTypeId queueType, Team botTeam, uint32 botLevel, bool temporary);
        void DeleteBattleBots();

        void DeleteAll();
        void AddAllBots();

        void OnBotLogout(PlayerBotEntry *e);
        void OnBotLogin(PlayerBotEntry *e);
        void OnPlayerInWorld(Player* pPlayer);
        void AddTempBot(uint32 account, uint32 time);
        void RefreshTempBot(uint32 account);

        bool ForceAccountConnection(WorldSession* sess);
        bool IsPermanentBot(uint32 playerGuid);
        bool IsChatBot(uint32 playerGuid);
        bool IsSavingAllowed() { return m_confAllowSaving; }

        uint32 GenBotAccountId() { return ++m_maxAccountId; }
        PlayerBotStats& GetStats(){ return m_stats; }
        void Start() { m_confEnableRandomBots = true; }

        // В объявлении класса PlayerBotMgr (ai generated shit, need to make it myself...)
        std::vector<WorldBotConfig::TravelNode> FindAStarPath(uint8 faction, uint32 startMapId, Position posStart, uint32 endMapId, Position posEnd);
        float CalculateHeuristic(const WorldBotConfig::TravelNode& from, const WorldBotConfig::TravelNode& to);
        // my proper development!
        std::vector<WorldBotConfig::TravelNode> FindGreedyPath(uint8 faction, uint32 startMapId, Position posStart, uint32 endMapId, Position posEnd);
        WorldBotConfig::TravelNode GetTravelNode(uint32 id) const;
        std::vector<WorldBotConfig::TravelNode> GetTravelNodesFilterByRegex(std::string regex);
        std::vector<WorldBotConfig::TravelNodeLink> GetTravelNodeLinksById(uint32 id) const;
        WorldBotConfig::TravelNodeLink GetTravelNodeLink(uint32 node_id, uint32 to_node_id);
        std::vector<WorldBotConfig::TravelNodePath> GetTravelNodePathVector(uint32 mapId, uint32 node_id, uint32 to_node_id) const;
        std::vector<WorldBotConfig::TravelNodePath> GetRecordNodePathVector(uint32 mapId, uint32 node_id, uint32 to_node_id) const;
        WorldBotConfig::TaxiFlightNodeInfo GetTaxiFlightNodeInfoById(uint32 id) const;
        inline float GetWorldBotGearQuality() { return m_worldBotConfig.gearQuality; }
        std::unordered_map<std::string /* spec name */, std::vector<std::pair<uint32, uint32>> /* talent sequence */> GetSpecsFilterByRegex(std::string regex);
        std::vector<std::pair<uint32, uint32>> GetSpecByName(std::string properName);
        void SpawnWorldBots(uint8 lvlMin, uint8 lvlMax, std::vector<uint8> allowesRaces, std::vector<uint8> allowedClasses, std::string location, uint32 count);

        WorldBotConfig* GetWorldBotConfig() { return &m_worldBotConfig; }
        uint32 GetRecordNodeIndex(uint32 id){
            uint32 index = UINT32_MAX;
            for (uint32 i=0; i < m_worldBotConfig.recordNodes.size(); ++i){
                if (m_worldBotConfig.recordNodes[i].id == id){
                    index = i;
                    break;
                }
            }
            return index;
        }
        bool DoesLinkExist(uint32 node_id, uint32 to_node_id){
            bool result = false;
            for (auto link: m_worldBotConfig.recordNodeLinks){
                if (link.node_id == node_id && link.to_node_id == to_node_id){
                    result = true;
                    break;
                }
            }
            return result;
        }

    protected:
        // How long since last update?
        uint32 m_elapsedTime;
        uint32 m_lastBotsRefresh;
        uint32 m_lastUpdate;
        uint32 m_totalChance;
        uint32 m_maxAccountId;
        time_t m_lastBattleBotQueueUpdate;

        std::map<uint64 /*pl guid*/, std::shared_ptr<PlayerBotEntry>> m_bots;
        std::map<uint32 /*account*/, uint32> m_tempBots;
        PlayerBotStats m_stats;

        uint32 m_confMinRandomBots;
        uint32 m_confMaxRandomBots;
        uint32 m_confRandomBotsRefresh;
        uint32 m_confUpdateDiff;
        bool m_confAllowSaving;
        bool m_confDebug;
        bool m_confEnableRandomBots;
        bool m_confBattleBotAutoJoin;

        WorldBotConfig m_worldBotConfig;
        std::unordered_map<std::string, std::vector<std::pair<uint32, uint32>>> m_worldBotDefaultTalentSpecs;
};

#define sPlayerBotMgr MaNGOS::Singleton<PlayerBotMgr>::Instance()
#endif
