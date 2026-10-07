#include <gtest/gtest.h>

#include <components/esm4/crimecontracts.hpp>

#include <functional>
#include <limits>
#include <stdexcept>

namespace
{
    ESM::FormKey content(std::uint32_t id) { return ESM::FormKey::content("contracts.esm", id); }

    ESM4::CrimeStateContracts populated()
    {
        ESM4::CrimeStateContracts state;
        state.mNextIncident = 3;
        state.mNextTransaction = 3;
        ESM4::CrimeIncident incident;
        incident.mRequest.mAction = 9;
        incident.mRequest.mPerpetrator = ESM::FormKey::dynamic("player", 1);
        incident.mRequest.mVictim = content(1);
        incident.mRequest.mAffectedReference = content(2);
        incident.mRequest.mCell = content(3);
        incident.mOutcome.mIncident = 1;
        incident.mOutcome.mConsequencesCommitted = true;
        incident.mOutcome.mWitnesses = {{content(4), true, true}};
        incident.mOutcome.mFactionDeltas = {{content(5), -2}};
        state.mIncidents.push_back(incident);
        ESM4::ArrestTransaction arrest;
        arrest.mTransaction = 1;
        arrest.mIncident = 1;
        arrest.mActor = incident.mRequest.mPerpetrator;
        arrest.mAuthority = content(4);
        arrest.mDestination = content(6);
        arrest.mResolution = ESM4::ArrestResolution::Jail;
        arrest.mPhase = ESM4::ArrestPhase::Committed;
        arrest.mConfiscationCommitted = arrest.mTransitionCommitted = true;
        state.mArrests.push_back(arrest);
        ESM4::JailTransaction jail;
        jail.mTransaction = 1;
        jail.mActor = arrest.mActor;
        jail.mPrison = content(6);
        jail.mEvidence = content(7);
        jail.mBelongings = content(8);
        jail.mRelease = content(9);
        jail.mPhase = ESM4::JailPhase::Serving;
        jail.mSentenceStart = 11.25;
        jail.mRemainingHours = 3.5f;
        jail.mPropertyCommitted = true;
        ESM4::CrimePropertyMetadata property;
        property.mInstance = content(10);
        property.mBase = content(11);
        property.mCount = 3;
        property.mOriginalOwner = content(12);
        property.mOriginalOwnershipRank = -2;
        property.mOriginalOwnershipGlobal = content(13);
        property.mCondition = 37.125f;
        property.mCharge = 9.25f;
        property.mQuestItem = true;
        jail.mProperty.push_back(property);
        state.mJails.push_back(jail);
        return state;
    }
}

TEST(ESM4CrimeContracts, EmptyAndPopulatedContractsValidateWithoutMutation)
{
    EXPECT_NO_THROW(ESM4::CrimeStateContracts{}.validate());
    const auto state = populated();
    const auto copy = state;
    EXPECT_NO_THROW(state.validate());
    EXPECT_EQ(state, copy);
    EXPECT_EQ(state.mJails.front().mProperty.front().mCondition, 37.125f);
    EXPECT_EQ(state.mJails.front().mProperty.front().mOriginalOwnershipRank, -2);
}

TEST(ESM4CrimeContracts, CausalIdentityDeduplicatesOffenseWithoutCombiningDifferentVictims)
{
    auto state = populated();
    auto second = state.mIncidents.front();
    second.mOutcome.mIncident = 2;
    state.mIncidents.push_back(second);
    EXPECT_THROW(state.validate(), std::invalid_argument);
    state.mIncidents.back().mRequest.mVictim = content(99);
    EXPECT_NO_THROW(state.validate());
    state.mActionRetentionFloor = 10;
    EXPECT_THROW(state.validate(), std::invalid_argument);
    state.mActionRetentionFloor = 9;
    EXPECT_NO_THROW(state.validate());
}

TEST(ESM4CrimeContracts, RejectsDuplicateDanglingAndReusedGraphIdentities)
{
    const std::vector<std::function<void(ESM4::CrimeStateContracts&)>> mutations{
        [](auto& s) { s.mNextIncident = 0; },
        [](auto& s) { s.mNextTransaction = 0; },
        [](auto& s) { s.mActionRetentionFloor = 0; },
        [](auto& s) { s.mNextIncident = 1; },
        [](auto& s) { s.mNextTransaction = 1; },
        [](auto& s) { s.mIncidents.push_back(s.mIncidents.front()); },
        [](auto& s) { s.mArrests.push_back(s.mArrests.front()); },
        [](auto& s) { s.mJails.push_back(s.mJails.front()); },
        [](auto& s) { s.mArrests.front().mIncident = 2; },
        [](auto& s) { s.mArrests.front().mActor = content(99); },
        [](auto& s) { s.mJails.front().mTransaction = 2; },
        [](auto& s) { s.mJails.front().mActor = content(99); },
        [](auto& s) { s.mJails.front().mProperty.push_back(s.mJails.front().mProperty.front()); },
        [](auto& s) { s.mIncidents.front().mOutcome.mWitnesses.push_back({content(4), true, true}); },
        [](auto& s) { s.mIncidents.front().mOutcome.mFactionDeltas.push_back({content(5), 3}); }};
    for (std::size_t i = 0; i < mutations.size(); ++i)
    {
        SCOPED_TRACE(i);
        auto state = populated();
        mutations[i](state);
        const auto before = state;
        EXPECT_THROW(state.validate(), std::invalid_argument);
        EXPECT_EQ(state, before);
    }
}

TEST(ESM4CrimeContracts, RejectsUnrepresentableFieldsAndContradictoryCommitFlags)
{
    const std::vector<std::function<void(ESM4::CrimeStateContracts&)>> mutations{
        [](auto& s) { s.mIncidents.front().mRequest.mAction = 0; },
        [](auto& s) { s.mIncidents.front().mRequest.mCell = {}; },
        [](auto& s) { s.mIncidents.front().mRequest.mPerpetrator.mValue = 0; },
        [](auto& s) { s.mIncidents.front().mRequest.mVictim.mNamespace = "CONTRACTS.ESM"; },
        [](auto& s) { s.mIncidents.front().mRequest.mOffense = static_cast<ESM4::CrimeOffense>(99); },
        [](auto& s) { s.mIncidents.front().mRequest.mCount = -1; },
        [](auto& s) { s.mIncidents.front().mRequest.mItemValue = -1; },
        [](auto& s) { s.mIncidents.front().mOutcome.mReportPhase = static_cast<ESM4::CrimeReportPhase>(99); },
        [](auto& s) { s.mIncidents.front().mOutcome.mConsequencesCommitted = false; },
        [](auto& s) { s.mIncidents.front().mOutcome.mWitnesses.front().mObserved = false; },
        [](auto& s) { s.mArrests.front().mAssessedFine = -1; },
        [](auto& s) { s.mArrests.front().mFineCommitted = true; },
        [](auto& s) { s.mArrests.front().mDestination = {}; },
        [](auto& s) { s.mArrests.front().mResolution = ESM4::ArrestResolution::Undecided; },
        [](auto& s) { s.mArrests.front().mPhase = ESM4::ArrestPhase::Cancelled; },
        [](auto& s) { s.mArrests.front().mPhase = ESM4::ArrestPhase::Choosing; },
        [](auto& s) { s.mJails.front().mRemainingHours = std::numeric_limits<float>::infinity(); },
        [](auto& s) { s.mJails.front().mSentenceStart = -1; },
        [](auto& s) { s.mJails.front().mRemainingHours = -1; },
        [](auto& s) { s.mJails.front().mPhase = static_cast<ESM4::JailPhase>(99); },
        [](auto& s) { s.mJails.front().mReleaseCommitted = true; },
        [](auto& s) { s.mJails.front().mPropertyCommitted = false; },
        [](auto& s) { s.mJails.front().mProperty.front().mCount = 0; },
        [](auto& s) { s.mJails.front().mProperty.front().mCondition = -1.f; },
        [](auto& s) { s.mJails.front().mProperty.front().mCharge = std::numeric_limits<float>::quiet_NaN(); }};
    for (std::size_t i = 0; i < mutations.size(); ++i)
    {
        SCOPED_TRACE(i);
        auto state = populated();
        mutations[i](state);
        EXPECT_THROW(state.validate(), std::exception);
    }
}

TEST(ESM4CrimeContracts, CollectionCapacityRejectsBeforeWalkingUnresolvedWitnesses)
{
    ESM4::CrimeOutcome outcome;
    outcome.mIncident = 1;
    outcome.mWitnesses.resize(ESM4::MaxCrimeContractEntries + 1);
    try
    {
        outcome.validate();
        FAIL() << "Oversized witness collection accepted";
    }
    catch (const std::invalid_argument& error)
    {
        EXPECT_STREQ(error.what(), "Crime contract exceeds collection capacity");
    }
}

TEST(ESM4CrimeContracts, ExplicitResolutionAndReleaseBoundariesRemainValid)
{
    auto state = populated();
    state.mArrests.front().mPhase = ESM4::ArrestPhase::Resolving;
    EXPECT_NO_THROW(state.validate());
    state.mArrests.front().mPhase = ESM4::ArrestPhase::Committed;
    state.mIncidents.front().mOutcome.mReportPhase = ESM4::CrimeReportPhase::Resolved;
    state.mIncidents.front().mOutcome.mConsequencesCommitted = true;
    for (const auto phase : {ESM4::JailPhase::Released, ESM4::JailPhase::Escaped})
    {
        state.mJails.front().mPhase = phase;
        state.mJails.front().mReleaseCommitted = true;
        EXPECT_NO_THROW(state.validate());
    }
    state.mJails.clear();
    auto& arrest = state.mArrests.front();
    arrest.mResolution = ESM4::ArrestResolution::PayFine;
    arrest.mFineCommitted = true;
    arrest.mTransitionCommitted = false;
    EXPECT_NO_THROW(state.validate());
    arrest.mFineCommitted = arrest.mConfiscationCommitted = false;
    arrest.mResolution = ESM4::ArrestResolution::Resist;
    EXPECT_NO_THROW(state.validate());
}

TEST(ESM4CrimeContracts, PlayerReferenceAliasSharesGraphWitnessAndCausalIdentityWithoutRewritingKeys)
{
    const auto player = ESM::FormKey::dynamic("player", 1);
    const auto alias = ESM::FormKey::content("oblivion.esm", 0x14);
    auto state = populated();
    state.mArrests.front().mActor = alias;
    const auto before = state;
    EXPECT_NO_THROW(state.validate());
    EXPECT_EQ(state, before);
    state.mJails.front().mActor = alias;
    EXPECT_NO_THROW(state.validate());
    auto duplicate = state.mIncidents.front();
    duplicate.mOutcome.mIncident = 2;
    duplicate.mRequest.mPerpetrator = alias;
    state.mIncidents.push_back(duplicate);
    EXPECT_THROW(state.validate(), std::invalid_argument);
    for (const auto& distinct : {ESM::FormKey::content("other.esm", 0x14),
             ESM::FormKey::content("oblivion.esm", 7)})
    {
        state.mIncidents.back().mRequest.mPerpetrator = distinct;
        EXPECT_NO_THROW(state.validate());
    }
    state.mIncidents.resize(1);
    state.mIncidents.front().mOutcome.mWitnesses = {{player, true, false}, {alias, true, false}};
    EXPECT_THROW(state.validate(), std::invalid_argument);
    state.mIncidents.front().mOutcome.mWitnesses.back().mWitness = ESM::FormKey::content("other.esm", 0x14);
    EXPECT_NO_THROW(state.validate());
    state.mIncidents.front().mRequest.mVictim = player;
    state.mIncidents.front().mRequest.mAffectedReference = player;
    duplicate = state.mIncidents.front(); duplicate.mOutcome.mIncident = 2;
    duplicate.mRequest.mVictim = duplicate.mRequest.mAffectedReference = alias;
    state.mIncidents.push_back(duplicate);
    EXPECT_THROW(state.validate(), std::invalid_argument);
}
