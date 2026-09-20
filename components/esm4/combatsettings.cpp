#include "combatsettings.hpp"
#include "combatstylepolicy.hpp"
#include "loadgmst.hpp"

#include <cmath>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>

namespace ESM4
{
    namespace
    {
        std::string key(std::string_view name)
        {
            std::string result(name);
            for (char& c : result)
                if (c >= 'A' && c <= 'Z')
                    c += 'a' - 'A';
            return result;
        }

        class Inputs
        {
            std::map<std::string, const GameSetting*, std::less<>> mValues;

        public:
            explicit Inputs(std::span<const GameSetting* const> settings)
            {
                for (const auto* setting : settings)
                {
                    if (!setting || setting->mEditorId.empty())
                        throw std::invalid_argument("native combat settings include a null or unnamed record");
                    if (!mValues.emplace(key(setting->mEditorId), setting).second)
                        throw std::invalid_argument("ambiguous winning native setting: " + setting->mEditorId);
                }
            }

            template <typename T> T number(std::string_view name, T fallback) const
            {
                const auto found = mValues.find(key(name));
                if (found == mValues.end())
                    return fallback;
                if (const T* value = std::get_if<T>(&found->second->mData))
                    return *value;
                throw std::invalid_argument("incorrect native combat setting type: " + std::string(name));
            }

            std::uint8_t chance(std::string_view name, std::int32_t fallback) const
            {
                const auto value = number(name, fallback);
                if (value < 0 || value > 100)
                    throw std::invalid_argument("native combat percentage outside domain: " + std::string(name));
                return static_cast<std::uint8_t>(value);
            }
        };
    }

    CombatStyleDefaults buildCombatStyleDefaults(std::span<const GameSetting* const> settings)
    {
        const Inputs inputs(settings);
        CombatStyleDefaults result;
        auto& standard = result.mStandard;
        auto& advanced = result.mAdvanced;
        // Verified original 1.2.0416 initializer facts, with winning native GMST
        // overrides applied first. See M15-COMBAT-STYLE-DEFAULTS.json for provenance.
        standard.mDodgeChance = inputs.chance("iAIDefaultDodgeChance", 75);
        standard.mLeftRightChance = inputs.chance("iAIDefaultDodgeLeftRightChance", 50);
        standard.mBlockChance = inputs.chance("iAIDefaultBlockChance", 30);
        standard.mAttackChance = inputs.chance("iAIDefaultAttackChance", 40);
        standard.mPowerAttackChance = inputs.chance("iAIDefaultPowerAttackChance", 25);
        standard.mAcrobaticDodgeChance = inputs.chance("iAIDefaultAcrobaticDodgeChance", 0);
        standard.mRushChance = inputs.chance("iAIDefaultRushingAttackPercentChance", 25);
        standard.mDodgeLeftRight = { inputs.number("fAIDefaultDodgeLeftRightMinTime", 0.5f),
            inputs.number("fAIDefaultDodgeLeftRightMaxTime", 1.5f) };
        standard.mDodgeForward = { inputs.number("fAIDefaultDodgeForwardMinTime", 0.5f),
            inputs.number("fAIDefaultDodgeForwardMaxTime", 1.f) };
        standard.mDodgeBack = { inputs.number("fAIDefaultDodgeBackwardMinTime", 0.25f),
            inputs.number("fAIDefaultDodgeBackwardMaxTime", 0.75f) };
        standard.mIdle = { inputs.number("fAIDefaultIdleMinTime", 0.5f),
            inputs.number("fAIDefaultIdleMaxTime", 1.5f) };
        standard.mHold = { inputs.number("fAIDefaultHoldMinTime", 0.5f),
            inputs.number("fAIDefaultHoldMaxTime", 1.5f) };
        standard.mAttackRecoilBonus = inputs.number("fAIDefaultAttackDuringRecoilStaggerBonus", 5.f);
        standard.mAttackUnconsciousBonus = inputs.number("fAIDefaultAttackDuringUnconsciousBonus", 5.f);
        standard.mAttackUnarmedBonus = inputs.number("fAIDefaultAttackHandBonus", 5.f);
        standard.mPowerAttackRecoilBonus = inputs.number("fAIDefaultPowerAttackRecoilStaggerBonus", 5.f);
        standard.mPowerAttackUnconsciousBonus = inputs.number("fAIDefaultPowerAttackUnconsciousBonus", 5.f);
        standard.mBuffStandoff = inputs.number("fAIDefaultBuffStandoffDistance", 325.f);
        standard.mRushDistanceMultiplier = inputs.number("fAIDefaultRushingAttackDistanceMult", 1.f);
        standard.mPowerAttackDirections[0] = inputs.chance("iAIDefaultPowerAttackNormalChance", 20);
        standard.mPowerAttackDirections[1] = inputs.chance("iAIDefaultPowerAttackForwardChance", 20);
        standard.mPowerAttackDirections[2] = inputs.chance("iAIDefaultPowerAttackBackwardChance", 20);
        standard.mPowerAttackDirections[3] = inputs.chance("iAIDefaultPowerAttackLeftChance", 20);
        standard.mPowerAttackDirections[4] = inputs.chance("iAIDefaultPowerAttackRightChance", 20);
        standard.mRangeMultipliers = std::array<float, 2>{ inputs.number("fAIDefaultOptimalRangeMult", 1.f),
            inputs.number("fAIDefaultMaximumRangeMult", 1.f) };
        standard.mSwitchDistances = std::array<float, 2>{ inputs.number("fAIDefaultSwitchToMeleeDistance", 250.f),
            inputs.number("fAIDefaultSwitchToRangedDistance", 1000.f) };
        standard.mRangedGroupStandoff = std::array<float, 2>{ inputs.number("fAIDefaultRangedStandoffDistance", 500.f),
            inputs.number("fAIDefaultGroupStandoffDistance", 325.f) };
        if (inputs.number("iAIDefaultIgnoreAlliesInArea", std::int32_t{ 0 }) != 0)
            standard.mFlags |= static_cast<std::uint8_t>(CombatStyleFlag::IgnoreAllies);
        if (inputs.number("iAIDefaultYieldEnabled", std::int32_t{ 0 }) != 0)
            standard.mFlags |= static_cast<std::uint8_t>(CombatStyleFlag::WillYield);
        if (inputs.number("iAIDefaultRejectYield", std::int32_t{ 0 }) != 0)
            standard.mFlags |= static_cast<std::uint8_t>(CombatStyleFlag::RejectYields);
        if (inputs.number("iAIDefaultFleeDisabled", std::int32_t{ 0 }) != 0)
            standard.mFlags |= static_cast<std::uint8_t>(CombatStyleFlag::DisableFleeing);
        if (inputs.number("iAIDefaultPrefersRangedAttacks", std::int32_t{ 0 }) != 0)
            standard.mFlags |= static_cast<std::uint8_t>(CombatStyleFlag::PreferRanged);
        if (inputs.number("iAIDefaultMeleeAlertAllowed", std::int32_t{ 0 }) != 0)
            standard.mFlags |= static_cast<std::uint8_t>(CombatStyleFlag::MeleeAlert);
        standard.mDoNotAcquire = inputs.number("iAIDefaultDoNotAcquire", std::int32_t{ 0 }) != 0;
        advanced.mDodgeFatigueMultiplier = inputs.number("fAIDefaultDodgeFatigueMult", -20.f);
        advanced.mDodgeFatigueBase = inputs.number("fAIDefaultDodgeFatigueBase", 0.f);
        advanced.mEncumberedSpeedBase = inputs.number("fAIDefaultDodgeSpeedBase", -110.f);
        advanced.mEncumberedSpeedMultiplier = inputs.number("fAIDefaultDodgeSpeedMult", 1.f);
        advanced.mDodgeUnderAttack = inputs.number("fAIDefaultDodgeDuringAttackMult", 1.f);
        advanced.mDodgeNotUnderAttack = inputs.number("fAIDefaultDodgeNoAttackMult", 0.75f);
        advanced.mBackDodgeUnderAttack = inputs.number("fAIDefaultDodgeBackDuringAttackMult", 1.f);
        advanced.mBackDodgeNotUnderAttack = inputs.number("fAIDefaultDodgeBackNoAttackMult", 0.7f);
        advanced.mForwardDodgeAttacking = inputs.number("fAIDefaultDodgeForwardWhileAttackingMult", 1.f);
        advanced.mForwardDodgeNotAttacking = inputs.number("fAIDefaultDodgeForwardNotAttackingMult", 0.5f);
        advanced.mBlockSkillMultiplier = inputs.number("fAIDefaultBlockSkillMult", 20.f);
        advanced.mBlockSkillBase = inputs.number("fAIDefaultBlockSkillBase", 0.f);
        advanced.mBlockUnderAttack = inputs.number("fAIDefaultBlockDuringAttackMult", 2.f);
        advanced.mBlockNotUnderAttack = inputs.number("fAIDefaultBlockNoAttackMult", 1.f);
        advanced.mAttackSkillMultiplier = inputs.number("fAIDefaultAttackSkillMult", 20.f);
        advanced.mAttackSkillBase = inputs.number("fAIDefaultAttackSkillBase", 0.f);
        advanced.mAttackUnderAttack = inputs.number("fAIDefaultAttackDuringAttackMult", 0.75f);
        advanced.mAttackNotUnderAttack = inputs.number("fAIDefaultAttackNoAttackMult", 1.f);
        advanced.mAttackDuringBlock = inputs.number("fAIDefaultAttackDuringBlockMult", 0.5f);
        advanced.mPowerAttackFatigueBase = inputs.number("fAIDefaultPowerAttackFatigueBase", 5.f);
        advanced.mPowerAttackFatigueMultiplier = inputs.number("fAIDefaultPowerAttackFatigueMult", -10.f);
        // Validate the same finite/domain contracts as resolved authored policies.
        resolveCombatStyleStandard(nullptr, standard);
        resolveCombatStyleAdvanced(nullptr, advanced);
        return result;
    }
    PhysicalCombatSettings buildPhysicalCombatSettings(std::span<const GameSetting* const> settings)
    {
        const Inputs inputs(settings);
        PhysicalCombatSettings result{
            inputs.number("iActorLuckSkillBase", std::int32_t{-20}),
            inputs.number("fActorLuckSkillMult", .4f),
            inputs.number("fFatigueBase", 1.25f),
            inputs.number("fFatigueMult", .5f),
            inputs.number("fDamageWeaponMult", 1.f),
            inputs.number("fDamageSkillBase", .2f),
            inputs.number("fDamageSkillMult", 1.8f),
            inputs.number("fDamageWeaponConditionBase", 0.f),
            inputs.number("fDamageWeaponConditionMult", 1.f),
            inputs.number("fDamageStrengthBase", .5f),
            inputs.number("fDamageStrengthMult", 1.f),
        };
        validatePhysicalCombatSettings(result);
        return result;
    }

    AttackFatigueSettings buildAttackFatigueSettings(std::span<const GameSetting* const> settings)
    {
        const Inputs inputs(settings);
        AttackFatigueSettings result{
            inputs.number("fFatigueAttackWeaponBase", 8.f),
            inputs.number("fFatigueAttackWeaponMult", .1f),
            inputs.number("fPowerAttackFatiguePenalty", 5.f),
        };
        validateAttackFatigueSettings(result);
        return result;
    }

    CombatMasterySettings buildCombatMasterySettings(std::span<const GameSetting* const> settings)
    {
        const Inputs inputs(settings);
        CombatMasterySettings result{{
            inputs.number("iSkillApprenticeMin", std::int32_t{25}),
            inputs.number("iSkillJourneymanMin", std::int32_t{50}),
            inputs.number("iSkillExpertMin", std::int32_t{75}),
            inputs.number("iSkillMasterMin", std::int32_t{100}),
        }};
        validateCombatMasterySettings(result);
        return result;
    }
    PowerAttackSettings buildPowerAttackSettings(std::span<const GameSetting* const> settings)
    {
        const Inputs inputs(settings);
        PowerAttackSettings result{
            inputs.number("fDamagePowerAttackBonus", 3.f),
            inputs.number("fDamagePowerAttackStandBonus", 4.f),
            inputs.number("fDamagePowerAttackSideBonus", 3.f),
            inputs.number("fDamagePowerAttackBackBonus", 3.f),
            inputs.number("fDamagePowerAttackForwardBonus", 3.f),
        };
        validatePowerAttackSettings(result);
        return result;
    }

    HandToHandSettings buildHandToHandSettings(std::span<const GameSetting* const> settings)
    {
        const Inputs inputs(settings);
        HandToHandSettings result{
            inputs.number("fHandDamageSkillBase", 0.f),
            inputs.number("fHandDamageSkillMult", 1.f),
            inputs.number("fHandDamageStrengthBase", 0.f),
            inputs.number("fHandDamageStrengthMult", .75f),
            inputs.number("fHandHealthMin", 1.f),
            inputs.number("fHandHealthMax", 20.f),
            inputs.number("fHandFatigueDamageBase", 0.f),
            inputs.number("fHandFatigueDamageMult", .25f),
        };
        validateHandToHandSettings(result);
        return result;
    }

    BlockSettings buildBlockSettings(std::span<const GameSetting* const> settings)
    {
        const Inputs inputs(settings);
        BlockSettings result{
            inputs.number("fBlockSkillBase", 0.f),
            inputs.number("fBlockSkillMult", 1.f),
            inputs.number("fBlockMax", .75f),
            inputs.number("fBlockAmountWeaponMult", .5f),
            inputs.number("fBlockAmountHandToHandMult", .25f),
        };
        validateBlockSettings(result);
        return result;
    }

    float buildDifficultyDamageMultiplier(std::span<const GameSetting* const> settings)
    {
        const float result = Inputs(settings).number("fDifficultyDamageMultiplier", 10.f);
        if (!std::isfinite(result) || result < 0.f)
            throw std::invalid_argument("invalid native difficulty damage multiplier");
        return result;
    }
    ArmorRatingSettings buildArmorRatingSettings(std::span<const GameSetting* const> settings)
    {
        const Inputs inputs(settings);
        ArmorRatingSettings result{
            inputs.number("fArmorRatingBase", .35f),
            inputs.number("fArmorRatingMax", 1.f),
            inputs.number("fArmorRatingConditionBase", 0.f),
            inputs.number("fArmorRatingConditionMult", 1.f),
        };
        validateArmorRatingSettings(result);
        return result;
    }

    float buildMaximumArmorRating(std::span<const GameSetting* const> settings)
    {
        const float result = Inputs(settings).number("fMaxArmorRating", 90.f);
        if (!std::isfinite(result) || result < 0.f)
            throw std::invalid_argument("invalid native maximum armor rating");
        return result;
    }
    ProjectileSettings buildProjectileSettings(std::span<const GameSetting* const> settings)
    {
        const Inputs inputs(settings);
        ProjectileSettings result{
            inputs.number("fArrowBowTimerBase", .25f),
            inputs.number("fArrowBowTimerMult", .4f),
            inputs.number("fArrowSpeedMult", 1500.f),
            inputs.number("fArrowWeakSpeed", .01f),
            inputs.number("fArrowGravityBase", .3f),
            inputs.number("fArrowGravityMult", .002f),
            inputs.number("fArrowWeakGravity", 1.75f),
        };
        validateProjectileSettings(result);
        return result;
    }

    BowFatigueSettings buildBowFatigueSettings(std::span<const GameSetting* const> settings)
    {
        const Inputs inputs(settings);
        const BowFatigueSettings result{
            inputs.number("fMarksmanFatigueBurnPerSecond", 15.f),
            inputs.number("fMarksmanFatigueBurnPerShot", 5.f),
        };
        validateBowFatigueSettings(result);
        return result;
    }

    KnockdownSettings buildKnockdownSettings(std::span<const GameSetting* const> settings)
    {
        const Inputs inputs(settings);
        const KnockdownSettings result{
            inputs.number("fKnockdownAgilBase", 0.f),
            inputs.number("fKnockdownAgilMult", 1.f),
            inputs.number("fKnockdownDamageBase", 0.f),
            inputs.number("fKnockdownDamageMult", -3.f),
            inputs.number("fKnockdownChance", .25f),
        };
        validateKnockdownSettings(result);
        return result;
    }

    KnockbackSettings buildKnockbackSettings(std::span<const GameSetting* const> settings)
    {
        const Inputs inputs(settings);
        const KnockbackSettings result{
            inputs.number("fKnockbackAgilBase", 1.f),
            inputs.number("fKnockbackAgilMult", -.008f),
            inputs.number("fKnockbackDamageBase", 50.f),
            inputs.number("fKnockbackDamageMult", 10.f),
            inputs.number("fKnockbackForceMax", 512.f),
            inputs.number("fKnockbackTime", 1.f),
        };
        validateKnockbackSettings(result);
        return result;
    }

    MasteryProcSettings buildMasteryProcSettings(std::span<const GameSetting* const> settings)
    {
        const Inputs inputs(settings);
        const MasteryProcSettings result{
            inputs.number("iPerkAttackDisarmChance", std::int32_t{5}),
            inputs.number("iPerkBlockDisarmChance", std::int32_t{5}),
            inputs.number("iPerkBlockStaggerChance", std::int32_t{5}),
            inputs.number("iPerkMarksmanKnockdownChance", std::int32_t{5}),
            inputs.number("iPerkMarksmanParalyzeChance", std::int32_t{5}),
            inputs.number("iPerkHandToHandBlockRecoilChance", std::int32_t{25}),
        };
        validateMasteryProcSettings(result);
        return result;
    }

    ArmorWearSelectionSettings buildArmorWearSelectionSettings(std::span<const GameSetting* const> settings)
    {
        const Inputs inputs(settings);
        ArmorWearSelectionSettings result{
            inputs.number("iArmorDamageHelmChance", std::int32_t{10}),
            inputs.number("iArmorDamageCuirassChance", std::int32_t{25}),
            inputs.number("iArmorDamageGreavesChance", std::int32_t{15}),
            inputs.number("iArmorDamageGauntletsChance", std::int32_t{10}),
            inputs.number("iArmorDamageBootsChance", std::int32_t{10}),
        };
        validateArmorWearSelectionSettings(result);
        return result;
    }

    ArmorWearMasterySettings buildArmorWearMasterySettings(std::span<const GameSetting* const> settings)
    {
        const Inputs inputs(settings);
        ArmorWearMasterySettings result{
            inputs.number("fPerkLightArmorNoviceDamageMult", 1.5f),
            inputs.number("fPerkHeavyArmorNoviceDamageMult", 1.5f),
            inputs.number("fPerkLightArmorJourneymanDamageMult", .5f),
            inputs.number("fPerkHeavyArmorJourneymanDamageMult", .5f),
        };
        validateArmorWearMasterySettings(result);
        return result;
    }

    DurabilitySettings buildDurabilitySettings(std::span<const GameSetting* const> settings)
    {
        const Inputs inputs(settings);
        DurabilitySettings result{
            inputs.number("fDamageToWeaponPercentage", .01f),
            inputs.number("fDamageToArmorPercentage", .5f),
        };
        validateDurabilitySettings(result);
        return result;
    }
}
