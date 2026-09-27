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
