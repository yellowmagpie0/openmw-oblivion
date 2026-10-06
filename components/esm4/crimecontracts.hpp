#ifndef OPENMW_COMPONENTS_ESM4_CRIMECONTRACTS_H
#define OPENMW_COMPONENTS_ESM4_CRIMECONTRACTS_H

#include "crimerules.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <tuple>
#include <vector>

namespace ESM4
{
    // S3 interfaces and temporary-state contracts. These do not evaluate crime
    // rules, transfer property, spend gold, dispatch callbacks or enter jail.
    // World/content binding and runtime-envelope integration are separate.
    inline constexpr std::size_t MaxCrimeContractEntries = 1'000'000;

    struct CrimeRequest
    {
        std::uint64_t mAction = 0;
        ESM::FormKey mPerpetrator;
        ESM::FormKey mVictim;
        ESM::FormKey mAffectedReference;
        ESM::FormKey mCell;
        ResolvedOwnership mOwnership;
        CrimeOffense mOffense = CrimeOffense::Assault;
        std::int32_t mItemValue = 0;
        std::int32_t mCount = 0;
        bool mLawfulCombat = false;
        bool mOwnerHasClaim = false;

        void validate() const;
        friend bool operator==(const CrimeRequest& left, const CrimeRequest& right)
        {
            const auto fields = [](const CrimeRequest& value) {
                return std::tie(value.mAction, value.mPerpetrator, value.mVictim, value.mAffectedReference,
                    value.mCell, value.mOwnership.mOwner, value.mOwnership.mRank, value.mOwnership.mGlobal,
                    value.mOffense, value.mItemValue, value.mCount, value.mLawfulCombat, value.mOwnerHasClaim);
            };
            return fields(left) == fields(right);
        }
    };

    enum class CrimeReportPhase : std::uint8_t { Unreported, Pending, Reported, Resolved };
    struct CrimeWitnessDecision
    {
        ESM::FormKey mWitness;
        bool mObserved = false;
        bool mWillReport = false;
        friend bool operator==(const CrimeWitnessDecision&, const CrimeWitnessDecision&) = default;
    };
    struct CrimeFactionDelta
    {
        ESM::FormKey mFaction;
        std::int32_t mDelta = 0;
        friend bool operator==(const CrimeFactionDelta&, const CrimeFactionDelta&) = default;
    };
    struct CrimeOutcome
    {
        std::uint64_t mIncident = 0;
        CrimeReportPhase mReportPhase = CrimeReportPhase::Unreported;
        std::vector<CrimeWitnessDecision> mWitnesses;
        std::int32_t mBountyDelta = 0;
        std::int32_t mInfamyDelta = 0;
        std::vector<CrimeFactionDelta> mFactionDeltas;
        bool mLawfulCombatException = false;
        bool mGuardResponseRequested = false;
        bool mConsequencesCommitted = false;

        void validate() const;
        friend bool operator==(const CrimeOutcome&, const CrimeOutcome&) = default;
    };
    struct CrimeIncident
    {
        CrimeRequest mRequest;
        CrimeOutcome mOutcome;
        friend bool operator==(const CrimeIncident&, const CrimeIncident&) = default;
    };

    enum class ArrestResolution : std::uint8_t { Undecided, PayFine, Jail, Resist };
    enum class ArrestPhase : std::uint8_t { Pursuing, Choosing, Resolving, Committed, Cancelled };
    struct ArrestTransaction
    {
        std::uint64_t mTransaction = 0;
        std::uint64_t mIncident = 0;
        ESM::FormKey mActor;
        ESM::FormKey mAuthority;
        ESM::FormKey mDestination;
        ArrestResolution mResolution = ArrestResolution::Undecided;
        ArrestPhase mPhase = ArrestPhase::Pursuing;
        std::int32_t mAssessedFine = 0;
        bool mFineCommitted = false;
        bool mConfiscationCommitted = false;
        bool mTransitionCommitted = false;

        void validate() const;
        friend bool operator==(const ArrestTransaction&, const ArrestTransaction&) = default;
    };

    struct CrimePropertyMetadata
    {
        ESM::FormKey mInstance;
        ESM::FormKey mBase;
        std::int32_t mCount = 0;
        ESM::FormKey mOriginalOwner;
        std::optional<std::int32_t> mOriginalOwnershipRank;
        ESM::FormKey mOriginalOwnershipGlobal;
        std::optional<float> mCondition;
        std::optional<float> mCharge;
        bool mQuestItem = false;

        void validate() const;
        friend bool operator==(const CrimePropertyMetadata&, const CrimePropertyMetadata&) = default;
    };
    enum class JailPhase : std::uint8_t { Prepared, Serving, Released, Escaped, Cancelled };
    struct JailTransaction
    {
        // Same transaction as the arrest which chose jail, not a second debit.
        std::uint64_t mTransaction = 0;
        ESM::FormKey mActor;
        ESM::FormKey mPrison;
        ESM::FormKey mEvidence;
        ESM::FormKey mBelongings;
        ESM::FormKey mRelease;
        JailPhase mPhase = JailPhase::Prepared;
        double mSentenceStart = 0;
        float mRemainingHours = 0;
        std::vector<CrimePropertyMetadata> mProperty;
        bool mPropertyCommitted = false;
        bool mSkillPenaltyCommitted = false;
        bool mTimePenaltyCommitted = false;
        bool mReleaseCommitted = false;

        void validate() const;
        friend bool operator==(const JailTransaction&, const JailTransaction&) = default;
    };

    struct CrimeStateContracts
    {
        std::uint64_t mNextIncident = 1;
        std::uint64_t mNextTransaction = 1;
        // An admitted cause below this floor is retired, never a new offense.
        // Advancing it may only retire resolved incidents. Unresolved offenses
        // must fit the explicit capacity or fail; they are never dropped.
        std::uint64_t mActionRetentionFloor = 1;
        std::vector<CrimeIncident> mIncidents;
        std::vector<ArrestTransaction> mArrests;
        std::vector<JailTransaction> mJails;

        void validate() const;
        friend bool operator==(const CrimeStateContracts&, const CrimeStateContracts&) = default;
    };
}

#endif
