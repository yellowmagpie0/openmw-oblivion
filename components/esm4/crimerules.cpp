#include "crimerules.hpp"

#include "loadcell.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ESM4
{
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
