#ifndef OPENMW_ESM4_CRIMERULES_H
#define OPENMW_ESM4_CRIMERULES_H

#include <components/esm/formkey.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <optional>

namespace ESM4
{
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
