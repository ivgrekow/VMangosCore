#include "Common.h"
#include "Policies/SingletonImp.h"
#include "PlayerBotMgr.h"
#include "ObjectMgr.h"
#include "World.h"
#include "WorldSession.h"
#include "AccountMgr.h"
#include "Config/Config.h"
#include "Chat.h"
#include "Player.h"
#include "Group.h"
#include "PlayerBotAI.h"
#include "PartyBotAI.h"
#include "BattleBotAI.h"
#include "WorldBotAI.h"
#include "BattleBotWaypoints.h"
#include "BattleGroundMgr.h"
#include "MapManager.h"
#include "Language.h"
#include "Utilities/Random.h"
#include "Spell.h"

#include <fstream>

INSTANTIATE_SINGLETON_1(PlayerBotMgr);


PlayerBotMgr::PlayerBotMgr()
{
    m_totalChance = 0;
    m_maxAccountId = 0;

    // Config
    m_confMinRandomBots         = 3;
    m_confMaxRandomBots         = 10;
    m_confRandomBotsRefresh     = 60000;
    m_confUpdateDiff            = 10000;
    m_confEnableRandomBots      = false;
    m_confDebug                 = false;
    m_confBattleBotAutoJoin     = false;

    // Time
    m_elapsedTime = 0;
    m_lastBotsRefresh = 0;
    m_lastUpdate = 0;
    m_lastBattleBotQueueUpdate = 0;
}

PlayerBotMgr::~PlayerBotMgr()
{

}

void PlayerBotMgr::LoadConfig()
{
    m_confEnableRandomBots = sConfig.GetBoolDefault("RandomBot.Enable", false);
    m_confMinRandomBots = sConfig.GetIntDefault("RandomBot.MinBots", 3);
    m_confMaxRandomBots = sConfig.GetIntDefault("RandomBot.MaxBots", 10);
    m_confRandomBotsRefresh = sConfig.GetIntDefault("RandomBot.Refresh", 60000);
    m_confAllowSaving = sConfig.GetBoolDefault("PlayerBot.AllowSaving", false);
    m_confDebug = sConfig.GetBoolDefault("PlayerBot.Debug", false);
    m_confUpdateDiff = sConfig.GetIntDefault("PlayerBot.UpdateMs", 10000);
    m_confBattleBotAutoJoin = sConfig.GetBoolDefault("BattleBot.AutoJoin", false);

    m_worldBotConfig.gearQuality = sConfig.GetFloatDefault("WorldBot.GearQuality", 0.5);

    if (!sWorld.getConfig(CONFIG_BOOL_FORCE_LOGOUT_DELAY))
        m_tempBots.clear();
}

void PlayerBotMgr::Load()
{
    // 1- Clean
    DeleteAll();
    m_bots.clear();
    m_tempBots.clear();
    m_totalChance = 0;

    // 2- Configuration
    LoadConfig();

    // 3- Load usable account ID
    std::unique_ptr<QueryResult> result = LoginDatabase.PQuery(
                              "SELECT MAX(`id`)"
                              " FROM `account`");
    if (!result)
    {
        sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "Playerbot: unable to load max account id.");
        return;
    }
    Field* fields = result->Fetch();
    m_maxAccountId = fields[0].GetUInt32() + 10000;

    // 4- LoadFromDB
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, ">> [PlayerBotMgr] Loading Bots ...");
    result = CharacterDatabase.PQuery(
                 "SELECT char_guid, chance, ai"
                 " FROM playerbot");
    if (!result)
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "Table `playerbot` is empty.");
    else
    {
        do
        {
            fields = result->Fetch();
            uint32 guid = fields[0].GetUInt32();
            uint32 acc = GenBotAccountId();
            uint32 chance = fields[1].GetUInt32();

            std::shared_ptr<PlayerBotEntry> entry = std::make_shared<PlayerBotEntry>(guid, acc, chance);
            entry->ai.reset(CreatePlayerBotAI(fields[2].GetCppString()));
            entry->ai->botEntry = entry.get();
            if (!sObjectMgr.GetPlayerNameByGUID(guid, entry->name))
                entry->name = "<Unknown>";
            entry->ai->OnBotEntryLoad(entry.get());
            m_bots.insert({ entry->playerGUID, entry });
            m_totalChance += chance;
        }
        while (result->NextRow());
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "%u bots loaded", m_bots.size());
    }

    // 5- Check config/DB
    if (m_confMinRandomBots >= m_bots.size() && !m_bots.empty())
        m_confMinRandomBots = m_bots.size() - 1;
    if (m_confMaxRandomBots > m_bots.size())
        m_confMaxRandomBots = m_bots.size();
    if (m_confMaxRandomBots <= m_confMinRandomBots)
        m_confMaxRandomBots = m_confMinRandomBots + 1;

    // 6- Start initial bots
    if (m_confEnableRandomBots)
    {
        for (uint32 i = 0; i < m_confMinRandomBots; i++)
            AddRandomBot();
    }

    // 7- Fill stats info
    m_stats.confMaxOnline = m_confMaxRandomBots;
    m_stats.confMinOnline = m_confMinRandomBots;
    m_stats.totalBots = m_bots.size();
    m_stats.confRandomBotsRefresh = m_confRandomBotsRefresh;
    m_stats.confUpdateDiff = m_confUpdateDiff;

    // 8- Show stats if debug
    if (m_confDebug)
    {
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[PlayerBotMgr] Between %u and %u bots online", m_confMinRandomBots, m_confMaxRandomBots);
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[PlayerBotMgr] %u now loading", m_stats.loadingCount);
    }

    // 9 - WorldBot talent sequences (default hardcoded)
    // warrior tank
    std::vector<std::pair<uint32, uint32>> talentSequence
    {
        {1601, 0}, {1601, 1}, {1601, 2}, {1601, 3}, {1601, 4}, // Shield Specialization
        {142, 0}, {142, 1}, // Improved Bloodrage
        {138, 0}, {138, 1}, {138, 2}, // Anticipation
        {153, 0}, // Last Stand
        {147, 0}, {147, 1}, {147, 2}, // Improved Revenge 
        {145, 0}, // Improved Shield Block
        {143, 0}, {143, 1}, // Improved Taunt
        {146, 0}, {146, 1}, {146, 2}, // Improved Sunder Armor
        {152, 0}, // Concussion Blow
        {145, 1}, {145, 2}, // Improved Shield Block
        {138, 3}, {138, 4}, // Anticipation
        {144, 0}, {144, 1}, {144, 2}, {144, 3}, {144, 4}, // Defiance
        {148, 0}, // Shield Slam
        {124, 0}, {124, 1}, {124, 2}, // Improved Heroic Strike
        {130, 0}, {130, 1}, // Deflection
        {641, 0}, {641, 1}, {641, 2}, {641, 3}, {641, 4}, // Tactical Mastery
        {137, 0}, // Anger Managment
        {140, 0}, {140, 1}, {140, 2}, {140, 3}, {140, 4}, // Toughness
        {130, 2}, {130, 3}, {130, 4}, // Deflection
        {150, 0} // Improved Shield Wall
    };
    m_worldBotDefaultTalentSpecs["warrior_tank"] = talentSequence;
    talentSequence.clear();

    // warrior arms
    talentSequence = {
        {127, 0}, {127, 1}, {127, 2}, // Improved Rend
        {124, 0}, {124, 1}, // Improved Heroic Strike
        {641, 0}, {641, 1}, {641, 2}, {641, 3}, {641, 4}, // Tactical Mastery
        {121, 0}, {121, 1}, {121, 2}, // Deep Wounds
        {131, 0}, {131, 1}, // Improved Overpower
        {662, 0}, {662, 1}, // Impale
        {137, 0}, // Anger Managment
        {136, 0}, {136, 1}, // Two-Handed Weapon Specialization
        {133, 0}, // Sweeping Strikes
        {136, 2}, {136, 3}, {136, 4}, // Two-Handed Weapon Specialization
        {124, 2}, // Improved Heroic Strike
        {129, 0}, {129, 1}, {129, 2}, // Improved Hamstring
        {126, 0}, {126, 1}, // Improved Charge
        {135, 0}, // Mortal Strike
        {157, 0}, {157, 1}, {157, 2}, {157, 3}, {157, 4}, // Cruelty
        {159, 0}, {159, 1}, {159, 2}, {159, 3}, {159, 4}, // Unbridled Wrath
        {160, 0}, // Piercing Howl
        {661, 0}, {661, 1}, {661, 2}, // Blood Craze
        {166, 0}, // Improved Cleave
        {1542, 0}, {1542, 1}, // Improved Execute
        {155, 0}, {155, 1}, {155, 2} // Enrage
    };
    m_worldBotDefaultTalentSpecs["warrior_arms"] = talentSequence;
    talentSequence.clear();
    
    // warrior fury
    talentSequence = {
        {157, 0}, {157, 1}, {157, 2}, {157, 3}, {157, 4}, // Cruelty
        {159, 0}, {159, 1}, {159, 2}, {159, 3}, {159, 4}, // Unbridled Wrath
        {160, 0}, // Piercing Howl
        {166, 0}, {166, 1}, {166, 2}, // Improved Cleave
        {160, 0}, // Piercing Howl
        {661, 0}, // Blood Craze
        {155, 0}, {155, 1}, {155, 2}, {155, 3}, {155, 4}, // Enrage
        {165, 0}, // Death Wish
        {1543, 0}, {1543, 1}, // Improved Intercept
        {661, 1}, {661, 2}, // Blood Craze
        {156, 0}, {156, 1}, {156, 2}, {156, 3}, {156, 4}, // Flurry
        {167, 0}, // Blood Thirst
        {127, 0}, {127, 1}, {127, 2}, // Improved Rend
        {124, 0}, {124, 1}, // Improved Heroic Strike
        {641, 0}, {641, 1}, {641, 2}, {641, 3}, {641, 4}, // Tactical Mastery
        {137, 0}, // Anger Managment
        {121, 0}, {121, 1}, {121, 2}, // Deep Wounds
        {124, 2}, // Improved Heroic Strike
        {662, 0}, {662, 1}, // Impale
        {1542, 0}, {1542, 1}, // Improved Execute
        {1581, 0}, // Dual Wield Specialization
    };
    m_worldBotDefaultTalentSpecs["warrior_fury"] = talentSequence;
    talentSequence.clear();

    // druid restoration
    talentSequence = {
        {821, 0}, {821, 1}, {821, 2}, {821, 3}, {821, 4}, // Improved Mark of the Wild
        {823, 0}, {823, 1}, {823, 2}, {823, 3}, {823, 4}, // Nature Focus
        {824, 0}, {824, 1}, {824, 2}, {824, 3}, {824, 4}, // Improved Healing Touch
        {827, 0}, // Insect Swarm
        {830, 0}, {830, 1}, {830, 2}, // Improved Rejuvenation
        {843, 0}, // Tranquil Spirit
        {831, 0}, // Nature Swiftness
        {843, 1}, {843, 2}, {843, 3}, {843, 4}, // Tranquil Spirit
        {828, 0}, {828, 1}, {828, 2}, {828, 3}, {828, 4}, // Gift of the Nature
        {844, 0}, // Swiftmend
        {829, 0}, {829, 1}, {829, 2}, // Reflection
        {841, 0}, {841, 1}, {841, 2}, {841, 3}, {841, 4}, // Subtlety
        {762, 0}, {762, 1}, {762, 2}, {762, 3}, {762, 4}, // Improved Wrath
        {761, 0}, // Nature's Grasp
        {921, 0}, {921, 1}, {921, 2}, {921, 3}, // Improved Nature's Grasp
        {782, 0}, {782, 1}, // Improved Thorns
    };
    m_worldBotDefaultTalentSpecs["druid_restoration"] = talentSequence;
    talentSequence.clear();

    // worldbot travelnodes
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, ">> [PlayerBotMgr] Loading worldbot travelnodes...");
    result.reset();
    result = WorldDatabase.PQuery("SELECT id, name, map_id, x, y, z, linked FROM worldbot_travelnode;");
    if (!result)
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "Table `worldbot_travelnode` is empty.");
    else{
        do {
            fields = result->Fetch();
            uint32 id = fields[0].GetUInt32();
            std::string name {fields[1].GetString()};
            uint32 map_id = fields[2].GetUInt32();
            float x = fields[3].GetFloat();
            float y = fields[4].GetFloat();
            float z = fields[5].GetFloat();
            uint8 linked = fields[6].GetUInt8();
            WorldBotConfig::TravelNode travelNode;
            travelNode.id = id;
            travelNode.name = name;
            travelNode.map_id = map_id;
            travelNode.x = x;
            travelNode.y = y;
            travelNode.z = z;
            travelNode.linked = linked;
            m_worldBotConfig.travelNode.push_back(travelNode);
        } while (result->NextRow());
    }
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, ">> Loaded %d worldbot travelnodes.", m_worldBotConfig.travelNode.size());

    // worldbot travelnode links
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, ">> [PlayerBotMgr] Loading worldbot travelnode links...");
    result.reset();
    result = WorldDatabase.PQuery("SELECT node_id, to_node_id, type, object, distance, swim_distance, extra_cost, calculated, max_creature_0, max_creature_1, max_creature_2 FROM worldbot_travelnode_link;");
    if (!result)
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, ">> Table `worldbot_travelnode_link` is empty.");
    else {
        do {
            fields = result->Fetch();
            WorldBotConfig::TravelNodeLink travelnodeLink;
            travelnodeLink.node_id = fields[0].GetUInt32();
            travelnodeLink.to_node_id = fields[1].GetUInt32();
            travelnodeLink.type = fields[2].GetUInt8();
            travelnodeLink.object = fields[3].GetUInt32();
            travelnodeLink.distance = fields[4].GetFloat();
            travelnodeLink.swim_distance = fields[5].GetFloat();
            travelnodeLink.extra_cost = fields[6].GetFloat();
            travelnodeLink.calculated = fields[7].GetUInt8();
            travelnodeLink.max_creature0 = fields[8].GetUInt8();
            travelnodeLink.max_creature1 = fields[9].GetUInt8();
            travelnodeLink.max_creature2 = fields[10].GetUInt8();
            m_worldBotConfig.travelNodeLink.push_back(travelnodeLink);
        } while (result->NextRow());
    }
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, ">> Loaded %d worldbot travelnode links.", m_worldBotConfig.travelNodeLink.size());

    // worldbot travelnode paths
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, ">> [PlayerBotMgr] Loading worldbot travelnode paths...");
    result.reset();
    result = WorldDatabase.PQuery("SELECT node_id, to_node_id, nr, map_id, x, y, z FROM worldbot_travelnode_path;");
    if (!result)
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "Table `worldbot_travelnode_path` is empty.");
    else {
        do {
            fields = result->Fetch();
            WorldBotConfig::TravelNodePath travelnodePath;
            travelnodePath.node_id = fields[0].GetUInt32();
            travelnodePath.to_node_id = fields[1].GetUInt32();
            travelnodePath.nr = fields[2].GetUInt32();
            travelnodePath.map_id = fields[3].GetUInt32();
            travelnodePath.x = fields[4].GetFloat();
            travelnodePath.y = fields[5].GetFloat();
            travelnodePath.z = fields[6].GetFloat();
            m_worldBotConfig.travelNodePath.push_back(travelnodePath);
        } while (result->NextRow());
    }
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, ">> Loaded %d worldbot travelnode paths.", m_worldBotConfig.travelNodePath.size());

    // taxi flight info
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, ">> [PlayerBotMgr] Loading worldbot taxi info...");
    result.reset();
    result = WorldDatabase.PQuery("SELECT id, mount_creature_id1, mount_creature_id2 FROM taxi_nodes;");
    if (!result)
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "Table `taxi_nodes` is empty.");
    else {
        do {
            fields = result->Fetch();
            WorldBotConfig::TaxiFlightNodeInfo info;
            info.id = fields[0].GetUInt32();
            info.mount_creature_id1 = fields[1].GetUInt32();
            info.mount_creature_id2 = fields[2].GetUInt32();
            m_worldBotConfig.taxiNodesInfo.push_back(info);
        }while (result->NextRow());
    }
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, ">> Loaded %d worldbot taxi info entries.", m_worldBotConfig.taxiNodesInfo.size());
}

void PlayerBotMgr::DeleteAll()
{
    m_stats.onlineCount = 0;
    m_stats.loadingCount = 0;

    for (auto i = m_bots.begin(); i != m_bots.end(); i++)
    {
        if (i->second->state != PB_STATE_OFFLINE)
        {
            OnBotLogout(i->second.get());
            m_totalChance += i->second->chance;
        }
    }
    m_tempBots.clear();

    if (m_confDebug)
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[PlayerBotMgr] Deleting all bots [OK]");
}

void PlayerBotMgr::OnBotLogin(PlayerBotEntry *e)
{
    e->state = PB_STATE_ONLINE;
    if (m_confDebug)
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[PlayerBot][Login]  '%s' GUID:%u Acc:%u", e->name.c_str(), e->playerGUID, e->accountId);
}
void PlayerBotMgr::OnBotLogout(PlayerBotEntry *e)
{
    e->state = PB_STATE_OFFLINE;
    if (m_confDebug)
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[PlayerBot][Logout] '%s' GUID:%u Acc:%u", e->name.c_str(), e->playerGUID, e->accountId);
}

void PlayerBotMgr::OnPlayerInWorld(Player* player)
{
    if (PlayerBotEntry* e = player->GetSession()->GetBot())
    {
        player->SetAI(e->ai.get());
        e->ai->SetPlayer(player);
        e->ai->OnPlayerLogin();
    }
}

void PlayerBotMgr::Update(uint32 diff)
{
    // Temporary bots.
    std::map<uint32, uint32>::iterator it;
    for (it = m_tempBots.begin(); it != m_tempBots.end(); ++it)
    {
        if (it->second < diff)
            it->second = 0;
        else
            it->second -= diff;
    }
    it = m_tempBots.begin();
    while (it != m_tempBots.end())
    {
        if (!it->second)
        {
            // Update of "chatBot" too.
            for (auto iter = m_bots.begin(); iter != m_bots.end(); ++iter)
                if (iter->second->accountId == it->first)
                {
                    iter->second->state = PB_STATE_OFFLINE; // Will get logged out at next WorldSession::Update call
                    m_bots.erase(iter);
                    break;
                }
            m_tempBots.erase(it);
            it = m_tempBots.begin();
        }
        else
            ++it;
    }

    m_elapsedTime += diff;
    if (!((m_elapsedTime - m_lastUpdate) > m_confUpdateDiff))
        return; // No need to update
    m_lastUpdate = m_elapsedTime;

    for (auto iter = m_bots.begin(); iter != m_bots.end();)
    {
        if (!m_confEnableRandomBots && !iter->second->customBot)
        {
            ++iter;
            continue;
        }

        if (iter->second->state == PB_STATE_ONLINE)
        {
            if (iter->second->requestRemoval)
            {
                if (iter->second->ai && iter->second->ai->me)
                {
                    if (!iter->second->ai->me->IsAlive())
                    {
                        // don't leave permanent corpse
                        iter->second->ai->me->ResurrectPlayer(1.0f);
                        iter->second->ai->me->SpawnCorpseBones();
                    }
                    iter->second->ai->me->RemoveFromGroup();
                }

                DeleteBot(iter);

                if (WorldSession* sess = sWorld.FindSession(iter->second->accountId))
                    sess->LogoutPlayer(m_confAllowSaving);

                iter->second->requestRemoval = false;

                if (iter->second->customBot)
                    iter = m_bots.erase(iter);
                else
                    ++iter;
                continue;
            }
        }

        // Connection of pending bots
        if (iter->second->state != PB_STATE_LOADING)
        {
            ++iter;
            continue;
        }

        WorldSession* sess = sWorld.FindSession(iter->second->accountId);

        if (!sess)
        {
            // This may happen : just wait for the World to add the session.
            ++iter;
            continue;
        }

        if (iter->second->ai->OnSessionLoaded(iter->second.get(), sess))
        {
            OnBotLogin(iter->second.get());
            m_stats.loadingCount--;

            if (iter->second->isChatBot)
                m_stats.onlineChat++;
            else
                m_stats.onlineCount++;
        }
        else
        {
            sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "PLAYERBOT: Unable to load session id %u", iter->second->accountId);
            DeleteBot(iter);

            if (iter->second->customBot)
                iter = m_bots.erase(iter);
            else
                ++iter;
            continue;
        }

        ++iter;
    }

    if (m_confBattleBotAutoJoin && (m_lastBattleBotQueueUpdate <= (sWorld.GetGameTime() - 10)))
    {
        m_lastBattleBotQueueUpdate = sWorld.GetGameTime();
        for (uint32 queueType = BATTLEGROUND_QUEUE_AV; queueType < MAX_BATTLEGROUND_QUEUE_TYPES; ++queueType)
        {
            bool hasPlayerInQueue[MAX_BATTLEGROUND_BRACKETS] = {};
            uint32 queuedAllianceCount[MAX_BATTLEGROUND_BRACKETS] = {};
            uint32 queuedHordeCount[MAX_BATTLEGROUND_BRACKETS] = {};
            BattleGroundQueue const& bgQueue = sBattleGroundMgr.m_battleGroundQueues[queueType];
            for (auto const& itr : bgQueue.m_queuedPlayers)
            {
                if (itr.second.groupInfo->isInvitedToBgInstanceGuid)
                    continue;

                if (Player* pPlayer = sObjectAccessor.FindPlayer(itr.first))
                {
                    BattleGroundTypeId bgTypeId = itr.second.groupInfo->bgTypeId;
                    BattleGroundBracketId bgBracketId = pPlayer->GetBattleGroundBracketIdFromLevel(bgTypeId);
                    if (bgBracketId == BG_BRACKET_ID_NONE)
                        continue;

                    if (itr.second.groupInfo->groupTeam == ALLIANCE)
                        ++queuedAllianceCount[bgBracketId];
                    else
                        ++queuedHordeCount[bgBracketId];

                    if (!pPlayer->IsBot())
                        hasPlayerInQueue[bgBracketId] = true;
                }
            }

            for (uint32 bracketId = BG_BRACKET_ID_FIRST; bracketId < MAX_BATTLEGROUND_BRACKETS; ++bracketId)
            {
                if (!hasPlayerInQueue[bracketId])
                    continue;

                if (!queuedAllianceCount[bracketId] && !queuedHordeCount[bracketId])
                    continue;

                BattleGroundTypeId bgTypeId = BattleGroundMgr::BgTemplateId(BattleGroundQueueTypeId(queueType));
                BattleGround* bg = sBattleGroundMgr.GetBattleGroundTemplate(bgTypeId);
                ASSERT(bg);

                uint32 const minLevel = bg->GetMinLevel() + 10 * bracketId;
                ASSERT(minLevel <= PLAYER_MAX_LEVEL);
                uint32 const maxLevel = std::min<uint32>(minLevel + 9, PLAYER_MAX_LEVEL);

                for (uint32 i = queuedAllianceCount[bracketId]; i < bg->GetMinPlayersPerTeam(); ++i)
                {
                    uint32 const botLevel = urand(minLevel, maxLevel);
                    AddBattleBot(BattleGroundQueueTypeId(queueType), ALLIANCE, botLevel, true);
                }
                for (uint32 i = queuedHordeCount[bracketId]; i < bg->GetMinPlayersPerTeam(); ++i)
                {
                    uint32 const botLevel = urand(minLevel, maxLevel);
                    AddBattleBot(BattleGroundQueueTypeId(queueType), HORDE, botLevel, true);
                }
            }
        }
    }

    if (m_confEnableRandomBots)
    {
        uint32 updatesCount = (m_elapsedTime - m_lastBotsRefresh) / m_confRandomBotsRefresh;
        for (uint32 i = 0; i < updatesCount; ++i)
        {
            AddOrRemoveBot();
            m_lastBotsRefresh += m_confRandomBotsRefresh;
        }
    }
}

/*
Toutes les X minutes, ajoute ou enleve un bot.
*/
bool PlayerBotMgr::AddOrRemoveBot()
{
    uint32 alea = urand(m_confMinRandomBots, m_confMaxRandomBots);
    /*
    10 --- --- --- --- --- --- --- --- --- --- 20 bots
                NumActuel
    [alea ici : remove    ][    ici, add    ]
    */
    if (alea > m_stats.onlineCount)
        return AddRandomBot();
    else
        return DeleteRandomBot();

}

bool PlayerBotMgr::AddBot(PlayerBotAI* ai)
{
    // Find a correct accountid ?
    std::shared_ptr<PlayerBotEntry> e = std::make_shared<PlayerBotEntry>();
    e->ai.reset(ai);
    e->accountId = GenBotAccountId();
    e->playerGUID = sObjectMgr.GeneratePlayerLowGuid();
    e->customBot = true;
    ai->botEntry = e.get();
    m_bots.insert({ e->playerGUID, e });
    return AddBot(e->playerGUID, false);
}

bool PlayerBotMgr::AddBot(uint32 playerGUID, bool chatBot, PlayerBotAI* pAI)
{
    uint32 accountId = 0;
    auto iter = m_bots.find(playerGUID);
    if (iter == m_bots.end())
        accountId = sObjectMgr.GetPlayerAccountIdByGUID(playerGUID);
    else
        accountId = iter->second->accountId;

    if (!accountId)
    {
        sLog.Out(LOG_BASIC, LOG_LVL_DETAIL, "Player account %u not found...", playerGUID);
        return false;
    }

    if (sWorld.FindSession(accountId))
    {
        sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "[PlayerBotMgr] Account %u is already online!", accountId);
        return false;
    }

    std::shared_ptr<PlayerBotEntry> e;
    if (iter != m_bots.end())
    {
        e = iter->second;

        if (pAI) // new AI
        {
            e->ai.reset(pAI);
            e->customBot = true;
        }
    }
    else
    {
        sLog.Out(LOG_BASIC, LOG_LVL_DETAIL, "[PlayerBotMgr] Adding temporary PlayerBot with GUID %u.", playerGUID);
        e = std::make_shared<PlayerBotEntry>();
        e->state        = PB_STATE_LOADING;
        e->playerGUID   = playerGUID;
        e->chance       = 10;
        e->accountId    = accountId;
        e->isChatBot    = chatBot;
        if (pAI)
        {
            e->ai.reset(pAI);
            e->customBot = true;
        }
        else
        {
            e->ai.reset(new PlayerBotAI(nullptr));
            e->customBot = false;
        }
        m_bots.insert({ playerGUID , e });
    }

    e->ai->botEntry = e.get();
    e->state = PB_STATE_LOADING;
    WorldSession* session = new WorldSession(accountId, nullptr, sAccountMgr.GetSecurity(accountId), 0, LOCALE_enUS);
    session->SetBot(e);
    sWorld.AddSession(session);
    m_stats.loadingCount++;
    if (chatBot)
        AddTempBot(accountId, 20000);

    return true;
}

bool PlayerBotMgr::AddRandomBot()
{
    uint32 alea = urand(0, m_totalChance);
    bool done = false;
    for (auto it = m_bots.begin(); it != m_bots.end() && !done; it++)
    {
        if (it->second->state != PB_STATE_OFFLINE)
            continue;
        if (it->second->customBot)
            continue;
        uint32 chance = it->second->chance;

        if (chance >= alea)
        {
            AddBot(it->first);
            done = true;
        }
        alea -= chance;
    }
    return done;
}

void PlayerBotMgr::AddTempBot(uint32 account, uint32 time)
{
    m_tempBots[account] = time;
}

void PlayerBotMgr::RefreshTempBot(uint32 account)
{
    if (m_tempBots.find(account) != m_tempBots.end())
    {
        uint32& delay = m_tempBots[account];
        if (delay < 1000)
            delay = 1000;
    }
}

bool PlayerBotMgr::DeleteBot(uint32 playerGUID)
{
    auto iter = m_bots.find(playerGUID);
    if (iter == m_bots.end())
        return false;

    return DeleteBot(iter);
}

bool PlayerBotMgr::DeleteBot(std::map<uint64, std::shared_ptr<PlayerBotEntry>>::iterator iter)
{
    if (iter->second->state == PB_STATE_LOADING)
        m_stats.loadingCount--;
    else if (iter->second->state == PB_STATE_ONLINE)
        m_stats.onlineCount--;

    OnBotLogout(iter->second.get());
    return true;
}

bool PlayerBotMgr::DeleteRandomBot()
{
    if (m_stats.onlineCount < 1)
        return false;
    uint32 idDelete = urand(0, m_stats.onlineCount);
    uint32 onlinePassed = 0;
    std::map<uint32, PlayerBotEntry*>::iterator iter;
    for (auto iter = m_bots.begin(); iter != m_bots.end(); iter++)
    {
        if (!iter->second->customBot && !iter->second->isChatBot && iter->second->state == PB_STATE_ONLINE)
        {
            onlinePassed++;
            if (onlinePassed == idDelete)
            {
                OnBotLogout(iter->second.get());
                m_stats.onlineCount--;
                return true;
            }
        }
    }
    return false;
}

uint8 SelectRandomRaceForClass(uint8 playerClass, Team playerTeam)
{
    std::vector<uint32> validRaces;
    for (uint32 raceId = 1; raceId < MAX_RACES; ++raceId)
    {
        if (playerTeam == ALLIANCE)
        {
            if (!((1 << (raceId - 1)) & RACEMASK_ALLIANCE))
                continue;
        }
        else if (playerTeam == HORDE)
        {
            if (!((1 << (raceId - 1)) & RACEMASK_HORDE))
                continue;
        }

        if (sObjectMgr.GetPlayerInfo(raceId, playerClass))
            validRaces.push_back(raceId);
    }

    if (validRaces.empty())
        return 0;

    return SelectRandomContainerElement(validRaces);
}

void PlayerBotMgr::AddBattleBot(BattleGroundQueueTypeId queueType, Team botTeam, uint32 botLevel, bool temporary)
{
    std::vector<uint32> availableClasses = { CLASS_WARRIOR, CLASS_HUNTER, CLASS_ROGUE, CLASS_MAGE, CLASS_WARLOCK, CLASS_PRIEST, CLASS_DRUID };
    if (botTeam == HORDE)
        availableClasses.push_back(CLASS_SHAMAN);
    else
        availableClasses.push_back(CLASS_PALADIN);

    uint8 botClass = SelectRandomContainerElement(availableClasses);
    uint8 botRace = SelectRandomRaceForClass(botClass, botTeam);
    if (!botRace)
        return;

    // Spawn bot on GM Island
    uint32 const instanceId = sMapMgr.GetContinentInstanceId(1, 16224.356f, 16284.763f);
    BattleBotAI* ai = new BattleBotAI(botRace, botClass, botLevel, 1, instanceId, 16224.356f, 16284.763f, 13.175f, 4.56f, queueType, temporary);
    AddBot(ai);

    if (botTeam == ALLIANCE)
    {
        if (temporary)
        {
            sWorld.SendWorldTextToBGAndQueue(LANG_ALLIANCE_BATTLEBOT_TEMP_ADDED, botLevel, queueType, botLevel, queueType);
            sLog.Out(LOG_BG, LOG_LVL_BASIC, "[PlayerBotMgr] Adding temporary level %u alliance battlebot to bg queue %u.", botLevel, queueType);
        }
        else
        {
            sWorld.SendWorldTextToBGAndQueue(LANG_ALLIANCE_BATTLEBOT_ADDED, botLevel, queueType, botLevel, queueType);
            sLog.Out(LOG_BG, LOG_LVL_BASIC, "[PlayerBotMgr] Adding level %u alliance battlebot to bg queue %u.", botLevel, queueType);
        }
    }
    else
    {
        if (temporary)
        {
            sWorld.SendWorldTextToBGAndQueue(LANG_HORDE_BATTLEBOT_TEMP_ADDED, botLevel, queueType, botLevel, queueType);
            sLog.Out(LOG_BG, LOG_LVL_BASIC, "[PlayerBotMgr] Adding temporary level %u horde battlebot to bg queue %u.", botLevel, queueType);
        }
        else
        {
            sWorld.SendWorldTextToBGAndQueue(LANG_HORDE_BATTLEBOT_ADDED, botLevel, queueType, botLevel, queueType);
            sLog.Out(LOG_BG, LOG_LVL_BASIC, "[PlayerBotMgr] Adding level %u horde battlebot to bg queue %u.", botLevel, queueType);
        }
    }
}

void PlayerBotMgr::DeleteBattleBots()
{
    for (auto const& itr : m_bots)
    {
        if (dynamic_cast<BattleBotAI*>(itr.second->ai.get()))
            itr.second->requestRemoval = true;
    }
    m_confBattleBotAutoJoin = false;
}

bool PlayerBotMgr::ForceAccountConnection(WorldSession* sess)
{
    if (sess->GetBot())
        return sess->GetBot()->state != PB_STATE_OFFLINE;

    // Temporary bots.
    return m_tempBots.find(sess->GetAccountId()) != m_tempBots.end();
}

bool PlayerBotMgr::IsPermanentBot(uint32 playerGUID)
{
    auto iter = m_bots.find(playerGUID);
    return iter != m_bots.end();
}

bool PlayerBotMgr::IsChatBot(uint32 playerGuid)
{
    auto iter = m_bots.find(playerGuid);
    return iter != m_bots.end() && iter->second->isChatBot;
}

void PlayerBotMgr::AddAllBots()
{
    for (auto it = m_bots.begin(); it != m_bots.end(); it++)
    {
        if (!it->second->isChatBot && it->second->state == PB_STATE_OFFLINE)
            AddBot(it->first);
    }
}

bool ChatHandler::HandleBotReloadCommand(char * args)
{
    sPlayerBotMgr.Load();
    SendSysMessage("PlayerBot system reloaded");
    return true;
}

bool ChatHandler::HandleBotAddRandomCommand(char * args)
{
    uint32 count = 1;
    char* sCount = strtok((char*)args, " ");
    if (sCount)
        count = uint32(atoi(sCount));
    for (uint32 i = 0; i < count; ++i)
        sPlayerBotMgr.AddRandomBot();
    PSendSysMessage("%u bots added", count);
    return true;
}

bool ChatHandler::HandleBotStopCommand(char * args)
{
    sPlayerBotMgr.DeleteAll();
    SendSysMessage("All the bots have been unloaded.");
    return true;
}

bool ChatHandler::HandleBotAddAllCommand(char * args)
{
    sPlayerBotMgr.AddAllBots();
    SendSysMessage("All bots have been loaded.");
    return true;
}

bool ChatHandler::HandleBotAddCommand(char* args)
{
    uint32 guid = 0;
    char *charname = nullptr;
    if (*args)
    {
        charname = strtok((char*)args, " ");
        if (charname && strcmp(charname, "") == 0)
            return false;

        guid = sObjectMgr.GetPlayerGuidByName(charname).GetCounter();
        if (!guid)
        {
            PSendSysMessage("Player not found : '%s'", charname);
            SetSentErrorMessage(true);
            return false;
        }
    }
    if (!guid || !sPlayerBotMgr.AddBot(guid))
    {
        SendSysMessage("[PlayerBotMgr] Unable to load bot.");
        return true;
    }
    PSendSysMessage("[PlayerBotMgr] Bot added : '%s', GUID=%u", charname ? charname : "NULL", guid);
    return true;
}

bool ChatHandler::HandleBotDeleteCommand(char * args)
{
    char *charname = strtok((char*)args, " ");

    if (!charname || strcmp(charname, "") == 0)
    {
        SendSysMessage("Syntax : $playerName");
        SetSentErrorMessage(true);
        return false;
    }
    uint32 lowGuid = sObjectMgr.GetPlayerGuidByName(charname).GetCounter();
    if (!lowGuid)
    {
        PSendSysMessage("Unable to find player: '%s'", charname);
        SetSentErrorMessage(true);
        return false;
    }
    if (sPlayerBotMgr.DeleteBot(lowGuid))
    {
        PSendSysMessage("Bot %s (GUID:%u) disconnected.", charname, lowGuid);
        return true;
    }
    else
    {
        PSendSysMessage("Bot %s (GUID:%u) : unable to disconnect.", charname, lowGuid);
        SetSentErrorMessage(true);
        return false;
    }
}

bool ChatHandler::HandleBotInfoCommand(char * args)
{
    uint32 online = sWorld.GetActiveSessionCount();

    PlayerBotStats stats = sPlayerBotMgr.GetStats();
    SendSysMessage("-- PlayerBot stats --");
    PSendSysMessage("Min:%u Max:%u Total:%u", stats.confMinOnline, stats.confMaxOnline, stats.totalBots);
    PSendSysMessage("Loading : %u, Online : %u, Chat : %u", stats.loadingCount, stats.onlineCount, stats.onlineChat);
    PSendSysMessage("%up + %ub = %u",
                    (online - stats.onlineCount), stats.onlineCount, online);
    return true;
}

bool ChatHandler::HandleBotStartCommand(char * args)
{
    sPlayerBotMgr.Start();
    return true;
}

bool ChatHandler::PartyBotAddRequirementCheck(Player const* pPlayer, Player const* pTarget)
{
    if (pPlayer->IsTaxiFlying())
    {
        SendSysMessage("Cannot add bots while flying.");
        return false;
    }

    // Spawning bots inside BG will cause server crash on BG end.
    if (pPlayer->InBattleGround())
    {
        SendSysMessage("Cannot add bots inside battlegrounds.");
        return false;
    }

    if (pPlayer->GetGroup() && (pPlayer->GetGroup()->IsFull() || sWorld.getConfig(CONFIG_UINT32_PARTY_BOT_MAX_BOTS) &&
        (pPlayer->GetGroup()->GetMembersCount() - 1 >= sWorld.getConfig(CONFIG_UINT32_PARTY_BOT_MAX_BOTS))))
    {
        SendSysMessage("Cannot add more bots. Group is full.");
        return false;
    }

    if (Map const* pMap = pPlayer->GetMap())
    {
        if (pMap->IsDungeon() &&
            pMap->GetPlayers().getSize() >= pMap->GetMapEntry()->maxPlayers)
        {
            SendSysMessage("Cannot add more bots. Instance is full.");
            return false;
        }
    }

    if (pTarget && pTarget->GetTeam() != pPlayer->GetTeam())
    {
        SendSysMessage("Cannot clone enemy faction characters.");
        return false;
    }

    // Restrictions when the command is made public to avoid abuse.
    if (GetSession()->GetSecurity() <= SEC_PLAYER && !sWorld.getConfig(CONFIG_BOOL_PARTY_BOT_SKIP_CHECKS))
    {
        if (pPlayer->IsDead())
        {
            SendSysMessage("Cannot add bots while dead.");
            return false;
        }

        if (pPlayer->IsInCombat())
        {
            SendSysMessage("Cannot add bots while in combat.");
            return false;
        }

        if (pPlayer->GetMap()->IsDungeon())
        {
            SendSysMessage("Cannot add bots while inside instances.");
            return false;
        }

        // Clone command.
        if (pTarget)
        {
            if (pTarget->IsDead())
            {
                SendSysMessage("Cannot clone dead characters.");
                return false;
            }

            if (pTarget->IsInCombat())
            {
                SendSysMessage("Cannot clone characters that are in combat.");
                return false;
            }

            if (pTarget->GetLevel() > pPlayer->GetLevel() + 10)
            {
                SendSysMessage("Cannot clone higher level characters.");
                return false;
            }
        }
    }

    return true;
}

bool ChatHandler::HandlePartyBotAddCommand(char* args)
{
    Player* pPlayer = m_session->GetPlayer();
    if (!pPlayer)
        return false;

    if (!PartyBotAddRequirementCheck(pPlayer, nullptr))
    {
        SetSentErrorMessage(true);
        return false;
    }

    if (!args)
    {
        SendSysMessage("Incorrect syntax. Expected role or class.");
        SetSentErrorMessage(true);
        return false;
    }

    uint8 botClass = 0;
    uint32 botLevel = pPlayer->GetLevel();
    CombatBotRoles botRole = ROLE_INVALID;

    if (char* arg1 = ExtractArg(&args))
    {
        std::string option = arg1;
        if (option == "warrior")
            botClass = CLASS_WARRIOR;
        else if (option == "paladin" && pPlayer->GetTeam() == ALLIANCE)
            botClass = CLASS_PALADIN;
        else if (option == "hunter")
            botClass = CLASS_HUNTER;
        else if (option == "rogue")
            botClass = CLASS_ROGUE;
        else if (option == "priest")
            botClass = CLASS_PRIEST;
        else if (option == "shaman" && pPlayer->GetTeam() == HORDE)
            botClass = CLASS_SHAMAN;
        else if (option == "mage")
            botClass = CLASS_MAGE;
        else if (option == "warlock")
            botClass = CLASS_WARLOCK;
        else if (option == "druid")
            botClass = CLASS_DRUID;
        else if (option == "dps")
        {
            botClass = PickRandomValue(CLASS_WARRIOR, CLASS_HUNTER, CLASS_ROGUE, CLASS_MAGE, CLASS_WARLOCK);
            botRole = CombatBotBaseAI::IsMeleeDamageClass(botClass) ? ROLE_MELEE_DPS : ROLE_RANGE_DPS;
        }
        else if (option == "healer")
        {
            std::vector<uint32> dpsClasses = { CLASS_PRIEST, CLASS_DRUID };
            if (pPlayer->GetTeam() == HORDE)
                dpsClasses.push_back(CLASS_SHAMAN);
            else
                dpsClasses.push_back(CLASS_PALADIN);
            botClass = SelectRandomContainerElement(dpsClasses);
            botRole = ROLE_HEALER;
        }
        else if (option == "tank")
        {
            botClass = CLASS_WARRIOR;
            botRole = ROLE_TANK;
        }

        // Prevent setting a custom level for bots unless the account is a GM or skipping checks is enabled.
        if (GetSession()->GetSecurity() > SEC_PLAYER || sWorld.getConfig(CONFIG_BOOL_PARTY_BOT_SKIP_CHECKS))
            ExtractUInt32(&args, botLevel);
    }

    if (!botClass)
    {
        SendSysMessage("Incorrect syntax. Expected role or class.");
        SetSentErrorMessage(true);
        return false;
    }

    uint8 botRace = SelectRandomRaceForClass(botClass, pPlayer->GetTeam());
    if (!botRace)
    {
        SendSysMessage("Unable to select race for bot.");
        SetSentErrorMessage(true);
        return false;
    }

    float x, y, z;
    pPlayer->GetNearPoint(pPlayer, x, y, z, 0, 5.0f, frand(0.0f, 6.0f));

    PartyBotAI* ai = new PartyBotAI(pPlayer, nullptr, botRole, botRace, botClass, botLevel, pPlayer->GetMapId(), pPlayer->GetMap()->GetInstanceId(), x, y, z, pPlayer->GetOrientation());
    if (sPlayerBotMgr.AddBot(ai))
        SendSysMessage("New party bot added.");
    else
    {
        delete ai;
        SendSysMessage("Error spawning bot.");
        SetSentErrorMessage(true);
        return false;
    }

    return true;
}

bool ChatHandler::HandlePartyBotCloneCommand(char* args)
{
    Player* pPlayer = m_session->GetPlayer();
    if (!pPlayer)
        return false;

    Player* pTarget = GetSelectedPlayer();
    if (!pTarget)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    if (!PartyBotAddRequirementCheck(pPlayer, pTarget))
    {
        SetSentErrorMessage(true);
        return false;
    }

    uint8 botRace = pTarget->GetRace();
    uint8 botClass = pTarget->GetClass();

    float x, y, z;
    pPlayer->GetNearPoint(pPlayer, x, y, z, 0, 5.0f, frand(0.0f, 6.0f));

    PartyBotAI* ai = new PartyBotAI(pPlayer, pTarget, ROLE_INVALID, botRace, botClass, pPlayer->GetLevel(), pPlayer->GetMapId(), pPlayer->GetMap()->GetInstanceId(), x, y, z, pPlayer->GetOrientation());
    if (sPlayerBotMgr.AddBot(ai))
        SendSysMessage("New party bot added.");
    else
    {
        delete ai;
        SendSysMessage("Error spawning bot.");
        SetSentErrorMessage(true);
        return false;
    }

    return true;
}

bool ChatHandler::HandlePartyBotLoadCommand(char* args)
{
    Player* pPlayer = m_session->GetPlayer();
    if (!pPlayer)
        return false;

    std::string name = ExtractPlayerNameFromLink(&args);
    if (name.empty())
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    ObjectGuid guid = sObjectMgr.GetPlayerGuidByName(name).GetCounter();
    if (!guid)
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    if (sObjectAccessor.FindPlayerNotInWorld(guid))
    {
        SendSysMessage("Player is already online!");
        SetSentErrorMessage(true);
        return false;
    }

    float x, y, z;
    pPlayer->GetNearPoint(pPlayer, x, y, z, 0, 5.0f, frand(0.0f, 6.0f));

    PartyBotAI* pAI = new PartyBotAI(pPlayer, pPlayer->GetMapId(), pPlayer->GetMap()->GetInstanceId(), x, y, z, pPlayer->GetOrientation());

    if (!sPlayerBotMgr.AddBot(guid, false, pAI))
    {
        delete pAI;
        SendSysMessage("Error spawning bot.");
        SetSentErrorMessage(true);
        return false;
    }

    PSendSysMessage("Loading %s as party bot.", name.c_str());
    return true;
}

bool ChatHandler::HandlePartyBotSetRoleCommand(char* args)
{
    if (!args)
        return false;

    Player* pTarget = GetSelectedPlayer();
    if (!pTarget)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    CombatBotRoles role = ROLE_INVALID;
    std::string roleStr = args;

    if (roleStr == "tank")
        role = ROLE_TANK;
    else if (roleStr == "dps")
        role = CombatBotBaseAI::IsMeleeDamageClass(pTarget->GetClass()) ? ROLE_MELEE_DPS : ROLE_RANGE_DPS;
    else if (roleStr == "meleedps")
        role = ROLE_MELEE_DPS;
    else if (roleStr == "rangedps")
        role = ROLE_RANGE_DPS;
    else if (roleStr == "healer")
        role = ROLE_HEALER;

    if (role == ROLE_INVALID)
        return false;

    if (pTarget->AI())
    {
        if (PartyBotAI* pAI = dynamic_cast<PartyBotAI*>(pTarget->AI()))
        {
            pAI->m_role = role;
            pAI->ResetSpellData();
            pAI->PopulateSpellData();
            PSendSysMessage("%s is now a %s.", pTarget->GetName(), roleStr.c_str());
            return true;
        }
    }

    SendSysMessage("Target is not a party bot.");
    SetSentErrorMessage(true);
    return false;
}

bool ChatHandler::HandlePartyBotAttackStartCommand(char* args)
{
    Player* pPlayer = GetSession()->GetPlayer();
    Unit* pTarget = GetSelectedUnit();
    if (!pTarget || (pTarget == pPlayer))
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    Group* pGroup = pPlayer->GetGroup();
    if (!pGroup)
    {
        SendSysMessage("You are not in a group.");
        SetSentErrorMessage(true);
        return false;
    }

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        if (Player* pMember = itr->getSource())
        {
            if (pMember == pPlayer)
                continue;

            if (pMember->AI())
            {
                if (PartyBotAI* pAI = dynamic_cast<PartyBotAI*>(pMember->AI()))
                {
                    if (pMember->IsValidAttackTarget(pTarget))
                        pAI->AttackStart(pTarget);
                }
            }
        }
    }

    PSendSysMessage("All party bots are now attacking %s.", pTarget->GetName());
    return true;
}

void StopPartyBotAttackHelper(PartyBotAI* pAI, Player* pBot)
{
    pBot->AttackStop(true);
    pBot->InterruptNonMeleeSpells(false);
    if (!pBot->IsStopped())
        pBot->StopMoving();
    if (pBot->GetMotionMaster()->GetCurrentMovementGeneratorType() == CHASE_MOTION_TYPE)
        pBot->GetMotionMaster()->Clear();
    if (pAI->m_updateTimer.GetExpiry() < 3000)
        pAI->m_updateTimer.Reset(3000);
}

bool ChatHandler::HandlePartyBotAttackStopCommand(char* args)
{
    Player* pPlayer = GetSession()->GetPlayer();
    Unit* pTarget = GetSelectedUnit();
    if (!pTarget || (pTarget == pPlayer))
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    Group* pGroup = pPlayer->GetGroup();
    if (!pGroup)
    {
        SendSysMessage("You are not in a group.");
        SetSentErrorMessage(true);
        return false;
    }

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        if (Player* pMember = itr->getSource())
        {
            if (pMember == pPlayer)
                continue;

            if (pMember->AI())
            {
                if (PartyBotAI* pAI = dynamic_cast<PartyBotAI*>(pMember->AI()))
                {
                    if (pMember->GetVictim() == pTarget)
                        StopPartyBotAttackHelper(pAI, pMember);
                }
            }
        }
    }

    PSendSysMessage("All party bots have stopped attacking %s.", pTarget->GetName());
    return true;
}

bool ChatHandler::HandlePartyBotAoECommand(char* args)
{
    Player* pPlayer = GetSession()->GetPlayer();
    Unit* pTarget = GetSelectedUnit();
    if (!pTarget || !pPlayer->IsValidAttackTarget(pTarget, true))
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    Group* pGroup = pPlayer->GetGroup();
    if (!pGroup)
    {
        SendSysMessage("You are not in a group.");
        SetSentErrorMessage(true);
        return false;
    }

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        if (Player* pMember = itr->getSource())
        {
            if (pMember == pPlayer)
                continue;

            if (pMember->AI())
            {
                if (PartyBotAI* pAI = dynamic_cast<PartyBotAI*>(pMember->AI()))
                {
                    for (auto const& pSpell : pAI->m_spells.raw.spells)
                    {
                        if (pSpell && pSpell->IsAreaOfEffectSpell() &&
                           !pSpell->IsPositiveSpell() &&
                            pSpell->IsTargetInRange(pMember, pTarget))
                        {
                            if (pMember->GetCurrentSpell(CURRENT_GENERIC_SPELL) &&
                               !pMember->GetCurrentSpell(CURRENT_GENERIC_SPELL)->m_spellInfo->IsAreaOfEffectSpell())
                                pMember->InterruptSpell(CURRENT_GENERIC_SPELL);

                            if (pMember->CastSpell(pTarget, pSpell, false) == SPELL_CAST_OK)
                                break;
                        }
                    }
                }
            }
        }
    }

    PSendSysMessage("All party bots are casting AoE spells at %s.", pTarget->GetName());
    return true;
}

bool ChatHandler::HandlePartyBotStartCastingCommand(char * args)
{
    return HandlePartyBotToggleCastingCommand(true);
}

bool ChatHandler::HandlePartyBotStopCastingCommand(char * args)
{
    return HandlePartyBotToggleCastingCommand(false);
}

bool ChatHandler::HandlePartyBotToggleCastingCommand(bool allowCasting)
{
    Player* pPlayer = GetSession()->GetPlayer();
    Player* pTarget = GetSelectedPlayer();

    if (pTarget && (pTarget != pPlayer))
    {
        if (pTarget->AI())
        {
            if (PartyBotAI* pAI = dynamic_cast<PartyBotAI*>(pTarget->AI()))
            {
                if (allowCasting)
                {
                    pAI->m_preventCasting = false;
                    PSendSysMessage("%s will be allowed to cast spells.", pTarget->GetName());
                }
                else
                {
                    pAI->m_preventCasting = true;
                    pTarget->InterruptNonMeleeSpells(false);
                    PSendSysMessage("%s will no longer cast spells.", pTarget->GetName());
                }
                return true;
            }
        }
        SendSysMessage("Target is not a party bot.");
        SetSentErrorMessage(true);
        return false;
    }

    Group* pGroup = pPlayer->GetGroup();
    if (!pGroup)
    {
        SendSysMessage("You are not in a group.");
        SetSentErrorMessage(true);
        return false;
    }

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        if (Player* pMember = itr->getSource())
        {
            if (pMember == pPlayer)
                continue;

            if (pMember->AI())
            {
                if (PartyBotAI* pAI = dynamic_cast<PartyBotAI*>(pMember->AI()))
                {
                    if (allowCasting)
                    {
                        pAI->m_preventCasting = false;
                    }
                    else
                    {
                        pAI->m_preventCasting = true;
                        pTarget->InterruptNonMeleeSpells(false);
                    }
                }
            }
        }
    }

    if (allowCasting)
        SendSysMessage("All bots are now allowed to cast spells again.");
    else
        SendSysMessage("All bots are now forbidden from casting spells.");

    return true;
}

static std::map<std::string, RaidTargetIcon> raidTargetIcons =
{
    { "star",     RAID_TARGET_ICON_STAR     },
    { "circle",   RAID_TARGET_ICON_CIRCLE   },
    { "diamond",  RAID_TARGET_ICON_DIAMOND  },
    { "triangle", RAID_TARGET_ICON_TRIANGLE },
    { "moon",     RAID_TARGET_ICON_MOON     },
    { "square",   RAID_TARGET_ICON_SQUARE   },
    { "cross",    RAID_TARGET_ICON_CROSS    },
    { "skull",    RAID_TARGET_ICON_SKULL    },
};

bool ChatHandler::HandlePartyBotControlMarkCommand(char* args)
{
    std::string mark = args;
    auto itrMark = raidTargetIcons.find(mark);
    if (itrMark == raidTargetIcons.end())
    {
        SendSysMessage("Unknown target mark. Valid names are: star, circle, diamond, triangle, moon, square, cross, skull");
        SetSentErrorMessage(true);
        return false;
    }

    Player* pPlayer = GetSession()->GetPlayer();
    Player* pTarget = GetSelectedPlayer();

    if (pTarget && (pTarget != pPlayer))
    {
        if (pTarget->AI())
        {
            if (PartyBotAI* pAI = dynamic_cast<PartyBotAI*>(pTarget->AI()))
            {
                PSendSysMessage("%s will crowd control %s.", pTarget->GetName(), args);
                pAI->m_marksToCC.push_back(itrMark->second);
                return true;
            }
        }
        SendSysMessage("Target is not a party bot.");
        SetSentErrorMessage(true);
        return false;
    }

    Group* pGroup = pPlayer->GetGroup();
    if (!pGroup)
    {
        SendSysMessage("You are not in a group.");
        SetSentErrorMessage(true);
        return false;
    }

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        if (Player* pMember = itr->getSource())
        {
            if (pMember == pPlayer)
                continue;

            if (pMember->AI())
            {
                if (PartyBotAI* pAI = dynamic_cast<PartyBotAI*>(pMember->AI()))
                {
                    pAI->m_marksToCC.push_back(itrMark->second);
                }
            }
        }
    }

    PSendSysMessage("All party bots will crowd control %s.", args);
    return true;
}

bool ChatHandler::HandlePartyBotFocusMarkCommand(char* args)
{
    std::string mark = args;
    auto itrMark = raidTargetIcons.find(mark);
    if (itrMark == raidTargetIcons.end())
    {
        SendSysMessage("Unknown target mark. Valid names are: star, circle, diamond, triangle, moon, square, cross, skull");
        SetSentErrorMessage(true);
        return false;
    }

    Player* pPlayer = GetSession()->GetPlayer();
    Player* pTarget = GetSelectedPlayer();

    if (pTarget && (pTarget != pPlayer))
    {
        if (pTarget->AI())
        {
            if (PartyBotAI* pAI = dynamic_cast<PartyBotAI*>(pTarget->AI()))
            {
                if (std::find(pAI->m_marksToFocus.begin(), pAI->m_marksToFocus.end(), itrMark->second) != pAI->m_marksToFocus.end())
                {
                    PSendSysMessage("%s already have focus %s.", pTarget->GetName(), args);
                    return false;
                }

                PSendSysMessage("%s will focus %s.", pTarget->GetName(), args);
                pAI->m_marksToFocus.push_back(itrMark->second);
                return true;
            }
        }
        SendSysMessage("Target is not a party bot.");
        SetSentErrorMessage(true);
        return false;
    }

    Group* pGroup = pPlayer->GetGroup();
    if (!pGroup)
    {
        SendSysMessage("You are not in a group.");
        SetSentErrorMessage(true);
        return false;
    }

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        if (Player* pMember = itr->getSource())
        {
            if (pMember == pPlayer)
                continue;

            if (pMember->AI())
            {
                if (PartyBotAI* pAI = dynamic_cast<PartyBotAI*>(pMember->AI()))
                {
                    if (std::find(pAI->m_marksToFocus.begin(), pAI->m_marksToFocus.end(), itrMark->second) != pAI->m_marksToFocus.end())
                    {
                        // Already have focus mark
                        continue;
                    }
                    pAI->m_marksToFocus.push_back(itrMark->second);
                }
            }
        }
    }

    PSendSysMessage("All party bots will focus %s.", args);
    return true;
}

bool ChatHandler::HandlePartyBotClearMarksCommand(char* args)
{
    Player* pPlayer = GetSession()->GetPlayer();
    Player* pTarget = GetSelectedPlayer();

    if (pTarget && (pTarget != pPlayer))
    {
        if (pTarget->AI())
        {
            if (PartyBotAI* pAI = dynamic_cast<PartyBotAI*>(pTarget->AI()))
            {
                PSendSysMessage("All mark assignments cleared for %s.", pTarget->GetName());
                pAI->m_marksToCC.clear();
                pAI->m_marksToFocus.clear();
                return true;
            }
        }
        SendSysMessage("Target is not a party bot.");
        SetSentErrorMessage(true);
        return false;
    }

    Group* pGroup = pPlayer->GetGroup();
    if (!pGroup)
    {
        SendSysMessage("You are not in a group.");
        SetSentErrorMessage(true);
        return false;
    }

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        if (Player* pMember = itr->getSource())
        {
            if (pMember == pPlayer)
                continue;

            if (pMember->AI())
            {
                if (PartyBotAI* pAI = dynamic_cast<PartyBotAI*>(pMember->AI()))
                {
                    pAI->m_marksToCC.clear();
                    pAI->m_marksToFocus.clear();
                }
            }
        }
    }

    SendSysMessage("Mark assignments cleared for all bots.");
    return true;
}

bool HandlePartyBotComeToMeHelper(Player* pBot, Player* pPlayer)
{
    if (pBot->AI() && pBot->IsAlive() && pBot->IsInMap(pPlayer) && !pBot->HasUnitState(UNIT_STATE_NO_FREE_MOVE))
    {
        if (PartyBotAI* pAI = dynamic_cast<PartyBotAI*>(pBot->AI()))
        {
            if (pBot->GetVictim())
                StopPartyBotAttackHelper(pAI, pBot);

            if (pBot->GetStandState() != UNIT_STAND_STATE_STAND)
                pBot->SetStandState(UNIT_STAND_STATE_STAND);

            pBot->InterruptSpellsWithInterruptFlags(SPELL_INTERRUPT_FLAG_MOVEMENT);
            pBot->MonsterMove(pPlayer->GetPositionX(), pPlayer->GetPositionY(), pPlayer->GetPositionZ());
            return true;
        }
    }

    return false;
}

bool ChatHandler::HandlePartyBotComeToMeCommand(char* args)
{
    Player* pPlayer = GetSession()->GetPlayer();
    Player* pTarget = GetSelectedPlayer();

    bool ok = false;

    if (pTarget && pTarget != pPlayer)
    {
        if (ok = HandlePartyBotComeToMeHelper(pTarget, pPlayer))
            PSendSysMessage("%s is coming to your position.", pTarget->GetName());
        else
            PSendSysMessage("%s is not a party bot or it cannot move.", pTarget->GetName());
        return ok;
    }
    else if (Group* pGroup = pPlayer->GetGroup())
    {
        bool ok = false;
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            if (Player* pMember = itr->getSource())
            {
                if (pMember == pPlayer)
                    continue;

                ok = HandlePartyBotComeToMeHelper(pMember, pPlayer) || ok;
            }
        }

        if (ok)
            SendSysMessage("All party bots are coming to your position.");
        else
            SendSysMessage("There are no party bots in the group or they cannot move.");
        return ok;
    }

    SendSysMessage("You are not in a group.");
    SetSentErrorMessage(true);
    return false;
}

bool HandlePartyBotUseGObjectHelper(Player* pTarget, GameObject* pGo)
{
    if (pTarget->AI())
    {
        if (PartyBotAI* pAI = dynamic_cast<PartyBotAI*>(pTarget->AI()))
        {
            if (pTarget->IsWithinDist(pGo, INTERACTION_DISTANCE))
            {
                pGo->Use(pTarget);
                return true;
            }
        }
    }

    return false;
}

bool ChatHandler::HandlePartyBotUseGObjectCommand(char* args)
{
    Player* pPlayer = GetSession()->GetPlayer();
    Player* pTarget = GetSelectedPlayer();

    GameObject* pGo = getSelectedGameObject();
    if (!pGo)
    {
        SendSysMessage(LANG_COMMAND_NOGAMEOBJECTFOUND);
        return false;
    }

    bool ok = false;

    if (pTarget && pTarget != pPlayer)
    {
        if (ok = HandlePartyBotUseGObjectHelper(pTarget, pGo))
            PSendSysMessage("%s has used the object.", pTarget->GetName());
        else
            PSendSysMessage("%s is not in range or is not a party bot.", pTarget->GetName());
        return ok;
    }
    else if (Group* pGroup = pPlayer->GetGroup())
    {
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            if (Player* pMember = itr->getSource())
                ok = HandlePartyBotUseGObjectHelper(pMember, pGo) || ok;
        }

        if (ok)
            SendSysMessage("All party bots in range have used the object.");
        else
            SendSysMessage("There are no party bots in range of the object.");
        return ok;
    }

    SendSysMessage("You are not in a group.");
    SetSentErrorMessage(true);
    return false;
}

bool HandlePartyBotPauseApplyHelper(Player* pTarget, uint32 duration)
{
    if (pTarget->AI())
    {
        if (PartyBotAI* pAI = dynamic_cast<PartyBotAI*>(pTarget->AI()))
        {
            pAI->m_updateTimer.Reset(duration);

            if (duration)
            {
                pTarget->StopMoving();
                pTarget->GetMotionMaster()->MoveIdle();
            }

            return true;
        }
    }

    return false;
}

bool ChatHandler::HandlePartyBotPauseHelper(char* args, bool pause)
{
    bool all = false;
    uint32 duration = 0;
    if (char* arg1 = ExtractArg(&args))
    {
        if (!(all = (strcmp(arg1, "all") == 0)) && pause)
            duration = atoi(arg1);

        if (char* arg2 = ExtractArg(&args))
        {
            if (!duration && pause)
                duration = atoi(arg2);
            else if (!all)
                all = strcmp(arg2, "all") == 0;
        }
    }

    if (pause && !duration)
        duration = 5 * MINUTE * IN_MILLISECONDS;

    if (all)
    {
        Player* pPlayer = GetSession()->GetPlayer();
        Group* pGroup = pPlayer->GetGroup();
        if (!pGroup)
        {
            SendSysMessage("You are not in a group.");
            SetSentErrorMessage(true);
            return false;
        }

        bool success = false;
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            if (Player* pMember = itr->getSource())
            {
                if (pMember == pPlayer)
                    continue;

                if (HandlePartyBotPauseApplyHelper(pMember, duration))
                    success = true;
            }
        }

        if (success)
        {
            if (pause)
                PSendSysMessage("All party bots paused for %u seconds.", (duration / IN_MILLISECONDS));
            else
                SendSysMessage("All party bots unpaused.");
        }
        else
            SendSysMessage("No party bots in group.");
    }
    else
    {
        Player* pTarget = GetSelectedPlayer();
        if (!pTarget)
        {
            SendSysMessage(LANG_NO_CHAR_SELECTED);
            SetSentErrorMessage(true);
            return false;
        }

        if (HandlePartyBotPauseApplyHelper(pTarget, duration))
        {
            if (pause)
                PSendSysMessage("%s paused for %u seconds.", pTarget->GetName(), (duration / IN_MILLISECONDS));
            else
                PSendSysMessage("%s unpaused.", pTarget->GetName());
        }

        else
            SendSysMessage("Target is not a party bot.");
    }

    return true;
}

bool ChatHandler::HandlePartyBotPauseCommand(char* args)
{
    return HandlePartyBotPauseHelper(args, true);
}

bool ChatHandler::HandlePartyBotUnpauseCommand(char* args)
{
    return HandlePartyBotPauseHelper(args, false);
}

bool ChatHandler::HandlePartyBotPullCommand(char* args)
{
    Player* pPlayer = GetSession()->GetPlayer();
    Unit* pTarget = GetSelectedUnit();
    if (!pTarget || !pPlayer->IsValidAttackTarget(pTarget, true))
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    Group* pGroup = pPlayer->GetGroup();
    if (!pGroup)
    {
        SendSysMessage("You are not in a group.");
        SetSentErrorMessage(true);
        return false;
    }

    uint32 duration;
    if (!ExtractUInt32(&args, duration))
        duration = 10 * IN_MILLISECONDS;

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        if (Player* pMember = itr->getSource())
        {
            if (pMember == pPlayer)
                continue;

            if (pMember->AI())
            {
                if (PartyBotAI* pAI = dynamic_cast<PartyBotAI*>(pMember->AI()))
                {
                    if (pAI->m_role == ROLE_MELEE_DPS || pAI->m_role == ROLE_RANGE_DPS)
                    {
                        HandlePartyBotPauseApplyHelper(pMember, duration);
                        continue;
                    }
                    else if (pAI->m_role == ROLE_TANK)
                    {
                        if (pMember->IsValidAttackTarget(pTarget))
                            pAI->AttackStart(pTarget);
                    }
                }
            }
        }
    }

    PSendSysMessage("Tank party bots are pulling %s, DPS party bots are paused for %d seconds.", pTarget->GetName(), (duration / IN_MILLISECONDS));
    return true;
}

bool ChatHandler::HandlePartyBotUnequipCommand(char* args)
{
    Player* pTarget = GetSelectedPlayer();
    if (!pTarget)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    char* cId = ExtractKeyFromLink(&args, "Hitem");
    if (!cId)
        return false;

    uint32 itemId = 0;
    if (!ExtractUInt32(&cId, itemId))
        return false;

    uint32 count = pTarget->GetItemCount(itemId, false);
    if (!count)
    {
        SendSysMessage("Target does not have that item.");
        SetSentErrorMessage(true);
        return false;
    }

    if (pTarget->AI())
    {
        if (PartyBotAI* pAI = dynamic_cast<PartyBotAI*>(pTarget->AI()))
        {
            pTarget->DestroyItemCount(itemId, count, true);
            return true;
        }
    }

    SendSysMessage("Target is not a party bot.");
    SetSentErrorMessage(true);
    return false;
}

bool ChatHandler::HandlePartyBotRemoveCommand(char* args)
{
    Player* pTarget = GetSelectedPlayer();
    if (!pTarget)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    if (pTarget->AI())
    {
        if (PartyBotAI* pAI = dynamic_cast<PartyBotAI*>(pTarget->AI()))
        {
            pAI->botEntry->requestRemoval = true;
            return true;
        }
    }

    SendSysMessage("Target is not a party bot.");
    SetSentErrorMessage(true);
    return false;
}

bool ChatHandler::HandleBattleBotAddAlteracCommand(char* args)
{
    return HandleBattleBotAddCommand(args, BATTLEGROUND_QUEUE_AV);
}

bool ChatHandler::HandleBattleBotAddArathiCommand(char* args)
{
    return HandleBattleBotAddCommand(args, BATTLEGROUND_QUEUE_AB);
}

bool ChatHandler::HandleBattleBotAddWarsongCommand(char* args)
{
    return HandleBattleBotAddCommand(args, BATTLEGROUND_QUEUE_WS);
}

bool ChatHandler::HandleBattleBotAddCommand(char* args, uint8 bg)
{
    if (!*args)
    {
        SendSysMessage("Incorrect syntax. Expected faction");
        SetSentErrorMessage(true);
        return false;
    }

    Team botTeam = HORDE;
    uint32 botLevel = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);
    bool isTemporary = false;
    std::string option;
    if (char* arg1 = ExtractArg(&args))
    {
        option = arg1;
        if (option == "horde")
            botTeam = HORDE;
        else if (option == "alliance")
            botTeam = ALLIANCE;
        else
        {
            SendSysMessage("Incorrect syntax. Expected faction");
            SetSentErrorMessage(true);
            return false;
        }


        ExtractUInt32(&args, botLevel);


        if (char* tempStr = ExtractArg(&args))
        {
            if (strcmp(tempStr, "temp") == 0)
                isTemporary = true;
        }
    }

    sPlayerBotMgr.AddBattleBot(BattleGroundQueueTypeId(bg), botTeam, botLevel, isTemporary);
    return true;
}

bool ChatHandler::HandleBattleBotRemoveCommand(char* args)
{
    Player* pTarget = GetSelectedPlayer();
    if (!pTarget)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    if (pTarget->AI())
    {
        if (BattleBotAI* pAI = dynamic_cast<BattleBotAI*>(pTarget->AI()))
        {
            pAI->botEntry->requestRemoval = true;
            return true;
        }
    }

    SendSysMessage("Target is not a battle bot.");
    SetSentErrorMessage(true);
    return false;
}

bool ChatHandler::HandleBattleBotRemoveAllCommand(char* args)
{
    sPlayerBotMgr.DeleteBattleBots();
    SendSysMessage("Removed all battlebots.");
    return true;
}

#define SPELL_RED_GLOW 20370

void ShowBattleBotPathHelper(Map* pMap, BattleBotPath* pPath, uint32 id)
{
    for (const auto& point : *pPath)
    {
        if (Creature* pWaypoint = pMap->SummonCreature(VISUAL_WAYPOINT, point.x, point.y, point.z, 0.0f, TEMPSUMMON_TIMED_DESPAWN, 120000, true))
        {
            // Show path id as level to distinguish individual paths.
            pWaypoint->SetUInt32Value(UNIT_FIELD_LEVEL, id);

            // Mark points that have script attached.
            if (point.pFunc)
                pWaypoint->CastSpell(pWaypoint, SPELL_RED_GLOW, true);
        }
    }
}

bool ChatHandler::HandleBattleBotShowPathCommand(char* args)
{
    Player* pTarget = GetSelectedPlayer();
    if (!pTarget)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    if (pTarget->AI())
    {
        if (BattleBotAI* pAI = dynamic_cast<BattleBotAI*>(pTarget->AI()))
        {
            if (pAI->m_currentPath)
                ShowBattleBotPathHelper(pTarget->GetMap(), pAI->m_currentPath, 1);
            else
                SendSysMessage("Target is not following a path.");

            return true;
        }
    }

    SendSysMessage("Target is not a battle bot.");
    SetSentErrorMessage(true);
    return false;
}

bool ChatHandler::HandleBattleBotShowAllPathsCommand(char* args)
{
    Player* pPlayer = m_session->GetPlayer();
    BattleGround* pBG = pPlayer->GetBattleGround();
    if (!pBG)
    {
        SendSysMessage("You are not in a battleground.");
        return false;
    }

    std::vector<BattleBotPath*> const* pPaths;

    switch (pBG->GetTypeID())
    {
        case BATTLEGROUND_AB:
        {
            pPaths = &vPaths_AB;
            break;
        }
        case BATTLEGROUND_AV:
        {
            pPaths = &vPaths_AV;
            break;
        }
        case BATTLEGROUND_WS:
        {
            pPaths = &vPaths_WS;
            break;
        }
        default:
            return false;
    }

    uint32 id = 1;
    for (const auto& path : *pPaths)
    {
        ShowBattleBotPathHelper(pPlayer->GetMap(), path, id++);
    }

    PSendSysMessage("Showing %u paths for battleground.", id);
    return true;
}

std::vector<WorldBotConfig::TravelNode> PlayerBotMgr::FindAStarPath(uint8 faction, uint32 startMapId, Position posStart, uint32 endMapId, Position posEnd){
    std::vector<WorldBotConfig::TravelNode> result;
    
    if (startMapId != endMapId){
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[PlayerBotMgr]::FindAStarPath : start and end maps are different.");
        return result;
    }
    
    // Собираем все узлы на целевой карте
    std::vector<WorldBotConfig::TravelNode> mapNodes;
    for (auto node : m_worldBotConfig.travelNode){
        std::regex pattern;
        if (faction == HORDE)
            pattern = "alliance";
        else if (faction == ALLIANCE)
            pattern = "horde";

        if (std::regex_search(node.name, pattern))
            continue;

        if (node.map_id == endMapId)
            mapNodes.push_back(node);
    }
    
    if (mapNodes.empty()){
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[PlayerBotMgr]::FindAStarPath : no nodes found on target map.");
        return result;
    }
    
    // Находим ближайший стартовый и целевой узлы
    float minDistStart = std::numeric_limits<float>::max();
    float minDistEnd = std::numeric_limits<float>::max();
    WorldBotConfig::TravelNode startNode;
    WorldBotConfig::TravelNode goalNode;
    
    for (auto node : mapNodes){
        Vector3 startVec(Vector3({node.x, node.y, node.z}) - Vector3({posStart.x, posStart.y, posStart.z}));
        Vector3 endVec(Vector3({node.x, node.y, node.z}) - Vector3({posEnd.x, posEnd.y, posEnd.z}));
        
        float lengthToStart = startVec.length();
        if (lengthToStart < minDistStart){
            minDistStart = lengthToStart;
            startNode = node;
        }
        
        float lengthToEnd = endVec.length();
        if (lengthToEnd < minDistEnd){
            minDistEnd = lengthToEnd;
            goalNode = node;
        }
    }
    
    if (!startNode.linked || !goalNode.linked){
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[PlayerBotMgr]::FindAStarPath : start or goal node has no links.");
        return result;
    }
    
    // A* алгоритм
    struct AStarNode {
        WorldBotConfig::TravelNode node;
        float g; // стоимость от старта до текущего узла
        float h; // эвристика (расстояние до цели)
        float f; // g + h
        uint32 parentId;
        
        AStarNode(WorldBotConfig::TravelNode n, float gCost, float hCost, uint32 pId) 
            : node(n), g(gCost), h(hCost), f(gCost + hCost), parentId(pId) {}
        
        bool operator>(const AStarNode& other) const {
            return f > other.f;
        }
    };
    
    std::unordered_map<uint32, float> gScore; // стоимость от старта до узла
    std::unordered_map<uint32, uint32> cameFrom; // предыдущий узел в оптимальном пути
    std::unordered_map<uint32, bool> closedSet; // уже обработанные узлы
    
    // Приоритетная очередь (минимальная куча)
    std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> openSet;
    
    // Инициализация
    gScore[startNode.id] = 0.0f;
    float startH = CalculateHeuristic(startNode, goalNode);
    openSet.push(AStarNode(startNode, 0.0f, startH, 0));
    
    while (!openSet.empty()){
        AStarNode current = openSet.top();
        openSet.pop();
        
        // Проверяем, не обработали ли мы этот узел уже с лучшей стоимостью
        if (closedSet[current.node.id])
            continue;
            
        closedSet[current.node.id] = true;
        
        // Достигли цели?
        if (current.node.id == goalNode.id){
            // Восстанавливаем путь
            std::vector<WorldBotConfig::TravelNode> path;
            uint32 currentNodeId = goalNode.id;
            while (currentNodeId != 0){
                // path.push_back(m_worldBotConfig.travelNode[currentNodeId]);
                path.push_back(GetTravelNode(currentNodeId));
                currentNodeId = cameFrom[currentNodeId];
            }
            std::reverse(path.begin(), path.end());
            result = path;
            break;
        }
        
        // Получаем соседей
        std::vector<WorldBotConfig::TravelNodeLink> neighbours = GetTravelNodeLinksById(current.node.id);
        
        for (auto& nodeLink : neighbours){
            if (closedSet[nodeLink.to_node_id])
                continue;
                
            // WorldBotConfig::TravelNode neighborNode = m_worldBotConfig.travelNode[nodeLink.to_node_id];
            WorldBotConfig::TravelNode neighborNode = GetTravelNode(nodeLink.to_node_id);
            float tentativeG = current.g + nodeLink.distance;
            
            // Если нашли лучший путь к соседу
            if (!gScore.count(neighborNode.id) || tentativeG < gScore[neighborNode.id]){
                cameFrom[neighborNode.id] = current.node.id;
                gScore[neighborNode.id] = tentativeG;
                float heuristic = CalculateHeuristic(neighborNode, goalNode);
                openSet.push(AStarNode(neighborNode, tentativeG, heuristic, current.node.id));
            }
        }
    }
    
    if (result.empty()){
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[PlayerBotMgr]::FindAStarPath : failed to find path from node %d to node %d.", startNode.id, goalNode.id);
    }
    
    return result;
}

std::vector<WorldBotConfig::TravelNode> PlayerBotMgr::FindGreedyPath(uint8 faction, uint32 startMapId, Position posStart, uint32 endMapId, Position posEnd){
    std::vector<WorldBotConfig::TravelNode> result;
    if (startMapId == endMapId){
        std::vector<WorldBotConfig::TravelNode> mapNodes;
        for (auto node: m_worldBotConfig.travelNode){
            if (node.map_id == endMapId)
                mapNodes.push_back(node);
        }

        float minDistStart = 1000000.0f;
        float minDistEnd = 1000000.0f;
        WorldBotConfig::TravelNode currentNode;
        WorldBotConfig::TravelNode targetNode;
        for (auto node: mapNodes){
            Vector3 startVec({node.x - posStart.x, node.y - posStart.y, node.z - posStart.z});
            Vector3 endVec({node.x - posEnd.x, node.y - posEnd.y, node.z - posEnd.z});
            float length = startVec.length();
            if (length < minDistStart){
                minDistStart = length;
                currentNode = node;
            }
            length = endVec.length();
            if (length < minDistEnd){
                minDistEnd = length;
                targetNode = node;
            }
        }

        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[PlayerBotMgr]::FindGreedyPath : minDistStart = %f, minDistEnd = %f.", minDistStart, minDistEnd);

        if (!currentNode.linked || !targetNode.linked){
            sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[PlayerBotMgr]::FindGreedyPath : can't find any closest start node or closest to posEnd target node.");
            result.clear();
            return result;
        }

        result.push_back(currentNode);
        std::vector<uint32> closedNodes {currentNode.id};

        float straightDistance = Vector3({currentNode.x - targetNode.x, currentNode.y - targetNode.y, currentNode.z - targetNode.z}).length();
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[WB]::___ Straight distance from #%d to #%d = %f.", currentNode.id, targetNode.id, straightDistance);
        if (straightDistance > 500.0f){
            std::vector<WorldBotConfig::TravelNode> allFlightMasters;
            std::regex pattern;
            if (faction == ALLIANCE){
                pattern = "flightMaster alliance";
                sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "___ Selected alliance flightmasters");
            }
            else if (faction == HORDE){
                pattern = "flightMaster horde";
                sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "___ Selected horde flightmasters.");
            }
            else
                pattern = "flightMaster";
            for (auto node: m_worldBotConfig.travelNode){
                if (std::regex_search(node.name, pattern))
                    allFlightMasters.push_back(node);
            }

            WorldBotConfig::TravelNode closestFlightMasterNode;
            if (!allFlightMasters.empty()){
                sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[WB]::FLIGHTMASTERS ARRAY IS FOUND!");
                float minDistToFlightMaster = 400.0f;
                for (auto node: allFlightMasters){
                    float distToFlightMaster = Vector3({node.x-currentNode.x, node.y-currentNode.y, node.z-currentNode.z}).length();
                    if (distToFlightMaster < minDistToFlightMaster){
                        minDistToFlightMaster = distToFlightMaster;
                        closestFlightMasterNode = node;
                    }
                }
            }
            else
                sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[WB]::FLIGHTMASTER IS NOT FOUND.");

            if (closestFlightMasterNode.linked){
                sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[WB]::FLIGHTMASTER -- Selected flightmaster node #%d; name \"%s\".", closestFlightMasterNode.id, closestFlightMasterNode.name.c_str());
            }
        }

        while (currentNode != targetNode){
            std::vector<WorldBotConfig::TravelNodeLink> travelLinks = GetTravelNodeLinksById(currentNode.id);
            std::vector<WorldBotConfig::TravelNodeLink> neighbours;

            for (auto nodeLink: travelLinks){
                bool skip = false;
                for (auto id: closedNodes){
                    if (id == nodeLink.to_node_id){
                        skip = true;
                        break;
                    }
                }
                if (skip || nodeLink.type == PATH_TYPE_TRANSPORT)
                    continue;
                
                neighbours.push_back(nodeLink);
            }

            if (neighbours.empty()){
                sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[PlayerBotMgr]::FindGreedyPath : current node #%d is leaf (has no links), but targetNode is not reached.", currentNode.id);
                // result.clear();
                // return result;
                if (!result.empty()){
                    result.pop_back();
                    currentNode = result.back();
                    continue;
                }
                else{
                    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[PlayerBotMgr]::FindGreedyPath : nothing to pop from `result` vector. No path found.");
                    return result;
                }
            }

            float distance = 1000000.0f;
            // WorldBotConfig::TravelNode nextNode;
            sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "For node #%d:\t", currentNode.id);
            for (auto nodeLink: neighbours){
                // skip nodes that are already checked
                // bool skip = false;
                // for (auto node: closedNodes)
                //     if (nodeLink.to_node_id == node){
                //         skip = true;
                //         break;
                //     }
                // if (skip)
                //     continue;

                WorldBotConfig::TravelNode node = m_worldBotConfig.travelNode[nodeLink.to_node_id];
                // used for heuristic function h(x)
                float length = Vector3({targetNode.x - node.x, targetNode.y - node.y, targetNode.z - node.z}).length();
                if (nodeLink.distance + length < distance){
                    distance = nodeLink.distance + length;
                    currentNode = node;
                }
                sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "handling neighbour #%d", node.id);
            }
            closedNodes.push_back(currentNode.id);
            result.push_back(currentNode);
            sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "\n\tselected node #%d; name \"%s\".", currentNode.id, currentNode.name.c_str());
        }
    }

    return result;
}

// Вспомогательный метод для эвристики
float PlayerBotMgr::CalculateHeuristic(const WorldBotConfig::TravelNode& from, const WorldBotConfig::TravelNode& to){
    Vector3 fromVec(from.x, from.y, from.z);
    Vector3 toVec(to.x, to.y, to.z);
    return (toVec - fromVec).length();
}

std::vector<WorldBotConfig::TravelNodeLink> PlayerBotMgr::GetTravelNodeLinksById(uint32 id) const {
    std::vector<WorldBotConfig::TravelNodeLink> result;
    for (auto nodeLink: m_worldBotConfig.travelNodeLink){
        if (nodeLink.node_id == id)
            result.push_back(nodeLink);
    }

    return result;
}

WorldBotConfig::TravelNodeLink PlayerBotMgr::GetTravelNodeLink(uint32 node_id, uint32 to_node_id){
    WorldBotConfig::TravelNodeLink result;
    for (auto entry: m_worldBotConfig.travelNodeLink){
        if (entry.node_id == node_id && entry.to_node_id == to_node_id)
            result = entry;
    }
    return result;
}

std::vector<WorldBotConfig::TravelNodePath> PlayerBotMgr::GetTravelNodePathVector(uint32 mapId, uint32 node_id, uint32 to_node_id) const {
    std::vector<WorldBotConfig::TravelNodePath> result;
    for (auto entry: m_worldBotConfig.travelNodePath){
        if (mapId == entry.map_id && node_id == entry.node_id && to_node_id == entry.to_node_id){
            result.push_back(entry);
            // sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "____ added travel node path entry with mapId #%d, nodeId #%d, to_node_id #%d.", entry.map_id, entry.node_id, entry.to_node_id);
        }
    }

    std::sort(result.begin(), result.end(), [](const WorldBotConfig::TravelNodePath& a, const WorldBotConfig::TravelNodePath& b){
        return a.nr < b.nr;
    });

    return result;
}

std::vector<WorldBotConfig::TravelNodePath> PlayerBotMgr::GetRecordNodePathVector(uint32 mapId, uint32 node_id, uint32 to_node_id) const{
    std::vector<WorldBotConfig::TravelNodePath> result;
    for (auto entry: m_worldBotConfig.recordNodePaths){
        if (mapId == entry.map_id && node_id == entry.node_id && to_node_id == entry.to_node_id){
            result.push_back(entry);
            // sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "____ added travel node path entry with mapId #%d, nodeId #%d, to_node_id #%d.", entry.map_id, entry.node_id, entry.to_node_id);
        }
    }

    std::sort(result.begin(), result.end(), [](const WorldBotConfig::TravelNodePath& a, const WorldBotConfig::TravelNodePath& b){
        return a.nr < b.nr;
    });

    return result;
}

WorldBotConfig::TaxiFlightNodeInfo PlayerBotMgr::GetTaxiFlightNodeInfoById(uint32 id) const {
    WorldBotConfig::TaxiFlightNodeInfo result;
    for (auto entry: m_worldBotConfig.taxiNodesInfo){
        if (entry.id == id){
            result = entry;
        }
    }
    return result;
}

std::vector<WorldBotConfig::TravelNode> PlayerBotMgr::GetTravelNodesFilterByRegex(std::string regex){
    std::vector<WorldBotConfig::TravelNode> result;
    for (auto node: m_worldBotConfig.travelNode){
        std::regex pattern {regex};
        if (std::regex_search(node.name, pattern)){
            result.push_back(node);
        }
    }
    return result;
}

void PlayerBotMgr::SpawnWorldBots(uint8 lvlMin, uint8 lvlMax, std::vector<uint8> allowedRaces, std::vector<uint8> allowedClasses, std::string location, uint32 count){
    
}

bool ChatHandler::HandleWorldBotAddCommand(char * args){
    Player* pPlayer = m_session->GetPlayer();
    if (!pPlayer)
        return false;

    uint8 botRace = SelectRandomRaceForClass((uint8)CLASS_WARRIOR, pPlayer->GetTeam());
    if (!botRace)
    {
        SendSysMessage("Unable to select race for bot.");
        SetSentErrorMessage(true);
        return false;
    }

    uint32 botLevel = 1;
    ExtractUInt32(&args, botLevel);
    if (botLevel == 0)
        botLevel = 1;
    else if (botLevel > 60)
        botLevel = 60;

    float x, y, z;
    pPlayer->GetNearPoint(pPlayer, x, y, z, 0, 5.0f, frand(0.0f, 6.0f));
    
    WorldBotAI* ai = new WorldBotAI(nullptr, botRace, (uint8)CLASS_WARRIOR, (uint8)botLevel, pPlayer->GetMapId(), pPlayer->GetMap()->GetInstanceId(),
        x, y, z, pPlayer->GetOrientation());
    if (sPlayerBotMgr.AddBot(ai))
        SendSysMessage("New world bot added.");
    else
    {
        delete ai;
        SendSysMessage("Error spawning world bot.");
        SetSentErrorMessage(true);
        return false;
    }
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "___ GetInstanceId() = %d", pPlayer->GetMap()->GetInstanceId());
    return true;
}

bool ChatHandler::HandleWorldBotFindPathCommand(char * args){
    Player* pPlayer = GetSession()->GetPlayer();
    if (!pPlayer){
        return false;
    }

    Player* pTarget = GetSelectedPlayer();
    if (!pTarget){
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }
    
    if (!pTarget->AI()){
        SendSysMessage("Selected target has no AI.");
        SetSentErrorMessage(true);
        return false;
    }

    WorldBotAI* pAI = dynamic_cast<WorldBotAI*>(pTarget->AI());
    if (!pAI){
        SendSysMessage("Selected target is not a world bot.");
        SetSentErrorMessage(true);
        return false;
    }

    
    // uint32 taxiPathId = 47;
    // uint32 displayId = 0;
    // uint32 creatureId = 0;
    // WorldBotConfig::TaxiFlightNodeInfo taxiInfo = sPlayerBotMgr.GetTaxiFlightNodeInfoById(taxiPathId);
    // if (taxiInfo.id){
    //     if (taxiInfo.mount_creature_id1)
    //         creatureId = taxiInfo.mount_creature_id1;
    //     else if (taxiInfo.mount_creature_id2)
    //         creatureId = taxiInfo.mount_creature_id2;
    // }
    // for (auto itr = sObjectMgr.GetCreatureInfoMap().begin(); itr != sObjectMgr.GetCreatureInfoMap().end(); ++itr){
    //     if (creatureId == itr->first){
    //         displayId = itr->second->display_id[0];
    //         break;
    //     }
    // }
    // if (displayId)
    //     pTarget->Mount(displayId);
    // pTarget->GetMotionMaster()->MoveTaxiFlight(taxiPathId, 0);

    // pAI->SetLeader(pPlayer);

    // std::vector<WorldBotConfig::TravelNodePath> travelPath = sPlayerBotMgr.GetTravelNodePathVector(pPlayer->GetMapId(), 1068, 491);
    // sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[PBM]::FindPath : travel path size = %d.", travelPath.size());

    std::vector<WorldBotConfig::TravelNode> travel;
    travel = sPlayerBotMgr.FindAStarPath(pTarget->GetFactionId(), pTarget->GetMapId(), pTarget->GetPosition(), pPlayer->GetMapId(), pPlayer->GetPosition());
    if (!travel.size()){
        SendSysMessage("Path is not found.");
        SetSentErrorMessage(true);
        return false;
    }
    pAI->SetTravelJourney(travel, pPlayer->GetPosition());
    
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[PlayerBotMgr]::HandleWorldBotFindPath : found path:");
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "Start here %d -> ", travel[0].id);
    for (int i=1; i < travel.size(); ++i)
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, " -> %d; name \"%s\"", travel[i].id, travel[i].name.c_str());
    
    return true;
}

bool ChatHandler::HandleWorldBotCastEnchantCommand(char * args){
    Player* pTarget = GetSelectedPlayer();
    if (!pTarget){
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }
    
    if (!pTarget->AI()){
        SendSysMessage("Selected target has no AI.");
        SetSentErrorMessage(true);
        return false;
    }

    WorldBotAI* pAI = dynamic_cast<WorldBotAI*>(pTarget->AI());
    if (!pAI){
        SendSysMessage("Selected target is not a world bot.");
        SetSentErrorMessage(true);
        return false;
    }

    if (pAI->GetCraftProfessionId() != SKILL_ENCHANTING){
        SendSysMessage("Selected bot doesn't know enchanting.");
        SetSentErrorMessage(true);
        return false;
    }

    uint32 spellId = 0;
    ExtractUInt32(&args, spellId);
    if (spellId){
        pAI->CastEnchantSpell(spellId);
        return true;
    }
    else {
        SendSysMessage("No profession spell id specified.");
        SetSentErrorMessage(true);
        return false;
    }

    return true;
}

bool ChatHandler::HandleWorldBotProfInfoCommand(char * args){
    Player* pPlayer = GetSession()->GetPlayer();
    if (!pPlayer)
        return false;

    uint32 lang = 0;
    if (pPlayer->GetTeam() == ALLIANCE)
        lang = LANG_COMMON;
    else if (pPlayer->GetTeam() == HORDE)
        lang = LANG_ORCISH;

    Player* pTarget = GetSelectedPlayer();
    if (!pTarget){
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }
    
    if (!pTarget->AI()){
        SendSysMessage("Selected target has no AI.");
        SetSentErrorMessage(true);
        return false;
    }

    WorldBotAI* pAI = dynamic_cast<WorldBotAI*>(pTarget->AI());
    if (!pAI){
        SendSysMessage("Selected target is not a world bot.");
        SetSentErrorMessage(true);
        return false;
    }

    const char* filterName = ExtractArg(&args);
    if (filterName){
        std::string pattern {filterName};
        std::vector<const SpellEntry*> spellVec = pAI->GetProffessionSpellsFilterByRegex(pattern);
        if (spellVec.empty()){
            std::string msg = "No matches found with filter word \"" + pattern + "\".";
            pTarget->MonsterWhisper(msg.c_str(), pPlayer);
            return true;
        }
        std::string msg = "Found matches for word \"" + pattern + "\":";
        auto data = std::make_unique<WorldPackets::Chat::ChatMessage>();
        data->message = msg;
        data->lang = lang;
        data->whisperTargetOrChannel = pPlayer->GetName();
        data->type = CHAT_MSG_WHISPER;
        pTarget->GetSession()->QueuePacket(std::move(data));
        // pTarget->MonsterWhisper(msg.c_str(), GetSession()->GetPlayer());
        msg.clear();
        int32 i = 1;
        for (auto spell: spellVec){
            if (i % 7 == 0){
                data = std::make_unique<WorldPackets::Chat::ChatMessage>();
                data->message = msg;
                data->lang = lang;
                data->whisperTargetOrChannel = pPlayer->GetName();
                data->type = CHAT_MSG_WHISPER;
                pTarget->GetSession()->QueuePacket(std::move(data));
                msg.clear();
                i = 1;
            }
            msg.append("[" + (spell->SpellName[0]) + "]" + " - " + std::to_string(spell->Id) + "; ");
            i++;
        }
        if (!msg.empty()){
            data = std::make_unique<WorldPackets::Chat::ChatMessage>();
            data->message = msg;
            data->lang = lang;
            data->whisperTargetOrChannel = pPlayer->GetName();
            data->type = CHAT_MSG_WHISPER;
            pTarget->GetSession()->QueuePacket(std::move(data));
        }
        // pTarget->MonsterWhisper(msg.c_str(), GetSession()->GetPlayer());
    }

    return true;
}

bool ChatHandler::HandleWorldBotRemoveCommand(char * args){
    Player* pTarget = GetSelectedPlayer();
    if (!pTarget){
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }
    
    if (!pTarget->AI()){
        SendSysMessage("Selected target has no AI.");
        SetSentErrorMessage(true);
        return false;
    }

    WorldBotAI* pAI = dynamic_cast<WorldBotAI*>(pTarget->AI());
    if (!pAI){
        SendSysMessage("Selected target is not a world bot.");
        SetSentErrorMessage(true);
        return false;
    }

    pAI->botEntry->requestRemoval = true;

    return true;
}

bool ChatHandler::HandleWorldBotRecordPointCommand(char * args){
    WorldBotConfig* config = sPlayerBotMgr.GetWorldBotConfig();
    if (!config)
        return false;
    Player* pPlayer = m_session->GetPlayer();
    if (!pPlayer)
        return false;

    const char* type = ExtractArg(&args);
    if (!type){
        SendSysMessage("No command type specified. Abort.");
        SetSentErrorMessage(true);
        return false;
    }
    std::string str {type};
    if (str == "init"){
        uint32 id = 0;
        if (!ExtractUInt32(&args, id)){
            SendSysMessage("Failed to extract init node id. Abort.");
            SetSentErrorMessage(true);
            return false;
        }
        const char* nameChar = ExtractArg(&args);
        std::string name;
        if (nameChar){
            name = nameChar;
        }
        
        config->previousNodeId = id;
        WorldBotConfig::TravelNode node {config->previousNodeId, name.c_str(), pPlayer->GetMapId(), pPlayer->GetPositionX(), 
            pPlayer->GetPositionY(), pPlayer->GetPositionZ(), true};
        config->recordNodes.push_back(node);
        config->currentNodeId = id + 1;
        std::string msg = "Set init point:\n (" + std::to_string(node.id) + ", " + name + ", " + std::to_string(node.map_id) +
            ", " + std::to_string(node.x) + ", " + std::to_string(node.y) + ", " + std::to_string(node.z) + ", " + std::to_string(node.linked)
            + ")";
        std::fstream file("waypoints.txt", std::ios_base::app);
        if (file.is_open()){
            file << msg << std::endl;
        }
        file.close();
        SendSysMessage(msg.c_str());
        return true;
    }
    else if (str == "setpreviosnode"){
        uint32 id = 0;
        if (!ExtractUInt32(&args, id)){
            SendSysMessage("Failed to extract previous node id. Abort.");
            SetSentErrorMessage(true);
            return false;
        }
        
        uint32 index = sPlayerBotMgr.GetRecordNodeIndex(id);
        if (index == UINT32_MAX){
            SendSysMessage("No nodes found for provided id. Abort.");
            SetSentErrorMessage(true);
            return false;
        }
        WorldBotConfig::TravelNode travelNode = config->recordNodes[index];
        config->previousNodeId = travelNode.id;
        pPlayer->TeleportTo(travelNode.map_id, travelNode.x, travelNode.y, travelNode.z, 0.0f);
        std::string msg = "Node [#" + std::to_string(travelNode.id) + ", name \"" + travelNode.name + "\"]" + " has been set as previous.";
        std::fstream file("waypoints.txt", std::ios_base::app);
        if (file.is_open()){
            file << msg << std::endl;
            file.close();
        }
        SendSysMessage(msg.c_str());
    }
    else if (str == "addnode"){
        const char* nameChar = ExtractArg(&args);
        std::string name;
        if (nameChar)
            name = nameChar;
        uint32 index = sPlayerBotMgr.GetRecordNodeIndex(config->previousNodeId);
        if (index == UINT32_MAX){
            SendSysMessage("No nodes found for provided id. Abort.");
            SetSentErrorMessage(true);
            return false;
        }
        WorldBotConfig::TravelNode prevNode = config->recordNodes[index];
        if (prevNode.id == 0){
            SendSysMessage("No previous node found. Run \'init\' subcommand first. Abort.");
            SetSentErrorMessage(true);
            return false;
        }
        WorldBotConfig::TravelNode node {config->currentNodeId, name.c_str(), pPlayer->GetMapId(), pPlayer->GetPositionX(), 
            pPlayer->GetPositionY(), pPlayer->GetPositionZ(), false};
        
        int32 isBidirectional = -1;
        ExtractInt32(&args, isBidirectional);
        if (isBidirectional !=0 && isBidirectional != 1){
            SendSysMessage("No link directions specified. Abort.");
            SetSentErrorMessage(true);
            return false;
        }
        uint32 pathType = 0;
        ExtractUInt32(&args, pathType);
        if (!pathType){
            SendSysMessage("No path type specified. Abort.");
            SetSentErrorMessage(true);
            return false;
        }
        uint32 objectId = 0;
        ExtractUInt32(&args, objectId);
        float straightDistance = Vector3(node.x-prevNode.x, node.y-prevNode.y, node.z-prevNode.z).length();
        if (straightDistance > 100.0f){
            SendSysMessage("Error: straight distance between these nodes is greater than 100 yards. Abort.");
            SetSentErrorMessage(true);
            return false;
        }
        PathFinder pathFinder(pPlayer);
        pathFinder.calculate(Vector3(prevNode.x, prevNode.y, prevNode.z), Vector3(node.x, node.y, node.z));
        float distance = pathFinder.Length();
        WorldBotConfig::TravelNodeLink linkToCurrent {prevNode.id, node.id, static_cast<uint8>(pathType), objectId, distance, 0.0f, 0.0f, 0, 0, 0, 0};
        WorldBotConfig::TravelNodeLink linkFromCurrent {node.id, prevNode.id, static_cast<uint8>(pathType), objectId, distance, 0.0f, 0.0f, 0, 0, 0, 0};
        config->recordNodeLinks.push_back(linkToCurrent);
        if (isBidirectional){
            config->recordNodeLinks.push_back(linkFromCurrent);
            prevNode.linked = true;
        }
        node.linked = true;
        config->recordNodes.push_back(node);
        config->previousNodeId = config->currentNodeId;
        config->currentNodeId++;

        std::string msg = "Add node (" + std::to_string(node.id) + ", " + node.name + ", " + std::to_string(node.map_id) +
            ", " + std::to_string(node.x) + ", " + std::to_string(node.y) + ", " + std::to_string(node.z) + ", " + 
            std::to_string(node.linked) + ") linked to #" + std::to_string(prevNode.id) + ".";

        std::fstream file("waypoints.txt", std::ios_base::app);
        if (file.is_open()){
            file << msg << std::endl;
            file.close();
        }
        SendSysMessage(msg.c_str());
    }
    else if (str == "addpathnode"){
        uint32 firstId = 0;
        ExtractUInt32(&args, firstId);
        if (!firstId){
            SendSysMessage("Failed to extract first node id. Abort.");
            SetSentErrorMessage(true);
            return false;
        }
        uint32 secondId = 0;
        ExtractUInt32(&args, secondId);
        if (!secondId){
            SendSysMessage("Failed to extract second node id. Abort.");
            SetSentErrorMessage(true);
            return false;
        }

        WorldBotConfig::TravelNodePath path {firstId, secondId, config->currentPathNodeIndex, pPlayer->GetMapId(), pPlayer->GetPositionX(), pPlayer->GetPositionY(),
            pPlayer->GetPositionZ()};

        config->recordNodePaths.push_back(path);
        config->currentPathNodeIndex++;
        std::string msg = "Add path node (" + std::to_string(path.node_id) + ", " + std::to_string(path.to_node_id) + ", " + 
            std::to_string(path.nr) + ").";
        SendSysMessage(msg.c_str());
    }
    else if (str == "savepath"){
        uint32 node_id = 0, to_node_id = 0;
        ExtractUInt32(&args, node_id);
        if (!node_id){
            SendSysMessage("Failed to extract first node id. Abort.");
            SetSentErrorMessage(true);
            return false;
        }
        ExtractUInt32(&args, to_node_id);
        if (!to_node_id){
            SendSysMessage("Failed to extract second node id. Abort.");
            SetSentErrorMessage(true);
            return false;
        }

        config->currentPathNodeIndex = 0;
        float distance = 0.0f;
        std::vector<WorldBotConfig::TravelNodePath> path = sPlayerBotMgr.GetRecordNodePathVector(pPlayer->GetMapId(), node_id, to_node_id);
        if (path.empty()){
            SendSysMessage("Can't find any path for provided nodes. Abort.");
            SetSentErrorMessage(true);
            return false;
        }
        for (int i=1; i < path.size(); ++i){
            float dx = path[i-1].x - path[i].x;
            float dy = path[i-1].y - path[i].y;
            float dz = path[i-1].z - path[i].z;
            distance += sqrtf(dx*dx + dy*dy + dz*dz);
        }
        std::reverse(path.begin(), path.end());
        for (auto& entry: path){
            uint32 tmp = entry.node_id;
            entry.node_id = entry.to_node_id;
            entry.to_node_id = tmp;
            entry.nr = path.size() - 1 - entry.nr;
            config->recordNodePaths.push_back(entry);
        }
        WorldBotConfig::TravelNodeLink link {node_id, to_node_id, PATH_TYPE_WALK, 0, distance, 0.0f, 0.0f, 0, 0, 0, 0};
        WorldBotConfig::TravelNodeLink link2 {to_node_id, node_id, PATH_TYPE_WALK, 0, distance, 0.0f, 0.0f, 0, 0, 0, 0};
        config->recordNodeLinks.push_back(link);
        config->recordNodeLinks.push_back(link2);
    }
    else if (str == "linknodes"){
        uint32 node1 = 0;
        uint32 node2 = 0;
        
        if (!ExtractUInt32(&args, node1)){
            SendSysMessage("Failed to extract first node id. Abort.");
            SetSentErrorMessage(true);
            return false;
        }
        uint32 index1 = sPlayerBotMgr.GetRecordNodeIndex(node1);
        if (index1 == UINT_MAX){
            std::string msg = "No nodes found in recordNodes vector for node1 = #" + std::to_string(node1) + ". Abort.";
            SendSysMessage(msg.c_str());
            SetSentErrorMessage(true);
            return false;
        }

        if (!ExtractUInt32(&args, node2)){
            SendSysMessage("Failed to extract second node id. Abort.");
            SetSentErrorMessage(true);
            return false;
        }
        uint32 index2 = sPlayerBotMgr.GetRecordNodeIndex(node2);
        if (index2 == UINT_MAX){
            std::string msg = "No nodes found in recordNodes vector for node2 = #" + std::to_string(node1) + ". Abort.";
            SendSysMessage(msg.c_str());
            SetSentErrorMessage(true);
            return false;
        }

        uint32 isBidirectional = 0;
        if (!ExtractUInt32(&args, isBidirectional)){
            SendSysMessage("Failed to extract bidirectional flag. Abort.");
            SetSentErrorMessage(true);
            return false;
        }
        if (isBidirectional != 0 && isBidirectional != 1){
            SendSysMessage("Wrong \'isBidirectional\' argument. Should be 0 or 1. Abort.");
            SetSentErrorMessage(true);
            return false;
        }

        uint32 pathType = PATH_TYPE_NONE;
        if (!ExtractUInt32(&args, pathType)){
            SendSysMessage("Failed to extract travel type. Abort.");
            SetSentErrorMessage(true);
            return false;
        }

        uint32 object = 0;
        ExtractUInt32(&args, object);

        WorldBotConfig::TravelNode travelNode1 = config->recordNodes[index1];
        WorldBotConfig::TravelNode travelNode2 = config->recordNodes[index2];
        float straightDistance = Vector3(travelNode1.x-travelNode2.x, travelNode1.y-travelNode2.y, travelNode1.z-travelNode2.z).length();
        if (straightDistance > 100.0f){
            SendSysMessage("Straight distance between these nodes is greater that 100 yards. Abort.");
            SetSentErrorMessage(true);
            return false;
        }

        PathFinder pathFinder(pPlayer);
        pathFinder.calculate(Vector3(travelNode1.x, travelNode1.y, travelNode1.z), Vector3(travelNode2.x, travelNode2.y, travelNode2.z));
        float distance = pathFinder.Length();
        WorldBotConfig::TravelNodeLink link1 {node1, node2, static_cast<uint8>(pathType), object, distance, 0.0f, 0.0f, 0, 0, 0, 0};
        config->recordNodeLinks.push_back(link1);
        config->recordNodes[index1].linked = true;
        if (isBidirectional){
            WorldBotConfig::TravelNodeLink link2 {node2, node1, static_cast<uint8>(pathType), object, distance, 0.0f, 0.0f, 0, 0, 0, 0};
            config->recordNodeLinks.push_back(link2);
            config->recordNodes[index2].linked = true;
        }

        std::string msg = "Link has been set from #" + std::to_string(node1) + " to #" + std::to_string(node2);
        if (isBidirectional)
            msg.append(" and vice versa");
        msg.append(".");
        std::fstream file("waypoints.txt", std::ios_base::app);
        if (file.is_open()){
            file << msg << std::endl;
            file.close();
        }
        SendSysMessage(msg.c_str());
    }
    else if (str == "linknodeswithindist"){
        float boundDist = 20.0f;
        ExtractFloat(&args, boundDist);
        if (boundDist > 20.0f)
            boundDist = 20.0f;
        std::string msg = "Using bound distance = " + std::to_string(boundDist) + ".";
        SendSysMessage(msg.c_str());
        std::fstream file {"createdlinks.txt", std::ios::out | std::ios::trunc};
        for (uint32 i=0; i < config->recordNodes.size(); ++i){
            WorldBotConfig::TravelNode node = config->recordNodes[i];
            for (uint32 j=0; j < config->recordNodes.size(); ++j){
                if (j == i)
                    continue;

                WorldBotConfig::TravelNode neighbour = config->recordNodes[j];
                
                if (sPlayerBotMgr.DoesLinkExist(node.id, neighbour.id))
                    continue;

                // TODO: make this stupid working...
                float straightDist = Vector3(node.x-neighbour.x, node.y-neighbour.y, node.z-neighbour.z).length();
                if (straightDist > boundDist)
                    continue;

                PathFinder pathFinder(pPlayer);
                pathFinder.calculate(Vector3(node.x, node.y, node.z), Vector3(neighbour.x, neighbour.y, neighbour.z));
                float dist = pathFinder.Length();
                if (dist > boundDist)
                    continue;

                WorldBotConfig::TravelNodeLink link {node.id, neighbour.id, PATH_TYPE_WALK, 0, dist, 0.0f, 0.0f, 0, 0, 0, 0};
                config->recordNodeLinks.push_back(link);

                if (file.is_open())
                    file << "(" << link.node_id << ", " << link.to_node_id << ")" << std::endl;
            }
        }
        file.close();
    }
    else if (str == "clear"){
        config->recordNodes.clear();
        config->recordNodeLinks.clear();
        config->recordNodePaths.clear();
    }
    else if (str == "makesql"){
        std::fstream file {"recordNodes.sql", std::ios::in | std::ios::out | std::ios::trunc};
        if (!file.is_open()){
            SendSysMessage("Error opening file \"recordeNodes.sql\". Abort.");
            SetSentErrorMessage(true);
            return false;
        }
        file << "INSERT INTO `worldbot_travelnode` (id, name, map_id, x, y, z, linked)" << std::endl;
        file << "VALUES" << std::endl;
        for (auto node: config->recordNodes){
            file << "(" << node.id << ", '" << node.name << "', " << node.map_id << ", " << node.x << ", "
                << node.y << ", " << node.z << ", " << static_cast<int>(node.linked) << ")";
            if (node.id != config->recordNodes.back().id)
                file << ",";
            file << std::endl;
        }
        file << " AS new_data" << std::endl;
        file << "ON DUPLICATE KEY UPDATE" << std::endl;
        file << "name = new_data.name," << std::endl;
        file << "map_id = new_data.map_id," << std::endl;
        file << "x = new_data.x," << std::endl;
        file << "y = new_data.y," << std::endl;
        file << "z = new_data.z," << std::endl;
        file << "linked = new_data.linked;" << std::endl;
        file.close();

        file.open("recordNodeLinks.sql", std::ios::out | std::ios::trunc);
        if (!file.is_open()){
            SendSysMessage("Error opening file \"recordeNodeLinks.sql\". Abort.");
            SetSentErrorMessage(true);
            return false;
        }
        file << "INSERT INTO `worldbot_travelnode_link` (node_id, to_node_id, type, object, distance, swim_distance, extra_cost, calculated, max_creature_0, max_creature_1, max_creature_2)" << std::endl;
        file << "VALUES" << std::endl;
        for (auto link: config->recordNodeLinks){
            file << "(" << link.node_id << ", " << link.to_node_id << ", " << static_cast<int>(link.type) << ", " << link.object << ", "
                << link.distance << ", " << link.swim_distance << ", " << link.extra_cost << ", " << static_cast<int>(link.calculated)
                << ", " << static_cast<int>(link.max_creature0) << ", " << static_cast<int>(link.max_creature1) << ", " << static_cast<int>(link.max_creature2) << ")";
            if (link.node_id != config->recordNodeLinks.back().node_id || link.to_node_id != config->recordNodeLinks.back().to_node_id)
                file << ",";
            file << std::endl;
        }
        file << " AS new_data" << std::endl;
        file << "ON DUPLICATE KEY UPDATE" << std::endl;
        file << "type = new_data.type," << std::endl;
        file << "object = new_data.object," << std::endl;
        file << "distance = new_data.distance," << std::endl;
        file << "swim_distance = new_data.swim_distance," << std::endl;
        file << "extra_cost = new_data.extra_cost," << std::endl;
        file << "calculated = new_data.calculated," << std::endl;
        file << "max_creature_0 = new_data.max_creature_0," << std::endl;
        file << "max_creature_1 = new_data.max_creature_1," << std::endl;
        file << "max_creature_2 = new_data.max_creature_2;" << std::endl;
        file.close();

        file.open("recordNodePaths.sql", std::ios::out | std::ios::trunc);
        if (!file.is_open()){
            SendSysMessage("Error opening file \"recordeNodePaths.sql\". Abort.");
            SetSentErrorMessage(true);
            return false;
        }
        file << "INSERT INTO `worldbot_travelnode_path` (node_id, to_node_id, nr, map_id, x, y, z)" << std::endl;
        file << "VALUES" << std::endl;
        for (auto path: config->recordNodePaths){
            file << "(" << path.node_id << ", " << path.to_node_id << ", " << path.nr << ", " << path.map_id << ", "
                << path.x << ", " << path.y << ", " << path.z << ")";
            if (path.node_id != config->recordNodePaths.back().node_id || path.to_node_id != config->recordNodePaths.back().to_node_id
                || path.nr != config->recordNodePaths.back().nr)
                file << ",";
            file << std::endl;
        }
        file << "AS new_data" << std::endl;
        file << "ON DUPLICATE KEY UPDATE" << std::endl;
        file << "node_id = new_data.node_id," << std::endl;
        file << "to_node_id = new_data.to_node_id," << std::endl;
        file << "nr = new_data.nr," << std::endl;
        file << "map_id = new_data.map_id," << std::endl;
        file << "x = new_data.x," << std::endl;
        file << "y = new_data.y," << std::endl;
        file << "z = new_data.z;" << std::endl;
        file.close();
    }
    else if (str == "loadrecordnodes"){
        const char* arg = ExtractArg(&args);
        if (!arg){
            SendSysMessage("No file name provided. Abort.");
            SetSentErrorMessage(true);
            return false;
        }
        std::ifstream file {arg, std::ios::in};
        if (!file.is_open()){
            sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "[PlayerBotMgr]::HandleWorldBotRecordPointCommand : error \"loadrecordnodes\" subcommand - can't open file.");
            return false;
        }
        std::string line;
        while (std::getline(file, line)){
            std::regex pattern {R"(\((\d+), '([^']*)', (\d+), ([+-]?\d+(?:[.,]\d+)?), ([+-]?\d+(?:[.,]\d+)?), ([+-]?\d+(?:[.,]\d+)?), (\d)\))"};
            std::smatch matches;
            if (std::regex_search(line, matches, pattern)){
                uint32 id = std::stoul(matches[1].str());
                std::string name {matches[2].str()};
                uint32 map_id = std::stoul(matches[3].str());
                float x = std::stof(matches[4].str());
                float y = std::stof(matches[5].str());
                float z = std::stof(matches[6].str());
                bool linked = static_cast<bool>(std::stoi(matches[7].str()));

                WorldBotConfig::TravelNode node {id, name, map_id, x, y, z, linked};
                // if not found in record node vector
                if (sPlayerBotMgr.GetRecordNodeIndex(node.id) == UINT_MAX){
                    config->recordNodes.push_back(node);
                }
            }
        }
        file.close();
        std::string msg = "Loaded " + std::to_string(config->recordNodes.size()) + " nodes from file \"" + arg + "\".";
        SendSysMessage(msg.c_str());
        // sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[PlayerBotMgr]::HandleWorldBotRecordPointCommand : loaded %d nodes from file.", config->recordNodes.size());

        if (uint32 size = config->recordNodes.size()){
            config->previousNodeId = config->recordNodes[size-1].id;
            config->currentNodeId = config->previousNodeId + 1;
        }
    }
    else if (str == "loadrecordnodelinks"){
        const char* arg = ExtractArg(&args);
        if (!arg){
            SendSysMessage("No file name provided. Abort.");
            SetSentErrorMessage(true);
            return false;
        }
        std::ifstream file {arg, std::ios::in};
        if (!file.is_open()){
            sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "[PlayerBotMgr]::HandleWorldBotRecordPointCommand : error \"loadrecordnodelinks\" subcommand - can't open file.");
            return false;
        }
        std::string line;
        uint32 count = 0;
        while (std::getline(file, line)){
            std::regex pattern {R"(\((\d+), (\d+), (\d+), (\d+), ([+-]?\d+(?:[.,]\d+)?), ([+-]?\d+(?:[.,]\d+)?), ([+-]?\d+(?:[.,]\d+)?), (\d+), (\d+), (\d+), (\d+)\))"};
            std::smatch matches;
            if (std::regex_search(line, matches, pattern)){
                uint32 node_id = std::stol(matches[1].str());
                uint32 to_node_id = std::stol(matches[2].str());
                uint32 type = std::stol(matches[3].str());
                uint32 object = std::stol(matches[4].str());
                float distance = std::stof(matches[5].str());
                float swim_distance = std::stof(matches[6].str());
                float extra_cost = std::stof(matches[7].str());
                uint8 calculated = static_cast<uint8>(std::stoi(matches[8].str()));
                uint8 max_creature_0 = static_cast<uint8>(std::stoi(matches[9].str()));
                uint8 max_creature_1 = static_cast<uint8>(std::stoi(matches[10].str()));
                uint8 max_creature_2 = static_cast<uint8>(std::stoi(matches[11].str()));

                WorldBotConfig::TravelNodeLink link {node_id, to_node_id, static_cast<uint8>(type), object, distance, swim_distance, extra_cost, 
                    calculated, max_creature_0, max_creature_1, max_creature_2};

                
                // WorldBotConfig::TravelNodeLink linkFromVector = sPlayerBotMgr.GetTravelNodeLink(node_id, to_node_id);
                // if not found in recordNodeLinks vector
                if (!sPlayerBotMgr.DoesLinkExist(node_id, to_node_id)){
                    config->recordNodeLinks.push_back(link);
                    count++;
                }
            }
        }
        file.close();
        std::string msg = "Loaded " + std::to_string(config->recordNodeLinks.size()) + " node links from file \"" + arg + "\".";
        SendSysMessage(msg.c_str());
        // sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[PlayerBotMgr]::HandleWorldBotRecordPointCommand : loaded %d node links from file.", count);
    }
    else if (str == "parselogfile"){
        std::fstream file {"waypoints.txt", std::ios::in};
        if (!file.is_open()){
            SendSysMessage("Error opening file. Abort.");
            SetSentErrorMessage(true);
            return false;
        }
        std::string line;
        uint32 count1 = 0, count2 = 0;
        while(std::getline(file, line)){
            // Add node (1840, Orgrimmar, 1, 1418.283691, -4366.668457, 25.235546, 1) linked to #1839.
            std::regex pattern {R"(Add node \((\d+), ([^,]*), (\d+), ([+-]?\d+(?:[.,]\d+)?), ([+-]?\d+(?:[.,]\d+)?), ([+-]?\d+(?:[.,]\d+)?), (\d)\) linked to #(\d+)\.)"};
            std::regex pattern2 {R"(Link has been set from #(\d+) to #(\d+) and vice versa.)"};
            std::smatch matches;
            if (std::regex_search(line, matches, pattern)){
                uint32 id = std::stol(matches[1].str());
                std::string name = matches[2].str();
                uint32 map_id = std::stol(matches[3].str());
                float x = std::stof(matches[4].str());
                float y = std::stof(matches[5].str());
                float z = std::stof(matches[6].str());
                bool linked = static_cast<bool>(std::stod(matches[7].str()));
                uint32 to_node_id = std::stol(matches[8].str());
                if (sPlayerBotMgr.GetRecordNodeIndex(id) != UINT_MAX)
                    continue;

                WorldBotConfig::TravelNode node {id, name, map_id, x, y, z, linked};
                // index is guaranteed
                uint32 index = sPlayerBotMgr.GetRecordNodeIndex(to_node_id);
                WorldBotConfig::TravelNode toNode = config->recordNodes[index];
                PathFinder pathFinder(pPlayer);
                pathFinder.calculate(Vector3(node.x, node.y, node.z), Vector3(toNode.x, toNode.y, toNode.z));
                float distance = pathFinder.Length();
                WorldBotConfig::TravelNodeLink link {node.id, toNode.id, PATH_TYPE_WALK, 0, distance, 0.0f, 0.0f, 0, 0, 0, 0};
                WorldBotConfig::TravelNodeLink link2 {toNode.id, node.id, PATH_TYPE_WALK, 0, distance, 0.0f, 0.0f, 0, 0, 0, 0};
                
                if (sPlayerBotMgr.GetRecordNodeIndex(node.id) == UINT_MAX){
                    config->recordNodes.push_back(node);
                    count1++;
                }

                if (!sPlayerBotMgr.DoesLinkExist(link.node_id, link.to_node_id) && !sPlayerBotMgr.DoesLinkExist(link.node_id, link2.to_node_id)){
                    config->recordNodeLinks.push_back(link);
                    config->recordNodeLinks.push_back(link2);
                    count2++;
                }
            }
            else if (std::regex_search(line, matches, pattern2)){
                uint32 node_id = std::stol(matches[1].str());
                uint32 to_node_id = std::stol(matches[2].str());
                if (sPlayerBotMgr.DoesLinkExist(node_id, to_node_id))
                    continue;

                uint32 index1 = sPlayerBotMgr.GetRecordNodeIndex(node_id);
                if (index1 == UINT_MAX)
                    continue;
                uint32 index2 = sPlayerBotMgr.GetRecordNodeIndex(to_node_id);
                if (index2 == UINT_MAX)
                    continue;

                WorldBotConfig::TravelNode node = config->recordNodes[index1];
                WorldBotConfig::TravelNode toNode = config->recordNodes[index2];
                PathFinder pathFinder(pPlayer);
                float distance = pathFinder.calculate(Vector3(node.x, node.y, node.z), Vector3(toNode.x, toNode.y, toNode.z));

                WorldBotConfig::TravelNodeLink link {node_id, to_node_id, PATH_TYPE_WALK, 0, distance, 0.0f, 0.0f, 0, 0, 0, 0};
                config->recordNodeLinks.push_back(link);
                count2++;
            }
        }
        std::string msg = "Parsed " + std::to_string(count1) + " record nodes and " + std::to_string(count2) + " node links.";
        SendSysMessage(msg.c_str());
        file.close();
    }
    // TODO: change this command to link existing (in travel node vector) nodes...
    else if (str == "getdistancebetweennodes"){
        uint32 firstId = 0, secondId = 0;
        ExtractUInt32(&args, firstId);
        if (!firstId){
            SendSysMessage("Can't extract first node id. Abort.");
            SetSentErrorMessage(true);
            return false;
        }
        ExtractUInt32(&args, secondId);
        if (!secondId){
            SendSysMessage("Can't extract second node id. Abort");
            SetSentErrorMessage(true);
            return false;
        }

        WorldBotConfig::TravelNode firstNode = sPlayerBotMgr.GetTravelNode(firstId);
        if (firstNode.id == 0){
            std::string msg = "Can't get node with id #" + std::to_string(firstId) + ". Abort.";
            SendSysMessage(msg.c_str());
            SetSentErrorMessage(true);
            return false;
        }
        WorldBotConfig::TravelNode secondNode = sPlayerBotMgr.GetTravelNode(secondId);
        if (secondNode.id == 0){
            std::string msg = "Can't get node with id #" + std::to_string(secondId) + ". Abort.";
            SendSysMessage(msg.c_str());
            SetSentErrorMessage(true);
            return false;
        }
        float distance = Vector3(firstNode.x-secondNode.x, firstNode.y-secondNode.y, firstNode.z-secondNode.z).length();
        std::string msg = "Straight distance between #" + std::to_string(firstNode.id) + " and #" + std::to_string(secondNode.id) +
            " = " + std::to_string(distance) + ".";
        SendSysMessage(msg.c_str());
    }
    else if (str == "getclosestnode"){
        float minDistance = FLT_MAX;
        uint32 closestNodeId = 0;
        for (auto& node: config->travelNode){
            float distance = Vector3(pPlayer->GetPositionX()-node.x, pPlayer->GetPositionY()-node.y, pPlayer->GetPositionZ()-node.z).length();
            if (distance < minDistance){
                minDistance = distance;
                closestNodeId = node.id;
            }
        }

        if (!closestNodeId){
            SendSysMessage("No nodes nearby found for current position.");
            return true;
        }

        std::string msg = "Closest node is #" + std::to_string(closestNodeId) + " with distance " + std::to_string(minDistance) +
            ".";
        SendSysMessage(msg.c_str());
    }
    else if (str == "gotonode"){
        uint32 id = 0;
        ExtractUInt32(&args, id);
        if (!id){
            SendSysMessage("Can't extract first node id. Abort.");
            SetSentErrorMessage(true);
            return false;
        }

        WorldBotConfig::TravelNode node = sPlayerBotMgr.GetTravelNode(id);
        if (node.id == 0){
            SendSysMessage("Can't find the node with provided id. Abort.");
            SetSentErrorMessage(true);
            return false;
        }

        pPlayer->TeleportTo(node.map_id, node.x, node.y, node.z, pPlayer->GetOrientation());
    }

    return true;
}

bool ChatHandler::HandleWorldBotSpawnCommand(char * args){
    WorldBotConfig* config = sPlayerBotMgr.GetWorldBotConfig();
    if (!config)
        return false;
    Player* pPlayer = m_session->GetPlayer();
    if (!pPlayer)
        return false;

    uint32 count = 10;
    ExtractUInt32(&args, count);

    std::vector<uint8> allowedRaces {RACE_ORC};
    std::vector<uint8> allowedClasses {CLASS_WARRIOR};

    if (count > 2000)
        count = 2000;

    if (allowedRaces.empty()){
        SendSysMessage("Error: no allowed races specified.");
        SetSentErrorMessage(true);
        return false;
    }
    
    if (allowedClasses.empty()){
        SendSysMessage("Error: no allowed classes specified.");
        SetSentErrorMessage(true);
        return false;
    }

    std::vector<WorldBotConfig::TravelNode> spawnPoints = sPlayerBotMgr.GetTravelNodesFilterByRegex("Orgrimmar");
    if (spawnPoints.empty()){
        SendSysMessage("Error: no locations found for provided location.");
        SetSentErrorMessage(true);
        return false;
    }

    for (int i=0; i < count; ++i){
        uint8 botLevel = urand(6, 18);
        uint8 botRace = SelectRandomContainerElement(allowedRaces);
        uint8 botClass = SelectRandomContainerElement(allowedClasses);
        WorldBotConfig::TravelNode point {SelectRandomContainerElement(spawnPoints)};
        WorldBotAI* ai = new WorldBotAI(nullptr, botRace, botClass, botLevel, point.map_id, pPlayer->GetMap()->GetInstanceId(), point.x, point.y, point.z, frand(0.0f, 6.28f));
        if (sPlayerBotMgr.AddBot(ai)){
            SendSysMessage("Spawned world bot.");
        }
        else {
            delete ai;
            SendSysMessage("Error spawning world bot.");
            SetSentErrorMessage(true);
        }
        ai->SetTimerTracker((int32)urand(1000, 5000));
    }
    return true;
}

std::unordered_map<std::string, std::vector<std::pair<uint32, uint32>>> PlayerBotMgr::GetSpecsFilterByRegex(std::string regex){
    std::unordered_map<std::string, std::vector<std::pair<uint32, uint32>>> specMap;

    std::regex pattern(regex);
    for (const auto entry: m_worldBotDefaultTalentSpecs){
        if (std::regex_search(entry.first, pattern)){
            specMap[entry.first] = entry.second;
        }
    }

    return specMap;
}

std::vector<std::pair<uint32, uint32>> PlayerBotMgr::GetSpecByName(std::string properName){
    std::vector<std::pair<uint32, uint32>> talentSequence;
    
    auto it = m_worldBotDefaultTalentSpecs.find(properName);
    if (it != m_worldBotDefaultTalentSpecs.end())
        talentSequence = it->second;

    return talentSequence;
}

WorldBotConfig::TravelNode PlayerBotMgr::GetTravelNode(uint32 id) const {
    WorldBotConfig::TravelNode travelNode;
    for (auto node: m_worldBotConfig.travelNode){
        if (id == node.id)
            travelNode = node;
    }
    return travelNode;
}