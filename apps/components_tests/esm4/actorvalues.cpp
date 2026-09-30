#include <components/esm4/actorvalues.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace
{
    using Owner = ESM4::ActorValueOwner;
    using Process = ESM4::ActorValueProcess;
    using Modifier = ESM4::ActorValueModifier;
}

TEST(ESM4ActorValues, CompositionPreservesPlayerAndNpcStoreOrder)
{
    const ESM4::ActorValueState state{1, {-1, 0x1p-24f, 0}};
    EXPECT_EQ(ESM4::composeActorValue(state, Owner::Player, Process::Active), 0x1p-24f);
    EXPECT_EQ(ESM4::composeActorValue(state, Owner::Player, Process::Low), 0x1p-24f);
    EXPECT_EQ(ESM4::composeActorValue(state, Owner::NonPlayer, Process::Active), 0.f);
    EXPECT_EQ(ESM4::composeActorValue(state, Owner::NonPlayer, Process::Low), 1.f);
}

TEST(ESM4ActorValues, ProcessCompositionRetainsSeparateModifiersAndNegativeCurrentValues)
{
    const ESM4::ActorValueState state{100, {20, -10, -5}};
    EXPECT_EQ(ESM4::composeActorValue(state, Owner::Player, Process::Active), 105);
    EXPECT_EQ(ESM4::composeActorValue(state, Owner::NonPlayer, Process::Active), 105);
    EXPECT_EQ(ESM4::composeActorValue(state, Owner::NonPlayer, Process::Low), 85);
    EXPECT_EQ(state.mModifiers[0], 20); // Low-process read does not erase maximum modifiers.
    EXPECT_EQ(ESM4::composeActorValue({1, {0, 0, -10}}, Owner::NonPlayer, Process::Active), -9);
    EXPECT_EQ(ESM4::composeActorValue({1, {0, 0, -10}}, Owner::Player, Process::Active), -9);
    EXPECT_EQ(ESM4::composeActorValue({37, {}}, Owner::NonPlayer, Process::Active), 37);
}

TEST(ESM4ActorValues, ChannelMutationPreservesOtherChannelsAndOwnerStorageSemantics)
{
    const ESM4::ActorValueState initial{100, {20, -10, std::nullopt}};
    const auto player = ESM4::changeActorValueModifier(initial, Owner::Player, 8, Modifier::Damage, 5);
    const auto npc = ESM4::changeActorValueModifier(initial, Owner::NonPlayer, 8, Modifier::Damage, 5);
    EXPECT_EQ(player.mModifiers[2], 0);
    EXPECT_EQ(npc.mModifiers[2], 5);
    EXPECT_EQ(player.mModifiers[0], 20);
    EXPECT_EQ(npc.mModifiers[1], -10);
    EXPECT_EQ(player.mBase, 100);
    EXPECT_FALSE(initial.mModifiers[2]);
    const auto script = ESM4::changeActorValueModifier(npc, Owner::NonPlayer, 8, Modifier::Script, 30);
    EXPECT_EQ(script.mModifiers[1], 20);
    EXPECT_EQ(script.mModifiers[2], 5);
    const auto maximum = ESM4::changeActorValueModifier(script, Owner::NonPlayer, 8, Modifier::Maximum, -30);
    EXPECT_EQ(maximum.mModifiers[0], -10);
    EXPECT_EQ(maximum.mModifiers[1], 20);
    const auto removed = ESM4::changeActorValueModifier(maximum, Owner::NonPlayer, 8, Modifier::Damage, -5);
    EXPECT_FALSE(removed.mModifiers[2]);
    EXPECT_EQ(ESM4::composeActorValue(removed, Owner::NonPlayer, Process::Active), 110);
}

TEST(ESM4ActorValues, RejectsCorruptStateEnumsAndArithmeticWithoutChangingInput)
{
    ESM4::ActorValueState state{100, {20, 10, -5}};
    const auto original = state;
    EXPECT_THROW(ESM4::composeActorValue(state, static_cast<Owner>(255), Process::Active), std::invalid_argument);
    EXPECT_THROW(ESM4::composeActorValue(state, Owner::Player, static_cast<Process>(255)), std::invalid_argument);
    EXPECT_THROW(ESM4::changeActorValueModifier(state, Owner::Player, 8, static_cast<Modifier>(255), 1),
        std::invalid_argument);
    EXPECT_THROW(ESM4::changeActorValueModifier(state, static_cast<Owner>(255), 8, Modifier::Script, 1),
        std::invalid_argument);
    EXPECT_THROW(ESM4::changeActorValueModifier(state, Owner::NonPlayer, 8, Modifier::Damage,
        std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);
    EXPECT_EQ(state, original);
    state.mBase = std::numeric_limits<float>::infinity();
    EXPECT_THROW(ESM4::validateActorValueState(state), std::invalid_argument);
    state = original;
    state.mModifiers[1] = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(ESM4::validateActorValueState(state), std::invalid_argument);
    const auto largest = std::numeric_limits<float>::max();
    EXPECT_THROW(ESM4::composeActorValue({largest, {largest, 0, 0}}, Owner::Player, Process::Active),
        std::invalid_argument);
}

TEST(ESM4ActorValues, PlayerDynamicBasesUseCurrentAttributesAndRetainFormContribution)
{
    const ESM4::PlayerDynamicBaseSettings settings{2, .5f, 5};
    ESM4::PlayerDynamicBaseInput input{ESM4::DynamicActorValue::Health, 37,
        {1, 49, 3, 4, 5, 6, 7, 8}, 15};
    EXPECT_EQ(ESM4::calculatePlayerDynamicBaseValue(input, settings), 49);
    input.mCurrentAttributes[0] = 100;
    EXPECT_EQ(ESM4::calculatePlayerDynamicBaseValue(input, settings), 49); // Strength is ignored for health.
    input.mValue = ESM4::DynamicActorValue::Magicka;
    EXPECT_EQ(ESM4::calculatePlayerDynamicBaseValue(input, settings), 165); // trunc(49*1.5), then scale1.5
    input.mMagickaMultiplier = 0;
    EXPECT_EQ(ESM4::calculatePlayerDynamicBaseValue(input, settings), 110);
    input.mValue = ESM4::DynamicActorValue::Fatigue;
    EXPECT_EQ(ESM4::calculatePlayerDynamicBaseValue(input, settings), 150);
    input.mValue = ESM4::DynamicActorValue::Encumbrance;
    EXPECT_EQ(ESM4::calculatePlayerDynamicBaseValue(input, settings), 537);
    input.mCurrentAttributes[0] = -10;
    EXPECT_EQ(ESM4::calculatePlayerDynamicBaseValue(input, settings), 37);
}

TEST(ESM4ActorValues, MagickaScaleUsesTenthsAndZeroAfterFloatStorage)
{
    EXPECT_EQ(ESM4::actorMagickaScale(0), 1);
    EXPECT_EQ(ESM4::actorMagickaScale(-0.f), 1);
    EXPECT_EQ(ESM4::actorMagickaScale(std::numeric_limits<float>::denorm_min()), 1);
    EXPECT_EQ(ESM4::actorMagickaScale(10), 1);
    EXPECT_EQ(ESM4::actorMagickaScale(15), 1.5f);
    EXPECT_EQ(ESM4::actorMagickaScale(-10), -1);
    EXPECT_THROW(ESM4::actorMagickaScale(std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);
}

TEST(ESM4ActorValues, PlayerDynamicBasesPreserveIntegerThenFloatStorageAndFatigueWrap)
{
    ESM4::PlayerDynamicBaseInput input{ESM4::DynamicActorValue::Magicka, 1,
        {0, 16'777'217, 0, 0, 0, 0, 0, 0}, 10};
    EXPECT_EQ(ESM4::calculatePlayerDynamicBaseValue(input, {2, 0, 5}), 16'777'216.f);
    input.mValue = ESM4::DynamicActorValue::Fatigue;
    input.mCurrentAttributes.fill(std::numeric_limits<std::int32_t>::max());
    EXPECT_EQ(ESM4::calculatePlayerDynamicBaseValue(input, {2, 0, 5}), -3);
    input.mCurrentAttributes.fill(-1);
    EXPECT_EQ(ESM4::calculatePlayerDynamicBaseValue(input, {2, 0, 5}), -3);
}

TEST(ESM4ActorValues, PlayerDynamicBasesRejectInvalidQueriesSettingsAndOverflow)
{
    ESM4::PlayerDynamicBaseInput input{ESM4::DynamicActorValue::Health, 0,
        {1, 2, 3, 4, 5, 6, 7, 8}, 0};
    const auto invalid = std::numeric_limits<float>::infinity();
    EXPECT_THROW(ESM4::calculatePlayerDynamicBaseValue(input, {invalid, 1, 5}), std::invalid_argument);
    input.mValue = static_cast<ESM4::DynamicActorValue>(255);
    EXPECT_THROW(ESM4::calculatePlayerDynamicBaseValue(input, {2, 1, 5}), std::invalid_argument);
    input.mValue = ESM4::DynamicActorValue::Health;
    input.mCurrentAttributes[5] = std::numeric_limits<std::int32_t>::max();
    EXPECT_THROW(ESM4::calculatePlayerDynamicBaseValue(input, {2, 1, 5}), std::invalid_argument);
    input.mValue = ESM4::DynamicActorValue::Magicka;
    input.mFormValue = std::numeric_limits<std::int32_t>::max();
    input.mMagickaMultiplier = std::numeric_limits<float>::max();
    EXPECT_THROW(ESM4::calculatePlayerDynamicBaseValue(input, {2, 1, 5}), std::invalid_argument);
}

TEST(ESM4ActorValues, IntegerQueriesHaveIndependentPlayerAndNpcTruncationBoundaries)
{
    const ESM4::ActorValueModifiers modifiers{.75f, .75f, 0};
    EXPECT_EQ(ESM4::composeIntegerActorValue(1, modifiers, Owner::Player, Process::Active), 2);
    EXPECT_EQ(ESM4::composeIntegerActorValue(1, modifiers, Owner::NonPlayer, Process::Active), 1);
    EXPECT_EQ(ESM4::composeIntegerActorValue(1, modifiers, Owner::NonPlayer, Process::Low), 1);
    EXPECT_EQ(ESM4::composeActorValue({1, modifiers}, Owner::NonPlayer, Process::Active), 2.5f);
    EXPECT_EQ(ESM4::composeIntegerActorValue(100, {.5f, -.5f, 0}, Owner::Player, Process::Active), 100);
    EXPECT_EQ(ESM4::composeIntegerActorValue(100, {.5f, -.5f, 0}, Owner::NonPlayer, Process::Active), 99);
}

TEST(ESM4ActorValues, IntegerQueriesTruncateTowardZeroWithoutAnExtraFloatStore)
{
    EXPECT_EQ(ESM4::composeIntegerActorValue(-1, {0, -.75f, 0}, Owner::Player, Process::Active), -1);
    EXPECT_EQ(ESM4::composeIntegerActorValue(-1, {0, -.75f, 0}, Owner::NonPlayer, Process::Low), -1);
    EXPECT_EQ(ESM4::composeIntegerActorValue(16'777'217, {.5f, 0, 0}, Owner::Player, Process::Active),
        16'777'217);
    EXPECT_EQ(ESM4::composeIntegerActorValue(16'777'217, {.5f, 0, 0}, Owner::NonPlayer, Process::Active),
        16'777'217);
    EXPECT_EQ(ESM4::composeIntegerActorValue(std::numeric_limits<std::int32_t>::max(), {},
        Owner::Player, Process::Active), std::numeric_limits<std::int32_t>::max());
    EXPECT_EQ(ESM4::composeIntegerActorValue(std::numeric_limits<std::int32_t>::min(), {},
        Owner::NonPlayer, Process::Active), std::numeric_limits<std::int32_t>::min());
}

TEST(ESM4ActorValues, IntegerQueriesRejectInvalidEnumsModifiersAndOverflow)
{
    EXPECT_THROW(ESM4::composeIntegerActorValue(0, {}, static_cast<Owner>(255), Process::Low),
        std::invalid_argument);
    EXPECT_THROW(ESM4::composeIntegerActorValue(0, {}, Owner::Player, static_cast<Process>(255)),
        std::invalid_argument);
    EXPECT_THROW(ESM4::composeIntegerActorValue(0, {0, std::numeric_limits<float>::infinity(), 0},
        Owner::Player, Process::Active), std::invalid_argument);
    EXPECT_THROW(ESM4::composeIntegerActorValue(std::numeric_limits<std::int32_t>::max(), {1, 0, 0},
        Owner::Player, Process::Active), std::invalid_argument);
    EXPECT_THROW(ESM4::composeIntegerActorValue(std::numeric_limits<std::int32_t>::min(), {0, -1, 0},
        Owner::NonPlayer, Process::Low), std::invalid_argument);
}

TEST(ESM4ActorValues, DynamicMaximumUsesIntegerBaseAndOnlyEligibleMaximumModifier)
{
    EXPECT_EQ(ESM4::dynamicActorValueMaximum(100, 10.5f, Owner::Player, Process::Low), 110.5f);
    EXPECT_EQ(ESM4::dynamicActorValueMaximum(100, 10.5f, Owner::NonPlayer, Process::Low), 100.f);
    EXPECT_EQ(ESM4::dynamicActorValueMaximum(100, 10.5f, Owner::NonPlayer, Process::Active), 110.5f);
    EXPECT_EQ(ESM4::dynamicActorValueMaximum(-100, 10.5f, Owner::Player, Process::Active), -89.5f);
    EXPECT_EQ(ESM4::dynamicActorValueMaximum(16777217, 1.f, Owner::Player, Process::Active), 16777218.f);
    EXPECT_THROW(ESM4::dynamicActorValueMaximum(1, 0, static_cast<Owner>(2), Process::Active), std::invalid_argument);
    EXPECT_THROW(ESM4::dynamicActorValueMaximum(1, 0, Owner::Player, static_cast<Process>(2)), std::invalid_argument);
    EXPECT_THROW(ESM4::dynamicActorValueMaximum(1, std::numeric_limits<float>::infinity(), Owner::NonPlayer,
        Process::Low), std::invalid_argument);
}

TEST(ESM4ActorValues, NpcMagickaScalesDistinctFloatAndIntegerProcessResults)
{
    EXPECT_EQ(ESM4::scaleNpcMagicka(10.75f, 5.f), 5.375f);
    EXPECT_EQ(ESM4::scaleNpcIntegerMagicka(10, 5.f), 5);
    EXPECT_EQ(ESM4::scaleNpcIntegerMagicka(-11, 5.f), -5);
    EXPECT_EQ(ESM4::scaleNpcMagicka(10.75f, 0.f), 10.75f);
    EXPECT_EQ(ESM4::scaleNpcIntegerMagicka(16777217, 10.f), 16777217);
    EXPECT_EQ(ESM4::scaleNpcIntegerMagicka(11, -10.f), -11);
    EXPECT_THROW(ESM4::scaleNpcMagicka(std::numeric_limits<float>::infinity(), 10), std::invalid_argument);
    EXPECT_THROW(ESM4::scaleNpcMagicka(1, std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);
    EXPECT_THROW(ESM4::scaleNpcMagicka(std::numeric_limits<float>::max(), 20), std::invalid_argument);
    EXPECT_THROW(ESM4::scaleNpcIntegerMagicka(std::numeric_limits<std::int32_t>::max(), 20), std::invalid_argument);
}

TEST(ESM4ActorValues, ForceCommandRetainsExactIntegerRequestUntilDeltaStore)
{
    EXPECT_EQ(ESM4::forceActorValueDelta(100, 90.75f), 9.25f);
    EXPECT_EQ(ESM4::forceActorValueDelta(16777217, 16777216.f), 1.f);
    EXPECT_EQ(ESM4::forceActorValueDelta(-16777217, -16777216.f), -1.f);
    EXPECT_EQ(ESM4::forceActorValueDelta(std::numeric_limits<std::int32_t>::max(), 2147483648.f), -1.f);
    EXPECT_EQ(ESM4::forceActorValueDelta(std::numeric_limits<std::int32_t>::min(), -2147483648.f), 0.f);
    EXPECT_THROW(ESM4::forceActorValueDelta(0, std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);
    EXPECT_THROW(ESM4::forceActorValueDelta(0, std::numeric_limits<float>::infinity()), std::invalid_argument);
}

TEST(ESM4ActorValues, BaseSetterPreservesNativeStorageWidths)
{
    using Kind = ESM4::ActorBaseKind;
    for (Kind kind : {Kind::Npc, Kind::Creature})
    {
        for (std::uint8_t av : {0, 7, 12, 18, 19, 25, 26, 32, 33, 36})
        {
            for (const auto [requested, expected] : {std::pair{-1, 255}, {0, 0}, {255, 255},
                     {256, 0}, {257, 1}, {std::numeric_limits<std::int32_t>::min(), 0}})
            {
                const auto change = ESM4::prepareActorBaseValueSet(kind, av, requested);
                ASSERT_TRUE(change);
                EXPECT_EQ(std::get<std::int32_t>(change->mValue), expected);
            }
        }
        for (std::uint8_t av : {9, 10})
        {
            EXPECT_EQ(std::get<std::int32_t>(ESM4::prepareActorBaseValueSet(kind, av, -1)->mValue), 65535);
            EXPECT_EQ(std::get<std::int32_t>(ESM4::prepareActorBaseValueSet(kind, av, 65536)->mValue), 0);
        }
        for (std::int32_t request : {-1, 0, 16777217, std::numeric_limits<std::int32_t>::min(),
                 std::numeric_limits<std::int32_t>::max()})
        {
            const auto health = ESM4::prepareActorBaseValueSet(kind, 8, request);
            ASSERT_TRUE(health);
            EXPECT_EQ(health->mActorValue, 8);
            EXPECT_EQ(std::get<std::int32_t>(health->mValue), request);
        }
        EXPECT_EQ(std::get<float>(ESM4::prepareActorBaseValueSet(kind, 40, 16777217)->mValue), 16777216.f);
        EXPECT_EQ(std::get<float>(ESM4::prepareActorBaseValueSet(kind, 71,
            std::numeric_limits<std::int32_t>::max())->mValue), 2147483648.f);
    }
}

TEST(ESM4ActorValues, FloatBaseConversionPreservesBothNativeCpuDomains)
{
    using Mode = ESM4::ActorValueConversionMode;
    struct Case { float input; std::int32_t nonSse; std::int32_t sse; };
    constexpr auto minimum = std::numeric_limits<std::int32_t>::min();
    for (const auto& [input, nonSse, sse] : {
             Case{-1.75f, -1, -1}, Case{-.5f, 0, 0}, Case{-0.f, 0, 0}, Case{.5f, 0, 0},
             Case{1.75f, 1, 1}, Case{0x1p31f, minimum, minimum},
             Case{-0x1p31f, minimum, minimum}, Case{0x1p31f + 256.f, minimum + 256, minimum},
             Case{-0x1p31f - 256.f, 2147483392, minimum},
             Case{0x1p32f, 0, minimum}, Case{-0x1p32f, 0, minimum},
             Case{0x1p63f, 0, minimum}, Case{-0x1p63f, 0, minimum},
             Case{std::numeric_limits<float>::max(), 0, minimum}})
    {
        SCOPED_TRACE(input);
        EXPECT_EQ(ESM4::convertActorBaseFloat(input, Mode::NonSse), nonSse);
        EXPECT_EQ(ESM4::convertActorBaseFloat(input, Mode::Sse), sse);
    }
    for (Mode mode : {Mode::NonSse, Mode::Sse})
    {
        EXPECT_EQ(ESM4::convertActorBaseFloat(std::nextafter(0x1p31f, 0.f), mode), 2147483520);
        EXPECT_EQ(ESM4::convertActorBaseFloat(std::numeric_limits<float>::denorm_min(), mode), 0);
        EXPECT_THROW(ESM4::convertActorBaseFloat(std::numeric_limits<float>::quiet_NaN(), mode),
            std::invalid_argument);
        EXPECT_THROW(ESM4::convertActorBaseFloat(std::numeric_limits<float>::infinity(), mode),
            std::invalid_argument);
    }
    EXPECT_THROW(ESM4::convertActorBaseFloat(1, static_cast<Mode>(2)), std::invalid_argument);
}

TEST(ESM4ActorValues, FloatBaseSetConvertsBeforeNativeWidthsAndAliases)
{
    using Mode = ESM4::ActorValueConversionMode;
    using Kind = ESM4::ActorBaseKind;
    for (Mode mode : {Mode::NonSse, Mode::Sse})
        for (Kind kind : {Kind::Npc, Kind::Creature})
        {
            EXPECT_EQ(std::get<std::int32_t>(ESM4::prepareActorBaseValueFloatSet(kind, 0, -1.75f, mode)->mValue), 255);
            EXPECT_EQ(std::get<std::int32_t>(ESM4::prepareActorBaseValueFloatSet(kind, 9, -1.75f, mode)->mValue), 65535);
            EXPECT_EQ(std::get<std::int32_t>(ESM4::prepareActorBaseValueFloatSet(kind, 8, -1.75f, mode)->mValue), -1);
            EXPECT_EQ(std::get<float>(ESM4::prepareActorBaseValueFloatSet(kind, 40, -1.75f, mode)->mValue), -1.f);
            const auto skill = ESM4::prepareActorBaseValueFloatSet(kind, 28, 257.75f, mode);
            EXPECT_EQ(skill->mActorValue, kind == Kind::Creature ? 12 : 28);
            EXPECT_EQ(std::get<std::int32_t>(skill->mValue), 1);
            EXPECT_FALSE(ESM4::prepareActorBaseValueFloatSet(kind, 11, 1.75f, mode));
            EXPECT_FALSE(ESM4::prepareActorBaseValueFloatSet(kind, 38, 1.75f, mode));
        }
    EXPECT_EQ(std::get<float>(ESM4::prepareActorBaseValueFloatSet(Kind::Npc, 40, 0x1p32f, Mode::NonSse)->mValue), 0.f);
    EXPECT_EQ(std::get<float>(ESM4::prepareActorBaseValueFloatSet(Kind::Npc, 40, 0x1p32f, Mode::Sse)->mValue), -0x1p31f);
}

TEST(ESM4ActorValues, BaseSetterAliasesCreatureSkillsAndIdentifiesNoBaseWrite)
{
    using Kind = ESM4::ActorBaseKind;
    for (std::uint8_t av = 0; av < 72; ++av)
    {
        for (Kind kind : {Kind::Npc, Kind::Creature})
        {
            const auto change = ESM4::prepareActorBaseValueSet(kind, av, 100);
            if (av == 11 || (av >= 37 && av <= 39))
                EXPECT_FALSE(change);
            else
            {
                ASSERT_TRUE(change);
                const auto expected = kind == Kind::Npc || av < 12 || av > 32 ? av
                    : av <= 18 || av == 28 ? 12 : av <= 25 ? 19 : 26;
                EXPECT_EQ(change->mActorValue, expected);
                EXPECT_EQ(change->mValue.index(), av < 40 ? 0 : 1);
            }
        }
    }
    EXPECT_THROW(ESM4::prepareActorBaseValueSet(static_cast<Kind>(2), 8, 100), std::invalid_argument);
    EXPECT_THROW(ESM4::prepareActorBaseValueSet(Kind::Npc, 72, 100), std::invalid_argument);
    EXPECT_THROW(ESM4::prepareActorBaseValueSet(Kind::Creature, 255, 100), std::invalid_argument);
}

TEST(ESM4ActorValues, NonPlayerFormFloatRetainsExactIntegerUntilModifierAddition)
{
    // Original 51E790 returns FILD without FSTP; 6433E0 stores only after
    // Script and Damage, then 6587E0 adds Maximum for an active process.
    for (const auto process : {Process::Low, Process::Active})
    {
        EXPECT_EQ(ESM4::composeNonPlayerActorValue(16'777'217, {0, .5f, 0}, process), 16'777'218.f);
        EXPECT_EQ(ESM4::composeNonPlayerActorValue(-16'777'217, {0, -.5f, 0}, process), -16'777'218.f);
        EXPECT_EQ(ESM4::composeNonPlayerActorValue(16'777'217, {0, 0, -.5f}, process), 16'777'216.f);
    }
    EXPECT_EQ(ESM4::composeNonPlayerActorValue(16'777'217, {1, 1, 0}, Process::Active), 16'777'220.f);
    EXPECT_EQ(ESM4::composeNonPlayerActorValue(16'777'217, {1, 1, 0}, Process::Low), 16'777'218.f);
    EXPECT_THROW(ESM4::composeNonPlayerActorValue(1, {}, static_cast<Process>(255)), std::invalid_argument);
    EXPECT_THROW(ESM4::composeNonPlayerActorValue(1, {std::numeric_limits<float>::infinity(), 0, 0},
        Process::Low), std::invalid_argument);
    EXPECT_THROW(ESM4::composeNonPlayerActorValue(1, {0, 3e38f, 3e38f}, Process::Active), std::invalid_argument);
}

TEST(ESM4ActorValues, BaseFloatStorageQueriesTruncateBeforeActorComposition)
{
    EXPECT_EQ(ESM4::actorBaseValueInteger({8, std::int32_t{16'777'217}}), 16'777'217);
    EXPECT_EQ(ESM4::actorBaseValueInteger({71, 1.75f}), 1);
    EXPECT_EQ(ESM4::actorBaseValueInteger({71, -1.75f}), -1);
    EXPECT_EQ(ESM4::actorBaseValueInteger({40, -.75f}), 0);
    EXPECT_THROW(ESM4::actorBaseValueInteger({40, 2147483648.f}), std::invalid_argument);
    EXPECT_THROW(ESM4::actorBaseValueInteger({40, std::numeric_limits<float>::quiet_NaN()}), std::invalid_argument);
}

TEST(ESM4ActorValues, ScriptNamesUseVanillaIndicesAndAsciiCaseFolding)
{
    const std::array<std::pair<std::string_view, std::uint8_t>, 16> cases{{
        {"Strength", 0}, {"LUCK", 7}, {"Health", 8}, {"Encumbrance", 11}, {"Armorer", 12}, {"HandToHand", 17},
        {"Marksman", 28}, {"Speechcraft", 32}, {"Aggression", 33}, {"Bounty", 37}, {"MagickaMultiplier", 40},
        {"Paralysis", 48}, {"StuntedMagicka", 57}, {"ResistMagic", 64}, {"Vampirism", 69}, {"resistWATERdamage", 71}}};
    for (const auto& [name, index] : cases)
        EXPECT_EQ(ESM4::actorValueIndex(name), index) << name;
    for (const auto name : {"", " Strength", "Strength ", "CarryWeight", "Level", "LongBlade", "Spear", "ResistWaterDamageX"})
        EXPECT_FALSE(ESM4::actorValueIndex(name)) << name;
}

TEST(ESM4ActorValues, ModifierCommandsPreserveOriginEligibilityAndIntegerBoundaryOrder)
{
    using Command = ESM4::ActorValueCommand;
    using Source = ESM4::ActorValueCommandSource;
    const auto prepare = [](Owner owner, std::uint8_t value, Command command, Source source,
                             std::int32_t requested, float current, bool god, bool spend) {
        return ESM4::prepareActorValueModifierCommand(owner, value, command, source, requested, current, {god, spend});
    };
    for (const auto source : {Source::Script, Source::Console})
    {
        const auto force = prepare(Owner::Player, 8, Command::Force, source, 16777217, 16777216, false, true);
        ASSERT_TRUE(force);
        EXPECT_EQ(force->mDelta, 1);
        EXPECT_EQ(force->mModifier, source == Source::Script ? Modifier::Script : Modifier::Damage);
        EXPECT_FALSE(force->mHealthReaction);
        EXPECT_FALSE(prepare(Owner::Player, 8, Command::Mod, source, -1, 0, true, true));
        EXPECT_FALSE(prepare(Owner::Player, 9, Command::Force, source, 0, 1, true, true));
        EXPECT_FALSE(prepare(Owner::NonPlayer, 10, Command::Mod, source, -1, 0, false, false));
        EXPECT_FALSE(prepare(Owner::NonPlayer, 10, Command::Force, source, 10, 10.5, true, false));
        EXPECT_TRUE(prepare(Owner::Player, 10, Command::Mod, source, -1, 0, false, false));
        EXPECT_TRUE(prepare(Owner::NonPlayer, 9, Command::Mod, source, -1, 0, true, false));
        const auto health = prepare(Owner::NonPlayer, 8, Command::Force, source, 10, 10.5, true, false);
        ASSERT_TRUE(health);
        EXPECT_EQ(health->mDelta, -.5f);
        EXPECT_TRUE(health->mHealthReaction);
        const auto wrapped = prepare(Owner::Player, 8, Command::Mod, source,
            std::numeric_limits<std::int32_t>::max(), 0, true, true);
        ASSERT_TRUE(wrapped); // Positive input passes god-mode gate before wrapping.
        EXPECT_EQ(wrapped->mDelta, -0x1p31f);
        EXPECT_TRUE(wrapped->mHealthReaction);
        const auto zero = prepare(Owner::NonPlayer, 10, Command::Mod, source, 0, 0, true, false);
        ASSERT_TRUE(zero);
        EXPECT_EQ(zero->mDelta, 0);
    }
    EXPECT_THROW(prepare(Owner::Player, 8, Command::Set, Source::Script, 1, 0, false, true), std::invalid_argument);
    EXPECT_THROW(prepare(Owner::Player, 72, Command::Mod, Source::Script, 1, 0, false, true), std::invalid_argument);
    EXPECT_THROW(prepare(static_cast<Owner>(255), 8, Command::Mod, Source::Script, 1, 0, false, true),
        std::invalid_argument);
    EXPECT_THROW(prepare(Owner::Player, 8, Command::Mod, static_cast<Source>(255), 1, 0, false, true),
        std::invalid_argument);
    EXPECT_THROW(prepare(Owner::Player, 8, Command::Force, Source::Script, 1,
        std::numeric_limits<float>::infinity(), false, true), std::invalid_argument);
}

TEST(ESM4ActorValues, MagickaAndFatigueModifierSlotsRemainAllocatedAtZero)
{
    for (std::uint8_t av : {9, 10})
    {
        for (auto modifier : {Modifier::Maximum, Modifier::Script, Modifier::Damage})
        {
            const auto index = static_cast<std::size_t>(modifier);
            ESM4::ActorValueState initial{100, {11, 12, -13}};
            initial.mModifiers[index] = -5;
            const auto zero = ESM4::changeActorValueModifier(initial, Owner::NonPlayer, av, modifier, 5);
            EXPECT_EQ(zero.mModifiers[index], 0);
            EXPECT_EQ(zero.mBase, 100);
            for (std::size_t other = 0; other < initial.mModifiers.size(); ++other)
            {
                if (other != index)
                {
                    EXPECT_EQ(zero.mModifiers[other], initial.mModifiers[other]);
                }
            }
            // Older staged snapshots can omit these slots. A missing entry
            // denotes the native constructor's permanent zero, not allocation
            // of a new sparse slot on the first positive Damage write.
            initial.mModifiers[index].reset();
            const auto added = ESM4::changeActorValueModifier(initial, Owner::NonPlayer, av, modifier, 5);
            EXPECT_EQ(added.mModifiers[index], modifier == Modifier::Damage ? 0 : 5);
            const auto unchanged = ESM4::changeActorValueModifier(initial, Owner::NonPlayer, av, modifier, 0);
            EXPECT_EQ(unchanged.mModifiers[index], 0);
            initial.mModifiers[index] = -0.f;
            const auto positiveZero = ESM4::changeActorValueModifier(initial, Owner::NonPlayer, av, modifier, -0.f);
            ASSERT_TRUE(positiveZero.mModifiers[index]);
            EXPECT_FALSE(std::signbit(*positiveZero.mModifiers[index]));
        }
    }
}

TEST(ESM4ActorValues, ModifierMutationRequiresCanonicalActorValueIndex)
{
    for (auto owner : {Owner::Player, Owner::NonPlayer})
        for (unsigned av = 72; av < 256; ++av)
        {
            EXPECT_THROW(ESM4::changeActorValueModifier({}, owner,
                static_cast<std::uint8_t>(av), Modifier::Damage, 0), std::invalid_argument);
        }
}

TEST(ESM4ActorValues, ResurrectionResetPreservesOwnerSpecificModifierStorage)
{
    for (auto owner : {Owner::Player, Owner::NonPlayer})
        for (std::uint8_t av = 0; av < 72; ++av)
            for (bool present : {false, true})
            {
                SCOPED_TRACE(testing::Message() << "owner=" << static_cast<int>(owner)
                    << " av=" << static_cast<int>(av) << " present=" << present);
                const ESM4::ActorValueState initial{100,
                    present ? ESM4::ActorValueModifiers{1, -2, -3} : ESM4::ActorValueModifiers{}};
                const auto reset = ESM4::resetResurrectionModifiers(initial, owner, av);
                EXPECT_EQ(reset.mBase, 100);
                if (owner == Owner::Player)
                {
                    EXPECT_EQ(reset.mModifiers[0], initial.mModifiers[0]);
                    EXPECT_EQ(reset.mModifiers[1], initial.mModifiers[1]);
                    EXPECT_EQ(reset.mModifiers[2], av >= 8 && av <= 10
                        ? std::optional<float>(0) : initial.mModifiers[2]);
                }
                else
                {
                    EXPECT_FALSE(reset.mModifiers[0]);
                    EXPECT_EQ(reset.mModifiers[1], av == 9 || av == 10
                        ? std::optional<float>(present ? -2 : 0) : std::nullopt);
                    EXPECT_EQ(reset.mModifiers[2], av == 9 || av == 10
                        ? std::optional<float>(0) : std::nullopt);
                }
                EXPECT_EQ(ESM4::resetResurrectionModifiers(reset, owner, av), reset);
            }
    EXPECT_THROW(ESM4::resetResurrectionModifiers({}, static_cast<Owner>(255), 8), std::invalid_argument);
    EXPECT_THROW(ESM4::resetResurrectionModifiers({}, Owner::Player, 72), std::invalid_argument);
    EXPECT_THROW(ESM4::resetResurrectionModifiers({std::numeric_limits<float>::infinity(), {}},
        Owner::NonPlayer, 8), std::invalid_argument);
    EXPECT_THROW(ESM4::resetResurrectionModifiers({1, {0, std::numeric_limits<float>::quiet_NaN(), 0}},
        Owner::Player, 9), std::invalid_argument);
}

TEST(ESM4ActorValues, InitialModifierStorageMatchesOriginalOwnerAndProcessConstructors)
{
    // Independent original construction reports: Player stores 216 positive
    // zeros; sparse NPC containers store only permanent Magicka/Fatigue nodes.
    for (const auto owner : {Owner::Player, Owner::NonPlayer})
        for (const auto process : {Process::Low, Process::Active})
        {
            SCOPED_TRACE(static_cast<unsigned>(owner));
            SCOPED_TRACE(static_cast<unsigned>(process));
            const auto storage = ESM4::initialActorValueModifierStorage(owner, process);
            std::array<std::size_t, 3> present{};
            for (std::uint8_t av = 0; av < storage.size(); ++av)
            {
                for (std::size_t channel = 0; channel < storage[av].size(); ++channel)
                    if (const auto value = storage[av][channel])
                    {
                        ++present[channel];
                        EXPECT_EQ(std::bit_cast<std::uint32_t>(*value), 0u);
                        if (owner == Owner::NonPlayer)
                            EXPECT_TRUE(av == 9 || av == 10);
                    }
                ESM4::ActorValueState initial{100.f, storage[av]};
                EXPECT_EQ(ESM4::composeActorValue(initial, owner, process), 100.f);
                const auto zeroScript = ESM4::changeActorValueModifier(initial, owner, av, Modifier::Script, 0.f);
                EXPECT_EQ(zeroScript, initial);
                const auto debit = ESM4::changeActorValueModifier(initial, owner, av, Modifier::Damage, -1.f);
                EXPECT_EQ(ESM4::composeActorValue(debit, owner, process), 99.f);
                EXPECT_EQ(storage[av], initial.mModifiers); // Immutable construction inputs.
            }
            const auto expected = owner == Owner::Player ? std::array<std::size_t, 3>{72, 72, 72}
                : process == Process::Low ? std::array<std::size_t, 3>{0, 2, 2}
                                         : std::array<std::size_t, 3>{2, 2, 2};
            EXPECT_EQ(present, expected);
        }
}

TEST(ESM4ActorValues, InitialModifierStorageRejectsInvalidConstructionDomains)
{
    EXPECT_THROW(ESM4::initialActorValueModifierStorage(static_cast<Owner>(255), Process::Active),
        std::invalid_argument);
    for (const auto owner : {Owner::Player, Owner::NonPlayer})
        EXPECT_THROW(ESM4::initialActorValueModifierStorage(owner, static_cast<Process>(255)),
            std::invalid_argument);
}

TEST(ESM4ActorValues, FloatBaseModifierAddsExactIntegerBeforeFloatStoreAndSetterConversion)
{
    using Kind = ESM4::ActorBaseKind;
    using Mode = ESM4::ActorValueConversionMode;
    const auto change = ESM4::prepareActorBaseValueFloatMod(Kind::Npc, 8, 16777217, .5f, Mode::NonSse);
    ASSERT_TRUE(change);
    EXPECT_EQ(std::get<std::int32_t>(change->mValue), 16777218);
    EXPECT_EQ(std::get<std::int32_t>(ESM4::prepareActorBaseValueFloatMod(
        Kind::Npc, 8, -1, -.75f, Mode::NonSse)->mValue), -1);
    EXPECT_EQ(std::get<std::int32_t>(ESM4::prepareActorBaseValueFloatMod(
        Kind::Npc, 9, 65535, 1.75f, Mode::NonSse)->mValue), 0);
    EXPECT_EQ(std::get<std::int32_t>(ESM4::prepareActorBaseValueFloatMod(
        Kind::Npc, 0, 255, 1.75f, Mode::NonSse)->mValue), 0);
    EXPECT_EQ(std::get<std::int32_t>(ESM4::prepareActorBaseValueFloatMod(
        Kind::Npc, 8, std::numeric_limits<std::int32_t>::max(), 0.f, Mode::Sse)->mValue),
        std::numeric_limits<std::int32_t>::min());
    EXPECT_EQ(std::get<std::int32_t>(ESM4::prepareActorBaseValueFloatMod(
        Kind::Npc, 8, std::numeric_limits<std::int32_t>::max(), 0.f, Mode::NonSse)->mValue),
        std::numeric_limits<std::int32_t>::min());
    EXPECT_EQ(std::get<float>(ESM4::prepareActorBaseValueFloatMod(
        Kind::Npc, 55, 1, .75f, Mode::NonSse)->mValue), 1.f);
    const auto creature = ESM4::prepareActorBaseValueFloatMod(Kind::Creature, 28, 50, 10, Mode::NonSse);
    ASSERT_TRUE(creature);
    EXPECT_EQ(creature->mActorValue, 12);
    EXPECT_EQ(std::get<std::int32_t>(creature->mValue), 60);
    for (const auto ignored : {11, 37, 38, 39})
        EXPECT_FALSE(ESM4::prepareActorBaseValueFloatMod(Kind::Npc, ignored, 0, 1, Mode::NonSse));
}

TEST(ESM4ActorValues, FloatBaseModifierRejectsNonfiniteAndInvalidDomains)
{
    using Kind = ESM4::ActorBaseKind;
    using Mode = ESM4::ActorValueConversionMode;
    for (float delta : {std::numeric_limits<float>::infinity(),
             -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        EXPECT_THROW(ESM4::prepareActorBaseValueFloatMod(Kind::Npc, 8, 0, delta, Mode::NonSse), std::invalid_argument);
    EXPECT_THROW(ESM4::prepareActorBaseValueFloatMod(Kind::Npc, 72, 0, 0, Mode::NonSse), std::invalid_argument);
    EXPECT_THROW(ESM4::prepareActorBaseValueFloatMod(static_cast<Kind>(2), 8, 0, 0, Mode::NonSse), std::invalid_argument);
    EXPECT_THROW(ESM4::prepareActorBaseValueFloatMod(Kind::Npc, 8, 0, 0, static_cast<Mode>(2)), std::invalid_argument);
}

TEST(ESM4ActorValues, LegacyPlayerFormReencodingPreservesSmallUnscaledCachedBases)
{
    const ESM4::PlayerDynamicBaseSettings settings{2, 1, 5};
    const std::array<std::int32_t, 8> attributes{40, 40, 40, 40, 40, 50, 40, 40};
    const std::array<float, 4> bases{110, 130, 170, 205};
    const auto raw = ESM4::legacyPlayerFormValues(bases, attributes, settings);
    EXPECT_EQ(raw, (std::array<std::int32_t, 4>{10, 50, 0, 5}));
    for (unsigned i = 0; i < raw.size(); ++i)
        EXPECT_EQ(ESM4::calculatePlayerDynamicBaseValue(
            {static_cast<ESM4::DynamicActorValue>(8 + i), raw[i], attributes, 0}, settings), bases[i]);
    auto negative = bases; negative[0] = -80;
    EXPECT_EQ(ESM4::legacyPlayerFormValues(negative, attributes, settings)[0], -180);
    auto fractionalSettings = settings; fractionalSettings.mStrengthEncumbranceMultiplier = 5.25f;
    auto fractional = bases; fractional[3] = 215;
    EXPECT_EQ(ESM4::legacyPlayerFormValues(fractional, attributes, fractionalSettings)[3], 5);
}

TEST(ESM4ActorValues, LegacyPlayerFormReencodingRejectsAmbiguousOrUnrepresentableCaches)
{
    const ESM4::PlayerDynamicBaseSettings settings{2, 1, 5};
    const std::array<std::int32_t, 8> attributes{40, 40, 40, 40, 40, 50, 40, 40};
    const std::array<float, 4> valid{110, 130, 170, 205};
    for (float unsupported : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN(),
        8388608.f, -8388608.f, 110.25f})
    {
        auto invalid = valid; invalid[0] = unsupported;
        EXPECT_THROW(ESM4::legacyPlayerFormValues(invalid, attributes, settings), std::invalid_argument);
    }
    auto nonfinite = settings; nonfinite.mHealthMultiplier = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(ESM4::legacyPlayerFormValues(valid, attributes, nonfinite), std::invalid_argument);
    auto zero = valid; zero[0] = -.0f;
    EXPECT_THROW(ESM4::legacyPlayerFormValues(zero, attributes, settings), std::invalid_argument);
}
