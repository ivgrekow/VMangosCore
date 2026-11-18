#include "WorldBotAI.h"

void WorldBotAI::PopulateSpellData() {
    // CombatBotBaseAI::PopulateSpellData();
    memset(&m_spells, 0, sizeof(m_spells));

    uint32 m_class = me->GetClass();
    uint32 m_race = me->GetRace();
    uint32 my_level = me->GetLevel();

    std::vector<uint32> spellCandidates;

    uint32 spellFoodGUID = 1131;
    if (my_level < 5)
        spellFoodGUID = 433;
    else if (my_level < 15)
        spellFoodGUID = 434;
    else if (my_level < 25)
        spellFoodGUID = 435;
    else if (my_level < 35)
        spellFoodGUID = 1127;
    else if (my_level < 45)
        spellFoodGUID = 1129;
    else if (my_level < 55)
        spellFoodGUID = 1131;
    else if (my_level <= 60)
        spellFoodGUID = 25695;

    uint32 spellDrinkGUID = 1137;
    if (my_level < 5)
        spellDrinkGUID = 430;
    else if (my_level < 15)
        spellDrinkGUID = 431;
    else if (my_level < 25)
        spellDrinkGUID = 432;
    else if (my_level < 35)
        spellDrinkGUID = 1133;
    else if (my_level < 45)
        spellDrinkGUID = 1135;
    else if (my_level < 55)
        spellDrinkGUID = 1137;
    else if (my_level <= 60)
        spellDrinkGUID = 25696;

    switch(m_race)
    {
        case RACE_HUMAN:
        {
            // Perception
            m_racials.pPerception = sSpellMgr.GetSpellEntry(20600);
        }
        case RACE_DWARF:
        {
            // Stoneform
            m_racials.pStoneForm = sSpellMgr.GetSpellEntry(20594);
        }
        case RACE_NIGHTELF:
        {
            // Shadowmeld
            m_racials.pShadowmeld = sSpellMgr.GetSpellEntry(20580);
        }
        case RACE_GNOME:
        {
            // Escape Artist
            m_racials.pEscapeArtist = sSpellMgr.GetSpellEntry(20589);
        }
        case RACE_ORC:
        {
            // Blood Fury
            m_racials.pBloodFury = sSpellMgr.GetSpellEntry(20572);
        }
        case RACE_UNDEAD:
        {
            // Will of the Forsaken
            m_racials.pWilloftheForsaken = sSpellMgr.GetSpellEntry(7744);
            m_racials.pCannibalize = sSpellMgr.GetSpellEntry(20577);
        }
        case RACE_TAUREN:
        {
            // War Stomp
            m_racials.pWarStomp = sSpellMgr.GetSpellEntry(20549);
        }
        case RACE_TROLL:
        {
            // Berserking
            m_racials.pBerserking = sSpellMgr.GetSpellEntry(26297);
        }
    }

    switch (m_class){
        case CLASS_WARRIOR:
        {
            // Weapon skills (all weapons except for wands)
            if (my_level >= 10)
            {
                if (!me->HasSpell(264)) // Bows
                    me->LearnSpell(264, false);

                if (!me->HasSpell(266)) // Guns
                    me->LearnSpell(266, false);

                if (!me->HasSpell(5011)) // Crossbows
                    me->LearnSpell(5011, false);

                if (!me->HasSpell(1180)) // Daggers
                    me->LearnSpell(1180, false);

                if (!me->HasSpell(15590)) // Fist
                    me->LearnSpell(15590, false);

                if (!me->HasSpell(198)) // One-handed maces
                    me->LearnSpell(198, false);

                if (!me->HasSpell(199)) // Two-handed Maces
                    me->LearnSpell(199, false);

                if (!me->HasSpell(196)) // One-handed axes
                    me->LearnSpell(196, false);

                if (!me->HasSpell(197)) // Two-handed Axes
                    me->LearnSpell(197, false);

                if (!me->HasSpell(201)) // One-handed swords
                    me->LearnSpell(201, false);

                if (!me->HasSpell(202)) // Two-handed swords
                    me->LearnSpell(202, false);

                if (my_level >= 20 && !me->HasSpell(200)) // Polearms
                    me->LearnSpell(200, false);

                if (!me->HasSpell(227)) // Staves
                    me->LearnSpell(227, false);

                // Shoot Bow
                m_spells.warrior.pShootBow = sSpellMgr.GetSpellEntry(2480);

                // Shoot Gun
                m_spells.warrior.pShootGun = sSpellMgr.GetSpellEntry(7918);

                // Shoot Crossbow
                m_spells.warrior.pShootCrossbow = sSpellMgr.GetSpellEntry(7919);

                // plate armor proficiency
                if (me->GetLevel() >= 40 && !me->HasSpell(750))
                    me->LearnSpell(750, false, false);
            }
            
            // Dual Wield
            if (my_level >= 20) // warriors can learn it at this level
                me->LearnSpell(674, false);

            // Parry
            if (my_level >= 6)
                me->LearnSpell(3127, false);

            // Heroic Strike
            spellCandidates = {78, 284, 285, 1608, 11564, 11565, 11566, 11567, 25286}; // Heroic Strike all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (my_level >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    m_spells.warrior.pHeroicStrike = pSpellEntry;
                }
            }

            // Rend
            spellCandidates = {772, 6546, 6547, 6548, 11572, 11573, 11574}; // Rend all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (my_level >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    m_spells.warrior.pRend = pSpellEntry;
                }
            }

            // Execute
            spellCandidates = {5308, 20658, 20660, 20661, 20662}; // Execute all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (my_level >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    m_spells.warrior.pExecute = pSpellEntry;
                }
            }

            // Charge
            spellCandidates = {100, 6178, 11578}; // Charge all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (my_level >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    m_spells.warrior.pCharge = pSpellEntry;
                }
            }

            // Thunder Clap
            spellCandidates = {6343, 8198, 8204, 8205, 11580, 11581}; // Thunder Clap all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (my_level >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    m_spells.warrior.pThunderClap = pSpellEntry;
                }
            }

            // Hamstring
            spellCandidates = {1715, 7372, 7373}; // Hamstring all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (my_level >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    m_spells.warrior.pHamstring = pSpellEntry;
                }
            }

            // Overpower
            spellCandidates = {7384, 7887, 11584, 11585}; // Overpower all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (my_level >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    m_spells.warrior.pOverpower = pSpellEntry;
                }
            }

            // Mocking Blow
            spellCandidates = {694, 7400, 7402, 20559, 20560}; // Mocking Blow all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (my_level >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    m_spells.warrior.pMockingBlow = pSpellEntry;
                }
            }

            // Retaliation
            {
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(20230);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(20230, false);
                    m_spells.warrior.pRetaliation = spell;
                }
            }

            // Sweeping Strikes
            if (me->HasSpell(12292))
            {
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(12292);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(12292, false);
                    m_spells.warrior.pSweepingStrikes = spell;
                }
            }

            if (me->HasSpell(12294))
            {
                // Mortal Strike
                spellCandidates = {12294, 21551, 21552, 21553}; // Mortal Strike all ranks
                for (auto spellId : spellCandidates)
                {
                    const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                    if (!pSpellEntry)
                        continue;

                    if (my_level >= pSpellEntry->spellLevel)
                    {
                        me->LearnSpell(spellId, false);
                        m_spells.warrior.pMortalStrike = pSpellEntry;
                    }
                }
            }

            // Battle Shout
            spellCandidates = {6673, 5242, 6192, 11549, 11550, 11551, 25289}; // Battle Shout all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (my_level >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    m_spells.warrior.pBattleShout = pSpellEntry;
                }
            }

            // Demoralizing Shout
            spellCandidates = {1160, 6190, 11554, 11555, 11556}; // Demoralizing Shout all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (my_level >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    m_spells.warrior.pDemoralizingShout = pSpellEntry;
                }
            }

            // Challenging Shout
            {
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(1161);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(1161, false);
                    m_spells.warrior.pChallengingShout = spell;
                }
            }

            // Intimidating Shout
            {
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(5246);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spell->Id, false);
                    m_spells.warrior.pIntimidatingShout = spell;
                }
            }

            // Slam
            spellCandidates = {1464, 8820, 11604, 11605}; // Slam all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (my_level >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    m_spells.warrior.pSlam = pSpellEntry;
                }
            }

            // Berserker Stance
            {
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(2458);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spell->Id, false);
                    m_spells.warrior.pBerserkerStance = spell;
                }
            }

            // Cleave
            spellCandidates = {845, 7369, 11608, 11609, 20569}; // Cleave all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (my_level >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    m_spells.warrior.pCleave = pSpellEntry;
                }
            }

            // Intercept
            spellCandidates = {20252, 20616, 20617}; // Intercept all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (my_level >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    m_spells.warrior.pIntercept = pSpellEntry;
                }
            }

            // Whirlwind
            {
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(1680);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spell->Id, false);
                    m_spells.warrior.pWhirlwind = spell;
                }
            }

            // Pummel
            spellCandidates = {6552, 6554}; // Pummel all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (my_level >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    m_spells.warrior.pPummel = pSpellEntry;
                }
            }

            // Piercing Howl
            if (me->HasSpell(12323))
            {
                m_spells.warrior.pPiercingHowl = sSpellMgr.GetSpellEntry(12323);
            }

            // Death Wish
            if (me->HasSpell(12328))
            {
                m_spells.warrior.pDeathWish = sSpellMgr.GetSpellEntry(12328);
            }

            // Bloodthirst
            if (me->HasSpell(23881))
            {
                spellCandidates = {23881, 23892, 23893, 23894}; // Bloodthirst all ranks
                for (auto spellId : spellCandidates)
                {
                    const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                    if (!pSpellEntry)
                        continue;

                    if (my_level >= pSpellEntry->spellLevel)
                    {
                        me->LearnSpell(spellId, false);
                        m_spells.warrior.pBloodthirst = pSpellEntry;
                    }
                }
            }

            // Recklessness
            {
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(1719);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spell->Id, false);
                    m_spells.warrior.pRecklessness = spell;
                }
            }

            // Battle Stance
            // {
            //     const SpellEntry* spell = sSpellMgr.GetSpellEntry();
            //     if (spell && my_level >= spell->spellLevel){
            //         me->LearnSpell(spell->Id, false);
            //         m_spells.warrior.pBattleStance = spell;
            //     }
            // }

            // Defensive Stance
            {
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(71);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spell->Id, false);
                    m_spells.warrior.pDefensiveStance = spell;
                }
            }

            // Sunder Armor
            spellCandidates = {7386, 7405, 8380, 11596, 11597}; // Sunder Armor all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (my_level >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    m_spells.warrior.pSunderArmor = pSpellEntry;
                }
            }

            // Bloodrage
            {
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(2687);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spell->Id, false);
                    m_spells.warrior.pBloodrage = spell;
                }
            }

            // Taunt
            {
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(355);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spell->Id, false);
                    m_spells.warrior.pTaunt = spell;
                }
            }

            // Shield Bash
            spellCandidates = {72, 1671, 1672}; // Shield Bash all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (my_level >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    m_spells.warrior.pShieldBash = pSpellEntry;
                }
            }

            // Revenge
            spellCandidates = {6572, 6574, 7379, 11600, 11601, 25288}; // Revenge all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (my_level >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    m_spells.warrior.pRevenge = pSpellEntry;
                }
            }

            // Shield Wall
            {
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(871);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spell->Id, false);
                    m_spells.warrior.pShieldWall = spell;
                }
            }

            // Shield Block
            {
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(2565);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spell->Id, false);
                    m_spells.warrior.pShieldBlock = spell;
                }
            }

            // Disarm
            {
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(676);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(676, false);
                    m_spells.warrior.pDisarm = spell;
                }
            }

            // Last Stand
            if (me->HasSpell(12975)){
                m_spells.warrior.pLastStand = sSpellMgr.GetSpellEntry(12975);
            }

            // Concussion Blow
            if (me->HasSpell(12809)){
                m_spells.warrior.pConcussionBlow = sSpellMgr.GetSpellEntry(12809);
            }

            // Shield Slam
            spellCandidates = {23922, 23923, 23924, 23925}; // Shield Slam all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (my_level >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    m_spells.warrior.pShieldSlam = pSpellEntry;
                }
            }

            break;
        }
        case CLASS_PALADIN:{
            if (my_level >= 10)
            {
                if (!me->HasSpell(198)) // One-handed maces
                    me->LearnSpell(198, false);

                if (!me->HasSpell(199)) // Two-handed Maces
                    me->LearnSpell(199, false);

                if (!me->HasSpell(196)) // One-handed axes
                    me->LearnSpell(196, false);

                if (!me->HasSpell(197)) // Two-handed Axes
                    me->LearnSpell(197, false);

                if (!me->HasSpell(201)) // One-handed swords
                    me->LearnSpell(201, false);

                if (!me->HasSpell(202)) // Two-handed swords
                    me->LearnSpell(202, false);

                if (my_level >= 20 && !me->HasSpell(200)) // Polearms
                    me->LearnSpell(200, false);

                // plate armor proficiency
                if (my_level >= 40 && !me->HasSpell(750))
                    me->LearnSpell(750, false);
            }

            // Holy Light
            spellCandidates = {639, 647, 1026, 1042, 3472, 10328, 10329, 25292};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.paladin.pHolyLight = spell;
                }
            }

            // Seal of Righteousness
            spellCandidates = {20287, 20288, 20289, 20290, 20291, 20292, 20293};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.paladin.pSealofRighteousness = spell;
                }
            }

            // Purify/Cleanse
            {
                uint32 spellId = 4987; // Cleanse
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spellId, false);
                    m_spells.paladin.pPurify = spell;
                }
                else {
                    spellId = 1152; // Purify
                    spell = sSpellMgr.GetSpellEntry(spellId);
                    if (spell && my_level >= spell->spellLevel){
                        me->LearnSpell(spellId, false);
                        m_spells.paladin.pPurify = spell;
                    }
                }
            }

            // Lay on Hands
            spellCandidates = {633, 2800, 10310};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.paladin.pLayOnHands = spell;
                }
            }

            // Redemption
            spellCandidates = {7328, 10322, 10324, 20772, 20773};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.paladin.pRedemption = spell;
                }
            }

            // Blessing of Wisdom
            spellCandidates = {19742, 19850, 19852, 19853, 19854, 25290};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.paladin.pBlessingofWisdom = spell;
                }
            }

            // Greater Blessing of Wisdom
            spellCandidates = {25894, 25918};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.paladin.pGreaterBlessingofWisdom = spell;
                }
            }

            // Exorcism
            spellCandidates = {879, 5614, 5615, 10312, 10313, 10314};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.paladin.pExorcism = spell;
                }
            }

            // Flash of Light
            spellCandidates = {19750, 19939, 19940, 19941, 19942, 19943};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.paladin.pFlashofLight = spell;
                }
            }

            // Consecration
            if (me->HasSpell(26573)){
                spellCandidates = {20116, 20922, 20923, 20924};
                for (auto id: spellCandidates){
                    const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                    if (!spell)
                        continue;

                    if (my_level >= spell->spellLevel){
                        me->LearnSpell(id, false);
                        m_spells.paladin.pConsecration = spell;
                    }
                }
            }

            // Concentration Aura
            {
                uint32 spellId = 19746;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spellId, false);
                    m_spells.paladin.pConcentrationAura = spell;
                }
            }

            // Seal of Light
            spellCandidates = {20165, 20347, 20348, 20349};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.paladin.pSealofLight = spell;
                }
            }

            // Seal of Wisdom
            spellCandidates = {20166, 20356, 20357};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.paladin.pSealofWisdom = spell;
                }
            }

            // Summon Warhorse/Charger
            {
                uint32 spellId = 23214; // Charger
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spellId, false);
                    m_spells.paladin.pSummonHorse = spell;
                }
                else {
                    spellId = 13819; // Warhorse
                    spell = sSpellMgr.GetSpellEntry(spellId);
                    if (spell && my_level >= spell->spellLevel){
                        me->LearnSpell(spellId, false);
                        m_spells.paladin.pSummonHorse = spell;
                    }
                }
            }

            // Holy Shock
            if (me->HasSpell(20473)){
                spellCandidates = {20929, 20930};
                for (auto id: spellCandidates){
                    const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                    if (!spell)
                        continue;

                    if (my_level >= spell->spellLevel){
                        me->LearnSpell(id, false);
                        m_spells.paladin.pHolyShock = spell;
                    }
                }
            }

            // Holy Wrath
            spellCandidates = {2812, 10318};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.paladin.pHolyWrath = spell;
                }
            }

            // Turn Undead
            spellCandidates = {2878, 5627, 10326};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.paladin.pTurnUndead = spell;
                }
            }

            // Devotion Aura
            spellCandidates = {465, 10290, 643, 10291, 1032, 10292, 10293};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.paladin.pDevotionAura = spell;
                }
            }

            // Divine Protection
            spellCandidates = {498, 5573};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.paladin.pDivineProtection = spell;
                }
            }

            // Hammer of Justice
            spellCandidates = {853, 5588, 5589, 10308};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.paladin.pHammerOfJustice = spell;
                }
            }

            // Blessing of Protection
            spellCandidates = {1022, 5599, 10278};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.paladin.pBlessingOfProtection= spell;
                }
            }

            // Righteous Fury
            {
                uint32 spellId = 25780;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spellId, false);
                    m_spells.paladin.pRighteousFury = spell;
                }
            }

            // Blessing of Freedom
            {
                uint32 spellId = 1044;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spellId, false);
                    m_spells.paladin.pBlessingOfFreedom = spell;
                }
            }

            // Greater Blessing of Kings
            if (me->HasSpell(20217)){
                uint32 spellId = 25898;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spellId, false);
                    m_spells.paladin.pGreaterBlessingofKings = spell;
                }
            }

            // Seal of Justice
            {
                uint32 spellId = 20164;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spellId, false);
                    m_spells.paladin.pSealofJustice = spell;
                }
            }

            // Blessing of Salvation
            {
                uint32 spellId = 1038;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spellId, false);
                    m_spells.paladin.pBlessingofSalvation = spell;
                }

                spellId = 25895;
                spell = sSpellMgr.GetSpellEntry(spellId);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spellId, false);
                    m_spells.paladin.pGreaterBlessingofSalvation = spell;
                }
            }

            // Shadow Resistance Aura
            spellCandidates = {19876, 19895, 19896};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.paladin.pShadowResistanceAura = spell;
                }
            }

            // Blessing of Sanctuary
            if (me->HasSpell(20911)){
                spellCandidates = {20912, 20913, 20914};
                for (auto id: spellCandidates){
                    const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                    if (!spell)
                        continue;

                    if (my_level >= spell->spellLevel){
                        me->LearnSpell(id, false);
                        m_spells.paladin.pBlessingofSanctuary = spell;
                    }
                }

                // Greater Blessing of Sanctuary
                uint32 spellId = 25899;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spellId, false);
                    m_spells.paladin.pGreaterBlessingofSanctuary = spell;
                }
            }

            // Divine Intervention
            {
                uint32 spellId = 19752;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spellId, false);
                    m_spells.paladin.pDivineIntervention = spell;
                }
            }

            // Frost Resistance Aura
            spellCandidates = {19888, 19897, 19898};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.paladin.pFrostResistanceAura = spell;
                }
            }

            // Fire Resistance Aura
            spellCandidates = {19891, 19899, 19900};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.paladin.pFireResistanceAura = spell;
                }
            }

            // Blessing of Might
            spellCandidates = {19740, 19834, 19835, 19836, 19837, 19838, 25291};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.paladin.pBlessingofMight = spell;
                }
            }

            // Greater Blessing of Might
            spellCandidates = {25782, 25916};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.paladin.pGreaterBlessingofMight = spell;
                }
            }

            // Judgement
            {
                uint32 spellId = 20271;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spellId, false);
                    m_spells.paladin.pJudgement = spell;
                }
            }

            // Retribution Aura
            spellCandidates = {7294, 10298, 10299, 10300, 10301};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.paladin.pRetributionAura = spell;
                }
            }

            // Repentance
            {
                uint32 spellId = 20066;
                if (me->HasSpell(spellId))
                    m_spells.paladin.pRepentance = sSpellMgr.GetSpellEntry(spellId);
            }

            // Hammer of Wrath
            spellCandidates = {24275, 24274, 24239};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.paladin.pHammerOfWrath = spell;
                }
            }



            break;
        }
        case CLASS_HUNTER:{
            if (my_level >= 10){
                if (!me->HasSpell(264)) // Bows
                    me->LearnSpell(264, false);

                if (!me->HasSpell(266)) // Guns
                    me->LearnSpell(266, false);

                if (!me->HasSpell(5011)) // Crossbows
                    me->LearnSpell(5011, false);

                if (!me->HasSpell(1180)) // Daggers
                    me->LearnSpell(1180, false);

                if (!me->HasSpell(196)) // One-handed axes
                    me->LearnSpell(196, false);

                if (!me->HasSpell(197)) // Two-handed Axes
                    me->LearnSpell(197, false);

                if (!me->HasSpell(201)) // One-handed swords
                    me->LearnSpell(201, false);

                if (!me->HasSpell(202)) // Two-handed swords
                    me->LearnSpell(202, false);

                if (my_level >= 20 && !me->HasSpell(200)) // Polearms
                    me->LearnSpell(200, false);

                if (!me->HasSpell(227)) // Staves
                    me->LearnSpell(227, false);

                // Dual Wield
                if (my_level >= 20) // hunters can learn it at this level
                    me->LearnSpell(674, false);

                if (my_level >= 40 && !me->HasSpell(8737))
                    me->LearnSpell(8737, false, false);
            }

            {
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(75);
                if (spell)
                    m_spells.hunter.pAutoShoot = spell;
            }

            // Growl
            spellCandidates = {2649, 14916, 14917, 14918, 14919, 14920, 14921};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    // me->LearnSpell(id, false);
                    m_spells.hunter.pPetGrowl = spell;
                }
            }

            // Bite (all pets)
            spellCandidates = {17253, 17255, 17256, 17257, 17258, 17259, 17260, 17261};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    // me->LearnSpell(id, false);
                    m_spells.hunter.pPetBite = spell;
                }
            }

            // Dash (cats, boars, raptors)
            spellCandidates = {23099, 23109, 23110};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    // me->LearnSpell(id, false);
                    m_spells.hunter.pPetDash = spell;
                }
            }

            // Cower (all pets)
            spellCandidates = {1742, 1753, 1754, 1755, 1756, 16697};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    // me->LearnSpell(id, false);
                    m_spells.hunter.pPetCower = spell;
                }
            }

            // Claw (cats)
            spellCandidates = {16827, 16828, 16829, 16830, 16831, 16832, 3010, 3009};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    // me->LearnSpell(id, false);
                    m_spells.hunter.pPetClaw = spell;
                }
            }

            // Prowl (cats)
            spellCandidates = {24450, 24454, 24455};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    // me->LearnSpell(id, false);
                    m_spells.hunter.pPetProwl = spell;
                }
            }

            // Charge (boars)
            spellCandidates = {7371, 26177, 26178, 26179, 26201, 27685};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    // me->LearnSpell(id, false);
                    m_spells.hunter.pPetCharge = spell;
                }
            }

            // Web (spiders)
            spellCandidates = {4167, 4168, 4169};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    // me->LearnSpell(id, false);
                    m_spells.hunter.pPetWeb = spell;
                }
            }

            // Aspect of the Monkey
            {
                uint32 id = 13163;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.hunter.pAspectOfTheMonkey = spell;
                }
            }

            // Aspect of the Hawk
            spellCandidates = {13165, 14318, 14319, 14320, 14321, 14322, 25296};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    // me->LearnSpell(id, false);
                    m_spells.hunter.pAspectOfTheHawk = spell;
                }
            }

            // Aspect of the Cheetah
            {
                uint32 id = 5118;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.hunter.pAspectOfTheCheetah = spell;
                }
            }

            // Aspect of the Pack
            {
                uint32 id = 13159;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.hunter.pAspectOfThePack = spell;
                }
            }

            // Mend pet
            spellCandidates = {136, 3111, 3661, 3662, 13542, 13543, 13544};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.hunter.pMendPet = spell;
                }
            }

            // Intimidation
            if (me->HasSpell(19577)){
                m_spells.hunter.pIntimidation = sSpellMgr.GetSpellEntry(19577);
            }

            // Bestial Wrath
            if (me->HasSpell(19574)){
                m_spells.hunter.pBestialWrath = sSpellMgr.GetSpellEntry(19574);
            }

            // Serpent Sting
            spellCandidates = {1978, 13549, 13550, 13551, 13552, 13553, 13554, 13555, 25295};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.hunter.pSerpentSting = spell;
                }
            }

            // Arcane Shot
            spellCandidates = {3044, 14281, 14282, 14283, 14284, 14285, 14286, 14287};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.hunter.pArcaneShot = spell;
                }
            }

            // Aimed Shot
            if (me->HasSpell(19434)){
                spellCandidates = {20900, 20901, 20902, 20903, 20904};
                for (auto id: spellCandidates){
                    const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                    if (!spell)
                        continue;
    
                    if (my_level >= spell->spellLevel){
                        me->LearnSpell(id, false);
                        m_spells.hunter.pAimedShot = spell;
                    }
                }
            }

            // Multi-shot
            spellCandidates = {2643, 14288, 14289, 14290, 25294};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.hunter.pMultiShot = spell;
                }
            }

            // Concussive Shot
            {
                uint32 id = 5116;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spell->Id, false);
                    m_spells.hunter.pConcussiveShot = spell;
                }
            }

            // Viper Sting
            spellCandidates = {3034, 14279, 14280};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.hunter.pViperSting = spell;
                }
            }

            // Track Hidden
            {
                uint32 id = 19885;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.hunter.pTrackHidden = spell;
                }
            }

            // Scatter Shot
            if (me->HasSpell(19503)){
                m_spells.hunter.pScatterShot = sSpellMgr.GetSpellEntry(19503);
            }

            // Trueshot Aura
            spellCandidates = {19506, 20905, 20906};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.hunter.pTrueshotAura = spell;
                }
            }

            // Wing Clip
            spellCandidates = {2974, 14267, 14268};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.hunter.pWingClip = spell;
                }
            }

            // Hunter's Mark
            spellCandidates = {1130, 14323, 14324, 14325};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.hunter.pHuntersMark = spell;
                }
            }
            
            // Mongoose Bite
            spellCandidates = {1495, 14269, 14270, 14271};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.hunter.pMongooseBite = spell;
                }
            }

            // Raptor Strike
            spellCandidates = {14260, 14261, 14262, 14263, 14264, 14265, 14266};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.hunter.pRaptorStrike = spell;
                }
            }

            // Disengage
            spellCandidates = {781, 14272, 14273};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.hunter.pDisengage = spell;
                }
            }

            // Feign Death
            {
                uint32 id = 5384;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.hunter.pFeignDeath = spell;
                }
            }

            // Scare Beast
            spellCandidates = {1513, 14326, 14327};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.hunter.pScareBeast = spell;
                }
            }

            // Volley
            spellCandidates = {1510, 14294, 14295};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.hunter.pVolley = spell;
                }
            }

            // Deterrence
            if (me->HasSpell(19263)){
                m_spells.hunter.pDeterrence = sSpellMgr.GetSpellEntry(19263);
            }

            // Counterattack
            if (me->HasSpell(19306)){
                spellCandidates = {19306, 20909, 20910};
                for (auto id: spellCandidates){
                    const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                    if (!spell)
                        continue;

                    if (my_level >= spell->spellLevel){
                        me->LearnSpell(id, false);
                        m_spells.hunter.pCounterattack = spell;
                    }
                }
            }

            // Wyvern Sting
            if (me->HasSpell(19386)){
                spellCandidates = {19386, 24132, 24133};
                for (auto id: spellCandidates){
                    const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                    if (!spell)
                        continue;

                    if (my_level >= spell->spellLevel){
                        me->LearnSpell(id, false);
                        m_spells.hunter.pWyvernSting = spell;
                    }
                }
            }

            // Immolation Trap
            spellCandidates = {13795, 14302, 14303, 14304, 14305};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.hunter.pImmolationTrap = spell;
                }
            }

            // Explosive Trap
            spellCandidates = {13813, 14316, 14317};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.hunter.pExplosiveTrap = spell;
                }
            }

            // Freezing Trap
            spellCandidates = {1499, 14310, 14311};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.hunter.pFreezingTrap = spell;
                }
            }

            // Frost Trap
            {
                uint32 id = 13809;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.hunter.pFrostTrap = spell;
                }
            }

            break;
        }
        case CLASS_SHAMAN:{
            if (my_level >= 10)
            {
                if (!me->HasSpell(1180)) // Daggers
                    me->LearnSpell(1180, false);

                if (!me->HasSpell(15590)) // Fist
                    me->LearnSpell(15590, false);

                if (!me->HasSpell(198)) // One-handed maces
                    me->LearnSpell(198, false);

                if (!me->HasSpell(196)) // One-handed axes
                    me->LearnSpell(196, false);

                if (!me->HasSpell(227)) // Staves
                    me->LearnSpell(227, false);

                if (me->GetLevel() >= 40 && !me->HasSpell(8737))
                    me->LearnSpell(8737, false, false);
            }

            break;
        }
        case CLASS_DRUID:{
            if (my_level >= 10)
            {
                if (!me->HasSpell(1180)) // Daggers
                    me->LearnSpell(1180, false);

                if (!me->HasSpell(15590)) // Fist
                    me->LearnSpell(15590, false);

                if (!me->HasSpell(198)) // One-handed maces
                    me->LearnSpell(198, false);

                if (!me->HasSpell(199)) // Two-handed Maces
                    me->LearnSpell(199, false);

                if (!me->HasSpell(227)) // Staves
                    me->LearnSpell(227, false);
            }

            // Wrath
            spellCandidates = {5176, 5177, 5178, 5179, 5180, 6780, 8905, 9912};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pWrath = spell;
                }
            }

            // Moonfire
            spellCandidates = {8921, 8924, 8925, 8926, 8927, 8928, 8929, 9833, 9834, 9835};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pMoonfire = spell;
                }
            }

            // Thorns
            spellCandidates = {467, 782, 1075, 8914, 9756, 9910};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pThorns = spell;
                }
            }

            // Entangling Roots
            // spells are moved to InitTalentsByRandomSpec() because these ones are required for
            // Nature's Grasp talent

            // Nature's Grasp
            if (me->HasSpell(16689)){
                spellCandidates = {16810, 16811, 16812, 16813, 17329};
                for (auto id: spellCandidates){
                    const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                    if (!spell)
                        continue;
    
                    if (my_level >= spell->spellLevel){
                        me->LearnSpell(id, false);
                        m_spells.druid.pNaturesGrasp = spell;
                    }
                }
            }

            // Barkskin
            {
                uint32 spellId = 22812;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId);
                if (spell){
                    if (my_level >= spell->spellLevel){
                        me->LearnSpell(spellId, false);
                        m_spells.druid.pBarkskin = spell;
                    }
                }
            }

            // Teleport: Moonglade
            {
                uint32 spellId = 18960;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId);
                if (spell){
                    if (my_level >= spell->spellLevel){
                        me->LearnSpell(spellId, false);
                        m_spells.druid.pTeleportMoonglade = spell;
                    }
                }
            }

            // Faerie Fire
            spellCandidates = {770, 778, 9749, 9907};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pFaerieFire = spell;
                }
            }

            // Hibernate
            spellCandidates = {2637, 18657, 18658};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pHibernate = spell;
                }
            }

            // Starfire
            spellCandidates = {2912, 8949, 8950, 8951, 9875, 9876, 25298};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pStarfire = spell;
                }
            }

            // Insect Swarm
            if (me->HasSpell(5570)){
                spellCandidates = {24974, 24975, 24976, 24977};
                for (auto id: spellCandidates){
                    const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                    if (!spell)
                        continue;

                    if (my_level >= spell->spellLevel){
                        me->LearnSpell(id, false);
                        m_spells.druid.pInsectSwarm = spell;
                    }
                }
            }

            // Soothe Animal
            spellCandidates = {2908, 8955, 9901};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pSootheAnimal = spell;
                }
            }

            // Innervate
            {
                uint32 spellId = 29166;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId);
                if (spell){
                    if (my_level >= spell->spellLevel){
                        me->LearnSpell(spellId, false);
                        m_spells.druid.pInnervate = spell;
                    }
                }
            }

            // Moonkin Form
            {
                uint32 spellId = 24858;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId);
                if (spell){
                    if (my_level >= spell->spellLevel){
                        me->LearnSpell(spellId, false);
                        m_spells.druid.pMoonkinForm = spell;
                    }
                }
            }

            // Hurricane
            spellCandidates = {16914, 17401, 17402};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pHurricane = spell;
                }
            }

            // Bear Form
            {
                uint32 spellId = 9634; // Dire Bear Form
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spellId, false);
                    m_spells.druid.pBearForm = spell;
                }
                else{
                    spellId = 5487;
                    spell = sSpellMgr.GetSpellEntry(spellId);
                    if (spell && my_level >= spell->spellLevel){
                        me->LearnSpell(spellId, false);
                        m_spells.druid.pBearForm = spell;
                    }
                }
            }

            // Demoralizing Roar
            spellCandidates = {99, 1735, 9490, 9747, 9898};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pDemoralizingRoar = spell;
                }
            }

            // Maul
            spellCandidates = {6807, 6808, 6809, 8972, 9745, 9880, 9881};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pMaul = spell;
                }
            }

            // Growl
            {
                uint32 spellId = 6795;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId);
                if (spell){
                    if (my_level >= spell->spellLevel){
                        me->LearnSpell(spellId, false);
                        m_spells.druid.pGrowl = spell;
                    }
                }
            }

            // Enrage
            {
                uint32 spellId = 5229;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId);
                if (spell){
                    if (my_level >= spell->spellLevel){
                        me->LearnSpell(spellId, false);
                        m_spells.druid.pEnrage = spell;
                    }
                }
            }

            // Bash
            spellCandidates = {5211, 6798, 8983};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pBash = spell;
                }
            }
            
            // Swipe
            spellCandidates = {779, 780, 769, 9754, 9908};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pSwipe = spell;
                }
            }

            // Aquatic Form
            {
                uint32 spellId = 1066;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId);
                if (spell){
                    if (my_level >= spell->spellLevel){
                        me->LearnSpell(spellId, false);
                        m_spells.druid.pAquaticForm = spell;
                    }
                }
            }

            // Cat Form
            {
                uint32 spellId = 768;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId);
                if (spell){
                    if (my_level >= spell->spellLevel){
                        me->LearnSpell(spellId, false);
                        m_spells.druid.pCatForm = spell;
                    }
                }
            }

            // Claw
            spellCandidates = {1082, 3029, 5201, 9849, 9850};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pClaw = spell;
                }
            }

            // Rip
            spellCandidates = {1079, 9492, 9493, 9752, 9894, 9896};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pRip = spell;
                }
            }

            // Prowl
            spellCandidates = {5215, 6783, 9913};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pProwl = spell;
                }
            }

            // Shred
            spellCandidates = {5221, 6800, 8992, 9829, 9830};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pShred = spell;
                }
            }

            // Tiger's Fury
            spellCandidates = {5217, 6793, 9845, 9846};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pTigersFury = spell;
                }
            }

            // Rake
            spellCandidates = {1822, 1823, 1824, 9904};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pRake = spell;
                }
            }

            // Faerie Fire (Feral)
            if (me->HasSpell(16857)){
                spellCandidates = {17390, 17391, 17392};
                for (auto id: spellCandidates){
                    const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                    if (!spell)
                        continue;
    
                    if (my_level >= spell->spellLevel){
                        me->LearnSpell(id, false);
                        m_spells.druid.pFaerieFireFeral = spell;
                    }
                }
            }

            // Dash
            spellCandidates = {1850, 9821};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pDash = spell;
                }
            }

            // Challenging Roar
            {
                uint32 spellId = 5209;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId);
                if (spell){
                    if (my_level >= spell->spellLevel){
                        me->LearnSpell(spellId, false);
                        m_spells.druid.pChallengingRoar = spell;
                    }
                }
            }

            // Cower
            spellCandidates = {8998, 9000, 9892};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pCower = spell;
                }
            }

            // Travel Form
            {
                uint32 spellId = 783;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId);
                if (spell){
                    if (my_level >= spell->spellLevel){
                        me->LearnSpell(spellId, false);
                        m_spells.druid.pTravelForm = spell;
                    }
                }
            }

            // Ravage
            spellCandidates = {6785, 6787, 9866, 9867};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pRavage = spell;
                }
            }

            // Ferocious Bite
            spellCandidates = {22568, 22827, 22828, 22829, 31018};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pFerociousBite = spell;
                }
            }

            // Pounce
            spellCandidates = {9005, 9823, 9827};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pPounce = spell;
                }
            }

            // Frenzied Regeneration
            spellCandidates = {22842, 22895, 22896};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pFrenziedRegeneration = spell;
                }
            }

            // Feral Charge
            if (me->HasSpell(16979))
                m_spells.druid.pFeralCharge = sSpellMgr.GetSpellEntry(16979);

            // Healing Touch
            spellCandidates = {5186, 5187, 5188, 5189, 6778, 8903, 9758, 9888, 9889, 25297};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pHealingTouch = spell;
                }
            }

            // Mark of the Wild
            spellCandidates = {1126, 5232, 6756, 5234, 8907, 9884, 9885};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pMarkoftheWild = spell;
                }
            }

            // Rejuvenation
            spellCandidates = {774, 1058, 1430, 2090, 2091, 3627, 8910, 9839, 9840, 9841, 25299};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pRejuvenation = spell;
                }
            }

            // Regrowth
            spellCandidates = {8936, 8938, 8939, 8940, 8941, 9750, 9856, 9857, 9858};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pRegrowth = spell;
                }
            }

            // Cure Poison
            {
                uint32 spellId = 2893; // Abolish Poison
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spellId, false);
                    m_spells.druid.pCurePoison = spell;
                }
                else {
                    spellId = 8946; // Cure Poison
                    spell = sSpellMgr.GetSpellEntry(spellId);
                    if (spell && my_level >= spell->spellLevel){
                        me->LearnSpell(spellId, false);
                        m_spells.druid.pCurePoison = spell;
                    }
                }
            }

            // Rebirth
            spellCandidates = {20484, 20739, 20742, 20747, 20748};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pRebirth = spell;
                }
            }

            // Omen of Clarity
            if (me->HasSpell(16864))
                m_spells.druid.pOmenofClarity = sSpellMgr.GetSpellEntry(16864);

            // Remove Curse
            {
                uint32 spellId = 2782;
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(spellId);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spellId, false);
                    m_spells.druid.pRemoveCurse = spell;
                }
            }

            // Tranquility
            spellCandidates = {740, 8918, 9862, 9863};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.druid.pTranquility = spell;
                }
            }

            // Nature's Swiftness
            if (me->HasSpell(17116))
                m_spells.druid.pNaturesSwiftness = sSpellMgr.GetSpellEntry(17116);

            // Swiftmend
            if (me->HasSpell(18562))
                m_spells.druid.pSwiftmend = sSpellMgr.GetSpellEntry(18562);

            break;
        }
        case CLASS_ROGUE:{
            if (my_level >= 10){
                if (!me->HasSpell(674))
                    me->LearnSpell(674, false); // Dual Wield

                if (!me->HasSpell(264)) // Bows
                    me->LearnSpell(264, false);
    
                if (!me->HasSpell(266)) // Guns
                    me->LearnSpell(266, false);
    
                if (!me->HasSpell(5011)) // Crossbows
                    me->LearnSpell(5011, false);

                if (!me->HasSpell(1180)) // Daggers
                    me->LearnSpell(1180, false);

                if (!me->HasSpell(15590)) // Fist
                    me->LearnSpell(15590, false);

                if (!me->HasSpell(198)) // One-handed maces
                    me->LearnSpell(198, false);

                if (!me->HasSpell(201)) // One-handed swords
                    me->LearnSpell(201, false);

                // Shoot Bow
                m_spells.warrior.pShootBow = sSpellMgr.GetSpellEntry(2480);

                // Shoot Gun
                m_spells.warrior.pShootGun = sSpellMgr.GetSpellEntry(7918);

                // Shoot Crossbow
                m_spells.warrior.pShootCrossbow = sSpellMgr.GetSpellEntry(7919);
            }

            break;
        }
        case CLASS_MAGE:{
            if (my_level >= 10){
                // Wands are learnt by default

                if (!me->HasSpell(1180)) // Daggers
                    me->LearnSpell(1180, false);

                if (!me->HasSpell(201)) // One-handed swords
                    me->LearnSpell(201, false);

                if (!me->HasSpell(227)) // Staves
                    me->LearnSpell(227, false);
            }

            break;
        }
        case CLASS_WARLOCK:{
            if (my_level >= 10){
                // Wands are learnt by default

                if (!me->HasSpell(1180)) // Daggers
                    me->LearnSpell(1180, false);

                if (!me->HasSpell(201)) // One-handed swords
                    me->LearnSpell(201, false);

                if (!me->HasSpell(227)) // Staves
                    me->LearnSpell(227, false);
            }

            // Curse of Weakness
            spellCandidates = {702, 1108, 6205, 7646, 11707, 11708};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pCurseofWeakness = spell;
                }
            }

            // Corruption
            spellCandidates = {172, 6222, 6223, 7648, 11671, 11672, 25311};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(spell->Id, false);
                    m_spells.warlock.pCorruption = spell;
                }
            }

            // Life Tap
            spellCandidates = {1454, 1455, 1456, 11687, 11688, 1689};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pLifeTap = spell;
                }
            }

            // Curse of Agony
            spellCandidates = {980, 1014, 6217, 11711, 11712, 11713, };
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pCurseofAgony = spell;
                }
            }

            // Fear
            spellCandidates = {5782, 6213, 6215};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pFear = spell;
                }
            }

            // Drain Soul
            spellCandidates = {1120, 8288, 8289, 11675};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pDrainSoul = spell;
                }
            }

            // Drain Life
            spellCandidates = {689, 699, 709, 7651, 11699, 11700};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pDrainLife = spell;
                }
            }

            // Drain Mana
            spellCandidates = {5138, 6226, 11703, 11704};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pDrainMana = spell;
                }
            }

            // Curse of Tongues
            spellCandidates = {1714, 11719};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pCurseofTongues = spell;
                }
            }

            // Curse of the Elements
            spellCandidates = {1490, 11721, 11722};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pCurseoftheElements = spell;
                }
            }

            // Howl of Terror
            spellCandidates = {5484, 17928};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pHowlofTerror = spell;
                }
            }

            // Dark Pact
            if (me->HasSpell(18220)){
                spellCandidates = {18937, 18938};
                for (auto id: spellCandidates){
                    const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                    if (!spell)
                        continue;

                    if (my_level >= spell->spellLevel){
                        me->LearnSpell(id, false);
                        m_spells.warlock.pDarkPact = spell;
                    }
                }
            }

            // Death Coil
            spellCandidates = {6789, 17925, 17926};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(spell->Id, false);
                    m_spells.warlock.pDeathCoil = spell;
                }
            }

            // Curse of Doom
            {
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(603);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spell->Id, false);
                    m_spells.warlock.pCurseofDoom = spell;
                }
            }

            // Siphon Life
            if (me->HasSpell(18265)){
                spellCandidates = {18879, 18880, 18881};
                for (auto id: spellCandidates){
                    const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                    if (!spell)
                        continue;
    
                    if (my_level >= spell->spellLevel){
                        me->LearnSpell(id, false);
                        m_spells.warlock.pSiphonLife = spell;
                    }
                }
            }

            // Amplify Curse
            if (me->HasSpell(18288)){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(18288);
                m_spells.warlock.pAmplifyCurse = spell;
                // if (my_level >= spell->spellLevel){
                //     m_spells.warlock.pAmplifyCurse = spell;
                // }
            }

            // Curse of Exhaustion
            if (me->HasSpell(18223)){
                m_spells.warlock.pCurseofExhaustion = sSpellMgr.GetSpellEntry(18223);
            }

            // Demon Skin
            spellCandidates = {687, 696};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pDemonSkin = spell;
                }
            }

            // Demon Armor
            spellCandidates = {706, 1086, 11733, 11734, 11735};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pDemonArmor = spell;
                }
            }

            // Summon Imp
            {
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(688);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spell->Id, false);
                    m_spells.warlock.pSummonImp = spell;
                }
            }

            // Summon Voidwalker
            {
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(697);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spell->Id, false);
                    m_spells.warlock.pSummonVoidwalker = spell;
                }
            }

            // Summon Succubus
            {
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(712);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spell->Id, false);
                    m_spells.warlock.pSummonSuccubus = spell;
                }
            }

            // Summon Felhunter
            {
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(691);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spell->Id, false);
                    m_spells.warlock.pSummonFelHunter = spell;
                }
            }

            // Summon Felsteed or Summon Dreadsteed
            {
                spellCandidates = {5784, 23161};
                for (auto id: spellCandidates){
                    const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                    if (!spell)
                        continue;

                    if (my_level >= spell->spellLevel){
                        me->LearnSpell(id, false);
                        m_spells.warlock.pSummonSteed = spell;
                    }
                }
            }

            // Create Healthstone
            spellCandidates = {6201, 6202, 5699, 11729, 11730};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pCreateHealthstone = spell;
                }
            }

            // Health Funnel
            spellCandidates = {755, 3698, 3699, 3700, 11693, 11694, 11695};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pHealthFunnel = spell;
                }
            }

            // Unending Breath
            {
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(5697);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(spell->Id, false);
                    m_spells.warlock.pUnendingBreath = spell;
                }
            }
            
            // Detect Invisibility
            spellCandidates = {132, 2970};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pDetectInvisibility = spell;
                }
            }

            // Banish
            spellCandidates = {710, 18647};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pBanish = spell;
                }
            }

            // Shadow Ward
            spellCandidates = {6229, 11739, 11740, 28610};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pShadowWard = spell;
                }
            }

            // Create Soulstone
            spellCandidates = {693, 20752, 20755, 20756, 20757};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pCreateSoulstone = spell;
                }
            }

            // Fel Domination
            if (me->HasSpell(18708)){
                m_spells.warlock.pFelDomination = sSpellMgr.GetSpellEntry(18708);
            }

            // Demonic Sacrifice
            if (me->HasSpell(18788)){
                m_spells.warlock.pDemonicSacrifice = sSpellMgr.GetSpellEntry(18788);
            }

            // Soul Link
            if (me->HasSpell(19028)){
                m_spells.warlock.pSoulLink = sSpellMgr.GetSpellEntry(19028);
            }

            // Shadow Bolt
            spellCandidates = {686, 695, 705, 1088, 1106, 7641, 11659, 11660, 11661, 25307};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pShadowBolt = spell;
                }
            }

            // Immolate
            spellCandidates = {348, 707, 1094, 2941, 11665, 11667, 11668, 25309};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pImmolate = spell;
                }
            }

            // Searing Pain
            spellCandidates = {5676, 17919, 17920, 17921, 17922, 17923};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pSearingPain = spell;
                }
            }

            // Rain of Fire
            spellCandidates = {5740, 6219, 11677, 11678};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pRainOfFire = spell;
                }
            }

            // Shadowburn
            if (me->HasSpell(17877)){
                spellCandidates = {17877, 18867, 18868, 18869, 18870, 18871};
                for (auto id: spellCandidates){
                    const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                    if (!spell)
                        continue;

                    if (my_level >= spell->spellLevel){
                        me->LearnSpell(id, false);
                        m_spells.warlock.pShadowburn = spell;
                    }
                }
            }

            // Hellfire
            spellCandidates = {1949, 11683, 11684};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pHellfire = spell;
                }
            }

            // Soul Fire
            spellCandidates = {6353, 17924};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pSoulFire = spell;
                }
            }

            // Firebolt (imp)
            spellCandidates = {3110, 7799, 7800, 7801, 7802, 11762, 11763};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pFirebolt = spell;
                }
            }

            // Fire Shield (imp)
            spellCandidates = {2947, 8316, 8317, 11770, 11771};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pFireShield = spell;
                }
            }

            // Blood Pact (imp)
            spellCandidates = {6307, 7804, 7805, 11766, 11767};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pBloodPact = spell;
                }
            }

            // Phase Shift (imp)
            {
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(4511);
                if (spell && my_level >= spell->spellLevel)
                    m_spells.warlock.pPhaseShift = spell;
            }

            // Torment (voidwalker)
            spellCandidates = {3716, 7809, 7810, 7811, 11774, 11775};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pTorment = spell;
                }
            }

            // Suffering (voidwalker)
            spellCandidates = {17735, 17750, 17751, 17752};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pSuffering = spell;
                }
            }

            // Sacrifice (voidwalker)
            spellCandidates = {7812, 19438, 19440, 19441, 19442, 19443};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pSacrifice = spell;
                }
            }

            // Consume Shadows (voidwalker)
            spellCandidates = {17767, 17850, 17851, 17852, 17853, 17854};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pConsumeShadows = spell;
                }
            }

            // Lash of Pain (succubus)
            spellCandidates = {7814, 7815, 7816, 11778, 11779, 11780};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pLashofPain = spell;
                }
            }

            // Soothing Kiss (succubus)
            spellCandidates = {6360, 7813, 11784, 11785};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pSoothingKiss = spell;
                }
            }

            // Seduction (succubus)
            {
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(6358);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(6358, false);
                    m_spells.warlock.pSeduction = spell;
                }
            }

            // Invisibility (succubus)
            {
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(7870);
                if (spell && my_level >= spell->spellLevel){
                    me->LearnSpell(7870, false);
                    m_spells.warlock.pInvisibility = spell;
                }
            }

            // Devour Magic (felhunter)
            spellCandidates = {19505, 19731, 19734, 19736};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pDevourMagic = spell;
                }
            }

            // Spell Lock (felhunter)
            spellCandidates = {19244, 19647};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pSpellLock = spell;
                }
            }

            // Tainted Blood (felhunter)
            spellCandidates = {19478, 19655, 19656, 19660};
            for (auto id: spellCandidates){
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(id);
                if (!spell)
                    continue;

                if (my_level >= spell->spellLevel){
                    me->LearnSpell(id, false);
                    m_spells.warlock.pTaintedBlood = spell;
                }
            }

            // Paranoia (felhunter) (passive)
            {
                const SpellEntry* spell = sSpellMgr.GetSpellEntry(19480);
                if (spell && my_level >= spell->spellLevel)
                    me->LearnSpell(spell->Id, false);
            }

            break;
        }
        case CLASS_PRIEST:{
            if (my_level >= 10)
            {
                // Wands are learnt by default

                if (!me->HasSpell(1180)) // Daggers
                    me->LearnSpell(1180, false);

                if (!me->HasSpell(198)) // One-handed maces
                    me->LearnSpell(198, false);

                if (!me->HasSpell(227)) // Staves
                    me->LearnSpell(227, false);
            }

            break;
        }
    }
}