#include "physicalcombat.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ESM4
{
    namespace
    {
        void finite(float value)
        {
            if (!std::isfinite(value))
                throw std::invalid_argument("nonfinite native physical combat input");
        }
        void nonnegative(float value)
        {
            finite(value);
            if (value < 0)
                throw std::invalid_argument("negative native physical combat factor");
        }
        float rounded(double value)
        {
            if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
                throw std::invalid_argument("native physical combat arithmetic overflow");
            return static_cast<float>(value);
        }
        float skillValue(std::int32_t skill, std::int32_t luck, const PhysicalCombatSettings& settings)
        {
            const float luckTerm = rounded(double(luck) * settings.mLuckSkillMultiplier);
            const float adjustment = rounded(double(luckTerm) + settings.mLuckSkillBase);
            return rounded(std::clamp(double(skill) + adjustment, 0.0, 100.0));
        }
        float fatigueValue(float ratio, const PhysicalCombatSettings& settings)
        {
            finite(ratio);
            return rounded(settings.mFatigueBase - (1.0 - ratio) * settings.mFatigueMultiplier);
        }
    }

    bool actorWaterProbe(float positionZ, float height, float ratio, float waterLevel)
    {
        finite(positionZ);
        finite(height);
        finite(ratio);
        finite(waterLevel);
        const float offset = rounded(double(height) * ratio);
        return double(waterLevel) > double(positionZ) + offset;
    }

    bool actorNeedsAir(bool pureAquatic, bool deeplySubmerged, bool swimming)
    {
        return pureAquatic ? !deeplySubmerged : deeplySubmerged && swimming;
    }

    void validateSwimBreathSettings(const SwimBreathSettings& settings)
    {
        finite(settings.mBase);
        finite(settings.mEnduranceMultiplier);
        finite(settings.mDamageMultiplier);
    }

    float swimBreathMaximum(std::int32_t endurance, const SwimBreathSettings& settings)
    {
        validateSwimBreathSettings(settings);
        return rounded(double(endurance) * settings.mEnduranceMultiplier + settings.mBase);
    }

    float drowningDamage(std::int32_t baseHealth, float duration, const SwimBreathSettings& settings)
    {
        validateSwimBreathSettings(settings);
        nonnegative(duration);
        const float rate = rounded(double(baseHealth) * settings.mDamageMultiplier);
        const float damage = rounded(double(rate) * duration);
        return damage > 0.f ? damage : 0.f;
    }

    SwimBreathUpdate updateSwimBreath(float remaining, float maximum, float duration,
        std::int32_t waterBreathing)
    {
        finite(remaining);
        finite(maximum);
        nonnegative(duration);
        const float updated = rounded(double(remaining) + (waterBreathing != 0 ? duration : -double(duration)));
        if (updated < 0.f)
            return {0.f, true};
        return {std::min(updated, maximum), false};
    }

    bool requiresIncapacitation(float currentFatigue, bool paralyzed, bool essentialUnconscious)
    {
        finite(currentFatigue);
        return currentFatigue < 0 || paralyzed || essentialUnconscious;
    }

    namespace
    {
        // The original movement path can produce a transient NaN/Inf at zero
        // capacity. Its positive-cost test and expenditure cap consume these
        // before writing an actor value. Avoid an out-of-range float conversion.
        float movementRounded(double value)
        {
            if (std::isnan(value))
                return std::numeric_limits<float>::quiet_NaN();
            if (std::abs(value) > std::numeric_limits<float>::max())
                return std::copysign(std::numeric_limits<float>::infinity(), value);
            return static_cast<float>(value);
        }

        float movementEncumbrance(const MovementFatigueInput& input, const MovementFatigueSettings& settings)
        {
            finite(input.mCurrentFatigue);
            finite(input.mCurrentStrength);
            if (input.mCurrentEncumbrance < 0)
                throw std::invalid_argument("negative native inventory encumbrance");
            float capacity = movementRounded(double(input.mCurrentStrength) * settings.mStrengthCapacityMultiplier);
            // Native comparison preserves -0: unlike max(0, capacity), this
            // matters when positive weight divided by capacity becomes -Inf.
            if (capacity < 0)
                capacity = 0;
            if (capacity == 0)
                return input.mCurrentEncumbrance == 0 ? std::numeric_limits<float>::quiet_NaN()
                    : std::copysign(std::numeric_limits<float>::infinity(), capacity);
            return movementRounded(double(input.mCurrentEncumbrance) / capacity);
        }

        float movementDebit(float current, float cost)
        {
            // This comparison deliberately rejects an unordered (NaN) cost.
            return current > 0 && cost > 0 ? std::min(current, cost) : 0.f;
        }
    }

    void validateMovementFatigueSettings(const MovementFatigueSettings& settings)
    {
        for (float value : {settings.mStrengthCapacityMultiplier, settings.mRunBase, settings.mRunMultiplier,
                 settings.mJumpBase, settings.mJumpMultiplier, settings.mExpertJumpMultiplier})
            finite(value);
        for (float value : settings.mAthleticsMultipliers)
            finite(value);
    }

    float runningFatigueDebit(const MovementFatigueInput& input, float duration,
        const MovementFatigueSettings& settings, const CombatMasterySettings& mastery)
    {
        validateMovementFatigueSettings(settings);
        nonnegative(duration);
        const auto rank = combatMastery(input.mBaseSkill, mastery);
        const float ratio = movementEncumbrance(input, settings);
        const float rate = movementRounded(settings.mRunBase + double(ratio) * settings.mRunMultiplier);
        const float elapsedCost = movementRounded(double(rate) * duration);
        const float cost = movementRounded(double(elapsedCost) * settings.mAthleticsMultipliers[static_cast<int>(rank)]);
        return movementDebit(input.mCurrentFatigue, cost);
    }

    float jumpingFatigueDebit(const MovementFatigueInput& input,
        const MovementFatigueSettings& settings, const CombatMasterySettings& mastery)
    {
        validateMovementFatigueSettings(settings);
        const auto rank = combatMastery(input.mBaseSkill, mastery);
        const float ratio = movementEncumbrance(input, settings);
        float cost = movementRounded(settings.mJumpBase + double(ratio) * settings.mJumpMultiplier);
        if (rank >= CombatMastery::Expert)
            cost = movementRounded(double(cost) * settings.mExpertJumpMultiplier);
        return movementDebit(input.mCurrentFatigue, cost);
    }

    void validateFatigueRegenerationSettings(const FatigueRegenerationSettings& settings)
    {
        finite(settings.mBase);
        finite(settings.mEnduranceMultiplier);
    }

    float fatigueRegeneration(const FatigueRegenerationInput& input, const FatigueRegenerationSettings& settings)
    {
        validateFatigueRegenerationSettings(settings);
        const auto current = combatBaseValue(input.mCurrent);
        const auto base = combatBaseValue(input.mBase);
        finite(input.mMaximumModifier);
        nonnegative(input.mDuration);
        if (double(current) >= double(base) + input.mMaximumModifier)
            return 0;
        const float rate = rounded(settings.mBase + double(input.mEndurance) * settings.mEnduranceMultiplier);
        return std::max(0.f, rounded(double(rate) * input.mDuration));
    }

    float healthRestoration(float current, std::int32_t base, float maximumModifier)
    {
        finite(current);
        finite(maximumModifier);
        const float maximum = rounded(double(base) + maximumModifier);
        return maximum > current ? rounded(double(maximum) - current) : 0.f;
    }

    void validateMagickaRegenerationSettings(const MagickaRegenerationSettings& settings)
    {
        finite(settings.mBase);
        finite(settings.mWillpowerMultiplier);
    }

    float magickaRegeneration(const MagickaRegenerationInput& input, const MagickaRegenerationSettings& settings)
    {
        validateMagickaRegenerationSettings(settings);
        const auto current = combatBaseValue(input.mCurrent);
        finite(input.mMaximumModifier);
        nonnegative(input.mDuration);
        const float maximum = rounded(double(input.mBase) + input.mMaximumModifier);
        if ((input.mCheckActiveMagicItem && input.mHasActiveMagicItem)
            || input.mStuntedMagicka > 0 || maximum <= static_cast<float>(current))
            return 0;
        const float rate = rounded((settings.mBase + double(input.mWillpower) * settings.mWillpowerMultiplier)
            * (double(maximum) / 100.0));
        return std::max(0.f, rounded(double(rate) * input.mDuration));
    }

    CombatConeResult combatHitCone(float facingRadians, float bearingRadians, float coneDegrees)
    {
        finite(facingRadians);
        finite(bearingRadians);
        nonnegative(coneDegrees);
        const float difference = std::abs(rounded(double(facingRadians) - bearingRadians));
        // Original conversion constant, not an independently recomputed 180/pi.
        float degrees = rounded(double(difference) * 57.2957763671875);
        if (degrees > 180)
            degrees = std::abs(rounded(double(degrees) - 360));
        return {degrees, coneDegrees > degrees};
    }

    void validateMeleeReachSettings(const MeleeReachSettings& settings)
    {
        nonnegative(settings.mCombatDistance);
        nonnegative(settings.mHandMultiplier);
        nonnegative(settings.mGiantMultiplier);
    }

    float weaponMeleeReach(float weaponReach, float scale, const MeleeReachSettings& settings)
    {
        validateMeleeReachSettings(settings);
        nonnegative(weaponReach);
        nonnegative(scale);
        return rounded(double(rounded(double(settings.mCombatDistance) * weaponReach)) * scale);
    }

    float unarmedMeleeReach(float scale, const MeleeReachSettings& settings)
    {
        return weaponMeleeReach(settings.mHandMultiplier, scale, settings);
    }

    float creatureMeleeReach(std::uint8_t reach, std::uint8_t creatureType, float scale,
        const MeleeReachSettings& settings)
    {
        validateMeleeReachSettings(settings);
        nonnegative(scale);
        if (creatureType > 5)
            throw std::invalid_argument("invalid native creature reach type");
        const float base = creatureType == 5 ? rounded(double(reach) * settings.mGiantMultiplier) : reach;
        return rounded(double(base) * scale);
    }

    float meleeContactDistance(float referenceDistance, const MeleeDistanceActor& attacker,
        const MeleeDistanceActor& target, bool selectedTarget, float slopeDifference)
    {
        nonnegative(referenceDistance);
        nonnegative(slopeDifference);
        for (const auto* actor : {&attacker, &target})
        {
            for (float coordinate : actor->mPosition)
                finite(coordinate);
            finite(actor->mMinimumZ);
            finite(actor->mMaximumZ);
            nonnegative(actor->mMaximumY);
            nonnegative(actor->mScale);
            if (actor->mMinimumZ > actor->mMaximumZ)
                throw std::invalid_argument("inverted native melee bounds");
        }
        // Native invalid/cross-space distance sentinel bypasses bounds entirely.
        if (referenceDistance == std::numeric_limits<float>::max())
            return referenceDistance;
        float distance = referenceDistance;
        if (attacker.mIsActor && target.mIsActor)
        {
            const float dz = rounded(double(attacker.mPosition[2]) - target.mPosition[2]);
            if ((attacker.mFlying && target.mFlying)
                || (selectedTarget && std::abs(dz) >= slopeDifference))
            {
                const float aMin = rounded(double(attacker.mMinimumZ) + attacker.mPosition[2]);
                const float aMax = rounded(double(attacker.mMaximumZ) + attacker.mPosition[2]);
                const float tMin = rounded(double(target.mMinimumZ) + target.mPosition[2]);
                const float tMax = rounded(double(target.mMaximumZ) + target.mPosition[2]);
                if (aMax >= tMin && tMax >= aMin)
                {
                    const float dx = rounded(double(attacker.mPosition[0]) - target.mPosition[0]);
                    const float dy = rounded(double(attacker.mPosition[1]) - target.mPosition[1]);
                    const float squared = rounded(double(dx) * dx + double(dy) * dy);
                    distance = rounded(std::sqrt(double(squared)));
                }
            }
        }
        // The original stores each product as double and truncates the sum
        // once. Subtracting two float half-extents gives different boundaries.
        const double radius = double(attacker.mMaximumY) * attacker.mScale
            + double(target.mMaximumY) * target.mScale;
        if (!std::isfinite(radius) || radius >= 2147483648.0)
            throw std::invalid_argument("native melee radius outside supported int32 domain");
        return rounded(double(distance) - static_cast<std::int32_t>(radius));
    }

    void validateEssentialRecoverySettings(const EssentialRecoverySettings& settings)
    {
        nonnegative(settings.mDelay);
        nonnegative(settings.mHealthFraction);
    }

    EssentialRecoveryHealth essentialRecoveryHealth(std::int32_t baseHealth, float currentHealth,
        const EssentialRecoverySettings& settings)
    {
        validateEssentialRecoverySettings(settings);
        finite(currentHealth);
        const float target = rounded(double(static_cast<float>(baseHealth)) * settings.mHealthFraction);
        return {target, rounded(double(target) - currentHealth)};
    }

    EssentialRecoveryTick advanceEssentialRecovery(float remaining, float frameSeconds,
        bool essentialUnconscious, std::int8_t knockedState)
    {
        finite(remaining);
        nonnegative(frameSeconds);
        if (!essentialUnconscious || (knockedState != 1 && knockedState != 3))
            return {remaining, false};
        const float next = rounded(double(remaining) - frameSeconds);
        return {next, next <= 0.f};
    }

    std::int32_t creatureNaturalDamage(std::uint16_t baseDamage, float fatigueRatio,
        const PhysicalCombatSettings& settings)
    {
        validatePhysicalCombatSettings(settings);
        const float damage = rounded(double(baseDamage) * fatigueValue(fatigueRatio, settings));
        if (double(damage) < std::numeric_limits<std::int32_t>::min()
            || double(damage) > std::numeric_limits<std::int32_t>::max())
            throw std::invalid_argument("native creature damage integer overflow");
        return static_cast<std::int32_t>(damage);
    }

    void validateKnockdownSettings(const KnockdownSettings& settings)
    {
        for (float value : {settings.mAgilityBase, settings.mAgilityMultiplier,
                 settings.mDamageBase, settings.mDamageMultiplier, settings.mMaximumChance})
            finite(value);
        if (settings.mMaximumChance < 0 || settings.mMaximumChance > 1)
            throw std::invalid_argument("invalid native knockdown chance");
    }

    void validateKnockbackSettings(const KnockbackSettings& settings)
    {
        for (float value : {settings.mAgilityBase, settings.mAgilityMultiplier,
                 settings.mDamageBase, settings.mDamageMultiplier})
            finite(value);
        nonnegative(settings.mMaximumForce);
        nonnegative(settings.mDuration);
    }

    float damageKnockback(std::int32_t agility, std::int32_t luck, float fatigueRatio,
        std::int32_t damage, const KnockbackSettings& settings, const PhysicalCombatSettings& physical)
    {
        validateKnockbackSettings(settings);
        validatePhysicalCombatSettings(physical);
        const float fatigue = fatigueValue(fatigueRatio, physical);
        if (fatigue == 0)
            throw std::invalid_argument("singular native knockback fatigue factor");
        const float adjusted = rounded(double(skillValue(agility, luck, physical)) / fatigue);
        const float damageFactor = rounded(double(damage) * settings.mDamageMultiplier + settings.mDamageBase);
        const float agilityFactor = rounded(double(settings.mAgilityMultiplier) * adjusted + settings.mAgilityBase);
        return std::min(rounded(double(damageFactor) * agilityFactor), settings.mMaximumForce);
    }

    std::int32_t combatBaseValue(float value)
    {
        finite(value);
        const double result = std::floor(double(value));
        if (result < std::numeric_limits<std::int32_t>::min() || result > std::numeric_limits<std::int32_t>::max())
            throw std::invalid_argument("native combat base value integer overflow");
        return static_cast<std::int32_t>(result);
    }

    float combatFatigueRatio(float current, std::int32_t base)
    {
        finite(current);
        return base == 0 ? 1.f : rounded(double(current) / static_cast<float>(base));
    }

    void validateBlockCostSettings(const BlockCostSettings& settings)
    {
        for (float value : {settings.mBase, settings.mMultiplier, settings.mSkillBase, settings.mSkillMultiplier})
            finite(value);
    }

    BlockContactCosts blockContactCosts(std::int32_t baseBlock, std::int32_t currentBlock,
        float damage, float absorbedFraction, bool hasBlockingItem,
        const BlockCostSettings& settings, const CombatMasterySettings& mastery)
    {
        validateBlockCostSettings(settings);
        nonnegative(damage);
        nonnegative(absorbedFraction);
        if (absorbedFraction > 1)
            throw std::invalid_argument("invalid native block absorbed fraction");
        const auto rank = combatMastery(baseBlock, mastery);
        BlockContactCosts result{};
        if (rank == CombatMastery::Novice)
        {
            const float skill = rounded(double(currentBlock) * settings.mSkillMultiplier + settings.mSkillBase);
            const float block = rounded(double(settings.mMultiplier) * absorbedFraction + settings.mBase);
            result.mFatigueDebit = rounded(double(skill) + block);
        }
        if (rank <= CombatMastery::Apprentice && hasBlockingItem)
            result.mBlockingItemWear = rounded(double(damage) * absorbedFraction);
        return result;
    }

    bool damageKnockdown(std::int32_t agility, std::int32_t luck, float fatigueRatio,
        std::int32_t damage, unsigned draw, const KnockdownSettings& settings,
        const PhysicalCombatSettings& physical)
    {
        validateKnockdownSettings(settings);
        validatePhysicalCombatSettings(physical);
        if (draw >= 100)
            throw std::invalid_argument("invalid native knockdown percentile");
        const float adjusted = rounded(double(skillValue(agility, luck, physical)) * fatigueValue(fatigueRatio, physical));
        const float numerator = rounded(double(damage) * settings.mDamageMultiplier + settings.mDamageBase);
        const float denominator = rounded(double(settings.mAgilityMultiplier) * adjusted + settings.mAgilityBase);
        float threshold = settings.mMaximumChance;
        if (denominator == 0)
        {
            // Original unordered 0/0 comparison fails; positive infinity is
            // capped, negative infinity cannot beat any nonnegative draw.
            if (numerator == 0 || std::signbit(numerator) != std::signbit(denominator))
                return false;
        }
        else
        {
            const double ratio = double(numerator) / denominator;
            if (ratio < -std::numeric_limits<float>::max())
                return false;
            if (ratio <= std::numeric_limits<float>::max())
                threshold = std::min(rounded(ratio), settings.mMaximumChance);
        }
        // Original divides the integer remainder by double 100 and uses <=.
        return double(draw) / 100.0 <= threshold;
    }

    void validateArmorWearSelectionSettings(const ArmorWearSelectionSettings& settings)
    {
        for (int value : {settings.mHeadChance, settings.mUpperBodyChance, settings.mLowerBodyChance,
                 settings.mHandsChance, settings.mFeetChance})
            if (value < 0 || value > 100)
                throw std::invalid_argument("invalid native armor wear selection percentage");
    }

    std::optional<ArmorWearSlot> selectArmorWearSlot(unsigned draw,
        const std::array<bool, 7>& available, const ArmorWearSelectionSettings& settings)
    {
        validateArmorWearSelectionSettings(settings);
        if (draw >= 100)
            throw std::invalid_argument("invalid native armor wear selection draw");
        const auto has = [&](ArmorWearSlot slot) { return available[static_cast<unsigned>(slot)]; };
        unsigned threshold = settings.mHeadChance;
        if (draw < threshold)
        {
            if (has(ArmorWearSlot::Head))
                return ArmorWearSlot::Head;
            if (has(ArmorWearSlot::Hair))
                return ArmorWearSlot::Hair;
        }
        for (const auto& [chance, slot] : {std::pair{settings.mUpperBodyChance, ArmorWearSlot::UpperBody},
                 {settings.mLowerBodyChance, ArmorWearSlot::LowerBody}, {settings.mHandsChance, ArmorWearSlot::Hands}})
        {
            threshold += chance;
            if (draw < threshold && has(slot))
                return slot;
        }
        threshold += settings.mFeetChance;
        if (draw < threshold)
            return has(ArmorWearSlot::Feet) ? std::optional{ArmorWearSlot::Feet} : std::nullopt;
        return has(ArmorWearSlot::Shield) ? std::optional{ArmorWearSlot::Shield} : std::nullopt;
    }

    void validateArmorWearMasterySettings(const ArmorWearMasterySettings& settings)
    {
        for (float value : {settings.mLightNoviceMultiplier, settings.mHeavyNoviceMultiplier,
                 settings.mLightJourneymanMultiplier, settings.mHeavyJourneymanMultiplier})
            nonnegative(value);
    }

    float armorWearMasteryMultiplier(std::int32_t skill, ArmorWeight weight,
        const ArmorWearMasterySettings& settings, const CombatMasterySettings& mastery)
    {
        validateArmorWearMasterySettings(settings);
        const auto rank = combatMastery(skill, mastery);
        switch (weight)
        {
            case ArmorWeight::Light:
                return rank >= CombatMastery::Journeyman ? settings.mLightJourneymanMultiplier
                    : rank == CombatMastery::Novice ? settings.mLightNoviceMultiplier : 1.f;
            case ArmorWeight::Heavy:
                return rank >= CombatMastery::Journeyman ? settings.mHeavyJourneymanMultiplier
                    : rank == CombatMastery::Novice ? settings.mHeavyNoviceMultiplier : 1.f;
        }
        throw std::invalid_argument("invalid native armor weight class");
    }

    float conditionAfterWear(float current, float wear)
    {
        nonnegative(current);
        nonnegative(wear);
        if (wear == 0.f)
            return current;
        const float remaining = rounded(double(current) - wear);
        return remaining < 1.f ? 0.f : remaining;
    }

    ArmorMitigation mitigateArmor(float damage, float rating, float maximumFraction, bool bypass)
    {
        nonnegative(damage);
        nonnegative(rating);
        nonnegative(maximumFraction);
        const float fraction = bypass ? 0.f : std::min(rounded(std::min(double(rating), 100.0) / 100.0), maximumFraction);
        return {rounded(double(damage) * (1.0 - fraction)), fraction};
    }

    void validateDurabilitySettings(const DurabilitySettings& settings)
    {
        nonnegative(settings.mWeaponDamageMultiplier);
        nonnegative(settings.mArmorDamageMultiplier);
    }

    float weaponWear(std::uint16_t baseDamage, const DurabilitySettings& settings)
    {
        validateDurabilitySettings(settings);
        return rounded(double(baseDamage) * settings.mWeaponDamageMultiplier);
    }

    float armorWear(float incomingDamage, float absorbedFraction, const DurabilitySettings& settings)
    {
        validateDurabilitySettings(settings);
        nonnegative(incomingDamage);
        nonnegative(absorbedFraction);
        if (absorbedFraction > 1.f)
            throw std::invalid_argument("invalid native absorbed damage fraction");
        return rounded(double(incomingDamage) * absorbedFraction * settings.mArmorDamageMultiplier);
    }

    void validateCombatMasterySettings(const CombatMasterySettings& settings)
    {
        if (settings.mMinimumSkill.front() < 0
            || !std::is_sorted(settings.mMinimumSkill.begin(), settings.mMinimumSkill.end()))
            throw std::invalid_argument("invalid native combat mastery thresholds");
    }
    void validatePowerAttackSettings(const PowerAttackSettings& settings)
    {
        for (const float value : {settings.mBaseMultiplier, settings.mStandingMultiplier,
                 settings.mSideMultiplier, settings.mBackwardMultiplier, settings.mForwardMultiplier})
            nonnegative(value);
    }
    CombatMastery combatMastery(std::int32_t skill, const CombatMasterySettings& settings)
    {
        validateCombatMasterySettings(settings);
        return static_cast<CombatMastery>(std::upper_bound(settings.mMinimumSkill.begin(),
            settings.mMinimumSkill.end(), skill) - settings.mMinimumSkill.begin());
    }
    float powerAttackMultiplier(std::int32_t skill, PowerAttackDirection direction,
        const PowerAttackSettings& settings, const CombatMasterySettings& mastery)
    {
        validatePowerAttackSettings(settings);
        const auto rank = combatMastery(skill, mastery);
        switch (direction)
        {
            case PowerAttackDirection::Standing:
                return rank >= CombatMastery::Apprentice ? settings.mStandingMultiplier : settings.mBaseMultiplier;
            case PowerAttackDirection::Left:
            case PowerAttackDirection::Right:
                return rank >= CombatMastery::Journeyman ? settings.mSideMultiplier : settings.mBaseMultiplier;
            case PowerAttackDirection::Backward:
                return rank >= CombatMastery::Expert ? settings.mBackwardMultiplier : settings.mBaseMultiplier;
            case PowerAttackDirection::Forward:
                return rank >= CombatMastery::Master ? settings.mForwardMultiplier : settings.mBaseMultiplier;
        }
        throw std::invalid_argument("invalid native power attack direction");
    }

    void validatePhysicalCombatSettings(const PhysicalCombatSettings& settings)
    {
        for (const float value : { settings.mLuckSkillMultiplier, settings.mFatigueBase,
                 settings.mFatigueMultiplier, settings.mWeaponMultiplier, settings.mSkillBase,
                 settings.mSkillMultiplier, settings.mConditionBase, settings.mConditionMultiplier,
                 settings.mAttributeBase, settings.mAttributeMultiplier })
            nonnegative(value);
    }

    void validateAttackFatigueSettings(const AttackFatigueSettings& settings)
    {
        nonnegative(settings.mBase);
        nonnegative(settings.mWeightMultiplier);
        nonnegative(settings.mPowerMultiplier);
    }

    float attackFatigueCost(float weaponWeight, bool powerAttack, const AttackFatigueSettings& settings)
    {
        validateAttackFatigueSettings(settings);
        nonnegative(weaponWeight);
        const float cost = rounded(settings.mBase + double(weaponWeight) * settings.mWeightMultiplier);
        return powerAttack ? rounded(double(cost) * settings.mPowerMultiplier) : cost;
    }

    float effectiveCombatSkill(std::int32_t skill, std::int32_t luck, const PhysicalCombatSettings& settings)
    {
        validatePhysicalCombatSettings(settings);
        return skillValue(skill, luck, settings);
    }

    float combatFatigueMultiplier(float ratio, const PhysicalCombatSettings& settings)
    {
        validatePhysicalCombatSettings(settings);
        return fatigueValue(ratio, settings);
    }

    float weaponDamage(const WeaponDamageInput& input, const PhysicalCombatSettings& settings)
    {
        validatePhysicalCombatSettings(settings);
        nonnegative(input.mConditionRatio);
        finite(input.mFatigueRatio);
        nonnegative(input.mAttackMultiplier);
        if (input.mAttribute < 0)
            throw std::invalid_argument("negative governing combat attribute");
        // Original x87 routines explicitly store each of these terms as float.
        // Use wider intermediates and those same rounding boundaries; do not
        // round each individual multiplication to float or truncate to integer.
        constexpr double percent = static_cast<double>(0.01f);
        const float base = rounded(double(input.mBaseDamage) * settings.mWeaponMultiplier);
        const float condition = rounded(settings.mConditionBase
            + double(input.mConditionRatio) * settings.mConditionMultiplier);
        const float skill = rounded(settings.mSkillBase
            + skillValue(input.mSkill, input.mLuck, settings) * percent * settings.mSkillMultiplier);
        const float attribute = rounded(settings.mAttributeBase
            + std::min(input.mAttribute, 100) * percent * settings.mAttributeMultiplier);
        const float fatigue = input.mIgnoreFatigue ? 1.f : fatigueValue(input.mFatigueRatio, settings);
        const float subtotal = rounded(double(condition) * base * skill * attribute * fatigue);
        return rounded(double(subtotal) * input.mAttackMultiplier);
    }
    void validateHandToHandSettings(const HandToHandSettings& settings)
    {
        for (float value : { settings.mSkillBase, settings.mSkillMultiplier, settings.mStrengthBase,
                 settings.mStrengthMultiplier, settings.mHealthMinimum, settings.mHealthMaximum,
                 settings.mFatigueBase, settings.mFatigueMultiplier })
            nonnegative(value);
        if (settings.mHealthMinimum > settings.mHealthMaximum)
            throw std::invalid_argument("reversed native hand damage range");
    }

    BlockContactDisposition blockContactDisposition(const BlockContactInput& input)
    {
        switch (input.mEquipment)
        {
            case BlockEquipment::Shield:
            case BlockEquipment::Weapon:
            case BlockEquipment::Unarmed: break;
            default: throw std::invalid_argument("invalid native block equipment");
        }
        if (!input.mBlocking || input.mParalyzed || input.mBypassBlock || !input.mInsideCone)
            return BlockContactDisposition::None;
        if (input.mEquipment == BlockEquipment::Unarmed && (input.mWeaponAttack || input.mProjectile))
            return BlockContactDisposition::ReactionOnly;
        return BlockContactDisposition::Absorb;
    }

    void validateBlockSettings(const BlockSettings& settings)
    {
        for (float value : { settings.mSkillBase, settings.mSkillMultiplier, settings.mMaximum,
                 settings.mWeaponMultiplier, settings.mUnarmedMultiplier })
            nonnegative(value);
        if (settings.mMaximum > 1.f)
            throw std::invalid_argument("native maximum block fraction exceeds one");
    }

    HandToHandDamage handToHandDamage(const HandToHandInput& input, const HandToHandSettings& settings,
        const PhysicalCombatSettings& physical)
    {
        validatePhysicalCombatSettings(physical);
        validateHandToHandSettings(settings);
        if (input.mStrength < 0)
            throw std::invalid_argument("negative native hand damage strength");
        constexpr double percent = static_cast<double>(0.01f);
        const float skill = rounded(settings.mSkillBase
            + skillValue(input.mSkill, input.mLuck, physical) * percent * settings.mSkillMultiplier);
        const float strength = rounded(settings.mStrengthBase
            + std::min(input.mStrength, 100) * percent * settings.mStrengthMultiplier);
        const float factor = std::min(1.f, rounded(double(strength) * skill * fatigueValue(input.mFatigueRatio, physical)));
        HandToHandDamage result;
        result.mHealth = rounded(settings.mHealthMinimum
            + (double(settings.mHealthMaximum) - settings.mHealthMinimum) * factor);
        result.mFatigue = input.mSuppressFatigueDamage ? 0.f
            : rounded(double(result.mHealth) * settings.mFatigueMultiplier + settings.mFatigueBase);
        return result;
    }

    HandToHandDamage handToHandContactDamage(const HandToHandContactInput& input,
        const HandToHandSettings& settings, const PhysicalCombatSettings& physical)
    {
        const float ratio = combatFatigueRatio(input.mCurrentFatigue, input.mBaseFatigue);
        return handToHandDamage({input.mSkill, input.mLuck, input.mStrength, ratio,
            input.mVictimKnockedState && *input.mVictimKnockedState != 0}, settings, physical);
    }

    float blockFraction(const BlockInput& input, const BlockSettings& settings, const PhysicalCombatSettings& physical)
    {
        validatePhysicalCombatSettings(physical);
        validateBlockSettings(settings);
        float equipment;
        switch (input.mEquipment)
        {
            case BlockEquipment::Shield: equipment = 1.f; break;
            case BlockEquipment::Weapon: equipment = settings.mWeaponMultiplier; break;
            case BlockEquipment::Unarmed: equipment = settings.mUnarmedMultiplier; break;
            default: throw std::invalid_argument("invalid native block equipment");
        }
        constexpr double percent = static_cast<double>(0.01f);
        const float skill = rounded(settings.mSkillBase
            + skillValue(input.mSkill, input.mLuck, physical) * percent * settings.mSkillMultiplier);
        return std::min(settings.mMaximum,
            rounded(double(fatigueValue(input.mFatigueRatio, physical)) * skill * equipment));
    }

    float difficultyDamage(float damage, float difficulty, float multiplier, PlayerDamageRole role)
    {
        nonnegative(damage);
        finite(difficulty);
        nonnegative(multiplier);
        if (difficulty < -1.f || difficulty > 1.f)
            throw std::invalid_argument("native difficulty outside normalized slider domain");
        if (role != PlayerDamageRole::Unaffected && role != PlayerDamageRole::Victim
            && role != PlayerDamageRole::Attacker)
            throw std::invalid_argument("invalid player damage role");
        if (role == PlayerDamageRole::Unaffected || difficulty == 0.f)
            return damage;
        const float scaled = rounded(double(difficulty) * multiplier);
        const float factor = difficulty < 0.f ? rounded(1.0 / rounded(1.0 - scaled)) : rounded(1.0 + scaled);
        return role == PlayerDamageRole::Victim ? rounded(double(damage) * factor) : rounded(double(damage) / factor);
    }
    void validateArmorMasterySettings(const ArmorMasterySettings& settings)
    {
        for (auto weight : settings.mCoverage)
            if (weight < 0)
                throw std::invalid_argument("negative native armor coverage weight");
        if (settings.mLightMasterMinimum < 0)
            throw std::invalid_argument("negative native armor mastery minimum");
        for (float factor : {settings.mLightMasterRatingMultiplier, settings.mHeavyExpertWeightMultiplier,
                 settings.mHeavyMasterWeightMultiplier, settings.mLightExpertWeightMultiplier})
            nonnegative(factor);
    }

    std::int32_t armorCoverage(const std::array<bool, 7>& matchingSlots, const ArmorMasterySettings& settings)
    {
        validateArmorMasterySettings(settings);
        std::int64_t sum = 0;
        for (std::size_t i = 0; i < matchingSlots.size(); ++i)
            if (matchingSlots[i])
                sum += settings.mCoverage[i];
        return static_cast<std::int32_t>(std::min<std::int64_t>(sum, 100));
    }

    float masteryArmorRating(float itemRating, float otherRating, std::int32_t baseLightArmor,
        std::int32_t lightCoverage, std::int32_t heavyCoverage, float maximum,
        const ArmorMasterySettings& settings, const CombatMasterySettings& mastery)
    {
        validateArmorMasterySettings(settings);
        nonnegative(itemRating);
        nonnegative(otherRating);
        nonnegative(maximum);
        for (auto coverage : {lightCoverage, heavyCoverage})
            if (coverage < 0 || coverage > 100)
                throw std::invalid_argument("invalid native armor coverage sum");
        const auto rank = combatMastery(baseLightArmor, mastery);
        if (rank == CombatMastery::Master && heavyCoverage == 0 && lightCoverage >= settings.mLightMasterMinimum)
            itemRating = rounded(double(itemRating) * settings.mLightMasterRatingMultiplier);
        return capArmorRating(rounded(double(itemRating) + otherRating), maximum);
    }

    float wornArmorWeight(float weight, bool heavy, std::int32_t baseSkill, bool worn,
        const ArmorMasterySettings& settings, const CombatMasterySettings& mastery)
    {
        validateArmorMasterySettings(settings);
        nonnegative(weight);
        const auto rank = combatMastery(baseSkill, mastery);
        if (!worn || rank < CombatMastery::Expert)
            return weight;
        const float multiplier = !heavy ? settings.mLightExpertWeightMultiplier
            : rank == CombatMastery::Expert ? settings.mHeavyExpertWeightMultiplier
                                           : settings.mHeavyMasterWeightMultiplier;
        return rounded(double(weight) * multiplier);
    }

    void validateArmorRatingSettings(const ArmorRatingSettings& settings)
    {
        for (float value : { settings.mSkillBase, settings.mSkillMaximum,
                 settings.mConditionBase, settings.mConditionMultiplier })
            nonnegative(value);
        if (settings.mSkillBase > settings.mSkillMaximum)
            throw std::invalid_argument("reversed native armor skill range");
    }

    float armorRating(const ArmorRatingInput& input, const ArmorRatingSettings& settings,
        const PhysicalCombatSettings& physical)
    {
        validateArmorRatingSettings(settings);
        validatePhysicalCombatSettings(physical);
        nonnegative(input.mConditionRatio);
        // The native caller converts hundredths to integral armor units first.
        const auto base = input.mBaseHundredths / 100;
        const float range = rounded(double(settings.mSkillMaximum) - settings.mSkillBase);
        const float skill = skillValue(input.mSkill, input.mLuck, physical);
        const float scaled = rounded((settings.mSkillBase + double(skill) / 100.0 * range) * base);
        const float floored = std::max(1.f, std::floor(scaled));
        const float condition = rounded(settings.mConditionBase
            + double(input.mConditionRatio) * settings.mConditionMultiplier);
        return rounded(double(floored) * condition);
    }

    float capArmorRating(float total, float maximum)
    {
        nonnegative(total);
        nonnegative(maximum);
        return maximum == 0.f ? total : std::min(total, maximum);
    }
}
