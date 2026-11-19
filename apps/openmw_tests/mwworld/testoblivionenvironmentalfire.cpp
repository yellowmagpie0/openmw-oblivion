#include "oblivionworldfixture.hpp"

#include "apps/openmw/mwclass/esm4interactive.hpp"
#include "apps/openmw/mwmechanics/oblivioncombat.hpp"
#include "apps/openmw/mwworld/oblivionactorstats.hpp"
#include "apps/openmw/mwworld/oblivioncombatmagic.hpp"
#include "apps/openmw/mwworld/oblivionscriptmanager.hpp"
#include "apps/openmw/mwworld/player.hpp"

#include <components/esm4/runtimestate.hpp>
#include <components/esm4/loadmgef.hpp>
#include <components/esm4/loadspel.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace
{
    struct EnvironmentalFireFixture : TestingOpenMW::NativeWorldFixture
    {
        const ESM::FormKey mSourceKey = ESM::FormKey::content("headless.esm", 0x981);
        const ESM::FormKey mSpellKey = ESM::FormKey::content("headless.esm", 0x983);
        MWWorld::Ptr mSource;

        EnvironmentalFireFixture()
        {
            auto& world = mWorld;
            auto& store = world.getStore();
            ESM4::RuntimeState seed;
            seed.mPlayer.mCell = ESM::FormKey::content("headless.esm", 1);
            prepareNativeSnapshotPlayer(*this, seed);
            auto& cell = world.getWorldModel().getCell(ESM::RefId(ESM::FormId{1, 0}));
            world.getPlayer().setCell(&cell);
            ESM4::RuntimeActorValues values;
            values.mActor = ESM::FormKey::dynamic("player", 1);
            values.mBase = ESM::FormKey::dynamic("player-base", 1);
            values.mOwner = ESM4::ActorValueOwner::Player;
            values.mPlayerFormValues = {{10, 0, 0, 0}};
            for (std::size_t i = 0; i < 8; ++i) values.mValues[i].mBase = 40;
            auto& combat = *world.getOblivionCombatService();
            combat.publishPlayerValues(world.getPlayer(), values,
                MWWorld::resolveOblivionPlayerDynamicBaseSettings(store));
            ESM4::RuntimeActorLife life;
            life.mActor = values.mActor; life.mBase = values.mBase;
            combat.publishPlayerLife(world.getPlayer(), life);

            MWClass::ESM4Activator::registerSelf();
            ESM4::Activator base{};
            const auto baseKey = ESM::FormKey::content("headless.esm", 0x980);
            base.mId = {0x980, 0};
            store.getWritable<ESM4::Activator>().insertStatic(base, baseKey);
            ESM4::Reference reference{};
            reference.mId = {0x981, 0}; reference.mFormKey = mSourceKey;
            reference.mBaseObj = base.mId; reference.mBaseKey = baseKey;
            reference.mParent = cell.getCell()->getId(); reference.mParentKey = seed.mPlayer.mCell;
            store.getWritable<ESM4::Reference>().insertStatic(reference, mSourceKey);
            MWWorld::LiveCellRef<ESM4::Activator> live(reference, store.search<ESM4::Activator>(baseKey));
            mSource = MWWorld::Ptr(cell.insert(&live), &cell);
            world.getWorldModel().registerPtr(mSource);

            ESM4::EffectSetting definition{};
            definition.mId = {0x982, 0}; definition.mFormKey = ESM::FormKey::content("headless.esm", 0x982);
            definition.mEffectCode = ESM::fourCC("FIDG"); definition.mData.emplace();
            definition.mData->mFlags = 0x21001c75;
            definition.mData->mAssociatedData = 8; definition.mData->mSchool = 2;
            definition.mData->mResistanceActorValue = 61;
            store.getWritable<ESM4::EffectSetting>().insertStatic(definition, definition.mFormKey);
            ESM4::Spell spell{};
            spell.mId = {0x983, 0}; spell.mFormKey = mSpellKey; spell.mData.emplace();
            spell.mData->mType = 0; spell.mData->mFlags = 0x40;
            ESM4::SpellEffect effect;
            effect.mId = ESM::fourCC("FIDG"); effect.mMagnitude = 6; effect.mDuration = 2;
            effect.mRange = 1; effect.mActorValue = 8;
            spell.mEffects.push_back(effect);
            store.getWritable<ESM4::Spell>().insertStatic(spell, mSpellKey);
        }

        bool cast()
        {
            return mWorld.castOblivionEnvironmentalFire(mSource, mSpellKey, ESM::FormKey::dynamic("player", 1));
        }

        std::optional<MWWorld::OblivionEnvironmentalFire> pending()
        {
            const auto state = mWorld.captureOblivionRuntimeState();
            const auto owner = std::find_if(state.mReferences.begin(), state.mReferences.end(),
                [&](const auto& reference) { return reference.mKey == mSourceKey; });
            if (owner == state.mReferences.end()) throw std::runtime_error("missing fixture fire source");
            return MWWorld::readOblivionEnvironmentalFire(*owner);
        }
    };
}

TEST(OblivionWorldTest, EnvironmentalFireScriptCastTicksAndExpiresWithoutActorCastingOrMagicka)
{
    EnvironmentalFireFixture fixture;
    auto& world = fixture.mWorld;
    auto& service = *world.getOblivionCombatService();
    const float health = service.getPlayerValue(8), magicka = service.getPlayerValue(9);
    MWWorld::OblivionScriptManager host(world, world.getStore(), {"headless.esm"});
    ObScript::RuntimeContext context; context.mSelf = fixture.mSourceKey;
    const std::vector<ObScript::Value> arguments{ObScript::ReferenceValue{fixture.mSpellKey, {}},
        ObScript::ReferenceValue{ESM::FormKey::content("oblivion.esm", 0x14), {}}};
    ASSERT_NO_THROW(host.call("Cast", {}, arguments, context, {}));
    ASSERT_TRUE(fixture.pending());
    EXPECT_FLOAT_EQ(service.getPlayerValue(8), health);
    const auto before = world.captureOblivionRuntimeState().serializeBinary();
    EXPECT_THROW(world.updateOblivionEnvironmentalFire(-1), std::invalid_argument);
    EXPECT_THROW(world.updateOblivionEnvironmentalFire(std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);
    world.updateOblivionEnvironmentalFire(0);
    EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before);
    world.updateOblivionEnvironmentalFire(.25f);
    EXPECT_NEAR(service.getPlayerValue(8), health - 1.5f, .001f);
    ASSERT_TRUE(fixture.pending());
    EXPECT_FLOAT_EQ(fixture.pending()->mElapsed, .25f);
    world.updateOblivionEnvironmentalFire(20); // Only the remaining 1.75 seconds are charged.
    EXPECT_NEAR(service.getPlayerValue(8), health - 12, .001f);
    EXPECT_FALSE(fixture.pending());
    world.updateOblivionEnvironmentalFire(20);
    EXPECT_NEAR(service.getPlayerValue(8), health - 12, .001f);
    EXPECT_FLOAT_EQ(service.getPlayerValue(9), magicka);
    EXPECT_TRUE(world.captureOblivionRuntimeState().mNativeSpellCasts.empty());
    EXPECT_FALSE(world.castOblivionEnvironmentalFire(world.getPlayerPtr(), fixture.mSpellKey,
        ESM::FormKey::dynamic("player", 1)));
}

TEST(OblivionWorldTest, EnvironmentalFireRecastReplacesItsSourceAndRespectsResistanceAndGodMode)
{
    for (int fireResistance : {0, 50, 100, 150})
    {
        EnvironmentalFireFixture fixture;
        auto& world = fixture.mWorld;
        auto& service = *world.getOblivionCombatService();
        ASSERT_TRUE(world.executeOblivionActorValueCommand(world.getPlayerPtr(), 61, ESM4::ActorValueCommand::Set,
            ESM4::ActorValueCommandSource::Script, fireResistance));
        ASSERT_TRUE(world.executeOblivionActorValueCommand(world.getPlayerPtr(), 64, ESM4::ActorValueCommand::Set,
            ESM4::ActorValueCommandSource::Script, 50));
        const auto health = service.getPlayerValue(8);
        ASSERT_TRUE(fixture.cast());
        world.updateOblivionEnvironmentalFire(.5f);
        ASSERT_TRUE(fixture.cast());
        ASSERT_TRUE(fixture.pending());
        EXPECT_FLOAT_EQ(fixture.pending()->mElapsed, 0);
        world.updateOblivionEnvironmentalFire(2);
        EXPECT_NEAR(service.getPlayerValue(8), health - 15 * std::max(0.f, 1.f - fireResistance / 100.f) * .5f, .001f);
        EXPECT_FALSE(fixture.pending());
        ASSERT_TRUE(world.toggleGodMode());
        ASSERT_TRUE(fixture.cast());
        const auto protectedHealth = service.getPlayerValue(8);
        world.updateOblivionEnvironmentalFire(2);
        EXPECT_FLOAT_EQ(service.getPlayerValue(8), protectedHealth);
        EXPECT_FALSE(fixture.pending());
        EXPECT_FALSE(world.toggleGodMode());
    }
}

TEST(OblivionWorldTest, EnvironmentalFirePendingSaveResumesOnlyRemainingDamageAndRejectsInvalidOwnership)
{
    EnvironmentalFireFixture fixture;
    auto& world = fixture.mWorld;
    auto& service = *world.getOblivionCombatService();
    const auto health = service.getPlayerValue(8);
    ASSERT_TRUE(fixture.cast());
    world.updateOblivionEnvironmentalFire(.5f);
    const auto saved = ESM4::RuntimeState::deserializeBinary(world.captureOblivionRuntimeState().serializeBinary());
    ASSERT_NO_THROW(world.validateOblivionSaveState(saved));
    world.updateOblivionEnvironmentalFire(2);
    EXPECT_NEAR(service.getPlayerValue(8), health - 12, .001f);
    readNativeSnapshot(fixture, saved);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    EXPECT_NEAR(service.getPlayerValue(8), health - 3, .001f);
    ASSERT_TRUE(fixture.pending());
    EXPECT_FLOAT_EQ(fixture.pending()->mElapsed, .5f);
    world.updateOblivionEnvironmentalFire(2);
    EXPECT_NEAR(service.getPlayerValue(8), health - 12, .001f);
    EXPECT_FALSE(fixture.pending());
    for (const auto& field : {"native.fire.elapsed", "native.fire.spell", "native.fire.duration"})
    {
        auto invalid = saved;
        const auto owner = std::find_if(invalid.mReferences.begin(), invalid.mReferences.end(),
            [&](const auto& reference) { return reference.mKey == fixture.mSourceKey; });
        ASSERT_NE(owner, invalid.mReferences.end());
        owner->mCustomState.erase(field);
        EXPECT_THROW(world.validateOblivionSaveState(invalid), std::invalid_argument);
    }
    auto changed = *world.getStore().get<ESM4::Spell>().search(fixture.mSpellKey);
    changed.mEffects.front().mMagnitude = 7;
    world.getStore().getWritable<ESM4::Spell>().insertStatic(changed, fixture.mSpellKey);
    EXPECT_THROW(world.validateOblivionSaveState(saved), std::invalid_argument);
}

TEST(OblivionWorldTest, EnvironmentalFireLethalTickKeepsEnvironmentalDeathOwnershipAndCannotReplay)
{
    EnvironmentalFireFixture fixture;
    auto& world = fixture.mWorld;
    auto& service = *world.getOblivionCombatService();
    const auto settings = MWWorld::resolveOblivionPlayerDynamicBaseSettings(world.getStore());
    service.changePlayerHealth(world.getPlayer(), 5 - service.getPlayerValue(8), {}, false, {}, settings);
    ASSERT_TRUE(fixture.cast());
    world.updateOblivionEnvironmentalFire(1);
    const auto* life = service.findActorLife(ESM::FormKey::dynamic("player", 1));
    ASSERT_NE(life, nullptr);
    EXPECT_EQ(life->mPhase, ESM4::ActorLifePhase::Dead);
    EXPECT_TRUE(life->mKiller.isNull());
    EXPECT_FALSE(fixture.pending());
    const auto saved = world.captureOblivionRuntimeState().serializeBinary();
    world.updateOblivionEnvironmentalFire(10);
    EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), saved);
}
