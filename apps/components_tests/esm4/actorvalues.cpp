#include <components/esm4/actorvalues.hpp>

#include <gtest/gtest.h>

#include <limits>
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
    const auto player = ESM4::changeActorValueModifier(initial, Owner::Player, Modifier::Damage, 5);
    const auto npc = ESM4::changeActorValueModifier(initial, Owner::NonPlayer, Modifier::Damage, 5);
    EXPECT_EQ(player.mModifiers[2], 0);
    EXPECT_EQ(npc.mModifiers[2], 5);
    EXPECT_EQ(player.mModifiers[0], 20);
    EXPECT_EQ(npc.mModifiers[1], -10);
    EXPECT_EQ(player.mBase, 100);
    EXPECT_FALSE(initial.mModifiers[2]);
    const auto script = ESM4::changeActorValueModifier(npc, Owner::NonPlayer, Modifier::Script, 30);
    EXPECT_EQ(script.mModifiers[1], 20);
    EXPECT_EQ(script.mModifiers[2], 5);
    const auto maximum = ESM4::changeActorValueModifier(script, Owner::NonPlayer, Modifier::Maximum, -30);
    EXPECT_EQ(maximum.mModifiers[0], -10);
    EXPECT_EQ(maximum.mModifiers[1], 20);
    const auto removed = ESM4::changeActorValueModifier(maximum, Owner::NonPlayer, Modifier::Damage, -5);
    EXPECT_FALSE(removed.mModifiers[2]);
    EXPECT_EQ(ESM4::composeActorValue(removed, Owner::NonPlayer, Process::Active), 110);
}

TEST(ESM4ActorValues, RejectsCorruptStateEnumsAndArithmeticWithoutChangingInput)
{
    ESM4::ActorValueState state{100, {20, 10, -5}};
    const auto original = state;
    EXPECT_THROW(ESM4::composeActorValue(state, static_cast<Owner>(255), Process::Active), std::invalid_argument);
    EXPECT_THROW(ESM4::composeActorValue(state, Owner::Player, static_cast<Process>(255)), std::invalid_argument);
    EXPECT_THROW(ESM4::changeActorValueModifier(state, Owner::Player, static_cast<Modifier>(255), 1),
        std::invalid_argument);
    EXPECT_THROW(ESM4::changeActorValueModifier(state, static_cast<Owner>(255), Modifier::Script, 1),
        std::invalid_argument);
    EXPECT_THROW(ESM4::changeActorValueModifier(state, Owner::NonPlayer, Modifier::Damage,
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
