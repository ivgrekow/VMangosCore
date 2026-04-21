#include "WorldBotAI.h"

void WorldBotAI::UpdateAI(uint32 const diff){
    m_updateTimer.Update(diff);
    if (!m_updateTimer.Passed())
        return;
        
    m_updateTimer.Reset(WB_UPDATE_INTERVAL);
    
    if (!me->IsInWorld() || me->IsBeingTeleported())
        return;

    if (m_isInitialized == false){
        me->SetVisibility(VISIBILITY_OFF);
        me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SPAWNING);
        if (m_level)
            me->GiveLevel(m_level);
        InitTalentsByRandomSpec();
        sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "WorldBotAI::UpdateAI : Talents are initialized.");
        me->SetUInt32Value(PLAYER_XP, 0);
        GenerateGear();
        m_isInitialized = true;
        me->SetVisibility(VISIBILITY_ON);
    }
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

void WorldBotAI::OnPlayerLogin(){
    if (!m_isInitialized)
        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SPAWNING);
}

// generate and equip armor/weapon
void WorldBotAI::GenerateGear(){
    // learn armor proficiences first
    switch (me->GetClass())
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        {
            // plate armor proficiency
            if (me->GetLevel() >= 40 && !me->HasSpell(750))
                me->LearnSpell(750, false, false);
            break;
        }
        case CLASS_HUNTER:
        case CLASS_SHAMAN:
        {
            if (me->GetLevel() >= 40 && !me->HasSpell(8737))
                me->LearnSpell(8737, false, false);
            break;
        }
        default:
            break;
    }

    std::map<uint32 /*slot*/, std::vector<ItemPrototype const*>> itemsPerSlot;
    m_honorRank = urand(5, 18);
    
    for (auto const& itr: sObjectMgr.GetItemPrototypeMap()){
        ItemPrototype const* pProto = &itr.second;

        // Only items that have already been discovered by someone
        if (!pProto->Discovered)
            continue;

        // Skip unobtainable items
        if (pProto->HasExtraFlag(ITEM_EXTRA_NOT_OBTAINABLE))
            continue;

        // Only gear and weapons
        if (pProto->Class != ITEM_CLASS_WEAPON && pProto->Class != ITEM_CLASS_ARMOR)
            continue;

        // No tabards and shirts
        if (pProto->InventoryType == INVTYPE_TABARD || pProto->InventoryType == INVTYPE_BODY)
            continue;

        if (pProto->SourceQuestRaces && !(pProto->SourceQuestRaces & me->GetRaceMask()))
            continue;

        if (pProto->SourceQuestClasses && !(pProto->SourceQuestClasses & me->GetClassMask()))
            continue;

        if (me->GetLevel() - pProto->RequiredLevel > 10)
            continue;
        
        if (me->GetLevel() < 60 && (pProto->ItemLevel - me->GetLevel()) > 5)
            continue;
        else if (me->GetLevel() == 60 && pProto->ItemLevel < 60)
            continue;

        if (me->CanUseItem(pProto, false) != EQUIP_ERR_OK)
            continue;

        if (pProto->RequiredHonorRank > m_honorRank)
            continue;

        if (pProto->RequiredReputationFaction && uint32(me->GetReputationRank(pProto->RequiredReputationFaction)) < pProto->RequiredReputationRank)
            continue;

        if (uint32 skill = pProto->GetProficiencySkill()){
            if (me->GetClass() == CLASS_WARRIOR){
                if ((skill == SKILL_CLOTH && pProto->InventoryType != INVTYPE_CLOAK) || skill == SKILL_LEATHER)
                    continue;
                
                if (me->GetLevel() >= 50 && skill == SKILL_MAIL)
                    continue;
            }
        }

        // skip irrelevant items
        switch (me->GetClass()){
            case CLASS_WARRIOR:{
                if (pProto->InventoryType == INVTYPE_HOLDABLE)
                    continue;
            }
            
            default:
                break;
        }

        // uint8 slots[4];
        // pProto->GetAllowedEquipSlots(slots, me->GetClass(), me->CanDualWield());
        // sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "Found item id %d, name \"%s\", inventory %d.", pProto->ItemId, pProto->Name1, pProto->InventoryType);
        itemsPerSlot[pProto->InventoryType].push_back(pProto);
    }
    // for (int i=0; i < itemsPerSlot[INVTYPE_CHEST].size(); ++i)
    //     sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "\tFound chest armor id %d, name \"%s\".", itemsPerSlot[INVTYPE_CHEST][i]->ItemId, itemsPerSlot[INVTYPE_CHEST][i]->Name1);
    
    for (uint32 i=INVTYPE_HEAD; i < INVTYPE_TRINKET; i++) {
        std::sort(itemsPerSlot[i].begin(), itemsPerSlot[i].end(), [](const ItemPrototype* a, const ItemPrototype* b){
            return a->ItemLevel < b->ItemLevel;
        });
        sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "WorldBotAI::GenerateGear : itemsPerSlot[%d].size = %d", i, itemsPerSlot[i].size());
    }
    
    std::vector<uint32> invSlots {
        INVTYPE_HEAD,
        INVTYPE_NECK,
        INVTYPE_SHOULDERS,
        INVTYPE_CHEST,
        INVTYPE_WRISTS,
        INVTYPE_HANDS,
        INVTYPE_WAIST,
        INVTYPE_LEGS,
        INVTYPE_FEET,
        INVTYPE_FINGER,
        INVTYPE_CLOAK
    };
    bool isFirstRing = true;
    for (uint32 invSlot: invSlots) {

        bool isGearSelected = false;
        std::vector<uint32> enchIds;

        while (itemsPerSlot[invSlot].size() > 0 && !isGearSelected){
            float gearQuality = sPlayerBotMgr.GetWorldBotGearQuality();
            
            // gearQuality = frand(gearQuality, 1.0f);
            // uint32 indexOffset = static_cast<uint32>( std::floor(frand(gearQuality, 1.0f) * itemsPerSlot[invSlot].size()) );
            std::vector<uint32> whiteIndexes;
            std::vector<uint32> greenIndexes;
            std::vector<uint32> blueIndexes;
            std::vector<uint32> purpleIndexes;
            for (uint32 j = 0; j < itemsPerSlot[invSlot].size(); ++j){
                if (itemsPerSlot[invSlot][j]->Quality == ITEM_QUALITY_UNCOMMON)
                    greenIndexes.push_back(j);
                else if (itemsPerSlot[invSlot][j]->Quality == ITEM_QUALITY_RARE)
                    blueIndexes.push_back(j);
                else if (itemsPerSlot[invSlot][j]->Quality == ITEM_QUALITY_EPIC)
                purpleIndexes.push_back(j);
                else if (itemsPerSlot[invSlot][j]->Quality == ITEM_QUALITY_NORMAL)
                whiteIndexes.push_back(j);
            }
            // sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "WorldBotAI::GenerateGear : index vectors' sizes:");
            // sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "-- white: %d, green: %d, blue: %d, purple: %d.", whiteIndexes.size(), greenIndexes.size(), blueIndexes.size(), purpleIndexes.size());
            uint32 randomIndex = 0;
            float effectiveQuality = gearQuality + frand(-0.2f, 0.085f);
            if (effectiveQuality < 0.0f)
                effectiveQuality = 0.0f;
            else if (effectiveQuality > 1.0f)
                effectiveQuality = 1.0f;
            
            if (!purpleIndexes.empty() && effectiveQuality >= 0.8f){
                uint32 size = purpleIndexes.size();
                uint32 indexOffset = static_cast<uint32>(std::round(effectiveQuality * (size - 1)));
                randomIndex = purpleIndexes[urand(indexOffset, size - 1)];
            }
            else if (!blueIndexes.empty() && effectiveQuality >= 0.65f){
                uint32 size = blueIndexes.size();
                uint32 indexOffset = static_cast<uint32>(std::round(effectiveQuality * (size - 1)));
                randomIndex = blueIndexes[urand(indexOffset, size - 1)];
            }
            else if (!greenIndexes.empty() && effectiveQuality >= 0.3f){
                uint32 size = greenIndexes.size();
                uint32 indexOffset = static_cast<uint32>(std::round(effectiveQuality * (size - 1)));
                randomIndex = greenIndexes[urand(indexOffset, size - 1)];
            }
            else if (!whiteIndexes.empty())
                randomIndex = SelectRandomContainerElement(whiteIndexes);
            
            ItemPrototype const* pProto = itemsPerSlot[invSlot][randomIndex];
            uint32 randomEnchId = 0;

            if (pProto->RandomProperty){
                EnchStoreList enchList = GetEnchStoreListByItemRandomProperty(pProto->RandomProperty);
                if (enchList.empty()){
                    sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "WorldBotAI::GenerateGear : enchList is empty. Excluding item '%s' (#%d) from candidate list.", pProto->Name1, pProto->ItemId);
                    return;
                }

                // sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "WorldBotAI::EquipWarriorGear : found ench list for item proto #%d; list size: %d", pProto->ItemId, enchList.size());
                // for (EnchStoreItem ench: enchList){
                //     sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "\t enchId: %d", ench.ench);
                // }


                for (EnchStoreItem ench: enchList){
                    std::string suffx {sItemRandomPropertiesStore.LookupEntry(ench.ench)->internalName};
                    if (IsRandomEnchantRelevant(suffx)){
                        enchIds.push_back(ench.ench);
                    }
                }
                if (enchIds.empty()){
                    itemsPerSlot[invSlot].erase(itemsPerSlot[invSlot].begin() + randomIndex);
                    sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "WorldBotAI::GenerateGear : 'enchIds' is empty.");
                    continue;
                }

                randomEnchId = enchIds[urand(0, enchIds.size()-1)];
            }
            
            if (!IsItemStatsRelevant(pProto) && !randomEnchId){
                isGearSelected = false;
                itemsPerSlot[invSlot].erase(itemsPerSlot[invSlot].begin() + randomIndex);
                continue;
            }

            Item* item = Item::CreateItem(pProto->ItemId, 1, me->GetGUID());
            if (!item){
                sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "WorldBotAI::GenerateGear : error creating item %d id", pProto->ItemId);
                isGearSelected = false;
                itemsPerSlot[invSlot].erase(itemsPerSlot[invSlot].begin() + randomIndex);
                continue;
            }

            if (randomEnchId)
                item->SetItemRandomProperties(randomEnchId);
            me->SatisfyItemRequirements(pProto);

            uint16 dest;
            uint8 msg;

            msg = me->CanEquipItem(NULL_SLOT, dest, item, false);
            if (msg != EQUIP_ERR_OK){
                sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "WorldBotAI::GenerateGear : me->CanEquipItem returned error %d.", msg);
                isGearSelected = false;
                itemsPerSlot[invSlot].erase(itemsPerSlot[invSlot].begin() + randomIndex);
                continue;
            }
            
            itemsPerSlot[invSlot].erase(itemsPerSlot[invSlot].begin() + randomIndex);
            me->EquipItem(dest, item, true);
            if (isFirstRing && invSlot == INVTYPE_FINGER){
                isFirstRing = false;
                isGearSelected = false;
            }
            else
                isGearSelected = true;
            
            sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "WorldBotAI::EquipWarriorGear : slot %d, selected item `%s` (#%d).", invSlot, pProto->Name1, pProto->ItemId);
            sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "\t effectiveQuality = %f", effectiveQuality);
        }
    }
}

bool WorldBotAI::IsRandomEnchantRelevant(std::string suffx){
    bool result = false;

    switch (me->GetClass()){
        case CLASS_WARRIOR:
            if (suffx == "of the Bear" || suffx == "of the Tiger" || suffx == "of Strength" || suffx == "of Stamina")
            result = true;
            break;
        default:
            break;
    }
    return result;
}

bool WorldBotAI::IsItemStatsRelevant(const ItemPrototype* pProto){
    // which aura mods the item applies
    std::bitset<TOTAL_AURAS> auraMods;

    // which stats the item modifies
    std::bitset<MAX_ITEM_MOD> itemStats;

    bool result = false;
    
    for (_ItemStat stat: pProto->ItemStat){
        if (stat.ItemStatType < MAX_ITEM_MOD)
            itemStats.set(stat.ItemStatType);
    }

    for (_ItemSpell _itemSpell: pProto->Spells){
        const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(_itemSpell.SpellId);
        if (!pSpellEntry)
            continue;
        
        for (uint32 auraIndex: pSpellEntry->EffectApplyAuraName){
            if (auraIndex < TOTAL_AURAS)
                auraMods.set(auraIndex);
        }
    }

    switch (me->GetClass()){
        case CLASS_WARRIOR:
            if (
                pProto->Quality == ITEM_QUALITY_NORMAL ||
                itemStats[ITEM_MOD_STRENGTH] || 
                (itemStats[ITEM_MOD_STAMINA] && itemStats[ITEM_MOD_AGILITY]) ||
                auraMods[SPELL_AURA_MOD_ATTACK_POWER] ||
                auraMods[SPELL_AURA_MOD_ATTACKER_MELEE_CRIT_CHANCE] ||
                auraMods[SPELL_AURA_MOD_ATTACKER_MELEE_HIT_CHANCE]
            )
                result = true;

        default:
            break;
    }

    return result;
}

void WorldBotAI::InitTalentsByRandomSpec(){
    std::unordered_map<std::string, std::vector<std::pair<uint32, uint32>>> specMap;
    std::vector<std::string> specNames;
    
    if (me->GetClass() == CLASS_WARRIOR){
        specMap = sPlayerBotMgr.GetSpecsFilterByRegex("warrior_*");
        if (!specMap.size()){
            sLog.Out(LOG_DBERROR, LOG_LVL_BASIC, "WorldBotAI::InitTalentSpecs : specMap.size() is equal to 0. The bot's talents were not initialized.");
            return;
        }

        for (const auto entry: specMap){
            specNames.push_back(entry.first);
        }
        
        m_currentSpec = SelectRandomContainerElement(specNames);
        int index = 0;
        while (me->GetFreeTalentPoints()){
            uint32 id = specMap[m_currentSpec][index].first;
            uint32 rank = specMap[m_currentSpec][index].second;
            me->LearnTalent(id, rank);
            index++;
        }
        if (me->HasSpell(23922))
            sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "WorldBotAI::IniTalentsByRandomSpec : I've learnt spell `%s` with id %d", sSpellMgr.GetSpellEntry(23922)->SpellName, 23922);
    }
        
    // std::vector<uint32> talentTabsForClass;
    // for (uint32 talentTabId = 0; talentTabId < sTalentTabStore.GetNumRows(); ++talentTabId)
    // {
    //     TalentTabEntry const* talentTabEntry = sTalentTabStore.LookupEntry(talentTabId);
    //     if (!talentTabEntry)
    //     continue;
        
    //     if ((me->GetClassMask() & talentTabEntry->ClassMask) == 0)
    //     continue;
        
    //     talentTabsForClass.push_back(talentTabId);
    // }
    
    // if (talentTabsForClass.empty()){
    //     sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "WorldBotAI::InitTalentSpecs : no talent tabs were found; talents were not been initialized.");
    //     return;
    // }
    
    // warrior tank
    // для талантов не нужно хранить таблицы; изучение таланта происходит посредством вызова me->LearnTalent(id, rank),
    // в который передаётся id таланта и его ранг
    
}