#include "actorstats.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ESM4
{
    float addActorValueModifier(float current, float delta, bool allowPositive)
    {
        if (!std::isfinite(current) || !std::isfinite(delta))
            throw std::invalid_argument("nonfinite native actor-value modifier");
        const float result = static_cast<float>(double(current) + double(delta));
        if (!std::isfinite(result))
            throw std::invalid_argument("native actor-value modifier overflow");
        return !allowPositive && result > 0.f ? 0.f : result;
    }

    void validateCreatureBaseStatsSettings(const CreatureBaseStatsSettings& settings)
    {
        for (float value : {settings.mCombatMultiplier, settings.mMagicMultiplier,
                 settings.mStealthMultiplier, settings.mDamageMultiplier})
            if (!std::isfinite(value))
                throw std::invalid_argument("nonfinite native creature stat multiplier");
    }

    CreatureBaseStats calculateCreatureBaseStats(
        const CreatureBaseStatsInput& input, const CreatureBaseStatsSettings& settings)
    {
        validateCreatureBaseStatsSettings(settings);
        if (!input.mPlayerLevelOffset)
            return {input.mCombat, input.mMagic, input.mStealth, input.mHealth,
                input.mMagicka, input.mFatigue, input.mDamage};
        const int level = std::max<int>(1, input.mResolvedLevel);
        const auto addLevel = [level](unsigned base, float multiplier) {
            const double result = std::trunc(double(base) + level * double(multiplier));
            if (!std::isfinite(result) || result < std::numeric_limits<std::int32_t>::min()
                || result > std::numeric_limits<std::int32_t>::max())
                throw std::invalid_argument("native creature stat integer overflow");
            return static_cast<std::int32_t>(result);
        };
        const auto multiplyLevel = [level](std::uint16_t base) {
            return static_cast<std::uint16_t>(std::uint32_t(base) * level);
        };
        return {static_cast<std::uint8_t>(addLevel(input.mCombat, settings.mCombatMultiplier)),
            static_cast<std::uint8_t>(addLevel(input.mMagic, settings.mMagicMultiplier)),
            static_cast<std::uint8_t>(addLevel(input.mStealth, settings.mStealthMultiplier)),
            multiplyLevel(input.mHealth), multiplyLevel(input.mMagicka), multiplyLevel(input.mFatigue),
            static_cast<std::uint16_t>(addLevel(input.mDamage, settings.mDamageMultiplier))};
    }

    void validateNpcAutoStatsSettings(const NpcAutoStatsSettings& settings)
    {
        if (!std::isfinite(settings.mPrimaryAttributeBonus) || !std::isfinite(settings.mSecondaryAttributeBonus))
            throw std::invalid_argument("nonfinite native NPC attribute bonus");
    }

    NpcAutoStats calculateNpcAutoStats(const NpcAutoStatsInput& input, const NpcAutoStatsSettings& settings)
    {
        validateNpcAutoStatsSettings(settings);
        if (input.mLevel < 1 || input.mSpecialization > 2
            || std::ranges::any_of(input.mFavoredAttributes, [](auto value) { return value > 7; })
            || std::ranges::any_of(input.mMajorSkills, [](auto value) { return value < 12 || value > 32; })
            || std::ranges::any_of(input.mSkills,
                [](const auto& skill) { return skill.mGoverningAttribute > 7 || skill.mSpecialization > 2; })
            || std::ranges::any_of(input.mRaceBonuses,
                [](const auto& bonus) { return bonus.mSkill != -1 && (bonus.mSkill < 12 || bonus.mSkill > 32); }))
            throw std::invalid_argument("unsupported native NPC auto-calculation input");
        const auto rounded = [](double value) {
            if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
                throw std::invalid_argument("native NPC auto-calculation float overflow");
            return static_cast<float>(value);
        };
        const auto storeByte = [](float value) {
            const double capped = std::min(double(value), 100.0);
            const double lower = std::floor(capped);
            const double fraction = capped - lower;
            // Native FISTP uses nearest-even. Do not depend on the host fenv.
            const double result = lower + (fraction > .5 || (fraction == .5 && std::fmod(lower, 2) != 0));
            if (result < std::numeric_limits<std::int32_t>::min())
                throw std::invalid_argument("native NPC auto-calculation integer overflow");
            return static_cast<std::uint8_t>(static_cast<std::int32_t>(result));
        };
        const auto major = [&](std::size_t index) {
            return std::ranges::find(input.mMajorSkills, index + 12) != input.mMajorSkills.end();
        };
        const auto gained = input.mLevel - 1;
        NpcAutoStats result{};
        for (std::size_t attribute = 0; attribute < result.mAttributes.size(); ++attribute)
        {
            if (attribute == 6)
            {
                result.mAttributes[attribute] = input.mAuthoredPersonality;
                continue;
            }
            float value = input.mRaceAttributes[attribute];
            if (attribute == input.mFavoredAttributes[0])
                value = rounded(double(value) + settings.mPrimaryAttributeBonus);
            else if (attribute == input.mFavoredAttributes[1])
                value = rounded(double(value) + settings.mSecondaryAttributeBonus);
            for (std::size_t skill = 0; skill < input.mSkills.size(); ++skill)
                if (input.mSkills[skill].mGoverningAttribute == attribute)
                    value = rounded(double(value) + gained * (major(skill) ? 1.0 : double(.2f)));
            result.mAttributes[attribute] = storeByte(value);
        }
        for (std::size_t skill = 0; skill < result.mSkills.size(); ++skill)
        {
            float value = major(skill) ? rounded(gained + 25.0) : rounded(gained * double(.1f) + 5);
            if (input.mSkills[skill].mSpecialization == input.mSpecialization)
                value = rounded(double(rounded(double(value) + 5)) + gained * .5);
            for (const auto& bonus : input.mRaceBonuses)
                if (bonus.mSkill == static_cast<int>(skill + 12))
                    value = rounded(double(value) + bonus.mBonus);
            result.mSkills[skill] = storeByte(value);
        }
        return result;
    }

    void validateNpcDynamicStatsSettings(const NpcDynamicStatsSettings& settings)
    {
        if (!std::isfinite(settings.mAttributeHealthMultiplier) || settings.mAttributeHealthMultiplier < 0
            || settings.mPerLevelHealthMultiplier < 1 || settings.mLowLevelMaximum < 0
            || !std::isfinite(settings.mLowLevelHealthMultiplier) || settings.mLowLevelHealthMultiplier < 0
            || settings.mLowLevelHealthMultiplier > 1 || !std::isfinite(settings.mMagickaMultiplier)
            || settings.mMagickaMultiplier < 0)
            throw std::invalid_argument("unsupported native NPC dynamic-stat settings");
    }

    NpcDynamicStats calculateNpcDynamicStats(
        const NpcDynamicStatsInput& input, const NpcDynamicStatsSettings& settings)
    {
        validateNpcDynamicStatsSettings(settings);
        if (input.mLevel < 1 || input.mSpecialization > 2)
            throw std::invalid_argument("unsupported native NPC level or specialization");
        const auto checkedInteger = [](double value, double maximum) {
            if (!std::isfinite(value) || value < 0 || std::trunc(value) > maximum)
                throw std::invalid_argument("native NPC dynamic-stat arithmetic overflow");
            return static_cast<std::uint32_t>(value);
        };
        const auto attributeHealth = checkedInteger(
            double(input.mStrength + input.mEndurance) * settings.mAttributeHealthMultiplier,
            std::numeric_limits<std::int32_t>::max());
        const std::int64_t perLevel = std::int64_t(settings.mPerLevelHealthMultiplier)
            + input.mFavoredEndurance + (input.mSpecialization == 0 ? 1 : input.mSpecialization == 1 ? -1 : 0);
        const auto gained = input.mLevel - 1;
        const auto subtotal = attributeHealth + perLevel * gained;
        if (perLevel > std::numeric_limits<std::int32_t>::max()
            || subtotal > std::numeric_limits<std::int32_t>::max())
            throw std::invalid_argument("native NPC health integer arithmetic overflow");
        float factor = 1;
        if (gained < settings.mLowLevelMaximum)
        {
            const float ratio = static_cast<float>(double(gained) / settings.mLowLevelMaximum);
            const float scaled = static_cast<float>(double(ratio) * (1.0 - settings.mLowLevelHealthMultiplier));
            factor = static_cast<float>(double(settings.mLowLevelHealthMultiplier) + scaled);
        }
        const auto health = checkedInteger(double(subtotal) * factor, std::numeric_limits<std::int32_t>::max());
        const auto magicka = checkedInteger(double(input.mIntelligence) * settings.mMagickaMultiplier
                + input.mIntelligence, std::numeric_limits<std::uint16_t>::max());
        const auto fatigue = input.mStrength + input.mWillpower + input.mAgility + input.mEndurance;
        return {health, static_cast<std::uint16_t>(magicka), static_cast<std::uint16_t>(fatigue)};
    }

    std::int16_t resolveActorLevel(const ACBS_TES4& base, std::optional<std::uint16_t> playerLevel)
    {
        if (!(base.flags & 0x80))
            return base.levelOrOffset;
        const auto word = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(base.levelOrOffset) + playerLevel.value_or(0));
        const auto level = std::bit_cast<std::int16_t>(word);
        if (base.calcMin > 0 && level < base.calcMin)
            return std::bit_cast<std::int16_t>(base.calcMin);
        if (base.calcMax > 0 && level > base.calcMax)
            return std::bit_cast<std::int16_t>(base.calcMax);
        return std::max<std::int16_t>(level, 1);
    }
}
