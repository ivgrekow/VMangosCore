#include "WorldBotAI.h"

void WorldBotAI::PopulateSpellData(){
    uint32 myclass = me->GetClass();
    uint32 myrace = me->GetRace();

    std::vector<const SpellEntry*> actualSpells;
    std::string spellName;
    std::vector<uint32> spellCandidates;

    uint32 spellFoodGUID = 1131;
    if (me->GetLevel() < 5)
        spellFoodGUID = 433;
    else if (me->GetLevel() < 15)
        spellFoodGUID = 434;
    else if (me->GetLevel() < 25)
        spellFoodGUID = 435;
    else if (me->GetLevel() < 35)
        spellFoodGUID = 1127;
    else if (me->GetLevel() < 45)
        spellFoodGUID = 1129;
    else if (me->GetLevel() < 55)
        spellFoodGUID = 1131;
    else if (me->GetLevel() <= 60)
        spellFoodGUID = 25695;

    uint32 spellDrinkGUID = 1137;
    if (me->GetLevel() < 5)
        spellDrinkGUID = 430;
    else if (me->GetLevel() < 15)
        spellDrinkGUID = 431;
    else if (me->GetLevel() < 25)
        spellDrinkGUID = 432;
    else if (me->GetLevel() < 35)
        spellDrinkGUID = 1133;
    else if (me->GetLevel() < 45)
        spellDrinkGUID = 1135;
    else if (me->GetLevel() < 55)
        spellDrinkGUID = 1137;
    else if (me->GetLevel() <= 60)
        spellDrinkGUID = 25696;

    m_spellBook.insert({ "Eat (consumable)", std::vector<const SpellEntry*>{sSpellMgr.GetSpellEntry(spellFoodGUID)} });
    m_spellBook.insert({ "Drink (consumable)", std::vector<const SpellEntry*>{sSpellMgr.GetSpellEntry(spellDrinkGUID)} });

    actualSpells.clear();

    switch(myrace)
    {
        case RACE_HUMAN:
        {
            // Perception
            spellName = "Perception (racial)";
            spellCandidates = {20600}; // Perception
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
                
            }
            m_spellBook.insert({ spellName, actualSpells });
        }
        case RACE_DWARF:
        {
            // Stoneform
            spellName = "Stoneform (racial)";
            spellCandidates = {20594}; // Stoneform
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
                
            }
            m_spellBook.insert({ spellName, actualSpells });
        }
        case RACE_NIGHTELF:
        {
            // Shadowmeld
            spellName = "Shadowmeld (racial)";
            spellCandidates = {20580}; // Shadowmeld
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
                
            }
            m_spellBook.insert({ spellName, actualSpells });
        }
        case RACE_GNOME:
        {
            // Escape Artist
            spellName = "Escape Artist (racial)";
            spellCandidates = {20589}; // Escape Artist
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
                
            }
            m_spellBook.insert({ spellName, actualSpells });
        }
        case RACE_ORC:
        {
            // Blood Fury
            spellName = "Blood Fury (racial)";
            spellCandidates = {20572}; // Blood Fury
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
                
            }
            m_spellBook.insert({ spellName, actualSpells });
        }
        case RACE_UNDEAD:
        {
            // Will of the Forsaken
            spellName = "Will of the Forsaken (racial)";
            spellCandidates = {7744}; // Will of the Forsaken
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
                
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Cannibalize
            spellName = "Cannibalize (racial)";
            actualSpells.clear();
            spellCandidates = {20577}; // Cannibalize
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
                
            }
            m_spellBook.insert({ spellName, actualSpells });
        }
        case RACE_TAUREN:
        {
            // War Stomp
            spellName = "War Stomp (racial)";
            spellCandidates = {20549}; // War Stomp
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
                
            }
            m_spellBook.insert({ spellName, actualSpells });
        }
        case RACE_TROLL:
        {
            // Berserking
            spellName = "Berserking (racial)";
            spellCandidates = {26297}; // Berserking
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
                
            }
            m_spellBook.insert({ spellName, actualSpells });
        }
    }

    actualSpells.clear();

    switch (myclass){
        case CLASS_WARRIOR:
        {
            // Weapon skills (all weapons except for wands)
            if (me->GetLevel() >= 10)
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

                actualSpells.clear();
                // Shoot Bow
                if (const SpellEntry* pSpell = sSpellMgr.GetSpellEntry(2480))
                {
                    std::vector<const SpellEntry*> vec = {pSpell};
                    m_spellBook.insert({ "Shoot Bow", vec });
                }
                // Shoot Gun
                if (const SpellEntry* pSpell = sSpellMgr.GetSpellEntry(7918))
                {
                    std::vector<const SpellEntry*> vec = {pSpell};
                    m_spellBook.insert({ "Shoot Gun", vec });
                }
                // Shoot Crossbow
                if (const SpellEntry* pSpell = sSpellMgr.GetSpellEntry(7919))
                {
                    std::vector<const SpellEntry*> vec = {pSpell};
                    m_spellBook.insert({ "Shoot Crossbow", vec });
                }
            }

            // Parry
            if (me->GetLevel() >= 6)
                me->LearnSpell(3127, false);

            // Heroic Strike
            spellName = "Heroic Strike";
            actualSpells.clear();
            spellCandidates = {78, 284, 285, 1608, 11564, 11565, 11566, 11567, 25286}; // Heroic Strike all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Rend
            spellName = "Rend";
            actualSpells.clear();
            spellCandidates = {772, 6546, 6547, 6548, 11572, 11573, 11574}; // Rend all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Execute
            spellName = "Execute";
            actualSpells.clear();
            spellCandidates = {5308, 20658, 20660, 20661, 20662}; // Execute all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Charge
            spellName = "Charge";
            actualSpells.clear();
            spellCandidates = {100, 6178, 11578}; // Charge all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Thunder Clap
            spellName = "Thunder Clap";
            actualSpells.clear();
            spellCandidates = {6343, 8198, 8204, 8205, 11580, 11581}; // Thunder Clap all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Hamstring
            spellName = "Hamstring";
            actualSpells.clear();
            spellCandidates = {1715, 7372, 7373}; // Hamstring all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Overpower
            spellName = "Overpower";
            actualSpells.clear();
            spellCandidates = {7384, 7887, 11584, 11585}; // Overpower all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Mocking Blow
            spellName = "Mocking Blow";
            actualSpells.clear();
            spellCandidates = {694, 7400, 7402, 20559, 20560}; // Mocking Blow all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Retaliation
            spellName = "Retaliation";
            actualSpells.clear();
            spellCandidates = {20230}; // Retaliation
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            if (me->HasSpell(12292))
            {
                // Sweeping Strikes
                spellName = "Sweeping Strikes";
                actualSpells.clear();
                spellCandidates = {12292}; // Sweeping Strikes all ranks
                for (auto spellId : spellCandidates)
                {
                    const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                    if (!pSpellEntry)
                        continue;

                    if (me->GetLevel() >= pSpellEntry->spellLevel)
                    {
                        me->LearnSpell(spellId, false);
                        actualSpells.push_back(pSpellEntry);
                    }
                }
                m_spellBook.insert({ spellName, actualSpells });
            }

            if (me->HasSpell(12294))
            {
                // Mortal Strike
                spellName = "Mortal Strike";
                actualSpells.clear();
                spellCandidates = {12294, 21551, 21552, 21553}; // Mortal Strike all ranks
                for (auto spellId : spellCandidates)
                {
                    const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                    if (!pSpellEntry)
                        continue;

                    if (me->GetLevel() >= pSpellEntry->spellLevel)
                    {
                        me->LearnSpell(spellId, false);
                        actualSpells.push_back(pSpellEntry);
                    }
                }
                m_spellBook.insert({ spellName, actualSpells });
            }

            // Battle Shout
            spellName = "Battle Shout";
            actualSpells.clear();
            spellCandidates = {6673, 5242, 6192, 11549, 11550, 11551, 25289}; // Battle Shout all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Demoralizing Shout
            spellName = "Demoralizing Shout";
            actualSpells.clear();
            spellCandidates = {1160, 6190, 11554, 11555, 11556}; // Demoralizing Shout all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Challenging Shout
            spellName = "Challenging Shout";
            actualSpells.clear();
            spellCandidates = {1161}; // Challenging Shout
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Intimidating Shout
            spellName = "Intimidating Shout";
            actualSpells.clear();
            spellCandidates = {5246}; // Intimidating Shout
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Slam
            spellName = "Slam";
            actualSpells.clear();
            spellCandidates = {1464, 8820, 11604, 11605}; // Slam all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Berserker Stance
            spellName = "Berserker Stance";
            actualSpells.clear();
            spellCandidates = {2458}; // Berserker Stance
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Cleave
            spellName = "Cleave";
            actualSpells.clear();
            spellCandidates = {845, 7369, 11608, 11609, 20569}; // Cleave all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Intercept
            spellName = "Intercept";
            actualSpells.clear();
            spellCandidates = {20252, 20616, 20617}; // Intercept all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Whirlwind
            spellName = "Whirlwind";
            actualSpells.clear();
            spellCandidates = {1680}; // Whirlwind
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Pummel
            spellName = "Pummel";
            actualSpells.clear();
            spellCandidates = {6552, 6554}; // Pummel all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            if (me->HasSpell(12323))
            {
                // Piercing Howl
                spellName = "Piercing Howl";
                actualSpells.clear();
                spellCandidates = {12323}; // Piercing Howl
                for (auto spellId : spellCandidates)
                {
                    const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                    if (!pSpellEntry)
                        continue;

                    if (me->GetLevel() >= pSpellEntry->spellLevel)
                    {
                        me->LearnSpell(spellId, false);
                        actualSpells.push_back(pSpellEntry);
                    }
                }
                m_spellBook.insert({ spellName, actualSpells });
            }

            if (me->HasSpell(12328))
            {
                // Death Wish
                spellName = "Death Wish";
                actualSpells.clear();
                spellCandidates = {12328}; // Death Wish
                for (auto spellId : spellCandidates)
                {
                    const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                    if (!pSpellEntry)
                        continue;

                    if (me->GetLevel() >= pSpellEntry->spellLevel)
                    {
                        me->LearnSpell(spellId, false);
                        actualSpells.push_back(pSpellEntry);
                    }
                }
                m_spellBook.insert({ spellName, actualSpells });
            }

            if (me->HasSpell(23881))
            {
                // Bloodthirst
                spellName = "Bloodthirst";
                actualSpells.clear();
                spellCandidates = {23881, 23892, 23893, 23894}; // Bloodthirst all ranks
                for (auto spellId : spellCandidates)
                {
                    const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                    if (!pSpellEntry)
                        continue;

                    if (me->GetLevel() >= pSpellEntry->spellLevel)
                    {
                        me->LearnSpell(spellId, false);
                        actualSpells.push_back(pSpellEntry);
                    }
                }
                m_spellBook.insert({ spellName, actualSpells });
            }

            // Recklessness
            spellName = "Recklessness";
            actualSpells.clear();
            spellCandidates = {1719}; // Recklessness
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Battle Stance
            spellName = "Battle Stance";
            actualSpells.clear();
            spellCandidates = {2457}; // Battle Stance
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Defensive Stance
            spellName = "Defensive Stance";
            actualSpells.clear();
            spellCandidates = {71}; // Defensive Stance
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Sunder Armor
            spellName = "Sunder Armor";
            actualSpells.clear();
            spellCandidates = {7386, 7405, 8380, 11596, 11597}; // Sunder Armor all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Bloodrage
            spellName = "Bloodrage";
            actualSpells.clear();
            spellCandidates = {2687}; // Bloodrage all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Taunt
            spellName = "Taunt";
            actualSpells.clear();
            spellCandidates = {355}; // Taunt all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Shield Bash
            spellName = "Shield Bash";
            actualSpells.clear();
            spellCandidates = {72, 1671, 1672}; // Shield Bash all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Revenge
            spellName = "Revenge";
            actualSpells.clear();
            spellCandidates = {6572, 6574, 7379, 11600, 11601, 25288}; // Revenge all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Shield Wall
            spellName = "Shield Wall";
            actualSpells.clear();
            spellCandidates = {871}; // Shield Wall
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Shield Block
            spellName = "Shield Block";
            actualSpells.clear();
            spellCandidates = {2565}; // Shield Block
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Disarm
            spellName = "Disarm";
            actualSpells.clear();
            spellCandidates = {676}; // Disarm
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Last Stand
            spellName = "Last Stand";
            actualSpells.clear();
            spellCandidates = {12975}; // Last Stand
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Concussion Blow
            spellName = "Concussion Blow";
            actualSpells.clear();
            spellCandidates = {12809}; // Concussion Blow
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            // Shield Slam
            spellName = "Shield Slam";
            actualSpells.clear();
            spellCandidates = {23922, 23923, 23924, 23925}; // Shield Slam all ranks
            for (auto spellId : spellCandidates)
            {
                const SpellEntry* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);

                if (!pSpellEntry)
                    continue;

                if (me->GetLevel() >= pSpellEntry->spellLevel)
                {
                    me->LearnSpell(spellId, false);
                    actualSpells.push_back(pSpellEntry);
                }
            }
            m_spellBook.insert({ spellName, actualSpells });

            break;
        }
    }
}