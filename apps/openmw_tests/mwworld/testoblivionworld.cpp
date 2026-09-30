#include <gtest/gtest.h>

#include <array>
#include <fstream>
#include <stdexcept>
#include <sstream>

#include <components/esm/records.hpp>
#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/files/hash.hpp>
#include <components/files/collections.hpp>
#include <components/loadinglistener/loadinglistener.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/testing/util.hpp>

#include "apps/openmw/mwbase/environment.hpp"
#include "apps/openmw/mwclass/esm4npc.hpp"
#include "apps/openmw/mwclass/esm4interactive.hpp"
#include "apps/openmw/mwmechanics/oblivioncombat.hpp"
#include "apps/openmw/mwlua/context.hpp"
#include "apps/openmw/mwlua/localscripts.hpp"
#include "apps/openmw/mwlua/luamanagerimp.hpp"
#include "apps/openmw/mwlua/stats.hpp"
#include "apps/openmw/mwsound/soundmanagerimp.hpp"
#include "apps/openmw/mwworld/worldimp.hpp"

namespace
{
    // A real native content-load path with no renderer, input, physics or audio
    // device. Actors enter authority explicitly; this is not automatic actor
    // publication or a normal-input gameplay fixture.
    struct NativeWorldFixture
    {
        const std::filesystem::path mDirectory = TestingOpenMW::currentTestDirPath();
        MWBase::Environment mEnvironment;
        VFS::Manager mVfs;
        Resource::ResourceSystem mResources{&mVfs, 0., nullptr};
        MWWorld::World mWorld{&mResources, -1, "", mDirectory, ESM::GameProfile::Oblivion};
        std::unique_ptr<MWLua::LuaManager> mLuaManager;
        std::unique_ptr<MWSound::SoundManager> mSoundManager;

        NativeWorldFixture()
        {
            mEnvironment.setResourceSystem(mResources);
            mEnvironment.setWorld(mWorld);
            mEnvironment.setWorldModel(mWorld.getWorldModel());
            mEnvironment.setESMStore(mWorld.getStore());
            mLuaManager = std::make_unique<MWLua::LuaManager>(&mVfs, std::filesystem::path{});
            mEnvironment.setLuaManager(*mLuaManager);
            mSoundManager = std::make_unique<MWSound::SoundManager>(&mVfs, false);
            mEnvironment.setSoundManager(*mSoundManager);

            const auto bytes = [](const auto& value) {
                return std::string(reinterpret_cast<const char*>(&value), sizeof(value));
            };
            const auto subrecord = [&](std::uint32_t tag, const std::string& data) {
                return bytes(tag) + bytes(static_cast<std::uint16_t>(data.size())) + data;
            };
            const auto record = [&](std::uint32_t tag, std::uint32_t id, const std::string& data) {
                return bytes(tag) + bytes(static_cast<std::uint32_t>(data.size()))
                    + bytes(std::uint32_t{0}) + bytes(id) + bytes(std::uint32_t{0}) + data;
            };
            ESM4::ACBS_TES4 config{};
            config.baseSpell = 20;
            config.fatigue = 40;
            config.levelOrOffset = config.calcMin = config.calcMax = 1;
            std::array<std::uint8_t, 21> skills;
            skills.fill(5);
            std::array<std::uint8_t, 8> attributes;
            attributes.fill(40);
            const auto header = record(ESM4::REC_TES4, 0,
                subrecord(ESM::fourCC("HEDR"), bytes(1.f) + bytes(std::uint32_t{1}) + bytes(std::uint32_t{0x801})));
            const auto npc = record(ESM4::REC_NPC_, 0x800,
                subrecord(ESM::fourCC("EDID"), std::string("Player\0", 7))
                + subrecord(ESM::fourCC("ACBS"), bytes(config).substr(0, 16))
                + subrecord(ESM::fourCC("DATA"), bytes(skills) + bytes(std::uint32_t{100}) + bytes(attributes)));
            {
                std::ofstream stream(mDirectory / "headless.esm", std::ios::binary);
                stream << header << npc;
                if (!stream)
                    throw std::runtime_error("failed to write native world fixture");
            }
            Files::Collections files(Files::PathContainer{mDirectory});
            Loading::Listener listener;
            mWorld.loadData(files, {"headless.esm"}, {}, nullptr, &listener);
        }
    };

    struct InspectableScripts : MWLua::LocalScripts
    {
        using LocalScripts::LocalScripts;
        MWLua::SelfObject& self() { return mData; }
    };

    MWWorld::Ptr addNativeNpc(NativeWorldFixture& fixture, std::uint32_t referenceId, std::uint32_t health = 100)
    {
        auto& store = fixture.mWorld.getStore();
        ESM4::Race race{};
        race.mId = {0x810, 0};
        race.mAttribMale = {40, 40, 40, 40, 40, 40, 40, 40};
        race.mAttribFemale = race.mAttribMale;
        store.getWritable<ESM4::Race>().insertStatic(race, ESM::FormKey::content("headless.esm", 0x810));
        const auto base = ESM::FormKey::content("headless.esm", 0x800);
        auto npc = *store.search<ESM4::Npc>(base);
        npc.mRace = race.mId;
        npc.mData.health = health;
        npc.mData.attribs.strength = 37;
        npc.mAIData = {13, 27, 39, 101, 0, 0, 0, 0};
        store.getWritable<ESM4::Npc>().insertStatic(npc, base);
        MWClass::ESM4Npc::registerSelf();
        ESM4::ActorCharacter reference{};
        reference.mId = {referenceId, 0};
        reference.mFormKey = ESM::FormKey::content("headless.esm", referenceId);
        reference.mBaseKey = base;
        store.getWritable<ESM4::ActorCharacter>().insertStatic(reference, reference.mFormKey);
        MWWorld::LiveCellRef<ESM4::Npc> live(reference, store.search<ESM4::Npc>(base));
        auto& cell = fixture.mWorld.getWorldModel().getDraftCell();
        return MWWorld::Ptr(cell.insert(&live), &cell);
    }

    ESM4::RuntimeState captureNativeActorState(NativeWorldFixture& fixture, const MWWorld::Ptr& ptr)
    {
        ESM4::RuntimeState state;
        state.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        state.mPlayer.mCell = ESM::FormKey::content("headless.esm", 1);
        state.mPlayer.mRace = ESM::FormKey::content("headless.esm", 0x810);
        state.mPlayer.mClass = ESM::FormKey::dynamic("fixture-class", 1);
        ESM4::RuntimeReferenceState reference;
        reference.mKey = ptr.getCellRef().getFormKey();
        reference.mBase = ptr.getType() == ESM::REC_NPC_4
            ? ptr.get<ESM4::Npc>()->mBase->mFormKey : ptr.get<ESM4::Creature>()->mBase->mFormKey;
        reference.mCell = state.mPlayer.mCell;
        reference.mEnabled = ptr.getRefData().isEnabled();
        state.mReferences.push_back(reference);
        fixture.mWorld.getOblivionCombatService()->capture(state);
        return state;
    }

    void readNativeSnapshot(NativeWorldFixture& fixture, const ESM4::RuntimeState& state)
    {
        ESM::ESMWriter writer;
        auto stream = std::make_unique<std::stringstream>();
        writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion);
        writer.save(*stream);
        writer.startRecord(ESM::REC_T4ST);
        state.save(writer);
        writer.endRecord(ESM::REC_T4ST);
        ESM::ESMReader reader;
        reader.open(std::move(stream), "native-marker-fixture");
        ASSERT_EQ(reader.getRecName(), ESM::REC_T4ST);
        reader.getRecHeader();
        fixture.mWorld.readRecord(reader, ESM::REC_T4ST);
    }
    TEST(OblivionWorldTest, actualReaderLegacyMarkerAdoptionIsTransactional)
    {
        NativeWorldFixture fixture;
        const auto ptr = addNativeNpc(fixture, 0x900);
        auto& world = fixture.mWorld;
        auto& service = *world.getOblivionCombatService();
        const auto key = ptr.getCellRef().getFormKey();
        auto state = captureNativeActorState(fixture, ptr);
        std::ifstream content(fixture.mDirectory / "headless.esm", std::ios::binary);
        state.mContent.push_back({"headless.esm", "sha256:" + Files::getSha256("headless.esm", content)});
        state.mReferences[0].mCustomState["obscript.dead"]=true;
        readNativeSnapshot(fixture, state);
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(ptr, ESM4::ActorValueProcess::Active));
        ASSERT_NE(service.findActorLife(key), nullptr);
        EXPECT_EQ(service.findActorLife(key)->mPhase, ESM4::ActorLifePhase::Dead);
        EXPECT_FALSE(service.takeNextDeathEvent());
        EXPECT_EQ(service.getDeadCount(service.findActorLife(key)->mBase), 0);
        auto alive = *service.findActorLife(key);
        alive.mPhase = ESM4::ActorLifePhase::Alive;
        service.publishNonPlayerLife(ptr, alive);
        // A consumed true marker must not conflict with this later alive authority.
        EXPECT_TRUE(world.initializeOblivionNonPlayerActor(ptr, ESM4::ActorValueProcess::Low));
        auto foreign = state;
        foreign.mContent[0].mFingerprint="sha256:0000000000000000000000000000000000000000000000000000000000000000";
        EXPECT_THROW(readNativeSnapshot(fixture, foreign), std::runtime_error);
        EXPECT_TRUE(world.initializeOblivionNonPlayerActor(ptr, ESM4::ActorValueProcess::Low));
        readNativeSnapshot(fixture, state);
        const auto original = *service.findActorValues(key);
        EXPECT_THROW(world.initializeOblivionNonPlayerActor(ptr, ESM4::ActorValueProcess::Low), std::invalid_argument);
        EXPECT_EQ(*service.findActorValues(key), original);
        EXPECT_EQ(*service.findActorLife(key), alive);
        service.clear();
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(ptr, ESM4::ActorValueProcess::Low));
        EXPECT_EQ(service.findActorLife(key)->mPhase, ESM4::ActorLifePhase::Dead);
        state.mReferences[0].mCustomState["obscript.dead"]=std::int64_t{42};
        readNativeSnapshot(fixture, state);
        service.clear();
        EXPECT_THROW(world.initializeOblivionNonPlayerActor(ptr, ESM4::ActorValueProcess::Low), std::invalid_argument);
        EXPECT_EQ(service.findActorValues(key), nullptr);
        EXPECT_EQ(service.findActorLife(key), nullptr);
        EXPECT_THROW(world.initializeOblivionNonPlayerActor(ptr, ESM4::ActorValueProcess::Active), std::invalid_argument);
        EXPECT_FALSE(service.takeNextDeathEvent());
    }

    TEST(OblivionWorldTest, supportedLegacySchemaDeathMarkersOverrideFreshClassDefaults)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& service = *world.getOblivionCombatService();
        const auto ptr = addNativeNpc(fixture, 0x900);
        const auto key = ptr.getCellRef().getFormKey();
        const auto empty = captureNativeActorState(fixture, ptr);
        for (std::uint32_t version = 7; version <= ESM4::CurrentRuntimeStateVersion; ++version)
        {
            for (const bool dead : {true, false})
            {
                SCOPED_TRACE(version);
                SCOPED_TRACE(dead);
                service.clear();
                auto state = empty;
                state.mVersion = version;
                state.mReferences[0].mCustomState["obscript.dead"] = dead;
                ASSERT_NO_FATAL_FAILURE(readNativeSnapshot(fixture, state));
                ASSERT_TRUE(world.initializeOblivionNonPlayerActor(ptr, ESM4::ActorValueProcess::Low));
                ASSERT_NE(service.findActorLife(key), nullptr);
                EXPECT_EQ(service.findActorLife(key)->mPhase,
                    dead ? ESM4::ActorLifePhase::Dead : ESM4::ActorLifePhase::Alive);
                EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).isDead(), dead);
                EXPECT_EQ(service.getDeadCount(service.findActorLife(key)->mBase), 0);
                EXPECT_FALSE(service.takeNextDeathEvent());
                // Removal is checked against the opposite subsequent phase.
                auto opposite = *service.findActorLife(key);
                opposite.mPhase = dead ? ESM4::ActorLifePhase::Alive : ESM4::ActorLifePhase::Dead;
                service.publishNonPlayerLife(ptr, opposite);
                ASSERT_TRUE(world.initializeOblivionNonPlayerActor(ptr, ESM4::ActorValueProcess::Active));
                EXPECT_EQ(*service.findActorLife(key), opposite);
                EXPECT_EQ(service.findActorValues(key)->mProcess, ESM4::ActorValueProcess::Low);
            }
        }
    }

    TEST(OblivionWorldTest, restoredAuthorityRefreshesCachedResidentViews)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& service = *world.getOblivionCombatService();
        const auto ptr = addNativeNpc(fixture, 0x900);
        ptr.getRefData().disable();
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(ptr, ESM4::ActorValueProcess::Active));
        const auto saved = captureNativeActorState(fixture, ptr);
        ASSERT_TRUE(world.executeOblivionActorValueCommand(ptr, 8, ESM4::ActorValueCommand::Mod,
            ESM4::ActorValueCommandSource::Console, -7));
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 93);
        MWMechanics::OblivionCombatService replacement;
        replacement.restore(saved, world.getStore());
        const auto residents = world.getWorldModel().getResidentPtrs();
        service.installRestoredNonPlayerState(std::move(replacement), residents);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 100);
        EXPECT_EQ(captureNativeActorState(fixture, ptr).mNativeActorValues, saved.mNativeActorValues);
        EXPECT_FALSE(service.takeNextDeathEvent());
    }

    TEST(OblivionWorldTest, restoredResidentBatchRejectsBeforeAuthorityOrViewCommit)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& service = *world.getOblivionCombatService();
        const auto first = addNativeNpc(fixture, 0x900);
        const auto second = addNativeNpc(fixture, 0x901);
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(first, ESM4::ActorValueProcess::Active));
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(second, ESM4::ActorValueProcess::Low));
        auto saved = captureNativeActorState(fixture, first);
        auto reference = saved.mReferences.front();
        reference.mKey = second.getCellRef().getFormKey();
        saved.mReferences.push_back(reference);
        saved.validate();
        ASSERT_TRUE(world.executeOblivionActorValueCommand(first, 8, ESM4::ActorValueCommand::Mod,
            ESM4::ActorValueCommandSource::Console, -7));
        ASSERT_TRUE(world.executeOblivionActorValueCommand(second, 8, ESM4::ActorValueCommand::Mod,
            ESM4::ActorValueCommandSource::Console, -11));
        auto dead = *service.findActorLife(first.getCellRef().getFormKey());
        dead.mPhase = ESM4::ActorLifePhase::Dead;
        service.publishNonPlayerLife(first, dead);
        const auto before = captureNativeActorState(fixture, first);
        MWMechanics::OblivionCombatService replacement;
        replacement.restore(saved, world.getStore());
        auto wrongBase = *second.get<ESM4::Npc>()->mBase;
        wrongBase.mFormKey = ESM::FormKey::content("headless.esm", 0x899);
        ESM4::ActorCharacter duplicate{};
        duplicate.mId = {0x901, 0};
        duplicate.mFormKey = second.getCellRef().getFormKey();
        duplicate.mBaseKey = wrongBase.mFormKey;
        MWWorld::LiveCellRef<ESM4::Npc> wrong(duplicate, &wrongBase);
        const MWWorld::Ptr wrongPtr(&wrong, first.getCell());
        const std::array invalidResidents{first, wrongPtr};
        EXPECT_THROW(service.installRestoredNonPlayerState(std::move(replacement), invalidResidents),
            std::invalid_argument);
        EXPECT_EQ(captureNativeActorState(fixture, first).mNativeActorValues, before.mNativeActorValues);
        EXPECT_EQ(captureNativeActorState(fixture, first).mNativeActorLife, before.mNativeActorLife);
        EXPECT_EQ(first.getClass().getCreatureStats(first).getHealth().getCurrent(), 93);
        EXPECT_TRUE(first.getClass().getCreatureStats(first).isDead());
        EXPECT_EQ(second.getClass().getCreatureStats(second).getHealth().getCurrent(), 89);
        // Two distinct live references claiming the same stable key must
        // reject even if both use the correct base. Otherwise only one view
        // would refresh and the second would silently retain stale stats.
        duplicate.mBaseKey = second.get<ESM4::Npc>()->mBase->mFormKey;
        MWWorld::LiveCellRef<ESM4::Npc> duplicateLive(duplicate, second.get<ESM4::Npc>()->mBase);
        const MWWorld::Ptr duplicatePtr(&duplicateLive, second.getCell());
        const std::array ambiguousResidents{first, second, duplicatePtr};
        EXPECT_THROW(service.installRestoredNonPlayerState(std::move(replacement), ambiguousResidents),
            std::invalid_argument);
        EXPECT_EQ(captureNativeActorState(fixture, first).mNativeActorValues, before.mNativeActorValues);
        EXPECT_EQ(first.getClass().getCreatureStats(first).getHealth().getCurrent(), 93);
        EXPECT_TRUE(first.getClass().getCreatureStats(first).isDead());
        EXPECT_EQ(second.getClass().getCreatureStats(second).getHealth().getCurrent(), 89);
        // The rejected replacement remains intact and can be installed once
        // the roster is corrected. Duplicate valid pointers are harmless.
        const std::array residents{first, second, first};
        service.installRestoredNonPlayerState(std::move(replacement), residents);
        EXPECT_EQ(first.getClass().getCreatureStats(first).getHealth().getCurrent(), 100);
        EXPECT_FALSE(first.getClass().getCreatureStats(first).isDead());
        EXPECT_EQ(second.getClass().getCreatureStats(second).getHealth().getCurrent(), 100);
        EXPECT_EQ(captureNativeActorState(fixture, first).mNativeActorValues, saved.mNativeActorValues);
        EXPECT_EQ(captureNativeActorState(fixture, first).mNativeActorLife, saved.mNativeActorLife);
        EXPECT_THROW(service.installRestoredNonPlayerState(std::move(service), residents), std::invalid_argument);
        EXPECT_FALSE(service.takeNextDeathEvent());
    }

    TEST(OblivionWorldTest, lazyNativeClassConstructionReadsExistingRestoredAuthority)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& service = *world.getOblivionCombatService();
        const auto ptr = addNativeNpc(fixture, 0x900);
        const auto key = ptr.getCellRef().getFormKey();
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(ptr, ESM4::ActorValueProcess::Active));
        ASSERT_TRUE(world.executeOblivionActorValueCommand(ptr, 8, ESM4::ActorValueCommand::Mod,
            ESM4::ActorValueCommandSource::Console, -7));
        ASSERT_TRUE(world.requestOblivionStatBase(ptr, 0, 257.f));
        auto dead = *service.findActorLife(key);
        dead.mPhase = ESM4::ActorLifePhase::Dead;
        service.publishNonPlayerLife(ptr, dead);
        const auto saved = captureNativeActorState(fixture, ptr);
        service.clear();
        service.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()), world.getStore());
        ptr.getRefData().setCustomData(nullptr);
        // The ordinary class query must reconstruct the existing authority view.
        const auto& stats = ptr.getClass().getCreatureStats(ptr);
        EXPECT_TRUE(stats.getHealth().isNativeProjection());
        EXPECT_EQ(stats.getHealth().getCurrent(), 93);
        EXPECT_EQ(stats.getAttribute(ESM::Attribute::Strength).getBase(), 1);
        EXPECT_TRUE(stats.isDead());
        const auto after = captureNativeActorState(fixture, ptr);
        EXPECT_EQ(after.mNativeActorValues, saved.mNativeActorValues);
        EXPECT_EQ(after.mNativeActorLife, saved.mNativeActorLife);
        EXPECT_EQ(after.mNativeDeathCounts, saved.mNativeDeathCounts);
        EXPECT_FALSE(service.takeNextDeathEvent());
    }

    TEST(OblivionWorldTest, nativeConstructionReprojectsAuthorityWithoutResettingRestoredState)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& service = *world.getOblivionCombatService();
        const auto ptr = addNativeNpc(fixture, 0x900, 16777217);
        ptr.getRefData().disable();
        const auto key = ptr.getCellRef().getFormKey();
        EXPECT_EQ(service.findActorValues(key), nullptr);
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(ptr, ESM4::ActorValueProcess::Active));
        ASSERT_NE(service.findActorValues(key), nullptr);
        ASSERT_NE(service.findActorLife(key), nullptr);
        EXPECT_EQ(service.findActorValues(key)->mNonPlayerFormHealth, 16777217);
        EXPECT_EQ(service.findActorLife(key)->mPhase, ESM4::ActorLifePhase::Alive);
        EXPECT_EQ(service.getNonPlayerIntegerValue(ptr, 8), 16777217);
        EXPECT_TRUE(ptr.getClass().getCreatureStats(ptr).getHealth().isNativeProjection());
        EXPECT_FALSE(ptr.getRefData().isEnabled());
        EXPECT_TRUE(world.getWorldModel().getPtr(ptr.getCellRef().getRefNum()).isEmpty());
        EXPECT_FALSE(service.takeNextDeathEvent());
        EXPECT_EQ(service.getDeadCount(ptr.get<ESM4::Npc>()->mBase->mFormKey), 0);
        ASSERT_TRUE(world.executeOblivionActorValueCommand(ptr, 8, ESM4::ActorValueCommand::Mod,
            ESM4::ActorValueCommandSource::Console, -7));
        ASSERT_TRUE(world.requestOblivionStatBase(ptr, 0, 257.f));
        service.advanceFrameClock(.125f);
        ASSERT_TRUE(world.updateOblivionFrameResources(ptr, 0.f, false));
        const auto saved = captureNativeActorState(fixture, ptr);
        const auto restored = ESM4::RuntimeState::deserializeBinary(saved.serializeBinary());
        service.clear();
        service.restore(restored, world.getStore());
        ptr.getRefData().setCustomData(nullptr);
        // A changed construction process request cannot reset restored process,
        // modifiers, shared overrides, lifecycle or completed actor timestamps.
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(ptr, ESM4::ActorValueProcess::Low));
        EXPECT_EQ(service.findActorValues(key)->mProcess, ESM4::ActorValueProcess::Active);
        EXPECT_EQ(service.getNonPlayerIntegerValue(ptr, 8), 16777210);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 16777210.f);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getAttribute(ESM::Attribute::Strength).getBase(), 1);
        const auto after = captureNativeActorState(fixture, ptr);
        EXPECT_EQ(after.mNativeActorValues, saved.mNativeActorValues);
        EXPECT_EQ(after.mNativeActorBases, saved.mNativeActorBases);
        EXPECT_EQ(after.mNativeActorLife, saved.mNativeActorLife);
        EXPECT_EQ(after.mNativeActorUpdateTimes, saved.mNativeActorUpdateTimes);
        EXPECT_EQ(after.mNativeActorManagerTime, saved.mNativeActorManagerTime);
        EXPECT_EQ(after.mNativeDeathCounts, saved.mNativeDeathCounts);
        EXPECT_FALSE(service.takeNextDeathEvent());

        const auto sibling = addNativeNpc(fixture, 0x901, 16777217);
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(sibling, ESM4::ActorValueProcess::Low));
        const auto* siblingValues = service.findActorValues(sibling.getCellRef().getFormKey());
        ASSERT_NE(siblingValues, nullptr);
        EXPECT_EQ(siblingValues->mValues[0].mBase, 1);
        EXPECT_EQ(siblingValues->mProcess, ESM4::ActorValueProcess::Low);
        EXPECT_FALSE(siblingValues->mValues[8].mModifiers[2]);
        EXPECT_EQ(service.getNonPlayerIntegerValue(sibling, 8), 16777217);
        EXPECT_EQ(service.getNonPlayerIntegerValue(ptr, 8), 16777210);
        auto terminal = *service.findActorLife(key);
        terminal.mPhase = ESM4::ActorLifePhase::Dead;
        service.publishNonPlayerLife(ptr, terminal);
        EXPECT_THROW(service.initializeNonPlayerActor(ptr, world.getStore(), {},
            ESM4::ActorValueProcess::Low, false), std::invalid_argument);
        EXPECT_EQ(*service.findActorLife(key), terminal);
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(ptr, ESM4::ActorValueProcess::Low));
        EXPECT_TRUE(ptr.getClass().getCreatureStats(ptr).isDead());
        EXPECT_EQ(*service.findActorLife(key), terminal);
        EXPECT_EQ(service.getDeadCount(terminal.mBase), 0);
        EXPECT_FALSE(service.takeNextDeathEvent());
    }

    TEST(OblivionWorldTest, nativeCreatureConstructionPublishesRuntimeAliasesAndAdoptsLegacyDeathWithoutEvents)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
        auto& service = *world.getOblivionCombatService();
        MWClass::ESM4Creature::registerSelf();
        ESM4::Creature creature{};
        creature.mId = {0x820, 0};
        creature.mFormKey = ESM::FormKey::content("headless.esm", 0x820);
        creature.mAttackReach = 64;
        creature.mBaseConfig.tes4.levelOrOffset = 1;
        creature.mBaseConfig.tes4.baseSpell = 20;
        creature.mBaseConfig.tes4.fatigue = 40;
        creature.mData.health = 99;
        creature.mData.combat = 10;
        creature.mData.magic = 20;
        creature.mData.stealth = 30;
        creature.mData.damage = 40;
        creature.mData.attribs.strength = 37;
        store.getWritable<ESM4::Creature>().insertStatic(creature, creature.mFormKey);
        ESM4::ActorCreature reference{};
        reference.mId = {0x920, 0};
        reference.mFormKey = ESM::FormKey::content("headless.esm", 0x920);
        reference.mBaseKey = creature.mFormKey;
        store.getWritable<ESM4::ActorCreature>().insertStatic(reference, reference.mFormKey);
        MWWorld::LiveCellRef<ESM4::Creature> live(reference, store.search<ESM4::Creature>(creature.mFormKey));
        auto& cell = world.getWorldModel().getDraftCell();
        MWWorld::Ptr ptr(cell.insert(&live), &cell);
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(ptr, ESM4::ActorValueProcess::Active));
        EXPECT_EQ(service.findActorValues(reference.mFormKey)->mNonPlayerFormHealth, 99);
        EXPECT_EQ(service.findActorValues(reference.mFormKey)->mValues[28].mBase, 30);
        EXPECT_EQ(service.getNonPlayerValue(ptr, 28), 10);
        EXPECT_EQ(ptr.getClass().getSkill(ptr, ESM::Skill::Marksman), 10);
        EXPECT_TRUE(ptr.getClass().getCreatureStats(ptr).getHealth().isNativeProjection());
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 99);
        const auto original = *service.findActorValues(reference.mFormKey);
        service.clear();
        service.initializeNonPlayerActor(ptr, store, {}, ESM4::ActorValueProcess::Low, true);
        EXPECT_TRUE(ptr.getClass().getCreatureStats(ptr).isDead());
        EXPECT_EQ(service.findActorLife(reference.mFormKey)->mPhase, ESM4::ActorLifePhase::Dead);
        EXPECT_EQ(service.findActorValues(reference.mFormKey)->mValues, original.mValues);
        EXPECT_EQ(service.getDeadCount(creature.mFormKey), 0);
        EXPECT_FALSE(service.takeNextDeathEvent());
        EXPECT_TRUE(world.getWorldModel().getPtr(ptr.getCellRef().getRefNum()).isEmpty());
    }

    TEST(OblivionWorldTest, lazyNativeCreatureConstructionReadsExistingRestoredAuthority)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
        auto& service = *world.getOblivionCombatService();
        MWClass::ESM4Creature::registerSelf();
        ESM4::Creature creature{};
        creature.mId = {0x820, 0};
        creature.mFormKey = ESM::FormKey::content("headless.esm", 0x820);
        creature.mAttackReach = 64;
        creature.mBaseConfig.tes4.levelOrOffset = 1;
        creature.mBaseConfig.tes4.baseSpell = 20;
        creature.mBaseConfig.tes4.fatigue = 40;
        creature.mData.health = 99;
        creature.mData.combat = 10;
        creature.mData.magic = 20;
        creature.mData.stealth = 30;
        creature.mData.damage = 40;
        creature.mData.attribs.strength = 37;
        store.getWritable<ESM4::Creature>().insertStatic(creature, creature.mFormKey);
        ESM4::ActorCreature reference{};
        reference.mId = {0x920, 0};
        reference.mFormKey = ESM::FormKey::content("headless.esm", 0x920);
        reference.mBaseKey = creature.mFormKey;
        store.getWritable<ESM4::ActorCreature>().insertStatic(reference, reference.mFormKey);
        MWWorld::LiveCellRef<ESM4::Creature> live(reference, store.search<ESM4::Creature>(creature.mFormKey));
        auto& cell = world.getWorldModel().getDraftCell();
        MWWorld::Ptr ptr(cell.insert(&live), &cell);
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(ptr, ESM4::ActorValueProcess::Active));
        ASSERT_TRUE(world.executeOblivionActorValueCommand(ptr, 8, ESM4::ActorValueCommand::Mod,
            ESM4::ActorValueCommandSource::Console, -7));
        auto dead = *service.findActorLife(reference.mFormKey);
        dead.mPhase = ESM4::ActorLifePhase::Dead;
        service.publishNonPlayerLife(ptr, dead);
        const auto saved = captureNativeActorState(fixture, ptr);
        service.clear();
        service.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()), store);
        ptr.getRefData().setCustomData(nullptr);
        const auto& stats = ptr.getClass().getCreatureStats(ptr);
        EXPECT_TRUE(stats.getHealth().isNativeProjection());
        EXPECT_EQ(stats.getHealth().getCurrent(), 92);
        EXPECT_TRUE(stats.isDead());
        EXPECT_EQ(ptr.getClass().getSkill(ptr, ESM::Skill::Marksman), 10);
        const auto after = captureNativeActorState(fixture, ptr);
        EXPECT_EQ(after.mNativeActorValues, saved.mNativeActorValues);
        EXPECT_EQ(after.mNativeActorLife, saved.mNativeActorLife);
        EXPECT_EQ(after.mNativeDeathCounts, saved.mNativeDeathCounts);
        EXPECT_FALSE(service.takeNextDeathEvent());
    }

    TEST(OblivionWorldTest, nativeConstructionAndRestoreFailuresPreserveAuthority)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& service = *world.getOblivionCombatService();
        const auto ptr = addNativeNpc(fixture, 0x900);
        const auto key = ptr.getCellRef().getFormKey();
        EXPECT_FALSE(world.initializeOblivionNonPlayerActor({}, ESM4::ActorValueProcess::Active));
        EXPECT_THROW(world.initializeOblivionNonPlayerActor(ptr, static_cast<ESM4::ActorValueProcess>(255)),
            std::invalid_argument);
        EXPECT_EQ(service.findActorValues(key), nullptr);
        EXPECT_EQ(service.findActorLife(key), nullptr);
        ASSERT_TRUE(world.getStore().getWritable<ESM::Attribute>().eraseStatic(ESM::Attribute::Luck));
        EXPECT_THROW(world.initializeOblivionNonPlayerActor(ptr, ESM4::ActorValueProcess::Active), std::out_of_range);
        EXPECT_EQ(service.findActorValues(key), nullptr);
        EXPECT_EQ(service.findActorLife(key), nullptr);
        EXPECT_EQ(ptr.getRefData().getCustomData(), nullptr);
        ESM::Attribute luck{};
        luck.mId = ESM::Attribute::Luck;
        world.getStore().getWritable<ESM::Attribute>().insertStatic(luck);
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(ptr, ESM4::ActorValueProcess::Active));
        const auto original = captureNativeActorState(fixture, ptr);
        auto bad = original;
        bad.mNativeActorValues[0].mValues[9].mModifiers[1] = 1e32f;
        bad.mNativeActorValues[0].mValues[40].mBase = 1e9f;
        ASSERT_NO_THROW(bad.validate()); // Ordinary composition alone is finite.
        EXPECT_THROW(service.restore(bad, world.getStore()), std::invalid_argument);
        const auto after = captureNativeActorState(fixture, ptr);
        EXPECT_EQ(after.mNativeActorValues, original.mNativeActorValues);
        EXPECT_EQ(after.mNativeActorLife, original.mNativeActorLife);
        EXPECT_EQ(after.mPhysicalActions, original.mPhysicalActions);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getMagicka().getCurrent(), 20);
        MWWorld::World legacy(nullptr, -1, "", {}, ESM::GameProfile::Morrowind);
        EXPECT_FALSE(legacy.initializeOblivionNonPlayerActor(ptr, ESM4::ActorValueProcess::Active));
    }

    TEST(OblivionWorldTest, playerIdentityQueriesAreEmptyBeforeRendererSetup)
    {
        for (const auto profile : {ESM::GameProfile::Morrowind, ESM::GameProfile::Oblivion})
        {
            MWWorld::World world(nullptr, -1, "", {}, profile);
            EXPECT_TRUE(world.getPlayerPtr().isEmpty());
            EXPECT_TRUE(world.getPlayerConstPtr().isEmpty());
        }
    }

    TEST(OblivionWorldTest, nativeWorldCommandsAndLuaCacheRefreshInactiveSharedBaseSiblings)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        EXPECT_EQ(world.getGameProfile(), ESM::GameProfile::Oblivion);
        ASSERT_NE(world.getOblivionCombatService(), nullptr);
        EXPECT_TRUE(world.getPlayerPtr().isEmpty());
        auto& store = world.getStore();
        ESM4::Race race{};
        race.mId = {0x810, 0};
        race.mAttribMale = {40, 40, 40, 40, 40, 40, 40, 40};
        race.mAttribFemale = race.mAttribMale;
        store.getWritable<ESM4::Race>().insertStatic(race, ESM::FormKey::content("headless.esm", 0x810));
        const auto base = ESM::FormKey::content("headless.esm", 0x800);
        auto npc = *store.search<ESM4::Npc>(base);
        npc.mRace = race.mId;
        store.getWritable<ESM4::Npc>().insertStatic(npc, base);
        MWClass::ESM4Npc::registerSelf();
        auto& model = world.getWorldModel();
        auto& cell = model.getDraftCell();
        auto& service = *world.getOblivionCombatService();
        std::array<MWWorld::Ptr, 3> ptrs;
        for (std::size_t i = 0; i < ptrs.size(); ++i)
        {
            ESM4::ActorCharacter reference{};
            reference.mId = {static_cast<std::uint32_t>(0x900 + i), 0};
            reference.mFormKey = ESM::FormKey::content("headless.esm", 0x900 + i);
            reference.mBaseKey = base;
            store.getWritable<ESM4::ActorCharacter>().insertStatic(reference, reference.mFormKey);
            MWWorld::LiveCellRef<ESM4::Npc> live(reference, store.search<ESM4::Npc>(base));
            if (i == 1)
                live.mData.disable();
            if (i == 2)
                live.mData.setDeletedByContentFile(true);
            ptrs[i] = MWWorld::Ptr(cell.insert(&live), &cell);
            ESM4::RuntimeActorValues values;
            values.mActor = reference.mFormKey;
            values.mBase = base;
            values.mValues[0] = {40, {2, 3, -1}};
            values.mValues[14] = {5, {1, 2, -1}};
            values.mValues[8].mBase = 100;
            values.mValues[9].mBase = 20;
            values.mValues[10].mBase = 40;
            service.publishNonPlayerValues(ptrs[i], values);
        }
        model.registerPtr(ptrs[0]);
        const auto key = ptrs[0].getCellRef().getFormKey();
        const auto original = *service.findActorValues(key);
        ASSERT_EQ(service.findActorLife(key), nullptr);
        EXPECT_THROW(world.requestOblivionStatBase(ptrs[0], 72, 1.f), std::invalid_argument);
        EXPECT_EQ(service.findActorLife(key), nullptr);
        EXPECT_EQ(*service.findActorValues(key), original);
        EXPECT_THROW(world.executeOblivionActorValueCommand(ptrs[0], 0, ESM4::ActorValueCommand::Set,
            static_cast<ESM4::ActorValueCommandSource>(255), 1), std::invalid_argument);
        EXPECT_EQ(service.findActorLife(key), nullptr);

        // Valid finite input can fail later, while preparing a shared-base
        // projection. Initial lifecycle adoption must also roll back here.
        auto large = original;
        large.mValues[9].mModifiers[1] = 1e32f;
        service.publishNonPlayerValues(ptrs[0], large);
        const auto largeView = ptrs[0].getClass().getCreatureStats(ptrs[0]).getMagicka().getCurrent();
        ASSERT_THROW(world.requestOblivionStatBase(ptrs[0], 40, 1e9f), std::invalid_argument);
        EXPECT_EQ(service.findActorLife(key), nullptr);
        EXPECT_EQ(*service.findActorValues(key), large);
        EXPECT_EQ(service.getNonPlayerBaseValue(key, 40, store), 0);
        EXPECT_EQ(ptrs[0].getClass().getCreatureStats(ptrs[0]).getMagicka().getCurrent(), largeView);
        service.publishNonPlayerValues(ptrs[0], original);

        // A failed adoption restores all lifecycle projection flags, including
        // flags cleared by a transient Dead publication, without allocation.
        auto& stats = ptrs[0].getClass().getCreatureStats(ptrs[0]);
        stats.setDeathAnimationFinished(true);
        stats.setKnockedDown(true);
        stats.setKnockedDownOneFrame(true);
        stats.setKnockedDownOverOneFrame(true);
        {
            auto adoption = service.guardLifeAdoption(ptrs[0]);
            ESM4::RuntimeActorLife life;
            life.mActor = key;
            life.mBase = base;
            life.mPhase = ESM4::ActorLifePhase::Dead;
            service.publishNonPlayerLife(ptrs[0], life);
            ASSERT_TRUE(stats.isDead());
            EXPECT_FALSE(stats.getKnockedDown());
        }
        EXPECT_EQ(service.findActorLife(key), nullptr);
        EXPECT_FALSE(stats.isDead());
        EXPECT_TRUE(stats.isDeathAnimationFinished());
        EXPECT_TRUE(stats.getKnockedDown());
        EXPECT_TRUE(stats.getKnockedDownOneFrame());
        EXPECT_TRUE(stats.getKnockedDownOverOneFrame());
        stats.setKnockedDown(false);
        stats.setKnockedDownOneFrame(false);
        stats.setKnockedDownOverOneFrame(false);
        cell.forEachConst([](const MWWorld::ConstPtr&) { return true; });
        ASSERT_TRUE(world.executeOblivionActorValueCommand(ptrs[1].getCellRef().getFormKey(), 0,
            ESM4::ActorValueCommand::Set, ESM4::ActorValueCommandSource::Script, 257));
        ASSERT_NE(service.findActorLife(ptrs[1].getCellRef().getFormKey()), nullptr);
        for (const auto& ptr : ptrs)
            EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getAttribute(ESM::Attribute::Strength).getBase(), 1);

        LuaUtil::ScriptsConfiguration config;
        LuaUtil::LuaState luaState(&fixture.mVfs, &config);
        InspectableScripts scripts(&luaState, MWLua::LObject(ptrs[0]));
        MWLua::Context context{MWLua::Context::Local};
        context.mLuaManager = fixture.mLuaManager.get();
        context.mLua = &luaState;
        sol::state_view lua = luaState.unsafeState();
        sol::table actor(lua, sol::create), npcType(lua, sol::create);
        MWLua::addActorStatsBindings(actor, context);
        npcType["baseType"] = actor;
        MWLua::addNpcStatsBindings(npcType, context);
        lua["Actor"] = actor;
        lua["NPC"] = npcType;
        lua.new_usertype<MWLua::SelfObject>("SelfObject", sol::no_constructor);
        lua["target"] = &scripts.self();
        auto result = lua.safe_script("Actor.stats.attributes.strength(target).base = -1.75; "
            "NPC.stats.skills.longblade(target).base = 257.75", sol::script_pass_on_error);
        ASSERT_TRUE(result.valid()) << sol::error(result).what();
        EXPECT_EQ(service.findActorValues(ptrs[0].getCellRef().getFormKey())->mValues[0].mBase, 1);
        ASSERT_NO_THROW(scripts.applyStatsCache());
        ASSERT_NE(service.findActorLife(key), nullptr);
        for (const auto& ptr : ptrs)
        {
            EXPECT_EQ(service.findActorValues(ptr.getCellRef().getFormKey())->mValues[0].mBase, 255);
            EXPECT_EQ(service.findActorValues(ptr.getCellRef().getFormKey())->mValues[14].mBase, 1);
            EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getAttribute(ESM::Attribute::Strength).getModified(), 259);
            EXPECT_EQ(ptr.getClass().getNpcStats(ptr).getSkill(ESM::Skill::LongBlade).getModified(), 3);
        }
        EXPECT_FALSE(ptrs[1].getRefData().isEnabled());
        EXPECT_TRUE(ptrs[2].mRef->isDeleted());
        EXPECT_TRUE(model.getPtr(ptrs[1].getCellRef().getRefNum()).isEmpty());
        EXPECT_TRUE(model.getPtr(ptrs[2].getCellRef().getRefNum()).isEmpty());
    }
}
