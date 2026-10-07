#ifndef OPENMW_COMPONENTS_ESM4_CRIMECONTRACTSIO_H
#define OPENMW_COMPONENTS_ESM4_CRIMECONTRACTSIO_H

#include "crimecontracts.hpp"

#include <cmath>
#include <iomanip>
#include <ostream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ESM4::Detail
{
    // Field order is the v42/v43 binary contract; v43 widens bounty_delta. Names are its canonical JSON keys.
    template <class Archive, class T>
    void crimeFields(Archive& archive, T& value)
    {
        using U = std::remove_cv_t<T>;
#define FIELD(name, member) archive(name, value.member)
        if constexpr (std::is_same_v<U, CrimeStateContracts>)
        {
            FIELD("next_incident", mNextIncident); FIELD("next_transaction", mNextTransaction);
            FIELD("action_retention_floor", mActionRetentionFloor); FIELD("incidents", mIncidents);
            FIELD("arrests", mArrests); FIELD("jails", mJails);
        }
        else if constexpr (std::is_same_v<U, CrimeIncident>)
        { FIELD("request", mRequest); FIELD("outcome", mOutcome); }
        else if constexpr (std::is_same_v<U, CrimeRequest>)
        {
            FIELD("action", mAction); FIELD("perpetrator", mPerpetrator); FIELD("victim", mVictim);
            FIELD("affected_reference", mAffectedReference); FIELD("cell", mCell); FIELD("ownership", mOwnership);
            FIELD("offense", mOffense); FIELD("item_value", mItemValue); FIELD("count", mCount);
            FIELD("lawful_combat", mLawfulCombat); FIELD("owner_has_claim", mOwnerHasClaim);
        }
        else if constexpr (std::is_same_v<U, ResolvedOwnership>)
        { FIELD("owner", mOwner); FIELD("rank", mRank); FIELD("global", mGlobal); }
        else if constexpr (std::is_same_v<U, CrimeOutcome>)
        {
            FIELD("incident", mIncident); FIELD("report_phase", mReportPhase); FIELD("witnesses", mWitnesses);
            FIELD("bounty_delta", mBountyDelta); FIELD("infamy_delta", mInfamyDelta); FIELD("faction_deltas", mFactionDeltas);
            FIELD("lawful_combat_exception", mLawfulCombatException); FIELD("guard_response_requested", mGuardResponseRequested);
            FIELD("consequences_committed", mConsequencesCommitted);
        }
        else if constexpr (std::is_same_v<U, CrimeWitnessDecision>)
        { FIELD("witness", mWitness); FIELD("observed", mObserved); FIELD("will_report", mWillReport); }
        else if constexpr (std::is_same_v<U, CrimeFactionDelta>)
        { FIELD("faction", mFaction); FIELD("delta", mDelta); }
        else if constexpr (std::is_same_v<U, ArrestTransaction>)
        {
            FIELD("transaction", mTransaction); FIELD("incident", mIncident); FIELD("actor", mActor);
            FIELD("authority", mAuthority); FIELD("destination", mDestination); FIELD("resolution", mResolution);
            FIELD("phase", mPhase); FIELD("assessed_fine", mAssessedFine); FIELD("fine_committed", mFineCommitted);
            FIELD("confiscation_committed", mConfiscationCommitted); FIELD("transition_committed", mTransitionCommitted);
        }
        else if constexpr (std::is_same_v<U, JailTransaction>)
        {
            FIELD("transaction", mTransaction); FIELD("actor", mActor); FIELD("prison", mPrison); FIELD("evidence", mEvidence);
            FIELD("belongings", mBelongings); FIELD("release", mRelease); FIELD("phase", mPhase);
            FIELD("sentence_start", mSentenceStart); FIELD("remaining_hours", mRemainingHours); FIELD("property", mProperty);
            FIELD("property_committed", mPropertyCommitted); FIELD("skill_penalty_committed", mSkillPenaltyCommitted);
            FIELD("time_penalty_committed", mTimePenaltyCommitted); FIELD("release_committed", mReleaseCommitted);
        }
        else if constexpr (std::is_same_v<U, CrimePropertyMetadata>)
        {
            FIELD("instance", mInstance); FIELD("base", mBase); FIELD("count", mCount);
            FIELD("original_owner", mOriginalOwner); FIELD("original_ownership_rank", mOriginalOwnershipRank);
            FIELD("original_ownership_global", mOriginalOwnershipGlobal); FIELD("condition", mCondition); FIELD("charge", mCharge);
            FIELD("quest_item", mQuestItem);
        }
        else static_assert(!std::is_same_v<U, U>, "Unknown crime contract type");
#undef FIELD
    }

    template <class T> struct CrimeVector : std::false_type {};
    template <class T> struct CrimeVector<std::vector<T>> : std::true_type {};
    template <class T> struct CrimeOptional : std::false_type {};
    template <class T> struct CrimeOptional<std::optional<T>> : std::true_type {};

    template <class Writer>
    struct CrimeBinaryWriter
    {
        Writer& mWriter;
        std::uint32_t mVersion;
        template <class T> void operator()(std::string_view name, const T& value)
        {
            if constexpr (std::is_same_v<T, double>)
                if (name == "bounty_delta" && mVersion == 42)
                {
                    mWriter.template integer<std::int32_t>(static_cast<std::int32_t>(value));
                    return;
                }
            write(value);
        }
        template <class T> void write(const T& value)
        {
            if constexpr (std::is_same_v<T, ESM::FormKey>) mWriter.string(value.serialize());
            else if constexpr (std::is_same_v<T, bool>) mWriter.template integer<std::uint8_t>(value);
            else if constexpr (std::is_enum_v<T>) mWriter.template integer<std::uint8_t>(static_cast<std::uint8_t>(value));
            else if constexpr (std::is_integral_v<T>) mWriter.integer(value);
            else if constexpr (std::is_floating_point_v<T>) mWriter.floating(value);
            else if constexpr (CrimeOptional<T>::value)
            {
                mWriter.template integer<std::uint8_t>(value.has_value());
                if (value) write(*value);
            }
            else if constexpr (CrimeVector<T>::value)
            {
                mWriter.template integer<std::uint32_t>(static_cast<std::uint32_t>(value.size()));
                for (const auto& item : value) write(item);
            }
            else crimeFields(*this, value);
        }
    };

    template <class Reader>
    struct CrimeBinaryReader
    {
        Reader& mReader;
        std::uint32_t mVersion;
        bool boolean()
        {
            const auto value = mReader.template integer<std::uint8_t>();
            if (value > 1) throw std::runtime_error("Invalid crime contract boolean or presence flag");
            return value != 0;
        }
        template <class T> void operator()(std::string_view name, T& value)
        {
            if constexpr (std::is_same_v<T, double>)
                if (name == "bounty_delta" && mVersion == 42)
                {
                    value = mReader.template integer<std::int32_t>();
                    return;
                }
            read(value);
        }
        template <class T> void read(T& value)
        {
            if constexpr (std::is_same_v<T, ESM::FormKey>)
            {
                const auto text = mReader.string();
                try { value = ESM::FormKey::deserialize(text); }
                catch (const std::invalid_argument& error)
                { throw std::runtime_error(std::string("Invalid crime contract identity: ") + error.what()); }
                if (value.serialize() != text) throw std::runtime_error("Noncanonical crime contract identity");
            }
            else if constexpr (std::is_same_v<T, bool>) value = boolean();
            else if constexpr (std::is_enum_v<T>) value = static_cast<T>(mReader.template integer<std::uint8_t>());
            else if constexpr (std::is_integral_v<T>) value = mReader.template integer<T>();
            else if constexpr (std::is_same_v<T, float>) value = mReader.float32();
            else if constexpr (std::is_same_v<T, double>) value = mReader.float64();
            else if constexpr (CrimeOptional<T>::value)
            {
                if (boolean()) { value.emplace(); read(*value); }
                else value.reset();
            }
            else if constexpr (CrimeVector<T>::value)
            {
                const auto count = mReader.count();
                value.clear();
                // Read before inserting: a small truncated packet must not
                // reserve a hostile count of large incident/transaction objects.
                for (std::uint32_t i = 0; i < count; ++i)
                {
                    typename T::value_type item{};
                    read(item);
                    value.push_back(std::move(item));
                }
            }
            else crimeFields(*this, value);
        }
    };

    template <class Quote>
    struct CrimeJsonWriter
    {
        std::ostream& mStream;
        const Quote& mQuote;
        bool mFirst = true;
        template <class T> void operator()(std::string_view name, const T& value)
        {
            if (!mFirst) mStream << ',';
            mFirst = false;
            mStream << mQuote(name) << ':';
            write(value);
        }
        template <class T> void write(const T& value)
        {
            if constexpr (std::is_same_v<T, ESM::FormKey>) mStream << mQuote(value.serialize());
            else if constexpr (std::is_same_v<T, bool>) mStream << (value ? "true" : "false");
            else if constexpr (std::is_enum_v<T>) mStream << static_cast<unsigned>(value);
            else if constexpr (std::is_integral_v<T>) mStream << +value;
            else if constexpr (std::is_floating_point_v<T>)
            {
                if (value == 0 && std::signbit(value)) mStream << "-0.0";
                else mStream << std::setprecision(17) << value;
            }
            else if constexpr (CrimeOptional<T>::value)
            {
                if (value) write(*value);
                else mStream << "null";
            }
            else if constexpr (CrimeVector<T>::value)
            {
                mStream << '[';
                bool first = true;
                for (const auto& item : value) { if (!first) mStream << ','; first = false; write(item); }
                mStream << ']';
            }
            else
            {
                mStream << '{';
                CrimeJsonWriter nested{mStream, mQuote};
                crimeFields(nested, value);
                mStream << '}';
            }
        }
    };
}

#endif
