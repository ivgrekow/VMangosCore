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

#include <regex>
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

    // 9- Load travel nodes
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
            TravelNode travelNode;
            travelNode.id = id;
            travelNode.name = name;
            travelNode.map_id = map_id;
            travelNode.x = x;
            travelNode.y = y;
            travelNode.z = z;
            travelNode.linked = linked;
            m_travelNodes.push_back(travelNode);
        } while (result->NextRow());
    }
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, ">> Loaded %d worldbot travelnodes.", m_travelNodes.size());

    // 10- load travel node links
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, ">> [PlayerBotMgr] Loading worldbot travelnode links...");
    result.reset();
    result = WorldDatabase.PQuery("SELECT node_id, to_node_id, type, object, distance, swim_distance, extra_cost, calculated, max_creature_0, max_creature_1, max_creature_2 FROM worldbot_travelnode_link;");
    if (!result)
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, ">> Table `worldbot_travelnode_link` is empty.");
    else {
        do {
            fields = result->Fetch();
            TravelNodeLink travelnodeLink;
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
            m_travelNodeLinks.push_back(travelnodeLink);
        } while (result->NextRow());
    }
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, ">> Loaded %d worldbot travelnode links.", m_travelNodeLinks.size());

    // 11- load travel node paths
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, ">> [PlayerBotMgr] Loading worldbot travelnode paths...");
    result.reset();
    result = WorldDatabase.PQuery("SELECT node_id, to_node_id, nr, map_id, x, y, z FROM worldbot_travelnode_path;");
    if (!result)
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "Table `worldbot_travelnode_path` is empty.");
    else {
        do {
            fields = result->Fetch();
            TravelNodePath travelnodePath;
            travelnodePath.node_id = fields[0].GetUInt32();
            travelnodePath.to_node_id = fields[1].GetUInt32();
            travelnodePath.nr = fields[2].GetUInt32();
            travelnodePath.map_id = fields[3].GetUInt32();
            travelnodePath.x = fields[4].GetFloat();
            travelnodePath.y = fields[5].GetFloat();
            travelnodePath.z = fields[6].GetFloat();
            m_travelNodePaths.push_back(travelnodePath);
        } while (result->NextRow());
    }
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, ">> Loaded %d worldbot travelnode paths.", m_travelNodePaths.size());

    // 12- taxi flight info
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, ">> [PlayerBotMgr] Loading worldbot taxi info...");
    result.reset();
    result = WorldDatabase.PQuery("SELECT id, mount_creature_id1, mount_creature_id2 FROM taxi_nodes;");
    if (!result)
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "Table `taxi_nodes` is empty.");
    else {
        do {
            fields = result->Fetch();
            TaxiFlightNodeInfo info;
            info.id = fields[0].GetUInt32();
            info.mount_creature_id1 = fields[1].GetUInt32();
            info.mount_creature_id2 = fields[2].GetUInt32();
            m_taxiFlightNodeInfo.push_back(info);
        }while (result->NextRow());
    }
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, ">> Loaded %d worldbot taxi info entries.", m_taxiFlightNodeInfo.size());

    m_statsTimer.Reset(-1);
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

    m_statsTimer.Update(diff);
    if (m_statsTimer.Passed()){
        m_statsTimer.Reset(12000);
        uint32 totalCount = 0;
        uint32 warriorCount = 0;
        uint32 paladinCount = 0;
        uint32 shamanCount = 0;
        uint32 hunterCount = 0;
        uint32 rogueCount = 0;
        uint32 druidCount = 0;
        uint32 mageCount = 0;
        uint32 warlockCount = 0;
        uint32 priestCount = 0;

        uint32 idleCount = 0;
        uint32 walkingCount = 0;
        uint32 flightCount = 0;
        
        uint32 aliveCount = 0;
        uint32 deadCount = 0;

        uint32 allianceCount = 0;
        uint32 hordeCount = 0;

        for (const auto& bot: m_bots){
            WorldBotAI* worldBot = dynamic_cast<WorldBotAI*>(bot.second->ai.get());
            if (worldBot){
                totalCount++;
                uint32 my_class = worldBot->me->GetClass();
                if (my_class == CLASS_WARRIOR)
                    warriorCount++;
                else if (my_class == CLASS_PALADIN)
                    paladinCount++;
                else if (my_class == CLASS_SHAMAN)
                    shamanCount++;
                else if (my_class == CLASS_HUNTER)
                    hunterCount++;
                else if (my_class == CLASS_ROGUE)
                    rogueCount++;
                else if (my_class == CLASS_DRUID)
                    druidCount++;
                else if (my_class == CLASS_MAGE)
                    mageCount++;
                else if (my_class == CLASS_WARLOCK)
                    warlockCount++;
                else if (my_class == CLASS_PRIEST)
                    priestCount++;

                uint32 move_type = worldBot->me->GetMotionMaster()->GetCurrentMovementGeneratorType();
                if (move_type == IDLE_MOTION_TYPE)
                    idleCount++;
                else if (move_type == POINT_MOTION_TYPE)
                    walkingCount++;
                else if (worldBot->me->IsTaxiFlying())
                    flightCount++;

                if (worldBot->me->IsAlive())
                    aliveCount++;
                else
                    deadCount++;

                Team my_team = worldBot->me->GetTeam();
                if (my_team == ALLIANCE)
                    allianceCount++;
                else if (my_team == HORDE)
                    hordeCount++;
            }
        }
        std::fstream file ("worldbot_stats.txt", std::ios::trunc|std::ios::out);
        if (file.is_open()){
            file << "WorldBot stats:" << std::endl;
            file << "\tTotal count: " << totalCount << std::endl;
            file << "\t\tWarriors: " << warriorCount << ", Paladins: " << paladinCount << ", Shamans: " << shamanCount <<
                    ", Hunters: " << hunterCount << ", Rogues: " << rogueCount << ", Druids: " << druidCount << ", Mages: " <<
                    mageCount << ", Warlocks: " << warlockCount << ", Priests: " << priestCount << std::endl;

            file << "\tMovement type:" << std::endl;
            file << "\t\tIdle: " << idleCount << std::endl;
            file << "\t\tWalking: " << walkingCount << std::endl;
            file << "\t\tTaxi flying: " << flightCount << std::endl;

            file << "\tAlive: " << aliveCount << std::endl;
            file << "\tDead: " << deadCount << std::endl;

            file << "\tTeams:" << std::endl;
            file << "\t\tAlliances: " << allianceCount << std::endl;
            file << "\t\tHordes: " << hordeCount << std::endl;

            file.close();
            sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "__________ world bot stats printed.");

            return;
        }
        else
            sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "___ Failed to open file.");
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

void PlayerBotMgr::DeleteWorldBots(){
    for (auto const& itr: m_bots){
        if (dynamic_cast<WorldBotAI*>(itr.second->ai.get()))
            itr.second->requestRemoval = true;
    }
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

std::vector<uint32> PlayerBotMgr::FindAStarPath(Team team, uint32 startMapId, Position posStart, uint32 endMapId, Position posEnd, bool includeFlightPath, bool includeAreaTrigger, bool includeTransport, uint32 maxCreatureLevel, uint32 maxAllianceLevel, uint32 maxHordeLevel){
    std::vector<uint32> result;
    
    // if (startMapId != endMapId){
    //     sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[PlayerBotMgr]::FindAStarPath : start and end maps are different.");
    //     return result;
    // }
    std::set<uint32> excludedNodes;
    if (team == ALLIANCE)
        // Splintertree Post, Zoramgar Outpost, Sun Rock Retreat, Hammerfall, Gromgol Base Camp, Tarren Mill, Ghost Walker Post,
        // Shadowprey Village, Revantusk Village, Flame Crest, Camp Mojache, Valormok, Bloodvenom Post, Bulwark
        excludedNodes = {143, 156, 1297, 99, 1327, 770, 398, 410, 1539, 281, 647, 188, 631, 1704};
    else if (team == HORDE)
        // Astranaar, Southshore, Stonetalon Peak, Refuge Pointe, Rebel Camp, Nijels Point,
        // Theramor Isle, Feathermoon Stronghold, Thalanaar, Talonbranch Glade, Talrendis Point, Morgans Vigil, Chillwind Camp,
        // Lakeshire, Darkshire
        excludedNodes = {118, 769, 1296, 101, 1338, 404, 538, 650, 666, 643, 180, 282, 1696, 1089, 509};
    
    
    std::vector<TravelNode> startNodes;
    std::vector<TravelNode> endNodes;

    for (const auto& node : m_travelNodes){
        if (node.map_id == startMapId)
            startNodes.push_back(node);
        
        if (node.map_id == endMapId)
            endNodes.push_back(node);
    }
    
    if (startNodes.empty()){
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[PlayerBotMgr]::FindAStarPath : no start nodes found on source map.");
        return result;
    }

    if (endNodes.empty()){
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[PlayerBotMgr]::FindAStarPath : no end nodes found on target map.");
        return result;
    }
    
    // Находим ближайший стартовый и целевой узлы
    float minDistStart = std::numeric_limits<float>::max();
    float minDistEnd = std::numeric_limits<float>::max();
    TravelNode startNode;
    TravelNode goalNode;
    
    for (const auto& node : startNodes){
        Vector3 startVec(Vector3({node.x, node.y, node.z}) - Vector3({posStart.x, posStart.y, posStart.z}));
        float lengthToStart = startVec.length();
        if (lengthToStart < minDistStart){
            minDistStart = lengthToStart;
            startNode = node;
        }
    }

    for (const auto& node: endNodes){
        Vector3 endVec(Vector3({node.x, node.y, node.z}) - Vector3({posEnd.x, posEnd.y, posEnd.z}));
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
       TravelNode node;
        float g; // стоимость от старта до текущего узла
        float h; // эвристика (расстояние до цели)
        float f; // g + h
        uint32 parentId;
        
        AStarNode(TravelNode n, float gCost, float hCost, uint32 pId) 
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
            std::vector<TravelNode> path;
            uint32 currentNodeId = goalNode.id;
            while (currentNodeId != 0){
                // path.push_back(m_worldBotConfig.travelNode[currentNodeId]);
                path.push_back(GetTravelNode(currentNodeId));
                currentNodeId = cameFrom[currentNodeId];
            }
            std::reverse(path.begin(), path.end());
            // result = path;
            for (const auto& node: path)
                result.push_back(node.id);
            break;
        }
        
        // Получаем соседей
        std::vector<TravelNodeLink> neighbours = GetTravelNodeLinksById(team, current.node.id, includeFlightPath, includeAreaTrigger, includeTransport, maxCreatureLevel);
        
        for (const auto& nodeLink : neighbours){
            if (excludedNodes.count(nodeLink.to_node_id))
                continue;

            if (closedSet[nodeLink.to_node_id])
                continue;
                
            // WorldBotConfig::TravelNode neighborNode = m_worldBotConfig.travelNode[nodeLink.to_node_id];
            TravelNode neighborNode = GetTravelNode(nodeLink.to_node_id);
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
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[PlayerBotMgr]::FindAStarPath : failed to find (team %d) from node %d (%s) to node %d (%s).", team, startNode.id, startNode.name.c_str(), goalNode.id, goalNode.name.c_str());
    }
    
    return result;
}

// Вспомогательный метод для эвристики
float PlayerBotMgr::CalculateHeuristic(const TravelNode& from, const TravelNode& to){
    Vector3 fromVec(from.x, from.y, from.z);
    Vector3 toVec(to.x, to.y, to.z);
    return (toVec - fromVec).length();
}

std::vector<TravelNodeLink> PlayerBotMgr::GetTravelNodeLinksById(Team team, uint32 id, bool includeFlightPath, 
        bool includeAreaTrigger, bool includeTransport, uint32 maxCreatureLevel,
        uint32 maxAllianceCreatureLevel, uint32 maxHordeCreatureLevel) const {
    std::vector<TravelNodeLink> result;
    for (const auto& nodeLink: m_travelNodeLinks){

        if (nodeLink.node_id == id){
            // if (nodeLink.max_creature0 > (maxCreatureLevel + 3))
            //     continue;
            
            // if (team == ALLIANCE && nodeLink.max_creature2 > (maxHordeCreatureLevel + 3))
            //     continue;
            // else if (team == HORDE && nodeLink.max_creature1 > (maxAllianceCreatureLevel + 3))
            //     continue;
            
            if (nodeLink.type == PATH_TYPE_FLIGHTPATH){
                if (!includeFlightPath)
                    continue;
                
                const TaxiPathEntry* taxiEntry = sTaxiPathStore.LookupEntry(nodeLink.object);
                if (!taxiEntry){
                    sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "[PlayerBotMgr]::GetTravelNodeLinksById : error getting taxiEntry with id #%d.", nodeLink.object);
                    continue;
                }
                const TaxiFlightNodeInfo& flightInfo = GetTaxiFlightNodeInfoById(taxiEntry->from);
                const TaxiFlightNodeInfo& toInfo = GetTaxiFlightNodeInfoById(taxiEntry->to);
                if (team == ALLIANCE && !flightInfo.mount_creature_id2 || !toInfo.mount_creature_id2)
                    continue;
                else if (team == HORDE && !flightInfo.mount_creature_id1 || !toInfo.mount_creature_id1)
                    continue;
            }

            else if (nodeLink.type == PATH_TYPE_AREATRIGGER && !includeAreaTrigger)
                continue;

            else if (nodeLink.type == PATH_TYPE_TRANSPORT && !includeTransport)
                continue;
                
            result.push_back(nodeLink);
        }
    }

    return result;
}

TaxiFlightNodeInfo PlayerBotMgr::GetTaxiFlightNodeInfoById(uint32 id) const {
    TaxiFlightNodeInfo result;
    for (const auto& entry: m_taxiFlightNodeInfo){
        if (entry.id == id){
            result = entry;
            break;
        }
    }
    return result;
}

std::vector<TravelNode> PlayerBotMgr::GetTravelNodesFilterByRegex(std::string regex){
    std::vector<TravelNode> result;
    for (const auto& node: m_travelNodes){
        std::regex pattern {regex};
        if (std::regex_search(node.name, pattern)){
            result.push_back(node);
        }
    }
    return result;
}

TravelNode PlayerBotMgr::GetTravelNode(uint32 id) const {
    TravelNode travelNode;
    for (const auto& node: m_travelNodes){
        if (id == node.id){
            travelNode = node;
            break;
        }
    }
    return travelNode;
}

std::vector<TravelNodePath> PlayerBotMgr::GetTravelNodePath(uint32 node_id, uint32 to_node_id){
    std::vector<TravelNodePath> result;
    for (const auto& path: m_travelNodePaths){
        if (path.node_id == node_id && path.to_node_id == to_node_id)
            result.push_back(path);
    }
    
    return result;
}

TravelNodeLink PlayerBotMgr::GetTravelNodeLink(uint32 node_id, uint32 to_node_id){
    TravelNodeLink result;
    for (const auto& link: m_travelNodeLinks){
        if (link.node_id == node_id && link.to_node_id == to_node_id){
            result = link;
            break;
        }
    }
    return result;
}

std::vector<uint32> PlayerBotMgr::GetSafeSpawnNodes(Team team, uint32 level){
    std::vector<uint32> result;
    std::vector<std::string> locNames;
    std::set<uint32> excludedNodes;
    if (team == ALLIANCE){
        // Splintertree Post, Zoramgar Outpost, Sun Rock Retreat, Hammerfall, Gromgol Base Camp, Tarren Mill, Ghost Walker Post,
        // Shadowprey Village, Revantusk Village, Flame Crest, Camp Mojache, Valormok, Bloodvenom Post, Bulwark, Gnomeregan portal,
        // [PH] Teleport to Booty Bay 
        excludedNodes = {143, 156, 1297, 99, 1327, 770, 398, 410, 1539, 281, 647, 188, 631, 1704, 270, 1810};
        if (level <= 6)
            locNames = {"Aldrassil", "Northshire", "Coldridge"};
        
        else if (level <= 12)
            locNames = {"Elwynn", "Teldrassil", "Dun Morogh"};

        else if (level <= 20)
            locNames = {"Westfall", "Darkshore", "Loch Modan"};

        else if (level <= 25)
            locNames = {"Redridge", "Darkshire", "Menethil", "Southshore", "Astranaar"};

        else if (level <= 30)
            locNames = {"Duskwood", "Astranaar", "Menethil", "Stonetalon Peak"};

        else if (level <= 35)
            locNames = {"Refuge Pointe", "Rebel Camp", "Southshore", "Nijels"};

        else if (level <= 40)
            locNames = {"Refuge Pointe", "Rebel Camp", "Southshore", "Nijels", "Booty Bay", "Thelsamar", "Theramore", "Nether"};

        else if (level <= 45)
            locNames = {"Aerie", "Booty Bay", "Rebel Camp", "Thelsamar", "Theramore", "Gadgetzan"};

        else if (level <= 50)
            locNames = {"Aerie", "Thorium Point", "Gadgetzan", "Feathermoon", "Thalanaar", "Nether"};

        else if (level <= 55)
            locNames = {"Nether", "Marshals", "Talrendis", "Talonbranch", "Chillwind Camp", "Deadwind", "Morgans Vigil"};

        else if (level <= 60)
            locNames = {"Chillwind Camp", "Lights", "Morgans Vigil", "Deadwind", "Everlook", "Cenarion Hold"};
    }
    else if (team == HORDE){
        // Razorofen Downs, Razorfen Kraul, Astranaar, Southshore, Stonetalon Peak, Refuge Pointe, Rebel Camp, Nijels Point,
        // Theramor Isle, Feathermoon Stronghold, Thalanaar, Talonbranch Glade, Talrendis Point, Morgans Vigil, Chillwind Camp,
        // Lakeshire, Darkshire, Gnomeregan Portal, [PH] Teleport to Booty Bay
        excludedNodes = {1510, 1511, 118, 769, 1296, 101, 1338, 404, 538, 650, 666, 643, 180, 282, 1696, 1089, 509, 270, 1810};
        if (level <= 6)
            locNames = {"Valley of Trials", "Red Cloud Mesa", "Deathknell"};

        else if (level <= 12)
            locNames = {"Durotar", "Mulgor", "Tirisfal Glades"};

        else if (level <= 20)
            locNames = {"Barrens", "Silverpine"};

        else if (level <= 25)
            locNames = {"Zoramgar", "Sun Rock", "Tarren", "Taurajo"};
        
        else if (level <= 30)
            locNames = {"Zoramgar", "Splintertree", "Freewind", "Tarren", "Stonetalon"};

        else if (level <= 35)
            locNames = {"Freewind", "Hammerfall", "Tarren", "Gromgol", "Ghost"};

        else if (level <= 40)
            locNames = {"Hammerfall", "Tarren", "Gromgol", "Booty Bay", "Stonard", "Taurajo", "Kargath", "Shadowprey"};

        else if (level <= 45)
            locNames = {"Booty Bay", "Gromgol", "Stonard", "Kargath", "Gadgetzan", "Revantusk"};

        else if (level <= 50)
            locNames = {"Mojache", "Gadgetzan", "Revantusk", "Thorium Point", "Stonard"};

        else if (level <= 55)
            locNames = {"Bulwark", "Deadwind", "Flame Crest", "Marshals", "Bloodvenom", "Valormok", "Kargath"};

        else if (level <= 60)
            locNames = {"Bulwark", "Deadwind", "Flame Crest", "Lights", "Everlook", "Cenarion Hold"};
    }

    for (const auto& name: locNames){
        for (const auto& node: m_travelNodes){
            std::regex regex {name};
            if (std::regex_search(node.name, regex) && !excludedNodes.count(node.id))
                result.push_back(node.id);
        }
    }

    return result;
}

std::vector<uint32> PlayerBotMgr::GetGankerSpawnNodes(Team team, uint32 level){
    std::vector<uint32> result;
    std::vector<std::string> locNames;
    std::set<uint32> excludedNodes;
    if (team == ALLIANCE){
        // Splintertree Post, Zoramgar Outpost, Sun Rock Retreat, Hammerfall, Gromgol Base Camp, Tarren Mill, Ghost Walker Post,
        // Shadowprey Village, Revantusk Village, Flame Crest, Camp Mojache, Valormok, Bloodvenom Post, Bulwark
        excludedNodes = {143, 156, 1297, 99, 1327, 770, 398, 410, 1539, 281, 647, 188, 631, 1704};

        if (level >= 24 && level <= 28)
            locNames = {"Stonetalon", "Hillsbrad"};
        else if (level >= 29 && level <= 33)
            locNames = {"Thousand"};
    }
    else if (team == HORDE){
        // Astranaar, Southshore, Stonetalon Peak, Refuge Pointe, Rebel Camp, Nijels Point,
        // Theramor Isle, Feathermoon Stronghold, Thalanaar, Talonbranch Glade, Talrendis Point, Morgans Vigil, Chillwind Camp,
        // Lakeshire, Darkshire
        excludedNodes = {118, 769, 1296, 101, 1338, 404, 538, 650, 666, 643, 180, 282, 1696, 1089, 509};

        if (level >= 24 && level <= 28)
            locNames = {"Wetlands", "Redridge"};
        else if (level >= 29 && level <= 33)
            locNames = {"Duskwood"};
    }
    
    for (const auto& name: locNames){
        for (const auto& node: m_travelNodes){
            std::regex regex {name};
            if (std::regex_search(node.name, regex) && !excludedNodes.count(node.id))
                result.push_back(node.id);
        }
    }

    return result;
}

std::vector<uint32> PlayerBotMgr::GetSafeTravelPoints(Team team, uint32 level){
    std::vector<uint32> result;
    std::vector<std::string> locNames;
    std::set<uint32> excludedNodes;
    if (team == ALLIANCE){
        // Splintertree Post, Zoramgar Outpost, Sun Rock Retreat, Hammerfall, Gromgol Base Camp, Tarren Mill, Ghost Walker Post,
        // Shadowprey Village, Revantusk Village, Flame Crest, Camp Mojache, Valormok, Bloodvenom Post, Bulwark, [PH] Teleport to Booty Bay
        excludedNodes = {143, 156, 1297, 99, 1327, 770, 398, 410, 1539, 281, 647, 188, 631, 1704, 1810};
        if (level <= 6)
            locNames = {"Aldrassil", "Northshire", "Coldridge"};
        
        else if (level <= 12)
            locNames = {"Elwynn", "Teldrassil", "Dun Morogh"};

        else if (level <= 20)
            locNames = {"Westfall", "Darkshore", "Loch Modan"};

        else if (level <= 25)
            locNames = {"Redridge", "Darkshire", "Wetlands", "Southshore", "Ashenvale"};

        else if (level <= 30)
            locNames = {"Duskwood", "Ashenvale", "Wetlands", "Stonetalon"};

        else if (level <= 35)
            locNames = {"Arathi Highlands", "Stranglethorn", "Hillsbrad", "Desolace"};

        else if (level <= 40)
            locNames = {"Arathi Highlands", "Stranglethorn", "Hillsbrad", "Desolace", "Badlands", "Dustwallow", "Blasted"};

        else if (level <= 45)
            locNames = {"Hinterlands", "Stranglethorn", "Badlands", "Dustwallow", "Tanaris"};

        else if (level <= 50)
            locNames = {"Hinterlands", "Searing Gorge", "Tanaris", "Feralas", "Blasted"};

        else if (level <= 55)
            locNames = {"Blasted", "Ungoro", "Azshara", "Felwood", "Western Plague", "Deadwind", "Burning Steppes"};

        else if (level <= 60)
            locNames = {"Western Plague", "Eatern Plague", "Burning Steppes", "Deadwind", "Winterspring", "Silithus"};
    }
    else if (team == HORDE){
        // Astranaar, Southshore, Stonetalon Peak, Refuge Pointe, Rebel Camp, Nijels Point,
        // Theramor Isle, Feathermoon Stronghold, Thalanaar, Talonbranch Glade, Talrendis Point, Morgans Vigil, Chillwind Camp,
        // Lakeshire, Darkshire, [PH] Teleport to Booty Bay
        excludedNodes = {118, 769, 1296, 101, 1338, 404, 538, 650, 666, 643, 180, 282, 1696, 1089, 509, 1810};
        if (level <= 6)
            locNames = {"Valley of Trials", "Red Cloud Mesa", "Deathknell"};

        else if (level <= 12)
            locNames = {"Durotar", "Mulgor", "Tirisfal Glades"};

        else if (level <= 20)
            locNames = {"Barrens", "Silverpine"};

        else if (level <= 25)
            locNames = {"Ashenvale", "Stonetalon", "Hillsbrad", "Barrens"};
        
        else if (level <= 30)
            locNames = {"Ashenvale", "Thousand", "Hillsbrad", "Stonetalon"};

        else if (level <= 35)
            locNames = {"Arathi Highlands", "Stranglethorn", "Hillsbrad", "Desolace"};

        else if (level <= 40)
            locNames = {"Arathi Highlands", "Stranglethorn", "Hillsbrad", "Desolace", "Badlands", "Dustwallow", "Blasted"};

        else if (level <= 45)
            locNames = {"Hinterlands", "Stranglethorn", "Badlands", "Dustwallow", "Tanaris"};

        else if (level <= 50)
            locNames = {"Hinterlands", "Searing Gorge", "Tanaris", "Feralas", "Blasted"};

        else if (level <= 55)
            locNames = {"Blasted", "Ungoro", "Azshara", "Felwood", "Western Plague", "Deadwind", "Burning Steppes"};

        else if (level <= 60)
            locNames = {"Western Plague", "Eatern Plague", "Burning Steppes", "Deadwind", "Winterspring", "Silithus"};
    }

    for (const auto& name: locNames){
        for (const auto& node: m_travelNodes){
            if (excludedNodes.count(node.id))
                continue;
            
            // sort links to this node
            std::list<TravelNodeLink> links;
            for (const auto& link: m_travelNodeLinks){
                if (link.to_node_id == node.id)
                    links.push_back(link);
            }

            // count links where difference between me and hostile creature > 3
            uint32 count = 0;
            for (const auto& link: links){
                if ( (link.max_creature0 - level) > 3)
                    count++;
            }

            // if no good link found then skip this node
            if (count == links.size())
                continue;

            std::regex regex {name};
            if (std::regex_search(node.name, regex))
                result.push_back(node.id);
        }
    }

    return result;
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

bool ChatHandler::HandleWorldBotAddCommand(char * args){
    Player* pPlayer = m_session->GetPlayer();
    if (!pPlayer)
        return false;

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

    uint8 botRace = SelectRandomRaceForClass(botClass, HORDE/*pPlayer->GetTeam()*/);
    if (!botRace)
    {
        SendSysMessage("Unable to select race for bot.");
        SetSentErrorMessage(true);
        return false;
    }

    float x, y, z;
    pPlayer->GetNearPoint(pPlayer, x, y, z, 0, 5.0f, frand(0.0f, 6.0f));

    WorldBotAI* ai = new WorldBotAI(nullptr, nullptr, botRole, botRace, botClass, botLevel, pPlayer->GetMapId(), pPlayer->GetMap()->GetInstanceId(), x, y, z, pPlayer->GetOrientation());
    if (sPlayerBotMgr.AddBot(ai))
        SendSysMessage("New world bot added.");
    else
    {
        delete ai;
        SendSysMessage("Error spawning bot.");
        SetSentErrorMessage(true);
        return false;
    }

    return true;
}

bool ChatHandler::HandleWorldBotTradeEnchantCommand(char * args){
    Player* pPlayer = GetSession()->GetPlayer();
    if (!pPlayer)
        return false;

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

    if (!pTarget->GetGroup() || !pPlayer->GetGroup() || pPlayer->GetGroup() != pTarget->GetGroup()){
        SendSysMessage("The bot must be in your group first.");
        SetSentErrorMessage(true);
        return false;
    }

    uint32 spellId = 0;
    ExtractUInt32(&args, spellId);
    if (spellId){
        pAI->CastEnchantSpell(spellId);
        return true;
    }
    else if (char* arg = ExtractArg(&args)){
        std::vector<const SpellEntry*> spells = pAI->GetProfessionSpellsFilterByRegex(std::string(arg));
        std::string msg;

        uint32 lang = 0;
        if (pPlayer->GetTeam() == ALLIANCE)
            lang = LANG_COMMON;
        else if (pPlayer->GetTeam() == HORDE)
            lang = LANG_ORCISH;
        
        if (spells.empty()){
            msg = "No profession spells found for filter word \"" + std::string(arg) + "\".";
            std::unique_ptr<WorldPackets::Chat::ChatMessage> data = std::make_unique<WorldPackets::Chat::ChatMessage>();
            data->lang = lang;
            data->whisperTargetOrChannel = pPlayer->GetName();
            data->type = CHAT_MSG_WHISPER;
            pTarget->GetSession()->QueuePacket(std::move(data));
            return true;
        }

        msg = "Found profession spells for word \"" + std::string(arg) + "\":";
        auto data = std::make_unique<WorldPackets::Chat::ChatMessage>();
        data->message = msg;
        data->lang = lang;
        data->whisperTargetOrChannel = pPlayer->GetName();
        data->type = CHAT_MSG_WHISPER;
        pTarget->GetSession()->QueuePacket(std::move(data));
        msg.clear();
        int32 i = 1;
        for (const auto& spell: spells){
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
    else {
        SendSysMessage("Incorrect syntax. Usage: .worldbot enchant <prof_spell_id | filter_word>");
        SetSentErrorMessage(true);
        return false;
    }

    return true;

}

bool ChatHandler::HandleWorldBotRemoveCommand(char * args){
    Player* pPlayer = GetSession()->GetPlayer();
    if (!pPlayer)
        return false;
        
    if (char* all = ExtractArg(&args)){
        if (std::string(all) == "all")
            sPlayerBotMgr.DeleteWorldBots();
        return true;
    }

    Player* pTarget = pPlayer->GetSelectedPlayer();
    if (!pTarget)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    if (pTarget->AI())
    {
        if (WorldBotAI* pAI = dynamic_cast<WorldBotAI*>(pTarget->AI()))
        {
            pAI->botEntry->requestRemoval = true;
            return true;
        }
    }

    SendSysMessage("Target is not a world bot.");
    SetSentErrorMessage(true);
    return false;
}

bool ChatHandler::HandleWorldBotFindPathCommand(char * args){
    Player* pPlayer = GetSession()->GetPlayer();
    if (!pPlayer)
        return false;

        // Player* pTarget = pPlayer->GetSelectedPlayer();
    Map* map = sMapMgr.FindMap(0);
    Player* pTarget = map->GetPlayer(pPlayer->GetSelectionGuid());
    if (!pTarget){
        map = sMapMgr.FindMap(1);
        pTarget = map->GetPlayer(pPlayer->GetSelectionGuid());
    }

    if (!pTarget){
        SendSysMessage("No target selected.");
        SetSentErrorMessage(true);
        return false;
    }
    
    std::vector<uint32> vec = sPlayerBotMgr.FindAStarPath(pTarget->GetTeam(), pTarget->GetMapId(), pTarget->GetPosition(),
        pPlayer->GetMapId(), pPlayer->GetPosition(), true, true, true, pTarget->GetLevel());

    if (vec.empty()){
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[ChatHandler]::HandleWorldBotFindPathCommand : no nodes found.");
        SendSysMessage("No nodes found.");
        return true;
    }

    std::string msg = "[ChatHandler] : Found path:\n";
    for (auto& id: vec){
        msg.append("[" + std::to_string(id) + ", \"" + sPlayerBotMgr.GetTravelNode(id).name + "\"]\n -> ");
    }
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, msg.c_str());
    return true;
}

bool ChatHandler::HandleWorldBotSpawnCommand(char * args){
    Player* pPlayer = GetSession()->GetPlayer();
    if (!pPlayer)
        return false;

    uint32 minLevel = 0;
    ExtractUInt32(&args, minLevel);
    if (!minLevel){
        SendSysMessage("Error getting minimal level for worldbots.");
        SetSentErrorMessage(true);
        return false;
    }
    uint32 maxLevel = 0;
    ExtractUInt32(&args, maxLevel);
    if (!minLevel){
        SendSysMessage("Error getting maximum level for worldbots.");
        SetSentErrorMessage(true);
        return false;
    }

    if (minLevel > maxLevel){
        uint32 temp = minLevel;
        minLevel = maxLevel;
        maxLevel = minLevel;
    }

    uint32 count = 0;
    ExtractUInt32(&args, count);

    uint32 allianceCount = 0;
    uint32 hordeCount = 0;
    for (uint32 i=0; i < count; ++i){
        Team botTeam = PickRandomValue(ALLIANCE, HORDE);
        uint32 botLevel = urand(minLevel, maxLevel);
        std::vector<uint32> vec;
        bool isGanker = false;
        if (urand(0, 100) <= 33)
            isGanker = true;
        if (isGanker && botLevel >= 26 && botLevel <= 33)
            vec = sPlayerBotMgr.GetGankerSpawnNodes(botTeam, botLevel);
        else
            vec = sPlayerBotMgr.GetSafeSpawnNodes(botTeam, botLevel);
        if (!vec.size()){
            SendSysMessage("No nodes found for specified level range.");
            SetSentErrorMessage(true);
            return false;
        }
        uint32 nodeId = SelectRandomContainerElement(vec);
        const TravelNode& node = sPlayerBotMgr.GetTravelNode(nodeId);
        CombatBotRoles botRole = ROLE_INVALID;
        uint8 botClass = CLASS_WARRIOR;
        if (botTeam == ALLIANCE)
            botClass = PickRandomValue(CLASS_WARRIOR, CLASS_PALADIN, CLASS_HUNTER, CLASS_ROGUE, CLASS_DRUID, CLASS_MAGE, CLASS_WARLOCK, CLASS_PRIEST);
        else if (botTeam == HORDE)
            botClass = PickRandomValue(CLASS_WARRIOR, CLASS_SHAMAN, CLASS_HUNTER, CLASS_ROGUE, CLASS_DRUID, CLASS_MAGE, CLASS_WARLOCK, CLASS_PRIEST);
        
        if (botClass == CLASS_WARRIOR)
            botRole = PickRandomValue(ROLE_MELEE_DPS, ROLE_TANK);
        else if (botClass == CLASS_PALADIN)
            botRole = PickRandomValue(ROLE_MELEE_DPS, ROLE_HEALER);
        else if (botClass == CLASS_SHAMAN)
            botRole = PickRandomValue(ROLE_MELEE_DPS, ROLE_RANGE_DPS, ROLE_HEALER);
        else if (botClass == CLASS_HUNTER || botClass == CLASS_MAGE || botClass == CLASS_WARLOCK)
            botRole = ROLE_RANGE_DPS;
        else if (botClass == CLASS_ROGUE)
            botRole = ROLE_MELEE_DPS;
        else if (botClass == CLASS_DRUID)
            botRole = PickRandomValue(ROLE_RANGE_DPS, ROLE_HEALER, ROLE_MELEE_DPS);
            

        // CombatBotRoles botRole = PickRandomValue(ROLE_MELEE_DPS, ROLE_RANGE_DPS, ROLE_TANK /*, ROLE_HEALER */);
        // if (botRole == ROLE_MELEE_DPS){
        //     std::vector<Classes> vec {CLASS_WARRIOR, CLASS_ROGUE, CLASS_DRUID};
        //     if (botTeam == ALLIANCE)
        //         vec.push_back(CLASS_PALADIN);
        //     else
        //         vec.push_back(CLASS_SHAMAN);

        //     botClass = SelectRandomContainerElement(vec);
        // }
        // else if (botRole == ROLE_RANGE_DPS){
        //     std::vector<Classes> vec {CLASS_WARLOCK, CLASS_MAGE, CLASS_PRIEST, CLASS_HUNTER, CLASS_DRUID};
        //     if (botTeam == HORDE)
        //         vec.push_back(CLASS_SHAMAN);

        //     botClass = SelectRandomContainerElement(vec);
        // }
        // else if (botRole == ROLE_TANK){
        //     botClass = CLASS_WARRIOR;
        // }
        // else if (botRole == ROLE_HEALER){
        //     std::vector<Classes> vec {CLASS_PRIEST, CLASS_DRUID};
        //     if (botTeam == ALLIANCE)
        //         vec.push_back(CLASS_PALADIN);
        //     else
        //         vec.push_back(CLASS_SHAMAN);

        //     botClass = SelectRandomContainerElement(vec);
        // }
        uint8 botRace = SelectRandomRaceForClass(botClass, botTeam);
        WorldBotAI* pAI = new WorldBotAI(nullptr, nullptr, botRole, botRace, botClass, (uint8)botLevel, node.map_id, pPlayer->GetInstanceId(), node.x, node.y, node.z, frand(0.0f, 6.28f));
        pAI->m_safeSpawnNodes = vec;
        if (isGanker)
            pAI->m_safeTravelPoints = vec;
        else
            pAI->m_safeTravelPoints = sPlayerBotMgr.GetSafeTravelPoints(botTeam, botLevel);
        pAI->m_updateTimer.Reset(urand(250, 5000));
        if (!sPlayerBotMgr.AddBot(pAI))
        //     ;
        // else
        {
            delete pAI;
            SendSysMessage("Error spawning bot.");
            SetSentErrorMessage(true);
            return false;
        }
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "___ Spawned in %s.", node.name.c_str());
        if (botTeam == ALLIANCE)
            allianceCount++;
        else if (botTeam == HORDE)
            hordeCount++;
        // else
        //     SendSysMessage("New world bot added.");
        
        // if (botRole == ROLE_HEALER){
        //     botLevel = urand(minLevel, maxLevel);
        //     botTeam = PickRandomValue(ALLIANCE, HORDE);
        //     botRole = PickRandomValue(ROLE_MELEE_DPS, ROLE_RANGE_DPS, ROLE_TANK);

        //     if (botRole == ROLE_MELEE_DPS){
        //         std::vector<Classes> vec {CLASS_WARRIOR, CLASS_ROGUE, CLASS_DRUID};
        //         if (botTeam == ALLIANCE)
        //             vec.push_back(CLASS_PALADIN);
        //         else
        //             vec.push_back(CLASS_SHAMAN);

        //         botClass = SelectRandomContainerElement(vec);
        //     }
        //     else if (botRole == ROLE_RANGE_DPS){
        //         std::vector<Classes> vec {CLASS_WARLOCK, CLASS_MAGE, CLASS_PRIEST, CLASS_HUNTER, CLASS_DRUID};
        //         if (botTeam == HORDE)
        //             vec.push_back(CLASS_SHAMAN);

        //         botClass = SelectRandomContainerElement(vec);
        //     }
        //     else if (botRole == ROLE_TANK){
        //         botClass = CLASS_WARRIOR;
        //     }

        //     botRace = SelectRandomRaceForClass(botClass, botTeam);
        //     float x, y, z;
        //     pAI->me->GetNearPoint(pAI->me, x, y, z, 0, 5.0f, frand(0.0f, 6.28f));
        //     PartyBotAI* ai = new PartyBotAI(pAI->me, nullptr, botRole, botRace, botClass, (uint8)botLevel, node.map_id, pAI->me->GetInstanceId(), x, y, z, frand(0.0f, 6.28f));
        //     if (!sPlayerBotMgr.AddBot(ai)){
        //         delete ai;
        //         SendSysMessage("Error spawning bot.");
        //         SetSentErrorMessage(true);
        //         return false;
        //     }
        // }
        // else if (urand(0, 100) <= 35){
        //     botLevel = urand(minLevel, maxLevel);
        //     botTeam = PickRandomValue(ALLIANCE, HORDE);
        //     botRole = PickRandomValue(ROLE_MELEE_DPS, ROLE_RANGE_DPS, ROLE_TANK);

        //     if (botRole == ROLE_MELEE_DPS){
        //         std::vector<Classes> vec {CLASS_WARRIOR, CLASS_ROGUE, CLASS_DRUID};
        //         if (botTeam == ALLIANCE)
        //             vec.push_back(CLASS_PALADIN);
        //         else
        //             vec.push_back(CLASS_SHAMAN);

        //         botClass = SelectRandomContainerElement(vec);
        //     }
        //     else if (botRole == ROLE_RANGE_DPS){
        //         std::vector<Classes> vec {CLASS_WARLOCK, CLASS_MAGE, CLASS_PRIEST, CLASS_HUNTER, CLASS_DRUID};
        //         if (botTeam == HORDE)
        //             vec.push_back(CLASS_SHAMAN);

        //         botClass = SelectRandomContainerElement(vec);
        //     }
        //     else if (botRole == ROLE_TANK){
        //         botClass = CLASS_WARRIOR;
        //     }

        //     botRace = SelectRandomRaceForClass(botClass, botTeam);
        //     float x, y, z;
        //     pAI->me->GetNearPoint(pAI->me, x, y, z, 0, 5.0f, frand(0.0f, 6.28f));
        //     PartyBotAI* ai = new PartyBotAI(pAI->me, nullptr, botRole, botRace, botClass, (uint8)botLevel, node.map_id, pAI->me->GetInstanceId(), x, y, z, frand(0.0f, 6.28f));
        //     if (!sPlayerBotMgr.AddBot(ai)){
        //         delete ai;
        //         SendSysMessage("Error spawning bot.");
        //         SetSentErrorMessage(true);
        //         return false;
        //     }
        // }
    }
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, ">> Spawned %d alliances and %d hordes.", allianceCount, hordeCount);

    return true;
}