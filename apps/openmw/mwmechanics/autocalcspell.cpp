#include "autocalcspell.hpp"

#include <cmath>
#include <cstdint>
#include <limits>

#include <components/esm/attr.hpp>
#include <components/esm/refid.hpp>
#include <components/esm3/loadmgef.hpp>
#include <components/esm3/loadrace.hpp>
#include <components/esm3/loadspel.hpp>

#include "../mwworld/esmstore.hpp"

#include "../mwbase/environment.hpp"

#include "spellutil.hpp"
#include "spellcalculation.hpp"

namespace MWMechanics
{

    struct SchoolCaps
    {
        int mCount;
        int mLimit;
        bool mReachedLimit;
        int mMinCost;
        ESM::RefId mWeakestSpell;
    };

    static std::vector<ESM::RefId> autoCalcNpcSpellsImpl(const std::map<ESM::RefId, SkillValue>& actorSkills,
        const std::map<ESM::RefId, AttributeValue>& actorAttributes, const ESM::Race* race, SpellCalculationContext& calculation)
    {
        const float fNPCbaseMagickaMult = calculation.setting(spellCalculationCaches().mNpcMagicka);
        float baseMagicka = fNPCbaseMagickaMult * actorAttributes.at(ESM::Attribute::Intelligence).getBase();
        if (calculation.isPreparing() && !std::isfinite(baseMagicka))
            throw std::runtime_error("Prepared NPC spell magicka is nonfinite");

        std::map<ESM::RefId, SchoolCaps> schoolCaps;
        for (const ESM::Skill& skill : calculation.store().get<ESM::Skill>())
        {
            if (calculation.isPreparing()
                && calculation.store().get<ESM::Skill>().searchStatic(skill.mId) != &skill)
                continue;
            if (!skill.mSchool)
                continue;
            SchoolCaps caps;
            caps.mCount = 0;
            caps.mLimit = skill.mSchool->mAutoCalcMax;
            caps.mReachedLimit = skill.mSchool->mAutoCalcMax <= 0;
            caps.mMinCost = std::numeric_limits<int>::max();
            caps.mWeakestSpell = ESM::RefId();
            schoolCaps[skill.mId] = caps;
        }

        std::vector<ESM::RefId> selectedSpells;

        const auto spells = calculation.spells();

        // Note: the algorithm heavily depends on the traversal order of the spells. For vanilla-compatible results the
        // Store must preserve the record ordering as it was in the content files.
        for (const ESM::Spell* candidate : spells)
        {
            const ESM::Spell& spell = *candidate;
            if (spell.mData.mType != ESM::Spell::ST_Spell)
                continue;
            if (!(spell.mData.mFlags & ESM::Spell::F_Autocalc))
                continue;
            const int iAutoSpellTimesCanCast = calculation.setting(spellCalculationCaches().mTimesCanCast);
            int spellCost = MWMechanics::calcSpellCost(spell, &calculation);
            if (calculation.isPreparing())
            {
                const auto requiredMagicka = std::int64_t(iAutoSpellTimesCanCast) * spellCost;
                if (requiredMagicka < std::numeric_limits<int>::min()
                    || requiredMagicka > std::numeric_limits<int>::max())
                    throw std::runtime_error("Prepared NPC spell affordability exceeds the integer domain");
            }
            if (baseMagicka < iAutoSpellTimesCanCast * spellCost)
                continue;

            if (race && race->mPowers.exists(spell.mId))
                continue;

            if (!attrSkillCheck(&spell, actorSkills, actorAttributes, &calculation))
                continue;

            ESM::RefId school;
            float skillTerm;
            calcWeakestSchool(&spell, actorSkills, school, skillTerm, &calculation);
            if (school.empty())
                continue;
            SchoolCaps& cap = schoolCaps[school];

            if (cap.mReachedLimit && spellCost <= cap.mMinCost)
                continue;

            const float fAutoSpellChance = calculation.setting(spellCalculationCaches().mNpcChance);
            if (calcAutoCastChance(&spell, actorSkills, actorAttributes, school, &calculation) < fAutoSpellChance)
                continue;

            selectedSpells.push_back(spell.mId);

            if (cap.mReachedLimit)
            {
                auto found = std::find(selectedSpells.begin(), selectedSpells.end(), cap.mWeakestSpell);
                if (found != selectedSpells.end())
                    selectedSpells.erase(found);

                cap.mMinCost = std::numeric_limits<int>::max();
                for (const ESM::RefId& testSpellName : selectedSpells)
                {
                    const ESM::Spell* testSpell = calculation.spell(testSpellName);
                    int testSpellCost = MWMechanics::calcSpellCost(*testSpell, &calculation);

                    // int testSchool;
                    // float dummySkillTerm;
                    // calcWeakestSchool(testSpell, actorSkills, testSchool, dummySkillTerm);

                    // Note: if there are multiple spells with the same cost, we pick the first one we found.
                    // So the algorithm depends on the iteration order of the outer loop.
                    if (
                        // There is a huge bug here. It is not checked that weakestSpell is of the correct school.
                        // As result multiple SchoolCaps could have the same mWeakestSpell. Erasing the weakest spell
                        // would then fail if another school already erased it, and so the number of spells would often
                        // exceed the sum of limits. This bug cannot be fixed without significantly changing the results
                        // of the spell autocalc, which will not have been playtested.
                        // testSchool == school &&
                        testSpellCost < cap.mMinCost)
                    {
                        cap.mMinCost = testSpellCost;
                        cap.mWeakestSpell = testSpell->mId;
                    }
                }
            }
            else
            {
                cap.mCount += 1;
                if (cap.mCount == cap.mLimit)
                    cap.mReachedLimit = true;

                if (spellCost < cap.mMinCost)
                {
                    cap.mWeakestSpell = spell.mId;
                    cap.mMinCost = spellCost;
                }
            }
        }

        return selectedSpells;
    }

    std::vector<ESM::RefId> autoCalcNpcSpells(const std::map<ESM::RefId, SkillValue>& actorSkills,
        const std::map<ESM::RefId, AttributeValue>& actorAttributes, const ESM::Race* race)
    {
        SpellCalculationContext calculation(*MWBase::Environment::get().getESMStore());
        return autoCalcNpcSpellsImpl(actorSkills, actorAttributes, race, calculation);
    }

    PreparedNpcSpells prepareNpcSpells(const std::map<ESM::RefId, SkillValue>& actorSkills,
        const std::map<ESM::RefId, AttributeValue>& actorAttributes, const ESM::Race* race,
        const MWWorld::ESMStore& store, const MWWorld::ESMStore& incoming, std::size_t priorCapacity)
    {
        SpellCalculationContext calculation(store, &incoming);
        const auto selected = autoCalcNpcSpellsImpl(actorSkills, actorAttributes, race, calculation);
        PreparedNpcSpells result;
        result.mSpells = Spells::prepareInstance(selected, store, incoming, priorCapacity);
        result.mCacheCommits = calculation.cacheCommits();
        return result;
    }

    void PreparedNpcSpells::install(Spells& target)
    {
        if (mConsumed) throw std::logic_error("Prepared NPC autocalculated spells already consumed");
        for (const auto& commit : mCacheCommits) commit();
        mSpells.install(target);
        mConsumed = true;
    }

    std::vector<ESM::RefId> autoCalcPlayerSpells(const std::map<ESM::RefId, SkillValue>& actorSkills,
        const std::map<ESM::RefId, AttributeValue>& actorAttributes, const ESM::Race* race)
    {
        const MWWorld::ESMStore& esmStore = *MWBase::Environment::get().getESMStore();

        static const float fPCbaseMagickaMult
            = esmStore.get<ESM::GameSetting>().find("fPCbaseMagickaMult")->mValue.getFloat();

        float baseMagicka = fPCbaseMagickaMult * actorAttributes.at(ESM::Attribute::Intelligence).getBase();
        bool reachedLimit = false;
        const ESM::Spell* weakestSpell = nullptr;
        int minCost = std::numeric_limits<int>::max();

        std::vector<ESM::RefId> selectedSpells;

        const MWWorld::Store<ESM::Spell>& spells = esmStore.get<ESM::Spell>();
        for (const ESM::Spell& spell : spells)
        {
            if (spell.mData.mType != ESM::Spell::ST_Spell)
                continue;
            if (!(spell.mData.mFlags & ESM::Spell::F_PCStart))
                continue;

            int spellCost = MWMechanics::calcSpellCost(spell);
            if (reachedLimit && spellCost <= minCost)
                continue;
            if (race
                && std::find(race->mPowers.mList.begin(), race->mPowers.mList.end(), spell.mId)
                    != race->mPowers.mList.end())
                continue;
            if (baseMagicka < spellCost)
                continue;

            static const float fAutoPCSpellChance
                = esmStore.get<ESM::GameSetting>().find("fAutoPCSpellChance")->mValue.getFloat();
            if (calcAutoCastChance(&spell, actorSkills, actorAttributes, {}) < fAutoPCSpellChance)
                continue;

            if (!attrSkillCheck(&spell, actorSkills, actorAttributes))
                continue;

            selectedSpells.push_back(spell.mId);

            if (reachedLimit)
            {
                std::vector<ESM::RefId>::iterator it
                    = std::find(selectedSpells.begin(), selectedSpells.end(), weakestSpell->mId);
                if (it != selectedSpells.end())
                    selectedSpells.erase(it);

                minCost = std::numeric_limits<int>::max();
                for (const ESM::RefId& testSpellName : selectedSpells)
                {
                    const ESM::Spell* testSpell = esmStore.get<ESM::Spell>().find(testSpellName);
                    int testSpellCost = MWMechanics::calcSpellCost(*testSpell);
                    if (testSpellCost < minCost)
                    {
                        minCost = testSpellCost;
                        weakestSpell = testSpell;
                    }
                }
            }
            else
            {
                if (spellCost < minCost)
                {
                    weakestSpell = &spell;
                    minCost = MWMechanics::calcSpellCost(*weakestSpell);
                }
                static const unsigned int iAutoPCSpellMax
                    = esmStore.get<ESM::GameSetting>().find("iAutoPCSpellMax")->mValue.getInteger();
                if (selectedSpells.size() == iAutoPCSpellMax)
                    reachedLimit = true;
            }
        }

        return selectedSpells;
    }

    bool attrSkillCheck(const ESM::Spell* spell, const std::map<ESM::RefId, SkillValue>& actorSkills,
        const std::map<ESM::RefId, AttributeValue>& actorAttributes, SpellCalculationContext* context)
    {
        std::optional<SpellCalculationContext> ordinary;
        for (const auto& spellEffect : spell->mEffects.mList)
        {
            if (!context && !ordinary) ordinary.emplace(*MWBase::Environment::get().getESMStore());
            auto& calculation = context ? *context : *ordinary;
            const ESM::MagicEffect* magicEffect
                = calculation.effect(spellEffect.mData.mEffectID);
            const int iAutoSpellAttSkillMin = calculation.setting(spellCalculationCaches().mAttributeSkillMinimum);

            if ((magicEffect->mData.mFlags & ESM::MagicEffect::TargetSkill))
            {
                auto found = actorSkills.find(spellEffect.mData.mSkill);
                if (found == actorSkills.end() || found->second.getBase() < iAutoSpellAttSkillMin)
                    return false;
            }

            if ((magicEffect->mData.mFlags & ESM::MagicEffect::TargetAttribute))
            {
                auto found = actorAttributes.find(spellEffect.mData.mAttribute);
                if (found == actorAttributes.end() || found->second.getBase() < iAutoSpellAttSkillMin)
                    return false;
            }
        }

        return true;
    }

    void calcWeakestSchool(const ESM::Spell* spell, const std::map<ESM::RefId, SkillValue>& actorSkills,
        ESM::RefId& effectiveSchool, float& skillTerm, SpellCalculationContext* context)
    {
        std::optional<SpellCalculationContext> ordinary;
        // Morrowind for some reason uses a formula slightly different from magicka cost calculation
        float minChance = std::numeric_limits<float>::max();
        for (const ESM::IndexedENAMstruct& effect : spell->mEffects.mList)
        {
            if (!context && !ordinary) ordinary.emplace(*MWBase::Environment::get().getESMStore());
            auto& calculation = context ? *context : *ordinary;
            const ESM::MagicEffect* magicEffect
                = calculation.effect(effect.mData.mEffectID);

            int minMagn = 1;
            int maxMagn = 1;
            if (!(magicEffect->mData.mFlags & ESM::MagicEffect::NoMagnitude))
            {
                minMagn = effect.mData.mMagnMin;
                maxMagn = effect.mData.mMagnMax;
            }

            int duration = 0;
            if (!(magicEffect->mData.mFlags & ESM::MagicEffect::NoDuration))
                duration = effect.mData.mDuration;
            if (!(magicEffect->mData.mFlags & ESM::MagicEffect::AppliedOnce))
                duration = std::max(1, duration);

            const float fEffectCostMult = calculation.setting(spellCalculationCaches().mWeakestSchoolCost);

            if (calculation.isPreparing())
            {
                const auto magnitude = std::int64_t(std::max(1, minMagn)) + std::max(1, maxMagn);
                const auto durationTerm = std::int64_t(1) + duration;
                if (magnitude > std::numeric_limits<int>::max()
                    || durationTerm > std::numeric_limits<int>::max())
                    throw std::runtime_error("Prepared spell school arithmetic exceeds the integer domain");
            }
            float x = 0.5f * (std::max(1, minMagn) + std::max(1, maxMagn));
            x *= 0.1f * magicEffect->mData.mBaseCost;
            x *= 1 + duration;
            x += 0.05f * std::max(1, effect.mData.mArea) * magicEffect->mData.mBaseCost;
            x *= fEffectCostMult;

            if (effect.mData.mRange == ESM::RT_Target)
                x *= 1.5f;

            float s = 0.f;
            auto found = actorSkills.find(magicEffect->mData.mSchool);
            if (found != actorSkills.end())
                s = 2.f * found->second.getBase();
            if (s - x < minChance)
            {
                minChance = s - x;
                effectiveSchool = magicEffect->mData.mSchool;
                skillTerm = s;
            }
        }
    }

    float calcAutoCastChance(const ESM::Spell* spell, const std::map<ESM::RefId, SkillValue>& actorSkills,
        const std::map<ESM::RefId, AttributeValue>& actorAttributes, ESM::RefId effectiveSchool,
        SpellCalculationContext* context)
    {
        if (spell->mData.mType != ESM::Spell::ST_Spell)
            return 100.f;

        if (spell->mData.mFlags & ESM::Spell::F_Always)
            return 100.f;

        float skillTerm = 0;
        if (!effectiveSchool.empty())
        {
            auto found = actorSkills.find(effectiveSchool);
            if (found != actorSkills.end())
                skillTerm = 2.f * found->second.getBase();
        }
        else
            calcWeakestSchool(
                spell, actorSkills, effectiveSchool, skillTerm, context); // Note effectiveSchool is unused after this

        float castChance = skillTerm - MWMechanics::calcSpellCost(*spell, context)
            + 0.2f * actorAttributes.at(ESM::Attribute::Willpower).getBase()
            + 0.1f * actorAttributes.at(ESM::Attribute::Luck).getBase();
        return castChance;
    }
}
