#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <sstream>

#include <components/esm/records.hpp>
#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm4/loadglob.hpp>
#include <components/files/hash.hpp>
#include <components/files/collections.hpp>
#include <components/loadinglistener/loadinglistener.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/testing/util.hpp>

#include "apps/openmw/mwbase/environment.hpp"
#include "apps/openmw/mwclass/esm4npc.hpp"
#include "apps/openmw/mwclass/npc.hpp"
#include "apps/openmw/mwclass/esm4interactive.hpp"
#include "apps/openmw/mwmechanics/oblivioncombat.hpp"
#include "apps/openmw/mwlua/context.hpp"
#include "apps/openmw/mwlua/localscripts.hpp"
#include "apps/openmw/mwlua/luamanagerimp.hpp"
#include "apps/openmw/mwlua/stats.hpp"
#include "apps/openmw/mwsound/soundmanagerimp.hpp"
#include "apps/openmw/mwworld/worldimp.hpp"
#include "apps/openmw/mwworld/oblivionactorstats.hpp"
#include "apps/openmw/mwworld/timestamp.hpp"

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
        reference.mBaseObj = ESM::FormId{0x800, 0};
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
        for (const auto av : {9, 10})
            EXPECT_EQ(service.findActorValues(key)->mValues[av].mModifiers,
                (ESM4::ActorValueModifiers{std::nullopt, 0.f, 0.f}));
        const auto freshLow = captureNativeActorState(fixture, ptr);
        const auto decodedLow = ESM4::RuntimeState::deserializeBinary(freshLow.serializeBinary());
        EXPECT_EQ(decodedLow.mNativeActorValues, freshLow.mNativeActorValues);
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

    TEST(OblivionWorldTest, nativeAiClassAndLuaReadsFollowAuthorityWithoutLegacyClamps)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto ptr = addNativeNpc(fixture, 0x900);
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(ptr, ESM4::ActorValueProcess::Active));
        auto& service = *world.getOblivionCombatService();
        const auto& stats = ptr.getClass().getCreatureStats(ptr);
        EXPECT_EQ(stats.getAiSetting(MWMechanics::AiSetting::Hello).getModified(), 39);
        EXPECT_EQ(stats.getAiSetting(MWMechanics::AiSetting::Fight).getModified(), 13);
        EXPECT_EQ(stats.getAiSetting(MWMechanics::AiSetting::Flee).getModified(), 27);
        EXPECT_EQ(stats.getAiSetting(MWMechanics::AiSetting::Alarm).getModified(), 101);
        ASSERT_TRUE(world.executeOblivionActorValueCommand(ptr, 33, ESM4::ActorValueCommand::Mod,
            ESM4::ActorValueCommandSource::Script, -20));
        ASSERT_TRUE(world.executeOblivionActorValueCommand(ptr, 34, ESM4::ActorValueCommand::Mod,
            ESM4::ActorValueCommandSource::Console, -50));
        ASSERT_TRUE(world.executeOblivionActorValueCommand(ptr, 35, ESM4::ActorValueCommand::Set,
            ESM4::ActorValueCommandSource::Script, 257));
        ASSERT_TRUE(world.executeOblivionActorValueCommand(ptr, 36, ESM4::ActorValueCommand::Mod,
            ESM4::ActorValueCommandSource::Script, -120));
        EXPECT_EQ(service.getNonPlayerIntegerValue(ptr, 33), -7);
        EXPECT_EQ(service.getNonPlayerIntegerValue(ptr, 34), -23);
        EXPECT_EQ(service.getNonPlayerIntegerValue(ptr, 35), 1);
        EXPECT_EQ(service.getNonPlayerIntegerValue(ptr, 36), -19);
        EXPECT_EQ(stats.getAiSetting(MWMechanics::AiSetting::Fight).getModified(), -7);
        EXPECT_EQ(stats.getAiSetting(MWMechanics::AiSetting::Flee).getModified(), -23);
        EXPECT_EQ(stats.getAiSetting(MWMechanics::AiSetting::Alarm).getModified(), -19);
        EXPECT_EQ(stats.getAiSetting(MWMechanics::AiSetting::Hello).getModified(), 1);
        LuaUtil::ScriptsConfiguration config;
        LuaUtil::LuaState luaState(&fixture.mVfs, &config);
        InspectableScripts scripts(&luaState, MWLua::LObject(ptr));
        MWLua::Context context{MWLua::Context::Local};
        context.mLuaManager = fixture.mLuaManager.get();
        context.mLua = &luaState;
        sol::state_view lua = luaState.unsafeState();
        sol::table actor(lua, sol::create);
        MWLua::addActorStatsBindings(actor, context);
        lua["Actor"] = actor;
        lua.new_usertype<MWLua::SelfObject>("SelfObject", sol::no_constructor);
        lua["target"] = &scripts.self();
        auto result = lua.safe_script("assert(Actor.stats.ai.fight(target).modified == -7); "
            "assert(Actor.stats.ai.flee(target).modified == -23); "
            "assert(Actor.stats.ai.alarm(target).modified == -19); "
            "assert(Actor.stats.ai.hello(target).modified == 1)", sol::script_pass_on_error);
        EXPECT_TRUE(result.valid()) << sol::error(result).what();
        auto projected = stats.getAiSetting(MWMechanics::AiSetting::Fight);
        EXPECT_THROW(projected.setBase(50), std::logic_error);
        EXPECT_THROW(projected.setModifier(50), std::logic_error);
        EXPECT_THROW(ptr.getClass().getCreatureStats(ptr).setAiSetting(MWMechanics::AiSetting::Fight,
            MWMechanics::Stat<int>(50, 0)), std::logic_error);
        const auto sibling = addNativeNpc(fixture, 0x901);
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(sibling, ESM4::ActorValueProcess::Low));
        result = lua.safe_script("Actor.stats.ai.fight(target).base = 257.75; "
            "Actor.stats.ai.fight(target).modifier = 0.5; "
            "assert(Actor.stats.ai.fight(target).base == 1); "
            "assert(Actor.stats.ai.fight(target).modifier == 0.5); "
            "assert(Actor.stats.ai.fight(target).modified == -18)", sol::script_pass_on_error);
        ASSERT_TRUE(result.valid()) << sol::error(result).what();
        EXPECT_EQ(service.getNonPlayerIntegerValue(ptr, 33), -7);
        ASSERT_NO_THROW(scripts.applyStatsCache());
        const auto& values = *service.findActorValues(ptr.getCellRef().getFormKey());
        EXPECT_EQ(values.mValues[33].mBase, 1);
        EXPECT_EQ(values.mValues[33].mModifiers[0], 0.5f);
        EXPECT_EQ(values.mValues[33].mModifiers[1], -20.f);
        EXPECT_FALSE(values.mValues[33].mModifiers[2]);
        EXPECT_EQ(service.getNonPlayerIntegerValue(ptr, 33), -18);
        EXPECT_EQ(stats.getAiSetting(MWMechanics::AiSetting::Fight).getModified(), -18);
        EXPECT_EQ(sibling.getClass().getCreatureStats(sibling).getAiSetting(MWMechanics::AiSetting::Fight)
            .getModified(), 1);
        result = lua.safe_script("assert(Actor.stats.ai.fight(target).modified == -18); "
            "assert(Actor.stats.ai.fight(target).modifier == 0.5)", sol::script_pass_on_error);
        EXPECT_TRUE(result.valid()) << sol::error(result).what();
        EXPECT_THROW(world.requestOblivionStatModifier(ptr, 33, false,
            std::numeric_limits<float>::infinity()), std::invalid_argument);
        EXPECT_EQ(service.getNonPlayerIntegerValue(ptr, 33), -18);
        MWMechanics::Stat<int> legacy(-10, 1);
        EXPECT_FALSE(legacy.isNativeProjection());
        EXPECT_EQ(legacy.getModified(), 0);
        EXPECT_EQ(legacy.getModified(false), -9);
        legacy.setModifier(3);
        EXPECT_EQ(legacy.getModified(false), -7);
        MWMechanics::Stat<int> largeLegacy(std::numeric_limits<int>::max(), 0);
        EXPECT_EQ(largeLegacy.getModified(), std::numeric_limits<int>::max());
        EXPECT_EQ(largeLegacy.getModifiedWithOverrides({}, {}), std::numeric_limits<int>::max());
        EXPECT_THROW(largeLegacy.getModifiedWithOverrides(std::numeric_limits<float>::infinity(), {}),
            std::invalid_argument);
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
        service.installRestoredActorState(std::move(replacement), residents);
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
        EXPECT_THROW(service.installRestoredActorState(std::move(replacement), invalidResidents),
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
        EXPECT_THROW(service.installRestoredActorState(std::move(replacement), ambiguousResidents),
            std::invalid_argument);
        EXPECT_EQ(captureNativeActorState(fixture, first).mNativeActorValues, before.mNativeActorValues);
        EXPECT_EQ(first.getClass().getCreatureStats(first).getHealth().getCurrent(), 93);
        EXPECT_TRUE(first.getClass().getCreatureStats(first).isDead());
        EXPECT_EQ(second.getClass().getCreatureStats(second).getHealth().getCurrent(), 89);
        // The rejected replacement remains intact and can be installed once
        // the roster is corrected. Duplicate valid pointers are harmless.
        const std::array residents{first, second, first};
        service.installRestoredActorState(std::move(replacement), residents);
        EXPECT_EQ(first.getClass().getCreatureStats(first).getHealth().getCurrent(), 100);
        EXPECT_FALSE(first.getClass().getCreatureStats(first).isDead());
        EXPECT_EQ(second.getClass().getCreatureStats(second).getHealth().getCurrent(), 100);
        EXPECT_EQ(captureNativeActorState(fixture, first).mNativeActorValues, saved.mNativeActorValues);
        EXPECT_EQ(captureNativeActorState(fixture, first).mNativeActorLife, saved.mNativeActorLife);
        EXPECT_THROW(service.installRestoredActorState(std::move(service), residents), std::invalid_argument);
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
        const auto& freshLowValues = service.findActorValues(reference.mFormKey)->mValues;
        for (std::size_t av = 0; av < freshLowValues.size(); ++av)
        {
            EXPECT_EQ(freshLowValues[av].mBase, original.mValues[av].mBase);
            EXPECT_EQ(freshLowValues[av].mModifiers[0], std::nullopt);
            EXPECT_EQ(freshLowValues[av].mModifiers[1], original.mValues[av].mModifiers[1]);
            EXPECT_EQ(freshLowValues[av].mModifiers[2], original.mValues[av].mModifiers[2]);
        }
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
        auto badAi = original;
        badAi.mNativeActorValues[0].mValues[33].mModifiers[1] = 1e32f;
        ASSERT_NO_THROW(badAi.validate()); // Finite float composition, unsupported integer view.
        EXPECT_THROW(service.restore(badAi, world.getStore()), std::invalid_argument);
        EXPECT_EQ(captureNativeActorState(fixture, ptr).mNativeActorValues, original.mNativeActorValues);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getAiSetting(MWMechanics::AiSetting::Fight)
            .getModified(), 13);
        MWWorld::World legacy(nullptr, -1, "", {}, ESM::GameProfile::Morrowind);
        EXPECT_FALSE(legacy.initializeOblivionNonPlayerActor(ptr, ESM4::ActorValueProcess::Active));
    }

    TEST(OblivionWorldTest, explicitWorldPlayerConstructionSharesWritersAndClearsWithoutEvents)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& service = *world.getOblivionCombatService();
        const auto actor = ESM::FormKey::dynamic("player", 1);
        EXPECT_FALSE(world.initializeOblivionPlayerActor()); // Player is not set up yet.
        MWClass::Npc::registerSelf();
        world.setupPlayer();
        EXPECT_THROW(world.initializeOblivionPlayerActor(), std::invalid_argument);
        EXPECT_EQ(service.findActorValues(actor), nullptr);
        EXPECT_EQ(service.findActorLife(actor), nullptr);
        // The synthetic profile's facade was authored from headless.esm:800.
        // Inject canonical Player construction inputs independently of it.
        const auto base = ESM::FormKey::content("Oblivion.esm", 7);
        ESM4::Npc native{};
        native.mId = {7, 1};
        native.mFormKey = base;
        native.mIsTES4 = true;
        native.mData.attribs = {50, 50, 30, 30, 40, 40, 50, 50};
        native.mData.health = 45;
        native.mBaseConfig.tes4.fatigue = 150;
        world.getStore().getWritable<ESM4::Npc>().insertStatic(native, base);
        ASSERT_TRUE(world.initializeOblivionPlayerActor());
        auto player = world.getPlayerPtr();
        EXPECT_EQ(player.getClass().getCreatureStats(player).getHealth().getCurrent(), 80);
        ASSERT_NE(service.findActorLife(actor), nullptr);
        EXPECT_EQ(service.findActorLife(actor)->mPhase, ESM4::ActorLifePhase::Alive);
        EXPECT_EQ(world.getOblivionScriptActorValue(actor, 8, false), 80);
        ASSERT_TRUE(world.requestOblivionResourceCurrent(player, 8, 60));
        EXPECT_EQ(player.getClass().getCreatureStats(player).getHealth().getCurrent(), 60);
        EXPECT_EQ(world.getOblivionScriptActorValue(actor, 8, false), 60);
        const auto values = *service.findActorValues(actor);
        native.mIsTES4 = false;
        world.getStore().getWritable<ESM4::Npc>().insertStatic(native, base);
        ASSERT_TRUE(world.initializeOblivionPlayerActor());
        EXPECT_EQ(*service.findActorValues(actor), values);
        EXPECT_FALSE(service.takeNextDeathEvent());
        native.mIsTES4 = true;
        world.getStore().getWritable<ESM4::Npc>().insertStatic(native, base);
        ASSERT_NO_THROW(world.clear());
        EXPECT_EQ(service.findActorValues(actor), nullptr);
        EXPECT_EQ(service.findActorLife(actor), nullptr);
        world.setupPlayer();
        ASSERT_TRUE(world.initializeOblivionPlayerActor());
        player = world.getPlayerPtr();
        EXPECT_EQ(player.getClass().getCreatureStats(player).getHealth().getCurrent(), 80);
        EXPECT_FALSE(service.takeNextDeathEvent());
        MWWorld::World legacy(nullptr, -1, "", {}, ESM::GameProfile::Morrowind);
        EXPECT_FALSE(legacy.initializeOblivionPlayerActor());
    }

    TEST(OblivionWorldTest, actualReaderPlayerConstructorConsumesLegacyMarkersOnlyAfterCommit)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        MWClass::Npc::registerSelf();
        world.setupPlayer();
        const auto actor = ESM::FormKey::dynamic("player", 1);
        const auto base = ESM::FormKey::content("Oblivion.esm", 7);
        ESM4::Npc native{};
        native.mId = {7, 1};
        native.mFormKey = base;
        native.mIsTES4 = true;
        native.mData.health = 100;
        world.getStore().getWritable<ESM4::Npc>().insertStatic(native, base);
        auto& service = *world.getOblivionCombatService();
        ASSERT_TRUE(world.initializeOblivionPlayerActor());
        ESM4::RuntimeState state;
        state.mPlayer.mReference = actor;
        state.mPlayer.mCell = ESM::FormKey::content("headless.esm", 1);
        state.mPlayer.mRace = ESM::FormKey::content("headless.esm", 0x810);
        state.mPlayer.mClass = ESM::FormKey::dynamic("fixture-class", 1);
        ESM4::RuntimeReferenceState reference;
        reference.mKey = actor;
        reference.mBase = ESM::FormKey::dynamic("player-base", 1);
        reference.mCell = state.mPlayer.mCell;
        reference.mCustomState["obscript.dead"] = true;
        state.mReferences.push_back(reference);
        std::ifstream content(fixture.mDirectory / "headless.esm", std::ios::binary);
        state.mContent.push_back({"headless.esm", "sha256:" + Files::getSha256("headless.esm", content)});
        // The on-disk legacy snapshot has no typed native values/lifecycle.
        // Live authority may already exist when its marker is later adopted.
        const auto before = *service.findActorValues(actor);
        readNativeSnapshot(fixture, state);
        EXPECT_THROW(world.initializeOblivionPlayerActor(), std::invalid_argument);
        EXPECT_EQ(*service.findActorValues(actor), before);
        EXPECT_EQ(service.findActorLife(actor)->mPhase, ESM4::ActorLifePhase::Alive);
        service.clear();
        ASSERT_TRUE(world.initializeOblivionPlayerActor());
        EXPECT_EQ(service.findActorLife(actor)->mPhase, ESM4::ActorLifePhase::Dead);
        EXPECT_TRUE(world.getPlayerPtr().getClass().getCreatureStats(world.getPlayerPtr()).isDead());
        EXPECT_FALSE(service.takeNextDeathEvent());
        EXPECT_EQ(service.getDeadCount(reference.mBase), 0);
        auto alive = *service.findActorLife(actor);
        alive.mPhase = ESM4::ActorLifePhase::Alive;
        service.publishPlayerLife(world.getPlayer(), alive);
        ASSERT_TRUE(world.initializeOblivionPlayerActor()); // Consumed true marker cannot conflict.
        state.mReferences[0].mCustomState["obscript.dead"] = std::int64_t{42};
        readNativeSnapshot(fixture, state);
        service.clear();
        EXPECT_THROW(world.initializeOblivionPlayerActor(), std::invalid_argument);
        EXPECT_EQ(service.findActorValues(actor), nullptr);
        EXPECT_EQ(service.findActorLife(actor), nullptr);
        EXPECT_FALSE(service.takeNextDeathEvent());
    }

    TEST(OblivionWorldTest, restoredAuthorityRefreshesActualWorldPlayerView)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        MWClass::Npc::registerSelf();
        world.setupPlayer();
        const auto player = world.getPlayerPtr();
        const auto npc = addNativeNpc(fixture, 0x900);
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(npc, ESM4::ActorValueProcess::Active));
        auto& service = *world.getOblivionCombatService();
        const auto settings = MWWorld::resolveOblivionPlayerDynamicBaseSettings(world.getStore());
        ESM4::RuntimeActorValues values;
        values.mActor = ESM::FormKey::dynamic("player", 1);
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mPlayerFormValues = {{10, 0, 0, 0}};
        for (std::size_t i = 0; i < 8; ++i)
            values.mValues[i].mBase = 50;
        service.publishPlayerValues(world.getPlayer(), values, settings);
        ESM4::RuntimeActorLife dead;
        dead.mActor = values.mActor;
        dead.mBase = values.mBase;
        dead.mPhase = ESM4::ActorLifePhase::Dead;
        service.publishPlayerLife(world.getPlayer(), dead);
        const auto saved = captureNativeActorState(fixture, npc);
        EXPECT_EQ(player.getClass().getCreatureStats(player).getHealth().getCurrent(), 110);
        auto alive = dead;
        alive.mPhase = ESM4::ActorLifePhase::Alive;
        service.publishPlayerLife(world.getPlayer(), alive);
        ASSERT_TRUE(world.executeOblivionActorValueCommand(player, 0, ESM4::ActorValueCommand::Mod,
            ESM4::ActorValueCommandSource::Script, -7));
        service.changePlayerValue(world.getPlayer(), 8, ESM4::ActorValueModifier::Damage, -7, settings);
        ASSERT_EQ(player.getClass().getCreatureStats(player).getAttribute(ESM::Attribute::Strength)
            .getModified(), 43);
        ASSERT_FALSE(player.getClass().getCreatureStats(player).isDead());
        ASSERT_TRUE(world.executeOblivionActorValueCommand(npc, 8, ESM4::ActorValueCommand::Mod,
            ESM4::ActorValueCommandSource::Console, -11));
        const auto before = captureNativeActorState(fixture, npc);
        MWMechanics::OblivionCombatService replacement;
        replacement.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()), world.getStore());
        const auto residents = world.getWorldModel().getResidentPtrs();
        EXPECT_THROW(service.installRestoredActorState(std::move(replacement), residents), std::invalid_argument);
        EXPECT_EQ(captureNativeActorState(fixture, npc).serializeBinary(), before.serializeBinary());
        auto wrongBase = *npc.get<ESM4::Npc>()->mBase;
        wrongBase.mFormKey = ESM::FormKey::content("headless.esm", 0x899);
        ESM4::ActorCharacter reference{};
        reference.mId = {0x900, 0};
        reference.mFormKey = npc.getCellRef().getFormKey();
        reference.mBaseKey = wrongBase.mFormKey;
        MWWorld::LiveCellRef<ESM4::Npc> wrong(reference, &wrongBase);
        const std::array invalidResidents{MWWorld::Ptr(&wrong, npc.getCell())};
        // Player preparation precedes the invalid resident. Neither its view
        // nor authority may change when the later preparation fails.
        EXPECT_THROW(service.installRestoredActorState(std::move(replacement), invalidResidents,
            &world.getPlayer()), std::invalid_argument);
        EXPECT_EQ(captureNativeActorState(fixture, npc).serializeBinary(), before.serializeBinary());
        EXPECT_EQ(player.getClass().getCreatureStats(player).getAttribute(ESM::Attribute::Strength)
            .getModified(), 43);
        EXPECT_EQ(player.getClass().getCreatureStats(player).getHealth().getCurrent(), 103);
        EXPECT_FALSE(player.getClass().getCreatureStats(player).isDead());
        EXPECT_EQ(npc.getClass().getCreatureStats(npc).getHealth().getCurrent(), 89);
        auto retained = saved;
        replacement.capture(retained);
        EXPECT_EQ(retained.serializeBinary(), saved.serializeBinary());
        service.installRestoredActorState(std::move(replacement), residents, &world.getPlayer());
        EXPECT_EQ(npc.getClass().getCreatureStats(npc).getHealth().getCurrent(), 100);
        EXPECT_EQ(world.getOblivionScriptActorValue(values.mActor, 0, false), 50);
        const auto& stats = player.getClass().getCreatureStats(player);
        EXPECT_EQ(stats.getAttribute(ESM::Attribute::Strength).getModified(), 50);
        EXPECT_EQ(stats.getHealth().getCurrent(), 110);
        EXPECT_TRUE(stats.isDead());
        EXPECT_EQ(captureNativeActorState(fixture, npc).mNativeActorValues, saved.mNativeActorValues);
        EXPECT_EQ(captureNativeActorState(fixture, npc).mNativeActorLife, saved.mNativeActorLife);
        EXPECT_FALSE(service.takeNextDeathEvent());
    }

    TEST(OblivionWorldTest, actualWorldApplyUsesNativePlayerAuthorityOverLegacyTelemetry)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        MWClass::Npc::registerSelf();
        world.setupPlayer();
        // Register synthetic data through the normal stores, without scene
        // attachment. This is World apply, not graphical restart acceptance.
        const auto npc = addNativeNpc(fixture, 0x900);
        auto& store = world.getStore();
        ESM::Race race{};
        race.blank();
        race.mId = ESM::RefId(ESM::FormId{0x810, 0});
        store.getWritable<ESM::Race>().insertStatic(race);
        ESM4::Cell cell{};
        cell.mId = ESM::RefId(ESM::FormId{1, 0});
        cell.mFormKey = ESM::FormKey::content("headless.esm", 1);
        cell.mCellFlags = ESM4::CELL_Interior;
        cell.mEditorId = "NativeRestoreCell";
        store.getWritable<ESM4::Cell>().insertStatic(cell, cell.mFormKey);
        auto& service = *world.getOblivionCombatService();
        ESM4::RuntimeActorValues values;
        values.mActor = ESM::FormKey::dynamic("player", 1);
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mPlayerFormValues = {{10, 0, 0, 0}};
        for (std::size_t i = 0; i < 8; ++i)
            values.mValues[i].mBase = 50;
        const auto settings = MWWorld::resolveOblivionPlayerDynamicBaseSettings(store);
        service.publishPlayerValues(world.getPlayer(), values, settings);
        ESM4::RuntimeActorLife life;
        life.mActor = values.mActor;
        life.mBase = values.mBase;
        life.mPhase = ESM4::ActorLifePhase::Dead;
        service.publishPlayerLife(world.getPlayer(), life);
        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = values.mActor;
        saved.mPlayer.mCell = cell.mFormKey;
        saved.mPlayer.mRace = ESM::FormKey::content("headless.esm", 0x810);
        saved.mPlayer.mClass = ESM::FormKey::dynamic("fixture-class", 1);
        // Deliberately inconsistent legacy telemetry must not overwrite the
        // native channels or invoke guarded legacy stat mutation.
        saved.mPlayer.mActorValues["health.current"] = 999;
        saved.mPlayer.mActorValues["strength.base"] = 99;
        saved.mPlayer.mActorValues["blade.base"] = 99;
        service.capture(saved);
        std::ifstream content(fixture.mDirectory / "headless.esm", std::ios::binary);
        saved.mContent.push_back({"headless.esm", "sha256:" + Files::getSha256("headless.esm", content)});
        ASSERT_TRUE(world.executeOblivionActorValueCommand(world.getPlayerPtr(), 0,
            ESM4::ActorValueCommand::Mod, ESM4::ActorValueCommandSource::Script, -7));
        const auto clockBefore = world.getTimeStamp();
        auto bad = saved;
        bad.mClock.mHour = 7;
        bad.mNativeActorValues[0].mValues[33].mModifiers[1] = 1e32f;
        ASSERT_NO_THROW(bad.validate());
        readNativeSnapshot(fixture, bad);
        EXPECT_THROW(world.applyOblivionRuntimeState(), std::invalid_argument);
        // Reject unsupported Player integer views during detached service
        // preparation, before changing the world clock or live authority.
        EXPECT_EQ(world.getTimeStamp(), clockBefore);
        EXPECT_EQ(world.getPlayerPtr().getClass().getCreatureStats(world.getPlayerPtr())
            .getAttribute(ESM::Attribute::Strength).getModified(), 43);
        auto beforeRetry = saved;
        service.capture(beforeRetry);
        EXPECT_EQ(beforeRetry.mNativeActorValues[0].mValues[0].mModifiers[1], -7);
        for (const bool missingFormInputs : {false, true})
        {
            SCOPED_TRACE(missingFormInputs);
            auto invalidIdentity = saved;
            invalidIdentity.mClock.mHour = missingFormInputs ? 9 : 8;
            if (missingFormInputs)
                invalidIdentity.mNativeActorValues[0].mPlayerFormValues.reset();
            else
            {
                invalidIdentity.mNativeActorValues[0].mBase = ESM::FormKey::content("headless.esm", 0x800);
                invalidIdentity.mNativeActorLife[0].mBase = invalidIdentity.mNativeActorValues[0].mBase;
            }
            ASSERT_NO_THROW(invalidIdentity.validate());
            const auto beforeIdentityClock = world.getTimeStamp();
            auto beforeIdentity = saved;
            service.capture(beforeIdentity);
            readNativeSnapshot(fixture, invalidIdentity);
            EXPECT_THROW(world.applyOblivionRuntimeState(), std::invalid_argument);
            EXPECT_EQ(world.getTimeStamp(), beforeIdentityClock);
            auto afterIdentity = saved;
            service.capture(afterIdentity);
            EXPECT_EQ(afterIdentity.serializeBinary(), beforeIdentity.serializeBinary());
            EXPECT_EQ(world.getPlayerPtr().getClass().getCreatureStats(world.getPlayerPtr())
                .getAttribute(ESM::Attribute::Strength).getModified(), 43);
        }
        readNativeSnapshot(fixture, saved);
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        const auto player = world.getPlayerPtr();
        const auto& stats = player.getClass().getNpcStats(player);
        EXPECT_EQ(stats.getHealth().getCurrent(), 110);
        EXPECT_EQ(stats.getAttribute(ESM::Attribute::Strength).getModified(), 50);
        EXPECT_EQ(stats.getSkill(ESM::Skill::LongBlade).getBase(), 0);
        EXPECT_TRUE(stats.isDead());
        auto after = saved;
        service.capture(after);
        EXPECT_EQ(after.serializeBinary(), saved.serializeBinary());
        EXPECT_FALSE(service.takeNextDeathEvent());
        const auto resident = npc.getCell()->moveTo(npc, player.getCell());
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(resident, ESM4::ActorValueProcess::Active));
        const auto captured = world.captureOblivionRuntimeState();
        ASSERT_EQ(captured.mReferences.size(), 1);
        EXPECT_EQ(captured.mReferences[0].mKey, resident.getCellRef().getFormKey());
        EXPECT_EQ(captured.mPlayer.mActorValues.at("health.current"), 110);
        EXPECT_EQ(captured.mPlayer.mActorValues.at("strength.base"), 50);
        EXPECT_EQ(captured.mNativeActorValues.size(), 2);
        service.changePlayerValue(world.getPlayer(), 8, ESM4::ActorValueModifier::Damage, -3, settings);
        ASSERT_TRUE(world.executeOblivionActorValueCommand(resident, 8, ESM4::ActorValueCommand::Mod,
            ESM4::ActorValueCommandSource::Console, -7));
        readNativeSnapshot(fixture, captured);
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), captured.serializeBinary());
        EXPECT_EQ(player.getClass().getCreatureStats(player).getHealth().getCurrent(), 110);
        EXPECT_EQ(resident.getClass().getCreatureStats(resident).getHealth().getCurrent(), 100);
        EXPECT_FALSE(service.takeNextDeathEvent());
        auto secondCell = cell;
        secondCell.mId = ESM::RefId(ESM::FormId{2, 0});
        secondCell.mFormKey = ESM::FormKey::content("headless.esm", 2);
        secondCell.mEditorId = "DuplicateActorRestoreCell";
        store.getWritable<ESM4::Cell>().insertStatic(secondCell, secondCell.mFormKey);
        const auto* placed = store.search<ESM4::ActorCharacter>(resident.getCellRef().getFormKey());
        ASSERT_NE(placed, nullptr);
        MWWorld::LiveCellRef<ESM4::Npc> duplicate(*placed, resident.get<ESM4::Npc>()->mBase);
        auto& duplicateCell = world.getWorldModel().getCell(secondCell.mId);
        const MWWorld::Ptr duplicatePtr(duplicateCell.insert(&duplicate), &duplicateCell);
        ASSERT_NE(duplicatePtr, resident);
        const auto beforeDuplicateClock = world.getTimeStamp();
        const auto beforeDuplicatePosition = resident.getRefData().getPosition();
        auto conflictingResidents = captured;
        conflictingResidents.mClock.mHour = 9;
        conflictingResidents.mReferences[0].mPosition.pos[0] = 45;
        ASSERT_NO_THROW(conflictingResidents.validate());
        readNativeSnapshot(fixture, conflictingResidents);
        EXPECT_THROW(world.applyOblivionRuntimeState(), std::invalid_argument);
        EXPECT_EQ(world.getTimeStamp(), beforeDuplicateClock);
        EXPECT_EQ(resident.getRefData().getPosition(), beforeDuplicatePosition);
        auto retainedAuthority = captured;
        service.capture(retainedAuthority);
        EXPECT_EQ(retainedAuthority.mNativeActorValues, captured.mNativeActorValues);
        EXPECT_EQ(retainedAuthority.mNativeActorLife, captured.mNativeActorLife);
        EXPECT_FALSE(service.takeNextDeathEvent());
    }

    TEST(OblivionWorldTest, actualWorldApplyPreservesSupportedLegacyPlayerTelemetry)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        MWClass::Npc::registerSelf();
        world.setupPlayer();
        auto& store = world.getStore();
        ESM::Race race{};
        race.blank();
        race.mId = ESM::RefId(ESM::FormId{0x810, 0});
        store.getWritable<ESM::Race>().insertStatic(race);
        ESM4::Cell cell{};
        cell.mId = ESM::RefId(ESM::FormId{1, 0});
        cell.mFormKey = ESM::FormKey::content("headless.esm", 1);
        cell.mCellFlags = ESM4::CELL_Interior;
        cell.mEditorId = "LegacyRestoreCell";
        store.getWritable<ESM4::Cell>().insertStatic(cell, cell.mFormKey);
        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        saved.mPlayer.mCell = cell.mFormKey;
        saved.mPlayer.mRace = ESM::FormKey::content("headless.esm", 0x810);
        saved.mPlayer.mClass = ESM::FormKey::dynamic("fixture-class", 1);
        saved.mPlayer.mActorValues["health.current"] = 123;
        saved.mPlayer.mActorValues["strength.base"] = 11;
        saved.mPlayer.mActorValues["blade.base"] = 17;
        std::ifstream content(fixture.mDirectory / "headless.esm", std::ios::binary);
        saved.mContent.push_back({"headless.esm", "sha256:" + Files::getSha256("headless.esm", content)});
        for (std::uint32_t version = 1; version <= ESM4::CurrentRuntimeStateVersion; ++version)
        {
            SCOPED_TRACE(version);
            saved.mVersion = version;
            saved.mPlayer.mRace = version >= 3 ? ESM::FormKey::content("headless.esm", 0x810) : ESM::FormKey{};
            saved.mPlayer.mClass = version >= 3 ? ESM::FormKey::dynamic("fixture-class", 1) : ESM::FormKey{};
            readNativeSnapshot(fixture, saved);
            ASSERT_NO_THROW(world.applyOblivionRuntimeState());
            const auto player = world.getPlayerPtr();
            const auto& stats = player.getClass().getNpcStats(player);
            EXPECT_FALSE(stats.getHealth().isNativeProjection());
            EXPECT_EQ(stats.getHealth().getCurrent(), 123);
            EXPECT_EQ(stats.getAttribute(ESM::Attribute::Strength).getBase(), 11);
            EXPECT_EQ(stats.getSkill(ESM::Skill::LongBlade).getBase(), 17);
            EXPECT_EQ(world.getOblivionCombatService()->findActorValues(saved.mPlayer.mReference), nullptr);
            EXPECT_FALSE(world.getOblivionCombatService()->takeNextDeathEvent());
        }
    }

    TEST(OblivionWorldTest, nativeWorldApplyRequiresReadyPlayerBeforeMutation)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        EXPECT_TRUE(world.getPlayerPtr().isEmpty());
        ESM4::Cell cell{};
        cell.mId = ESM::RefId(ESM::FormId{1, 0});
        cell.mFormKey = ESM::FormKey::content("headless.esm", 1);
        cell.mCellFlags = ESM4::CELL_Interior;
        cell.mEditorId = "UnconstructedPlayerCell";
        world.getStore().getWritable<ESM4::Cell>().insertStatic(cell, cell.mFormKey);
        ESM4::RuntimeState saved;
        saved.mVersion = 2;
        saved.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        saved.mPlayer.mCell = cell.mFormKey;
        saved.mClock.mHour = 7;
        std::ifstream content(fixture.mDirectory / "headless.esm", std::ios::binary);
        saved.mContent.push_back({"headless.esm", "sha256:" + Files::getSha256("headless.esm", content)});
        const auto clockBefore = world.getTimeStamp();
        readNativeSnapshot(fixture, saved);
        EXPECT_THROW(world.applyOblivionRuntimeState(), std::runtime_error);
        EXPECT_TRUE(world.getPlayerPtr().isEmpty());
        EXPECT_EQ(world.getTimeStamp(), clockBefore);
        ESM4::RuntimeState captured;
        world.getOblivionCombatService()->capture(captured);
        EXPECT_TRUE(captured.mNativeActorValues.empty());
        EXPECT_TRUE(captured.mNativeActorLife.empty());
        EXPECT_FALSE(world.getOblivionCombatService()->takeNextDeathEvent());
    }

    TEST(OblivionWorldTest, nativeWorldDataClearResetsAuthorityAndSupportsAnotherCycle)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& service = *world.getOblivionCombatService();
        MWClass::Npc::registerSelf();
        for (std::uint32_t cycle = 0; cycle < 2; ++cycle)
        {
            SCOPED_TRACE(cycle);
            world.setupPlayer();
            const auto npc = addNativeNpc(fixture, 0x900 + cycle);
            ASSERT_TRUE(world.initializeOblivionNonPlayerActor(npc, ESM4::ActorValueProcess::Active));
            ESM4::RuntimeActorValues values;
            values.mActor = ESM::FormKey::dynamic("player", 1);
            values.mBase = ESM::FormKey::dynamic("player-base", 1);
            values.mOwner = ESM4::ActorValueOwner::Player;
            values.mPlayerFormValues = {{10, 0, 0, 0}};
            for (std::size_t i = 0; i < 8; ++i)
                values.mValues[i].mBase = 50;
            service.publishPlayerValues(world.getPlayer(), values,
                MWWorld::resolveOblivionPlayerDynamicBaseSettings(world.getStore()));
            ESM4::RuntimeActorLife life;
            life.mActor = values.mActor;
            life.mBase = values.mBase;
            service.publishPlayerLife(world.getPlayer(), life);
            ASSERT_TRUE(service.engage(values.mActor, npc.getCellRef().getFormKey()));
            const auto action = service.allocateAction();
            ASSERT_EQ(action, 1);
            ASSERT_TRUE(service.isActionPending(action));
            service.advanceFrameClock(7);
            ASSERT_EQ(service.actorManagerTime(), 7);
            ASSERT_NO_THROW(world.clear());
            EXPECT_EQ(world.getOblivionCombatService(), &service);
            EXPECT_TRUE(world.getWorldModel().getResidentPtrs().empty());
            EXPECT_FALSE(service.isActionPending(action));
            EXPECT_FALSE(service.isActionConsumed(action));
            EXPECT_FALSE(service.isInCombat(values.mActor));
            EXPECT_EQ(service.actorManagerTime(), 0);
            ESM4::RuntimeState cleared;
            service.capture(cleared);
            EXPECT_TRUE(cleared.mNativeActorValues.empty());
            EXPECT_TRUE(cleared.mNativeActorBases.empty());
            EXPECT_TRUE(cleared.mNativeActorLife.empty());
            EXPECT_TRUE(cleared.mNativeActorBreath.empty());
            EXPECT_TRUE(cleared.mNativeActorUpdateTimes.empty());
            EXPECT_TRUE(cleared.mNativeDeathCounts.empty());
            EXPECT_TRUE(cleared.mNativeCombatEngagements.empty());
            EXPECT_TRUE(cleared.mPendingDeathEvents.empty());
            EXPECT_EQ(cleared.mNextDeathEvent, 1);
            const auto player = world.getPlayerPtr();
            ASSERT_FALSE(player.isEmpty());
            EXPECT_FALSE(player.isInCell());
            EXPECT_FALSE(player.getClass().getCreatureStats(player).getHealth().isNativeProjection());
            EXPECT_EQ(player.getClass().getCreatureStats(player).getHealth().getCurrent(), 100);
        }
    }

    TEST(OblivionWorldTest, nativeWorldGlobalRestoreRejectsBeforeEarlierGlobalMutation)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
        const std::array keys{ESM::FormKey::content("headless.esm", 0xa10),
            ESM::FormKey::content("headless.esm", 0xa11), ESM::FormKey::content("headless.esm", 0xa12)};
        const std::array names{std::string("atomicfirst"), std::string("atomicsecond"), std::string("atomicinteger")};
        for (std::size_t i = 0; i < keys.size(); ++i)
        {
            ESM4::GlobalVariable native{};
            native.mId = {static_cast<std::uint32_t>(0xa10 + i), 0};
            native.mEditorId = names[i];
            native.mType = i == 2 ? 'l' : 'f';
            native.mValue = i == 0 ? 3 : i == 1 ? 5 : 7;
            store.getWritable<ESM4::GlobalVariable>().insertStatic(native, keys[i]);
            ESM::Global projected{};
            projected.mId = ESM::RefId::stringRefId(names[i]);
            projected.mValue = i == 2 ? ESM::Variant(std::int32_t{7}) : ESM::Variant(native.mValue);
            store.getWritable<ESM::Global>().insertStatic(projected);
        }
        ESM4::Cell cell{};
        cell.mId = ESM::RefId(ESM::FormId{1, 0});
        cell.mFormKey = ESM::FormKey::content("headless.esm", 1);
        cell.mCellFlags = ESM4::CELL_Interior;
        cell.mEditorId = "GlobalRestoreCell";
        store.getWritable<ESM4::Cell>().insertStatic(cell, cell.mFormKey);
        world.clear(); // Install the synthetic projected global definitions.
        MWClass::Npc::registerSelf();
        world.setupPlayer();
        ESM4::RuntimeState saved;
        saved.mVersion = 2;
        saved.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        saved.mPlayer.mCell = cell.mFormKey;
        saved.mClock.mHour = 7;
        saved.mGlobals[keys[0]] = 42.;
        std::ifstream content(fixture.mDirectory / "headless.esm", std::ios::binary);
        saved.mContent.push_back({"headless.esm", "sha256:" + Files::getSha256("headless.esm", content)});
        const MWWorld::GlobalVariableName firstName{names[0]};
        const MWWorld::GlobalVariableName secondName{names[1]};
        const std::array<ESM4::RuntimeValue, 2> invalidValues{std::string("wrong type"), 1e300};
        for (const auto& invalid : invalidValues)
        {
            world.setGlobalFloat(firstName, 3);
            world.setGlobalFloat(secondName, 5);
            const auto clockBefore = world.getTimeStamp();
            saved.mGlobals[keys[1]] = invalid;
            ASSERT_NO_THROW(saved.validate());
            readNativeSnapshot(fixture, saved);
            EXPECT_THROW(world.applyOblivionRuntimeState(), std::runtime_error);
            EXPECT_EQ(world.getGlobalFloat(firstName), 3);
            EXPECT_EQ(world.getGlobalFloat(secondName), 5);
            EXPECT_EQ(world.getTimeStamp(), clockBefore);
        }
        const MWWorld::GlobalVariableName integerName{names[2]};
        saved.mGlobals[keys[1]] = 10.;
        for (const double invalidInteger : {1e300, -1e300, std::ldexp(1., 63)})
        {
            SCOPED_TRACE(invalidInteger);
            world.setGlobalFloat(firstName, 3);
            world.setGlobalFloat(secondName, 5);
            world.setGlobalInt(integerName, 7);
            const auto clockBefore = world.getTimeStamp();
            saved.mGlobals[keys[2]] = invalidInteger;
            ASSERT_NO_THROW(saved.validate());
            readNativeSnapshot(fixture, saved);
            EXPECT_THROW(world.applyOblivionRuntimeState(), std::runtime_error);
            EXPECT_EQ(world.getGlobalFloat(firstName), 3);
            EXPECT_EQ(world.getGlobalFloat(secondName), 5);
            EXPECT_EQ(world.getGlobalInt(integerName), 7);
            EXPECT_EQ(world.getTimeStamp(), clockBefore);
        }
        const std::array<std::pair<ESM4::RuntimeValue, std::int32_t>, 5> supportedIntegers{{
            {std::int64_t{std::numeric_limits<std::int64_t>::max()}, std::numeric_limits<std::int32_t>::max()},
            {std::int64_t{std::numeric_limits<std::int64_t>::min()}, std::numeric_limits<std::int32_t>::min()},
            {1.9, 1}, {-1.9, -1}, {-std::ldexp(1., 63), std::numeric_limits<std::int32_t>::min()}}};
        for (const auto& [input, expected] : supportedIntegers)
        {
            saved.mGlobals[keys[2]] = input;
            readNativeSnapshot(fixture, saved);
            ASSERT_NO_THROW(world.applyOblivionRuntimeState());
            EXPECT_EQ(world.getGlobalInt(integerName), expected);
        }
        for (const double invalidScale : {1e300, -1e300})
        {
            world.setGlobalFloat(firstName, 3);
            world.setGlobalFloat(MWWorld::Globals::sTimeScale, 30);
            const auto clockBefore = world.getTimeStamp();
            auto invalidClock = saved;
            invalidClock.mClock.mTimeScale = invalidScale;
            ASSERT_NO_THROW(invalidClock.validate());
            readNativeSnapshot(fixture, invalidClock);
            EXPECT_THROW(world.applyOblivionRuntimeState(), std::runtime_error);
            EXPECT_EQ(world.getGlobalFloat(firstName), 3);
            EXPECT_EQ(world.getGlobalFloat(MWWorld::Globals::sTimeScale), 30);
            EXPECT_EQ(world.getTimeStamp(), clockBefore);
        }
        const std::array invalidScalars{"health.base", "health.modifier", "health.modified", "health.current",
            "magicka.base", "magicka.modifier", "magicka.modified", "magicka.current", "fatigue.base",
            "fatigue.modifier", "fatigue.modified", "fatigue.current", "strength.base", "strength.modifier",
            "blade.base", "blade.modifier", "breath_time.current", "level"};
        for (const auto* scalar : invalidScalars)
        {
            SCOPED_TRACE(scalar);
            world.setGlobalFloat(firstName, 3);
            const auto player = world.getPlayerPtr();
            const auto& stats = player.getClass().getNpcStats(player);
            const auto healthBefore = stats.getHealth().getCurrent();
            const auto levelBefore = stats.getLevel();
            const auto clockBefore = world.getTimeStamp();
            auto invalidPlayer = saved;
            invalidPlayer.mPlayer.mActorValues[scalar] = 1e300;
            ASSERT_NO_THROW(invalidPlayer.validate());
            readNativeSnapshot(fixture, invalidPlayer);
            EXPECT_THROW(world.applyOblivionRuntimeState(), std::runtime_error);
            EXPECT_EQ(world.getGlobalFloat(firstName), 3);
            EXPECT_EQ(stats.getHealth().getCurrent(), healthBefore);
            EXPECT_EQ(stats.getLevel(), levelBefore);
            EXPECT_EQ(world.getTimeStamp(), clockBefore);
        }
        {
            world.setGlobalFloat(firstName, 3);
            const auto player = world.getPlayerPtr();
            const auto* cellBefore = player.getCell();
            const auto clockBefore = world.getTimeStamp();
            auto missingPlayerCell = saved;
            missingPlayerCell.mClock.mHour = 8;
            missingPlayerCell.mPlayer.mCell = ESM::FormKey::content("headless.esm", 0xa99);
            ASSERT_NO_THROW(missingPlayerCell.validate());
            readNativeSnapshot(fixture, missingPlayerCell);
            EXPECT_THROW(world.applyOblivionRuntimeState(), std::runtime_error);
            EXPECT_EQ(world.getGlobalFloat(firstName), 3);
            EXPECT_EQ(player.getCell(), cellBefore);
            EXPECT_EQ(world.getTimeStamp(), clockBefore);
        }
        auto npc = addNativeNpc(fixture, 0x900);
        npc = npc.getCell()->moveTo(npc, &world.getWorldModel().getCell(cell.mId));
        ESM4::RuntimeReferenceState restoredReference;
        restoredReference.mKey = npc.getCellRef().getFormKey();
        restoredReference.mBase = ESM::FormKey::content("headless.esm", 0x800);
        restoredReference.mCell = cell.mFormKey;
        restoredReference.mEnabled = true;
        restoredReference.mPosition.pos[0] = 31;
        for (const int invalidBinding : {0, 1, 2, 3})
        {
            SCOPED_TRACE(invalidBinding);
            world.setGlobalFloat(firstName, 3);
            const auto clockBefore = world.getTimeStamp();
            const auto positionBefore = npc.getRefData().getPosition();
            auto invalidReference = saved;
            invalidReference.mReferences.push_back(restoredReference);
            auto& reference = invalidReference.mReferences.back();
            if (invalidBinding == 0)
                reference.mKey = ESM::FormKey::content("headless.esm", 0xa98);
            else if (invalidBinding == 1)
                reference.mBase = ESM::FormKey::content("headless.esm", 0xa99);
            else if (invalidBinding == 2)
                reference.mCell = ESM::FormKey::content("headless.esm", 0xa99);
            else
                reference.mOwner = ESM::FormKey::dynamic("unresolved-owner", 1);
            ASSERT_NO_THROW(invalidReference.validate());
            readNativeSnapshot(fixture, invalidReference);
            EXPECT_THROW(world.applyOblivionRuntimeState(), std::runtime_error);
            EXPECT_EQ(world.getGlobalFloat(firstName), 3);
            EXPECT_EQ(npc.getRefData().getPosition(), positionBefore);
            EXPECT_EQ(world.getTimeStamp(), clockBefore);
        }
        auto validReference = saved;
        validReference.mReferences.push_back(restoredReference);
        readNativeSnapshot(fixture, validReference);
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        EXPECT_EQ(npc.getRefData().getPosition(), restoredReference.mPosition);
        EXPECT_EQ(world.getGlobalFloat(firstName), 42);
        auto secondCell = cell;
        secondCell.mId = ESM::RefId(ESM::FormId{2, 0});
        secondCell.mFormKey = ESM::FormKey::content("headless.esm", 2);
        secondCell.mEditorId = "GlobalRestoreSecondCell";
        store.getWritable<ESM4::Cell>().insertStatic(secondCell, secondCell.mFormKey);
        auto movedReference = validReference;
        movedReference.mReferences[0].mCell = secondCell.mFormKey;
        readNativeSnapshot(fixture, movedReference);
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        MWWorld::Ptr movedNpc;
        auto& targetCell = world.getWorldModel().getCell(secondCell.mId);
        targetCell.forEach([&](const MWWorld::Ptr& candidate) {
            if (candidate.getCellRef().getFormKey() == restoredReference.mKey)
                movedNpc = candidate;
            return true;
        }, true);
        ASSERT_FALSE(movedNpc.isEmpty());
        EXPECT_EQ(movedNpc.getCell(), &targetCell);
        EXPECT_EQ(movedNpc.getRefData().getPosition(), restoredReference.mPosition);
        auto deferredReference = movedReference;
        deferredReference.mNextDynamicSerial = 2;
        deferredReference.mReferences.push_back(restoredReference);
        deferredReference.mReferences.back().mKey = ESM::FormKey::dynamic("unprojected-reference", 1);
        deferredReference.mReferences.back().mOwner = ESM::FormKey::dynamic("unresolved-owner", 1);
        ASSERT_NO_THROW(deferredReference.validate());
        readNativeSnapshot(fixture, deferredReference);
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        EXPECT_EQ(world.getGlobalFloat(firstName), 42);
        EXPECT_EQ(movedNpc.getCell(), &targetCell);
        const std::array<std::pair<std::string, ESM4::RuntimeValue>, 5> invalidCustom{{
            {"locked", std::string("wrong type")}, {"scale", 1e300},
            {"obscript.animation_scripted", std::int64_t{1}},
            {"obscript.animation_progress", std::string("wrong type")},
            {"obscript.animation_group", std::string{}}}};
        for (const auto& [field, value] : invalidCustom)
        {
            SCOPED_TRACE(field);
            world.setGlobalFloat(firstName, 3);
            const auto clockBefore = world.getTimeStamp();
            const auto positionBefore = movedNpc.getRefData().getPosition();
            const auto scaleBefore = movedNpc.getCellRef().getScale();
            auto invalidReference = movedReference;
            auto& reference = invalidReference.mReferences[0];
            reference.mPosition.pos[0] = 42;
            reference.mCustomState["obscript.animation_group"] = std::string("idle");
            reference.mCustomState["obscript.animation_progress"] = .5;
            reference.mCustomState[field] = value;
            ASSERT_NO_THROW(invalidReference.validate());
            readNativeSnapshot(fixture, invalidReference);
            EXPECT_THROW(world.applyOblivionRuntimeState(), std::runtime_error);
            EXPECT_EQ(world.getGlobalFloat(firstName), 3);
            EXPECT_EQ(movedNpc.getRefData().getPosition(), positionBefore);
            EXPECT_EQ(movedNpc.getCellRef().getScale(), scaleBefore);
            EXPECT_EQ(world.getTimeStamp(), clockBefore);
        }
        // Preserve unconsumed telemetry and legacy playing migration. Progress
        // is clamped before storing float, so large finite values remain valid.
        auto animationReference = movedReference;
        auto& animationCustom = animationReference.mReferences[0].mCustomState;
        animationCustom = {{"locked", true}, {"scale", 1.25},
            {"obscript.animation_group", std::string("idle")}, {"obscript.animation_playing", true},
            {"obscript.animation_loop_count", std::int64_t{-1}}, {"obscript.animation_absolute", true}};
        readNativeSnapshot(fixture, animationReference);
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        EXPECT_EQ(movedNpc.getCellRef().getScale(), 1.25f);
        const auto& animation = movedNpc.getRefData().getAnimationState().mScriptedAnims;
        ASSERT_EQ(animation.size(), 1u);
        EXPECT_EQ(animation[0].mGroup, "idle");
        EXPECT_EQ(animation[0].mTime, 1.f);
        EXPECT_EQ(animation[0].mLoopCount, 0u);
        EXPECT_TRUE(animation[0].mAbsolute);
        animationCustom["obscript.animation_progress"] = 1e300;
        readNativeSnapshot(fixture, animationReference);
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        EXPECT_EQ(movedNpc.getRefData().getAnimationState().mScriptedAnims[0].mTime, 1.f);
        animationCustom["obscript.animation_group"] = std::int64_t{17};
        animationCustom["obscript.animation_scripted"] = false;
        animationCustom["scale"] = std::string("unconsumed");
        animationCustom["count"] = std::string("unconsumed");
        readNativeSnapshot(fixture, animationReference);
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        EXPECT_EQ(movedNpc.getCellRef().getScale(), 1.25f);
        EXPECT_EQ(movedNpc.getRefData().getAnimationState().mScriptedAnims[0].mGroup, "idle");
        for (const auto* resource : {"health", "magicka", "fatigue"})
        {
            for (const bool oldModified : {false, true})
            {
                SCOPED_TRACE(resource);
                SCOPED_TRACE(oldModified);
                world.setGlobalFloat(firstName, 3);
                const auto player = world.getPlayerPtr();
                const auto& stats = player.getClass().getCreatureStats(player);
                const auto healthBefore = stats.getHealth();
                const auto magickaBefore = stats.getMagicka();
                const auto fatigueBefore = stats.getFatigue();
                const auto clockBefore = world.getTimeStamp();
                auto invalidComposite = saved;
                const std::string prefix(resource);
                invalidComposite.mPlayer.mActorValues[prefix + ".base"] = oldModified ? -3e38 : 3e38;
                invalidComposite.mPlayer.mActorValues[prefix + (oldModified ? ".modified" : ".modifier")] = 3e38;
                ASSERT_NO_THROW(invalidComposite.validate());
                readNativeSnapshot(fixture, invalidComposite);
                EXPECT_THROW(world.applyOblivionRuntimeState(), std::runtime_error);
                EXPECT_EQ(world.getGlobalFloat(firstName), 3);
                EXPECT_EQ(stats.getHealth(), healthBefore);
                EXPECT_EQ(stats.getMagicka(), magickaBefore);
                EXPECT_EQ(stats.getFatigue(), fatigueBefore);
                EXPECT_EQ(world.getTimeStamp(), clockBefore);
            }
        }
        saved.mPlayer.mActorValues = {{"health.base", 100}, {"health.modifier", 0}, {"health.current", 100},
            {"magicka.base", 20}, {"magicka.modifier", 0}, {"magicka.current", 20}, {"fatigue.base", 40},
            {"fatigue.modifier", 0}, {"fatigue.current", 40}, {"strength.base", 40}, {"strength.modifier", 0},
            {"blade.base", 5}, {"blade.modifier", 0}, {"breath_time.current", 10}, {"level", 1}};
        // An explicit modifier supersedes old modified telemetry; unknown
        // telemetry is unconsumed. Preflight must validate consumed inputs.
        saved.mPlayer.mActorValues["health.modified"] = 1e300;
        saved.mPlayer.mActorValues["unused.telemetry"] = 1e300;
        readNativeSnapshot(fixture, saved);
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        EXPECT_EQ(world.getGlobalFloat(firstName), 42);
        EXPECT_EQ(world.getGlobalFloat(secondName), 10);
        for (const double sign : {-1., 1.})
        {
            saved.mPlayer.mActorValues["health.base"] = sign * 3e38;
            saved.mPlayer.mActorValues["health.modifier"] = -sign * 3e38;
            readNativeSnapshot(fixture, saved);
            ASSERT_NO_THROW(world.applyOblivionRuntimeState());
            const auto player = world.getPlayerPtr();
            const auto& health = player.getClass().getCreatureStats(player).getHealth();
            EXPECT_EQ(health.getModified(false), 0);
            EXPECT_TRUE(std::isfinite(health.getModifier()));
            EXPECT_EQ(health.getCurrent(), 100);
        }
    }

    TEST(OblivionWorldTest, nativeWorldPlayerDataConstructionDoesNotRequireRendering)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        MWClass::Npc::registerSelf();
        EXPECT_TRUE(world.getPlayerPtr().isEmpty());
        // Actual loaded native profile and projected Player record; no World
        // rendering/physics initialization. This is data construction only.
        world.setupPlayer();
        const auto ptr = world.getPlayerPtr();
        ASSERT_FALSE(ptr.isEmpty());
        EXPECT_EQ(world.getPlayerConstPtr(), MWWorld::ConstPtr(ptr));
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 100);
        EXPECT_FALSE(ptr.getClass().getCreatureStats(ptr).getHealth().isNativeProjection());
        EXPECT_EQ(world.getOblivionCombatService()->findActorValues(ESM::FormKey::dynamic("player", 1)), nullptr);
        world.setupPlayer();
        EXPECT_EQ(world.getPlayerPtr(), ptr);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 100);
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
