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

    m_gearQuality = sConfig.GetFloatDefault("WorldBot.GearQuality", 0.5);

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

    // 13- default worldbot specs
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

    // paladin holy
    talentSequence = {
        {1449, 0}, {1449, 1}, {1449, 2}, {1449, 3}, {1449, 4}, // Divine Intellect
        {1432, 0}, {1432, 1}, {1432, 2}, {1432, 3}, {1432, 4}, // Spiritual Focus
        {1435, 0}, // Consecration
        {1444, 0}, {1444, 1}, {1444, 2}, // Healing Light
        {1443, 0}, // Improved Lay on Hands
        {1461, 0}, {1461, 1}, {1461, 2}, {1461, 3}, {1461, 4}, // Illumination
        {1433, 0}, // Divine Favor
        {1446, 0}, {1446, 1}, // Improved Blessing of Wisdom
        {1628, 0}, {1628, 1}, // Unyielding Faith
        {1627, 0}, {1627, 1}, {1627, 2}, {1627, 3}, {1627, 4}, // Holy Power
        {1502, 0}, // Holy Shock
        {1421, 0}, {1421, 1}, {1421, 2}, {1421, 3}, {1421, 4}, // Redoubt
        {1630, 0}, {1630, 1}, {1630, 2}, // Precision
        {1425, 0}, {1425, 1}, // Guardian's Favor
        {1442, 0}, // Blessing of Kings
        {1423, 0}, {1423, 1}, {1423, 2}, {1423, 3}, // Toughness
        {1626, 0}, {1626, 1}, {1626, 2}, // Improved Concentration Aura
        {1443, 1}, // Improved Lay on Hands
        {1423, 4}, // Toughness
    };
    m_worldBotDefaultTalentSpecs["paladin_holy"] = talentSequence;
    talentSequence.clear();

    // paladin retribution
    talentSequence = {
        {1450, 0}, {1450, 1}, {1450, 2}, {1450, 3}, {1450, 4}, // Divine Strength
        {1463, 0}, {1463, 1}, {1463, 2}, {1463, 3}, {1463, 4}, // Improved Seal of Righteousness
        {1435, 0}, // Consecration
        {1401, 0}, {1401, 1}, {1401, 2}, {1401, 3}, {1401, 4}, // Improved Blessing of Might
        {1631, 0}, {1631, 1}, // Improved Judgement
        {1464, 0}, {1464, 1}, {1464, 2}, // Improved Seal of the Crusader
        {1481, 0}, // Seal of Command
        {1634, 0}, {1634, 1}, // Pursuit of Justice
        {1411, 0}, {1411, 1}, {1411, 2}, {1411, 3}, {1411, 4}, // Conviction
        {1632, 0}, {1632, 1}, // Eye for an Eye
        {1409, 0}, // Sanctity Aura
        {1410, 0}, {1410, 1}, {1410, 2}, // Two-Handed Weapon Specialization
        {1405, 0}, // Improved Retribution Aura
        {1402, 0}, {1402, 1}, {1402, 2}, {1402, 3}, {1402, 4}, // Vengeance
        {1441, 0}, // Repentance
        {1405, 1}, // Improved Retribution Aura
        {1422, 0}, {1422, 1}, {1422, 2}, {1422, 3}, {1422, 4}, // Improved Devotion Aura
        {1630, 0}, {1630, 1}, {1630, 2}, // Precision
    };
    m_worldBotDefaultTalentSpecs["paladin_retri"] = talentSequence;
    talentSequence.clear();

    // shaman elementary
    talentSequence = {
        {563, 0}, {563, 1}, {563, 2}, {563, 3}, {563, 4}, // Concussion
        {561, 0}, {561, 1}, {561, 2}, // Call of Flame
        {572, 0}, {572, 1}, // Earth's Grasp
        {574, 0}, // Elemental Focus
        {562, 0}, {562, 1}, {562, 2}, {562, 3}, // Call of Thunder
        {567, 0}, {567, 1}, // Improved Fire Totems
        {1642, 0}, {1642, 1}, {1642, 2}, // Eye of the Storm
        {565, 0}, // Elemental Fury
        {1641, 0}, {1641, 1}, // Storm Reach
        {562, 4}, // Call of Thunder
        {564, 0}, // Convection
        {721, 0}, {721, 1}, {721, 2}, {721, 3}, {721, 4}, // Lightning Mastery
        {573, 0}, // Elemental Mastery
        {586, 0}, {586, 1}, {586, 2}, {586, 3}, {586, 4}, // Improved Healing Wave
        {589, 0}, {589, 1}, // Improved Reincarnation
        {595, 0}, {595, 1}, {595, 2}, // Totemic Focus
        {582, 0}, // Totemic Mastery
        {564, 1}, {564, 2}, {564, 3}, {564, 4}, // Convection
        {614, 0}, {614, 1}, {614, 2}, {614, 3}, {614, 4}, // Ancestral Knowledge
    };
    m_worldBotDefaultTalentSpecs["shaman_elem"] = talentSequence;
    talentSequence.clear();

    // shaman enhancement
    talentSequence = {
        {614, 0}, {614, 1}, {614, 2}, {614, 3}, {614, 4}, // Ancestral Knowledge
        {605, 0}, {605, 1}, // Improved Ghost Wolf
        {613, 0}, {613, 1}, {613, 2}, // Thundering Strikes
        {617, 0}, // Two-Handed Axes and Maces
        {613, 3}, {613, 4}, // Thundering Strikes
        {610, 0}, {610, 1}, // Enhancing Totems
        {602, 0}, {602, 1}, {602, 2}, {602, 3}, {602, 4}, // Flurry
        {611, 0}, {611, 1}, {611, 2}, // Elemental Weapons
        {1647, 0}, {1647, 1}, // Improved Weapon Totems
        {1643, 0}, {1643, 1}, {1643, 2}, {1643, 3}, {1643, 4}, // Weapon Mastery
        {901, 0}, // Stormstrike
        {586, 0}, {586, 1}, {586, 2}, {586, 3}, {586, 4}, // Improved Healing Wave
        {589, 0}, {589, 1}, // Improved Reincarnation
        {595, 0}, {595, 1}, {595, 2}, // Totemic Focus
        {583, 0}, {583, 1}, {583, 2}, // Nature's Guidance
        {582, 0}, // Totemic Mastery
        {616, 0}, // Parry
        {563, 0}, {563, 1}, {563, 2}, {563, 3}, {563, 4}, // Concussion
    };
    m_worldBotDefaultTalentSpecs["shaman_enhance"] = talentSequence;
    talentSequence.clear();

    // shaman restoration
    talentSequence = {
        {586, 0}, {586, 1}, {586, 2}, {586, 3}, {586, 4}, // Improved Healing Wave
        {595, 0}, {595, 1}, {595, 2}, {595, 3}, {595, 4}, // Totemic Focus
        {582, 0}, // Totemic Mastery
        {587, 0}, {587, 1}, {587, 2}, {587, 3}, // Healing Focus
        {588, 0}, {588, 1}, {588, 2}, {588, 3}, {588, 4}, // Restorative Totems
        {591, 0}, // Nature's Swiftness
        {1648, 0}, {1648, 1}, {1648, 2}, // Healing Way
        {587, 4}, // Healing Focus
        {592, 0}, {592, 1}, {592, 2}, {592, 3}, {592, 4}, // Purification
        {590, 0}, // Mana Tide Totem
        {594, 0}, {594, 1}, {594, 2}, {594, 3}, {594, 4}, // Tidal Mastery
        {581, 0}, {581, 1}, {581, 2}, // Ancestral Healing
        {589, 0}, {589, 1}, // Improved Reincarnation
        {593, 0}, {593, 1}, {593, 2}, {593, 3}, {593, 4}, // Tidal Focus
        {614, 0}, {614, 1}, {614, 2}, {614, 3}, {614, 4}, // Ancestral Knowledge
    };
    m_worldBotDefaultTalentSpecs["shaman_resto"] = talentSequence;
    talentSequence.clear();

    // hunter beastmaster
    talentSequence = {
        {1382, 0}, {1382, 1}, {1382, 2}, {1382, 3}, {1382, 4}, // Improved Aspect of the Hawk
        {1395, 0}, {1395, 1}, {1395, 2}, // Thick Hide
        {1625, 0}, {1625, 1}, // Improved Revive Pet
        {1391, 0}, // Bestial Swiftness
        {1396, 0}, {1396, 1}, {1396, 2}, {1396, 3}, // Unleashed Fury
        {1393, 0}, {1393, 1}, {1393, 2}, {1393, 3}, {1393, 4}, // Ferocity
        {1387, 0}, // Intimidation
        {1388, 0}, {1388, 1}, // Spirit Bond
        {1390, 0}, {1390, 1}, // Bestial Discipline
        {1397, 0}, {1397, 1}, {1397, 2}, {1397, 3}, {1397, 4}, // Frenzy
        {1386, 0}, // Bestial Wrath
        {1396, 4}, // Unleashed Fury
        {1341, 0}, {1341, 1}, {1341, 2}, {1341, 3}, {1341, 4}, // Improved Concussive Shot
        {1344, 0}, {1344, 1}, {1344, 2}, {1344, 3}, {1344, 4}, // Lethal Shots
        {1345, 0}, // Aimed Shot
        {1352, 0}, {1352, 1}, {1352, 2}, // Hawk Eye
        {1343, 0}, // Improved Hunter's Mark
        {1349, 0}, {1349, 1}, {1349, 2}, {1349, 3}, // Mortal Shots
    };
    m_worldBotDefaultTalentSpecs["hunter_bm"] = talentSequence;
    talentSequence.clear();

    // hunter marksmanship
    talentSequence = {
        {1341, 0}, {1341, 1}, {1341, 2}, {1341, 3}, {1341, 4}, // Improved Concussive Shot
        {1344, 0}, {1344, 1}, {1344, 2}, {1344, 3}, {1344, 4}, // Lethal Shots
        {1345, 0}, // Aimed Shot
        {1352, 0}, {1352, 1}, {1352, 2}, // Hawk Eye
        {1343, 0}, // Improved Hunter's Mark
        {1349, 0}, {1349, 1}, {1349, 2}, {1349, 3}, {1349, 4}, // Mortal Shots
        {1353, 0}, // Scatter Shot
        {1347, 0}, {1347, 1}, {1347, 2}, // Barrage
        {1343, 1}, // Improved Hunter's Mark
        {1362, 0}, {1362, 1}, {1362, 2}, {1362, 3}, {1362, 4}, // Ranged Weapon Specialization
        {1361, 0}, // Trueshot Aura
        {1301, 0}, {1301, 1}, {1301, 2}, // Humanoid Slaying
        {1311, 0}, {1311, 1}, // Deflection
        {1304, 0}, {1304, 1}, {1304, 2}, {1304, 3}, {1304, 4}, // Entrapment
        {1308, 0}, // Deterrence
        {1306, 0}, {1306, 1}, // Clever Traps
        {1343, 2}, {1343, 3}, {1343, 4}, // Improved Hunter's Mark
        {1382, 0}, {1382, 1}, {1382, 2}, {1382, 3}, // Improved Aspect of the Hawk
    };
    m_worldBotDefaultTalentSpecs["hunter_mm"] = talentSequence;
    talentSequence.clear();

    // hunter survival
    talentSequence = {
        {1311, 0}, {1311, 1}, {1311, 2}, {1311, 3}, {1311, 4}, // Deflection
        {1621, 0}, {1621, 1}, // Savage Strikes
        {1305, 0}, {1305, 1}, {1305, 2}, // Improved Wing Clip
        {1308, 0}, // Deterrence
        {1305, 3}, {1305, 4}, // Improved Wing Clip
        {1622, 0}, {1622, 1}, // Survivalist
        {1310, 0}, {1310, 1}, {1310, 2}, // Surefooted
        {1622, 2}, {1622, 3}, // Survivalist
        {1312, 0}, // Counterattack
        {1321, 0}, {1321, 1}, {1321, 2}, // Killer Instinct
        {1622, 4}, // Survivalist
        {1341, 0}, {1341, 1}, {1341, 2}, {1341, 3}, {1341, 4}, // Improved Concussive Shot
        {1344, 0}, {1344, 1}, {1344, 2}, {1344, 3}, {1344, 4}, // Lethal Shots
        {1345, 0}, // Aimed Shot
        {1352, 0}, {1352, 1}, {1352, 2}, // Hawk Eye
        {1342, 0}, // Efficiency
        {1349, 0}, {1349, 1}, {1349, 2}, {1349, 3}, {1349, 4}, // Mortal Shots
        {1353, 0}, // Scatter Shot
        {1303, 0}, {1303, 1}, {1303, 2}, {1303, 3}, {1303, 4} // Lightning Reflexes
    };
    m_worldBotDefaultTalentSpecs["hunter_surv"] = talentSequence;
    talentSequence.clear();

    // rogue assasination
    talentSequence = {
        {272, 0}, {272, 1}, // Remorseless Attacks
        {276, 0}, {276, 1}, {276, 2}, // Improved Eviscerate
        {273, 0}, {273, 1}, {273, 2}, // Ruthlessness
        {274, 0}, {274, 1}, // Murder
        {281, 0}, // Relentless Strikes
        {270, 0}, {270, 1}, {270, 2}, {270, 3}, // Malice
        {268, 0}, {268, 1}, {268, 2}, {268, 3}, {268, 4}, // Improved Poisons
        {280, 0}, // Cold Blood
        {270, 4}, // Malice
        {269, 0}, {269, 1}, {269, 2}, // Lethality
        {283, 0}, {283, 1}, {283, 2}, {283, 3}, {283, 4}, // Seal Fate
        {382, 0}, // Vigor
        {269, 3}, {269, 4}, // Lethality
        {203, 0}, {203, 1}, {203, 2}, // Improved Gouge
        {201, 0}, {201, 1}, // Improved Sinister Strike
        {202, 0}, {202, 1}, {202, 2}, // Improved Backstab
        {241, 0}, {241, 1}, {241, 2}, {241, 3}, {241, 4}, // Master of Deception
        {261, 0}, {261, 1}, {261, 2}, {261, 3}, {261, 4}, // Opportunity
    };
    m_worldBotDefaultTalentSpecs["rogue_assa"] = talentSequence;
    talentSequence.clear();

    talentSequence = {
        {201, 0}, {201, 1}, // Improved Sinister Strike
        {203, 0}, {203, 1}, {203, 2}, // Improved Gouge
        {187, 0}, {187, 1}, {187, 2}, {187, 3}, {187, 4}, // Deflection
        {301, 0}, // Riposte
        {181, 0}, {181, 1}, {181, 2}, {181, 3}, {181, 4}, // Precision
        {221, 0}, {221, 1}, {221, 2}, {221, 3}, // Dual Wield Specialization
        {223, 0}, // Blade Flurry
        {221, 4}, // Dual Wield Specialization
        {222, 0}, {222, 1}, // Improved Sprint
        {204, 0}, // Endurance
        {1703, 0}, {1703, 1}, // Weapon Expertise
        {1122, 0}, {1122, 1}, {1122, 2}, // Aggression
        {205, 0}, // Adrenaline Rush
        {270, 0}, {270, 1}, {270, 2}, {270, 3}, {270, 4}, // Malice
        {273, 0}, {273, 1}, {273, 2}, // Ruthlessness
        {274, 0}, {274, 1}, // Murder
        {269, 0}, {269, 1}, {269, 2}, {269, 3}, {269, 4}, // Lethality
        {241, 0}, {241, 1}, {241, 2}, {241, 3}, {241, 4}, // Master of Deception
    };
    m_worldBotDefaultTalentSpecs["rogue_combat"] = talentSequence;
    talentSequence.clear();

    // rogue hemorrhage
    talentSequence = {
        {241, 0}, {241, 1}, {241, 2}, {241, 3}, {241, 4}, // Master of Deception
        {244, 0}, {244, 1}, {244, 2}, {244, 3}, {244, 4}, // Camouflage
        {245, 0}, {245, 1}, {245, 2}, // Initiative
        {303, 0}, // Ghostly Strike
        {247, 0}, // Elusiveness
        {1123, 0}, {1123, 1}, {1123, 2}, // Serrated Blades
        {262, 0}, {262, 1}, // Improved Sap
        {681, 0}, // Hemorrhage
        {284, 0}, // Preparation
        {262, 2}, // Improved Sap
        {265, 0}, {265, 1}, // Dirty Deeds
        {1701, 0}, {1701, 1}, // Heightened Senses
        {270, 0}, {270, 1}, {270, 2}, {270, 3}, {270, 4}, // Malice
        {273, 0}, {273, 1}, {273, 2}, // Ruthlessness
        {274, 0}, {274, 1}, // Murder
        {281, 0}, // Relentless Strikes
        {269, 0}, {269, 1}, {269, 2}, {269, 3}, {269, 4}, // Lethality
        {276, 0}, {276, 1}, {276, 2}, // Improved Eviscerate
        {268, 0}, // Improved Poisons
        {280, 0}, // Cold Blood
        {203, 0}, {203, 1}, {203, 2}, // Improved Gouge
    };
    m_worldBotDefaultTalentSpecs["rogue_hemo"] = talentSequence;
    talentSequence.clear();

    // druid balance
    talentSequence = {
        {761, 0}, // Nature's Grasp
        {921, 0}, {921, 1}, {921, 2}, {921, 3}, // Improved Nature's Grasp
        {763, 0}, {763, 1}, {763, 2}, {763, 3}, {763, 4}, // Improved Moonfire
        {764, 0}, {764, 1}, // Nature's Reach
        {762, 0}, {762, 1}, {762, 2}, // Improved Wrath
        {784, 0}, {784, 1}, {784, 2}, {784, 3}, {784, 4}, // Improved Starfire
        {789, 0}, // Nature's Grace
        {792, 0}, {792, 1}, {792, 2}, {792, 3}, // Vengeance
        {790, 0}, {790, 1}, {790, 2}, {790, 3}, {790, 4}, // Moonfury
        {793, 0}, // Moonkin Form
        {821, 0}, {821, 1}, {821, 2}, {821, 3}, {821, 4}, // Improved Mark of the Wild
        {824, 0}, {824, 1}, {824, 2}, {824, 3}, {824, 4}, // Improved Healing Touch
        {827, 0}, // Insect Swarm
        {829, 0}, {829, 1}, {829, 2}, // Reflection
        {792, 4}, // Vengeance
        {783, 0}, {783, 1}, {783, 2}, // Moonglow
        {762, 3}, {762, 4}, // Improved Wrath
    };
    m_worldBotDefaultTalentSpecs["druid_balance"] = talentSequence;
    talentSequence.clear();

    // druid feral
    talentSequence = {
        {796, 0}, {796, 1}, {796, 2}, {796, 3}, {796, 4}, // Ferocity
        {799, 0}, {799, 1}, {799, 2}, {799, 3}, {799, 4}, // Feral Instinct
        {807, 0}, {807, 1}, // Feline Swiftness
        {798, 0}, {798, 1}, {798, 2}, // Sharpened Claws
        {800, 0}, {800, 1}, // Blood Frenzy
        {803, 0}, {803, 1}, {803, 2}, // Predatory Strikes
        {1162, 0}, // Faerie Fire (Feral)
        {805, 0}, {805, 1}, // Savage Fury
        {802, 0}, {802, 1}, // Improved Shred
        {808, 0}, {808, 1}, {808, 2}, {808, 3}, {808, 4}, // Heart of the Wild
        {809, 0}, // Leader of the Pack
        {822, 0}, {822, 1}, {822, 2}, {822, 3}, {822, 4}, // Furor
        {761, 0}, // Nature's Grasp
        {921, 0}, {921, 1}, {921, 2}, {921, 3}, // Improved Nature's Grasp
        {781, 0}, {781, 1}, {781, 2}, // Natural Shapeshifter
        {791, 0}, {791, 1}, {791, 2}, {791, 3}, {791, 4}, // Natural Weapons
        {788, 0}, // Omen of Clarity
        {804, 0}, // Feral Charge
    };
    m_worldBotDefaultTalentSpecs["druid_feral_cat"] = talentSequence;
    talentSequence.clear();

    // druid restoration
    talentSequence = {
        {821, 0}, {821, 1}, {821, 2}, {821, 3}, {821, 4}, // Improved Mark of the Wild
        {823, 0}, {823, 1}, {823, 2}, {823, 3}, {823, 4}, // Nature's Focus
        {827, 0}, // Insect Swarm
        {824, 0}, {824, 1}, {824, 2}, {824, 3}, // Improved Healing Touch
        {843, 0}, {843, 1}, {843, 2}, {843, 3}, // Tranquil Spirit
        {824, 4}, // Improved Healing Touch
        {831, 0}, // Nature's Swiftness
        {828, 0}, {828, 1}, {828, 2}, // Gift of Nature
        {843, 4}, // Tranquil Spirit
        {825, 0}, {825, 1}, {825, 2}, {825, 3}, {825, 4}, // Improved Regrowth
        {844, 0}, // Swiftmend
        {828, 3}, {828, 4}, // Gift of Nature
        {829, 0}, {829, 1}, {829, 2}, // Reflection
        {830, 0}, {830, 1}, {830, 2}, // Improved Rejuvenation
        {841, 0}, {841, 1}, {841, 2}, {841, 3}, {841, 4}, // Subtlety
        {842, 0}, {842, 1}, // Improved Tranquility
        {761, 0}, // Nature's Grasp
        {921, 0}, {921, 1}, {921, 2}, {921, 3} // Improved Nature's Grasp
    };
    m_worldBotDefaultTalentSpecs["druid_resto"] = talentSequence;
    talentSequence.clear();

    // mage arcane
    talentSequence = {
        {80, 0}, {80, 1}, {80, 2}, {80, 3}, {80, 4}, // Improved Arcane Missiles
        {75, 0}, {75, 1}, {75, 2}, {75, 3}, {75, 4}, // Arcane Concentration
        {85, 0}, // Arcane Resilience
        {74, 0}, {74, 1}, // Arcane Subtlety
        {81, 0}, {81, 1}, // Improved Arcane Explosion
        {88, 0}, {88, 1}, // Improved Counterspell
        {83, 0}, {83, 1}, // Improved Mana Shield
        {81, 2}, // Improved Arcane Explosion
        {86, 0}, // Presence of Mind
        {77, 0}, {77, 1}, {77, 2}, {77, 3}, // Arcane Mind
        {421, 0}, {421, 1}, {421, 2}, // Arcane Instability
        {77, 4}, // Arcane Mind
        {1142, 0}, // Arcane Meditation
        {87, 0}, // Arcane Power
        {1142, 1}, {1142, 2}, // Arcane Meditation
        {30, 0}, {30, 1}, {30, 2}, {30, 3}, {30, 4}, // Impact
        {34, 0}, {34, 1}, {34, 2}, {34, 3}, {34, 4}, // Ignite
        {29, 0}, // Pyroblast
        {1650, 0}, {1650, 1}, {1650, 2}, {1650, 3}, {1650, 4}, // Magic Absorption
        {82, 0}, {82, 1}, // Magic Attunement
    };
    m_worldBotDefaultTalentSpecs["mage_arcane"] = talentSequence;
    talentSequence.clear();

    // mage fire
    talentSequence = {
        {26, 0}, {26, 1}, {26, 2}, {26, 3}, {26, 4}, // Improved Fireball
        {28, 0}, {28, 1}, // Flame Throwing
        {27, 0}, {27, 1}, {27, 2}, // Improved Fire Blast
        {29, 0}, // Pyroblast
        {1141, 0}, {1141, 1}, // Incinerate
        {34, 0}, {34, 1}, // Ignite
        {25, 0}, {25, 1}, {25, 2}, // Improved Scorch
        {34, 2}, {34, 3}, // Ignite
        {32, 0}, // Blast Wave
        {33, 0}, {33, 1}, {33, 2}, // Critical Mass
        {34, 4}, // Ignite
        {35, 0}, {35, 1}, {35, 2}, {35, 3}, {35, 4}, // Fire Power
        {36, 0}, // Combustion
        {74, 0}, {74, 1}, // Arcane Subtlety
        {76, 0}, {76, 1}, {76, 2}, // Arcane Focus
        {75, 0}, {75, 1}, {75, 2}, {75, 3}, {75, 4}, // Arcane Concentration
        {1650, 0}, {1650, 1}, {1650, 2}, {1650, 3}, {1650, 4}, // Magic Absorption
        {88, 0}, {88, 1}, // Improved Counterspell
        {1142, 0}, {1142, 1}, {1142, 2} // Arcane Meditation
    };
    m_worldBotDefaultTalentSpecs["mage_fire"] = talentSequence;
    talentSequence.clear();

    // mage frost
    talentSequence = {
        {37, 0}, {37, 1}, {37, 2}, {37, 3}, {37, 4}, // Improved Frostbolt
        {65, 0}, {65, 1}, {65, 2}, // Permafrost
        {62, 0}, {62, 1}, // Improved Frost Nova
        {69, 0}, // Cold Snap
        {38, 0}, {38, 1}, {38, 2}, // Frostbite
        {63, 0}, // Improved Blizzard
        {67, 0}, {67, 1}, {67, 2}, {67, 3}, {67, 4}, // Shatter
        {72, 0}, // Ice Block
        {63, 1}, {63, 2}, // Improved Blizzard
        {73, 0}, {73, 1}, // Ice Shards
        {68, 0}, {68, 1}, {68, 2}, {68, 3}, {68, 4}, // Winter's Chill
        {71, 0}, // Ice Barrier
        {73, 2}, {73, 3}, {73, 4}, // Ice Shards
        {741, 0}, {741, 1}, // Arctic Reach
        {61, 0}, {61, 1}, {61, 2}, // Piercing Ice
        {74, 0}, {74, 1}, // Arcane Subtlety
        {76, 0}, {76, 1}, {76, 2}, // Arcane Focus
        {75, 0}, {75, 1}, {75, 2}, {75, 3}, {75, 4}, // Arcane Concentration
        {1649, 0}, {1649, 1}, // Elemental Precision
    };
    m_worldBotDefaultTalentSpecs["mage_frost"] = talentSequence;
    talentSequence.clear();

    // warlock affliction
    talentSequence = {
        {1003, 0}, {1003, 1}, {1003, 2}, {1003, 3}, {1003, 4}, // Improved Corruption
        {1004, 0}, {1004, 1}, {1004, 2}, {1004, 3}, {1004, 4}, // Improved Drain Life
        {1061, 0}, // Amplify Curse
        {1001, 0}, {1001, 1}, {1001, 2}, {1001, 3}, // Fel Concentration
        {1002, 0}, {1002, 1}, // Nightfall
        {1001, 4}, // Fel Concentration
        {1021, 0}, {1021, 1}, // Grim Reach
        {1081, 0}, // Curse of Exhaustion
        {1041, 0}, // Siphon Life
        {1082, 0}, {1082, 1}, {1082, 2}, {1082, 3}, // Improved Curse of Exhaustion
        {1042, 0}, {1042, 1}, {1042, 2}, {1042, 3}, {1042, 4}, // Shadow Mastery
        {944, 0}, {944, 1}, {944, 2}, {944, 3}, {944, 4}, // Improved Shadow Bolt
        {943, 0}, {943, 1}, {943, 2}, {943, 3}, {943, 4}, // Bane
        {963, 0}, // Shadowburn
        {1005, 0}, {1005, 1}, {1005, 2}, {1005, 3}, {1005, 4}, // Suppression
        {1221, 0}, {1221, 1}, // Improved Healthstone
        {1007, 0}, {1007, 1}, // Improved Life Tap
    };
    m_worldBotDefaultTalentSpecs["warlock_affli"] = talentSequence;
    talentSequence.clear();

    // warlock demonology
    talentSequence = {
        {1222, 0}, {1222, 1}, {1222, 2}, // Improved Imp
        {1221, 0}, {1221, 1}, // Improved Healthstone
        {1242, 0}, {1242, 1}, {1242, 2}, {1242, 3}, {1242, 4}, // Fel Intellect
        {1226, 0}, // Fel Domination
        {1224, 0}, {1224, 1}, // Improved Health Funnel
        {1223, 0}, {1223, 1}, // Demonic Embrace
        {1227, 0}, {1227, 1}, // Master Summoner
        {1262, 0}, {1262, 1}, {1262, 2}, // Unholy Power
        {1281, 0}, // Demonic Sacrifice
        {1262, 3}, {1262, 4}, // Unholy Power
        {1223, 2}, {1223, 3}, // Demonic Embrace
        {1244, 0}, {1244, 1}, {1244, 2}, {1244, 3}, {1244, 4}, // Master Demonologist
        {1282, 0}, // Soul Link
        {944, 0}, {944, 1}, {944, 2}, {944, 3}, {944, 4}, // Improved Shadow Bolt
        {943, 0}, {943, 1}, {943, 2}, {943, 3}, {943, 4}, // Bane
        {963, 0}, // Shadowburn
        {1003, 0}, {1003, 1}, {1003, 2}, {1003, 3}, {1003, 4}, // Improved Corruption
        {1005, 0}, {1005, 1}, {1005, 2}, {1005, 3}, // Suppression
    };
    m_worldBotDefaultTalentSpecs["warlock_demon"] = talentSequence;
    talentSequence.clear();

    // warlock destruction
    talentSequence = {
        {944, 0}, {944, 1}, {944, 2}, {944, 3}, {944, 4}, // Improved Shadow Bolt
        {943, 0}, {943, 1}, {943, 2}, {943, 3}, {943, 4}, // Bane
        {963, 0}, // Shadowburn
        {1003, 0}, {1003, 1}, {1003, 2}, {1003, 3}, {1003, 4}, // Improved Corruption
        {1007, 0}, {1007, 1}, // Improved Life Tap
        {981, 0}, {981, 1}, {981, 2}, {981, 3}, // Devastation
        {964, 0}, {964, 1}, // Destructive Reach
        {985, 0}, {985, 1}, // Intensity
        {981, 4}, // Devastation
        {967, 0}, // Ruin
        {961, 0}, {961, 1}, {961, 2}, {961, 3}, {961, 4}, // Improved Immolate
        {966, 0}, {966, 1}, {966, 2}, {966, 3}, // Emberstorm
        {968, 0}, // Conflagrate
        {966, 4}, // Emberstorm
        {1005, 0}, {1005, 1}, {1005, 2}, // Suppression
        {1061, 0}, // Amplify Curse
        {1221, 0}, {1221, 1}, // Improved Healthstone
        {982, 0}, {982, 1}, {982, 2}, {982, 3}, {982, 4}, // Aftermath
        {986, 0}, // Pyroclasm
    };
    m_worldBotDefaultTalentSpecs["warlock_destr"] = talentSequence;
    talentSequence.clear();

    // priest discipline
    talentSequence = {
        {342, 0}, {342, 1}, {342, 2}, {342, 3}, {342, 4}, // Unbreakable Will
        {343, 0}, {343, 1}, {343, 2}, // Improved Power Word: Shield
        {344, 0}, {344, 1}, // Improved Power Word: Fortitude
        {348, 0}, // Inner Focus
        {347, 0}, {347, 1}, {347, 2}, // Meditation
        {321, 0}, // Martyrdom
        {346, 0}, {346, 1}, {346, 2}, // Improved Inner Fire
        {350, 0}, {350, 1}, // Improved Mana Burn
        {351, 0}, // Divine Spirit
        {1201, 0}, {1201, 1}, {1201, 2}, {1201, 3}, {1201, 4}, // Mental Strength
        {1202, 0}, {1202, 1}, {1202, 2}, {1202, 3}, // Force of Will
        {322, 0}, // Power Infusion
        {401, 0}, {401, 1}, {401, 2}, {401, 3}, {401, 4}, // Holy Specialization
        {1181, 0}, {1181, 1}, {1181, 2}, {1181, 3}, {1181, 4}, // Divine Fury
        {442, 0}, // Holy Nova
        {1636, 0}, {1636, 1}, {1636, 2}, // Blessed Recovery
        {406, 0}, // Improved Renew
        {403, 0}, {403, 1}, // Searing Light
        {1635, 0}, {1635, 1}, // Holy Reach
        {1202, 4}, // Force of Will
    };
    m_worldBotDefaultTalentSpecs["priest_disc"] = talentSequence;
    talentSequence.clear();

    // priest holy
    talentSequence = {
        {406, 0}, {406, 1}, {406, 2}, // Improved Renew
        {410, 0}, {410, 1}, // Healing Focus
        {401, 0}, {401, 1}, {401, 2}, {401, 3}, {401, 4}, // Holy Specialization
        {442, 0}, // Holy Nova
        {361, 0}, {361, 1}, {361, 2}, // Inspiration
        {1636, 0}, // Blessed Recovery
        {408, 0}, {408, 1}, {408, 2}, // Improved Healing
        {1636, 1}, {1636, 2}, // Blessed Recovery
        {1561, 0}, // Spirit of Redemption
        {413, 0}, {413, 1}, // Improved Prayer of Healing
        {402, 0}, {402, 1}, // Spiritual Guidance
        {404, 0}, {404, 1}, {404, 2}, {404, 3}, {404, 4}, // Spiritual Healing
        {1637, 0}, // Lightwell
        {402, 2}, {402, 3}, {402, 4}, // Spiritual Guidance
        {342, 0}, {342, 1}, {342, 2}, {342, 3}, {342, 4}, // Unbreakable Will
        {343, 0}, {343, 1}, {343, 2}, // Improved Power Word: Shield
        {344, 0}, {344, 1}, // Improved Power Word: Fortitude
        {348, 0}, // Inner Focus
        {347, 0}, {347, 1}, {347, 2}, // Meditation
        {321, 0}, // Martyrdom
        {1635, 0}, {1635, 1}, // Holy Reach
    };
    m_worldBotDefaultTalentSpecs["priest_holy"] = talentSequence;
    talentSequence.clear();

    // priest shadow
    talentSequence = {
        {464, 0}, {464, 1}, {464, 2}, {464, 3}, {464, 4}, // Blackout
        {482, 0}, {482, 1}, // Improved Shadow Word: Pain
        {463, 0}, {463, 1}, {463, 2}, // Shadow Focus
        {501, 0}, // Mind Flay
        {542, 0}, {542, 1}, // Improved Psychic Scream
        {463, 3}, {463, 4}, // Shadow Focus
        {881, 0}, {881, 1}, {881, 2}, // Shadow Reach
        {461, 0}, {461, 1}, // Shadow Weaving
        {541, 0}, // Silence
        {484, 0}, // Vampiric Embrace
        {1638, 0}, {1638, 1}, // Improved Vampiric Embrace
        {461, 2}, // Shadow Weaving
        {462, 0}, {462, 1}, {462, 2}, {462, 3}, {462, 4}, // Darkness
        {521, 0}, // Shadowform
        {342, 0}, {342, 1}, {342, 2}, {342, 3}, {342, 4}, // Unbreakable Will
        {343, 0}, {343, 1}, {343, 2}, // Improved Power Word: Shield
        {344, 0}, {344, 1}, // Improved Power Word: Fortitude
        {348, 0}, // Inner Focus
        {347, 0}, {347, 1}, {347, 2}, // Meditation
        {321, 0}, // Martyrdom
        {350, 0}, {350, 1}, // Improved Mana Burn
        {461, 3}, {461, 4}, // Shadow Weaving
        {321, 1}, // Martyrdom
    };
    m_worldBotDefaultTalentSpecs["priest_shadow"] = talentSequence;
    talentSequence.clear();


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
        std::vector<TravelNodeLink> neighbours = GetTravelNodeLinksById(team, current.node.id, includeFlightPath, includeAreaTrigger, includeTransport, maxCreatureLevel+3);
        
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
            locNames = {"Goldshire", "Stormwind", "Darnassus", "Dolanaar", "Ironforge", "Kharanos"};

        else if (level <= 20)
            locNames = {"Stormwind", "Sentinel", "Darnassus", "Auberdine", "Ironforge", "Thelsamar"};

        else if (level <= 25)
            locNames = {"Lakeshire", "Darkshire", "Menethil Harbor", "Southshore", "Astranaar"};

        else if (level <= 30)
            locNames = {"Darkshire", "Astranaar", "Menethil", "Stonetalon Peak"};

        else if (level <= 35)
            locNames = {"Refuge Pointe", "Rebel Camp", "Southshore", "Nijels"};

        else if (level <= 40)
            locNames = {"Refuge Pointe", "Rebel Camp", "Southshore", "Nijels", "Booty Bay", "Thelsamar", "Theramore", "Nether"};

        else if (level <= 45)
            locNames = {"Aerie", "Booty Bay", "Rebel Camp", "Thelsamar", "Theramore", "Gadgetzan"};

        else if (level <= 50)
            locNames = {"Aerie", "Thorium Point", "Gadgetzan", "Feathermoon", "Thalanaar", "Nether"};

        else if (level <= 55)
            locNames = {"Nether", "Talrendis", "Talonbranch", "Chillwind Camp", "Morgans Vigil"};

        else if (level <= 60)
            locNames = {"Chillwind Camp", "Lights", "Morgans Vigil", "Everlook", "Cenarion Hold"};
    }
    else if (team == HORDE){
        // Razorofen Downs, Razorfen Kraul, Astranaar, Southshore, Stonetalon Peak, Refuge Pointe, Rebel Camp, Nijels Point,
        // Theramor Isle, Feathermoon Stronghold, Thalanaar, Talonbranch Glade, Talrendis Point, Morgans Vigil, Chillwind Camp,
        // Lakeshire, Darkshire, Gnomeregan Portal, [PH] Teleport to Booty Bay
        excludedNodes = {1510, 1511, 118, 769, 1296, 101, 1338, 404, 538, 650, 666, 643, 180, 282, 1696, 1089, 509, 270, 1810};
        if (level <= 6)
            locNames = {"Valley of Trials", "Red Cloud Mesa", "Deathknell"};

        else if (level <= 12)
            locNames = {"Razor Hill", "Orgrimmar", "Bloodhoof Village", "Thunder Bluff", "Undercity", "Brill"};

        else if (level <= 20)
            locNames = {"Crossroads", "Sepulcher"};

        else if (level <= 25)
            locNames = {"Zoramgar", "Sun Rock", "Tarren", "Taurajo"};
        
        else if (level <= 30)
            locNames = {"Zoramgar", "Splintertree", "Freewind", "Tarren", "Sun Rock"};

        else if (level <= 35)
            locNames = {"Freewind", "Hammerfall", "Tarren", "Gromgol", "Ghost"};

        else if (level <= 40)
            locNames = {"Hammerfall", "Tarren", "Gromgol", "Booty Bay", "Stonard", "Taurajo", "Kargath", "Shadowprey"};

        else if (level <= 45)
            locNames = {"Booty Bay", "Gromgol", "Stonard", "Kargath", "Gadgetzan", "Revantusk"};

        else if (level <= 50)
            locNames = {"Mojache", "Gadgetzan", "Revantusk", "Thorium Point", "Stonard"};

        else if (level <= 55)
            locNames = {"Bulwark", "Flame Crest", "Bloodvenom", "Valormok", "Kargath"};

        else if (level <= 60)
            locNames = {"Bulwark", "Flame Crest", "Lights", "Everlook", "Cenarion Hold"};
    }

    for (const auto& name: locNames){
        for (const auto& node: m_travelNodes){
            if (node.name.find(name) != std::string::npos && !excludedNodes.count(node.id))
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
        // if (GetSession()->GetSecurity() > SEC_PLAYER || sWorld.getConfig(CONFIG_BOOL_PARTY_BOT_SKIP_CHECKS))
            ExtractUInt32(&args, botLevel);
    }

    if (!botClass)
    {
        SendSysMessage("Incorrect syntax. Expected role or class.");
        SetSentErrorMessage(true);
        return false;
    }

    uint32 exTeam = 0;
    Team team;
    ExtractUInt32(&args, exTeam);
    if (exTeam)
        // team = HORDE;
        team = ALLIANCE;
    else
        team = pPlayer->GetTeam();
    uint8 botRace = SelectRandomRaceForClass(botClass, team/*pPlayer->GetTeam()*/);
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
        std::vector<const SpellEntry*> spells = pAI->GetProfessionSpellsByFilter(std::string(arg));
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
    // Player* pPlayer = GetSession()->GetPlayer();
    // if (!pPlayer)
    //     return false;

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
        maxLevel = temp;
    }

    uint32 count = 0;
    ExtractUInt32(&args, count);

    uint32 allianceCount = 0;
    uint32 hordeCount = 0;
    for (uint32 i=0; i < count; ++i){
        Team botTeam = PickRandomValue(ALLIANCE, HORDE);
        // Team botTeam = HORDE;
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
        // nodeId = 1032;
        const TravelNode& node = sPlayerBotMgr.GetTravelNode(nodeId);
        CombatBotRoles botRole = ROLE_INVALID;
        uint8 botClass = CLASS_WARRIOR;
        if (botTeam == ALLIANCE)
            botClass = PickRandomValue(CLASS_WARRIOR, CLASS_PALADIN, CLASS_HUNTER, CLASS_ROGUE, CLASS_DRUID, CLASS_MAGE, CLASS_WARLOCK, CLASS_PRIEST);
            // botClass = CLASS_PRIEST;
        else if (botTeam == HORDE)
            botClass = PickRandomValue(CLASS_WARRIOR, CLASS_SHAMAN, CLASS_HUNTER, CLASS_ROGUE, CLASS_DRUID, CLASS_MAGE, CLASS_WARLOCK, CLASS_PRIEST);
            // botClass = CLASS_DRUID;
        
        // if (botClass == CLASS_WARRIOR)
        //     botRole = PickRandomValue(ROLE_MELEE_DPS, ROLE_TANK);
        // else if (botClass == CLASS_PALADIN)
        //     botRole = PickRandomValue(ROLE_MELEE_DPS, ROLE_HEALER);
        // else if (botClass == CLASS_SHAMAN)
        //     botRole = PickRandomValue(ROLE_MELEE_DPS, ROLE_RANGE_DPS /*, ROLE_HEALER */);
        // else if (botClass == CLASS_HUNTER || botClass == CLASS_MAGE || botClass == CLASS_WARLOCK)
        //     botRole = ROLE_RANGE_DPS;
        // else if (botClass == CLASS_ROGUE)
        //     botRole = ROLE_MELEE_DPS;
        // else if (botClass == CLASS_DRUID)
        //     botRole = PickRandomValue(ROLE_RANGE_DPS, /*ROLE_HEALER,*/ ROLE_MELEE_DPS);
        // else if (botClass == CLASS_PRIEST)
        //     botRole = PickRandomValue(ROLE_RANGE_DPS /*, ROLE_HEALER*/);
        
        uint8 botRace = SelectRandomRaceForClass(botClass, botTeam);
        WorldBotAI* pAI = new WorldBotAI(nullptr, nullptr, botRole, botRace, botClass, (uint8)botLevel, node.map_id, 0, node.x, node.y, node.z, frand(0.0f, 6.28f));
        pAI->m_safeSpawnNodes = vec;
        if (isGanker)
            pAI->m_safeTravelPoints = vec;
        else
            pAI->m_safeTravelPoints = sPlayerBotMgr.GetSafeTravelPoints(botTeam, botLevel);
        pAI->m_updateTimer.Reset(urand(250, 5000));
        if (!sPlayerBotMgr.AddBot(pAI))
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
    }
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, ">> Spawned %d alliances and %d hordes.", allianceCount, hordeCount);

    return true;
}

std::unordered_map<std::string, std::vector<std::pair<uint32, uint32>>> PlayerBotMgr::GetSpecsFilterBySubstring(std::string pattern){
    std::unordered_map<std::string, std::vector<std::pair<uint32, uint32>>> specMap;

    for (const auto entry: m_worldBotDefaultTalentSpecs){
        if (entry.first.find(pattern) != std::string::npos){
            specMap[entry.first] = entry.second;
        }
    }

    return specMap;
}