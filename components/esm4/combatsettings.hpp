#ifndef OPENMW_ESM4_COMBATSETTINGS_H
#define OPENMW_ESM4_COMBATSETTINGS_H

#include "loadcsty.hpp"
#include "physicalcombat.hpp"
#include "projectilerules.hpp"
#include "masteryrules.hpp"
#include "stealthrules.hpp"
#include "detection.hpp"
#include "crimerules.hpp"
#include "combatairules.hpp"
#include "actorstats.hpp"
#include "actorvalues.hpp"
#include <span>

namespace ESM4
{
    struct GameSetting;

    struct CombatStyleDefaults
    {
        CombatStyleStandard mStandard;
        CombatStyleAdvanced mAdvanced;
    };

    // Input is the native winning setting inventory, never shared TES3 settings.
    // Known absent entries use independently verified original initializers.
    CombatStyleDefaults buildCombatStyleDefaults(std::span<const GameSetting* const> settings);
    float buildCombatHitConeAngle(std::span<const GameSetting* const> settings);
    MeleeReachSettings buildMeleeReachSettings(std::span<const GameSetting* const> settings);
    PhysicalCombatSettings buildPhysicalCombatSettings(std::span<const GameSetting* const> settings);
    ProjectileSettings buildProjectileSettings(std::span<const GameSetting* const> settings);
    ArrowLifetimeSettings buildArrowLifetimeSettings(std::span<const GameSetting* const> settings);
    ArrowRecoverySettings buildArrowRecoverySettings(std::span<const GameSetting* const> settings);
    ArrowCleanupSettings buildArrowCleanupSettings(std::span<const GameSetting* const> settings);
    BowFatigueSettings buildBowFatigueSettings(std::span<const GameSetting* const> settings);
    KnockdownSettings buildKnockdownSettings(std::span<const GameSetting* const> settings);
    KnockbackSettings buildKnockbackSettings(std::span<const GameSetting* const> settings);
    MasteryProcSettings buildMasteryProcSettings(std::span<const GameSetting* const> settings);
    EssentialRecoverySettings buildEssentialRecoverySettings(std::span<const GameSetting* const> settings);
    BlockCostSettings buildBlockCostSettings(std::span<const GameSetting* const> settings);
    AttackFatigueSettings buildAttackFatigueSettings(std::span<const GameSetting* const> settings);
    MovementFatigueSettings buildMovementFatigueSettings(std::span<const GameSetting* const> settings);
    FatigueRegenerationSettings buildFatigueRegenerationSettings(std::span<const GameSetting* const> settings);
    SwimBreathSettings buildSwimBreathSettings(std::span<const GameSetting* const> settings);
    MagickaRegenerationSettings buildMagickaRegenerationSettings(std::span<const GameSetting* const> settings);
    PlayerDynamicBaseSettings buildPlayerDynamicBaseSettings(std::span<const GameSetting* const> settings);
    NpcDynamicStatsSettings buildNpcDynamicStatsSettings(std::span<const GameSetting* const> settings);
    CreatureBaseStatsSettings buildCreatureBaseStatsSettings(std::span<const GameSetting* const> settings);
    // Native full FormId comparison used by the original Player base calculator.
    std::uint32_t buildCharacterGenerationClassId(std::span<const GameSetting* const> settings);
    NpcAutoStatsSettings buildNpcAutoStatsSettings(std::span<const GameSetting* const> settings);
    DurabilitySettings buildDurabilitySettings(std::span<const GameSetting* const> settings);
    ArmorWearMasterySettings buildArmorWearMasterySettings(std::span<const GameSetting* const> settings);
    ArmorWearSelectionSettings buildArmorWearSelectionSettings(std::span<const GameSetting* const> settings);
    CombatMasterySettings buildCombatMasterySettings(std::span<const GameSetting* const> settings);
    MeleeInputSettings buildMeleeInputSettings(std::span<const GameSetting* const> settings);
    PowerAttackSettings buildPowerAttackSettings(std::span<const GameSetting* const> settings);
    HandToHandSettings buildHandToHandSettings(std::span<const GameSetting* const> settings);
    BlockSettings buildBlockSettings(std::span<const GameSetting* const> settings);
    NativeDetectionSettings buildNativeDetectionSettings(std::span<const GameSetting* const> settings);
    CrimeInfamySettings buildCrimeInfamySettings(std::span<const GameSetting* const> settings);
    CrimeFineSettings buildCrimeFineSettings(std::span<const GameSetting* const> settings);
    CrimeReportingSettings buildCrimeReportingSettings(std::span<const GameSetting* const> settings);
    TrespassWarningSettings buildTrespassWarningSettings(std::span<const GameSetting* const> settings);
    FightScoreSettings buildFightScoreSettings(std::span<const GameSetting* const> settings);
    CrimeAlarmSettings buildCrimeAlarmSettings(std::span<const GameSetting* const> settings);
    JailSettings buildJailSettings(std::span<const GameSetting* const> settings);
    PickpocketSettings buildPickpocketSettings(std::span<const GameSetting* const> settings);
    SneakAttackSettings buildSneakAttackSettings(std::span<const GameSetting* const> settings);
    ArmorMasterySettings buildArmorMasterySettings(std::span<const GameSetting* const> settings);
    ArmorRatingSettings buildArmorRatingSettings(std::span<const GameSetting* const> settings);
    float buildMaximumArmorRating(std::span<const GameSetting* const> settings);
    float buildDifficultyDamageMultiplier(std::span<const GameSetting* const> settings);
}

#endif
