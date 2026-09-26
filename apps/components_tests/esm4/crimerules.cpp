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

TEST(ESM4CrimeRules, OwnershipFieldsInheritIndependentlyAcrossReferenceTeleportAndCell)
{
    using K = ESM4::OwnershipReferenceKind;
    const auto owner = ESM::FormKey::content("Owner.esm", 0x801);
    const auto other = ESM::FormKey::content("Other.esm", 0x801);
    const auto global = ESM::FormKey::content("Globals.esm", 0x900);
    std::array<ESM4::OwnershipLayer, 3> layers{{{owner, -1, {}}, {other, 3, {}}, {other, 5, global}}};
    auto result = ESM4::resolveOwnership(K::Other, layers);
    EXPECT_EQ(result.mOwner, owner); EXPECT_EQ(result.mRank, 3); EXPECT_EQ(result.mGlobal, global);
    layers[0] = {}; layers[1].mGlobal = global; layers[2].mGlobal = other;
    result = ESM4::resolveOwnership(K::Other, layers);
    EXPECT_EQ(result.mOwner, other); EXPECT_EQ(result.mRank, 3); EXPECT_EQ(result.mGlobal, global);
    layers[1] = {};
    result = ESM4::resolveOwnership(K::Other, layers);
    EXPECT_EQ(result.mOwner, other); EXPECT_EQ(result.mRank, 5); EXPECT_EQ(result.mGlobal, other);
    result = ESM4::resolveOwnership(K::Other, {});
    EXPECT_TRUE(result.mOwner.isNull()); EXPECT_EQ(result.mRank, 0); EXPECT_TRUE(result.mGlobal.isNull());
}

TEST(ESM4CrimeRules, OwnershipInheritanceExceptionsAffectOwnerOnly)
{
    using K = ESM4::OwnershipReferenceKind;
    const auto owner = ESM::FormKey::content("Owner.esm", 0x801);
    const auto global = ESM::FormKey::content("Globals.esm", 0x900);
    std::array<ESM4::OwnershipLayer, 3> layers{{{}, {}, {owner, 2, global}}};
    for (K kind : {K::Actor, K::Furniture, K::Door, K::Activator})
    {
        const auto result = ESM4::resolveOwnership(kind, layers);
        EXPECT_TRUE(result.mOwner.isNull()); EXPECT_EQ(result.mRank, 2); EXPECT_EQ(result.mGlobal, global);
    }
    layers[1].mOwner = owner;
    EXPECT_TRUE(ESM4::resolveOwnership(K::Actor, layers).mOwner.isNull());
    for (K kind : {K::Furniture, K::Door, K::Activator})
        EXPECT_EQ(ESM4::resolveOwnership(kind, layers).mOwner, owner);
    layers[0].mOwner = owner;
    EXPECT_EQ(ESM4::resolveOwnership(K::Actor, layers).mOwner, owner);
    EXPECT_THROW(ESM4::resolveOwnership(static_cast<K>(99), layers), std::invalid_argument);
}

TEST(ESM4CrimeRules, OwnershipRankMinusOneInheritsButOtherSignedRanksArePreserved)
{
    using K = ESM4::OwnershipReferenceKind;
    for (int rank : {std::numeric_limits<int>::min(), -2, 0, 1, std::numeric_limits<int>::max()})
        for (std::size_t index : {0u, 1u, 2u})
        {
            std::array<ESM4::OwnershipLayer, 3> layers{};
            layers[index].mRank = rank;
            EXPECT_EQ(ESM4::resolveOwnership(K::Other, layers).mRank, rank);
        }
    const std::array<ESM4::OwnershipLayer, 3> inherited{{{{}, -1, {}}, {{}, -1, {}}, {{}, 7, {}}}};
    EXPECT_EQ(ESM4::resolveOwnership(K::Other, inherited).mRank, 7);
}

TEST(ESM4CrimeRules, CellTrespassHasIndependentPublicGuardAndGlobalPresenceExemptions)
{
    using K = ESM4::CrimeOwnerKind;
    const ESM4::CellTrespassInput privateCell{K::Actor, 1, false, true, false, false, -1, 0};
    EXPECT_TRUE(ESM4::cellTreatsActorAsTrespasser(privateCell));
    auto changed = privateCell; changed.mMatchesActorBase = true;
    EXPECT_FALSE(ESM4::cellTreatsActorAsTrespasser(changed));
    changed = privateCell; changed.mActorIsGuard = true;
    EXPECT_FALSE(ESM4::cellTreatsActorAsTrespasser(changed));
    changed = privateCell; changed.mHasPermissionGlobal = true;
    EXPECT_FALSE(ESM4::cellTreatsActorAsTrespasser(changed));
    changed = privateCell; changed.mActorIsNpc = false;
    EXPECT_FALSE(ESM4::cellTreatsActorAsTrespasser(changed));
    changed = privateCell; changed.mOwnerKind = K::None;
    EXPECT_FALSE(ESM4::cellTreatsActorAsTrespasser(changed));
    for (unsigned flags : {0x20, 0x40, 0x60, 0x21, 0x41, 0xff})
    {
        changed = privateCell; changed.mCellFlags = flags;
        EXPECT_FALSE(ESM4::cellTreatsActorAsTrespasser(changed));
    }
    // This query itself has no interior-only or show-sky test.
    for (unsigned flags : {0, 1, 2, 4, 8, 0x10, 0x80, 0x9f})
    {
        changed = privateCell; changed.mCellFlags = flags;
        EXPECT_TRUE(ESM4::cellTreatsActorAsTrespasser(changed));
    }
}

TEST(ESM4CrimeRules, CellTrespassUsesSignedFactionRankAndMissingRankSentinel)
{
    using K = ESM4::CrimeOwnerKind;
    ESM4::CellTrespassInput input{K::Faction, 1, false, true, false, false, -1, -1};
    EXPECT_TRUE(ESM4::cellTreatsActorAsTrespasser(input)); // Required -1 becomes 0.
    input.mActorFactionRank = 0;
    EXPECT_FALSE(ESM4::cellTreatsActorAsTrespasser(input));
    input.mRequiredRank = 2;
    for (int rank : {-1, 0, 1})
    {
        input.mActorFactionRank = rank;
        EXPECT_TRUE(ESM4::cellTreatsActorAsTrespasser(input));
    }
    for (int rank : {2, 3, std::numeric_limits<int>::max()})
    {
        input.mActorFactionRank = rank;
        EXPECT_FALSE(ESM4::cellTreatsActorAsTrespasser(input));
    }
    input.mRequiredRank = -2; input.mActorFactionRank = -1;
    EXPECT_FALSE(ESM4::cellTreatsActorAsTrespasser(input));
    input.mRequiredRank = std::numeric_limits<int>::min();
    input.mActorFactionRank = std::numeric_limits<int>::min();
    EXPECT_FALSE(ESM4::cellTreatsActorAsTrespasser(input));
}

TEST(ESM4CrimeRules, CellTrespassRejectsInvalidOwnerAndInconsistentIdentityInputs)
{
    using K = ESM4::CrimeOwnerKind;
    ESM4::CellTrespassInput input{static_cast<K>(99), 0x20, true, true, false, false, -1, 0};
    EXPECT_THROW(ESM4::cellTreatsActorAsTrespasser(input), std::invalid_argument);
    input.mOwnerKind = K::Faction; input.mMatchesActorBase = true;
    EXPECT_THROW(ESM4::cellTreatsActorAsTrespasser(input), std::invalid_argument);
    input.mMatchesActorBase = false; input.mActorIsGuard = true; input.mActorIsNpc = false;
    EXPECT_THROW(ESM4::cellTreatsActorAsTrespasser(input), std::invalid_argument);
}

TEST(ESM4CrimeRules, TrespassExitRequiresPlayerTrespassTeleportAndLockData)
{
    const ESM4::DoorTrespassExitInput allowed{true, true, true, 0, true, 0};
    EXPECT_TRUE(ESM4::playerHasTrespassExitExemption(allowed));
    auto changed = allowed; changed.mPlayerTrespassing = false;
    EXPECT_FALSE(ESM4::playerHasTrespassExitExemption(changed));
    changed = allowed; changed.mHasTeleport = false;
    EXPECT_FALSE(ESM4::playerHasTrespassExitExemption(changed));
    changed = allowed; changed.mHasLockData = false;
    EXPECT_FALSE(ESM4::playerHasTrespassExitExemption(changed));
}

TEST(ESM4CrimeRules, TrespassExitRejectsExactlyLevelOneHundredAndPrivateInteriors)
{
    ESM4::DoorTrespassExitInput input{true, true, true, 100, true, 0};
    EXPECT_FALSE(ESM4::playerHasTrespassExitExemption(input));
    for (unsigned level : {0, 1, 99, 101, 255})
    {
        input.mLockLevel = level;
        EXPECT_TRUE(ESM4::playerHasTrespassExitExemption(input));
    }
    input.mLockLevel = 0;
    for (unsigned flags : {0, 0x20, 0x21, 0x40, 0x80, 0xff})
    {
        input.mDestinationFlags = flags;
        EXPECT_TRUE(ESM4::playerHasTrespassExitExemption(input));
    }
    // HandChanged alone does not supply the Public flag in this query.
    for (unsigned flags : {1, 0x41, 0x81, 0xc1})
    {
        input.mDestinationFlags = flags;
        EXPECT_FALSE(ESM4::playerHasTrespassExitExemption(input));
    }
    input.mHasDestinationCell = false;
    EXPECT_TRUE(ESM4::playerHasTrespassExitExemption(input));
}

TEST(ESM4CrimeRules, ReferenceOffLimitsSeparatesObjectsHorsesAndOtherActors)
{
    using K = ESM4::ReferenceAccessKind;
    ESM4::ReferenceAccessInput input;
    for (K kind : {K::Object, K::Horse})
    {
        input.mKind = kind;
        EXPECT_FALSE(ESM4::playerReferenceIsOffLimits(input));
        input.mHasOwner = true;
        EXPECT_TRUE(ESM4::playerReferenceIsOffLimits(input));
        input.mPlayerHasClaim = true;
        EXPECT_FALSE(ESM4::playerReferenceIsOffLimits(input));
        input.mPlayerHasClaim = false; input.mOwnerEvil = true;
        EXPECT_FALSE(ESM4::playerReferenceIsOffLimits(input));
        input = {};
    }
    input.mHasOwner = true;
    for (K kind : {K::Npc, K::Creature})
    {
        input.mKind = kind;
        EXPECT_FALSE(ESM4::playerReferenceIsOffLimits(input));
    }
}

TEST(ESM4CrimeRules, LivingNpcSneakActivationIsOffLimitsExceptEvilOwnerExemption)
{
    using K = ESM4::ReferenceAccessKind;
    ESM4::ReferenceAccessInput input;
    input.mKind = K::Npc; input.mPlayerSneaking = true;
    EXPECT_TRUE(ESM4::playerReferenceIsOffLimits(input));
    input.mDead = true;
    EXPECT_FALSE(ESM4::playerReferenceIsOffLimits(input));
    input.mDead = false; input.mHasOwner = true; input.mPlayerHasClaim = true;
    EXPECT_TRUE(ESM4::playerReferenceIsOffLimits(input));
    input.mOwnerEvil = true;
    EXPECT_FALSE(ESM4::playerReferenceIsOffLimits(input));
    input = {}; input.mKind = K::Creature; input.mPlayerSneaking = true;
    EXPECT_FALSE(ESM4::playerReferenceIsOffLimits(input));
}

TEST(ESM4CrimeRules, DoorOffLimitsDistinguishesDoorClaimAndDestinationClaim)
{
    using K = ESM4::ReferenceAccessKind;
    ESM4::ReferenceAccessInput input;
    input.mKind = K::Door; input.mDestination = {true, false, false, 1};
    EXPECT_TRUE(ESM4::playerReferenceIsOffLimits(input));
    input.mDestination.mPlayerHasClaim = true;
    EXPECT_FALSE(ESM4::playerReferenceIsOffLimits(input));
    input.mHasOwner = true;
    EXPECT_TRUE(ESM4::playerReferenceIsOffLimits(input)); // Destination claim does not claim the door.
    input.mPlayerHasClaim = true;
    EXPECT_FALSE(ESM4::playerReferenceIsOffLimits(input));
    input.mDestination.mPlayerHasClaim = false;
    EXPECT_TRUE(ESM4::playerReferenceIsOffLimits(input)); // Door claim does not claim the cell.
    input.mPlayerHasClaim = false; input.mDoorPermission = true;
    EXPECT_FALSE(ESM4::playerReferenceIsOffLimits(input));
    input.mDoorPermission = false; input.mDestination = {}; input.mDoorLocked = true;
    EXPECT_TRUE(ESM4::playerReferenceIsOffLimits(input));
    input.mDoorLocked = false;
    EXPECT_FALSE(ESM4::playerReferenceIsOffLimits(input));
}

TEST(ESM4CrimeRules, DoorOffLimitsHonorsPublicHandChangedExteriorAndEvilOwners)
{
    using K = ESM4::ReferenceAccessKind;
    ESM4::ReferenceAccessInput input;
    input.mKind = K::Door; input.mDestination.mHasOwner = true;
    for (unsigned flags : {0, 0x20, 0x21, 0x40, 0x41, 0x61, 0xff})
    {
        input.mDestination.mFlags = flags;
        EXPECT_FALSE(ESM4::playerReferenceIsOffLimits(input));
    }
    input.mDestination.mFlags = 1; input.mDestination.mOwnerEvil = true;
    EXPECT_FALSE(ESM4::playerReferenceIsOffLimits(input));
    input.mHasOwner = true; input.mDoorLocked = true;
    EXPECT_TRUE(ESM4::playerReferenceIsOffLimits(input));
    input.mOwnerEvil = true;
    EXPECT_FALSE(ESM4::playerReferenceIsOffLimits(input));
}

TEST(ESM4CrimeRules, ReferenceOffLimitsRejectsInvalidKindsAndOwnerClaims)
{
    ESM4::ReferenceAccessInput input;
    input.mKind = static_cast<ESM4::ReferenceAccessKind>(99);
    EXPECT_THROW(ESM4::playerReferenceIsOffLimits(input), std::invalid_argument);
    input = {}; input.mPlayerHasClaim = true;
    EXPECT_THROW(ESM4::playerReferenceIsOffLimits(input), std::invalid_argument);
    input = {}; input.mOwnerEvil = true;
    EXPECT_THROW(ESM4::playerReferenceIsOffLimits(input), std::invalid_argument);
    input = {}; input.mDestination.mPlayerHasClaim = true;
    EXPECT_THROW(ESM4::playerReferenceIsOffLimits(input), std::invalid_argument);
}

TEST(ESM4CrimeRules, ActorFactionPolicyRequiresAllEvilButAnySpecialCombat)
{
    const auto empty = ESM4::actorFactionCrimePolicy({});
    EXPECT_FALSE(empty.mEvil);
    EXPECT_FALSE(empty.mSpecialCombat);
    struct Row { std::vector<std::uint8_t> flags; bool evil; bool special; };
    const Row rows[] = {
        {{0}, false, false}, {{1}, false, false}, {{2}, true, false}, {{3}, true, false},
        {{4}, false, true}, {{6}, true, true}, {{7}, true, true},
        {{2, 2}, true, false}, {{2, 0}, false, false}, {{0, 2}, false, false},
        {{2, 6}, true, true}, {{6, 2}, true, true}, {{6, 0}, false, true},
        {{0, 6}, false, true}, {{2, 4, 2}, false, true}, {{3, 7, 3}, true, true},
    };
    for (const auto& row : rows)
    {
        const auto result = ESM4::actorFactionCrimePolicy(row.flags);
        EXPECT_EQ(result.mEvil, row.evil);
        EXPECT_EQ(result.mSpecialCombat, row.special);
    }
}

TEST(ESM4CrimeRules, FactionPolicyPreservesIndependentFlagsAndOwnerAccessConsequences)
{
    // Hidden and unrelated bits do not change either native mask query.
    for (unsigned flags = 0; flags < 256; ++flags)
    {
        const std::array<std::uint8_t, 1> entries{static_cast<std::uint8_t>(flags)};
        const auto policy = ESM4::actorFactionCrimePolicy(entries);
        EXPECT_EQ(policy.mEvil, (flags & 2) != 0);
        EXPECT_EQ(policy.mSpecialCombat, (flags & 4) != 0);
    }
    ESM4::ReferenceAccessInput item;
    item.mHasOwner = true;
    const std::array<std::uint8_t, 2> mixedOwner{2, 0};
    item.mOwnerEvil = ESM4::actorFactionCrimePolicy(mixedOwner).mEvil;
    EXPECT_TRUE(ESM4::playerReferenceIsOffLimits(item));
    const std::array<std::uint8_t, 2> evilOwner{2, 6};
    item.mOwnerEvil = ESM4::actorFactionCrimePolicy(evilOwner).mEvil;
    EXPECT_FALSE(ESM4::playerReferenceIsOffLimits(item));
}

TEST(ESM4CrimeRules, AttackAlarmChecksRaceGuardAndSpecialCombatForDistinctActors)
{
    for (auto offense : {ESM4::CrimeOffense::Assault, ESM4::CrimeOffense::Murder})
    {
        ESM4::AttackCrimeAlarmInput input;
        input.mOffense = offense;
        EXPECT_TRUE(ESM4::attackCrimeAlarmEligible(input));
        input.mVictimPlayableRace = false;
        EXPECT_FALSE(ESM4::attackCrimeAlarmEligible(input));
        input.mVictimGuard = true;
        EXPECT_TRUE(ESM4::attackCrimeAlarmEligible(input));
        input.mOffenderPlayableRace = false;
        EXPECT_FALSE(ESM4::attackCrimeAlarmEligible(input));
        input.mOffenderPlayableRace = true; input.mOffenderGuard = true;
        EXPECT_FALSE(ESM4::attackCrimeAlarmEligible(input));
        input.mOffenderGuard = false; input.mOffenderNpc = false;
        EXPECT_FALSE(ESM4::attackCrimeAlarmEligible(input));
        input.mOffenderNpc = true;
        for (bool victim : {false, true})
            for (bool offender : {false, true})
            {
                input.mVictimSpecialCombat = victim; input.mOffenderSpecialCombat = offender;
                EXPECT_EQ(ESM4::attackCrimeAlarmEligible(input), !(victim && offender));
            }
    }
}

TEST(ESM4CrimeRules, AttackAlarmTrespassUsesVictimForAssaultAndOffenderForMurder)
{
    ESM4::AttackCrimeAlarmInput input;
    for (bool victim : {false, true})
        for (bool offender : {false, true})
        {
            input.mVictimTrespassing = victim; input.mOffenderTrespassing = offender;
            input.mOffense = ESM4::CrimeOffense::Assault;
            EXPECT_EQ(ESM4::attackCrimeAlarmEligible(input), !victim);
            input.mOffense = ESM4::CrimeOffense::Murder;
            EXPECT_EQ(ESM4::attackCrimeAlarmEligible(input), !offender);
        }
}

TEST(ESM4CrimeRules, AttackAlarmJailAndExactSneakExemptionsPreservePlayerDistinction)
{
    for (auto offense : {ESM4::CrimeOffense::Assault, ESM4::CrimeOffense::Murder})
        for (bool player : {false, true})
        {
            ESM4::AttackCrimeAlarmInput input;
            input.mOffense = offense; input.mOffenderPlayer = player;
            for (int days : {-1, 0, 1, std::numeric_limits<int>::max()})
                for (bool pursuit : {false, true})
                {
                    input.mPlayerJailDays = days; input.mPlayerHasCombatOrPursuit = pursuit;
                    EXPECT_EQ(ESM4::attackCrimeAlarmEligible(input), !(player && days > 0 && !pursuit));
                }
            input.mPlayerJailDays = 0;
            for (int sneak : {-1, 0, 99, 100, 101, std::numeric_limits<int>::max()})
                for (bool sneaking : {false, true})
                {
                    input.mOffenderSneak = sneak; input.mOffenderSneaking = sneaking;
                    EXPECT_EQ(ESM4::attackCrimeAlarmEligible(input), player || sneak != 100 || !sneaking);
                }
        }
    ESM4::AttackCrimeAlarmInput input;
    input.mVictimPlayableRace = false;
    for (auto offense : {ESM4::CrimeOffense::Theft, ESM4::CrimeOffense::Pickpocket,
             ESM4::CrimeOffense::Trespass, ESM4::CrimeOffense::HorseTheft, ESM4::CrimeOffense::JailBreak,
             static_cast<ESM4::CrimeOffense>(-1)})
    {
        input.mOffense = offense;
        EXPECT_THROW(ESM4::attackCrimeAlarmEligible(input), std::invalid_argument);
    }
}

TEST(ESM4CrimeRules, TrespassWarningsRequireCountAndTimerBeforeEscalation)
{
    using Action = ESM4::TrespassWarningAction;
    const ESM4::TrespassWarningSettings settings{30};
    for (int count : {0, 1, 2, std::numeric_limits<int>::max()})
        for (float timer : {0.f, -0.f, std::numeric_limits<float>::denorm_min(), 1.f})
        {
            ESM4::TrespassWarningInput input{true, false, count, timer, 0};
            const auto result = ESM4::advanceTrespassWarning(input, settings);
            EXPECT_EQ(result.mAction, timer > 0 ? Action::Wait : count > 1 ? Action::Escalate : Action::Warn);
            input.mCellOffLimits = true;
            EXPECT_EQ(ESM4::advanceTrespassWarning(input, settings).mAction, Action::Escalate);
            input.mTargetTrespassing = false;
            EXPECT_EQ(ESM4::advanceTrespassWarning(input, settings).mAction, Action::Leave);
        }
}

TEST(ESM4CrimeRules, TrespassTimerUsesDoubleFrameStepAndDefersExpiryAction)
{
    using Action = ESM4::TrespassWarningAction;
    const ESM4::TrespassWarningSettings settings{30};
    ESM4::TrespassWarningInput input{true, false, 2, 29, .5f};
    auto result = ESM4::advanceTrespassWarning(input, settings);
    EXPECT_EQ(result.mAction, Action::Wait);
    EXPECT_EQ(result.mTimer, 30); // Equality does not expire the native timer.
    input.mTimer = result.mTimer;
    result = ESM4::advanceTrespassWarning(input, settings);
    EXPECT_EQ(result.mAction, Action::Wait);
    EXPECT_EQ(result.mTimer, 0);
    input.mTimer = result.mTimer;
    EXPECT_EQ(ESM4::advanceTrespassWarning(input, settings).mAction, Action::Escalate);
    input.mWarningCount = 1;
    EXPECT_EQ(ESM4::advanceTrespassWarning(input, settings).mAction, Action::Warn);
    input.mTimer = 1; input.mFrameDuration = .25f;
    EXPECT_EQ(ESM4::advanceTrespassWarning(input, settings).mTimer, 1.5f);
    input.mFrameDuration = 0;
    for (float limit : {0.f, std::numeric_limits<float>::denorm_min(), 1.f, 10.f, 30.f})
    {
        input.mTimer = std::nextafter(limit, std::numeric_limits<float>::infinity());
        EXPECT_EQ(ESM4::advanceTrespassWarning(input, {limit}).mTimer, 0);
        if (limit > 0)
        {
            input.mTimer = limit;
            EXPECT_EQ(ESM4::advanceTrespassWarning(input, {limit}).mTimer, limit);
            input.mTimer = std::nextafter(limit, 0.f);
            EXPECT_EQ(ESM4::advanceTrespassWarning(input, {limit}).mTimer, input.mTimer);
        }
    }
}

TEST(ESM4CrimeRules, TrespassWarningRejectsInvalidStateAndArithmeticOverflow)
{
    ESM4::TrespassWarningInput input{true, false, 0, 1, .1f};
    for (float invalid : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        auto bad = input; bad.mTimer = invalid;
        EXPECT_THROW(ESM4::advanceTrespassWarning(bad, {30}), std::invalid_argument);
        bad = input; bad.mFrameDuration = invalid;
        EXPECT_THROW(ESM4::advanceTrespassWarning(bad, {30}), std::invalid_argument);
        EXPECT_THROW(ESM4::advanceTrespassWarning(input, {invalid}), std::invalid_argument);
    }
    input.mWarningCount = -1;
    EXPECT_THROW(ESM4::advanceTrespassWarning(input, {30}), std::invalid_argument);
    input.mWarningCount = 0; input.mFrameDuration = std::numeric_limits<float>::max();
    EXPECT_THROW(ESM4::advanceTrespassWarning(input, {30}), std::invalid_argument);
    input.mFrameDuration = std::numeric_limits<float>::max() / 2;
    input.mTimer = std::numeric_limits<float>::max();
    EXPECT_THROW(ESM4::advanceTrespassWarning(input, {30}), std::invalid_argument);
}

TEST(ESM4CrimeRules, CrimeWitnessCandidatesUseDifferentOffenderAndWitnessLifeGates)
{
    ESM4::CrimeWitnessCandidateInput input;
    input.mDetection = 1;
    for (int offender = 0; offender <= 6; ++offender)
        for (int candidate = 0; candidate <= 6; ++candidate)
        {
            input.mOffenderLifeState = offender; input.mCandidateLifeState = candidate;
            EXPECT_EQ(ESM4::crimeWitnessCandidate(input), offender != 1 && offender != 2
                && candidate != 1 && candidate != 2 && candidate != 3 && candidate != 6);
        }
    input.mOffenderLifeState = 2; input.mCandidateLifeState = 0; input.mOffenderPresent = false;
    EXPECT_TRUE(ESM4::crimeWitnessCandidate(input));
}

TEST(ESM4CrimeRules, CrimeWitnessCandidateRequiresPositiveSignedDetectionAndEligibleIdentity)
{
    ESM4::CrimeWitnessCandidateInput input;
    for (int detection : {std::numeric_limits<int>::min(), -100, -1, 0, 1, 100, std::numeric_limits<int>::max()})
    {
        input.mDetection = detection;
        EXPECT_EQ(ESM4::crimeWitnessCandidate(input), detection > 0);
    }
    input.mDetection = 1;
    input.mCandidateParalyzed = true;
    EXPECT_FALSE(ESM4::crimeWitnessCandidate(input));
    input.mCandidateParalyzed = false; input.mCandidateIsOffender = true;
    EXPECT_FALSE(ESM4::crimeWitnessCandidate(input));
    input.mCandidateIsOffender = false; input.mCandidateActor = false;
    EXPECT_FALSE(ESM4::crimeWitnessCandidate(input));
}

TEST(ESM4CrimeRules, CrimeWitnessEnumerationUsesNativeDisabledAndDeletedFlagMasks)
{
    ESM4::CrimeWitnessCandidateInput input;
    input.mDetection = 1;
    for (std::uint32_t flags : {0u, 0x20u, 0x800u, 0x820u, 0x400u, 0xffffffffu})
    {
        input.mOffenderFlags = flags;
        EXPECT_EQ(ESM4::crimeWitnessCandidate(input), (flags & 0x820u) == 0);
        input.mOffenderPresent = false;
        EXPECT_TRUE(ESM4::crimeWitnessCandidate(input));
        input.mOffenderPresent = true; input.mOffenderFlags = 0;
        input.mCandidateFlags = flags;
        // The enumeration's candidate branch checks disabled only. The world
        // collection supplies current candidates; do not invent another filter.
        EXPECT_EQ(ESM4::crimeWitnessCandidate(input), (flags & 0x800u) == 0);
        input.mCandidateFlags = 0;
    }
}

TEST(ESM4CrimeRules, AlarmReachUsesCellIdentityAndExteriorWorldspace)
{
    const auto cellA = ESM::FormKey::content("fixture.esm", 1);
    const auto cellB = ESM::FormKey::content("fixture.esm", 2);
    const auto world = ESM::FormKey::content("fixture.esm", 3);
    const auto otherWorld = ESM::FormKey::content("fixture.esm", 4);
    const ESM4::CrimeAlarmLocation interiorA{cellA, {}, true}, interiorB{cellB, {}, true};
    EXPECT_TRUE(ESM4::crimeAlarmReachesLocation(interiorA, interiorA, 4000, {}, {4000}));
    EXPECT_FALSE(ESM4::crimeAlarmReachesLocation(interiorA, interiorB, 0, {}, {4000}));
    EXPECT_TRUE(ESM4::crimeAlarmReachesLocation({cellA, world, false}, {cellB, world, false}, 1, {}, {4000}));
    EXPECT_FALSE(ESM4::crimeAlarmReachesLocation({cellA, world, false}, {cellB, otherWorld, false}, 0, {}, {4000}));
    EXPECT_FALSE(ESM4::crimeAlarmReachesLocation({}, {}, 0, {}, {4000}));
    EXPECT_TRUE(ESM4::crimeAlarmReachesLocation({}, {cellA, {}, false}, 0, {}, {4000}));
}

TEST(ESM4CrimeRules, AlarmDoorsBridgeSpacesButDoNotRetryDirectDistanceFailures)
{
    const auto cellA = ESM::FormKey::content("fixture.esm", 1);
    const auto cellB = ESM::FormKey::content("fixture.esm", 2);
    const auto world = ESM::FormKey::content("fixture.esm", 3);
    const ESM4::CrimeAlarmLocation interiorA{cellA, {}, true}, interiorB{cellB, {}, true};
    std::array<ESM4::CrimeAlarmDoorDestination, 2> doors{{{interiorA, 0}, {interiorB, 4000}}};
    EXPECT_TRUE(ESM4::crimeAlarmReachesLocation(interiorA, interiorB, 100000, doors, {4000}));
    doors[1].mRecipientDistance = std::nextafter(4000.f, 5000.f);
    EXPECT_FALSE(ESM4::crimeAlarmReachesLocation(interiorA, interiorB, 0, doors, {4000}));
    doors[1].mRecipientDistance = 0;
    EXPECT_FALSE(ESM4::crimeAlarmReachesLocation(interiorB, interiorB, 4001, doors, {4000}));
    doors[1] = {{{}, world, false}, 4000};
    EXPECT_TRUE(ESM4::crimeAlarmReachesLocation(interiorA, {cellB, world, false}, 100000, doors, {4000}));
    doors[1] = {{cellA, world, false}, 0};
    EXPECT_FALSE(ESM4::crimeAlarmReachesLocation(interiorA, {cellB, world, false}, 0, doors, {4000}));
    doors[1] = {{{}, {}, false}, 0};
    EXPECT_TRUE(ESM4::crimeAlarmReachesLocation(interiorA, {}, 100000, doors, {4000}));
}

TEST(ESM4CrimeRules, AlarmRadiusPreservesIntegerPrecisionAndRejectsMalformedInputs)
{
    const ESM4::CrimeAlarmLocation location{ESM::FormKey::content("fixture.esm", 1), {}, true};
    EXPECT_TRUE(ESM4::crimeAlarmReachesLocation(location, location, -0.f, {}, {0}));
    EXPECT_FALSE(ESM4::crimeAlarmReachesLocation(location, location,
        std::numeric_limits<float>::denorm_min(), {}, {0}));
    EXPECT_TRUE(ESM4::crimeAlarmReachesLocation(location, location, 16777216.f, {}, {16777217}));
    EXPECT_FALSE(ESM4::crimeAlarmReachesLocation(location, location, 16777218.f, {}, {16777217}));
    EXPECT_FALSE(ESM4::crimeAlarmReachesLocation(location, location, 2147483648.f, {}, {2147483647}));
    EXPECT_THROW(ESM4::crimeAlarmReachesLocation(location, location, 0, {}, {-1}), std::invalid_argument);
    EXPECT_THROW(ESM4::crimeAlarmReachesLocation({{}, {}, true}, location, 0, {}, {1}), std::invalid_argument);
    for (float invalid : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        EXPECT_THROW(ESM4::crimeAlarmReachesLocation(location, location, invalid, {}, {1}), std::invalid_argument);
        const std::array<ESM4::CrimeAlarmDoorDestination, 1> doors{{{location, invalid}}};
        EXPECT_THROW(ESM4::crimeAlarmReachesLocation(location, location, 0, doors, {1}), std::invalid_argument);
    }
}

TEST(ESM4CrimeRules, AlarmResponseSkipsExistingAlarmCombatAndSleepingButNotSleepTransitions)
{
    ESM4::CrimeAlarmRecipientInput input;
    input.mFightScore = 1;
    EXPECT_TRUE(ESM4::crimeAlarmRecipientResponds(input));
    for (unsigned state = 0; state <= 255; ++state)
    {
        input.mSitSleepState = static_cast<std::uint8_t>(state);
        EXPECT_EQ(ESM4::crimeAlarmRecipientResponds(input), state != 9);
    }
    input.mSitSleepState = 0;
    input.mActor = false;
    EXPECT_FALSE(ESM4::crimeAlarmRecipientResponds(input));
    input.mActor = true;
    input.mHasAlarmPackage = true;
    EXPECT_FALSE(ESM4::crimeAlarmRecipientResponds(input));
    input.mHasAlarmPackage = false;
    input.mInCombat = true;
    EXPECT_FALSE(ESM4::crimeAlarmRecipientResponds(input));
}

TEST(ESM4CrimeRules, AlarmResponseDistinguishesGuardsFromOtherRecipients)
{
    ESM4::CrimeAlarmRecipientInput input;
    for (bool guard : {false, true})
        for (bool suppressed : {false, true})
            for (bool evil : {false, true})
                for (std::int32_t score : {std::numeric_limits<std::int32_t>::min(), -1, 0, 1, 100,
                         std::numeric_limits<std::int32_t>::max()})
                {
                    input.mGuard = guard;
                    input.mIncidentSuppressesGuards = suppressed;
                    input.mEmitterEvil = evil;
                    input.mFightScore = score;
                    EXPECT_EQ(ESM4::crimeAlarmRecipientResponds(input), guard ? !suppressed && !evil : score > 0);
                }
    input.mGuard = true;
    input.mIncidentSuppressesGuards = false;
    input.mEmitterEvil = false;
    input.mFightScore = -1;
    input.mHasAlarmPackage = true;
    EXPECT_FALSE(ESM4::crimeAlarmRecipientResponds(input));
    input.mHasAlarmPackage = false;
    input.mSitSleepState = 9;
    EXPECT_FALSE(ESM4::crimeAlarmRecipientResponds(input));
}

TEST(ESM4CrimeRules, AdmittedReportsRequireNpcOffenderUnreportedIncidentAndResponsibleOrGuardReporter)
{
    for (bool present : {false, true})
        for (bool npc : {false, true})
            for (bool reported : {false, true})
                for (bool guard : {false, true})
                    for (std::int32_t responsibility : {std::numeric_limits<std::int32_t>::min(), 0, 99, 100, 101,
                             std::numeric_limits<std::int32_t>::max()})
                        EXPECT_EQ(ESM4::crimeReportEligible({present, npc, reported, responsibility, guard}),
                            present && npc && !reported && (guard || responsibility >= 100));
}

TEST(ESM4CrimeRules, InfamyUsesStrictIncrementGateAndSingleThresholdSubtraction)
{
    const ESM4::CrimeInfamySettings settings{500};
    ESM4::CrimeInfamyState state{3, 499};
    for (float delta : {-100.f, 0.f, std::nextafter(1.f, 0.f), 1.f})
    {
        const auto next = ESM4::advanceCrimeInfamy(state, delta, settings);
        EXPECT_EQ(next.mInfamy, 3);
        EXPECT_EQ(next.mAccumulatedBounty, 499);
    }
    auto next = ESM4::advanceCrimeInfamy(state, std::nextafter(1.f, 2.f), settings);
    EXPECT_EQ(next.mInfamy, 4);
    EXPECT_EQ(next.mAccumulatedBounty, 0);
    next = ESM4::advanceCrimeInfamy({0, 0}, 1500, settings);
    EXPECT_EQ(next.mInfamy, 1); // No loop awarding three points.
    EXPECT_EQ(next.mAccumulatedBounty, 1000);
    next = ESM4::advanceCrimeInfamy(next, 1, settings);
    EXPECT_EQ(next.mInfamy, 1);
    EXPECT_EQ(next.mAccumulatedBounty, 1000);
    next = ESM4::advanceCrimeInfamy(next, 2, settings);
    EXPECT_EQ(next.mInfamy, 2);
    EXPECT_EQ(next.mAccumulatedBounty, 502);
}

TEST(ESM4CrimeRules, InfamyThresholdStoresIntegerAccumulatorAsFloatBeforeComparison)
{
    const auto below = ESM4::advanceCrimeInfamy({0, 497}, std::nextafter(3.f, 0.f), {500});
    EXPECT_EQ(below.mInfamy, 0);
    EXPECT_EQ(below.mAccumulatedBounty, 499);
    const auto equal = ESM4::advanceCrimeInfamy({0, 497}, 3, {500});
    EXPECT_EQ(equal.mInfamy, 1);
    EXPECT_EQ(equal.mAccumulatedBounty, 0);
    const auto fractional = ESM4::advanceCrimeInfamy({0, 497}, 4, {500.5f});
    EXPECT_EQ(fractional.mInfamy, 1);
    EXPECT_EQ(fractional.mAccumulatedBounty, 0); // Remainder truncates.
    const auto rounded = ESM4::advanceCrimeInfamy({0, 16777213}, 6, {16777220});
    EXPECT_EQ(rounded.mInfamy, 1); // Integer 16777219 stores as float 16777220.
    EXPECT_EQ(rounded.mAccumulatedBounty, 0);
}

TEST(ESM4CrimeRules, InfamyRejectsMalformedStateSettingsAndIntegerOverflow)
{
    EXPECT_THROW(ESM4::advanceCrimeInfamy({-1, 0}, 2, {500}), std::invalid_argument);
    EXPECT_THROW(ESM4::advanceCrimeInfamy({0, -1}, 2, {500}), std::invalid_argument);
    for (float value : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        EXPECT_THROW(ESM4::advanceCrimeInfamy({0, 0}, value, {500}), std::invalid_argument);
        EXPECT_THROW(ESM4::advanceCrimeInfamy({0, 0}, 2, {value}), std::invalid_argument);
    }
    EXPECT_THROW(ESM4::advanceCrimeInfamy({0, 0}, 2, {0}), std::invalid_argument);
    EXPECT_THROW(ESM4::advanceCrimeInfamy({0, 0}, 2, {-1}), std::invalid_argument);
    EXPECT_THROW(ESM4::advanceCrimeInfamy({0, 0}, 2147483648.f, {500}), std::overflow_error);
    EXPECT_THROW(ESM4::advanceCrimeInfamy({0, 2147483647}, 2, {500}), std::overflow_error);
    EXPECT_THROW(ESM4::advanceCrimeInfamy({2147483647, 499}, 2, {500}), std::overflow_error);
}
