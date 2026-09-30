#include <gtest/gtest.h>

#include <array>
#include <fstream>
#include <stdexcept>

#include <components/esm/records.hpp>
#include <components/files/collections.hpp>
#include <components/loadinglistener/loadinglistener.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/testing/util.hpp>

#include "apps/openmw/mwbase/environment.hpp"
#include "apps/openmw/mwclass/esm4npc.hpp"
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
