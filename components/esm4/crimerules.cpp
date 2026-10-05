#include "crimerules.hpp"

#include "loadcell.hpp"
#include "loadfact.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ESM4
{
    JailDoorDecision jailDoorDecision(const JailDoorInput& input, const CrimeFineSettings& settings)
    {
        validateCrimeFineSettings(settings);
        if (!input.mActivatorPlayer || input.mSentenceDays <= 0)
            return {};
        if (input.mHasTeleportDestination)
            return {true, false, true, std::nullopt};
        if (input.mPlayerInShiveringIsles)
            return {};
        return {false, true, false, static_cast<float>(settings.mJailBreak)};
    }

    bool fineConfiscatesInstance(const FineConfiscationInput& input)
    {
        return input.mEntryCount > 0 && !input.mQuestItem && input.mHasExtraData
            && !input.mOwner.isNull() && input.mOwner != input.mSourceReference
            && (input.mRequiredOwner.isNull() || input.mOwner == input.mRequiredOwner);
    }

    bool servedReleaseReturnsInstance(const ServedPropertyInput& input)
    {
        return input.mEntryCount > 0 && (!input.mHasExtraData
            || input.mOwner.isNull() || input.mOwner == input.mDestinationBase);
    }

    std::optional<std::int32_t> servedReleaseRemainderRequest(std::int32_t entryCount,
        std::int32_t simpleInstances, std::int32_t complexInstances)
    {
        if (simpleInstances < 0 || complexInstances < 0)
            throw std::invalid_argument("negative native property instance count");
        // Explicit native 32-bit arithmetic, including authored extreme counts.
        const auto instances = std::uint32_t(simpleInstances) + std::uint32_t(complexInstances);
        if (std::bit_cast<std::int32_t>(instances) <= 0)
            return std::nullopt;
        return std::bit_cast<std::int32_t>(std::uint32_t(entryCount) - instances);
    }

    namespace
    {
        void validateBountyState(const CrimeBountyState& state)
        {
            if (!std::isfinite(state.mNormal) || state.mNormal < 0 || !std::isfinite(state.mShiveringIsles))
                throw std::invalid_argument("invalid native bounty storage");
        }
    }

    float queryCrimeBounty(const CrimeBountyState& state, bool player, bool playerInShiveringIsles)
    {
        validateBountyState(state);
        const float stored = player && playerInShiveringIsles ? state.mShiveringIsles : state.mNormal;
        return stored > 0 && stored < 1 ? 1.f : stored;
    }

    CrimeBountyChange modifyCrimeBounty(
        const CrimeBountyState& state, float increment, bool player, bool playerInShiveringIsles)
    {
        validateBountyState(state);
        if (!std::isfinite(increment))
            throw std::invalid_argument("nonfinite native bounty increment");
        const bool alternate = player && playerInShiveringIsles;
        CrimeBountyChange result{state, player && !alternate && increment > 1};
        float& stored = alternate ? result.mState.mShiveringIsles : result.mState.mNormal;
        const double sum = static_cast<double>(stored) + increment;
        if (std::abs(sum) > std::numeric_limits<float>::max())
            throw std::overflow_error("native bounty storage overflow");
        stored = static_cast<float>(sum);
        if (!alternate && stored <= 0)
            stored = 0; // Native removes ExtraCrimeGold at zero/negative, and absent reads as zero.
        return result;
    }

    bool crimeReportEligible(const CrimeReportInput& input)
    {
        return input.mIncidentPresent && input.mOffenderNpc && !input.mAlreadyReported
            && (input.mReporterGuard || input.mReporterResponsibility >= 100);
    }

    void validateCrimeInfamySettings(const CrimeInfamySettings& settings)
    {
        if (!std::isfinite(settings.mBountyThreshold) || settings.mBountyThreshold <= 0)
            throw std::invalid_argument("invalid native infamy bounty threshold");
    }

    CrimeInfamyState advanceCrimeInfamy(
        const CrimeInfamyState& state, float bountyIncrement, const CrimeInfamySettings& settings)
    {
        validateCrimeInfamySettings(settings);
        if (state.mInfamy < 0 || state.mAccumulatedBounty < 0 || !std::isfinite(bountyIncrement))
            throw std::invalid_argument("invalid native infamy state or bounty increment");
        if (bountyIncrement <= 1)
            return state;
        const double increment = std::trunc(static_cast<double>(bountyIncrement));
        if (increment > std::numeric_limits<std::int32_t>::max())
            throw std::overflow_error("native infamy bounty increment overflow");
        const std::int64_t accumulated = static_cast<std::int64_t>(state.mAccumulatedBounty)
            + static_cast<std::int32_t>(increment);
        if (accumulated > std::numeric_limits<std::int32_t>::max())
            throw std::overflow_error("native infamy accumulator overflow");
        CrimeInfamyState result{state.mInfamy, static_cast<std::int32_t>(accumulated)};
        // The native integer accumulator is stored to float before comparison
        // and subtraction; only one threshold is consumed per bounty update.
        const float rounded = static_cast<float>(result.mAccumulatedBounty);
        if (rounded >= settings.mBountyThreshold)
        {
            if (result.mInfamy == std::numeric_limits<std::int32_t>::max())
                throw std::overflow_error("native infamy counter overflow");
            const double remainder = std::trunc(static_cast<double>(rounded) - settings.mBountyThreshold);
            if (remainder > std::numeric_limits<std::int32_t>::max())
                throw std::overflow_error("native infamy remainder overflow");
            ++result.mInfamy;
            result.mAccumulatedBounty = static_cast<std::int32_t>(remainder);
        }
        return result;
    }

    bool crimeAlarmRecipientResponds(const CrimeAlarmRecipientInput& input)
    {
        if (!input.mActor || input.mHasAlarmPackage || input.mSitSleepState == 9 || input.mInCombat)
            return false;
        if (input.mGuard)
            return !input.mIncidentSuppressesGuards && !input.mEmitterEvil;
        return input.mFightScore > 0;
    }

    void validateCrimeAlarmSettings(const CrimeAlarmSettings& settings)
    {
        if (settings.mRecipientDistance < 0)
            throw std::invalid_argument("invalid native crime alarm recipient distance");
    }

    bool crimeAlarmReachesLocation(const CrimeAlarmLocation& offender, const CrimeAlarmLocation& recipient,
        float distanceToOffender, std::span<const CrimeAlarmDoorDestination> doors, const CrimeAlarmSettings& settings)
    {
        validateCrimeAlarmSettings(settings);
        const auto validateLocation = [](const CrimeAlarmLocation& location) {
            if (location.mInterior && location.mCell.isNull())
                throw std::invalid_argument("native crime alarm interior without cell identity");
        };
        const auto validateDistance = [](float distance) {
            if (!std::isfinite(distance) || distance < 0)
                throw std::invalid_argument("invalid native crime alarm spatial distance");
        };
        validateLocation(offender);
        validateLocation(recipient);
        validateDistance(distanceToOffender);
        for (const auto& door : doors)
        {
            validateLocation(door.mLocation);
            validateDistance(door.mRecipientDistance);
        }
        const bool sameCell = !recipient.mCell.isNull() && recipient.mCell == offender.mCell;
        const bool sharedExterior = recipient.mWorldspace == offender.mWorldspace
            && ((!recipient.mCell.isNull() && !recipient.mInterior)
                || (!offender.mCell.isNull() && !offender.mInterior));
        if (sameCell || sharedExterior)
            return double(distanceToOffender) <= settings.mRecipientDistance;
        for (const auto& door : doors)
        {
            const bool matchingDestination = door.mLocation.mCell == recipient.mCell
                || (door.mLocation.mCell.isNull() && door.mLocation.mWorldspace == recipient.mWorldspace);
            if (matchingDestination && double(door.mRecipientDistance) <= settings.mRecipientDistance)
                return true;
        }
        return false;
    }

    bool crimeWitnessCandidate(const CrimeWitnessCandidateInput& input)
    {
        if (input.mOffenderPresent && ((input.mOffenderFlags & 0x820u) != 0
                || input.mOffenderLifeState == 1 || input.mOffenderLifeState == 2))
            return false;
        if (!input.mCandidateActor || input.mCandidateIsOffender || input.mCandidateParalyzed
            || (input.mCandidateFlags & 0x800u) != 0)
            return false;
        switch (input.mCandidateLifeState)
        {
            case 1:
            case 2:
            case 3:
            case 6:
                return false;
        }
        return input.mDetection > 0;
    }

    void validateTrespassWarningSettings(const TrespassWarningSettings& settings)
    {
        if (!std::isfinite(settings.mTimerLimit) || settings.mTimerLimit < 0)
            throw std::invalid_argument("invalid native trespass warning timer setting");
    }

    TrespassWarningResult advanceTrespassWarning(
        const TrespassWarningInput& input, const TrespassWarningSettings& settings)
    {
        validateTrespassWarningSettings(settings);
        if (input.mWarningCount < 0 || !std::isfinite(input.mTimer) || input.mTimer < 0
            || !std::isfinite(input.mFrameDuration) || input.mFrameDuration < 0)
            throw std::invalid_argument("invalid native trespass warning state");
        if (!input.mTargetTrespassing)
            return {TrespassWarningAction::Leave, input.mTimer};
        if (input.mCellOffLimits || (input.mWarningCount > 1 && input.mTimer <= 0))
            return {TrespassWarningAction::Escalate, input.mTimer};
        if (input.mTimer <= 0)
            return {TrespassWarningAction::Warn, input.mTimer};
        const auto stored = [](double value) {
            if (value > std::numeric_limits<float>::max())
                throw std::invalid_argument("native trespass warning timer overflow");
            return static_cast<float>(value);
        };
        const float doubledStep = stored(double(input.mFrameDuration) * 2);
        const float timer = stored(double(input.mTimer) + doubledStep);
        return {TrespassWarningAction::Wait, timer > settings.mTimerLimit ? 0.f : timer};
    }

    bool attackCrimeAlarmEligible(const AttackCrimeAlarmInput& input)
    {
        if (input.mOffense != CrimeOffense::Assault && input.mOffense != CrimeOffense::Murder)
            throw std::invalid_argument("invalid native attack crime alarm offense");
        if (!input.mVictimPlayableRace && !input.mVictimGuard)
            return false;
        if (input.mOffenderPlayer && input.mPlayerJailDays > 0 && !input.mPlayerHasCombatOrPursuit)
            return false;
        if (input.mOffense == CrimeOffense::Assault ? input.mVictimTrespassing : input.mOffenderTrespassing)
            return false;
        if (!input.mOffenderNpc || !input.mOffenderPlayableRace || input.mOffenderGuard)
            return false;
        if (input.mVictimSpecialCombat && input.mOffenderSpecialCombat)
            return false;
        return input.mOffenderPlayer || input.mOffenderSneak != 100 || !input.mOffenderSneaking;
    }

    ActorFactionCrimePolicy actorFactionCrimePolicy(std::span<const std::uint8_t> factionFlags)
    {
        ActorFactionCrimePolicy result{!factionFlags.empty(), false};
        for (const auto flags : factionFlags)
        {
            result.mEvil = result.mEvil && (flags & static_cast<std::uint8_t>(FactionFlag::Evil)) != 0;
            result.mSpecialCombat = result.mSpecialCombat
                || (flags & static_cast<std::uint8_t>(FactionFlag::SpecialCombat)) != 0;
        }
        return result;
    }

    namespace
    {
        void nonnegative(float value)
        {
            if (!std::isfinite(value) || value < 0)
                throw std::invalid_argument("invalid native crime amount");
        }
    }

    ResolvedOwnership resolveOwnership(OwnershipReferenceKind kind, const std::array<OwnershipLayer, 3>& layers)
    {
        std::size_t ownerLayers;
        switch (kind)
        {
            case OwnershipReferenceKind::Other: ownerLayers = 3; break;
            case OwnershipReferenceKind::Actor: ownerLayers = 1; break;
            case OwnershipReferenceKind::Furniture:
            case OwnershipReferenceKind::Door:
            case OwnershipReferenceKind::Activator: ownerLayers = 2; break;
            default: throw std::invalid_argument("invalid native ownership reference kind");
        }
        ResolvedOwnership result;
        for (std::size_t i = 0; i < ownerLayers; ++i)
            if (!layers[i].mOwner.isNull())
            {
                result.mOwner = layers[i].mOwner;
                break;
            }
        for (const auto& layer : layers)
            if (layer.mRank != -1)
            {
                result.mRank = layer.mRank;
                break;
            }
        for (const auto& layer : layers)
            if (!layer.mGlobal.isNull())
            {
                result.mGlobal = layer.mGlobal;
                break;
            }
        return result;
    }

    bool playerReferenceIsOffLimits(const ReferenceAccessInput& input)
    {
        switch (input.mKind)
        {
            case ReferenceAccessKind::Object:
            case ReferenceAccessKind::Npc:
            case ReferenceAccessKind::Creature:
            case ReferenceAccessKind::Horse:
            case ReferenceAccessKind::Door: break;
            default: throw std::invalid_argument("invalid native reference access kind");
        }
        const auto& destination = input.mDestination;
        if ((!input.mHasOwner && (input.mOwnerEvil || input.mPlayerHasClaim))
            || (!destination.mHasOwner && (destination.mOwnerEvil || destination.mPlayerHasClaim)))
            throw std::invalid_argument("native reference access claim requires an owner");
        if (input.mOwnerEvil)
            return false;
        if (input.mKind == ReferenceAccessKind::Door)
        {
            const bool restrictedDestination = destination.mHasOwner && !destination.mOwnerEvil
                && (destination.mFlags & CELL_Interior) && !(destination.mFlags & (CELL_Public | CELL_HandChgd));
            if (input.mHasOwner && !input.mPlayerHasClaim)
                return !input.mDoorPermission && (input.mDoorLocked || restrictedDestination);
            return restrictedDestination && !destination.mPlayerHasClaim;
        }
        if (input.mKind == ReferenceAccessKind::Npc && input.mPlayerSneaking && !input.mDead)
            return true;
        return input.mHasOwner && !input.mPlayerHasClaim
            && (input.mKind == ReferenceAccessKind::Object || input.mKind == ReferenceAccessKind::Horse);
    }

    bool playerHasTrespassExitExemption(const DoorTrespassExitInput& input)
    {
        if (!input.mPlayerTrespassing || !input.mHasTeleport || !input.mHasLockData || input.mLockLevel == 100)
            return false;
        return !input.mHasDestinationCell || !(input.mDestinationFlags & CELL_Interior)
            || (input.mDestinationFlags & CELL_Public);
    }

    bool cellTreatsActorAsTrespasser(const CellTrespassInput& input)
    {
        switch (input.mOwnerKind)
        {
            case CrimeOwnerKind::None:
            case CrimeOwnerKind::Actor:
            case CrimeOwnerKind::Faction: break;
            default: throw std::invalid_argument("invalid native cell owner kind");
        }
        if (input.mMatchesActorBase && input.mOwnerKind != CrimeOwnerKind::Actor)
            throw std::invalid_argument("native cell identity match requires actor owner");
        if (input.mActorIsGuard && !input.mActorIsNpc)
            throw std::invalid_argument("native guard class requires NPC");
        if (input.mActorIsGuard || input.mOwnerKind == CrimeOwnerKind::None
            || (input.mCellFlags & (CELL_Public | CELL_HandChgd)) || input.mHasPermissionGlobal || !input.mActorIsNpc)
            return false;
        if (input.mOwnerKind == CrimeOwnerKind::Actor)
            return !input.mMatchesActorBase;
        const std::int32_t required = input.mRequiredRank == -1 ? 0 : input.mRequiredRank;
        return input.mActorFactionRank < required;
    }

    bool hasOwnershipClaim(const OwnershipClaimInput& input)
    {
        if (input.mGlobalValue && !std::isfinite(*input.mGlobalValue))
            throw std::invalid_argument("nonfinite native ownership global");
        if (input.mMatchesActorBase && input.mOwnerKind != CrimeOwnerKind::Actor)
            throw std::invalid_argument("native ownership identity match requires actor owner");
        const bool globalAllows = input.mGlobalValue && *input.mGlobalValue != 0;
        switch (input.mOwnerKind)
        {
            case CrimeOwnerKind::None:
                return false;
            case CrimeOwnerKind::Actor:
                return input.mMatchesActorBase || globalAllows;
            case CrimeOwnerKind::Faction:
                return (input.mUseFactionOwnership || globalAllows)
                    && input.mActorFactionRank >= input.mRequiredFactionRank;
        }
        throw std::invalid_argument("invalid native crime owner kind");
    }

    std::uint8_t advanceJailSkillSelection(std::optional<std::uint8_t> candidate, std::uint32_t draw)
    {
        if (draw > 32767 || (candidate && *candidate >= 12))
            throw std::invalid_argument("invalid native jail selection state or draw");
        return static_cast<std::uint8_t>(candidate ? *candidate + draw % 10 : draw % 21);
    }

    std::optional<std::uint8_t> jailSkillBaseAfterPenalty(std::uint8_t base, float current)
    {
        if (!std::isfinite(current) || double(current) < std::numeric_limits<std::int32_t>::min()
            || double(current) > std::numeric_limits<std::int32_t>::max())
            throw std::invalid_argument("invalid native jail current skill");
        if (static_cast<std::int32_t>(current) <= 1)
            return {};
        // Original TESNPC setter stores the low byte; the eligibility check
        // used the modified value, so it does not guarantee a positive base.
        return static_cast<std::uint8_t>(int(base) - 1);
    }

    void validateCrimeFineSettings(const CrimeFineSettings& settings)
    {
        nonnegative(settings.mTheftMultiplier);
        for (auto value : {settings.mPickpocket, settings.mTrespass, settings.mAssault,
                 settings.mMurder, settings.mHorseTheft, settings.mJailBreak})
            if (value < 0)
                throw std::invalid_argument("negative native crime fine setting");
    }

    void validateCrimeReportingSettings(const CrimeReportingSettings& settings)
    {
        nonnegative(settings.mResponsibilityMultiplier);
    }

    bool responsibilityAllowsAlarm(std::int32_t disposition, std::int32_t responsibility,
        const CrimeReportingSettings& settings)
    {
        validateCrimeReportingSettings(settings);
        // No float store or random draw occurs before the original comparison.
        return double(responsibility) * settings.mResponsibilityMultiplier > disposition;
    }

    void validateJailSettings(const JailSettings& settings)
    {
        if (settings.mGoldPerDay <= 0)
            throw std::invalid_argument("invalid native jail sentence divisor");
    }

    float baseCrimeFine(CrimeOffense offense, std::int32_t itemValue, const CrimeFineSettings& settings)
    {
        validateCrimeFineSettings(settings);
        switch (offense)
        {
            case CrimeOffense::Theft:
            {
                if (itemValue < 0)
                    throw std::invalid_argument("invalid native stolen item value");
                const double fine = double(itemValue == 0 ? 1 : itemValue) * settings.mTheftMultiplier;
                if (fine > std::numeric_limits<float>::max())
                    throw std::invalid_argument("native theft fine overflow");
                return static_cast<float>(fine);
            }
            case CrimeOffense::Pickpocket: return static_cast<float>(settings.mPickpocket);
            case CrimeOffense::Trespass: return static_cast<float>(settings.mTrespass);
            case CrimeOffense::Assault: return static_cast<float>(settings.mAssault);
            case CrimeOffense::Murder: return static_cast<float>(settings.mMurder);
            case CrimeOffense::HorseTheft: return static_cast<float>(settings.mHorseTheft);
            case CrimeOffense::JailBreak: return static_cast<float>(settings.mJailBreak);
        }
        throw std::invalid_argument("invalid native crime offense");
    }

    float factionCrimeFine(float baseFine, std::span<const float> factionMultipliers)
    {
        nonnegative(baseFine);
        float multiplier = 1.f;
        for (float value : factionMultipliers)
        {
            nonnegative(value);
            multiplier = std::max(multiplier, value);
        }
        const double result = double(baseFine) * multiplier;
        if (result > std::numeric_limits<float>::max())
            throw std::invalid_argument("native faction fine overflow");
        return static_cast<float>(result);
    }

    JailSentence jailSentence(float bounty, const JailSettings& settings)
    {
        validateJailSettings(settings);
        nonnegative(bounty);
        const double days = std::max(1.0, std::trunc(double(bounty) / settings.mGoldPerDay));
        if (days > std::numeric_limits<std::int32_t>::max() / 24)
            throw std::invalid_argument("native jail time integer overflow");
        const auto count = static_cast<std::int32_t>(days);
        return {count, count * 24, static_cast<std::uint32_t>(std::min(count, 10))};
    }
}
