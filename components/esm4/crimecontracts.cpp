#include "crimecontracts.hpp"
#include "runtimereferences.hpp"

#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>

namespace
{
    void require(bool valid, const char* message)
    {
        if (!valid) throw std::invalid_argument(message);
    }

    void key(const ESM::FormKey& value, bool nullable = false)
    {
        require(nullable || !value.isNull(), "Crime contract requires a stable identity");
        require(ESM::FormKey::deserialize(value.serialize()) == value, "Crime contract identity is not canonical");
    }

    void size(std::size_t count)
    {
        require(count <= ESM4::MaxCrimeContractEntries, "Crime contract exceeds collection capacity");
    }
}

namespace ESM4
{
    void CrimeRequest::validate() const
    {
        require(mAction != 0, "Crime request requires a causal action");
        key(mPerpetrator);
        key(mVictim, true);
        key(mAffectedReference, true);
        key(mCell);
        key(mOwnership.mOwner, true);
        key(mOwnership.mGlobal, true);
        require(mOffense >= CrimeOffense::Theft && mOffense <= CrimeOffense::JailBreak,
            "Crime request has an invalid offense");
        require(mItemValue >= 0 && mCount >= 0, "Crime request has a negative value or quantity");
    }

    void CrimeOutcome::validate() const
    {
        require(mIncident != 0, "Crime outcome requires an incident identity");
        require(mReportPhase <= CrimeReportPhase::Resolved, "Crime outcome has an invalid report phase");
        size(mWitnesses.size());
        size(mFactionDeltas.size());
        std::set<ESM::FormKey> witnesses, factions;
        for (const auto& witness : mWitnesses)
        {
            key(witness.mWitness);
            require(witnesses.insert(runtimeReferenceKey(witness.mWitness)).second, "Crime outcome repeats a witness");
            require(!witness.mWillReport || witness.mObserved, "Unobserved crime cannot have a reporting witness");
        }
        for (const auto& delta : mFactionDeltas)
        {
            key(delta.mFaction);
            require(factions.insert(delta.mFaction).second, "Crime outcome repeats a faction delta");
        }
        require(mReportPhase != CrimeReportPhase::Resolved || mConsequencesCommitted,
            "Resolved crime has uncommitted consequences");
    }

    void ArrestTransaction::validate() const
    {
        require(mTransaction != 0 && mIncident != 0, "Arrest transaction has no identity or incident");
        key(mActor);
        key(mAuthority);
        key(mDestination, true);
        require(mResolution <= ArrestResolution::Resist && mPhase <= ArrestPhase::Cancelled,
            "Arrest transaction has an invalid phase or resolution");
        require(mAssessedFine >= 0, "Arrest transaction has a negative fine");
        require(!(mFineCommitted || mConfiscationCommitted || mTransitionCommitted)
                || mPhase == ArrestPhase::Resolving || mPhase == ArrestPhase::Committed,
            "Arrest consequences precede resolution");
        require(mPhase != ArrestPhase::Resolving || mResolution != ArrestResolution::Undecided,
            "Resolving arrest has no selected resolution");
        require(!mConfiscationCommitted || mResolution == ArrestResolution::PayFine || mResolution == ArrestResolution::Jail,
            "Arrest confiscation commit disagrees with its resolution");
        require(!mFineCommitted || mResolution == ArrestResolution::PayFine,
            "Arrest fine commit disagrees with its resolution");
        require(!mTransitionCommitted || mResolution == ArrestResolution::Jail,
            "Arrest transition commit disagrees with its resolution");
        require(!mTransitionCommitted || !mDestination.isNull(), "Committed arrest transition has no destination");
        require(mPhase != ArrestPhase::Committed || mResolution != ArrestResolution::Undecided,
            "Committed arrest has no selected resolution");
        require(mPhase != ArrestPhase::Committed || mResolution != ArrestResolution::PayFine || mFineCommitted,
            "Committed fine resolution has no fine commit");
        require(mPhase != ArrestPhase::Committed || mResolution != ArrestResolution::Jail || mTransitionCommitted,
            "Committed jail resolution has no transition commit");
        require(mPhase != ArrestPhase::Cancelled || (!mFineCommitted && !mConfiscationCommitted && !mTransitionCommitted),
            "Cancelled arrest contains committed consequences");
    }

    void CrimePropertyMetadata::validate() const
    {
        key(mInstance);
        key(mBase);
        key(mOriginalOwner, true);
        key(mOriginalOwnershipGlobal, true);
        require(mCount > 0, "Crime property metadata requires a positive quantity");
        for (const auto& value : {mCondition, mCharge})
            require(!value || (std::isfinite(*value) && *value >= 0), "Crime property metadata has invalid item extras");
    }

    void JailTransaction::validate() const
    {
        require(mTransaction != 0, "Jail transaction has no identity");
        key(mActor);
        key(mPrison);
        key(mEvidence);
        key(mBelongings);
        key(mRelease);
        require(mPhase <= JailPhase::Cancelled, "Jail transaction has an invalid phase");
        require(std::isfinite(mSentenceStart) && mSentenceStart >= 0
                && std::isfinite(mRemainingHours) && mRemainingHours >= 0,
            "Jail transaction has an invalid sentence time");
        size(mProperty.size());
        std::set<ESM::FormKey> instances;
        for (const auto& item : mProperty)
        {
            item.validate();
            require(instances.insert(item.mInstance).second, "Jail transaction repeats a property instance");
        }
        require(mPhase == JailPhase::Prepared || mPhase == JailPhase::Cancelled || mPropertyCommitted,
            "Active or completed jail has uncommitted property");
        require(mReleaseCommitted == (mPhase == JailPhase::Released || mPhase == JailPhase::Escaped),
            "Jail release commit disagrees with its phase");
        require(mPhase != JailPhase::Cancelled || (!mPropertyCommitted && !mSkillPenaltyCommitted && !mTimePenaltyCommitted),
            "Cancelled jail contains committed consequences");
    }

    void CrimeStateContracts::validate() const
    {
        require(mNextIncident != 0 && mNextTransaction != 0 && mActionRetentionFloor != 0,
            "Crime contract counters and retention floor must be nonzero");
        size(mIncidents.size());
        size(mArrests.size());
        size(mJails.size());
        std::map<std::uint64_t, const CrimeIncident*> incidents;
        using Cause = std::tuple<std::uint64_t, ESM::FormKey, ESM::FormKey, ESM::FormKey, CrimeOffense>;
        std::set<Cause> causes;
        for (const auto& incident : mIncidents)
        {
            incident.mRequest.validate();
            incident.mOutcome.validate();
            const auto& request = incident.mRequest;
            require(request.mAction >= mActionRetentionFloor, "Crime cause is below the retention floor");
            require(incident.mOutcome.mIncident < mNextIncident
                    && incidents.emplace(incident.mOutcome.mIncident, &incident).second,
                "Crime incident is duplicated or reuses the next identity");
            require(causes.emplace(request.mAction, runtimeReferenceKey(request.mPerpetrator), runtimeReferenceKey(request.mVictim),
                        runtimeReferenceKey(request.mAffectedReference), request.mOffense).second,
                "Crime contract duplicates a causal offense");
        }
        std::map<std::uint64_t, const ArrestTransaction*> arrests;
        for (const auto& arrest : mArrests)
        {
            arrest.validate();
            require(arrest.mTransaction < mNextTransaction && arrests.emplace(arrest.mTransaction, &arrest).second,
                "Arrest transaction is duplicated or reuses the next identity");
            const auto incident = incidents.find(arrest.mIncident);
            require(incident != incidents.end(), "Arrest transaction has a dangling incident");
            require(incident->second->mOutcome.mConsequencesCommitted,
                "Arrest transaction has uncommitted crime consequences");
            require(runtimeReferenceKey(incident->second->mRequest.mPerpetrator) == runtimeReferenceKey(arrest.mActor),
                "Arrest actor disagrees with the incident perpetrator");
        }
        std::set<std::uint64_t> jails;
        for (const auto& jail : mJails)
        {
            jail.validate();
            require(jails.insert(jail.mTransaction).second, "Jail transaction is duplicated");
            const auto arrest = arrests.find(jail.mTransaction);
            require(arrest != arrests.end(), "Jail transaction has a dangling arrest");
            require(runtimeReferenceKey(arrest->second->mActor) == runtimeReferenceKey(jail.mActor) && arrest->second->mResolution == ArrestResolution::Jail,
                "Jail transaction disagrees with its arrest actor or resolution");
            require(jail.mPhase != JailPhase::Cancelled || arrest->second->mPhase == ArrestPhase::Cancelled,
                "Cancelled jail disagrees with its arrest phase");
            require(jail.mPhase == JailPhase::Prepared || jail.mPhase == JailPhase::Cancelled
                    || arrest->second->mTransitionCommitted,
                "Active jail has an uncommitted arrest transition");
        }
    }
}
