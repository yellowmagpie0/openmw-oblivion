#include "oblivionworldfixture.hpp"

#include "apps/openmw/mwclass/esm4interactive.hpp"
#include "apps/openmw/mwclass/esm4npc.hpp"
#include "apps/openmw/mwclass/npc.hpp"
#include "apps/openmw/mwmechanics/creaturestats.hpp"
#include "apps/openmw/mwmechanics/oblivioncombat.hpp"
#include "apps/openmw/mwworld/esmstore.hpp"
#include "apps/openmw/mwworld/livecellref.hpp"
#include "apps/openmw/mwworld/oblivionactorstats.hpp"
#include "apps/openmw/mwworld/player.hpp"
#include "apps/openmw/mwworld/worldmodel.hpp"

#include <components/esm3/loadmgef.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadrace.hpp>

#include <gtest/gtest.h>
#include <limits>

namespace
{
    using TestingOpenMW::NativeWorldFixture;
    enum class Kind { Player, Npc, Creature };

    MWWorld::Ptr setupPlayer(NativeWorldFixture& fixture)
    {
        auto& world = fixture.mWorld;
        MWClass::Npc::registerSelf();
        world.setupPlayer();
        ESM4::Npc native{};
        native.mId = {7, 1};
        native.mIsTES4 = true;
        native.mFormKey = ESM::FormKey::content("Oblivion.esm", 7);
        native.mBaseConfig.tes4.levelOrOffset = 1;
        native.mData.health = 100;
        native.mData.attribs = {40, 40, 40, 40, 40, 40, 40, 40};
        world.getStore().getWritable<ESM4::Npc>().insertStatic(native, native.mFormKey);
        if (!world.initializeOblivionPlayerActor())
            throw std::runtime_error("native paralysis Player initialization failed");
        return world.getPlayerPtr();
    }

    MWWorld::Ptr setupActor(NativeWorldFixture& fixture, Kind kind, ESM4::ActorValueProcess process)
    {
        const auto player = setupPlayer(fixture);
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
        auto& combat = *world.getOblivionCombatService();
        if (kind == Kind::Player)
        {
            auto values = *combat.findActorValues(ESM::FormKey::dynamic("player", 1));
            values.mProcess = process;
            if (process == ESM4::ActorValueProcess::Low)
            {
                values.mProcessAction.reset();
                values.mProcessKnockedState.reset();
            }
            combat.publishPlayerValues(world.getPlayer(), values,
                MWWorld::resolveOblivionPlayerDynamicBaseSettings(store));
            return player;
        }
        const auto base = ESM::FormKey::content("headless.esm", 0x800);
        MWWorld::Ptr actor;
        auto& cell = world.getWorldModel().getDraftCell();
        if (kind == Kind::Npc)
        {
            MWClass::ESM4Npc::registerSelf();
            ESM4::Race race{};
            race.mId = {0x810, 0};
            race.mAttribMale = {40, 40, 40, 40, 40, 40, 40, 40};
            race.mAttribFemale = race.mAttribMale;
            store.getWritable<ESM4::Race>().insertStatic(race, ESM::FormKey::content("headless.esm", 0x810));
            auto npc = *store.search<ESM4::Npc>(base);
            npc.mRace = race.mId;
            store.getWritable<ESM4::Npc>().insertStatic(npc, base);
            ESM4::ActorCharacter reference{};
            reference.mId = {0x900, 0};
            reference.mFormKey = ESM::FormKey::content("headless.esm", 0x900);
            reference.mBaseKey = base;
            reference.mBaseObj = npc.mId;
            store.getWritable<ESM4::ActorCharacter>().insertStatic(reference, reference.mFormKey);
            MWWorld::LiveCellRef<ESM4::Npc> live(reference, store.search<ESM4::Npc>(base));
            actor = {cell.insert(&live), &cell};
        }
        else
        {
            MWClass::ESM4Creature::registerSelf();
            ESM4::Creature creature{};
            creature.mId = {0x805, 0};
            creature.mFormKey = ESM::FormKey::content("headless.esm", 0x805);
            creature.mAttackReach = 64;
            creature.mBaseConfig.tes4.levelOrOffset = 1;
            creature.mData.health = 100;
            store.getWritable<ESM4::Creature>().insertStatic(creature, creature.mFormKey);
            ESM4::ActorCreature reference{};
            reference.mId = {0x901, 0};
            reference.mFormKey = ESM::FormKey::content("headless.esm", 0x901);
            reference.mBaseKey = creature.mFormKey;
            store.getWritable<ESM4::ActorCreature>().insertStatic(reference, reference.mFormKey);
            MWWorld::LiveCellRef<ESM4::Creature> live(reference, store.search<ESM4::Creature>(creature.mFormKey));
            actor = {cell.insert(&live), &cell};
        }
        if (!world.initializeOblivionNonPlayerActor(actor, process))
            throw std::runtime_error("native paralysis actor initialization failed");
        return actor;
    }

    ESM::FormKey actorKey(const MWWorld::Ptr& actor, Kind kind)
    {
        return kind == Kind::Player ? ESM::FormKey::dynamic("player", 1) : actor.getCellRef().getFormKey();
    }

    void publish(NativeWorldFixture& fixture, const MWWorld::Ptr& actor, Kind kind,
        const ESM4::RuntimeActorValues& values)
    {
        auto& world = fixture.mWorld;
        if (kind == Kind::Player)
            world.getOblivionCombatService()->publishPlayerValues(world.getPlayer(), values,
                MWWorld::resolveOblivionPlayerDynamicBaseSettings(world.getStore()));
        else
            world.getOblivionCombatService()->publishNonPlayerValues(actor, values);
    }

    struct NativeQuery
    {
        int mBase;
        float mMaximum, mScript, mDamage;
        int mLow, mHigh, mPlayer, mCommon;
    };
    constexpr NativeQuery sQueries[] = {
#include "paralysis_expected.inc"
    };
}

TEST(OblivionParalysisTest, SharedPredicateMatchesNativeIntegerGettersForAllActorKindsAndGodMode)
{
    for (const auto kind : {Kind::Player, Kind::Npc, Kind::Creature})
        for (const auto process : {ESM4::ActorValueProcess::Low, ESM4::ActorValueProcess::Active})
        {
            SCOPED_TRACE(static_cast<int>(kind));
            SCOPED_TRACE(static_cast<int>(process));
            NativeWorldFixture fixture;
            const auto actor = setupActor(fixture, kind, process);
            auto& combat = *fixture.mWorld.getOblivionCombatService();
            auto& stats = actor.getClass().getCreatureStats(actor);
            // A stale TES3 effect must never override the native integer view.
            stats.getMagicEffects().add({ESM::MagicEffect::Paralyze}, {1.f});
            const auto key = actorKey(actor, kind);
            const auto baseline = *combat.findActorValues(key);
            unsigned checked = 0;
            for (const auto& query : sQueries)
            {
                // Real form initialization supplies zero base for AV48.
                if (query.mBase != 0)
                    continue;
                SCOPED_TRACE(checked);
                auto values = baseline;
                values.mValues[48] = {0, {query.mMaximum, query.mScript, query.mDamage}};
                publish(fixture, actor, kind, values);
                const auto expected = kind == Kind::Player ? query.mPlayer
                    : process == ESM4::ActorValueProcess::Low ? query.mLow : query.mHigh;
                EXPECT_EQ(combat.getProcessParalysis(key), expected);
                for (const bool god : {false, true})
                {
                    if (fixture.mWorld.getGodModeState() != god)
                        fixture.mWorld.toggleGodMode();
                    EXPECT_EQ(stats.isParalyzed(), expected != 0);
                    if (expected != 0)
                    {
                        EXPECT_EQ(actor.getClass().getMaxSpeed(actor), 0.f);
                        EXPECT_EQ(actor.getClass().getJump(actor), 0.f);
                    }
                }
                EXPECT_TRUE(combat.findActorValues(key)->mMeleeParalysis.empty());
                ++checked;
            }
            EXPECT_EQ(checked, 125u);
        }
}

TEST(OblivionParalysisTest, FailedPublicationPreservesAuthorityAndSharedPredicate)
{
    for (const auto kind : {Kind::Player, Kind::Npc, Kind::Creature})
    {
        NativeWorldFixture fixture;
        const auto actor = setupActor(fixture, kind, ESM4::ActorValueProcess::Active);
        auto& combat = *fixture.mWorld.getOblivionCombatService();
        const auto key = actorKey(actor, kind);
        const auto original = *combat.findActorValues(key);
        auto invalid = original;
        invalid.mValues[48].mModifiers[1] = std::numeric_limits<float>::max();
        EXPECT_THROW(publish(fixture, actor, kind, invalid), std::invalid_argument);
        EXPECT_EQ(*combat.findActorValues(key), original);
        EXPECT_FALSE(actor.getClass().getCreatureStats(actor).isParalyzed());
    }
}

TEST(OblivionParalysisTest, PartialProjectionPreservesPredicateAndUncommittedProjectionDoesNotPublish)
{
    NativeWorldFixture fixture;
    const auto actor = setupActor(fixture, Kind::Creature, ESM4::ActorValueProcess::Active);
    auto& stats = actor.getClass().getCreatureStats(actor);
    MWMechanics::OblivionActorProjectionInput input;
    input.mParalyzed = true;
    { MWMechanics::OblivionActorProjection prepared(stats, input); }
    EXPECT_FALSE(stats.isParalyzed());
    MWMechanics::OblivionActorProjection installed(stats, input);
    EXPECT_TRUE(installed.commit());
    EXPECT_TRUE(stats.isParalyzed());
    input.mParalyzed.reset();
    MWMechanics::OblivionActorProjection partial(stats, input);
    EXPECT_TRUE(partial.commit());
    EXPECT_TRUE(stats.isParalyzed());
    input.mParalyzed = false;
    MWMechanics::OblivionActorProjection cleared(stats, input);
    EXPECT_TRUE(cleared.commit());
    EXPECT_FALSE(stats.isParalyzed());
    EXPECT_FALSE(installed.commit());
    EXPECT_FALSE(stats.isParalyzed());
}

TEST(OblivionParalysisTest, OwnerApplicationExpiryAndSkippedPresentationPublishTheSamePredicate)
{
    for (const auto kind : {Kind::Player, Kind::Npc, Kind::Creature})
    {
        NativeWorldFixture fixture;
        const auto actor = setupActor(fixture, kind, ESM4::ActorValueProcess::Active);
        auto& world = fixture.mWorld;
        auto& combat = *world.getOblivionCombatService();
        auto& stats = actor.getClass().getCreatureStats(actor);
        const auto key = actorKey(actor, kind);
        auto values = *combat.findActorValues(key);
        const auto cause = combat.allocateAction();
        ASSERT_TRUE(combat.consumeAction(cause));
        values.mMeleeParalysis = {{cause, {1, 0, false, false, false}}};
        publish(fixture, actor, kind, values);
        auto settings = MWWorld::resolveOblivionFrameSettings(world.getStore());
        settings.mMagicka = {0, 0};
        settings.mFatigue.mRegeneration = {0, 0};
        unsigned preparations = 0;
        const auto update = [&](bool ready) {
            const auto prepare = [&] { ++preparations; EXPECT_FALSE(stats.isParalyzed()); };
            if (kind == Kind::Player)
                combat.updatePlayerFrameResourcesFromClock(world.getPlayer(), {0, false, false}, false,
                    settings, prepare, ready);
            else
                combat.updateNonPlayerFrameResourcesFromClock(actor, {0, false, false}, false,
                    settings, prepare, ready);
        };
        EXPECT_FALSE(stats.isParalyzed()); // Pending ownership alone is insufficient.
        combat.advanceFrameClock(.25f);
        update(false);
        EXPECT_FALSE(stats.isParalyzed());
        EXPECT_EQ(preparations, 0u);
        combat.advanceFrameClock(.25f);
        update(true);
        EXPECT_TRUE(stats.isParalyzed());
        EXPECT_EQ(combat.getProcessParalysis(key), 1);
        EXPECT_EQ(preparations, 1u);
        combat.advanceFrameClock(.75f);
        update(true);
        EXPECT_FALSE(stats.isParalyzed());
        EXPECT_EQ(combat.getProcessParalysis(key), 0);
        EXPECT_TRUE(combat.findActorValues(key)->mMeleeParalysis.empty());
        EXPECT_EQ(preparations, 1u);
    }
}

TEST(OblivionParalysisTest, LegacyPlayerStillUsesMagicEffectsAndGodMode)
{
    NativeWorldFixture fixture;
    MWClass::Npc::registerSelf();
    fixture.mWorld.setupPlayer(); // Deliberately do not install native values.
    const auto player = fixture.mWorld.getPlayerPtr();
    auto& stats = player.getClass().getCreatureStats(player);
    EXPECT_FALSE(stats.isParalyzed());
    stats.getMagicEffects().add({ESM::MagicEffect::Paralyze}, {1.f});
    EXPECT_TRUE(stats.isParalyzed());
    fixture.mWorld.toggleGodMode();
    EXPECT_FALSE(stats.isParalyzed());
}

TEST(OblivionParalysisTest, BinaryRestorePublishesAndClearsDerivedPredicateOnlyWithAuthority)
{
    for (const auto kind : {Kind::Player, Kind::Npc, Kind::Creature})
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = setupActor(fixture, kind, ESM4::ActorValueProcess::Active);
        auto& combat = *world.getOblivionCombatService();
        auto& stats = actor.getClass().getCreatureStats(actor);
        const auto key = actorKey(actor, kind);
        const std::vector<MWWorld::Ptr> residents = kind == Kind::Player
            ? std::vector<MWWorld::Ptr>{} : std::vector<MWWorld::Ptr>{actor};
        ESM4::RuntimeState state;
        state.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        state.mPlayer.mCell = ESM::FormKey::content("headless.esm", 1);
        state.mPlayer.mRace = ESM::FormKey::content("headless.esm", 0x810);
        state.mPlayer.mClass = ESM::FormKey::dynamic("fixture-class", 1);
        if (kind != Kind::Player)
        {
            ESM4::RuntimeReferenceState reference;
            reference.mKey = key;
            reference.mBase = combat.findActorValues(key)->mBase;
            reference.mCell = state.mPlayer.mCell;
            state.mReferences.push_back(reference);
        }
        combat.capture(state);
        const auto setScript = [&](float amount) {
            for (auto& values : state.mNativeActorValues)
                if (values.mActor == key)
                    values.mValues[48].mModifiers[1] = amount;
        };
        const auto prepare = [&] {
            MWMechanics::OblivionCombatService replacement;
            replacement.restore(ESM4::RuntimeState::deserializeBinary(state.serializeBinary()));
            return combat.prepareRestoredActorState(std::move(replacement), residents, &world.getPlayer());
        };
        setScript(-1.25f);
        auto active = prepare();
        EXPECT_FALSE(stats.isParalyzed());
        EXPECT_EQ(combat.getProcessParalysis(key), 0);
        ASSERT_TRUE(active.commit());
        EXPECT_TRUE(stats.isParalyzed());
        EXPECT_EQ(combat.getProcessParalysis(key), -1);
        setScript(.75f);
        { auto discarded = prepare(); }
        EXPECT_TRUE(stats.isParalyzed());
        EXPECT_EQ(combat.getProcessParalysis(key), -1);
        auto clear = prepare();
        EXPECT_TRUE(stats.isParalyzed());
        ASSERT_TRUE(clear.commit());
        EXPECT_FALSE(stats.isParalyzed());
        EXPECT_EQ(combat.getProcessParalysis(key), 0);
        EXPECT_TRUE(combat.findActorValues(key)->mMeleeParalysis.empty());
        EXPECT_FALSE(active.commit());
        EXPECT_FALSE(stats.isParalyzed());
    }
}
