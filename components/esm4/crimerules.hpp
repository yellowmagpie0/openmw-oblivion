#ifndef OPENMW_ESM4_CRIMERULES_H
#define OPENMW_ESM4_CRIMERULES_H

#include <components/esm/formkey.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <optional>

namespace ESM4
{
    struct ActorFactionCrimePolicy
    {
        bool mEvil = false;
        bool mSpecialCombat = false;
    };
    // Resolved faction entries, not disposition relationships. Native queries
    // inspect flags regardless of signed membership rank. Empty is neither.
    ActorFactionCrimePolicy actorFactionCrimePolicy(std::span<const std::uint8_t> factionFlags);

    enum class OwnershipReferenceKind { Other, Actor, Furniture, Door, Activator };
    struct OwnershipLayer
    {
        ESM::FormKey mOwner;
        std::int32_t mRank = -1; // Native missing/inherit sentinel, including authored -1.
        ESM::FormKey mGlobal;
    };
    struct ResolvedOwnership
    {
        ESM::FormKey mOwner;
        std::int32_t mRank = 0;
        ESM::FormKey mGlobal;
    };
    // Layers are reference, teleport destination reference, and current cell.
    // Each field inherits independently. A missing layer is an empty value;
    // caller resolves winning records/teleport identity before this pure rule.
    ResolvedOwnership resolveOwnership(OwnershipReferenceKind kind, const std::array<OwnershipLayer, 3>& layers);

    enum class CrimeOwnerKind { None, Actor, Faction };
    enum class ReferenceAccessKind { Object, Npc, Creature, Horse, Door };
    struct DestinationCellAccess
    {
        bool mHasOwner = false;
        bool mOwnerEvil = false;
        bool mPlayerHasClaim = false;
        std::uint8_t mFlags = 0;
    };
    struct ReferenceAccessInput
    {
        ReferenceAccessKind mKind = ReferenceAccessKind::Object;
        bool mHasOwner = false;
        bool mOwnerEvil = false;
        bool mPlayerHasClaim = false;
        bool mPlayerSneaking = false;
        bool mDead = false;
        bool mDoorLocked = false;
        bool mDoorPermission = false; // Resolved native door permission, including exit exemption.
        DestinationCellAccess mDestination;
    };
    // Native player-facing off-limits query, not reported crime or permission
    // to bypass other interaction constraints. Inputs require resolved records.
    bool playerReferenceIsOffLimits(const ReferenceAccessInput& input);

    struct DoorTrespassExitInput
    {
        bool mPlayerTrespassing;
        bool mHasTeleport;
        bool mHasLockData;
        std::uint8_t mLockLevel;
        bool mHasDestinationCell;
        std::uint8_t mDestinationFlags;
    };
    // Player-only exit exemption within native door permission policy. Ownership,
    // guard/follower rights and actually opening/unlocking a door are separate.
    bool playerHasTrespassExitExemption(const DoorTrespassExitInput& input);

    struct CellTrespassInput
    {
        CrimeOwnerKind mOwnerKind;
        std::uint8_t mCellFlags;
        bool mHasPermissionGlobal;
        bool mActorIsNpc;
        bool mActorIsGuard;
        bool mMatchesActorBase;
        std::int32_t mActorFactionRank;
        std::int32_t mRequiredRank; // Raw cell -1 sentinel resolves to zero.
    };
    // Native cell trespass query only; does not report an incident or decide
    // theft/door access. A permission-global identity suffices here, regardless
    // of its value. Guard classification comes from the NPC's class flag.
    bool cellTreatsActorAsTrespasser(const CellTrespassInput& input);

    struct OwnershipClaimInput
    {
        CrimeOwnerKind mOwnerKind;
        bool mMatchesActorBase;
        std::optional<float> mGlobalValue;
        std::int32_t mActorFactionRank;
        std::int32_t mRequiredFactionRank;
        bool mUseFactionOwnership;
    };
    // Requires a resolved, valid actor base. This answers an ownership claim,
    // not crime legality: unowned property has no claim, but may be taken.
    // Evil-faction exemptions, access/public-cell policy and witnesses are
    // evaluated by the crime caller, not folded into this predicate.
    bool hasOwnershipClaim(const OwnershipClaimInput& input);

    enum class CrimeOffense { Theft, Pickpocket, Trespass, Assault, Murder, HorseTheft, JailBreak };
    struct CrimeFineSettings
    {
        float mTheftMultiplier;
        std::int32_t mPickpocket;
        std::int32_t mTrespass;
        std::int32_t mAssault;
        std::int32_t mMurder;
        std::int32_t mHorseTheft;
        std::int32_t mJailBreak;
    };
    struct CrimeReportingSettings
    {
        float mResponsibilityMultiplier;
    };
    // A willingness check only; caller owns witness eligibility and reporting.
    bool responsibilityAllowsAlarm(std::int32_t disposition, std::int32_t responsibility,
        const CrimeReportingSettings& settings);
    void validateCrimeReportingSettings(const CrimeReportingSettings& settings);
    struct JailSettings
    {
        std::int32_t mGoldPerDay;
    };
    struct JailSentence
    {
        std::int32_t mDays;
        std::int32_t mHours;
        std::uint32_t mSkillChecks; // Attempts, not guaranteed changes or unique skills.
    };
    // Base per-incident fine, not legality, witness reporting or committed bounty.
    // Theft value comes from the incident item or its supplied value fallback.
    float baseCrimeFine(CrimeOffense offense, std::int32_t itemValue, const CrimeFineSettings& settings);
    float factionCrimeFine(float baseFine, std::span<const float> factionMultipliers);
    JailSentence jailSentence(float bounty, const JailSettings& settings);
    // One original 15-bit draw per call. Null starts an attempt; candidates
    // below 12 require another draw. Results 12..20 are actor values, not
    // zero-based skill indices. Completed selections must not consume a draw.
    std::uint8_t advanceJailSkillSelection(std::optional<std::uint8_t> candidate, std::uint32_t draw);
    // Only the reachable penalty branch (selected actor values 12..20).
    // Null means the attempt consumes a check but does not change the skill.
    std::optional<std::uint8_t> jailSkillBaseAfterPenalty(std::uint8_t base, float current);
    void validateCrimeFineSettings(const CrimeFineSettings& settings);
    void validateJailSettings(const JailSettings& settings);
}

#endif
