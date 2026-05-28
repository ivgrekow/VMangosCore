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
        PopulateSpellData();
        // sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "WorldBotAI::UpdateAI : Talents are initialized.");
        me->SetUInt32Value(PLAYER_XP, 0);
        GenerateGear();
        m_isInitialized = true;
        me->SetVisibility(VISIBILITY_ON);
    }

    // if (!m_movementTimer.Passed())
    //     m_movementTimer.Update(diff);
    // else{
    //     // me->GetMotionMaster()->MovePoint(0, m_leader->GetPositionX(), m_leader->GetPositionX(),
    //     //     m_leader->GetPositionZ(), MOVE_EXCLUDE_STEEP_SLOPES);
    //     m_movementTimer.Reset(WB_UPDATE_MOVEMENT_INTERVAL);
    //     UpdateMovements();
    // }
    UpdateMovements();

    UpdateOutOfCombatAI();
}

void WorldBotAI::OnPacketReceived(WorldPacket const* packet){
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
        case SMSG_GROUP_INVITE:
        {
            std::unique_ptr<WorldPacket> data = std::make_unique<WorldPacket>(CMSG_GROUP_ACCEPT, 8);
            *data << me->GetObjectGuid();
            me->GetSession()->QueuePacket(std::move(data));

            const uint8* leaderName = packet->contents();
            Player* leader = sObjectMgr.GetPlayer((char*)leaderName);
            if (leader){
                m_leader = leader;
            }

            return;
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

    for (auto invSlot: invSlots) {
        std::sort(itemsPerSlot[invSlot].begin(), itemsPerSlot[invSlot].end(), [](const ItemPrototype* a, const ItemPrototype* b){
            return a->ItemLevel < b->ItemLevel;
        });
        // sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "WorldBotAI::GenerateGear : itemsPerSlot[%d].size = %d", invSlot, itemsPerSlot[invSlot].size());
        GenerateInventorySlotItem(itemsPerSlot, invSlot, true);
    }

    // generate weapon
    invSlots.clear();

    if (me->GetClass() == CLASS_WARRIOR){
        invSlots = {
            INVTYPE_SHIELD,
            INVTYPE_2HWEAPON,
            INVTYPE_RANGEDRIGHT,
            INVTYPE_WEAPON,
            INVTYPE_WEAPONMAINHAND
        };
        for (auto invSlot: invSlots) {
            std::sort(itemsPerSlot[invSlot].begin(), itemsPerSlot[invSlot].end(), [](const ItemPrototype* a, const ItemPrototype* b){
                return a->ItemLevel < b->ItemLevel;
            });
            // sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "WorldBotAI::GenerateGear : itemsPerSlot[%d].size = %d", invSlot, itemsPerSlot[invSlot].size());
        }

        uint32 coinChance = urand(0, 1);
        if (coinChance)
            invSlots = {INVTYPE_RANGED, INVTYPE_RANGEDRIGHT};
        else
            invSlots = {INVTYPE_RANGEDRIGHT, INVTYPE_RANGED};

        for (auto invSlot: invSlots)
            if (GenerateInventorySlotItem(itemsPerSlot, invSlot, true))
                break;

        bool needToEquipShield = false;
        bool needToEquipOneHand = false;
        bool needToEquipTwoHand = false;

        if (m_role == ROLE_TANK){
            needToEquipShield = true;
            needToEquipOneHand = true;
        }
        else if (m_role == ROLE_MELEE_DPS){
            if (std::regex_search(m_currentSpec, std::regex("fury"))){
                if (me->HasSpell(674))
                    needToEquipOneHand = true;
                else 
                    needToEquipTwoHand = true;
            }
            else if (std::regex_search(m_currentSpec, std::regex("arms"))){
                needToEquipTwoHand = true;
            }
        }
        
        GenerateInventorySlotItem(itemsPerSlot, INVTYPE_SHIELD, needToEquipShield);
        
        invSlots.clear();
        coinChance = urand(0, 1);
        if (coinChance)
            invSlots = {INVTYPE_WEAPONMAINHAND, INVTYPE_WEAPON};
        else
            invSlots = {INVTYPE_WEAPON, INVTYPE_WEAPON};

        for (auto invSlot: invSlots){
            GenerateInventorySlotItem(itemsPerSlot, invSlot, needToEquipOneHand);
            if (needToEquipShield)
                break;
        }
        
        GenerateInventorySlotItem(itemsPerSlot, INVTYPE_2HWEAPON, needToEquipTwoHand);
    }

    std::vector<uint32> ridingPets;
    if (me->GetRace() == RACE_HUMAN){
        if (me->GetLevel() == 60)
            ridingPets = {18777, 18778, 18776}; // Swift Brown Steed, Swift White Steed, Swift Palomino
        else if (me->GetLevel() >= 40)
            ridingPets = {2411, 5656, 5655, 2414}; // Black Stallion Bridle, Brown Horse Bridle, Chestnut Mare Bridle, Pinto Bridle
    }
    else if (me->GetRace() == RACE_DWARF){
        if (me->GetLevel() == 60)
            ridingPets = {18786, 18787, 18785};
        else if (me->GetLevel() >= 40)
            ridingPets = {5872, 5864, 5873};
    }
    else if (me->GetRace() == RACE_GNOME){
        if (me->GetLevel() == 60)
            ridingPets = {18772, 18773, 18774};
        else if (me->GetLevel() >= 40)
            ridingPets = {8595, 13321, 8563, 13322};
    }
    else if (me->GetRace() == RACE_NIGHTELF){
        if (me->GetLevel() == 60)
            ridingPets = {18766, 18767, 18902};
        else if (me->GetLevel() >= 40)
            ridingPets = {8632, 8631, 8629};
    }
    else if (me->GetRace() == RACE_ORC){
        if (me->GetLevel() == 60)
            ridingPets = {18796, 18798, 18797};
        else if (me->GetLevel() >= 40)
            ridingPets = {5668, 5665, 1132};
    }
    else if (me->GetRace() == RACE_TAUREN){
        if (me->GetLevel() == 60)
            ridingPets = {18794, 18795, 18793};
        else if (me->GetLevel() >= 40)
            ridingPets = {15290, 15277};
    }
    else if (me->GetRace() == RACE_UNDEAD){
        if (me->GetLevel() == 60)
            ridingPets = {13334, 18791};
        else if (me->GetLevel() >= 40)
            ridingPets = {13332, 13333, 13331};
    }
    else if (me->GetRace() == RACE_TROLL){
        if (me->GetLevel() == 60)
            ridingPets = {18788, 18789, 18790};
        else if (me->GetLevel() >= 40)
            ridingPets = {8588, 8591, 8592};
    }

    if (!ridingPets.empty()){
        uint32 itemId = SelectRandomContainerElement(ridingPets);
        Item* pItem = Item::CreateItem(itemId, 1, me->GetObjectGuid());
        ItemPosCountVec dest;
        if (me->CanStoreItem(INVENTORY_SLOT_BAG_0, NULL_SLOT, dest, pItem) == EQUIP_ERR_OK)
            me->StoreItem(dest, pItem, true);
    }
}

void WorldBotAI::GenerateInventorySlotPermEnchant(Item* pItem){
    if (!pItem)
        return;
    
    float qualityScale = 1.0f;
    uint32 invSlotType = pItem->GetProto()->InventoryType;
    std::string enchantName {"Enchant "};
    if (invSlotType == INVTYPE_CHEST)
        enchantName.append("Chest");
    else if (invSlotType == INVTYPE_WRISTS)
        enchantName.append("Bracer");
    else if (invSlotType == INVTYPE_HANDS)
        enchantName.append("Gloves");
    else if (invSlotType == INVTYPE_FEET)
        enchantName.append("Boots");
    else if (invSlotType == INVTYPE_SHIELD)
        enchantName.append("Shield");
    else if (invSlotType == INVTYPE_CLOAK)
        enchantName.append("Cloak");
    else if (invSlotType == INVTYPE_WEAPON || invSlotType == INVTYPE_WEAPONMAINHAND){
        enchantName.append("Weapon");
        qualityScale = 0.1f;
    }
    else if (invSlotType == INVTYPE_2HWEAPON){
        enchantName.append("2H Weapon");
        qualityScale = 0.1f;
    }
    else
        return;
        
    
    float gearQuality = (sPlayerBotMgr.GetWorldBotGearQuality() + frand(-0.2, 0.1)) * qualityScale;
    if (gearQuality < 0.0f)
        gearQuality = 0.0f;

    
    // pItem->SetEnchantment(PERM_ENCHANTMENT_SLOT, 71, 0, 0, me->GetObjectGuid());
    // const SpellItemEnchantmentEntry* entry = sSpellItemEnchantmentStore.LookupEntry(71);
    // sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "name: %s", entry->description[0]);
    std::vector<std::string> suffxVector;
    if (me->GetClass() == CLASS_WARRIOR)
        suffxVector = {"Stats", "Health", "Absorption", "Agility", "Strength", "Stamina", "Striking", "Impact", 
            "Icy Chill", "Fiery Weapon", "Demonslaying", "Unholy Weapon", "Crusader", "Lifestealing",
            "Defense", "Resistance", "Protection", "Dodge", "Block"};

    // std::vector<const SpellEntry*> possibleEnchants;
    // int32 maxSkillLevel = me->GetLevel() * 5;
    std::vector<uint32> enchantIds;

    SkillLineAbilityMapBounds skillMapBounds = sSpellMgr.GetSkillLineAbilityMapBoundsBySkillId(SKILL_ENCHANTING);
    for (auto it = skillMapBounds.first; it != skillMapBounds.second; ++it){
        const SkillLineAbilityEntry* entry = it->second;

        const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(entry->spellId);
        if (!pSpellEntry)
            continue;

        std::regex pattern {enchantName};
        if (!std::regex_search(pSpellEntry->SpellName[0], pattern))
            continue;

        if (suffxVector.empty())
            continue;

        bool isRelevant = false;
        for (auto suffx: suffxVector){
            pattern.assign(suffx);
            if (std::regex_search(pSpellEntry->SpellName[0], pattern)){
                isRelevant = true;
                break;
            }
        }
        if (!isRelevant)
            continue;

        enchantIds.push_back(pSpellEntry->EffectMiscValue[0]);
    }
    
    // not the best way to scale quality of enchants...
    gearQuality *= qualityScale;
    uint32 minIndex = static_cast<uint32>( std::round(gearQuality * (enchantIds.size() - 1)) );
    uint32 maxIndex = static_cast<uint32>(std::round((me->GetLevel() + 10 * gearQuality) / 60.0f  * (enchantIds.size()-1)));
    if (minIndex > maxIndex)
        minIndex = maxIndex;
    uint32 randomEnchantId = enchantIds[urand(minIndex, maxIndex)];
    pItem->SetEnchantment(PERM_ENCHANTMENT_SLOT, randomEnchantId, 0, 0, me->GetObjectGuid());
}

bool WorldBotAI::IsRandomEnchantRelevant(std::string suffx) const {
    bool result = false;

    switch (me->GetClass()){
        case CLASS_WARRIOR:
            if (suffx == "of the Bear" || suffx == "of the Tiger" || suffx == "of Strength" || suffx == "of Stamina" ||
                suffx == "of Defense" || suffx == "of Critical Strike" || suffx == "of Toughness")
                result = true;
            break;
        default:
            break;
    }

    return result;
}

bool WorldBotAI::AreItemStatsRelevant(const ItemPrototype* pProto) const {
    // which aura mods the item applies
    std::bitset<TOTAL_AURAS> auraMods;

    // which stats the item modifies
    std::bitset<MAX_ITEM_MOD> itemStats;

    std::bitset<MAX_SPELL_SCHOOL> spellSchools;

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

        spellSchools.set(pSpellEntry->School);
    }
    

    if (pProto->Quality == ITEM_QUALITY_NORMAL || pProto->InventoryType == INVTYPE_RANGED ||
                pProto->InventoryType == INVTYPE_RANGEDRIGHT)
        return true;

    switch (me->GetClass()){
        case CLASS_WARRIOR:
            if (
                
                itemStats[ITEM_MOD_STRENGTH] || 
                (itemStats[ITEM_MOD_STAMINA] && itemStats[ITEM_MOD_AGILITY]) ||
                auraMods[SPELL_AURA_MOD_ATTACK_POWER] ||
                auraMods[SPELL_AURA_MOD_ATTACKER_MELEE_CRIT_CHANCE] ||
                auraMods[SPELL_AURA_MOD_ATTACKER_MELEE_HIT_CHANCE]
            )
                result = true;
                break;
        case CLASS_WARLOCK:
            if ( (itemStats[ITEM_MOD_INTELLECT] ||
                itemStats[ITEM_MOD_STAMINA]) && !auraMods[SPELL_AURA_MOD_HEALING_DONE] ||
                (auraMods[SPELL_AURA_MOD_DAMAGE_DONE] && spellSchools[SPELL_SCHOOL_SHADOW])
            )
                result = true;
                break;
        case CLASS_MAGE:
            if ( auraMods[SPELL_AURA_MOD_DAMAGE_DONE] ||
                (itemStats[ITEM_MOD_STAMINA] && itemStats[ITEM_MOD_INTELLECT])
            )
                result = true;
                break;
        case CLASS_PRIEST:
            if ( 
                (itemStats[ITEM_MOD_STAMINA] && itemStats[ITEM_MOD_INTELLECT]) ||
                (itemStats[ITEM_MOD_SPIRIT] && itemStats[ITEM_MOD_INTELLECT]) ||
                auraMods[SPELL_AURA_MOD_HEALING]
            )
                result = true;
                break;
        default:
            break;
    }

    return result;
}

bool WorldBotAI::GenerateInventorySlotItem(std::map<uint32 /*slot*/, std::vector<ItemPrototype const*>>& itemsPerSlot, uint32 invSlot, bool isNeedToEquip){
    bool isGearSelected = false;
    std::vector<uint32> enchIds;
    bool isFirstRing = true;

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
        // sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "\t Evaluating `%s`; randomIndex = %d; effectiveQuality = %f", pProto->Name1, randomIndex, effectiveQuality);
        uint32 randomEnchId = 0;

        if (pProto->RandomProperty){
            EnchStoreList enchList = GetEnchStoreListByItemRandomProperty(pProto->RandomProperty);
            if (enchList.empty()){
                sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "WorldBotAI::GenerateGear : enchList is empty. Excluding item '%s' (#%d) from candidate list.", pProto->Name1, pProto->ItemId);
                return isGearSelected;
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
                // sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "WorldBotAI::GenerateGear : 'enchIds' is empty.");
                continue;
            }

            randomEnchId = enchIds[urand(0, enchIds.size()-1)];
        }
        
        if (!randomEnchId && !AreItemStatsRelevant(pProto)){
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

        GenerateInventorySlotPermEnchant(item);

        if (isNeedToEquip){
            uint16 dest;
            uint8 msg;

            msg = me->CanEquipItem(NULL_SLOT, dest, item, false);
            if (msg != EQUIP_ERR_OK){
                sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "WorldBotAI::GenerateGear : me->CanEquipItem returned error %d.", msg);
                isGearSelected = false;
                itemsPerSlot[invSlot].erase(itemsPerSlot[invSlot].begin() + randomIndex);
                continue;
            }

            me->EquipItem(dest, item, true);
        }
        else {
            ItemPosCountVec dest;
            uint8 msg = me->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item);
            if (msg == EQUIP_ERR_OK)
                me->StoreItem(dest, item, true);
        }
        
        itemsPerSlot[invSlot].erase(itemsPerSlot[invSlot].begin() + randomIndex);
        

        if (isFirstRing && invSlot == INVTYPE_FINGER){
            isFirstRing = false;
            isGearSelected = false;
        }
        else
            isGearSelected = true;
        
        // sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "WorldBotAI::EquipWarriorGear : slot %d, selected item `%s` (#%d).", invSlot, pProto->Name1, pProto->ItemId);
        // sLog.Out(LOG_BASIC, LOG_LVL_BASIC, "\t effectiveQuality = %f", effectiveQuality);
    }

    return isGearSelected;
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

        std::vector<std::string> regexVector {"tank", "protection"};
        for (auto str: regexVector){
            std::regex pattern {str};
            if (std::regex_search(m_currentSpec, pattern)){
                m_role = ROLE_TANK;
                return;
            }
        }
        regexVector.clear();

        regexVector = {"arms", "fury"};
        for (auto str: regexVector){
            std::regex pattern {str};
            if (std::regex_search(m_currentSpec, pattern)){
                m_role = ROLE_MELEE_DPS;
                return;
            }
        }
    }
}

void WorldBotAI::PopulateProffessionSpells() {
    std::vector<uint32> skillVector {SKILL_MINING, SKILL_SKINNING, SKILL_HERBALISM};
    uint32 gatherSkillId = SelectRandomContainerElement(skillVector);
    SkillLineAbilityMapBounds skillMapBounds = sSpellMgr.GetSkillLineAbilityMapBoundsBySkillId(gatherSkillId);
    // uint32 spellsCount = 0;
    std::vector<uint32> spellVector;
    for (auto it = skillMapBounds.first; it != skillMapBounds.second; ++it){
        spellVector.push_back(it->second->spellId);
        // spellsCount++;
    }
    if (spellVector.size()){
        uint32 skillLevel = me->GetLevel() * 5 + urand(5, 25);
        float normalizedLevel = skillLevel / 300.0f;
        if (normalizedLevel > 1.0f)
            normalizedLevel = 1.0f;
    
        uint32 maxIndex = static_cast<uint32>(std::round(normalizedLevel * (spellVector.size() - 1)));
        for (uint32 i=0; i != maxIndex; ++i){
            me->LearnSpell(spellVector[i], false);
            m_proffessionBook.push_back(sSpellMgr.GetSpellEntry(spellVector[i]));
        }
    }

    spellVector.clear();
    skillVector.clear();

    uint32 skillId = SKILL_NONE;
    switch (gatherSkillId){
        case SKILL_MINING:
        {
            skillVector = {SKILL_ENGINEERING, SKILL_BLACKSMITHING, SKILL_ENCHANTING};
            break;
        }
        case SKILL_HERBALISM:
        {
            // increased probability for alchemy
            skillVector = {SKILL_ALCHEMY, SKILL_ALCHEMY, SKILL_ENCHANTING};
            break;
        }
        case SKILL_SKINNING:
        {
            // increased probability for leatherworking
            skillVector = {SKILL_LEATHERWORKING, SKILL_LEATHERWORKING, SKILL_ENCHANTING};
            break;
        }
        default:
            // nonsense
            return;
    }
    
    skillId = SelectRandomContainerElement(skillVector);
    skillMapBounds = sSpellMgr.GetSkillLineAbilityMapBoundsBySkillId(skillId);
    
    for (auto it = skillMapBounds.first; it != skillMapBounds.second; ++it){
        spellVector.push_back(it->second->spellId);
    }

    if (spellVector.size()){
        uint32 skillLevel = me->GetLevel() * 5 + urand(5, 25);
        float normalizedLevel = skillLevel / 300.0f;
        if (normalizedLevel > 1.0f)
            normalizedLevel = 1.0f;
    
        uint32 maxIndex = static_cast<uint32>(std::round(normalizedLevel * (spellVector.size() - 1)));
        for (uint32 i=0; i != maxIndex; ++i){
            me->LearnSpell(spellVector[i], false);
            m_proffessionBook.push_back(sSpellMgr.GetSpellEntry(spellVector[i]));
        }
    }
}

void WorldBotAI::AddItemToInventory(uint32 itemId, uint32 count)
{
    ItemPosCountVec dest;
    uint8 msg = me->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, count);
    if (msg == EQUIP_ERR_OK)
    {
        if (Item* pItem = me->StoreNewItem(dest, itemId, true, Item::GenerateItemRandomPropertyId(itemId)))
            pItem->SetCount(count);
    }
}

bool WorldBotAI::UseItemEffect(Item* pItem)
{
    ItemPrototype const* pProto = pItem->GetProto();
    for (auto const& itr : pProto->Spells)
    {
        if (itr.SpellId && itr.SpellTrigger == ITEM_SPELLTRIGGER_ON_USE)
        {
            if (SpellEntry const* pSpellEntry = sSpellMgr.GetSpellEntry(itr.SpellId))
            {
                if (me->IsSpellReady(*pSpellEntry, pProto))
                {
                    if (pSpellEntry->IsPositiveSpell())
                        return me->CastSpell(me, pSpellEntry, false, pItem) == SPELL_CAST_OK;
                    else if (me->GetVictim())
                        return me->CastSpell(me->GetVictim(), pSpellEntry, false, pItem) == SPELL_CAST_OK;
                }
            }
        }
    }
    return false;
}

bool WorldBotAI::CanTryToCastSpell(Unit const* pTarget, SpellEntry const* pSpellEntry) const{

    if (!me->IsSpellReady(pSpellEntry->Id))
        return false;

    if (me->HasGCD(pSpellEntry))
        return false;

    if (pSpellEntry->TargetAuraState &&
       !pTarget->HasAuraState(AuraState(pSpellEntry->TargetAuraState)))
        return false;

    if (pSpellEntry->CasterAuraState &&
        !me->HasAuraState(AuraState(pSpellEntry->CasterAuraState)))
        return false;

    uint32 const powerCost = Spell::CalculatePowerCost(pSpellEntry, me);
    Powers const powerType = Powers(pSpellEntry->powerType);

    if (powerType == POWER_HEALTH)
    {
        if (me->GetHealth() <= powerCost)
            return false;
        return true;
    }

    if (me->GetPower(powerType) < powerCost)
        return false;

    if (pTarget->IsImmuneToSpell(pSpellEntry, false))
        return false;

    if (pSpellEntry->GetErrorAtShapeshiftedCast(me->GetShapeshiftForm()) != SPELL_CAST_OK)
        return false;

    if (pSpellEntry->IsSpellAppliesAura() && pTarget->HasAura(pSpellEntry->Id))
        return false;

    SpellRangeEntry const* srange = sSpellRangeStore.LookupEntry(pSpellEntry->rangeIndex);
    if (me != pTarget && pSpellEntry->EffectImplicitTargetA[0] != TARGET_UNIT_CASTER)
    {
        float const dist = me->GetCombatDistance(pTarget);

        if (dist > srange->maxRange)
            return false;
        if (srange->minRange && dist < srange->minRange)
            return false;
    }

    return true;
}

SpellCastResult WorldBotAI::DoCastSpell(Unit* pTarget, SpellEntry const* pSpellEntry){

    if (me != pTarget)
        me->SetFacingToObject(pTarget);

    if (me->IsMounted())
        me->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);

    me->SetTargetGuid(pTarget->GetObjectGuid());
    auto result = me->CastSpell(pTarget, pSpellEntry, false);

    //printf("cast %s result %u\n", pSpellEntry->SpellName[0].c_str(), result);

    if ((result == SPELL_FAILED_MOVING ||
        result == SPELL_CAST_OK) &&
        (pSpellEntry->GetCastTime(me) > 0) &&
        (me->IsMoving() || !me->IsStopped()))
        me->StopMoving();

    if ((result == SPELL_FAILED_NEED_AMMO_POUCH ||
        result == SPELL_FAILED_ITEM_NOT_READY) &&
        pSpellEntry->Reagent[0])
    {
        if (Item* pItem = me->GetItemByPos(INVENTORY_SLOT_BAG_0, INVENTORY_SLOT_ITEM_START))
            me->DestroyItem(INVENTORY_SLOT_BAG_0, INVENTORY_SLOT_ITEM_START, true);

        AddItemToInventory(pSpellEntry->Reagent[0]);
    }

    return result;
}

bool WorldBotAI::IsInDuel() const
{
    return me->m_duel && me->m_duel->startTime != 0;
}

bool WorldBotAI::IsValidHostileTarget(Unit const* pTarget) const{
    return me->IsValidAttackTarget(pTarget) &&
           pTarget->IsVisibleForOrDetect(me, me, false) &&
           !pTarget->HasBreakableByDamageCrowdControlAura() &&
           !pTarget->IsTotalImmune() &&
           pTarget->GetTransport() == me->GetTransport();
}

Unit* WorldBotAI::SelectPartyAttackTarget() const {
    Group* pGroup = me->GetGroup();
    std::map<uint32, Unit*> attackersMap;
    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        if (Player* pMember = itr->getSource())
        {
            // We already checked self.
            if (pMember == me)
                continue;

            for (const auto pAttacker : pMember->GetAttackers())
            {
                if (IsValidHostileTarget(pAttacker) &&
                    me->IsWithinDist(pAttacker, 50.0f))
                    return pAttacker;
            }
        }
    }

    return nullptr;
}

void WorldBotAI::UseMount(){
    uint32 petSpell = 0;
    if (m_class == CLASS_PALADIN){
        if (m_level >= 60)
            petSpell = 23214;
        else if (m_level >= 40)
            petSpell = 13819;
    }
    else if (m_class == CLASS_WARLOCK){
        if (m_level >= 60)
            petSpell = 23161;
        else if (m_level >= 40)
            petSpell = 5784;
    }

    if (petSpell){
        if (me->CastSpell(me, petSpell, false) == SPELL_CAST_OK)
            return;
    }
    else if (m_ridingPet){
        UseItemEffect(m_ridingPet);
    }
}

bool WorldBotAI::AttackStart(Unit* pVictim)
{
    if (me->IsMounted())
        me->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);

    if (me->Attack(pVictim, true))
    {
        if (m_role == ROLE_RANGE_DPS &&
            me->GetPowerPercent(POWER_MANA) > 10.0f &&
            me->GetCombatDistance(pVictim) > 8.0f)
            me->SetCasterChaseDistance(25.0f);
        else if (me->HasDistanceCasterMovement())
            me->SetCasterChaseDistance(0.0f);

        me->GetMotionMaster()->MoveChase(pVictim, 1.0f, m_role == ROLE_MELEE_DPS ? 3.0f : 0.0f);
        return true;
    }

    return false;
}

Unit* WorldBotAI::SelectAttackTarget(Player* pLeader) const
{
    if (IsInDuel())
    {
        if (me->m_duel->opponent && IsValidHostileTarget(me->m_duel->opponent))
            return me->m_duel->opponent;
    }
    else
    {
        // Stick to marked target in combat.
        if (me->IsInCombat() || pLeader->GetVictim())
        {
            for (auto markId : m_marksToFocus)
            {
                ObjectGuid targetGuid = me->GetGroup()->GetTargetWithIcon(markId);
                if (targetGuid.IsUnit())
                    if (Unit* pVictim = me->GetMap()->GetUnit(targetGuid))
                        if (IsValidHostileTarget(pVictim))
                            return pVictim;
            }
        }

        // Who is the leader attacking.
        if (Unit* pVictim = pLeader->GetVictim())
        {
            if (IsValidHostileTarget(pVictim))
                return pVictim;
        }
    }

    // Who is attacking me.
    for (const auto pAttacker : me->GetAttackers())
    {
        if (IsValidHostileTarget(pAttacker))
            return pAttacker;
    }

    if (!IsInDuel())
    {
        // Check if other group members are under attack.
        if (Unit* pPartyAttacker = SelectPartyAttackTarget())
            return pPartyAttacker;
    }

    // Assist pet if its in combat.
    if (Pet* pPet = me->GetPet())
    {
        if (Unit* pPetAttacker = pPet->GetAttackerForHelper())
            if (IsValidHostileTarget(pPetAttacker))
                return pPetAttacker;
    }

    return nullptr;
}

void WorldBotAI::UpdateMovements(){
    if (m_leader)
        FollowLeader();
}

void WorldBotAI::FollowLeader(){
    if (!m_leader){
        m_leader = nullptr;
        return;
    }
    
    Group* group = me->GetGroup();
    if (!group){
        m_leader = nullptr;
        return;
    }

    if (!m_leader->IsInWorld())
        return;

    float x, y, z;
    uint32 excludeSteepSlopes = 0;

    if (me->GetDistance(m_leader) <= 7.0f){
        if (me->GetMotionMaster()->GetCurrentMovementGeneratorType())
            me->StopMoving();
        return;
    }

    float angle = linlin(frand(0.0f, M_PI/3), 0.0f, M_PI/3,
        m_leader->GetOrientation()-7*M_PI/6,
        m_leader->GetOrientation()-5*M_PI/6);
    m_leader->GetNearPoint(m_leader, x, y, z, 0, 5.0f, angle);

    if (m_leader->GetPositionZ() - me->GetPositionZ() > 3.0f)
        excludeSteepSlopes = MOVE_EXCLUDE_STEEP_SLOPES;


    me->GetMotionMaster()->MovePoint(0, x, y, z, MOVE_PATHFINDING | excludeSteepSlopes);
}

void WorldBotAI::TravelToTarget(Unit* pTarget){

}

void WorldBotAI::TravelToPoint(uint32 mapId, Position pos){
    
}

Player* WorldBotAI::GetPartyLeader() const
{
    Group* pGroup = me->GetGroup();
    if (!pGroup)
        return nullptr;

    if (Player* originalLeader = ObjectAccessor::FindPlayerNotInWorld(m_leader->GetObjectGuid()))
    {
        if (me->InBattleGround() == originalLeader->InBattleGround())
        {
            // In case the original spawner is not in the same group as the bots anymore.
            if (pGroup != originalLeader->GetGroup())
                return nullptr;

            // In case the current leader is the bot itself and it's not inside a Battleground.
            ObjectGuid currentLeaderGuid = pGroup->GetLeaderGuid();
            if (currentLeaderGuid == me->GetObjectGuid() && !me->InBattleGround())
                return nullptr;
        }

        return originalLeader;
    }
    return nullptr;
}

Unit* WorldBotAI::GetMarkedTarget(RaidTargetIcon mark) const{
    ObjectGuid targetGuid = me->GetGroup()->GetTargetWithIcon(mark);
    if (targetGuid.IsUnit())
        return me->GetMap()->GetUnit(targetGuid);

    return nullptr;
}

bool WorldBotAI::AreOthersOnSameTarget(ObjectGuid guid, bool checkMelee, bool checkSpells) const
{
    Group* pGroup = me->GetGroup();
    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        if (Player* pMember = itr->getSource())
        {
            // Not self.
            if (pMember == me)
                continue;

            // Not the target itself.
            if (pMember->GetObjectGuid() == guid)
                continue;

            if (pMember->GetTargetGuid() == guid)
            {
                if (checkMelee && pMember->HasUnitState(UNIT_STATE_MELEE_ATTACKING))
                    return true;

                if (checkSpells && pMember->IsNonMeleeSpellCasted())
                    return true;
            }
        }
    }
    return false;
}

bool WorldBotAI::CanUseCrowdControl(SpellEntry const* pSpellEntry, Unit* pTarget) const
{
    if (IsInDuel())
        return true;

    if (pSpellEntry->HasAuraInterruptFlag(AURA_INTERRUPT_DAMAGE_CANCELS) &&
        AreOthersOnSameTarget(pTarget->GetObjectGuid()))
        return false;

    if (pSpellEntry->HasSingleTargetAura())
    {
        auto const& singleAuras = me->GetSingleCastSpellTargets();
        if (singleAuras.find(pSpellEntry) != singleAuras.end())
            return false;
    }

    return true;
}

void WorldBotAI::UpdateOutOfCombatAI() {
    if (m_level >= 40 && me->IsOutdoorOnTransport() && !me->IsMounted())
        UseMount();

    if (m_class == CLASS_WARRIOR)
        UpdateOutOfCombatWarrior();
}

void WorldBotAI::UpdateOutOfCombatWarrior(){

    if (!m_spellBook["Battle Stance"].empty() && CanTryToCastSpell(me, m_spellBook["Battle Stance"].back())){
        if (DoCastSpell(me, m_spellBook["Battle Stance"].back()) == SPELL_CAST_OK)
            return;
    }

    if (!m_spellBook["Battle Shout"].empty() &&
       !me->HasAura(m_spellBook["Battle Shout"].back()->Id))
    {
        const SpellEntry* pSpellEntry = m_spellBook["Battle Shout"].back();
        if (CanTryToCastSpell(me, pSpellEntry))
            DoCastSpell(me, pSpellEntry);
        else if (!m_spellBook["Bloodrage"].empty() &&
            (me->GetPower(POWER_RAGE) < 10) &&
            CanTryToCastSpell(me, m_spellBook["Bloodrage"].back()))
        {
            DoCastSpell(me, m_spellBook["Bloodrage"].back());
        }
    }

    if (Unit* pVictim = me->GetVictim())
    {
        if (!m_spellBook["Charge"].empty() &&
            CanTryToCastSpell(pVictim, m_spellBook["Charge"].back()))
        {
            if (DoCastSpell(pVictim, m_spellBook["Charge"].back()) == SPELL_CAST_OK)
                return;
        }
    }
}