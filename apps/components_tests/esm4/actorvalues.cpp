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
