/*
  Copyright (C) 2026 OpenMW contributors

  This file is part of OpenMW.

  OpenMW is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License version 3, or (at your option) any
  later version.
*/
#include "aiconditions.hpp"

#include <cmath>

namespace ESM4
{
    namespace
    {
        ConditionSubject subjectFor(const PackageCondition& condition)
        {
            if (condition.mRunOnTarget)
                return ConditionSubject::Target;
            if (!condition.mRunOnReferenceKey.isNull())
                return ConditionSubject::Reference;
            switch (condition.mRunOn)
            {
                case ConditionRunOn::Subject: return ConditionSubject::Self;
                case ConditionRunOn::Target: return ConditionSubject::Target;
                case ConditionRunOn::Reference: return ConditionSubject::Reference;
                case ConditionRunOn::CombatTarget: return ConditionSubject::CombatTarget;
                case ConditionRunOn::LinkedReference: return ConditionSubject::LinkedReference;
                case ConditionRunOn::Unknown: return ConditionSubject::Self;
            }
            return ConditionSubject::Self;
        }

        ConditionResult fromValue(ConditionValue value, const PackageCondition& condition,
            const ConditionEvaluationContext& context)
        {
            if (value.mStatus != ConditionValueStatus::Value || !std::isfinite(value.mValue))
            {
                return value.mStatus == ConditionValueStatus::Unsupported ? ConditionResult::Unsupported
                                                                           : ConditionResult::MissingContext;
            }

            double comparison = condition.mComparisonValue;
            if (condition.mUseGlobal)
            {
                if (condition.mComparisonGlobalKey.isNull() || !context.mResolveGlobal)
                    return ConditionResult::MissingContext;
                const std::optional<double> global = context.mResolveGlobal(condition.mComparisonGlobalKey);
                if (!global || !std::isfinite(*global))
                    return ConditionResult::MissingContext;
                comparison = *global;
            }
            return conditionIsTrue(value.mValue, comparison, condition.mOperator) ? ConditionResult::True
                                                                                    : ConditionResult::False;
        }

        bool subjectAvailable(ConditionSubject subject, const ConditionEvaluationContext& context)
        {
            switch (subject)
            {
                case ConditionSubject::Self:
                    return true;
                case ConditionSubject::Target:
                    return context.mHasTarget;
                case ConditionSubject::Reference:
                    return context.mHasReference;
                case ConditionSubject::Player:
                    return context.mHasPlayer;
                case ConditionSubject::CombatTarget:
                    return context.mHasCombatTarget;
                case ConditionSubject::LinkedReference:
                    return context.mHasLinkedReference;
            }
            return false;
        }
    }

    ConditionResult evaluateCondition(
        const PackageCondition& condition, const ConditionEvaluationContext& context)
    {
        if (condition.mOperator == ConditionOperator::Unknown)
            return ConditionResult::Unsupported;
        if (condition.mRunOn == ConditionRunOn::Unknown)
            return ConditionResult::Unsupported;
        const ConditionSubject subject = subjectFor(condition);
        if (!subjectAvailable(subject, context) || !context.mResolve)
            return ConditionResult::MissingContext;
        return fromValue(context.mResolve(condition, subject), condition, context);
    }

    ConditionResult evaluateConditions(
        std::span<const PackageCondition> conditions, const ConditionEvaluationContext& context)
    {
        if (conditions.empty())
            return ConditionResult::True;

        ConditionResult group = evaluateCondition(conditions.front(), context);
        for (std::size_t i = 1; i < conditions.size(); ++i)
        {
            const ConditionResult current = evaluateCondition(conditions[i], context);
            if (conditions[i].mOr)
            {
                if (group == ConditionResult::True || current == ConditionResult::True)
                    group = ConditionResult::True;
                else if (current == ConditionResult::Unsupported || group == ConditionResult::Unsupported)
                    group = ConditionResult::Unsupported;
                else if (current == ConditionResult::MissingContext || group == ConditionResult::MissingContext)
                    group = ConditionResult::MissingContext;
                else
                    group = ConditionResult::False;
                continue;
            }

            if (group != ConditionResult::True)
            {
                if (group == ConditionResult::Unsupported)
                    return ConditionResult::Unsupported;
                if (group == ConditionResult::MissingContext)
                    return ConditionResult::MissingContext;
                return ConditionResult::False;
            }
            group = current;
        }

        if (group == ConditionResult::True)
            return ConditionResult::True;
        if (group == ConditionResult::Unsupported)
            return ConditionResult::Unsupported;
        if (group == ConditionResult::MissingContext)
            return ConditionResult::MissingContext;
        return ConditionResult::False;
    }

    std::string_view conditionFunctionName(std::int32_t function)
    {
        // TES4 functions used by the stock package corpus. Unknown entries are
        // intentionally reported as unknown and cannot accidentally evaluate.
        switch (function)
        {
            case 1: return "GetDistance";
            case 5: return "GetLocked";
            case 6: return "GetPos";
            case 8: return "GetAngle";
            case 10: return "GetStartingPos";
            case 11: return "GetStartingAngle";
            case 12: return "GetSecondsPassed";
            case 14: return "GetActorValue";
            case 18: return "GetCurrentTime";
            case 24: return "GetScale";
            case 25: return "IsMoving";
            case 26: return "IsTurning";
            case 27: return "GetLineOfSight";
            case 32: return "GetIsInSameCell";
            case 35: return "GetDisabled";
            case 36: return "GetMenuMode";
            case 39: return "GetDisease";
            case 40: return "GetVampire";
            case 41: return "GetClothingValue";
            case 42: return "SameFaction";
            case 43: return "SameRace";
            case 44: return "SameSex";
            case 45: return "GetDetected";
            case 46: return "GetDead";
            case 47: return "GetItemCount";
            case 48: return "GetGold";
            case 49: return "GetSleeping";
            case 50: return "GetTalkedToPC";
            case 53: return "GetScriptVariable";
            case 56: return "GetQuestRunning";
            case 58: return "GetStage";
            case 59: return "GetStageDone";
            case 60: return "GetFactionRankDifference";
            case 61: return "GetAlarmed";
            case 62: return "IsRaining";
            case 63: return "GetAttacked";
            case 64: return "GetIsCreature";
            case 65: return "GetLockLevel";
            case 66: return "GetShouldAttack";
            case 67: return "GetInCell";
            case 68: return "GetIsClass";
            case 69: return "GetIsRace";
            case 70: return "GetIsSex";
            case 71: return "GetInFaction";
            case 72: return "GetIsID";
            case 73: return "GetFactionRank";
            case 74: return "GetGlobalValue";
            case 75: return "IsSnowing";
            case 76: return "GetDisposition";
            case 77: return "GetRandomPercent";
            case 79: return "GetQuestVariable";
            case 80: return "GetLevel";
            case 81: return "GetArmorRating";
            case 84: return "GetDeadCount";
            case 91: return "GetIsAlerted";
            case 98: return "GetPlayerControlsDisabled";
            case 99: return "GetHeadingAngle";
            case 101: return "IsWeaponOut";
            case 102: return "IsTorchOut";
            case 103: return "IsShieldOut";
            case 106: return "IsFacingUp";
            case 107: return "GetKnockedState";
            case 108: return "GetWeaponAnimType";
            case 109: return "IsWeaponSkillType";
            case 110: return "GetCurrentAIPackage";
            case 111: return "IsWaiting";
            case 112: return "IsIdlePlaying";
            case 116: return "GetMinorCrimeCount";
            case 117: return "GetMajorCrimeCount";
            case 118: return "GetActorAggroRadiusViolated";
            case 122: return "GetCrime";
            case 123: return "IsGreetingPlayer";
            case 125: return "GetIsGuard";
            case 127: return "HasBeenEaten";
            case 128: return "GetFatiguePercentage";
            case 129: return "GetPCIsClass";
            case 130: return "GetPCIsRace";
            case 131: return "GetPCIsSex";
            case 132: return "GetPCInFaction";
            case 133: return "SameFactionAsPC";
            case 134: return "SameRaceAsPC";
            case 135: return "SameSexAsPC";
            case 136: return "GetIsReference";
            case 141: return "IsTalking";
            case 142: return "GetWalkSpeed";
            case 143: return "GetCurrentAIProcedure";
            case 144: return "GetTrespassWarningLevel";
            case 145: return "IsTrespassing";
            case 146: return "IsInMyOwnedCell";
            case 147: return "GetWindSpeed";
            case 148: return "GetCurrentWeatherPercent";
            case 149: return "GetIsCurrentWeather";
            case 150: return "IsContinuingPackagePCNear";
            case 153: return "CanHaveFlames";
            case 154: return "HasFlames";
            case 157: return "GetOpenState";
            case 159: return "GetSitting";
            case 160: return "GetFurnitureMarkerID";
            case 161: return "GetIsCurrentPackage";
            case 162: return "IsCurrentFurnitureRef";
            case 163: return "IsCurrentFurnitureObj";
            case 170: return "GetDayofWeek";
            case 171: return "IsPlayerInJail";
            case 172: return "GetTalkedToPCParam";
            case 175: return "IsPCSleeping";
            case 176: return "IsPCAMurderer";
            case 180: return "GetDetectionLevel";
            case 182: return "GetEquipped";
            case 185: return "IsSwimming";
            case 190: return "GetAmountSoldStolen";
            case 192: return "GetIgnoreCrime";
            case 193: return "GetPCExpelled";
            case 195: return "GetPCFactionMurder";
            case 197: return "GetPCEnemyofFaction";
            case 199: return "GetPCFactionAttack";
            case 203: return "GetDestroyed";
            case 214: return "HasMagicEffect";
            case 215: return "GetDefaultOpen";
            case 219: return "GetAnimAction";
            case 223: return "IsSpellTarget";
            case 224: return "GetVATSMode";
            case 225: return "GetPersuasionNumber";
            case 226: return "GetSandman";
            case 227: return "GetCannibal";
            case 228: return "GetIsClassDefault";
            case 229: return "GetClassDefaultMatch";
            case 230: return "GetInCellParam";
            case 235: return "GetVatsTargetHeight";
            case 237: return "GetIsGhost";
            case 242: return "GetUnconscious";
            case 244: return "GetRestrained";
            case 246: return "GetIsUsedItem";
            case 247: return "GetIsUsedItemType";
            case 249: return "GetPCFame";
            case 254: return "GetIsPlayableRace";
            case 255: return "GetOffersServicesNow";
            case 258: return "GetUsedItemLevel";
            case 259: return "GetUsedItemActivate";
            case 264: return "GetBarterGold";
            case 265: return "IsTimePassing";
            case 266: return "IsPleasant";
            case 267: return "IsCloudy";
            case 274: return "GetArmorRatingUpperBody";
            case 277: return "GetBaseActorValue";
            case 278: return "IsOwner";
            case 280: return "IsCellOwner";
            case 282: return "IsHorseStolen";
            case 285: return "IsLeftUp";
            case 286: return "IsSneaking";
            case 287: return "IsRunning";
            case 288: return "GetFriendHit";
            case 289: return "IsInCombat";
            case 300: return "IsInInterior";
            case 304: return "IsWaterObject";
            case 306: return "IsActorUsingATorch";
            case 310: return "GetInWorldspace";
            case 327: return "IsRidingHorse";
            case 339: return "IsPlayersLastRiddenHorse";
            case 353: return "IsActor";
            case 354: return "IsEssential";
            case 358: return "IsPlayerMovingIntoNewSpace";
            case 365: return "IsChild";
            default: return {};
        }
        return {};
    }
}
