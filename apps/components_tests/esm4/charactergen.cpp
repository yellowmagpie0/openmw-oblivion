#include <gtest/gtest.h>

#include <components/esm4/charactergen.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <filesystem>
#include <limits>
#include <set>

TEST(ESM4CharacterGeneration, MatchesIndependentOriginalRecommendationCases)
{
    boost::property_tree::ptree input;
    boost::property_tree::read_json((std::filesystem::path(OPENMW_PROJECT_SOURCE_DIR)
        / "scripts/data/oblivion_compat/m15_class_recommendation_cases.json").string(), input);
    ASSERT_EQ(input.get<std::string>("source_exe_sha256"),
        "a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6");
    const auto& cases = input.get_child("cases");
    ASSERT_EQ(cases.size(), 71u);
    std::set<std::string_view> recommendations;
    for (const auto& [_, row] : cases)
    {
        std::array<float, 3> uses{};
        ASSERT_EQ(row.get_child("uses").size(), uses.size());
        std::size_t i = 0;
        for (const auto& entry : row.get_child("uses")) uses[i++] = entry.second.get_value<float>();
        SCOPED_TRACE(::testing::Message() << uses[0] << ',' << uses[1] << ',' << uses[2]);
        const auto proposal = ESM4::proposeCharacterClass(uses);
        EXPECT_EQ(proposal ? std::string(proposal->mSetting) : std::string{},
            row.get<std::string>("expected_proposal"));
        const auto result = proposal.value_or(ESM4::DefaultCharacterGenerationClass);
        EXPECT_EQ(result.mSetting, row.get<std::string>("expected_setting"));
        EXPECT_EQ(result.mFormId, row.get<std::uint32_t>("expected_form_id"));
        recommendations.insert(result.mSetting);
    }
    EXPECT_EQ(recommendations.size(), 21u);
}

TEST(ESM4CharacterGeneration, ClassMatchUsesIdentityBeforeSpecialization)
{
    boost::property_tree::ptree input;
    boost::property_tree::read_json((std::filesystem::path(OPENMW_PROJECT_SOURCE_DIR)
        / "scripts/data/oblivion_compat/m15_class_recommendation_cases.json").string(), input);
    ASSERT_EQ(input.get_child("match_cases").size(), 12u);
    for (const auto& [_, row] : input.get_child("match_cases"))
    {
        std::optional<std::uint32_t> suggested;
        if (row.get<std::string>("recommended_specialization") != "null")
            suggested = row.get<std::uint32_t>("recommended_specialization");
        EXPECT_EQ(ESM4::characterClassDefaultMatch(row.get<bool>("same_class"), suggested,
            row.get<std::uint32_t>("current_specialization")), row.get<unsigned>("expected"));
    }
}

TEST(ESM4CharacterGeneration, RejectsInvalidUseAndClassInputs)
{
    for (const float invalid : {-1.f, std::numeric_limits<float>::infinity(),
             std::numeric_limits<float>::quiet_NaN()})
        EXPECT_THROW(ESM4::proposeCharacterClass({invalid, 0, 0}), std::invalid_argument);
    EXPECT_THROW(ESM4::proposeCharacterClass({std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(), 0}), std::invalid_argument);
    EXPECT_THROW(ESM4::characterClassDefaultMatch(true, std::nullopt, 0), std::invalid_argument);
    EXPECT_THROW(ESM4::characterClassDefaultMatch(false, 3, 0), std::invalid_argument);
    EXPECT_THROW(ESM4::characterClassDefaultMatch(false, 0, 3), std::invalid_argument);
}

#include <components/esm4/charactergenstate.hpp>
#include <components/esm4/runtimestate.hpp>

TEST(ESM4CharacterGeneration, TrackingOwnsHistoryAndPreservesCachedIdentityWhenDisabled)
{
    ESM4::CharacterGenerationState state;
    state.mRecommendedClass = ESM::FormKey::content("headless.esm", 0x822);
    const auto initial = state;
    EXPECT_EQ(ESM4::recordCharacterGenerationSkillUse(state, 17, 2), initial);
    state = ESM4::setCharacterGenerationTracking(state, true);
    state = ESM4::recordCharacterGenerationSkillUse(state, 17, 2);
    EXPECT_EQ((*state.mSkillUses)[5], 2);
    EXPECT_EQ((*initial.mSkillUses)[5], 0);
    const auto disabled = ESM4::setCharacterGenerationTracking(state, false);
    EXPECT_FALSE(disabled.mTracking);
    EXPECT_FALSE(disabled.mSkillUses);
    EXPECT_EQ(disabled.mRecommendedClass, initial.mRecommendedClass);
    EXPECT_EQ(ESM4::recordCharacterGenerationSkillUse(disabled, 17, 5), disabled);
    EXPECT_THROW(ESM4::recordCharacterGenerationSkillUse(state, 11, 1), std::invalid_argument);
    EXPECT_EQ((*ESM4::recordCharacterGenerationSkillUse(state, 17, -1).mSkillUses)[5], 1);
    EXPECT_THROW(ESM4::recordCharacterGenerationSkillUse(state, 17,
        std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);
}

TEST(ESM4CharacterGeneration, Runtime80RoundTripsHistoryAndRejectsDowngradeOrInvalidHistory)
{
    ESM4::RuntimeState state;
    state.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
    state.mPlayer.mCell = ESM::FormKey::content("headless.esm", 1);
    state.mPlayer.mRace = ESM::FormKey::content("headless.esm", 0x810);
    state.mPlayer.mClass = ESM::FormKey::content("headless.esm", 0x820);
    auto& generation = state.mPlayer.mCharacterGeneration.emplace();
    generation.mTracking = true;
    (*generation.mSkillUses)[5] = .25f;
    generation.mRecommendedClass = ESM::FormKey::content("headless.esm", 0x822);
    EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(state.serializeBinary()), state);
    EXPECT_NE(state.canonicalJson().find("\"character_generation\""), std::string::npos);
    auto old = state;
    old.mVersion = 79;
    EXPECT_THROW(old.serializeBinary(), std::runtime_error);
    old.mPlayer.mCharacterGeneration.reset();
    EXPECT_FALSE(ESM4::RuntimeState::deserializeBinary(old.serializeBinary()).mPlayer.mCharacterGeneration);
    generation.mSkillUses.reset();
    EXPECT_THROW(state.serializeBinary(), std::invalid_argument);
    generation.mTracking = false;
    EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(state.serializeBinary()), state);
}
