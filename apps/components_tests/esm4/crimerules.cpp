#include <gtest/gtest.h>

#include <components/esm4/combatsettings.hpp>
#include <components/esm4/loadgmst.hpp>

#include <array>
#include <cmath>
#include <limits>
#include <random>
#include <vector>
#include <stdexcept>

TEST(ESM4CrimeRules, BaseFinesPreserveFractionalTheftAndNativeOffenseValues)
{
    const auto settings = ESM4::buildCrimeFineSettings({});
    for (int value : {0,1,2,3,9,10,11,100,100000})
        EXPECT_EQ(ESM4::baseCrimeFine(ESM4::CrimeOffense::Theft, value, settings), value == 0 ? .5f : value * .5f);
    EXPECT_EQ(ESM4::baseCrimeFine(ESM4::CrimeOffense::Pickpocket, 0, settings), 25);
    EXPECT_EQ(ESM4::baseCrimeFine(ESM4::CrimeOffense::Trespass, 0, settings), 5);
    EXPECT_EQ(ESM4::baseCrimeFine(ESM4::CrimeOffense::Assault, 0, settings), 40);
    EXPECT_EQ(ESM4::baseCrimeFine(ESM4::CrimeOffense::Murder, 0, settings), 1000);
    EXPECT_EQ(ESM4::baseCrimeFine(ESM4::CrimeOffense::HorseTheft, 0, settings), 25);
    EXPECT_EQ(ESM4::baseCrimeFine(ESM4::CrimeOffense::JailBreak, 0, settings), 100);
    auto installed = settings; installed.mHorseTheft = 250; installed.mJailBreak = 50;
    EXPECT_EQ(ESM4::baseCrimeFine(ESM4::CrimeOffense::HorseTheft, 0, installed), 250);
    EXPECT_EQ(ESM4::baseCrimeFine(ESM4::CrimeOffense::JailBreak, 0, installed), 50);
    installed.mTheftMultiplier = 0;
    EXPECT_EQ(ESM4::baseCrimeFine(ESM4::CrimeOffense::Theft, 0, installed), 0);
    EXPECT_THROW(ESM4::baseCrimeFine(static_cast<ESM4::CrimeOffense>(99), 0, settings), std::invalid_argument);
    EXPECT_THROW(ESM4::baseCrimeFine(ESM4::CrimeOffense::Theft, -1, settings), std::invalid_argument);
    installed.mTheftMultiplier = std::numeric_limits<float>::max();
    EXPECT_THROW(ESM4::baseCrimeFine(ESM4::CrimeOffense::Theft, 2, installed), std::invalid_argument);
}

TEST(ESM4CrimeRules, FactionFineUsesHighestMultiplierStartingAtOne)
{
    EXPECT_EQ(ESM4::factionCrimeFine(40, {}), 40);
    EXPECT_EQ(ESM4::factionCrimeFine(40, std::array{2.f,3.f}), 120);
    EXPECT_EQ(ESM4::factionCrimeFine(40, std::array{.5f,.25f,2.f}), 80);
    EXPECT_EQ(ESM4::factionCrimeFine(40, std::array{2.f,.25f,.5f}), 80);
    EXPECT_EQ(ESM4::factionCrimeFine(40, std::array{.5f,0.f}), 40);
    EXPECT_EQ(ESM4::factionCrimeFine(.5f, std::array{.5f}), .5f);
    EXPECT_EQ(ESM4::factionCrimeFine(40, std::array{std::nextafter(1.f, 0.f)}), 40);
    EXPECT_GT(ESM4::factionCrimeFine(40, std::array{std::nextafter(1.f, 2.f)}), 40);
    for (float bad : {-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
    {
        EXPECT_THROW(ESM4::factionCrimeFine(bad, {}), std::invalid_argument);
        EXPECT_THROW(ESM4::factionCrimeFine(40, std::array{bad}), std::invalid_argument);
    }
}

TEST(ESM4CrimeRules, SentenceFloorsBountyDivisionAndCapsSkillChecksNotElapsedDays)
{
    const auto settings = ESM4::buildJailSettings({});
    const std::array<std::pair<float,int>, 11> cases{{{0,1},{1,1},{99,1},{100,1},{199,1},{200,2},
        {700,7},{999,9},{1000,10},{1100,11},{100000,1000}}};
    for (const auto& [bounty, days] : cases)
    {
        const auto sentence = ESM4::jailSentence(bounty, settings);
        EXPECT_EQ(sentence.mDays, days); EXPECT_EQ(sentence.mHours, days * 24);
        EXPECT_EQ(sentence.mSkillChecks, unsigned(std::min(days,10)));
    }
    EXPECT_EQ(ESM4::jailSentence(std::nextafter(200.f, 0.f), settings).mDays, 1);
    EXPECT_EQ(ESM4::jailSentence(std::nextafter(200.f, 300.f), settings).mDays, 2);
    EXPECT_EQ(ESM4::jailSentence(200, {50}).mDays, 4);
    for (float bad : {-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::max()})
        EXPECT_THROW(ESM4::jailSentence(bad, settings), std::invalid_argument);
    EXPECT_THROW(ESM4::jailSentence(100, {0}), std::invalid_argument);
    EXPECT_THROW(ESM4::jailSentence(100, {-1}), std::invalid_argument);
    EXPECT_THROW(ESM4::jailSentence(100000000, {1}), std::invalid_argument); // original hours field is int32
}

TEST(ESM4CrimeRules, NativeSettingsRequireCorrectTypesAndNonnegativeFines)
{
    ESM4::GameSetting value{};
    const std::array<const ESM4::GameSetting*,1> values{&value};
    struct Binding { const char* mName; std::int32_t ESM4::CrimeFineSettings::* mMember; };
    const std::array bindings{
        Binding{"iCrimeGoldAttack", &ESM4::CrimeFineSettings::mAssault},
        Binding{"iCrimeGoldMurder", &ESM4::CrimeFineSettings::mMurder},
        Binding{"iCrimeGoldPickpocket", &ESM4::CrimeFineSettings::mPickpocket},
        Binding{"iCrimeGoldTresspass", &ESM4::CrimeFineSettings::mTrespass},
        Binding{"iCrimeGoldStealHorse", &ESM4::CrimeFineSettings::mHorseTheft},
        Binding{"iCrimeGoldJailBreak", &ESM4::CrimeFineSettings::mJailBreak},
    };
    for (const auto& binding : bindings)
    {
        value.mEditorId = binding.mName; value.mData = std::int32_t{0};
        EXPECT_EQ(ESM4::buildCrimeFineSettings(values).*binding.mMember, 0);
        value.mData = std::int32_t{17};
        EXPECT_EQ(ESM4::buildCrimeFineSettings(values).*binding.mMember, 17);
        value.mData = std::int32_t{-1};
        EXPECT_THROW(ESM4::buildCrimeFineSettings(values), std::invalid_argument);
        value.mData = 17.f;
        EXPECT_THROW(ESM4::buildCrimeFineSettings(values), std::invalid_argument);
    }
    value.mEditorId = "fCrimeGoldSteal"; value.mData = .75f;
    EXPECT_EQ(ESM4::buildCrimeFineSettings(values).mTheftMultiplier, .75f);
    value.mData = std::int32_t{1};
    EXPECT_THROW(ESM4::buildCrimeFineSettings(values), std::invalid_argument);
    value.mEditorId = "iCrimeDaysInPrisonMod"; value.mData = std::int32_t{50};
    EXPECT_EQ(ESM4::buildJailSettings(values).mGoldPerDay, 50);
    value.mData = 50.f;
    EXPECT_THROW(ESM4::buildJailSettings(values), std::invalid_argument);
}


TEST(ESM4CrimeRules, JailSelectionPreservesNativeDrawSequenceAndActorValueRange)
{
    struct Case { std::vector<std::uint32_t> draws; std::uint8_t expected; std::size_t consumed; };
    // Independently executed original 00670808..00670849 instructions.
    for (const Case& sample : std::vector<Case>{{{12},12,1}, {{20},20,1}, {{21,9,3},12,3},
             {{0,0,0,9,9},18,5}, {{11,9},20,2}, {{11,1},12,2}, {{32767,9,9},16,2},
             {{10,1,1},12,3}, {{0,1,1,1,1,1,1,1,1,1,1,1,1},12,13}})
    {
        std::optional<std::uint8_t> candidate;
        std::size_t consumed = 0;
        for (auto draw : sample.draws)
        {
            candidate = ESM4::advanceJailSkillSelection(candidate, draw);
            ++consumed;
            if (*candidate >= 12) break;
        }
        EXPECT_EQ(candidate, sample.expected);
        EXPECT_EQ(consumed, sample.consumed);
    }
    std::optional<std::uint8_t> candidate;
    for (unsigned i = 0; i < 1024; ++i)
    {
        candidate = ESM4::advanceJailSkillSelection(candidate, 0);
        EXPECT_EQ(candidate, 0); // Zero draws neither complete nor invent a retry cap.
    }
    EXPECT_THROW(ESM4::advanceJailSkillSelection({}, 32768), std::invalid_argument);
    EXPECT_THROW(ESM4::advanceJailSkillSelection({}, UINT32_MAX), std::invalid_argument);
    for (unsigned completed : {12u, 20u, 21u, 255u})
        EXPECT_THROW(ESM4::advanceJailSkillSelection(completed, 0), std::invalid_argument);
}

TEST(ESM4CrimeRules, JailSelectionMatchesIndependentAbsorbingChainDistribution)
{
    // Exact residue-weighted calculation for independent uniform 15-bit draws,
    // including modulo bias and repeated zero draws. Predeclared tolerance 600.
    const std::array<int, 9> expected{14903,14177,13370,12473,11477,10370,9141,7775,6316};
    std::array<int, 9> actual{};
    std::mt19937 random(0x4d15a1);
    for (unsigned i = 0; i < 100000; ++i)
    {
        std::optional<std::uint8_t> candidate;
        // Test safety bound is not part of the production selection rule.
        for (unsigned count = 0; count < 100; ++count)
        {
            candidate = ESM4::advanceJailSkillSelection(candidate, random() & 32767);
            if (*candidate >= 12) break;
        }
        ASSERT_GE(*candidate, 12);
        ASSERT_LE(*candidate, 20);
        ++actual[*candidate - 12];
    }
    for (unsigned i = 0; i < actual.size(); ++i)
        EXPECT_NEAR(actual[i], expected[i], 600);
}


TEST(ESM4CrimeRules, JailPenaltyChecksTruncatedModifiedSkillBeforeNativeByteMutation)
{
    for (float skipped : {-100.f, 0.f, 1.f, std::nextafter(2.f, 0.f)})
        EXPECT_FALSE(ESM4::jailSkillBaseAfterPenalty(100, skipped));
    for (float changed : {2.f, std::nextafter(2.f, 3.f), 100.f})
        EXPECT_EQ(ESM4::jailSkillBaseAfterPenalty(5, changed), 4);
    // The guard is on modified skill, not on the stored base byte.
    EXPECT_EQ(ESM4::jailSkillBaseAfterPenalty(1, 2), 0);
    EXPECT_EQ(ESM4::jailSkillBaseAfterPenalty(0, 2), 255);
    EXPECT_EQ(ESM4::jailSkillBaseAfterPenalty(255, 255), 254);
    for (float invalid : {std::numeric_limits<float>::quiet_NaN(),
             std::numeric_limits<float>::infinity(), std::numeric_limits<float>::max(),
             -std::numeric_limits<float>::max()})
        EXPECT_THROW(ESM4::jailSkillBaseAfterPenalty(5, invalid), std::invalid_argument);
}


TEST(ESM4CrimeRules, AlarmResponsibilityUsesStrictDispositionComparisonWithoutRandomDraw)
{
    const ESM4::CrimeReportingSettings installed{2.f};
    EXPECT_FALSE(ESM4::responsibilityAllowsAlarm(0, 0, installed));
    EXPECT_TRUE(ESM4::responsibilityAllowsAlarm(-1, 0, installed));
    EXPECT_TRUE(ESM4::responsibilityAllowsAlarm(79, 40, installed));
    EXPECT_FALSE(ESM4::responsibilityAllowsAlarm(80, 40, installed));
    EXPECT_FALSE(ESM4::responsibilityAllowsAlarm(81, 40, installed));
    EXPECT_TRUE(ESM4::responsibilityAllowsAlarm(100, 100, installed));
    EXPECT_FALSE(ESM4::responsibilityAllowsAlarm(100, 50, installed));
    EXPECT_FALSE(ESM4::responsibilityAllowsAlarm(0, -1, installed));
    EXPECT_FALSE(ESM4::responsibilityAllowsAlarm(1, 1, {1.f}));
    EXPECT_TRUE(ESM4::responsibilityAllowsAlarm(1, 1, {std::nextafter(1.f, 2.f)}));
    EXPECT_FALSE(ESM4::responsibilityAllowsAlarm(1, 1, {std::nextafter(1.f, 0.f)}));
    // Original compares the extended product directly; no intermediate float store.
    EXPECT_TRUE(ESM4::responsibilityAllowsAlarm(170, 100, {1.7f}));
    EXPECT_FALSE(ESM4::responsibilityAllowsAlarm(171, 100, {1.7f}));
    EXPECT_TRUE(ESM4::responsibilityAllowsAlarm(INT32_MAX, INT32_MAX, {std::numeric_limits<float>::max()}));
    EXPECT_FALSE(ESM4::responsibilityAllowsAlarm(0, 100, {0.f}));
    for (float invalid : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        EXPECT_THROW(ESM4::responsibilityAllowsAlarm(50, 100, {invalid}), std::invalid_argument);
}

TEST(ESM4CrimeRules, AlarmResponsibilityResolvesTypedNativeSetting)
{
    EXPECT_EQ(ESM4::buildCrimeReportingSettings({}).mResponsibilityMultiplier, 1.7f);
    ESM4::GameSetting setting{};
    setting.mEditorId = "fCrimeAlarmRespMult";
    const std::array<const ESM4::GameSetting*,1> input{&setting};
    setting.mData = 2.f;
    EXPECT_EQ(ESM4::buildCrimeReportingSettings(input).mResponsibilityMultiplier, 2.f);
    setting.mData = 0.f;
    EXPECT_EQ(ESM4::buildCrimeReportingSettings(input).mResponsibilityMultiplier, 0.f);
    setting.mData = std::int32_t{2};
    EXPECT_THROW(ESM4::buildCrimeReportingSettings(input), std::invalid_argument);
    setting.mData = -1.f;
    EXPECT_THROW(ESM4::buildCrimeReportingSettings(input), std::invalid_argument);
}

TEST(ESM4CrimeRules, OwnershipClaimUsesActorIdentityAndNonzeroPermissionGlobal)
{
    using Kind = ESM4::CrimeOwnerKind;
    EXPECT_FALSE(ESM4::hasOwnershipClaim({Kind::None, false, {}, -1, 0, true}));
    EXPECT_TRUE(ESM4::hasOwnershipClaim({Kind::Actor, true, {}, -1, 0, true}));
    EXPECT_FALSE(ESM4::hasOwnershipClaim({Kind::Actor, false, {}, -1, 0, true}));
    for (const float global : {0.f, -0.f, 1.f, -1.f, std::numeric_limits<float>::denorm_min(),
             -std::numeric_limits<float>::denorm_min(), std::numeric_limits<float>::max()})
    {
        EXPECT_EQ(ESM4::hasOwnershipClaim({Kind::Actor, false, global, -1, 0, true}), global != 0);
        EXPECT_TRUE(ESM4::hasOwnershipClaim({Kind::Actor, true, global, -1, 0, false}));
    }
}

TEST(ESM4CrimeRules, FactionOwnershipClaimPreservesRankBoundaryAndGlobalMode)
{
    using Kind = ESM4::CrimeOwnerKind;
    struct Case { int rank; int required; bool useFaction; std::optional<float> global; bool expected; };
    for (const auto& c : {Case{-1, 0, true, {}, false}, {0, 0, true, {}, true},
             {1, 2, true, {}, false}, {2, 2, true, {}, true}, {3, 2, true, {}, true},
             {-1, -2, true, {}, true}, {3, 2, false, {}, false},
             {3, 2, false, 1, true}, {1, 2, false, 1, false},
             {3, 2, true, 0, true}, {3, 2, false, -1, true}, {3, 2, false, 0, false},
             {std::numeric_limits<int>::min(), std::numeric_limits<int>::min(), true, {}, true},
             {std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), true, {}, false}})
        EXPECT_EQ(ESM4::hasOwnershipClaim({Kind::Faction, false, c.global, c.rank, c.required, c.useFaction}), c.expected);
    EXPECT_THROW(ESM4::hasOwnershipClaim({static_cast<Kind>(3), false, {}, -1, 0, true}), std::invalid_argument);
    EXPECT_THROW(ESM4::hasOwnershipClaim({Kind::Faction, true, {}, 0, 0, true}), std::invalid_argument);
    for (float invalid : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        EXPECT_THROW(ESM4::hasOwnershipClaim({Kind::Actor, false, invalid, -1, 0, true}), std::invalid_argument);
}
