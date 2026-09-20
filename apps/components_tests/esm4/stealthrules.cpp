#include <gtest/gtest.h>

#include <components/esm4/combatsettings.hpp>
#include <components/esm4/loadgmst.hpp>

#include <cmath>
#include <limits>
#include <random>
#include <stdexcept>

TEST(ESM4StealthRules, SneakMultipliersUseBaseRankAndNativeWeaponEligibility)
{
    const auto settings = ESM4::buildSneakAttackSettings({});
    const auto mastery = ESM4::buildCombatMasterySettings({});
    for (int skill : {-1, 0, 1, 24, 25, 26, 49, 50, 51, 74, 75, 76, 99, 100, 101})
        for (int weapon = -1; weapon <= 5; ++weapon)
        {
            const auto result = ESM4::sneakAttack({skill, weapon, 0, true, true, false, false}, settings, mastery);
            const float expected = weapon == 5 ? (skill < 25 ? 6 : 8)
                : weapon == -1 || weapon == 0 || weapon == 2 ? (skill < 25 ? 4 : 6) : 1;
            EXPECT_EQ(result.mMultiplier, expected);
            EXPECT_EQ(result.mBypassArmor, skill >= 100 && expected > 1);
            EXPECT_EQ(result.mBypassBlock, result.mBypassArmor);
        }
    EXPECT_EQ(ESM4::sneakAttack({10, 0, 0, true, true, false, false}, settings, {{10,20,30,40}}).mMultiplier, 6);
    EXPECT_TRUE(ESM4::sneakAttack({40, 0, 0, true, true, false, false}, settings, {{10,20,30,40}}).mBypassArmor);
}

TEST(ESM4StealthRules, SneakEligibilityUsesVictimAwarenessCombatThresholdAndPosture)
{
    const auto settings = ESM4::buildSneakAttackSettings({});
    const auto mastery = ESM4::buildCombatMasterySettings({});
    for (int detection : {std::numeric_limits<int>::min(), -51, -50, -49, -1, 0, 1, 100})
        for (bool combat : {false, true})
            for (bool npc : {false, true})
                for (bool sneak : {false, true})
                    for (bool swim : {false, true})
                    {
                        const auto result = ESM4::sneakAttack({100, 0, detection, npc, sneak, swim, combat}, settings, mastery);
                        const bool eligible = npc && sneak && !swim && detection <= 0 && (!combat || detection <= -50);
                        EXPECT_EQ(result.mMultiplier, eligible ? 6 : 1);
                        EXPECT_EQ(result.mBypassArmor, eligible);
                        EXPECT_EQ(result.mBypassBlock, eligible);
                    }
}

TEST(ESM4StealthRules, ArmorBypassRequiresStrictlyGreaterThanOneMultiplier)
{
    auto settings = ESM4::buildSneakAttackSettings({});
    const auto mastery = ESM4::buildCombatMasterySettings({});
    for (float mult : {0.f, std::nextafter(1.f, 0.f), 1.f, std::nextafter(1.f, 2.f), 2.f})
    {
        settings.mMeleeMultipliers[4] = mult;
        const auto result = ESM4::sneakAttack({100, 0, 0, true, true, false, false}, settings, mastery);
        EXPECT_EQ(result.mMultiplier, mult);
        EXPECT_EQ(result.mBypassArmor, mult > 1);
    }
}

TEST(ESM4StealthRules, SettingsUseEveryRankOverrideAndRejectMalformedInput)
{
    const auto mastery = ESM4::buildCombatMasterySettings({});
    const auto defaults = ESM4::buildSneakAttackSettings({});
    const std::array<std::string, 5> ranks{"Novice", "Apprentice", "Journeyman", "Expert", "Master"};
    const std::array<int, 5> skills{0,25,50,75,100};
    ESM4::GameSetting value{};
    const std::array<const ESM4::GameSetting*, 1> values{&value};
    for (std::size_t i = 0; i < ranks.size(); ++i)
        for (bool bow : {false, true})
        {
            value.mEditorId = std::string("fPerkSneakAttack") + (bow ? "Marksman" : "Melee") + ranks[i] + "Mult";
            value.mData = 2.f + float(i);
            const auto settings = ESM4::buildSneakAttackSettings(values);
            EXPECT_EQ(ESM4::sneakAttack({skills[i], bow ? 5 : 0, 0, true, true, false, false}, settings, mastery).mMultiplier, 2.f + float(i));
            value.mData = std::int32_t{2};
            EXPECT_THROW(ESM4::buildSneakAttackSettings(values), std::invalid_argument);
        }
    value.mEditorId = "iAICombatMinDetection"; value.mData = std::int32_t{-20};
    EXPECT_EQ(ESM4::buildSneakAttackSettings(values).mCombatMinimumDetection, -20);
    for (int bad : {-2, 6, 100})
        EXPECT_THROW(ESM4::sneakAttack({100, bad, 0, false, false, false, false}, defaults, mastery), std::invalid_argument);
    for (float bad : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        auto invalid = defaults; invalid.mMeleeMultipliers[0] = bad;
        EXPECT_THROW(ESM4::sneakAttack({100, 0, 0, false, false, false, false}, invalid, mastery), std::invalid_argument);
        invalid = defaults; invalid.mMarksmanMultipliers[4] = bad;
        EXPECT_THROW(ESM4::validateSneakAttackSettings(invalid), std::invalid_argument);
    }
}

TEST(ESM4StealthRules, PickpocketChanceMatchesOriginalStoredArithmetic)
{
    const ESM4::PickpocketSettings installed{40, .6f, 0, -.6f, 0, -.1f, 5, 85};
    struct Case { int mActor; int mTarget; float mAmount; int mExpected; };
    const std::array cases{
        Case{0, 0, 0.0f, 40},
        Case{50, 50, 0.0f, 39},
        Case{60, 20, 100.0f, 54},
        Case{100, 0, 0.0f, 85},
        Case{0, 100, 0.0f, 5},
        Case{24, 25, 1.0f, 39},
        Case{25, 24, 1.0f, 40},
        Case{100, 100, 1.0f, 39},
        Case{100, 100, 10.0f, 38},
        Case{100, 100, 100.0f, 29},
        Case{50, 50, 1000.0f, 5},
        Case{99, 1, 0.5f, 85},
    };
    for (const auto& c : cases)
        EXPECT_EQ(ESM4::pickpocketChance(c.mActor, c.mTarget, c.mAmount, installed), c.mExpected);
    // No final float store before integer conversion: 100 - 60.0000038 - 1
    // truncates to 38, rather than rounding the sum to 39 first.
    auto exact = installed; exact.mActorSkillMultiplier = 1; exact.mTargetSkillMultiplier = -1;
    exact.mActorSkillBase = 0; exact.mAmountMultiplier = -1;
    EXPECT_EQ(ESM4::pickpocketChance(50, 0, std::nextafter(1.f, 0.f), exact), 49);
    EXPECT_EQ(ESM4::pickpocketChance(50, 0, 1, exact), 49);
    EXPECT_EQ(ESM4::pickpocketChance(50, 0, std::nextafter(1.f, 2.f), exact), 48);
    exact.mMinimumChance = 5.9f; exact.mMaximumChance = 85.9f;
    EXPECT_EQ(ESM4::pickpocketChance(0, 100, 0, exact), 5);
    EXPECT_EQ(ESM4::pickpocketChance(100, 0, 0, exact), 85);
}

TEST(ESM4StealthRules, PickpocketAmountUsesItemValueTimesCountWithCheckedConversion)
{
    EXPECT_EQ(ESM4::pickpocketAmount(100, 3), 300);
    EXPECT_EQ(ESM4::pickpocketAmount(0, 100), 0);
    EXPECT_EQ(ESM4::pickpocketAmount(100, 0), 0);
    EXPECT_EQ(ESM4::pickpocketAmount(16777217, 1), 16777216.f);
    EXPECT_THROW(ESM4::pickpocketAmount(-1, 1), std::invalid_argument);
    EXPECT_THROW(ESM4::pickpocketAmount(std::numeric_limits<int>::max(), 2), std::invalid_argument);
    EXPECT_THROW(ESM4::pickpocketAmount(1, std::numeric_limits<unsigned>::max()), std::invalid_argument);
}

TEST(ESM4StealthRules, PickpocketTransferAndUntouchedExitUseDifferentRollBoundaries)
{
    for (int chance : {0, 1, 5, 40, 75, 85, 99, 100})
        for (unsigned draw = 0; draw < 100; ++draw)
        {
            EXPECT_EQ(ESM4::pickpocketCheckSucceeds(ESM4::PickpocketCheck::Transfer, chance, draw), int(draw) < chance);
            EXPECT_EQ(ESM4::pickpocketCheckSucceeds(ESM4::PickpocketCheck::UntouchedMenuExit, chance, draw), int(draw) <= chance);
        }
    EXPECT_THROW(ESM4::pickpocketCheckSucceeds(static_cast<ESM4::PickpocketCheck>(99), 50, 0), std::invalid_argument);
    EXPECT_THROW(ESM4::pickpocketCheckSucceeds(ESM4::PickpocketCheck::Transfer, 101, 0), std::invalid_argument);
    EXPECT_THROW(ESM4::pickpocketCheckSucceeds(ESM4::PickpocketCheck::Transfer, -1, 0), std::invalid_argument);
    EXPECT_THROW(ESM4::pickpocketCheckSucceeds(ESM4::PickpocketCheck::Transfer, 50, 100), std::invalid_argument);
    std::mt19937 rng(0x4d15cc);
    unsigned transfers = 0, exits = 0;
    for (unsigned i = 0; i < 100000; ++i)
    {
        const unsigned draw = rng() % 100;
        transfers += ESM4::pickpocketCheckSucceeds(ESM4::PickpocketCheck::Transfer, 85, draw);
        exits += ESM4::pickpocketCheckSucceeds(ESM4::PickpocketCheck::UntouchedMenuExit, 85, draw);
    }
    EXPECT_NEAR(transfers, 85000, 600); EXPECT_NEAR(exits, 86000, 600);
}

TEST(ESM4StealthRules, PickpocketSettingsBindAllFactorsAndRejectMalformedValues)
{
    const auto settings = ESM4::buildPickpocketSettings({});
    EXPECT_EQ(settings.mActorSkillBase, 0); EXPECT_EQ(settings.mActorSkillMultiplier, 1);
    EXPECT_EQ(settings.mTargetSkillBase, 0); EXPECT_EQ(settings.mTargetSkillMultiplier, -1);
    EXPECT_EQ(settings.mAmountBase, 0); EXPECT_EQ(settings.mAmountMultiplier, -3);
    EXPECT_EQ(settings.mMinimumChance, 5); EXPECT_EQ(settings.mMaximumChance, 75);
    struct Binding { const char* mName; float ESM4::PickpocketSettings::* mMember; float mValue; };
    const std::array bindings{
        Binding{"fPickPocketActorSkillBase", &ESM4::PickpocketSettings::mActorSkillBase, 40},
        Binding{"fPickPocketActorSkillMult", &ESM4::PickpocketSettings::mActorSkillMultiplier, .6f},
        Binding{"fPickPocketTargetSkillBase", &ESM4::PickpocketSettings::mTargetSkillBase, 1},
        Binding{"fPickPocketTargetSkillMult", &ESM4::PickpocketSettings::mTargetSkillMultiplier, -.6f},
        Binding{"fPickPocketAmountBase", &ESM4::PickpocketSettings::mAmountBase, 1},
        Binding{"fPickPocketAmountMult", &ESM4::PickpocketSettings::mAmountMultiplier, -.1f},
        Binding{"fPickPocketMinChance", &ESM4::PickpocketSettings::mMinimumChance, 1},
        Binding{"fPickPocketMaxChance", &ESM4::PickpocketSettings::mMaximumChance, 85},
    };
    ESM4::GameSetting value{};
    const std::array<const ESM4::GameSetting*, 1> values{&value};
    for (const auto& binding : bindings)
    {
        value.mEditorId = binding.mName; value.mData = binding.mValue;
        EXPECT_EQ(ESM4::buildPickpocketSettings(values).*binding.mMember, binding.mValue);
        value.mData = std::int32_t{0};
        EXPECT_THROW(ESM4::buildPickpocketSettings(values), std::invalid_argument);
        auto invalid = settings; invalid.*binding.mMember = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(ESM4::pickpocketChance(50, 50, 0, invalid), std::invalid_argument);
    }
    for (float bad : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        EXPECT_THROW(ESM4::pickpocketChance(50, 50, bad, settings), std::invalid_argument);
    auto invalid = settings; invalid.mMinimumChance = 80;
    EXPECT_THROW(ESM4::pickpocketChance(0, 0, 0, invalid), std::invalid_argument);
    invalid = settings; invalid.mActorSkillMultiplier = std::numeric_limits<float>::max();
    EXPECT_THROW(ESM4::pickpocketChance(100, 0, 0, invalid), std::invalid_argument);
    invalid = settings; invalid.mActorSkillBase = float(std::numeric_limits<int>::max());
    EXPECT_THROW(ESM4::pickpocketChance(0, 0, 0, invalid), std::invalid_argument);
}

TEST(ESM4StealthRules, TakingPickpocketItemsHidesWornBoundAndNonPlayableButNotQuestItems)
{
    using D = ESM4::PickpocketItemDecision;
    using R = ESM4::PickpocketDirection;
    const ESM4::PickpocketItemInput valid{R::Take, true, false, false, false, false, 10};
    EXPECT_EQ(ESM4::pickpocketItemDecision(valid), D::Allowed);
    auto input = valid; input.mAnyInstanceWorn = true;
    EXPECT_EQ(ESM4::pickpocketItemDecision(input), D::Equipped);
    input = valid; input.mFirstInstanceBound = true;
    EXPECT_EQ(ESM4::pickpocketItemDecision(input), D::Bound);
    input = valid; input.mPlayable = false;
    EXPECT_EQ(ESM4::pickpocketItemDecision(input), D::NonPlayable);
    input = valid; input.mQuestItem = true;
    EXPECT_EQ(ESM4::pickpocketItemDecision(input), D::Allowed);
}

TEST(ESM4StealthRules, PlacingPickpocketItemsAppliesQuestDrawnBoundAndPositiveWeightRestrictions)
{
    using D = ESM4::PickpocketItemDecision;
    using R = ESM4::PickpocketDirection;
    const ESM4::PickpocketItemInput valid{R::Place, true, false, false, false, false, 0};
    EXPECT_EQ(ESM4::pickpocketItemDecision(valid), D::Allowed);
    auto input = valid; input.mAnyInstanceWorn = true;
    EXPECT_EQ(ESM4::pickpocketItemDecision(input), D::Allowed);
    input = valid; input.mQuestItem = true;
    EXPECT_EQ(ESM4::pickpocketItemDecision(input), D::QuestItem);
    input = valid; input.mDrawnEquippedWeapon = true; input.mAnyInstanceWorn = true;
    EXPECT_EQ(ESM4::pickpocketItemDecision(input), D::DrawnWeapon);
    input = valid; input.mFirstInstanceBound = true;
    EXPECT_EQ(ESM4::pickpocketItemDecision(input), D::Bound);
    input = valid; input.mPlayable = false;
    EXPECT_EQ(ESM4::pickpocketItemDecision(input), D::NonPlayable);
    for (float weight : {std::nextafter(0.f, 1.f), 1.f, std::numeric_limits<float>::max()})
    {
        input = valid; input.mBaseWeight = weight;
        EXPECT_EQ(ESM4::pickpocketItemDecision(input), D::PositiveWeight);
    }
    input = valid; input.mBaseWeight = -0.f;
    EXPECT_EQ(ESM4::pickpocketItemDecision(input), D::Allowed);
}

TEST(ESM4StealthRules, PickpocketItemPolicyRejectsInvalidDirectionsAndWeights)
{
    using R = ESM4::PickpocketDirection;
    ESM4::PickpocketItemInput input{static_cast<R>(99), true, false, false, false, false, 0};
    EXPECT_THROW(ESM4::pickpocketItemDecision(input), std::invalid_argument);
    for (R direction : {R::Take, R::Place})
        for (float weight : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        {
            input.mDirection = direction; input.mBaseWeight = weight;
            EXPECT_THROW(ESM4::pickpocketItemDecision(input), std::invalid_argument);
        }
}
