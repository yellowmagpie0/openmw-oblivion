#include "spellutil.hpp"
#include "spellcalculation.hpp"

#include <cmath>
#include <cstdint>
#include <limits>

#include <components/esm3/loadalch.hpp>
#include <components/esm3/loadench.hpp>
#include <components/esm3/loadingr.hpp>
#include <components/esm3/loadmgef.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"

#include "actorutil.hpp"
#include "creaturestats.hpp"

namespace MWMechanics
{
    namespace
    {
        float getTotalCost(const ESM::EffectList& list, const EffectCostMethod method = EffectCostMethod::GameSpell,
            SpellCalculationContext* context = nullptr)
        {
            float cost = 0;

            for (const ESM::IndexedENAMstruct& effect : list.mList)
            {
                const float rawCost = MWMechanics::calcEffectCost(effect.mData, nullptr, method, context);
                if (context && context->isPreparing() && !std::isfinite(rawCost))
                    throw std::runtime_error("Prepared spell effect cost is nonfinite");
                float effectCost = std::max(0.f, rawCost);

                // This is applied to the whole spell cost for each effect when
                // creating spells, but is only applied on the effect itself in TES:CS.
                if (effect.mData.mRange == ESM::RT_Target)
                    effectCost *= 1.5;

                cost += effectCost;
            }
            return cost;
        }
    }

    float calcEffectCost(
        const ESM::ENAMstruct& effect, const ESM::MagicEffect* magicEffect, const EffectCostMethod method,
        SpellCalculationContext* context)
    {
        std::optional<SpellCalculationContext> ordinary;
        if (!context) ordinary.emplace(*MWBase::Environment::get().getESMStore());
        auto& calculation = context ? *context : *ordinary;
        if (!magicEffect)
            magicEffect = calculation.effect(effect.mEffectID);
        bool hasMagnitude = !(magicEffect->mData.mFlags & ESM::MagicEffect::NoMagnitude);
        bool hasDuration = !(magicEffect->mData.mFlags & ESM::MagicEffect::NoDuration);
        bool appliedOnce = magicEffect->mData.mFlags & ESM::MagicEffect::AppliedOnce;
        int minMagn = hasMagnitude ? effect.mMagnMin : 1;
        int maxMagn = hasMagnitude ? effect.mMagnMax : 1;
        if (method == EffectCostMethod::PlayerSpell || method == EffectCostMethod::GameSpell)
        {
            minMagn = std::max(1, minMagn);
            maxMagn = std::max(1, maxMagn);
        }
        int duration = hasDuration ? effect.mDuration : 1;
        if (!appliedOnce)
            duration = std::max(1, duration);
        const float fEffectCostMult = calculation.setting(spellCalculationCaches().mEffectCost);
        const float iAlchemyMod = calculation.setting(spellCalculationCaches().mAlchemyCost);

        int durationOffset = 0;
        int minArea = 0;
        float costMult = fEffectCostMult;
        if (method == EffectCostMethod::PlayerSpell)
        {
            durationOffset = 1;
            minArea = 1;
        }
        else if (method == EffectCostMethod::GamePotion)
        {
            minArea = 1;
            costMult = iAlchemyMod;
        }

        if (calculation.isPreparing())
        {
            const auto magnitude = std::int64_t(minMagn) + maxMagn;
            const auto durationTerm = std::int64_t(durationOffset) + duration;
            if (magnitude < std::numeric_limits<int>::min() || magnitude > std::numeric_limits<int>::max()
                || durationTerm < std::numeric_limits<int>::min() || durationTerm > std::numeric_limits<int>::max())
                throw std::runtime_error("Prepared spell effect arithmetic exceeds the integer domain");
        }
        float x = 0.5f * (minMagn + maxMagn);
        x *= 0.1f * magicEffect->mData.mBaseCost;
        x *= durationOffset + duration;
        x += 0.05f * std::max(minArea, effect.mArea) * magicEffect->mData.mBaseCost;

        return x * costMult;
    }

    int calcSpellCost(const ESM::Spell& spell, SpellCalculationContext* context)
    {
        if (!(spell.mData.mFlags & ESM::Spell::F_Autocalc))
            return spell.mData.mCost;

        float cost = getTotalCost(spell.mEffects, EffectCostMethod::GameSpell, context);

        const float rounded = std::round(cost);
        if (context && context->isPreparing()
            && (!std::isfinite(rounded) || rounded < -0x1p31f || rounded >= 0x1p31f))
            throw std::runtime_error("Prepared spell cost exceeds the integer domain");
        return static_cast<int>(rounded);
    }

    int getEffectiveEnchantmentCastCost(float castCost, const MWWorld::Ptr& actor)
    {
        /*
         * Each point of enchant skill above/under 10 subtracts/adds
         * one percent of enchantment cost while minimum is 1.
         */
        float eSkill = actor.getClass().getSkill(actor, ESM::Skill::Enchant);
        const float result = castCost - (castCost / 100) * (eSkill - 10);

        return static_cast<int>((result < 1) ? 1 : result);
    }

    int getEffectiveEnchantmentCastCost(const ESM::Enchantment& enchantment, const MWWorld::Ptr& actor)
    {
        float castCost;
        if (enchantment.mData.mFlags & ESM::Enchantment::Autocalc)
            castCost = getTotalCost(enchantment.mEffects, EffectCostMethod::GameEnchantment);
        else
            castCost = static_cast<float>(enchantment.mData.mCost);
        return getEffectiveEnchantmentCastCost(castCost, actor);
    }

    int getEnchantmentCharge(const ESM::Enchantment& enchantment)
    {
        if (enchantment.mData.mFlags & ESM::Enchantment::Autocalc)
        {
            int charge
                = static_cast<int>(std::round(getTotalCost(enchantment.mEffects, EffectCostMethod::GameEnchantment)));
            const auto& store = MWBase::Environment::get().getESMStore()->get<ESM::GameSetting>();
            switch (enchantment.mData.mType)
            {
                case ESM::Enchantment::CastOnce:
                {
                    static const int iMagicItemChargeOnce = store.find("iMagicItemChargeOnce")->mValue.getInteger();
                    return charge * iMagicItemChargeOnce;
                }
                case ESM::Enchantment::WhenStrikes:
                {
                    static const int iMagicItemChargeStrike = store.find("iMagicItemChargeStrike")->mValue.getInteger();
                    return charge * iMagicItemChargeStrike;
                }
                case ESM::Enchantment::WhenUsed:
                {
                    static const int iMagicItemChargeUse = store.find("iMagicItemChargeUse")->mValue.getInteger();
                    return charge * iMagicItemChargeUse;
                }
                case ESM::Enchantment::ConstantEffect:
                {
                    static const int iMagicItemChargeConst = store.find("iMagicItemChargeConst")->mValue.getInteger();
                    return charge * iMagicItemChargeConst;
                }
            }
        }
        return enchantment.mData.mCharge;
    }

    int getPotionValue(const ESM::Potion& potion)
    {
        if (potion.mData.mFlags & ESM::Potion::Autocalc)
        {
            float cost = getTotalCost(potion.mEffects, EffectCostMethod::GamePotion);
            return static_cast<int>(std::round(cost));
        }
        return potion.mData.mValue;
    }

    std::optional<ESM::EffectList> rollIngredientEffect(
        MWWorld::Ptr caster, const ESM::Ingredient* ingredient, uint32_t index)
    {
        if (index >= 4)
            throw std::range_error("Index out of range");

        ESM::ENAMstruct effect;
        effect.mEffectID = ingredient->mData.mEffectID[index];
        effect.mSkill = ingredient->mData.mSkills[index];
        effect.mAttribute = ingredient->mData.mAttributes[index];
        effect.mRange = ESM::RT_Self;
        effect.mArea = 0;

        if (effect.mEffectID.empty())
            return std::nullopt;

        const MWWorld::ESMStore& store = *MWBase::Environment::get().getESMStore();
        const auto magicEffect = store.get<ESM::MagicEffect>().find(effect.mEffectID);
        const MWMechanics::CreatureStats& creatureStats = caster.getClass().getCreatureStats(caster);

        float x = (caster.getClass().getSkill(caster, ESM::Skill::Alchemy)
                      + 0.2f * creatureStats.getAttribute(ESM::Attribute::Intelligence).getModified()
                      + 0.1f * creatureStats.getAttribute(ESM::Attribute::Luck).getModified())
            * creatureStats.getFatigueTerm();

        auto& prng = MWBase::Environment::get().getWorld()->getPrng();
        int roll = Misc::Rng::roll0to99(prng);
        if (roll > x)
        {
            return std::nullopt;
        }

        float magnitude = 0;
        float y = roll / std::min(x, 100.f);
        y *= 0.25f * x;
        if (magicEffect->mData.mFlags & ESM::MagicEffect::NoDuration)
            effect.mDuration = 1;
        else
            effect.mDuration = static_cast<int>(y);
        if (!(magicEffect->mData.mFlags & ESM::MagicEffect::NoMagnitude))
        {
            if (!(magicEffect->mData.mFlags & ESM::MagicEffect::NoDuration))
                magnitude = floor((0.05f * y) / (0.1f * magicEffect->mData.mBaseCost));
            else
                magnitude = floor(y / (0.1f * magicEffect->mData.mBaseCost));
            magnitude = std::max(1.f, magnitude);
        }
        else
            magnitude = 1;

        effect.mMagnMax = static_cast<int>(magnitude);
        effect.mMagnMin = static_cast<int>(magnitude);

        ESM::EffectList effects;
        effects.mList.push_back({ effect, index });
        return effects;
    }

    float calcSpellBaseSuccessChance(const ESM::Spell* spell, const MWWorld::Ptr& actor, ESM::RefId* effectiveSchool)
    {
        // Morrowind for some reason uses a formula slightly different from magicka cost calculation
        float y = std::numeric_limits<float>::max();
        float lowestSkill = 0;

        for (const ESM::IndexedENAMstruct& effect : spell->mEffects.mList)
        {
            float x = static_cast<float>(effect.mData.mDuration);
            const auto magicEffect
                = MWBase::Environment::get().getESMStore()->get<ESM::MagicEffect>().find(effect.mData.mEffectID);

            if (!(magicEffect->mData.mFlags & ESM::MagicEffect::AppliedOnce))
                x = std::max(1.f, x);

            x *= 0.1f * magicEffect->mData.mBaseCost;
            x *= 0.5f * (effect.mData.mMagnMin + effect.mData.mMagnMax);
            x += effect.mData.mArea * 0.05f * magicEffect->mData.mBaseCost;
            if (effect.mData.mRange == ESM::RT_Target)
                x *= 1.5f;
            static const float fEffectCostMult = MWBase::Environment::get()
                                                     .getESMStore()
                                                     ->get<ESM::GameSetting>()
                                                     .find("fEffectCostMult")
                                                     ->mValue.getFloat();
            x *= fEffectCostMult;

            float s = 2.0f * actor.getClass().getSkill(actor, magicEffect->mData.mSchool);
            if (s - x < y)
            {
                y = s - x;
                if (effectiveSchool)
                    *effectiveSchool = magicEffect->mData.mSchool;
                lowestSkill = s;
            }
        }

        CreatureStats& stats = actor.getClass().getCreatureStats(actor);

        float actorWillpower = stats.getAttribute(ESM::Attribute::Willpower).getModified();
        float actorLuck = stats.getAttribute(ESM::Attribute::Luck).getModified();

        float castChance = (lowestSkill - calcSpellCost(*spell) + 0.2f * actorWillpower + 0.1f * actorLuck);

        return castChance;
    }

    float getSpellSuccessChance(
        const ESM::Spell* spell, const MWWorld::Ptr& actor, ESM::RefId* effectiveSchool, bool cap, bool checkMagicka)
    {
        // NB: Base chance is calculated here because the effective school pointer must be filled
        float baseChance = calcSpellBaseSuccessChance(spell, actor, effectiveSchool);

        bool godmode = actor == getPlayer() && MWBase::Environment::get().getWorld()->getGodModeState();

        CreatureStats& stats = actor.getClass().getCreatureStats(actor);

        if (spell->mData.mType == ESM::Spell::ST_Power)
            return stats.getSpells().canUsePower(spell) ? 100.f : 0.f;

        if (godmode)
            return 100.f;

        if (stats.getMagicEffects().getOrDefault(ESM::MagicEffect::Silence).getMagnitude())
            return 0.f;

        if (spell->mData.mType != ESM::Spell::ST_Spell)
            return 100.f;

        if (checkMagicka && calcSpellCost(*spell) > 0 && stats.getMagicka().getCurrent() < calcSpellCost(*spell))
            return 0.f;

        if (spell->mData.mFlags & ESM::Spell::F_Always)
            return 100.f;

        float castBonus = -stats.getMagicEffects().getOrDefault(ESM::MagicEffect::Sound).getMagnitude();
        float castChance = baseChance + castBonus;
        castChance *= stats.getFatigueTerm();

        if (cap)
            return std::clamp(castChance, 0.f, 100.f);

        return std::max(castChance, 0.f);
    }

    float getSpellSuccessChance(
        const ESM::RefId& spellId, const MWWorld::Ptr& actor, ESM::RefId* effectiveSchool, bool cap, bool checkMagicka)
    {
        if (const auto spell = MWBase::Environment::get().getESMStore()->get<ESM::Spell>().search(spellId))
            return getSpellSuccessChance(spell, actor, effectiveSchool, cap, checkMagicka);
        return 0.f;
    }

    ESM::RefId getSpellSchool(const ESM::RefId& spellId, const MWWorld::Ptr& actor)
    {
        ESM::RefId school;
        getSpellSuccessChance(spellId, actor, &school);
        return school;
    }

    ESM::RefId getSpellSchool(const ESM::Spell* spell, const MWWorld::Ptr& actor)
    {
        ESM::RefId school;
        getSpellSuccessChance(spell, actor, &school);
        return school;
    }

    bool spellIncreasesSkill(const ESM::Spell* spell)
    {
        return spell->mData.mType == ESM::Spell::ST_Spell && !(spell->mData.mFlags & ESM::Spell::F_Always);
    }

    bool spellIncreasesSkill(const ESM::RefId& spellId)
    {
        const auto spell = MWBase::Environment::get().getESMStore()->get<ESM::Spell>().search(spellId);
        return spell && spellIncreasesSkill(spell);
    }
}
