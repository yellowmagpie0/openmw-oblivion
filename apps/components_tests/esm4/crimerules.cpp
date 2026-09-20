#include <gtest/gtest.h>

#include <components/esm4/combatsettings.hpp>
#include <components/esm4/loadgmst.hpp>

#include <array>
#include <cmath>
#include <limits>
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
