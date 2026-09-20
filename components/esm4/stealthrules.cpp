#include "stealthrules.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace ESM4
{
    void validatePickpocketSettings(const PickpocketSettings& settings)
    {
        for (float value : {settings.mActorSkillBase, settings.mActorSkillMultiplier, settings.mTargetSkillBase,
                 settings.mTargetSkillMultiplier, settings.mAmountBase, settings.mAmountMultiplier,
                 settings.mMinimumChance, settings.mMaximumChance})
            if (!std::isfinite(value))
                throw std::invalid_argument("nonfinite native pickpocket setting");
        if (settings.mMinimumChance < 0 || settings.mMaximumChance > 100
            || settings.mMinimumChance > settings.mMaximumChance)
            throw std::invalid_argument("invalid native pickpocket chance bounds");
    }

    float pickpocketAmount(std::int32_t itemValue, std::uint32_t count)
    {
        if (itemValue < 0)
            throw std::invalid_argument("invalid native pickpocket item value");
        const auto amount = std::uint64_t(itemValue) * count;
        if (amount > std::numeric_limits<std::int32_t>::max())
            throw std::invalid_argument("native pickpocket amount integer overflow");
        return static_cast<float>(amount);
    }

    std::int32_t pickpocketChance(std::int32_t actorSkill, std::int32_t targetSkill, float amount,
        const PickpocketSettings& settings)
    {
        validatePickpocketSettings(settings);
        if (!std::isfinite(amount) || amount < 0)
            throw std::invalid_argument("invalid native pickpocket amount");
        const auto stored = [](double value) {
            if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
                throw std::invalid_argument("native pickpocket arithmetic overflow");
            return static_cast<float>(value);
        };
        const float target = stored(double(targetSkill) * settings.mTargetSkillMultiplier + settings.mTargetSkillBase);
        const float actor = stored(double(actorSkill) * settings.mActorSkillMultiplier + settings.mActorSkillBase);
        const float value = stored(double(settings.mAmountMultiplier) * amount + settings.mAmountBase);
        // Original adds stored terms on the FPU and truncates without a final
        // float32 store. Rounding this sum first changes integer boundaries.
        const double total = double(target) + actor + value;
        if (total < std::numeric_limits<std::int32_t>::min() || total > std::numeric_limits<std::int32_t>::max())
            throw std::invalid_argument("native pickpocket chance integer overflow");
        const auto chance = static_cast<std::int32_t>(total);
        if (static_cast<float>(chance) < settings.mMinimumChance)
            return static_cast<std::int32_t>(settings.mMinimumChance);
        if (static_cast<float>(chance) > settings.mMaximumChance)
            return static_cast<std::int32_t>(settings.mMaximumChance);
        return chance;
    }

    bool pickpocketCheckSucceeds(PickpocketCheck check, std::int32_t chance, unsigned draw)
    {
        if (chance < 0 || chance > 100 || draw >= 100)
            throw std::invalid_argument("invalid native pickpocket percentile");
        switch (check)
        {
            case PickpocketCheck::Transfer: return static_cast<std::int32_t>(draw) < chance;
            case PickpocketCheck::UntouchedMenuExit: return static_cast<std::int32_t>(draw) <= chance;
        }
        throw std::invalid_argument("invalid native pickpocket check kind");
    }

    void validateSneakAttackSettings(const SneakAttackSettings& settings)
    {
        for (const auto* values : {&settings.mMeleeMultipliers, &settings.mMarksmanMultipliers})
            for (float value : *values)
                if (!std::isfinite(value) || value < 0)
                    throw std::invalid_argument("invalid native sneak attack multiplier");
    }

    SneakAttackResult sneakAttack(const SneakAttackInput& input, const SneakAttackSettings& settings,
        const CombatMasterySettings& mastery)
    {
        validateSneakAttackSettings(settings);
        const auto rank = combatMastery(input.mBaseSneak, mastery);
        if (input.mWeaponType < -1 || input.mWeaponType > 5)
            throw std::invalid_argument("invalid native sneak attack weapon type");
        if (!input.mNpcAttacker || !input.mSneaking || input.mSwimming || input.mVictimDetection > 0
            || (input.mVictimCombatTarget && input.mVictimDetection > settings.mCombatMinimumDetection))
            return {};
        const auto index = static_cast<std::size_t>(rank);
        float multiplier = 1.f;
        if (input.mWeaponType == -1 || input.mWeaponType == 0 || input.mWeaponType == 2)
            multiplier = settings.mMeleeMultipliers[index];
        else if (input.mWeaponType == 5)
            multiplier = settings.mMarksmanMultipliers[index];
        const bool bypass = rank == CombatMastery::Master && multiplier > 1.f;
        return {multiplier, bypass, bypass};
    }
}
