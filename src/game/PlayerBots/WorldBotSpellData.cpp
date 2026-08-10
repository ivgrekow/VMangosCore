#include "WorldBotAI.h"

void WorldBotAI::PopulateSpellData() {
    CombatBotBaseAI::PopulateSpellData();

    uint32 m_class = me->GetClass();
    uint32 m_race = me->GetRace();
    uint32 my_level = me->GetLevel();

    std::vector<uint32> spellCandidates;

    uint32 spellFoodGUID = 1131;
    if (m_level < 5)
        spellFoodGUID = 433;
    else if (m_level < 15)
        spellFoodGUID = 434;
    else if (m_level < 25)
        spellFoodGUID = 435;
    else if (m_level < 35)
        spellFoodGUID = 1127;
    else if (m_level < 45)
        spellFoodGUID = 1129;
    else if (m_level < 55)
        spellFoodGUID = 1131;
    else if (m_level <= 60)
        spellFoodGUID = 25695;

    uint32 spellDrinkGUID = 1137;
    if (m_level < 5)
        spellDrinkGUID = 430;
    else if (m_level < 15)
        spellDrinkGUID = 431;
    else if (m_level < 25)
        spellDrinkGUID = 432;
    else if (m_level < 35)
        spellDrinkGUID = 1133;
    else if (m_level < 45)
        spellDrinkGUID = 1135;
    else if (m_level < 55)
        spellDrinkGUID = 1137;
    else if (m_level <= 60)
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
            if (m_level >= 10)
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

                if (!me->HasSpell(200)) // Polearms
                    me->LearnSpell(200, false);

                if (!me->HasSpell(227)) // Staves
                    me->LearnSpell(227, false);

                // Shoot Bow
                m_spells.warrior.pShootBow = sSpellMgr.GetSpellEntry(2480);

                // Shoot Gun
                m_spells.warrior.pShootGun = sSpellMgr.GetSpellEntry(7918);

                // Shoot Crossbow
                m_spells.warrior.pShootCrossbow = sSpellMgr.GetSpellEntry(7919);
            }
            
            // Dual Wield
            if (m_level >= 20) // warriors can learn it at this level
                me->LearnSpell(674, false);

            // Parry
            if (m_level >= 6)
                me->LearnSpell(3127, false);

            // Heroic Strike
            spellCandidates = {78, 284, 285, 1608, 11564, 11565, 11566, 11567, 25286}; // Heroic Strike all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (m_level >= pSpellEntry->spellLevel)
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

                if (m_level >= pSpellEntry->spellLevel)
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

                if (m_level >= pSpellEntry->spellLevel)
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

                if (m_level >= pSpellEntry->spellLevel)
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

                if (m_level >= pSpellEntry->spellLevel)
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

                if (m_level >= pSpellEntry->spellLevel)
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

                if (m_level >= pSpellEntry->spellLevel)
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

                if (m_level >= pSpellEntry->spellLevel)
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

                    if (m_level >= pSpellEntry->spellLevel)
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

                if (m_level >= pSpellEntry->spellLevel)
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

                if (m_level >= pSpellEntry->spellLevel)
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

                if (m_level >= pSpellEntry->spellLevel)
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

                if (m_level >= pSpellEntry->spellLevel)
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

                if (m_level >= pSpellEntry->spellLevel)
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

                if (m_level >= pSpellEntry->spellLevel)
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

                    if (m_level >= pSpellEntry->spellLevel)
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

                if (m_level >= pSpellEntry->spellLevel)
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

                if (m_level >= pSpellEntry->spellLevel)
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

                if (m_level >= pSpellEntry->spellLevel)
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

                if (m_level >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    m_spells.warrior.pShieldSlam = pSpellEntry;
                }
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
                spellCandidates = {18220, 18937, 18938};
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
                spellCandidates = {18265, 18879, 18880, 18881};
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

                if (!me->HasSpell(200)) // Polearms
                    me->LearnSpell(200, false);

                if (!me->HasSpell(227)) // Staves
                    me->LearnSpell(227, false);
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
                    m_spells.hunter.pPetCower = spell;
                }
            }

            // Prowl (cats)
            spellCandidates = {24451, 24454, 24455};
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



            break;
        }
    }
}