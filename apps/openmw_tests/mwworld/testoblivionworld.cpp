#include <components/esm3/inventorystate.hpp>
#include <bit>
#include <components/esm4/loadweap.hpp>
#include "apps/openmw/mwclass/weapon.hpp"
#include "apps/openmw/mwclass/clothing.hpp"
#include <components/esm4/loadclot.hpp>
#include "apps/openmw/mwclass/armor.hpp"
#include "apps/openmw/mwworld/oblivionprofileservices.hpp"
#include "apps/openmw/mwworld/oblivioninteraction.hpp"
#include "apps/openmw/mwrender/animation.hpp"
#include "apps/openmw/mwmechanics/character.hpp"
#include "apps/openmw/mwsound/nativeaudioutils.hpp"
#include <components/esm4/loadsoun.hpp>
#include <components/sceneutil/keyframe.hpp>
#include <components/settings/values.hpp>
#include <components/vfs/filesystemarchive.hpp>
#include <osg/MatrixTransform>
#include <osg/Geode>
#include <osg/Geometry>
#include <osgDB/WriteFile>
#include "apps/openmw/mwphysics/actor.hpp"
#include "apps/openmw/mwphysics/physicssystem.hpp"
#include "apps/openmw/mwphysics/oblivionragdoll.hpp"
#include "apps/openmw/mwmechanics/oblivionmelee.hpp"
#include <components/esm4/loadbsgn.hpp>
#include "apps/openmw/mwworld/player.hpp"
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <fstream>
#include <limits>
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
#include <components/resource/scenemanager.hpp>
#include <components/shader/shadermanager.hpp>
#include <components/sceneutil/lightmanager.hpp>
#include <components/sceneutil/shadow.hpp>
#include <components/testing/util.hpp>

#include "apps/openmw/mwbase/environment.hpp"
#include "apps/openmw/mwclass/esm4npc.hpp"
#include "apps/openmw/mwclass/npc.hpp"
#include "apps/openmw/mwclass/esm4interactive.hpp"
#include "apps/openmw/mwmechanics/oblivioncombat.hpp"
#include "apps/openmw/mwmechanics/actors.hpp"
#include "apps/openmw/mwlua/context.hpp"
#include "apps/openmw/mwlua/localscripts.hpp"
#include "apps/openmw/mwlua/luamanagerimp.hpp"
#include "apps/openmw/mwlua/stats.hpp"
#include "apps/openmw/mwsound/soundmanagerimp.hpp"
#include "apps/openmw/mwworld/worldimp.hpp"
#include "apps/openmw/mwworld/oblivionscriptmanager.hpp"
#include "apps/openmw/mwworld/oblivionactorstats.hpp"
#include "apps/openmw/mwworld/timestamp.hpp"

namespace
{
    // A real native content-load path with no renderer, input, physics or audio
    // device. Registration tests exercise actual Actors admission without an
    // animation/controller; this remains a headless integration fixture.
    struct NativeWorldFixture
    {
        const std::filesystem::path mDirectory = TestingOpenMW::currentTestDirPath();
        MWBase::Environment mEnvironment;
        VFS::Manager mVfs;
        Resource::ResourceSystem mResources{&mVfs, 0., nullptr};
        MWWorld::World mWorld{&mResources, -1, "", mDirectory, ESM::GameProfile::Oblivion};
        std::unique_ptr<MWLua::LuaManager> mLuaManager;
        std::unique_ptr<MWSound::SoundManager> mSoundManager;

        explicit NativeWorldFixture(bool scriptsFirst = false, bool extraNative = false, bool activationScript = false)
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
                subrecord(ESM::fourCC("HEDR"), bytes(1.f)
                    + bytes(std::uint32_t{activationScript ? 2u : 1u})
                    + bytes(std::uint32_t{activationScript ? 0x881u : 0x801u})));
            const auto npc = record(ESM4::REC_NPC_, 0x800,
                subrecord(ESM::fourCC("EDID"), std::string("Player\0", 7))
                + subrecord(ESM::fourCC("ACBS"), bytes(config).substr(0, 16))
                + subrecord(ESM::fourCC("DATA"), bytes(skills) + bytes(std::uint32_t{100}) + bytes(attributes))
                + (activationScript ? subrecord(ESM::fourCC("SCRI"), bytes(std::uint32_t{0x880})) : std::string{}));
            {
                std::ofstream stream(mDirectory / "headless.esm", std::ios::binary);
                stream << header << npc;
                if (activationScript)
                {
                    const std::array<std::uint32_t, 6> local{1, 0, 0, 0, 1, 0};
                    stream << record(ESM4::REC_SCPT, 0x880,
                        subrecord(ESM::fourCC("EDID"), std::string("ActivationReceiver\0", 19))
                        + subrecord(ESM::fourCC("SLSD"), bytes(local))
                        + subrecord(ESM::fourCC("SCVR"), std::string("calls\0", 6))
                        + subrecord(ESM::fourCC("SCTX"), "scn ActivationReceiver\nshort calls\n"
                            "Begin OnActivate\nset calls to calls + 1\nEnd\n"));
                }
                if (!stream)
                    throw std::runtime_error("failed to write native world fixture");
            }
            Files::Collections files(Files::PathContainer{mDirectory});
            Loading::Listener listener;
            if (scriptsFirst)
            {
                std::ofstream scripts(mDirectory / "prefix.omwscripts");
                scripts << "# Empty script configuration shifts the resolved content index.\n";
            }
            std::vector<std::string> content;
            if (scriptsFirst)
                content.push_back("prefix.omwscripts");
            content.push_back("headless.esm");
            if (extraNative)
            {
                std::ofstream second(mDirectory / "second.esm", std::ios::binary);
                second << record(ESM4::REC_TES4, 0, subrecord(ESM::fourCC("HEDR"),
                    bytes(1.f) + bytes(std::uint32_t{0}) + bytes(std::uint32_t{0x800})));
                second.close();
                if (!second)
                    throw std::runtime_error("failed to write second native content fixture");
                content.push_back("second.esm");
            }
            mWorld.loadData(files, content, {}, nullptr, &listener);
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
    TEST(OblivionWorld, PhysicalSystemOwnsCapsuleHandoffSnapshotsAndRemoval)
    {
        struct RestoreThreads
        {
            int mPrevious = Settings::physics().mAsyncNumThreads;
            ~RestoreThreads() { Settings::physics().mAsyncNumThreads.set(mPrevious); }
        } restoreThreads;
        for (int threads : {0, 1, 2})
        {
            SCOPED_TRACE(threads);
            Settings::physics().mAsyncNumThreads.set(threads);
            NativeWorldFixture fixture;
            auto ptr = addNativeNpc(fixture, 0x900);
            // An editable synthetic mesh exercises public VFS/resource loading
            // and Actor admission without requiring installed game assets.
            osg::ref_ptr<osg::Geode> model = new osg::Geode;
            osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
            osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
            vertices->push_back(osg::Vec3(-1, -1, -1));
            vertices->push_back(osg::Vec3(1, -1, -1));
            vertices->push_back(osg::Vec3(0, 1, -1));
            vertices->push_back(osg::Vec3(0, 0, 1));
            geometry->setVertexArray(vertices);
            osg::ref_ptr<osg::DrawElementsUInt> triangles = new osg::DrawElementsUInt(GL_TRIANGLES);
            for (unsigned index : {0u, 2u, 1u, 0u, 1u, 3u, 1u, 2u, 3u, 2u, 0u, 3u})
                triangles->push_back(index);
            geometry->addPrimitiveSet(triangles);
            model->addDrawable(geometry);
            ASSERT_TRUE(osgDB::writeNodeFile(*model, (fixture.mDirectory / "physical-owner.osgt").string()));
            fixture.mVfs.addArchive(std::make_unique<VFS::FileSystemArchive>(fixture.mDirectory));
            fixture.mVfs.buildIndex();
            const VFS::Path::Normalized path("physical-owner.osgt");
            auto* scene = fixture.mResources.getSceneManager();
            scene->setShaderPath(std::filesystem::path(OPENMW_PROJECT_SOURCE_DIR) / "files/shaders");
            auto defines = Shader::getDefaultDefines();
            for (const auto& [name, value] : SceneUtil::ShadowManager::getShadowsDisabledDefines())
                defines[name] = value;
            osg::ref_ptr<SceneUtil::LightManager> lights
                = new SceneUtil::LightManager(SceneUtil::LightSettings{}, &fixture.mResources);
            for (const auto& [name, value] : lights->getLightDefines())
                defines[name] = value;
            scene->getShaderManager().setGlobalDefines(defines);
            MWPhysics::PhysicsSystem physics(&fixture.mResources, new osg::Group);
            NifBullet::ActorRagdollDefinition graph;
            graph.mSourceHash = std::string(16, 'a');
            NifBullet::RagdollBodyDefinition body{};
            body.mRecord = 12;
            body.mNodeRecord = 8;
            body.mMass = 2;
            body.mInertia = {1, 0, 0, 0, 1, 0, 0, 0, 1};
            body.mShape = NifBullet::RagdollSphere{.5f};
            graph.mBodies.push_back(body);
            const std::array<btTransform, 1> poses{
                btTransform(btQuaternion::getIdentity(), btVector3(0, 0, 20))};
            NifBullet::RagdollInternalCollisionFilter internalFilter;
            internalFilter.mSystemGroup = 10;
            const auto admit = [&](const auto& definition) {
                physics.addActorRagdoll(ptr, definition, 1, poses,
                    MWPhysics::CollisionType_Actor, MWPhysics::CollisionType_World, &internalFilter);
            };
            EXPECT_THROW(admit(graph), std::invalid_argument);
            EXPECT_FALSE(physics.hasActorRagdoll(ptr));
            physics.addActor(ptr, path);
            auto* capsule = physics.getActor(ptr);
            ASSERT_NE(capsule, nullptr);
            auto invalid = graph;
            invalid.mBodies[0].mMass = 0;
            EXPECT_THROW(admit(invalid), std::invalid_argument);
            EXPECT_FALSE(capsule->isCollisionSuspended());
            ASSERT_NE(capsule->getCollisionObject()->getBroadphaseHandle(), nullptr);
            invalid = graph;
            invalid.mBodies[0].mInfoFilter.mLayer = 32;
            EXPECT_THROW(admit(invalid), std::invalid_argument);
            EXPECT_FALSE(physics.hasActorRagdoll(ptr));
            EXPECT_FALSE(capsule->isCollisionSuspended());
            ASSERT_NE(capsule->getCollisionObject()->getBroadphaseHandle(), nullptr);
            admit(graph);
            EXPECT_TRUE(physics.hasActorRagdoll(ptr));
            EXPECT_TRUE(capsule->isCollisionSuspended());
            EXPECT_EQ(capsule->getCollisionObject()->getBroadphaseHandle(), nullptr);
            EXPECT_THROW(admit(graph), std::invalid_argument);
            const auto duplicate = addNativeNpc(fixture, 0x901);
            physics.addActor(duplicate, path);
            EXPECT_THROW(physics.updatePtr(ptr, duplicate), std::invalid_argument);
            EXPECT_THROW(physics.updatePtr(ptr, {}), std::invalid_argument);
            EXPECT_EQ(physics.getActor(ptr), capsule);
            EXPECT_TRUE(physics.hasActorRagdoll(ptr));
            EXPECT_FALSE(physics.hasActorRagdoll(duplicate));
            EXPECT_FALSE(physics.getActor(duplicate)->isCollisionSuspended());
            physics.remove(duplicate);
            MWWorld::LiveCellRef<ESM4::Npc> rebound(*ptr.get<ESM4::Npc>());
            const MWWorld::Ptr updated(&rebound, ptr.getCell());
            const auto previous = ptr;
            physics.updatePtr(previous, updated);
            EXPECT_EQ(physics.getActor(previous), nullptr);
            EXPECT_EQ(physics.getActor(updated), capsule);
            EXPECT_EQ(capsule->getPtr(), updated);
            EXPECT_TRUE(capsule->isCollisionSuspended());
            EXPECT_FALSE(physics.hasActorRagdoll(previous));
            EXPECT_TRUE(physics.hasActorRagdoll(updated));
            EXPECT_THROW(physics.captureActorRagdoll(previous), std::invalid_argument);
            physics.remove(previous); // A stale reference cannot remove the new owner.
            EXPECT_EQ(physics.getActor(updated), capsule);
            EXPECT_TRUE(physics.hasActorRagdoll(updated));
            physics.updatePtr(updated, updated);
            ptr = updated;
            EXPECT_EQ(physics.actorRagdollDefinition(ptr).mSourceHash, graph.mSourceHash);
            const auto base = ESM::FormKey::content("headless.esm", 0x800);
            const auto original = physics.captureActorRagdollSnapshot(ptr, base, path.value());
            EXPECT_EQ(physics.captureActorRagdoll(ptr)[0].mPose, poses[0]);
            physics.applyActorRagdollImpulse(ptr, 0, btVector3(2, 0, 0), poses[0].getOrigin());
            EXPECT_EQ(physics.captureActorRagdoll(ptr)[0].mLinearVelocity, btVector3(1, 0, 0));
            physics.restoreActorRagdollSnapshot(ptr, original, base, path.value());
            EXPECT_EQ(physics.captureActorRagdollSnapshot(ptr, base, path.value()), original);
            const std::array<NifBullet::RagdollNativeVelocityDrive, 1> drives{{
                {12, {{1, 0, 20}, {0, 0, 0, 1}}, .5f}}};
            EXPECT_THROW(physics.driveActorRagdollPoseVelocities(previous, drives, 120.f), std::invalid_argument);
            physics.driveActorRagdollPoseVelocities(ptr, drives, 120.f);
            EXPECT_TRUE(capsule->isCollisionSuspended());
            EXPECT_EQ(physics.captureActorRagdoll(ptr)[0].mLinearVelocity.x(), 60);
            EXPECT_EQ(physics.captureActorRagdoll(ptr)[0].mPose, poses[0]);
            physics.restoreActorRagdollSnapshot(ptr, original, base, path.value());
            EXPECT_EQ(physics.captureActorRagdollSnapshot(ptr, base, path.value()), original);
            auto bad = original;
            bad.mAssetHash[0] = '0';
            EXPECT_THROW(physics.restoreActorRagdollSnapshot(ptr, bad, base, path.value()), std::invalid_argument);
            EXPECT_EQ(physics.captureActorRagdollSnapshot(ptr, base, path.value()), original);
            physics.removeActorRagdoll(ptr);
            physics.removeActorRagdoll(ptr);
            EXPECT_FALSE(physics.hasActorRagdoll(ptr));
            EXPECT_FALSE(capsule->isCollisionSuspended());
            ASSERT_NE(capsule->getCollisionObject()->getBroadphaseHandle(), nullptr);
            admit(graph);
            physics.remove(ptr);
            EXPECT_FALSE(physics.hasActorRagdoll(ptr));
            EXPECT_EQ(physics.getActor(ptr), nullptr);
        }
    }

    ESM::FormKey addEquipmentWeapon(NativeWorldFixture& fixture, std::uint32_t id = 0x940)
    {
        auto& store = fixture.mWorld.getStore();
        MWClass::Weapon::registerSelf();
        ESM4::Weapon native{}; native.mId = {id, 0};
        native.mData.health = 100; native.mData.speed = native.mData.reach = 1;
        const auto key = ESM::FormKey::content("headless.esm", id);
        store.getWritable<ESM4::Weapon>().insertStatic(native, key);
        ESM::Weapon projected; projected.blank(); projected.mId = ESM::RefId(native.mId);
        projected.mData.mType = ESM::Weapon::LongBladeOneHand; projected.mData.mHealth = 100;
        store.insertStatic(projected);
        return key;
    }

    MWWorld::Ptr addEquipmentCreature(NativeWorldFixture& fixture)
    {
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
        ESM4::Creature base{}; base.mId = {0x820, 0};
        base.mFormKey = ESM::FormKey::content("headless.esm", 0x820); base.mAttackReach = 64;
        base.mData.health = 19; base.mData.combat = 10;
        base.mBaseConfig.tes4.levelOrOffset = 1; base.mBaseConfig.tes4.fatigue = 35;
        store.getWritable<ESM4::Creature>().insertStatic(base, base.mFormKey);
        MWClass::ESM4Creature::registerSelf();
        ESM4::ActorCreature placed{}; placed.mId = {0x920, 0};
        placed.mFormKey = ESM::FormKey::content("headless.esm", 0x920);
        placed.mBaseObj = base.mId; placed.mBaseKey = base.mFormKey;
        store.getWritable<ESM4::ActorCreature>().insertStatic(placed, placed.mFormKey);
        MWWorld::LiveCellRef<ESM4::Creature> live(placed, store.search<ESM4::Creature>(base.mFormKey));
        auto& cell = world.getWorldModel().getDraftCell(); const MWWorld::Ptr actor(cell.insert(&live), &cell);
        world.getWorldModel().registerPtr(actor);
        return actor;
    }

    void installEquipmentInventory(NativeWorldFixture& fixture, const MWWorld::Ptr& actor,
        const std::vector<ESM4::RuntimeInventoryItem>& items)
    {
        const auto prepared = MWWorld::OblivionProfileServices::prepareActorInventory(
            fixture.mWorld.getStore(), ESM::FormKeyResolver({"headless.esm"}), items);
        auto staged = MWWorld::OblivionProfileServices::stageActorInventory(prepared);
        actor.getClass().getInventoryStore(actor).swapPreparedContents(*staged);
    }

    void checkLazyNativeInventoryRestoration(NativeWorldFixture& fixture, const MWWorld::Ptr& actor)
    {
        auto& world = fixture.mWorld;
        const auto weapon = addEquipmentWeapon(fixture);
        ESM4::RuntimeInventoryItem equipped;
        equipped.mBase = weapon; equipped.mCount = 1; equipped.mCondition = 55.125f;
        equipped.mEquippedSlots = ESM4::InventorySlotWeapon;
        equipped.mOwner = ESM::FormKey::content("headless.esm", 0x800);
        auto other = equipped; other.mCount = 2; other.mCondition = 43.125f;
        other.mEquippedSlots = 0; other.mOwner = {};
        auto saved = captureNativeActorState(fixture, actor);
        saved.mReferences.front().mInventory = {equipped, other};
        readNativeSnapshot(fixture, saved);
        ASSERT_EQ(actor.getRefData().getCustomData(), nullptr);
        const auto authority = captureNativeActorState(fixture, actor).serializeBinary();
        auto& inventory = actor.getClass().getInventoryStore(actor);
        EXPECT_EQ(inventory.count(ESM::RefId(ESM::FormId{0x940, 0})), 3);
        const auto captured = world.captureOblivionActorInventory(actor);
        ASSERT_EQ(captured.size(), 2);
        EXPECT_EQ(captured[0].mCount, 1);
        EXPECT_EQ(captured[0].mCondition, 55.125);
        EXPECT_EQ(captured[0].mOwner, equipped.mOwner);
        EXPECT_EQ(captured[0].mEquippedSlots, ESM4::InventorySlotWeapon);
        EXPECT_EQ(captured[1].mCount, 2);
        EXPECT_EQ(captured[1].mCondition, 43.125);
        EXPECT_TRUE(captured[1].mOwner.isNull());
        EXPECT_EQ(captured[1].mEquippedSlots, 0u);
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), authority);
        EXPECT_EQ(world.getOblivionCombatService()->findActorValues(actor.getCellRef().getFormKey()), nullptr);
        EXPECT_EQ(world.getOblivionCombatService()->findActorLife(actor.getCellRef().getFormKey()), nullptr);
        // Ordinary subsequent reads must keep the live view, not replay a save.
        inventory.begin()->getCellRef().setNativeItemCondition(25.125f);
        EXPECT_EQ(actor.getClass().getInventoryStore(actor).begin()->getCellRef().getNativeItemCondition(), 25.125f);
    }

    TEST(OblivionWorldTest, LazyNativeNpcInventoryRestoresSavedMetadataWithoutAuthorityBirth)
    {
        NativeWorldFixture fixture;
        const auto actor = addNativeNpc(fixture, 0x900);
        fixture.mWorld.getWorldModel().registerPtr(actor);
        checkLazyNativeInventoryRestoration(fixture, actor);
    }

    TEST(OblivionWorldTest, LazyNativeCreatureInventoryRestoresSavedMetadataWithoutAuthorityBirth)
    {
        NativeWorldFixture fixture;
        checkLazyNativeInventoryRestoration(fixture, addEquipmentCreature(fixture));
    }

    TEST(OblivionWorldTest, LazyNativeSavedEmptyInventorySuppressesBaseStock)
    {
        for (const bool creature : {false, true})
        {
            SCOPED_TRACE(creature);
            NativeWorldFixture fixture;
            auto& world = fixture.mWorld;
            addEquipmentWeapon(fixture);
            const auto actor = creature ? addEquipmentCreature(fixture) : addNativeNpc(fixture, 0x900);
            auto& store = world.getStore();
            if (creature)
            {
                auto base = *actor.get<ESM4::Creature>()->mBase;
                base.mInventory.push_back({0x940, 9});
                store.getWritable<ESM4::Creature>().insertStatic(base, base.mFormKey);
                actor.get<ESM4::Creature>()->mBase = store.search<ESM4::Creature>(base.mFormKey);
            }
            else
            {
                auto base = *actor.get<ESM4::Npc>()->mBase;
                base.mInventory.push_back({0x940, 9});
                store.getWritable<ESM4::Npc>().insertStatic(base, base.mFormKey);
                actor.get<ESM4::Npc>()->mBase = store.search<ESM4::Npc>(base.mFormKey);
            }
            auto saved = captureNativeActorState(fixture, actor);
            ASSERT_TRUE(saved.mReferences.front().mInventory.empty());
            readNativeSnapshot(fixture, saved);
            const auto before = saved.serializeBinary();
            auto prepared = world.prepareOblivionSavedActorInventory(actor);
            ASSERT_NE(prepared, nullptr);
            EXPECT_EQ(prepared->begin(), prepared->end());
            EXPECT_EQ(actor.getRefData().getCustomData(), nullptr);
            EXPECT_EQ(actor.getClass().getInventoryStore(actor).begin(), actor.getClass().getInventoryStore(actor).end());
            EXPECT_TRUE(world.captureOblivionActorInventory(actor).empty());
            EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), before);
        }
    }

    TEST(OblivionWorldTest, LazyNativeSavedInventoryRejectsWrongBaseBeforePublishingClass)
    {
        for (const bool creature : {false, true})
        {
            SCOPED_TRACE(creature);
            NativeWorldFixture fixture;
            const auto npc = addNativeNpc(fixture, 0x900);
            const auto other = addEquipmentCreature(fixture);
            const auto actor = creature ? other : npc;
            auto saved = captureNativeActorState(fixture, actor);
            saved.mReferences.front().mBase = ESM::FormKey::content("headless.esm", creature ? 0x800 : 0x820);
            readNativeSnapshot(fixture, saved);
            const auto before = captureNativeActorState(fixture, actor).serializeBinary();
            for (int attempt = 0; attempt < 2; ++attempt)
            {
                EXPECT_THROW(actor.getClass().getInventoryStore(actor), std::invalid_argument);
                EXPECT_EQ(actor.getRefData().getCustomData(), nullptr);
                EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), before);
            }
        }
    }

    TEST(OblivionWorldTest, LazyNativeSavedInventoryRejectsLateInvalidItemWithoutPartialPublication)
    {
        for (const bool creature : {false, true})
        {
            SCOPED_TRACE(creature);
            NativeWorldFixture fixture;
            const auto actor = creature ? addEquipmentCreature(fixture) : addNativeNpc(fixture, 0x900);
            const auto weapon = addEquipmentWeapon(fixture);
            auto saved = captureNativeActorState(fixture, actor);
            ESM4::RuntimeInventoryItem first;
            first.mBase = weapon; first.mCount = 3; first.mCondition = 43.125f;
            auto invalid = first;
            invalid.mBase = ESM::FormKey::content("headless.esm", 0x999);
            saved.mReferences.front().mInventory = {first, invalid};
            readNativeSnapshot(fixture, saved);
            const auto before = captureNativeActorState(fixture, actor).serializeBinary();
            for (int attempt = 0; attempt < 2; ++attempt)
            {
                EXPECT_THROW(actor.getClass().getInventoryStore(actor), std::runtime_error);
                EXPECT_EQ(actor.getRefData().getCustomData(), nullptr);
                EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), before);
            }
        }
    }

    TEST(OblivionWorldTest, NativeSavedInventoryAbsenceDoesNotConstructClassOrAuthority)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addNativeNpc(fixture, 0x900);
        const auto other = addEquipmentCreature(fixture);
        EXPECT_EQ(world.prepareOblivionSavedActorInventory(actor), nullptr);
        readNativeSnapshot(fixture, captureNativeActorState(fixture, actor));
        EXPECT_EQ(world.prepareOblivionSavedActorInventory(other), nullptr);
        EXPECT_EQ(world.prepareOblivionSavedActorInventory({}), nullptr);
        EXPECT_EQ(actor.getRefData().getCustomData(), nullptr);
        EXPECT_EQ(other.getRefData().getCustomData(), nullptr);
        EXPECT_EQ(world.getOblivionCombatService()->findActorValues(actor.getCellRef().getFormKey()), nullptr);
        EXPECT_EQ(world.getOblivionCombatService()->findActorLife(actor.getCellRef().getFormKey()), nullptr);
        EXPECT_EQ(world.getOblivionCombatService()->findActorValues(other.getCellRef().getFormKey()), nullptr);
        EXPECT_EQ(world.getOblivionCombatService()->findActorLife(other.getCellRef().getFormKey()), nullptr);
    }

    TEST(OblivionWorldTest, LazyNativeInventoryUsesLegacyEquipmentMigrationOnlyBeforeMetadataSchema)
    {
        for (const bool creature : {false, true})
            for (const std::uint32_t version : {1u, 2u, 3u, 4u})
            {
                SCOPED_TRACE(creature);
                SCOPED_TRACE(version);
                NativeWorldFixture fixture;
                auto& world = fixture.mWorld;
                const auto actor = creature ? addEquipmentCreature(fixture) : addNativeNpc(fixture, 0x900);
                const auto weapon = addEquipmentWeapon(fixture);
                auto saved = captureNativeActorState(fixture, actor);
                saved.mVersion = version;
                if (version < 3)
                {
                    // Legacy versions predate the fixture's race/class fields.
                    saved.mPlayer.mRace = {};
                    saved.mPlayer.mClass = {};
                }
                ESM4::RuntimeInventoryItem item;
                item.mBase = weapon; item.mCount = 3;
                saved.mReferences.front().mInventory = {item};
                readNativeSnapshot(fixture, saved);
                const auto inventory = world.captureOblivionActorInventory(actor);
                // Existing legacy equip migration splits one equipped weapon
                // from the two remaining unequipped items; it conserves three.
                ASSERT_EQ(inventory.size(), version < 4 ? 2u : 1u);
                if (version < 4)
                {
                    EXPECT_EQ(inventory[0].mCount, 2);
                    EXPECT_EQ(inventory[0].mEquippedSlots, 0u);
                    EXPECT_EQ(inventory[1].mCount, 1);
                    EXPECT_EQ(inventory[1].mEquippedSlots, ESM4::InventorySlotWeapon);
                }
                else
                {
                    EXPECT_EQ(inventory.front().mCount, 3);
                    EXPECT_EQ(inventory.front().mEquippedSlots, 0u);
                }
                EXPECT_EQ(world.getOblivionCombatService()->findActorValues(actor.getCellRef().getFormKey()), nullptr);
                EXPECT_EQ(world.getOblivionCombatService()->findActorLife(actor.getCellRef().getFormKey()), nullptr);
            }
    }

    TEST(OblivionWorldTest, NativeScriptAddItemZeroCountDoesNotConstructInventoryOrAuthority)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addNativeNpc(fixture, 0x900);
        world.getWorldModel().registerPtr(actor);
        const auto weapon = addEquipmentWeapon(fixture);
        ESM4::RuntimeInventoryItem item;
        item.mBase = weapon;
        item.mCount = 3;
        item.mCondition = 43.125f;
        item.mOwner = ESM::FormKey::content("headless.esm", 0x800);
        installEquipmentInventory(fixture, actor, {item});
        MWWorld::OblivionScriptManager host(world, world.getStore(), {"headless.esm"});
        ObScript::RuntimeContext context;
        context.mSelf = actor.getCellRef().getFormKey();
        const std::vector<ObScript::Value> args{ObScript::ReferenceValue{weapon, {}}, std::int64_t(0)};
        EXPECT_NO_THROW(EXPECT_EQ(ObScript::asInteger(host.call("AddItem", {}, args, context, {})), 0));
        const auto captured = world.captureOblivionActorInventory(actor);
        ASSERT_EQ(captured.size(), 1u);
        EXPECT_EQ(captured.front().mCount, 3);
        EXPECT_EQ(captured.front().mCondition, 43.125f);
        EXPECT_EQ(captured.front().mOwner, item.mOwner);
        EXPECT_EQ(world.getOblivionCombatService()->findActorValues(context.mSelf), nullptr);
        EXPECT_EQ(world.getOblivionCombatService()->findActorLife(context.mSelf), nullptr);
    }

    TEST(OblivionWorldTest, NativePlayerInventoryCapturePreservesMetadataHotkeysWithoutAuthorityBirth)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        MWClass::Npc::registerSelf();
        world.setupPlayer();
        const auto actor = addNativeNpc(fixture, 0x900);
        const auto weapon = addEquipmentWeapon(fixture);
        auto saved = captureNativeActorState(fixture, actor);
        ESM4::RuntimeInventoryItem equipped;
        equipped.mBase = weapon; equipped.mCount = 1; equipped.mCondition = 55.125f;
        equipped.mOwner = ESM::FormKey::content("headless.esm", 0x800);
        equipped.mEquippedSlots = ESM4::InventorySlotWeapon; equipped.mHotkey = 5;
        auto other = equipped; other.mCount = 2; other.mCondition = 43.125f;
        other.mOwner = {}; other.mEquippedSlots = 0; other.mHotkey = -1;
        saved.mPlayer.mInventory = {equipped, other};
        readNativeSnapshot(fixture, saved);
        const auto player = world.getPlayerPtr();
        installEquipmentInventory(fixture, player, saved.mPlayer.mInventory);
        const auto before = captureNativeActorState(fixture, actor).serializeBinary();
        EXPECT_EQ(world.captureOblivionActorInventory(player), saved.mPlayer.mInventory);
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), before);
        const auto key = ESM::FormKey::dynamic("player", 1);
        EXPECT_EQ(world.getOblivionCombatService()->findActorValues(key), nullptr);
        EXPECT_EQ(world.getOblivionCombatService()->findActorLife(key), nullptr);
    }

    TEST(OblivionWorldTest, NativePlayerMissingItemRemovalDoesNotConstructAuthority)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        MWClass::Npc::registerSelf();
        world.setupPlayer();
        const auto weapon = addEquipmentWeapon(fixture);
        const auto key = ESM::FormKey::dynamic("player", 1);
        EXPECT_NO_THROW(EXPECT_EQ(world.oblivionChangePlayerInventory(weapon, -999), 0));
        EXPECT_TRUE(world.getPlayerPtr().getClass().getInventoryStore(world.getPlayerPtr()).begin()
            == world.getPlayerPtr().getClass().getInventoryStore(world.getPlayerPtr()).end());
        EXPECT_EQ(world.getOblivionCombatService()->findActorValues(key), nullptr);
        EXPECT_EQ(world.getOblivionCombatService()->findActorLife(key), nullptr);
    }

    TEST(OblivionWorldTest, NativePlayerRemovalHonorsSignedMinimumAvailabilityWithoutMutation)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        MWClass::Npc::registerSelf();
        world.setupPlayer();
        const auto actor = addNativeNpc(fixture, 0x900);
        const auto weapon = addEquipmentWeapon(fixture);
        ESM4::RuntimeInventoryItem large;
        large.mBase = weapon; large.mCount = std::numeric_limits<std::int32_t>::max();
        large.mCondition = 43.125f; large.mOwner = ESM::FormKey::content("headless.esm", 0x800);
        auto one = large; one.mCount = 1; one.mCondition = 55.125f;
        const auto player = world.getPlayerPtr();
        installEquipmentInventory(fixture, player, {large, one});
        const auto before = captureNativeActorState(fixture, actor).serializeBinary();
        EXPECT_EQ(world.oblivionPlayerItemCount(weapon), std::numeric_limits<std::int32_t>::min());
        EXPECT_NO_THROW(EXPECT_EQ(world.oblivionChangePlayerInventory(weapon, -7), 0));
        auto& live = player.getClass().getInventoryStore(player);
        ASSERT_EQ(std::distance(live.begin(), live.end()), 2);
        EXPECT_EQ(live.begin()->getCellRef().getCount(), large.mCount);
        EXPECT_EQ(live.begin()->getCellRef().getItemCondition(100), large.mCondition);
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), before);
        EXPECT_EQ(world.getOblivionCombatService()->findActorValues(ESM::FormKey::dynamic("player", 1)), nullptr);
        EXPECT_EQ(world.getOblivionCombatService()->findActorLife(ESM::FormKey::dynamic("player", 1)), nullptr);
    }

    TEST(OblivionWorldTest, NativePlayerNegativeAddDeltaIsNotNormalizedToOnePhysicalItem)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto weapon = addEquipmentWeapon(fixture);
        MWWorld::OblivionScriptManager host(world, world.getStore(), {"headless.esm"});
        ObScript::RuntimeContext context;
        context.mSelf = ESM::FormKey::dynamic("player", 1);
        const std::vector<ObScript::Value> args{ObScript::ReferenceValue{weapon, {}}, std::int64_t(-1)};
        // Full signed native entry persistence is still unsupported. Reject
        // explicitly before physical mutation or constructing a Player view.
        EXPECT_THROW(host.call("AddItem", {}, args, context, {}), std::invalid_argument);
        EXPECT_EQ(world.getOblivionCombatService()->findActorValues(context.mSelf), nullptr);
        EXPECT_EQ(world.getOblivionCombatService()->findActorLife(context.mSelf), nullptr);
    }

    TEST(OblivionWorldTest, NativeScriptPlayerNonpositiveInventoryNoopsDoNotRequirePlayerView)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto weapon = addEquipmentWeapon(fixture);
        MWWorld::OblivionScriptManager host(world, world.getStore(), {"headless.esm"});
        ObScript::RuntimeContext context;
        context.mSelf = ESM::FormKey::dynamic("player", 1);
        for (const auto& [command, count] : std::array{
                 std::pair{"AddItem", std::int64_t(0)}, std::pair{"RemoveItem", std::int64_t(0)},
                 std::pair{"RemoveItem", std::int64_t(-1)},
                 std::pair{"RemoveItem", std::int64_t(-2147483648LL)}})
        {
            const std::vector<ObScript::Value> args{ObScript::ReferenceValue{weapon, {}}, count};
            EXPECT_NO_THROW(EXPECT_EQ(ObScript::asInteger(host.call(command, {}, args, context, {})), 0));
        }
        EXPECT_EQ(world.getOblivionCombatService()->findActorValues(context.mSelf), nullptr);
        EXPECT_EQ(world.getOblivionCombatService()->findActorLife(context.mSelf), nullptr);
    }

    TEST(OblivionWorldTest, NativeActorAddRejectsPhysicalCountOverflowBeforeObservers)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addNativeNpc(fixture, 0x900);
        const auto weapon = addEquipmentWeapon(fixture);
        ESM4::RuntimeInventoryItem item;
        item.mBase = weapon;
        item.mCount = std::numeric_limits<std::int32_t>::max();
        item.mCondition = 43.125f;
        item.mOwner = ESM::FormKey::content("headless.esm", 0x800);
        installEquipmentInventory(fixture, actor, {item});
        // This is explicit rejection outside the supported physical AddItem
        // domain, not a claim of native overflow-delta parity.
        EXPECT_THROW(world.oblivionAddActorItem(actor, weapon, 1), std::invalid_argument);
        EXPECT_EQ(world.oblivionAddActorItem(actor, weapon, 0), 0);
        EXPECT_EQ(world.oblivionAddActorItem({}, weapon, 2), 0);
        const auto captured = world.captureOblivionActorInventory(actor);
        ASSERT_EQ(captured.size(), 1u);
        EXPECT_EQ(captured.front().mCount, item.mCount);
        EXPECT_EQ(captured.front().mCondition, item.mCondition);
        EXPECT_EQ(captured.front().mOwner, item.mOwner);
        EXPECT_EQ(world.getOblivionCombatService()->findActorValues(actor.getCellRef().getFormKey()), nullptr);
        EXPECT_EQ(world.getOblivionCombatService()->findActorLife(actor.getCellRef().getFormKey()), nullptr);
    }

    TEST(OblivionWorldTest, NativeActorAddNegativeDeltaFailsExplicitlyWithoutAuthorityBirth)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addNativeNpc(fixture, 0x900);
        world.getWorldModel().registerPtr(actor);
        const auto weapon = addEquipmentWeapon(fixture);
        MWWorld::OblivionScriptManager host(world, world.getStore(), {"headless.esm"});
        ObScript::RuntimeContext context;
        context.mSelf = actor.getCellRef().getFormKey();
        const std::vector<ObScript::Value> args{ObScript::ReferenceValue{weapon, {}}, std::int64_t(-1)};
        EXPECT_THROW(host.call("AddItem", {}, args, context, {}), std::invalid_argument);
        EXPECT_TRUE(world.captureOblivionActorInventory(actor).empty());
        EXPECT_EQ(world.getOblivionCombatService()->findActorValues(context.mSelf), nullptr);
        EXPECT_EQ(world.getOblivionCombatService()->findActorLife(context.mSelf), nullptr);
    }

    TEST(OblivionWorldTest, NativeScriptRemoveItemNonpositiveCountPreservesPhysicalAndSavedInventory)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addNativeNpc(fixture, 0x900);
        world.getWorldModel().registerPtr(actor);
        const auto weapon = addEquipmentWeapon(fixture);
        ESM4::RuntimeInventoryItem item;
        item.mBase = weapon;
        item.mCount = 3;
        item.mCondition = 43.125f;
        item.mOwner = ESM::FormKey::content("headless.esm", 0x800);
        installEquipmentInventory(fixture, actor, {item});
        MWWorld::OblivionScriptManager host(world, world.getStore(), {"headless.esm"});
        ObScript::RuntimeContext context;
        context.mSelf = actor.getCellRef().getFormKey();
        for (const std::int64_t count : {std::int64_t(-2147483648LL), std::int64_t(-1), std::int64_t(0)})
        {
            const std::vector<ObScript::Value> args{ObScript::ReferenceValue{weapon, {}}, count};
            EXPECT_EQ(ObScript::asInteger(host.call("RemoveItem", {}, args, context, {})), 0);
            const auto captured = world.captureOblivionActorInventory(actor);
            ASSERT_EQ(captured.size(), 1u);
            EXPECT_EQ(captured.front().mCount, 3);
            EXPECT_EQ(captured.front().mCondition, 43.125f);
            EXPECT_EQ(captured.front().mOwner, item.mOwner);
            EXPECT_EQ(world.getOblivionCombatService()->findActorValues(actor.getCellRef().getFormKey()), nullptr);
            EXPECT_EQ(world.getOblivionCombatService()->findActorLife(actor.getCellRef().getFormKey()), nullptr);
        }
    }

    TEST(OblivionWorldTest, NativeScriptRemoveItemWrappedMinimumCountPreservesInventory)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addNativeNpc(fixture, 0x900);
        world.getWorldModel().registerPtr(actor);
        const auto weapon = addEquipmentWeapon(fixture);
        ESM4::RuntimeInventoryItem first;
        first.mBase = weapon;
        first.mCount = std::numeric_limits<std::int32_t>::max();
        first.mCondition = 43.125f;
        auto second = first;
        second.mCount = 1;
        second.mCondition = 99.125f;
        installEquipmentInventory(fixture, actor, {first, second});
        MWWorld::OblivionScriptManager host(world, world.getStore(), {"headless.esm"});
        ObScript::RuntimeContext context;
        context.mSelf = actor.getCellRef().getFormKey();
        const std::vector<ObScript::Value> args{ObScript::ReferenceValue{weapon, {}}};
        ASSERT_EQ(ObScript::asInteger(host.call("GetItemCount", {}, args, context, {})),
            std::numeric_limits<std::int32_t>::min());
        // Original command clamps against the signed query result before the
        // positive-count gate: a wrapped INT_MIN must never touch inventory.
        EXPECT_EQ(world.oblivionRemoveActorItem(actor, weapon, 1), 0);
        const auto captured = world.captureOblivionActorInventory(actor);
        ASSERT_EQ(captured.size(), 2u);
        EXPECT_EQ(captured[0].mCount, first.mCount);
        EXPECT_EQ(captured[0].mCondition, first.mCondition);
        EXPECT_EQ(captured[1].mCount, second.mCount);
        EXPECT_EQ(captured[1].mCondition, second.mCondition);
        EXPECT_EQ(world.getOblivionCombatService()->findActorValues(context.mSelf), nullptr);
        EXPECT_EQ(world.getOblivionCombatService()->findActorLife(context.mSelf), nullptr);
    }

    TEST(OblivionWorldTest, NativeActorRemoveAbsentItemDoesNotConstructAuthority)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addNativeNpc(fixture, 0x900);
        const auto weapon = addEquipmentWeapon(fixture);
        EXPECT_EQ(world.oblivionRemoveActorItem(actor, weapon, 7), 0);
        EXPECT_EQ(world.oblivionRemoveActorItem({}, weapon, 7), 0);
        EXPECT_TRUE(world.captureOblivionActorInventory(actor).empty());
        EXPECT_TRUE(world.captureOblivionActorInventory({}).empty());
        EXPECT_EQ(world.getOblivionCombatService()->findActorValues(actor.getCellRef().getFormKey()), nullptr);
        EXPECT_EQ(world.getOblivionCombatService()->findActorLife(actor.getCellRef().getFormKey()), nullptr);
    }

    TEST(OblivionWorldTest, NativeScriptItemCountReadsLiveStacksInsteadOfSavedInventory)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addNativeNpc(fixture, 0x900);
        world.getWorldModel().registerPtr(actor);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        const auto weapon = addEquipmentWeapon(fixture);
        ESM4::RuntimeInventoryItem first;
        first.mBase = weapon; first.mCount = 2; first.mCondition = 43.125f;
        ESM4::RuntimeInventoryItem second = first;
        second.mCount = 3; second.mCondition = 99.125f;
        second.mOwner = ESM::FormKey::content("headless.esm", 0x800);
        installEquipmentInventory(fixture, actor, {first, second});
        auto cached = captureNativeActorState(fixture, actor);
        auto stale = first; stale.mCount = 99;
        cached.mReferences.front().mInventory = {stale};
        readNativeSnapshot(fixture, cached);
        const auto authority = captureNativeActorState(fixture, actor).serializeBinary();
        MWWorld::OblivionScriptManager host(world, world.getStore(), {"headless.esm"});
        ObScript::RuntimeContext context; context.mSelf = actor.getCellRef().getFormKey();
        const std::vector<ObScript::Value> args{ObScript::ReferenceValue{weapon, {}}};
        EXPECT_EQ(ObScript::asInteger(host.call("GetItemCount", {}, args, context, {})), 5);
        const std::vector<ObScript::Value> absent{
            ObScript::ReferenceValue{ESM::FormKey::content("headless.esm", 0x999), {}}};
        EXPECT_EQ(ObScript::asInteger(host.call("GetItemCount", {}, absent, context, {})), 0);
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), authority);
        auto& inventory = actor.getClass().getInventoryStore(actor);
        ASSERT_EQ(inventory.count(ESM::RefId(ESM::FormId{0x940, 0})), 5);
        auto it = inventory.begin();
        ASSERT_NE(it, inventory.end());
        EXPECT_EQ(it->getCellRef().getNativeItemCondition(), 43.125f);
        ++it;
        ASSERT_NE(it, inventory.end());
        EXPECT_EQ(it->getCellRef().getNativeItemCondition(), 99.125f);
        EXPECT_EQ(it->getCellRef().getOwner(), ESM::RefId(ESM::FormId{0x800, 0}));
    }

    TEST(OblivionWorldTest, NativeScriptItemCountMatchesOriginalReturnBoundariesWithoutAuthorityBirth)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addNativeNpc(fixture, 0x900);
        world.getWorldModel().registerPtr(actor);
        const auto weapon = addEquipmentWeapon(fixture);
        MWWorld::OblivionScriptManager host(world, world.getStore(), {"headless.esm"});
        ObScript::RuntimeContext context; context.mSelf = actor.getCellRef().getFormKey();
        const std::vector<ObScript::Value> args{ObScript::ReferenceValue{weapon, {}}};
        // Frozen original GetItemCount rows: full evaluator/count/list path,
        // native-getitemcount-oracle-02, both x87 precision words.
        struct Row { int first; int second; std::int64_t expected; };
        constexpr int max = std::numeric_limits<std::int32_t>::max();
        const std::array rows{Row{0, 0, 0}, Row{1, 0, 1}, Row{3, 1, 4},
            Row{999, 3, 1002}, Row{max, 0, max},
            Row{max, 1, std::numeric_limits<std::int32_t>::min()},
            Row{max, 3, 2147483646}, Row{max, max, 2}};
        for (const auto& row : rows)
        {
            SCOPED_TRACE(row.first);
            SCOPED_TRACE(row.second);
            std::vector<ESM4::RuntimeInventoryItem> items;
            for (const auto& [count, condition] : std::array{std::pair{row.first, 43.125f},
                     std::pair{row.second, 99.125f}})
                if (count != 0)
                {
                    ESM4::RuntimeInventoryItem item;
                    item.mBase = weapon; item.mCount = count; item.mCondition = condition;
                    items.push_back(item);
                }
            installEquipmentInventory(fixture, actor, items);
            EXPECT_EQ(ObScript::asInteger(host.call("GetItemCount", {}, args, context, {})), row.expected);
            EXPECT_EQ(world.getOblivionCombatService()->findActorValues(context.mSelf), nullptr);
            EXPECT_EQ(world.getOblivionCombatService()->findActorLife(context.mSelf), nullptr);
        }
    }

    TEST(OblivionWorldTest, NativePlayerItemCountMatchesOriginalSignedBoundariesWithoutAuthorityBirth)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        MWClass::Npc::registerSelf();
        world.setupPlayer();
        const auto player = world.getPlayerPtr();
        const auto weapon = addEquipmentWeapon(fixture);
        MWWorld::OblivionScriptManager host(world, world.getStore(), {"headless.esm"});
        ObScript::RuntimeContext context; context.mSelf = ESM::FormKey::dynamic("player", 1);
        const std::vector<ObScript::Value> args{ObScript::ReferenceValue{weapon, {}}};
        // Frozen original 4F48F0/4869C0 rows, both x87 precision modes.
        // Physical stack composition is setup, not AddItem overflow acceptance.
        struct Row { int first; int second; std::int64_t expected; };
        constexpr int max = std::numeric_limits<std::int32_t>::max();
        const std::array rows{Row{0, 0, 0}, Row{1, 0, 1}, Row{3, 1, 4},
            Row{999, 3, 1002}, Row{max, 0, max},
            Row{max, 1, std::numeric_limits<std::int32_t>::min()},
            Row{max, 3, 2147483646}, Row{max, max, 2}};
        for (const auto& row : rows)
        {
            SCOPED_TRACE(row.first);
            SCOPED_TRACE(row.second);
            std::vector<ESM4::RuntimeInventoryItem> items;
            for (const auto& [count, condition] : std::array{std::pair{row.first, 43.125f},
                     std::pair{row.second, 99.125f}})
                if (count != 0)
                {
                    ESM4::RuntimeInventoryItem item;
                    item.mBase = weapon; item.mCount = count; item.mCondition = condition;
                    item.mOwner = ESM::FormKey::content("headless.esm", 0x800);
                    items.push_back(item);
                }
            installEquipmentInventory(fixture, player, items);
            EXPECT_EQ(world.oblivionPlayerItemCount(weapon), row.expected);
            EXPECT_EQ(ObScript::asInteger(host.call("GetItemCount", {}, args, context, {})), row.expected);
            auto& inventory = player.getClass().getInventoryStore(player);
            std::size_t i = 0;
            for (const auto& entry : inventory)
            {
                ASSERT_LT(i, items.size());
                EXPECT_EQ(entry.getCellRef().getCount(false), items[i].mCount);
                EXPECT_EQ(entry.getCellRef().getNativeItemCondition(), items[i].mCondition);
                EXPECT_EQ(entry.getCellRef().getOwner(), ESM::RefId(ESM::FormId{0x800, 0}));
                ++i;
            }
            EXPECT_EQ(i, items.size());
            EXPECT_EQ(world.getOblivionCombatService()->findActorValues(context.mSelf), nullptr);
            EXPECT_EQ(world.getOblivionCombatService()->findActorLife(context.mSelf), nullptr);
        }
        EXPECT_EQ(world.oblivionPlayerItemCount(ESM::FormKey{}), 0);
        EXPECT_EQ(world.oblivionPlayerItemCount(ESM::FormKey::content("headless.esm", 0x999)), 0);
        EXPECT_EQ(world.oblivionPlayerItemCount(ESM::FormKey::content("unavailable.esm", 0x940)), 0);
    }

    TEST(OblivionWorldTest, NativeScriptItemCountUsesCreatureInventoryWithoutAuthorityBirth)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addEquipmentCreature(fixture);
        const auto weapon = addEquipmentWeapon(fixture);
        ESM4::RuntimeInventoryItem item;
        item.mBase = weapon; item.mCount = 4; item.mCondition = 67.125f;
        installEquipmentInventory(fixture, actor, {item});
        MWWorld::OblivionScriptManager host(world, world.getStore(), {"headless.esm"});
        ObScript::RuntimeContext context; context.mSelf = actor.getCellRef().getFormKey();
        const std::vector<ObScript::Value> args{ObScript::ReferenceValue{weapon, {}}};
        EXPECT_EQ(ObScript::asInteger(host.call("GetItemCount", {}, args, context, {})), 4);
        auto& inventory = actor.getClass().getContainerStore(actor);
        const auto entry = inventory.begin();
        ASSERT_NE(entry, inventory.end());
        EXPECT_EQ(entry->getCellRef().getCount(), 4);
        EXPECT_EQ(entry->getCellRef().getNativeItemCondition(), 67.125f);
        EXPECT_EQ(world.getOblivionCombatService()->findActorValues(context.mSelf), nullptr);
        EXPECT_EQ(world.getOblivionCombatService()->findActorLife(context.mSelf), nullptr);
    }

    TEST(OblivionWorldTest, NativeWorldInventoryCapturePreservesPlayerAndActorOwnerKeys)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        MWClass::Npc::registerSelf();
        world.setupPlayer();
        auto& store = world.getStore();
        ESM::Race race{}; race.blank(); race.mId = ESM::RefId(ESM::FormId{0x810, 0});
        store.getWritable<ESM::Race>().insertStatic(race);
        ESM4::Cell cell{}; cell.mId = ESM::RefId(ESM::FormId{1, 0});
        cell.mFormKey = ESM::FormKey::content("headless.esm", 1);
        cell.mCellFlags = ESM4::CELL_Interior; cell.mEditorId = "NativeOwnerCaptureCell";
        store.getWritable<ESM4::Cell>().insertStatic(cell, cell.mFormKey);
        // setupPlayer's fallback uses a string race. Give this capture fixture
        // a managed projected base with a native race key, as a real loaded
        // Oblivion player has; save validation must remain enabled.
        auto playerBase = *world.getPlayerPtr().get<ESM::NPC>()->mBase;
        playerBase.mId = ESM::RefId::stringRefId("NativeOwnerCapturePlayer");
        playerBase.mRace = race.mId;
        store.insertStatic(playerBase);
        world.getPlayerPtr().get<ESM::NPC>()->mBase = store.get<ESM::NPC>().find(playerBase.mId);
        auto& residentCell = world.getWorldModel().getCell(cell.mId);
        world.getPlayer().setCell(&residentCell);
        const auto draft = addNativeNpc(fixture, 0x900);
        const auto actor = draft.getCell()->moveTo(draft, &residentCell);
        world.getWorldModel().registerPtr(actor);
        ESM4::RuntimeActorValues values;
        values.mActor = ESM::FormKey::dynamic("player", 1);
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mPlayerFormValues = {{10, 0, 0, 0}};
        for (std::size_t i = 0; i < 8; ++i) values.mValues[i].mBase = 50;
        const auto settings = MWWorld::resolveOblivionPlayerDynamicBaseSettings(store);
        auto& combat = *world.getOblivionCombatService();
        combat.publishPlayerValues(world.getPlayer(), values, settings);
        ESM4::RuntimeActorLife life;
        life.mActor = values.mActor; life.mBase = values.mBase;
        combat.publishPlayerLife(world.getPlayer(), life);
        const auto weapon = addEquipmentWeapon(fixture);
        const auto owner = ESM::FormKey::content("headless.esm", 0x800);
        ESM4::RuntimeInventoryItem item;
        item.mBase = weapon; item.mCount = 2; item.mCondition = 51.125f; item.mOwner = owner;
        installEquipmentInventory(fixture, world.getPlayerPtr(), {item});
        item.mCount = 3; item.mCondition = 83.125f;
        installEquipmentInventory(fixture, actor, {item});
        const auto captured = world.captureOblivionRuntimeState();
        ASSERT_EQ(captured.mPlayer.mInventory.size(), 1);
        EXPECT_EQ(captured.mPlayer.mInventory[0].mOwner, owner);
        EXPECT_EQ(captured.mPlayer.mInventory[0].mCount, 2);
        EXPECT_EQ(captured.mPlayer.mInventory[0].mCondition, 51.125);
        ASSERT_EQ(captured.mReferences.size(), 1);
        ASSERT_EQ(captured.mReferences[0].mInventory.size(), 1);
        EXPECT_EQ(captured.mReferences[0].mInventory[0].mOwner, owner);
        EXPECT_EQ(captured.mReferences[0].mInventory[0].mCount, 3);
        EXPECT_EQ(captured.mReferences[0].mInventory[0].mCondition, 83.125);
        EXPECT_EQ(combat.findActorValues(actor.getCellRef().getFormKey()), nullptr);
        EXPECT_EQ(combat.findActorLife(actor.getCellRef().getFormKey()), nullptr);
        const auto decoded = ESM4::RuntimeState::deserializeBinary(captured.serializeBinary());
        EXPECT_EQ(decoded.mPlayer.mInventory[0].mOwner, owner);
        EXPECT_EQ(decoded.mReferences[0].mInventory[0].mOwner, owner);
    }

    TEST(OblivionWorldTest, NativeScriptEquipmentUsesLiveInstancesAndPreservesConditionOwnership)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addNativeNpc(fixture, 0x900);
        world.getWorldModel().registerPtr(actor);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        const auto weapon = addEquipmentWeapon(fixture);
        ESM4::RuntimeInventoryItem item; item.mBase = weapon; item.mCount = 1;
        item.mCondition = std::bit_cast<float>(1113509069u); item.mCharge = 7.25f;
        item.mOwner = ESM::FormKey::content("headless.esm", 0x800);
        item.mEquippedSlots = ESM4::InventorySlotWeapon;
        installEquipmentInventory(fixture, actor, {item});
        auto& inventory = actor.getClass().getInventoryStore(actor);
        const auto original = *inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
        const auto baseline = captureNativeActorState(fixture, actor).serializeBinary();
        auto cached = captureNativeActorState(fixture, actor);
        cached.mReferences.front().mInventory = {item};
        readNativeSnapshot(fixture, cached);
        MWWorld::OblivionScriptManager host(world, world.getStore(), {"headless.esm"});
        ObScript::RuntimeContext context; context.mSelf = actor.getCellRef().getFormKey();
        const std::vector<ObScript::Value> args{ObScript::ReferenceValue{weapon, {}}};
        EXPECT_EQ(ObScript::asInteger(host.call("GetEquipped", {}, args, context, {})), 1);
        EXPECT_EQ(ObScript::asInteger(host.call("UnequipItem", {}, args, context, {})), 1);
        EXPECT_EQ(inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight), inventory.end());
        EXPECT_EQ(ObScript::asInteger(host.call("GetEquipped", {}, args, context, {})), 0);
        // A stale, structurally valid saved mask must not drive either query
        // or the renderer's equipment view for this already-live inventory.
        readNativeSnapshot(fixture, cached);
        EXPECT_EQ(ObScript::asInteger(host.call("GetEquipped", {}, args, context, {})), 0);
        const auto empty = world.oblivionReferenceEquipment(actor);
        ASSERT_TRUE(empty); EXPECT_TRUE(empty->empty());
        EXPECT_EQ(ObScript::asInteger(host.call("EquipItem", {}, args, context, {})), 1);
        ASSERT_NE(inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight), inventory.end());
        const auto equipped = *inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
        EXPECT_EQ(equipped, original);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(*equipped.getCellRef().getNativeItemCondition()), 1113509069u);
        EXPECT_EQ(equipped.getCellRef().getEnchantmentCharge(), 7.25f);
        EXPECT_EQ(equipped.getCellRef().getOwner(), ESM::RefId(ESM::FormId{0x800, 0}));
        EXPECT_EQ(equipped.getCellRef().getCount(), 1);
        EXPECT_EQ(ObScript::asInteger(host.call("GetEquipped", {}, args, context, {})), 1);
        const auto visible = world.oblivionReferenceEquipment(actor);
        ASSERT_TRUE(visible); ASSERT_EQ(visible->size(), 1u);
        EXPECT_EQ(visible->front().second, ESM4::InventorySlotWeapon);
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), baseline);
        EXPECT_FALSE(world.oblivionEquipActorItem(actor, ESM::FormKey::content("headless.esm", 0x999), true));
        EXPECT_EQ(*inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight), original);
        MWWorld::World foreign(nullptr, -1, "", {}, ESM::GameProfile::Morrowind);
        EXPECT_FALSE(foreign.oblivionEquipActorItem(actor, weapon, false));
        EXPECT_FALSE(foreign.oblivionActorItemEquipped(actor, weapon));
        EXPECT_FALSE(foreign.oblivionReferenceEquipment(actor));
        EXPECT_EQ(*inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight), original);
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), baseline);
    }

    TEST(OblivionWorldTest, NativeEquipmentCancelsOwnedWeaponActionBeforeEquipmentCallbacks)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addNativeNpc(fixture, 0x900), peer = addNativeNpc(fixture, 0x901);
        ASSERT_TRUE(world.activateOblivionActor(actor)); ASSERT_TRUE(world.activateOblivionActor(peer));
        const auto weapon = addEquipmentWeapon(fixture);
        ESM4::RuntimeInventoryItem item; item.mBase = weapon; item.mCount = 1;
        item.mCondition = 99.125; item.mEquippedSlots = ESM4::InventorySlotWeapon;
        installEquipmentInventory(fixture, actor, {item});
        auto& inventory = actor.getClass().getInventoryStore(actor);
        auto& service = *world.getOblivionCombatService();
        const auto owner = actor.getCellRef().getFormKey(), other = peer.getCellRef().getFormKey();
        const auto values = *service.findActorValues(owner); const auto rng = service.combatRandomState();
        ESM4::RuntimeMeleeInput input; input.mInputHeld = true;
        service.setMeleeInput(owner, input);
        const auto id = service.beginMeleeStrike(owner, ESM4::MeleeStrikeKind::Left, "onehandattackleft", 1, weapon);
        const auto peerId = service.beginMeleeStrike(other, ESM4::MeleeStrikeKind::Right, "handtohandattackright");
        struct Observer final : MWWorld::InventoryStoreListener
        {
            MWMechanics::OblivionCombatService& service;
            ESM::FormKey owner; std::uint64_t id; int calls = 0;
            Observer(MWMechanics::OblivionCombatService& s, ESM::FormKey key, std::uint64_t action)
                : service(s), owner(std::move(key)), id(action) {}
            void equipmentChanged() override
            {
                ++calls;
                EXPECT_FALSE(service.isActionPending(id, owner));
                const auto* state = service.findMeleeState(owner);
                ASSERT_NE(state, nullptr); EXPECT_FALSE(state->mStrike);
                EXPECT_FALSE(state->mInput.mInputHeld); EXPECT_EQ(state->mInput.mQueued, ESM4::MeleeQueuedStrike::None);
            }
        } observer(service, owner, id);
        inventory.setInvListener(&observer);
        EXPECT_TRUE(world.oblivionEquipActorItem(actor, weapon, true)); // Same instance is a no-op.
        EXPECT_EQ(observer.calls, 0); EXPECT_TRUE(service.isActionPending(id, owner));
        EXPECT_FALSE(world.oblivionEquipActorItem(actor, ESM::FormKey::content("headless.esm", 0x999), false));
        EXPECT_TRUE(service.isActionPending(id, owner));
        EXPECT_TRUE(world.oblivionEquipActorItem(actor, weapon, false));
        EXPECT_EQ(observer.calls, 1); EXPECT_TRUE(service.isActionPending(peerId, other));
        EXPECT_EQ(service.combatRandomState(), rng);
        EXPECT_EQ(*service.findActorValues(owner), values);
        EXPECT_EQ(inventory.begin()->getCellRef().getNativeItemCondition(), std::optional<float>(99.125f));
        inventory.setInvListener(nullptr);
    }

    TEST(OblivionWorldTest, NativeEquipmentReentrantEquipDoesNotRepeatUnequipOrReplayOwnedAction)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addNativeNpc(fixture, 0x900);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        const auto weapon = addEquipmentWeapon(fixture);
        ESM4::RuntimeInventoryItem item; item.mBase = weapon; item.mCount = 1;
        item.mCondition = 99.125; item.mEquippedSlots = ESM4::InventorySlotWeapon;
        installEquipmentInventory(fixture, actor, {item});
        auto& inventory = actor.getClass().getInventoryStore(actor);
        const MWWorld::Ptr physical = *inventory.begin();
        auto& service = *world.getOblivionCombatService();
        const auto owner = actor.getCellRef().getFormKey();
        const auto values = *service.findActorValues(owner);
        const auto rng = service.combatRandomState();
        const auto id = service.beginMeleeStrike(owner, ESM4::MeleeStrikeKind::Left, "onehandattackleft", 1, weapon);
        struct Observer final : MWWorld::InventoryStoreListener
        {
            MWWorld::World& world; MWWorld::Ptr actor; ESM::FormKey weapon;
            MWMechanics::OblivionCombatService& service; ESM::FormKey owner; std::uint64_t id;
            int calls = 0;
            Observer(MWWorld::World& w, MWWorld::Ptr ptr, ESM::FormKey item,
                MWMechanics::OblivionCombatService& s, ESM::FormKey key, std::uint64_t action)
                : world(w), actor(ptr), weapon(std::move(item)), service(s), owner(std::move(key)), id(action) {}
            void equipmentChanged() override
            {
                ++calls;
                // Bound the old implementation's repeated callbacks so the
                // regression fails promptly instead of hanging the test suite.
                if (calls > 2)
                    throw std::runtime_error("native unequip repeated a reentrant equipment request");
                EXPECT_FALSE(service.isActionPending(id, owner));
                if (!world.oblivionActorItemEquipped(actor, weapon))
                {
                    EXPECT_TRUE(world.oblivionEquipActorItem(actor, weapon, true));
                }
            }
        } observer(world, actor, weapon, service, owner, id);
        struct ListenerGuard
        {
            MWWorld::InventoryStore& inventory;
            ~ListenerGuard() { inventory.setInvListener(nullptr); }
        } guard{inventory};
        inventory.setInvListener(&observer);
        EXPECT_TRUE(world.oblivionEquipActorItem(actor, weapon, false));
        EXPECT_EQ(observer.calls, 2);
        EXPECT_TRUE(world.oblivionActorItemEquipped(actor, weapon)); // The observer's later request wins.
        EXPECT_EQ(*inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight), physical);
        EXPECT_EQ(physical.getCellRef().getNativeItemCondition(), std::optional<float>(99.125f));
        EXPECT_EQ(physical.getCellRef().getCount(), 1);
        EXPECT_FALSE(service.isActionPending(id, owner));
        EXPECT_EQ(*service.findActorValues(owner), values);
        EXPECT_EQ(service.combatRandomState(), rng);
        inventory.setInvListener(nullptr);
    }

    TEST(OblivionWorldTest, NativeEquipmentRingCopiesAndApparelOverlapConserveItems)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld; auto& store = world.getStore();
        const auto actor = addNativeNpc(fixture, 0x900);
        world.getWorldModel().registerPtr(actor);
        MWClass::Clothing::registerSelf();
        const auto add = [&](std::uint32_t id, int type, std::uint32_t masks) {
            ESM4::Clothing native{}; native.mId = {id, 0}; native.mClothingFlags = masks;
            const auto key = ESM::FormKey::content("headless.esm", id);
            store.getWritable<ESM4::Clothing>().insertStatic(native, key);
            ESM::Clothing projected; projected.blank(); projected.mId = ESM::RefId(native.mId);
            projected.mData.mType = type; store.insertStatic(projected); return key;
        };
        const auto ring = add(0x940, ESM::Clothing::Ring, ESM4::Armor::TES4_RightRing | ESM4::Armor::TES4_LeftRing);
        const auto robe = add(0x941, ESM::Clothing::Robe, ESM4::Armor::TES4_UpperBody | ESM4::Armor::TES4_LowerBody);
        const auto shirt = add(0x942, ESM::Clothing::Shirt, ESM4::Armor::TES4_UpperBody);
        // Two existing physical copies: splitting a count2 stack calls the
        // real GUI inventory-update path, which this headless fixture lacks.
        // Real stacked script equipment remains a rendered acceptance case.
        ESM4::RuntimeInventoryItem a; a.mBase = ring; a.mCount = 1;
        ESM4::RuntimeInventoryItem b; b.mBase = robe; b.mCount = 1;
        ESM4::RuntimeInventoryItem c; c.mBase = shirt; c.mCount = 1;
        installEquipmentInventory(fixture, actor, {a,a,b,c});
        auto& inventory = actor.getClass().getInventoryStore(actor);
        ASSERT_TRUE(world.oblivionEquipActorItem(actor, ring, true));
        ASSERT_TRUE(world.oblivionEquipActorItem(actor, ring, true));
        auto right = inventory.getSlot(MWWorld::InventoryStore::Slot_RightRing);
        auto left = inventory.getSlot(MWWorld::InventoryStore::Slot_LeftRing);
        ASSERT_NE(right, inventory.end()); ASSERT_NE(left, inventory.end()); EXPECT_NE(*right, *left);
        EXPECT_EQ(right->getCellRef().getCount(), 1); EXPECT_EQ(left->getCellRef().getCount(), 1);
        EXPECT_EQ(inventory.count(ESM::RefId(ESM::FormId{0x940, 0})), 2);
        ASSERT_TRUE(world.oblivionEquipActorItem(actor, robe, true));
        ASSERT_TRUE(world.oblivionEquipActorItem(actor, shirt, true));
        const auto gear = world.oblivionReferenceEquipment(actor); ASSERT_TRUE(gear);
        EXPECT_EQ(gear->size(), 3u); // Two rings plus shirt; robe conflicts across its whole native mask.
        EXPECT_FALSE(world.oblivionActorItemEquipped(actor, robe));
        EXPECT_TRUE(world.oblivionActorItemEquipped(actor, shirt));
        EXPECT_TRUE(world.oblivionEquipActorItem(actor, ring, false));
        EXPECT_FALSE(world.oblivionActorItemEquipped(actor, ring));
        EXPECT_EQ(inventory.count(ESM::RefId(ESM::FormId{0x940, 0})), 2);
        EXPECT_FALSE(world.oblivionEquipActorItem(actor, ring, false));
        EXPECT_EQ(world.getOblivionCombatService()->findActorValues(actor.getCellRef().getFormKey()), nullptr);
    }

    TEST(OblivionWorldTest, NativeCreatureEquipmentDoesNotCreateResourceAuthority)
    {
        NativeWorldFixture fixture; auto& world = fixture.mWorld;
        const auto actor = addEquipmentCreature(fixture);
        const auto weapon = addEquipmentWeapon(fixture);
        ESM4::RuntimeInventoryItem item; item.mBase = weapon; item.mCount = 1; item.mCondition = 43.125;
        installEquipmentInventory(fixture, actor, {item});
        ASSERT_TRUE(world.oblivionEquipActorItem(actor, weapon, true));
        EXPECT_TRUE(world.oblivionActorItemEquipped(actor, weapon));
        ASSERT_TRUE(world.oblivionReferenceEquipment(actor));
        EXPECT_EQ(world.getOblivionCombatService()->findActorValues(actor.getCellRef().getFormKey()), nullptr);
        EXPECT_FALSE(world.captureOblivionActorDrawState(actor));
        EXPECT_TRUE(world.oblivionEquipActorItem(actor, weapon, false));
        EXPECT_FALSE(world.oblivionActorItemEquipped(actor, weapon));
        EXPECT_EQ(world.getOblivionCombatService()->findActorLife(actor.getCellRef().getFormKey()), nullptr);
    }

    TEST(OblivionWorldTest, NativeNpcDrawRestoresThroughRecordAndLazyClassWithoutCombatDeltas)
    {
        NativeWorldFixture fixture;
        const auto actor = addNativeNpc(fixture, 0x900);
        auto& world = fixture.mWorld;
        EXPECT_FALSE(world.captureOblivionActorDrawState(actor));
        EXPECT_EQ(actor.getRefData().getCustomData(), nullptr);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        const auto baseline = captureNativeActorState(fixture, actor).serializeBinary();
        const std::array states{MWMechanics::DrawState::Nothing, MWMechanics::DrawState::Weapon,
            MWMechanics::DrawState::Spell};
        for (const auto draw : states)
        {
            SCOPED_TRACE(static_cast<int>(draw));
            actor.getClass().getCreatureStats(actor).setDrawState(draw);
            auto saved = captureNativeActorState(fixture, actor);
            saved.mReferences.front().mActorDrawState = world.captureOblivionActorDrawState(actor);
            ASSERT_TRUE(saved.mReferences.front().mActorDrawState);
            EXPECT_EQ(static_cast<int>(*saved.mReferences.front().mActorDrawState), static_cast<int>(draw));
            readNativeSnapshot(fixture, saved);
            const auto opposite = draw == MWMechanics::DrawState::Weapon
                ? MWMechanics::DrawState::Nothing : MWMechanics::DrawState::Weapon;
            actor.getClass().getCreatureStats(actor).setDrawState(opposite);
            ASSERT_TRUE(world.restoreOblivionActorDrawState(actor));
            EXPECT_EQ(actor.getClass().getCreatureStats(actor).getDrawState(), draw);
            actor.getRefData().setCustomData(nullptr);
            EXPECT_EQ(actor.getClass().getCreatureStats(actor).getDrawState(), draw);
            EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), baseline);
        }
        auto legacy = captureNativeActorState(fixture, actor);
        legacy.mVersion = 28;
        readNativeSnapshot(fixture, legacy);
        actor.getClass().getCreatureStats(actor).setDrawState(MWMechanics::DrawState::Weapon);
        EXPECT_FALSE(world.restoreOblivionActorDrawState(actor));
        EXPECT_EQ(actor.getClass().getCreatureStats(actor).getDrawState(), MWMechanics::DrawState::Weapon);
        actor.getRefData().setCustomData(nullptr);
        EXPECT_EQ(actor.getClass().getCreatureStats(actor).getDrawState(), MWMechanics::DrawState::Nothing);
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), baseline);
    }

    TEST(OblivionWorldTest, NativeCreatureDrawRestoresThroughRecordAndLazyClassWithoutCombatDeltas)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
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
        const MWWorld::Ptr actor(cell.insert(&live), &cell);
        EXPECT_FALSE(world.captureOblivionActorDrawState(actor));
        EXPECT_EQ(actor.getRefData().getCustomData(), nullptr);
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(actor, ESM4::ActorValueProcess::Active));
        const auto baseline = captureNativeActorState(fixture, actor).serializeBinary();
        for (const auto draw : {MWMechanics::DrawState::Nothing, MWMechanics::DrawState::Weapon,
                 MWMechanics::DrawState::Spell})
        {
            actor.getClass().getCreatureStats(actor).setDrawState(draw);
            auto saved = captureNativeActorState(fixture, actor);
            saved.mReferences.front().mActorDrawState = world.captureOblivionActorDrawState(actor);
            readNativeSnapshot(fixture, saved);
            actor.getRefData().setCustomData(nullptr);
            EXPECT_EQ(actor.getClass().getCreatureStats(actor).getDrawState(), draw);
            EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), baseline);
        }
    }

    TEST(OblivionWorldTest, NativeDrawRejectsWrongLiveBaseAndWinningKindBeforeBuildingAView)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
        const auto actor = addNativeNpc(fixture, 0x900);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        const auto baseline = captureNativeActorState(fixture, actor).serializeBinary();
        auto saved = captureNativeActorState(fixture, actor);
        saved.mReferences.front().mActorDrawState = ESM4::ActorDrawState::Weapon;
        readNativeSnapshot(fixture, saved);
        auto otherBase = *store.search<ESM4::Npc>(saved.mReferences.front().mBase);
        otherBase.mId = {0x821, 0};
        otherBase.mFormKey = ESM::FormKey::content("headless.esm", 0x821);
        store.getWritable<ESM4::Npc>().insertStatic(otherBase, otherBase.mFormKey);
        const auto* reference = store.search<ESM4::ActorCharacter>(actor.getCellRef().getFormKey());
        ASSERT_NE(reference, nullptr);
        MWWorld::LiveCellRef<ESM4::Npc> wrongLive(*reference, store.search<ESM4::Npc>(otherBase.mFormKey));
        const MWWorld::Ptr wrong(&wrongLive, actor.getCell());
        EXPECT_THROW(world.oblivionSavedActorDrawState(wrong), std::invalid_argument);
        EXPECT_THROW(world.captureOblivionActorDrawState(wrong), std::invalid_argument);
        EXPECT_EQ(wrong.getRefData().getCustomData(), nullptr);
        ESM4::Creature conflicting{};
        conflicting.mId = {0x800, 0};
        conflicting.mFormKey = saved.mReferences.front().mBase;
        conflicting.mAttackReach = 64;
        store.getWritable<ESM4::Creature>().insertStatic(conflicting, conflicting.mFormKey);
        EXPECT_THROW(world.oblivionSavedActorDrawState(actor), std::invalid_argument);
        EXPECT_THROW(world.captureOblivionActorDrawState(actor), std::invalid_argument);
        EXPECT_EQ(actor.getClass().getCreatureStats(actor).getDrawState(), MWMechanics::DrawState::Nothing);
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), baseline);
    }

    TEST(OblivionWorldTest, NativeNpcActivationDispatchesScriptBeforeDefaultAndPreservesReceiverOnRestore)
    {
        NativeWorldFixture fixture(false, false, true);
        const auto target = addNativeNpc(fixture, 0x900);
        const auto activator = addNativeNpc(fixture, 0x901);
        auto& world = fixture.mWorld;
        world.getWorldModel().registerPtr(target);
        world.getWorldModel().registerPtr(activator);
        ASSERT_TRUE(world.activateOblivionActor(target));
        ASSERT_TRUE(world.activateOblivionActor(activator));
        auto* host = world.getOblivionScriptManager();
        ASSERT_NE(host, nullptr);
        const auto capture = [&] {
            auto state = captureNativeActorState(fixture, target);
            state.mReferences.push_back(captureNativeActorState(fixture, activator).mReferences.front());
            return state.serializeBinary();
        };
        const auto baseline = capture();
        const auto execute = [&] {
            auto action = target.getClass().activate(target, activator);
            ASSERT_NE(dynamic_cast<MWWorld::OblivionInteractionAction*>(action.get()), nullptr);
            action->execute(activator, true);
        };
        execute();
        execute();
        ESM4::RuntimeState state;
        host->capture(state);
        ASSERT_EQ(state.mScriptInstances.size(), 1u);
        const auto& instance = state.mScriptInstances.front();
        EXPECT_EQ(instance.mContext, target.getCellRef().getFormKey());
        ASSERT_EQ(instance.mLocals.size(), 1u);
        EXPECT_EQ(std::get<std::int64_t>(instance.mLocals.front()), 2);
        EXPECT_EQ(capture(), baseline);
        EXPECT_FALSE(world.isOblivionDefaultActivation());
        host->restore(state);
        execute();
        ESM4::RuntimeState after;
        host->capture(after);
        ASSERT_EQ(after.mScriptInstances.size(), 1u);
        EXPECT_EQ(std::get<std::int64_t>(after.mScriptInstances.front().mLocals.front()), 3);
        EXPECT_EQ(capture(), baseline);
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
    }    TEST(OblivionWorldTest, nativeCharacterChoicesCommitMetadataAuthorityAndPassiveReplacementTogether)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
        MWClass::Npc::registerSelf();
        world.setupPlayer();
        const auto actor = ESM::FormKey::dynamic("player", 1);
        auto& service = *world.getOblivionCombatService();
        const auto beforeMetadata = *world.getPlayerPtr().get<ESM::NPC>()->mBase;
        ESM4::Npc native{}; native.mId = {7, 0}; native.mIsTES4 = true;
        const auto playerBase = ESM::FormKey::content("Oblivion.esm", 7);
        native.mFormKey = playerBase; native.mData.attribs = {40, 40, 40, 40, 40, 40, 40, 40};
        store.getWritable<ESM4::Npc>().insertStatic(native, playerBase);
        ESM4::Race race{}; race.mId = {0x810, 0};
        race.mAttribMale = {40, 40, 40, 40, 40, 40, 40, 40};
        race.mAttribFemale = {30, 30, 30, 30, 30, 30, 30, 30};
        race.mTES4SkillBonuses = std::array<ESM4::Race::SkillBonus, 7>{};
        for (auto& bonus : *race.mTES4SkillBonuses) bonus.mSkill = -1;
        const auto raceKey = ESM::FormKey::content("headless.esm", 0x810);
        store.getWritable<ESM4::Race>().insertStatic(race, raceKey);
        ESM4::Class characterClass{}; characterClass.mId = {0x811, 0};
        characterClass.mData.mFavoredAttributes = {0, 5};
        characterClass.mData.mMajorSkills = {12, 13, 14, 15, 16, 17, 18};
        characterClass.mData.mSpecialization = 0;
        store.getWritable<ESM4::Class>().insertStatic(characterClass, ESM::FormKey::content("headless.esm", 0x811));
        for (unsigned i = 0; i < 21; ++i)
        {
            ESM4::Skill skill{}; skill.mId = {0x1000 + i, 0}; skill.mIndex = i + 12;
            skill.mData = ESM4::SkillData{i + 12, 0, i % 3, {1, 2}};
            store.getWritable<ESM4::Skill>().insertStatic(skill, ESM::FormKey::content("headless.esm", 0x1000 + i));
        }
        auto proposed = beforeMetadata; proposed.mRace = race.mId; proposed.mClass = characterClass.mId;
        proposed.mNpdt.mLevel = 1; proposed.setIsMale(true);
        auto invalid = proposed; invalid.mRace = ESM::RefId::stringRefId("missing-race");
        EXPECT_THROW(world.replaceOblivionPlayerCharacter(invalid, {}, nullptr, 2), std::invalid_argument);
        EXPECT_THROW(world.replaceOblivionPlayerCharacter(proposed, {}, nullptr, 0x80), std::invalid_argument);
        EXPECT_EQ(world.getPlayerPtr().get<ESM::NPC>()->mBase->mRace, beforeMetadata.mRace);
        EXPECT_EQ(world.getPlayer().getOblivionCharacterGenerationFlags(), 0);
        EXPECT_EQ(service.findActorValues(actor), nullptr);
        EXPECT_EQ(service.findActorLife(actor), nullptr);
        ASSERT_TRUE(world.replaceOblivionPlayerCharacter(proposed, {}, nullptr, 2));
        EXPECT_EQ(world.getPlayerPtr().get<ESM::NPC>()->mBase->mRace, proposed.mRace);
        EXPECT_EQ(world.getPlayer().getOblivionCharacterGenerationFlags(), 2);
        EXPECT_EQ(service.getPlayerValue(8), 90);
        EXPECT_EQ(service.getPlayerValue(9), 60);
        EXPECT_EQ(service.getPlayerValue(10), 170);
        ASSERT_TRUE(world.requestOblivionResourceCurrent(world.getPlayerPtr(), 8, 80));
        ESM4::EffectSetting effect{}; effect.mId = {0x812, 0}; effect.mEffectCode = ESM::fourCC("FOAT");
        effect.mFullName = "Fortify Attribute"; effect.mData.emplace(); effect.mData->mSchool = 2;
        effect.preparePassiveValueModifierDefinition();
        store.getWritable<ESM4::EffectSetting>().insertStatic(effect, ESM::FormKey::content("headless.esm", 0x812));
        ESM4::Spell spell{}; spell.mId = {0x813, 0}; spell.mData = ESM4::SpellData{4, 0, 0, 0, {}};
        spell.mEffects = {{ESM::fourCC("FOAT"), 25, 0, 0, 0, 5, {}}};
        const auto spellKey = ESM::FormKey::content("headless.esm", 0x813);
        store.getWritable<ESM4::Spell>().insertStatic(spell, spellKey);
        ESM4::BirthSign sign{}; sign.mId = {0x814, 0}; sign.mSpells = {spell.mId};
        store.getWritable<ESM4::BirthSign>().insertStatic(sign, ESM::FormKey::content("headless.esm", 0x814));
        ASSERT_TRUE(world.replaceOblivionPlayerCharacter(proposed, sign.mId, nullptr, 8));
        EXPECT_EQ(world.getPlayer().getBirthSign(), ESM::RefId(sign.mId));
        EXPECT_EQ(world.getPlayer().getOblivionCharacterGenerationFlags(), 10);
        EXPECT_EQ(service.getPlayerValue(5), 70);
        EXPECT_EQ(service.getPlayerValue(8), 130); // Existing Health damage survives.
        EXPECT_EQ(service.getPlayerValue(10), 195);
        ASSERT_EQ(service.findActorValues(actor)->mPassiveAbilities->size(), 1);
        const auto before = *service.findActorValues(actor);
        const auto* record = world.getPlayerPtr().get<ESM::NPC>()->mBase;
        EXPECT_THROW(world.replaceOblivionPlayerCharacter(proposed,
            ESM::RefId::stringRefId("missing-sign"), nullptr, 4), std::invalid_argument);
        EXPECT_EQ(*service.findActorValues(actor), before);
        EXPECT_EQ(world.getPlayerPtr().get<ESM::NPC>()->mBase, record);
        EXPECT_EQ(world.getPlayer().getBirthSign(), ESM::RefId(sign.mId));
        EXPECT_EQ(world.getPlayer().getOblivionCharacterGenerationFlags(), 10);
        ASSERT_TRUE(world.replaceOblivionPlayerCharacter(proposed, sign.mId, nullptr, 0));
        EXPECT_EQ(*service.findActorValues(actor), before); // No duplicated grant.
        ASSERT_TRUE(world.replaceOblivionPlayerCharacter(proposed, {}, nullptr, 0));
        EXPECT_EQ(service.getPlayerValue(5), 45);
        EXPECT_EQ(service.getPlayerValue(8), 80);
        EXPECT_TRUE(service.findActorValues(actor)->mPassiveAbilities->empty());
        proposed.setIsMale(false);
        ASSERT_TRUE(world.replaceOblivionPlayerCharacter(proposed, {}, nullptr, 0));
        EXPECT_EQ(service.getPlayerValue(8), 60);
        EXPECT_EQ(service.getPlayerValue(9), 45);
        EXPECT_EQ(service.getPlayerValue(10), 130);
        EXPECT_FALSE(world.getPlayerPtr().get<ESM::NPC>()->mBase->isMale());
        proposed.setIsMale(true);
        ASSERT_TRUE(world.replaceOblivionPlayerCharacter(proposed, {}, nullptr, 0));
        EXPECT_EQ(service.getPlayerValue(8), 80);
        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = actor;
        saved.mPlayer.mCell = ESM::FormKey::content("headless.esm", 1);
        saved.mPlayer.mRace = raceKey;
        saved.mPlayer.mClass = ESM::FormKey::content("headless.esm", 0x811);
        service.capture(saved);
        const auto bytes = saved.serializeBinary();
        service.clear();
        service.restore(ESM4::RuntimeState::deserializeBinary(bytes), store);
        ASSERT_TRUE(world.initializeOblivionPlayerActor());
        EXPECT_EQ(service.getPlayerValue(8), 80);
        ASSERT_TRUE(world.replaceOblivionPlayerCharacter(proposed, {}, nullptr, 0));
        EXPECT_EQ(*service.findActorValues(actor), saved.mNativeActorValues.front());
        ESM::Class custom{}; custom.blank();
        custom.mData.mAttribute = {ESM::Attribute::Strength, ESM::Attribute::Endurance};
        custom.mData.mSpecialization = 0;
        const auto& skills = MWWorld::oblivionSkillIds();
        for (unsigned i = 0; i < 5; ++i) custom.mData.mSkills[i][1] = skills[i];
        custom.mData.mSkills[0][0] = skills[5]; custom.mData.mSkills[1][0] = skills[6];
        ESM::RefId nextClass;
        {
            auto preparation = store.preparePlayerRecord(proposed, &custom);
            nextClass = preparation.customClass()->mId;
        }
        auto malformed = custom; malformed.mData.mSkills[0][1] = {};
        const auto beforeCustom = *service.findActorValues(actor);
        EXPECT_THROW(world.replaceOblivionPlayerCharacter(proposed, {}, &malformed, 4), std::invalid_argument);
        EXPECT_EQ(*service.findActorValues(actor), beforeCustom);
        EXPECT_EQ(store.get<ESM::Class>().search(nextClass), nullptr);
        EXPECT_EQ(world.getPlayer().getOblivionCharacterGenerationFlags(), 10);
        ASSERT_TRUE(world.replaceOblivionPlayerCharacter(proposed, {}, &custom, 4));
        EXPECT_EQ(world.getPlayerPtr().get<ESM::NPC>()->mBase->mClass, nextClass);
        ASSERT_NE(store.get<ESM::Class>().search(nextClass), nullptr);
        EXPECT_EQ(world.getPlayer().getOblivionCharacterGenerationFlags(), 14);
        EXPECT_EQ(service.getPlayerValue(8), 80);
        EXPECT_FALSE(service.takeNextDeathEvent());
        MWWorld::World legacy(nullptr, -1, "", {}, ESM::GameProfile::Morrowind);
        EXPECT_FALSE(legacy.replaceOblivionPlayerCharacter(proposed, {}, nullptr, 0));
    }

    TEST(OblivionWorldTest, olderNativePlayerViewAdoptionPreservesResourcesBreathAndNoHistoricalDeaths)
    {
        for (const unsigned version : {7u, 8u})
        for (const bool dead : {false, true})
        {
            NativeWorldFixture fixture;
            auto& world = fixture.mWorld;
            MWClass::Npc::registerSelf(); world.setupPlayer();
            const auto ptr = world.getPlayerPtr();
            auto& stats = ptr.getClass().getNpcStats(ptr);
            for (unsigned i = 0; i < 8; ++i)
                stats.setAttribute(ESM::Attribute::indexToRefId(i), 40);
            stats.setHealth(MWMechanics::DynamicStat<float>(100, 5, 80));
            stats.setMagicka(MWMechanics::DynamicStat<float>(80, 10, 60));
            stats.setFatigue(MWMechanics::DynamicStat<float>(180, 5, 150));
            stats.setTimeToStartDrowning(7.25f);
            stats.getSkill(ESM::Skill::Athletics).setProgress(.75f);
            ESM4::Npc native{}; native.mId = {7, 1}; native.mIsTES4 = true;
            const auto base = ESM::FormKey::content("Oblivion.esm", 7);
            native.mFormKey = base;
            world.getStore().getWritable<ESM4::Npc>().insertStatic(native, base);
            const auto actor = ESM::FormKey::dynamic("player", 1);
            ESM4::RuntimeState state; state.mVersion = version;
            state.mPlayer.mReference = actor;
            state.mPlayer.mCell = ESM::FormKey::content("headless.esm", 1);
            state.mPlayer.mRace = ESM::FormKey::content("headless.esm", 0x810);
            state.mPlayer.mClass = ESM::FormKey::dynamic("fixture-class", 1);
            ESM4::RuntimeReferenceState reference; reference.mKey = actor;
            reference.mBase = ESM::FormKey::dynamic("player-base", 1); reference.mCell = state.mPlayer.mCell;
            reference.mCustomState["obscript.dead"] = dead; state.mReferences.push_back(reference);
            std::ifstream content(fixture.mDirectory / "headless.esm", std::ios::binary);
            state.mContent.push_back({"headless.esm", "sha256:" + Files::getSha256("headless.esm", content)});
            readNativeSnapshot(fixture, state);
            auto& service = *world.getOblivionCombatService();
            ASSERT_TRUE(world.initializeOblivionPlayerActor());
            ASSERT_NE(service.findActorValues(actor), nullptr);
            const auto values = *service.findActorValues(actor);
            EXPECT_EQ(values.mPlayerFormValues, (std::optional<std::array<std::int32_t, 4>>{{20, 20, 20, 0}}));
            EXPECT_FALSE(values.mPassiveAbilities); // Omitted history remains unknown.
            EXPECT_EQ(values.mValues[8].mModifiers, (ESM4::ActorValueModifiers{5.f, 0.f, -25.f}));
            EXPECT_EQ(service.getPlayerValue(8), 80);
            EXPECT_EQ(service.getPlayerValue(9), 60);
            EXPECT_EQ(service.getPlayerValue(10), 150);
            EXPECT_EQ(stats.getHealth().getModified(false), 105);
            EXPECT_EQ(stats.getMagicka().getModified(false), 90);
            EXPECT_EQ(stats.getFatigue().getModified(false), 185);
            EXPECT_EQ(stats.getSkill(ESM::Skill::Athletics).getProgress(), .75f);
            EXPECT_EQ(service.findActorBreath(actor), 7.25f);
            EXPECT_EQ(service.findActorLife(actor)->mPhase,
                dead ? ESM4::ActorLifePhase::Dead : ESM4::ActorLifePhase::Alive);
            EXPECT_EQ(stats.isDead(), dead);
            EXPECT_FALSE(service.takeNextDeathEvent());
            EXPECT_EQ(service.getDeadCount(reference.mBase), 0);
            ASSERT_TRUE(world.initializeOblivionPlayerActor());
            EXPECT_EQ(*service.findActorValues(actor), values);
            auto saved = state; saved.mVersion = ESM4::CurrentRuntimeStateVersion;
            service.capture(saved);
            MWMechanics::OblivionCombatService restored;
            restored.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
            EXPECT_EQ(*restored.findActorValues(actor), values);
            EXPECT_EQ(restored.findActorBreath(actor), 7.25f);
            EXPECT_FALSE(restored.takeNextDeathEvent());
        }
    }

    TEST(OblivionWorldTest, olderNativePlayerUnrepresentableViewRejectsBeforeAuthorityOrMarkerConsumption)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        MWClass::Npc::registerSelf(); world.setupPlayer();
        const auto ptr = world.getPlayerPtr();
        auto& stats = ptr.getClass().getNpcStats(ptr);
        for (unsigned i = 0; i < 8; ++i) stats.setAttribute(ESM::Attribute::indexToRefId(i), 40);
        stats.setHealth(MWMechanics::DynamicStat<float>(100.25f, 0, 80));
        stats.setMagicka(MWMechanics::DynamicStat<float>(80, 0, 60));
        stats.setFatigue(MWMechanics::DynamicStat<float>(180, 0, 150));
        ESM4::Npc native{}; native.mId = {7, 1}; native.mIsTES4 = true;
        const auto base = ESM::FormKey::content("Oblivion.esm", 7); native.mFormKey = base;
        world.getStore().getWritable<ESM4::Npc>().insertStatic(native, base);
        const auto actor = ESM::FormKey::dynamic("player", 1);
        ESM4::RuntimeState state; state.mVersion = 7; state.mPlayer.mReference = actor;
        state.mPlayer.mCell = ESM::FormKey::content("headless.esm", 1);
        state.mPlayer.mRace = ESM::FormKey::content("headless.esm", 0x810);
        state.mPlayer.mClass = ESM::FormKey::dynamic("fixture-class", 1);
        ESM4::RuntimeReferenceState reference; reference.mKey = actor;
        reference.mBase = ESM::FormKey::dynamic("player-base", 1); reference.mCell = state.mPlayer.mCell;
        reference.mCustomState["obscript.dead"] = true; state.mReferences.push_back(reference);
        std::ifstream content(fixture.mDirectory / "headless.esm", std::ios::binary);
        state.mContent.push_back({"headless.esm", "sha256:" + Files::getSha256("headless.esm", content)});
        readNativeSnapshot(fixture, state);
        auto& service = *world.getOblivionCombatService();
        EXPECT_THROW(world.initializeOblivionPlayerActor(), std::invalid_argument);
        EXPECT_EQ(service.findActorValues(actor), nullptr);
        EXPECT_EQ(service.findActorLife(actor), nullptr);
        EXPECT_FALSE(service.findActorBreath(actor));
        EXPECT_EQ(stats.getHealth().getBase(), 100.25f);
        EXPECT_FALSE(stats.isDead());
        stats.setHealth(MWMechanics::DynamicStat<float>(100, 0, 80));
        stats.setAttribute(ESM::Attribute::Strength, 12.25f);
        EXPECT_THROW(world.initializeOblivionPlayerActor(), std::invalid_argument);
        EXPECT_EQ(service.findActorValues(actor), nullptr);
        stats.setAttribute(ESM::Attribute::Strength, 40);
        stats.setTimeToStartDrowning(std::numeric_limits<float>::quiet_NaN());
        EXPECT_THROW(world.initializeOblivionPlayerActor(), std::invalid_argument);
        EXPECT_EQ(service.findActorValues(actor), nullptr);
        EXPECT_EQ(service.findActorLife(actor), nullptr);
        EXPECT_FALSE(service.findActorBreath(actor));
        stats.setTimeToStartDrowning(7.5f);
        ASSERT_TRUE(world.initializeOblivionPlayerActor());
        EXPECT_EQ(service.findActorBreath(actor), 7.5f);
        EXPECT_EQ(service.findActorLife(actor)->mPhase, ESM4::ActorLifePhase::Dead);
        EXPECT_FALSE(service.takeNextDeathEvent());
    }


    TEST(OblivionWorldTest, actorRegistrationPromotesRestoredNpcWithoutResettingValuesOrLife)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& service = *world.getOblivionCombatService();
        const auto ptr = addNativeNpc(fixture, 0x900);
        const auto key = ptr.getCellRef().getFormKey();
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(ptr, ESM4::ActorValueProcess::Low));
        auto values = *service.findActorValues(key);
        values.mValues[8].mModifiers = {25.f, 3.f, -13.f};
        values.mValues[0].mModifiers = {7.f, 2.f, -1.f};
        service.publishNonPlayerValues(ptr, values);
        auto life = *service.findActorLife(key);
        life.mPhase = ESM4::ActorLifePhase::Dead;
        service.publishNonPlayerLife(ptr, life);
        auto saved = captureNativeActorState(fixture, ptr);
        service.clear();
        service.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
        ASSERT_EQ(service.findActorValues(key)->mProcess, ESM4::ActorValueProcess::Low);
        MWMechanics::Actors actors;
        ASSERT_NO_THROW(actors.addActor(ptr));
        values.mProcess = ESM4::ActorValueProcess::Active;
        values.mProcessKnockedState = 0; // Newly constructed active process.
        values.mProcessAction = -1;
        ASSERT_NE(service.findActorValues(key), nullptr);
        EXPECT_EQ(*service.findActorValues(key), values);
        EXPECT_EQ(*service.findActorLife(key), life);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 115);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getAttribute(ESM::Attribute::Strength).getModified(), 45);
        EXPECT_TRUE(ptr.getClass().getCreatureStats(ptr).isDead());
        EXPECT_EQ(service.getDeadCount(values.mBase), 0);
        EXPECT_FALSE(service.takeNextDeathEvent());
        EXPECT_EQ(actors.size(), 0); // Headless admission has no animation/controller.
        ASSERT_NO_THROW(actors.addActor(ptr));
        EXPECT_EQ(*service.findActorValues(key), values);
        EXPECT_TRUE(world.activateOblivionActor(ptr));
        EXPECT_FALSE(world.activateOblivionActor({}));
        EXPECT_FALSE(static_cast<const MWWorld::World&>(world).getAnimation(MWWorld::ConstPtr(ptr)));
    }

    TEST(OblivionWorldTest, actorRegistrationRejectsMalformedMarkerBeforeNativePublication)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& service = *world.getOblivionCombatService();
        const auto ptr = addNativeNpc(fixture, 0x900);
        const auto key = ptr.getCellRef().getFormKey();
        auto saved = captureNativeActorState(fixture, ptr);
        saved.mReferences[0].mCustomState["obscript.dead"] = std::string("invalid");
        ASSERT_NO_FATAL_FAILURE(readNativeSnapshot(fixture, saved));
        MWMechanics::Actors actors;
        EXPECT_THROW(actors.addActor(ptr), std::invalid_argument);
        EXPECT_EQ(service.findActorValues(key), nullptr);
        EXPECT_EQ(service.findActorLife(key), nullptr);
        EXPECT_EQ(actors.size(), 0);
        EXPECT_THROW(actors.addActor(ptr), std::invalid_argument);
        saved.mReferences[0].mCustomState["obscript.dead"] = true;
        ASSERT_NO_FATAL_FAILURE(readNativeSnapshot(fixture, saved));
        ASSERT_NO_THROW(actors.addActor(ptr));
        ASSERT_NE(service.findActorLife(key), nullptr);
        EXPECT_EQ(service.findActorLife(key)->mPhase, ESM4::ActorLifePhase::Dead);
        EXPECT_FALSE(service.takeNextDeathEvent());
    }

    TEST(OblivionWorldTest, freshPlayerRegistrationBuildsNativeCharacterAndRetainsGrantsOnReadmission)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
        auto& service = *world.getOblivionCombatService();
        MWClass::Npc::registerSelf();
        world.setupPlayer();
        const auto actor = ESM::FormKey::dynamic("player", 1);
        ESM4::Npc native{}; native.mId = {7, 0}; native.mIsTES4 = true;
        native.mFormKey = ESM::FormKey::content("Oblivion.esm", 7);
        native.mData.attribs = {40, 40, 40, 40, 40, 40, 40, 40};
        store.getWritable<ESM4::Npc>().insertStatic(native, native.mFormKey);
        ESM4::Race race{}; race.mId = {0x810, 0};
        race.mAttribMale = race.mAttribFemale = {40, 40, 40, 40, 40, 40, 40, 40};
        race.mTES4SkillBonuses = std::array<ESM4::Race::SkillBonus, 7>{};
        for (auto& bonus : *race.mTES4SkillBonuses) bonus.mSkill = -1;
        ESM4::Class characterClass{}; characterClass.mId = {0x811, 0};
        characterClass.mData.mFavoredAttributes = {0, 5};
        characterClass.mData.mMajorSkills = {12, 13, 14, 15, 16, 17, 18};
        characterClass.mData.mSpecialization = 0;
        store.getWritable<ESM4::Class>().insertStatic(characterClass, ESM::FormKey::content("headless.esm", 0x811));
        ESM4::EffectSetting effect{}; effect.mId = {0x812, 0}; effect.mEffectCode = ESM::fourCC("FOSP");
        effect.mFullName = "Fortify Magicka"; effect.mData.emplace(); effect.mData->mSchool = 2;
        effect.preparePassiveValueModifierDefinition();
        store.getWritable<ESM4::EffectSetting>().insertStatic(effect, ESM::FormKey::content("headless.esm", 0x812));
        ESM4::Spell spell{}; spell.mId = {0x813, 0}; spell.mData = ESM4::SpellData{4, 0, 0, 0, {}};
        spell.mEffects = {{ESM::fourCC("FOSP"), 25, 0, 0, 0, 0, {}}};
        race.mBonusSpells = {spell.mId};
        store.getWritable<ESM4::Spell>().insertStatic(spell, ESM::FormKey::content("headless.esm", 0x813));
        store.getWritable<ESM4::Race>().insertStatic(race, ESM::FormKey::content("headless.esm", 0x810));
        // Native profile loading also publishes the shared race facade used
        // by the Player class while constructing its first CustomData.
        ESM::Race sharedRace{}; sharedRace.blank(); sharedRace.mId = race.mId;
        store.insertStatic(sharedRace);
        auto proposed = *world.getPlayerPtr().get<ESM::NPC>()->mBase;
        proposed.mRace = race.mId; proposed.mClass = characterClass.mId; proposed.mNpdt.mLevel = 1;
        proposed.setIsMale(true);
        auto metadata = store.preparePlayerRecord(proposed);
        world.getPlayer().set(metadata.commit());
        ASSERT_EQ(service.findActorValues(actor), nullptr);
        MWMechanics::Actors actors;
        EXPECT_THROW(actors.addActor(world.getPlayerPtr()), std::invalid_argument);
        EXPECT_EQ(service.findActorValues(actor), nullptr);
        EXPECT_EQ(service.findActorLife(actor), nullptr);
        for (unsigned i = 0; i < 21; ++i)
        {
            ESM4::Skill skill{}; skill.mId = {0x1000 + i, 0}; skill.mIndex = i + 12;
            skill.mData = ESM4::SkillData{i + 12, 0, i % 3, {1, 2}};
            store.getWritable<ESM4::Skill>().insertStatic(skill, ESM::FormKey::content("headless.esm", 0x1000 + i));
        }
        ASSERT_NO_THROW(actors.addActor(world.getPlayerPtr()));
        ASSERT_NE(service.findActorValues(actor), nullptr);
        ASSERT_TRUE(service.findActorValues(actor)->mPassiveAbilities);
        ASSERT_EQ(service.findActorValues(actor)->mPassiveAbilities->size(), 1);
        EXPECT_EQ(service.getPlayerValue(8), 90);
        EXPECT_EQ(service.getPlayerValue(9), 85);
        EXPECT_EQ(world.getPlayer().getOblivionCharacterGenerationFlags(), 0);
        ASSERT_TRUE(world.requestOblivionResourceCurrent(world.getPlayerPtr(), 9, 55));
        const auto values = *service.findActorValues(actor);
        ASSERT_NO_THROW(actors.addActor(world.getPlayerPtr()));
        EXPECT_EQ(*service.findActorValues(actor), values);
        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = actor;
        saved.mPlayer.mCell = ESM::FormKey::content("headless.esm", 1);
        saved.mPlayer.mRace = ESM::FormKey::content("headless.esm", 0x810);
        saved.mPlayer.mClass = ESM::FormKey::content("headless.esm", 0x811);
        service.capture(saved);
        service.clear();
        service.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
        ASSERT_NO_THROW(actors.addActor(world.getPlayerPtr()));
        EXPECT_EQ(*service.findActorValues(actor), values);
        EXPECT_EQ(service.getPlayerValue(9), 55);
        EXPECT_FALSE(service.takeNextDeathEvent());
    }

    TEST(OblivionWorldTest, actorRegistrationRejectsActiveOverflowWithoutChangingLowAuthority)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& service = *world.getOblivionCombatService();
        const auto ptr = addNativeNpc(fixture, 0x900);
        const auto key = ptr.getCellRef().getFormKey();
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(ptr, ESM4::ActorValueProcess::Low));
        auto values = *service.findActorValues(key);
        const float large = std::numeric_limits<float>::max() * .75f;
        // Low processing ignores Maximum. Active processing would overflow.
        values.mValues[8].mModifiers = {large, large, 0.f};
        service.publishNonPlayerValues(ptr, values);
        const auto life = *service.findActorLife(key);
        const auto before = ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent();
        MWMechanics::Actors actors;
        EXPECT_THROW(actors.addActor(ptr), std::runtime_error);
        EXPECT_EQ(*service.findActorValues(key), values);
        EXPECT_EQ(*service.findActorLife(key), life);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), before);
        EXPECT_FALSE(service.takeNextDeathEvent());
        EXPECT_EQ(actors.size(), 0);
        values.mValues[8].mModifiers = {10.f, 3.f, -13.f};
        service.publishNonPlayerValues(ptr, values);
        ASSERT_NO_THROW(actors.addActor(ptr));
        EXPECT_EQ(service.findActorValues(key)->mProcess, ESM4::ActorValueProcess::Active);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 100);
    }

    TEST(OblivionWorldTest, actorRegistrationAdmitsNativeCreatureAndBypassesMorrowindProfile)
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
        creature.mData.attribs.strength = 37;
        store.getWritable<ESM4::Creature>().insertStatic(creature, creature.mFormKey);
        ESM4::ActorCreature reference{};
        reference.mId = {0x920, 0}; reference.mFormKey = ESM::FormKey::content("headless.esm", 0x920);
        reference.mBaseKey = creature.mFormKey;
        store.getWritable<ESM4::ActorCreature>().insertStatic(reference, reference.mFormKey);
        MWWorld::LiveCellRef<ESM4::Creature> live(reference, store.search<ESM4::Creature>(creature.mFormKey));
        auto& cell = world.getWorldModel().getDraftCell();
        MWWorld::Ptr ptr(cell.insert(&live), &cell);
        MWMechanics::Actors actors;
        ASSERT_NO_THROW(actors.addActor(ptr));
        ASSERT_NE(service.findActorValues(reference.mFormKey), nullptr);
        EXPECT_EQ(service.findActorValues(reference.mFormKey)->mProcess, ESM4::ActorValueProcess::Active);
        EXPECT_EQ(service.findActorValues(reference.mFormKey)->mNonPlayerFormHealth, 99);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 99);
        EXPECT_FALSE(service.takeNextDeathEvent());
        MWWorld::World legacy{&fixture.mResources, -1, "", fixture.mDirectory, ESM::GameProfile::Morrowind};
        EXPECT_FALSE(legacy.activateOblivionActor(ptr));
        EXPECT_FALSE(legacy.activateOblivionActor({}));
        EXPECT_EQ(legacy.getOblivionCombatService(), nullptr);
        EXPECT_EQ(service.findActorValues(reference.mFormKey)->mNonPlayerFormHealth, 99);
    }

    TEST(OblivionWorldTest, temporaryNativeClassIgnoresPrecedingScriptFilesWithoutAliasingOtherClasses)
    {
        for (const bool scriptsFirst : {false, true})
        for (const bool extraNative : {false, true})
        {
            SCOPED_TRACE(scriptsFirst);
            SCOPED_TRACE(extraNative);
            NativeWorldFixture fixture(scriptsFirst, extraNative);
            auto& store = fixture.mWorld.getStore();
            const auto base = store.search<ESM4::Npc>(ESM::FormKey::content("headless.esm", 0x800));
            ASSERT_NE(base, nullptr);
            const auto contentIndex = base->mId.mContentFile;
            EXPECT_EQ(contentIndex, scriptsFirst ? 1 : 0);
            ESM4::Race race{}; race.mId = {0x810, contentIndex};
            race.mAttribMale = race.mAttribFemale = {40, 40, 40, 40, 40, 40, 40, 40};
            race.mTES4SkillBonuses = std::array<ESM4::Race::SkillBonus, 7>{};
            for (auto& bonus : *race.mTES4SkillBonuses) bonus.mSkill = -1;
            store.getWritable<ESM4::Race>().insertStatic(race, ESM::FormKey::content("headless.esm", 0x810));
            ESM4::Class characterClass{}; characterClass.mId = {0x230e6, contentIndex};
            characterClass.mData.mFavoredAttributes = {0, 1};
            characterClass.mData.mMajorSkills = {12, 13, 14, 15, 16, 17, 18};
            characterClass.mData.mSpecialization = 0;
            store.getWritable<ESM4::Class>().insertStatic(characterClass,
                ESM::FormKey::content("headless.esm", 0x230e6));
            for (unsigned i = 0; i < 21; ++i)
            {
                ESM4::Skill skill{}; skill.mId = {0x1000 + i, contentIndex}; skill.mIndex = i + 12;
                skill.mData = ESM4::SkillData{i + 12, 0, 0, {1, 2}};
                store.getWritable<ESM4::Skill>().insertStatic(skill, ESM::FormKey::content("headless.esm", 0x1000 + i));
            }
            EXPECT_FALSE(store.resolveEsm4RuntimeFormId(0));
            EXPECT_EQ(store.resolveEsm4RuntimeFormId(0x230e6), characterClass.mId);
            EXPECT_FALSE(store.resolveEsm4RuntimeFormId(0xff0230e6));
            const auto temporary = MWWorld::resolveOblivionPlayerCharacterBaseStats(
                store, race.mId, characterClass.mId, false, 1);
            EXPECT_EQ(temporary.mAttributes, (std::array<std::uint8_t, 8>{40, 40, 40, 40, 40, 40, 40, 40}));
            EXPECT_EQ(temporary.mSkills, (std::array<std::uint8_t, 21>{5, 5, 5, 5, 5, 5, 5,
                5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5}));
            auto unrelated = characterClass;
            unrelated.mId.mContentFile = contentIndex + 1;
            store.getWritable<ESM4::Class>().insertStatic(unrelated, ESM::FormKey::content("other.esm", 0x230e6));
            const auto ordinary = MWWorld::resolveOblivionPlayerCharacterBaseStats(
                store, race.mId, unrelated.mId, false, 1);
            EXPECT_EQ(ordinary.mAttributes[0], 45);
            EXPECT_EQ(ordinary.mAttributes[1], 45);
            EXPECT_EQ(ordinary.mSkills[0], 30);
            ESM4::GameSetting disabled{}; disabled.mId = {0x3333, contentIndex};
            disabled.mEditorId = "iClassCharactergenClass"; disabled.mData = std::int32_t(0);
            store.getWritable<ESM4::GameSetting>().insertStatic(disabled);
            EXPECT_EQ(MWWorld::resolveOblivionPlayerCharacterBaseStats(
                store, race.mId, characterClass.mId, false, 1), ordinary);
            disabled.mData = std::int32_t(0x010230e6);
            store.getWritable<ESM4::GameSetting>().insertStatic(disabled);
            EXPECT_EQ(store.resolveEsm4RuntimeFormId(0x010230e6).has_value(), extraNative);
            const auto selectedSecond = MWWorld::resolveOblivionPlayerCharacterBaseStats(
                store, race.mId, unrelated.mId, false, 1);
            EXPECT_EQ(selectedSecond, extraNative ? temporary : ordinary);
            EXPECT_EQ(MWWorld::resolveOblivionPlayerCharacterBaseStats(
                store, race.mId, characterClass.mId, false, 1), ordinary);
        }
    }

    TEST(OblivionWorldTest, scriptCombatCommandsPublishNativeMembershipAndStopOwnedActions)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& service = *world.getOblivionCombatService();
        std::array<MWWorld::Ptr, 3> actors;
        std::array<ESM::FormKey, 3> keys;
        for (std::size_t i = 0; i < actors.size(); ++i)
        {
            actors[i] = addNativeNpc(fixture, 0x900 + i);
            world.getWorldModel().registerPtr(actors[i]);
            ASSERT_TRUE(world.activateOblivionActor(actors[i]));
            keys[i] = actors[i].getCellRef().getFormKey();
        }
        MWWorld::OblivionScriptManager host(world, world.getStore(), {"headless.esm"});
        ObScript::RuntimeContext context;
        context.mSelf = keys[0];
        const auto ref = [](const ESM::FormKey& key) -> ObScript::Value { return ObScript::ReferenceValue{key, {}}; };
        const auto snapshot = [&] {
            auto state = captureNativeActorState(fixture, actors[0]);
            for (std::size_t i = 1; i < actors.size(); ++i)
                state.mReferences.push_back(captureNativeActorState(fixture, actors[i]).mReferences[0]);
            return state.serializeBinary();
        };
        EXPECT_EQ(ObScript::asInteger(host.call("IsInCombat", {}, {}, context, {})), 0);
        EXPECT_EQ(ObScript::asInteger(host.call("StartCombat", {}, {ref(keys[1])}, context, {})), 0);
        EXPECT_TRUE(service.isInCombatWith(keys[0], keys[1]));
        EXPECT_TRUE(service.isInCombatWith(keys[1], keys[0]));
        EXPECT_EQ(ObScript::asInteger(host.call("IsInCombat", {}, {}, context, {})), 1);
        EXPECT_EQ(ObScript::asInteger(host.call("IsInCombat", ref(keys[1]), {}, context, {})), 1);
        const auto first = snapshot();
        host.call("startcombat", ref(keys[0]), {ref(keys[1])}, {}, {});
        EXPECT_EQ(snapshot(), first); // Repeat is idempotent.
        host.call("STARTCOMBAT", ref(keys[1]), {ref(keys[2])}, {}, {});
        EXPECT_TRUE(service.isInCombatWith(keys[1], keys[2]));
        const auto a = service.allocateAction(keys[0]);
        const auto b = service.allocateAction(keys[1]);
        const auto c = service.allocateAction(keys[2]);
        const auto anonymous = service.allocateAction();
        auto restartedState = ESM4::RuntimeState::deserializeBinary(snapshot());
        MWMechanics::OblivionCombatService restarted;
        restarted.restore(restartedState);
        EXPECT_TRUE(restarted.isInCombatWith(keys[0], keys[1]));
        EXPECT_TRUE(restarted.isActionPending(a, keys[0]));
        EXPECT_EQ(ObScript::asInteger(host.call("StopCombat", {}, {}, context, {})), 0);
        EXPECT_FALSE(service.isInCombat(keys[0]));
        EXPECT_FALSE(service.isInCombatWith(keys[1], keys[0]));
        EXPECT_TRUE(service.isInCombatWith(keys[1], keys[2]));
        EXPECT_FALSE(service.isActionPending(a, keys[0]));
        EXPECT_TRUE(service.isActionPending(b, keys[1]));
        EXPECT_TRUE(service.isActionPending(c, keys[2]));
        EXPECT_TRUE(service.isActionPending(anonymous));
        restarted.stopCombat(keys[0]); restarted.cancelActorActions(keys[0]);
        restarted.capture(restartedState);
        EXPECT_EQ(restartedState.serializeBinary(), snapshot());
        const auto stopped = snapshot();
        host.call("StopCombat", ref(keys[0]), {}, {}, {});
        EXPECT_EQ(snapshot(), stopped);
        const auto noMembership = service.allocateAction(keys[0]);
        host.call("StopCombat", ref(keys[0]), {}, {}, {});
        EXPECT_TRUE(service.isActionPending(noMembership));
        EXPECT_TRUE(service.isActionPending(b, keys[1]));
        EXPECT_EQ(service.getNonPlayerValue(actors[0], 8), 100);
        EXPECT_FALSE(service.takeNextDeathEvent());
    }

    TEST(OblivionWorldTest, scriptCombatRejectsInvalidEndpointsWithoutNativeMutation)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& service = *world.getOblivionCombatService();
        const auto a = addNativeNpc(fixture, 0x900);
        const auto b = addNativeNpc(fixture, 0x901);
        const auto uninitialized = addNativeNpc(fixture, 0x902);
        for (auto actor : {a, b, uninitialized}) world.getWorldModel().registerPtr(actor);
        ASSERT_TRUE(world.activateOblivionActor(a));
        ASSERT_TRUE(world.activateOblivionActor(b));
        const auto key = a.getCellRef().getFormKey(), other = b.getCellRef().getFormKey();
        const auto ref = [](const ESM::FormKey& value) -> ObScript::Value { return ObScript::ReferenceValue{value, {}}; };
        MWWorld::OblivionScriptManager host(world, world.getStore(), {"headless.esm"});
        ObScript::RuntimeContext context; context.mSelf = key;
        const auto snapshot = [&] {
            auto state = captureNativeActorState(fixture, a);
            state.mReferences.push_back(captureNativeActorState(fixture, b).mReferences[0]);
            return state.serializeBinary();
        };
        const auto owned = service.allocateAction(key);
        const auto before = snapshot();
        for (const auto& arguments : std::vector<std::vector<ObScript::Value>>{
                 {}, {ref(other), ref(other)}, {std::int64_t(1)}, {ref(key)},
                 {ref(ESM::FormKey::content("headless.esm", 0x990))},
                 {ref(ESM::FormKey::content("different.esm", 0x901))},
                 {ref(uninitialized.getCellRef().getFormKey())},
                 {ref(ESM::FormKey::content("headless.esm", 0x800))}})
        {
            EXPECT_THROW(host.call("StartCombat", {}, arguments, context, {}), ObScript::RuntimeError);
            EXPECT_EQ(snapshot(), before);
            EXPECT_TRUE(service.isActionPending(owned, key));
        }
        EXPECT_THROW(host.call("StopCombat", {}, {ref(other)}, context, {}), ObScript::RuntimeError);
        EXPECT_EQ(snapshot(), before);
        EXPECT_THROW(host.call("StartCombat", ref(uninitialized.getCellRef().getFormKey()), {ref(other)}, {}, {}),
            ObScript::RuntimeError);
        EXPECT_EQ(snapshot(), before);
        ASSERT_TRUE(world.killOblivionActor(b, {}));
        const auto afterDeath = snapshot();
        EXPECT_THROW(host.call("StartCombat", {}, {ref(other)}, context, {}), ObScript::RuntimeError);
        EXPECT_EQ(snapshot(), afterDeath);
        EXPECT_TRUE(service.isActionPending(owned, key));
    }

    TEST(OblivionWorldTest, scriptCombatPlayerAliasQueriesDoNotChangeStopCommandVirtualPolicy)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        MWClass::Npc::registerSelf(); world.setupPlayer();
        auto& service = *world.getOblivionCombatService();
        const auto actor = addNativeNpc(fixture, 0x900);
        world.getWorldModel().registerPtr(actor);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        ESM4::RuntimeActorValues values;
        values.mActor = ESM::FormKey::dynamic("player", 1);
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mPlayerFormValues = {{10, 0, 0, 0}};
        for (std::size_t i = 0; i < 8; ++i) values.mValues[i].mBase = 50;
        service.publishPlayerValues(world.getPlayer(), values, MWWorld::resolveOblivionPlayerDynamicBaseSettings(world.getStore()));
        service.publishPlayerLife(world.getPlayer(), {values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
        MWWorld::OblivionScriptManager host(world, world.getStore(), {"headless.esm"});
        const auto ref = [](const ESM::FormKey& key) -> ObScript::Value { return ObScript::ReferenceValue{key, {}}; };
        const auto npcKey = actor.getCellRef().getFormKey();
        const auto alias = ESM::FormKey::content("Oblivion.esm", 0x14);
        host.call("StartCombat", ref(npcKey), {ref(alias)}, {}, {});
        ASSERT_TRUE(service.isInCombatWith(npcKey, values.mActor));
        EXPECT_EQ(ObScript::asInteger(host.call("IsInCombat", ref(alias), {}, {}, {})), 1);
        EXPECT_EQ(ObScript::asInteger(host.call("IsInCombat", ref(values.mActor), {}, {}, {})), 1);
        const auto action = service.allocateAction(values.mActor);
        const auto before = captureNativeActorState(fixture, actor).serializeBinary();
        host.call("StopCombat", ref(alias), {}, {}, {});
        host.call("StopCombat", ref(values.mActor), {}, {}, {});
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), before);
        EXPECT_TRUE(service.isActionPending(action, values.mActor));
        host.call("StopCombat", ref(npcKey), {}, {}, {});
        EXPECT_FALSE(service.isInCombat(values.mActor));
        EXPECT_TRUE(service.isActionPending(action, values.mActor));
    }

    TEST(OblivionWorldTest, physicalWorldEntryPointsPreserveOwnershipMissReplayAndRejectedContact)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        MWBase::World& api = world;
        auto& service = *world.getOblivionCombatService();
        const auto a = addNativeNpc(fixture, 0x900);
        const auto b = addNativeNpc(fixture, 0x901);
        const auto uninitialized = addNativeNpc(fixture, 0x902);
        ASSERT_TRUE(world.activateOblivionActor(a));
        ASSERT_TRUE(world.activateOblivionActor(b));
        const auto key = a.getCellRef().getFormKey();
        const auto other = b.getCellRef().getFormKey();
        const auto snapshot = [&] {
            auto state = captureNativeActorState(fixture, a);
            state.mReferences.push_back(captureNativeActorState(fixture, b).mReferences[0]);
            return state.serializeBinary();
        };
        const auto initial = snapshot();
        EXPECT_EQ(api.beginOblivionPhysicalAction({}), 0);
        EXPECT_EQ(api.beginOblivionPhysicalAction(uninitialized), 0);
        EXPECT_FALSE(api.cancelOblivionPhysicalAction(1, {}));
        EXPECT_FALSE(api.commitOblivionPhysicalContact(1, {}, b, {-7, -3, -1}));
        EXPECT_EQ(snapshot(), initial);
        const auto id = api.beginOblivionPhysicalAction(a);
        ASSERT_NE(id, 0);
        EXPECT_TRUE(service.isActionPending(id, key));
        const auto before = snapshot();
        EXPECT_FALSE(api.cancelOblivionPhysicalAction(id, b));
        EXPECT_FALSE(api.commitOblivionPhysicalContact(id, b, a, {-7, -3, -1}));
        EXPECT_EQ(snapshot(), before);
        EXPECT_THROW(api.commitOblivionPhysicalContact(id, a, b,
            {std::numeric_limits<float>::quiet_NaN(), -3, -1}), std::invalid_argument);
        EXPECT_THROW(api.commitOblivionPhysicalContact(id, a, uninitialized, {-7, -3, -1}), std::invalid_argument);
        EXPECT_THROW(api.commitOblivionPhysicalContact(id, a, {}, {-7, -3, -1}), std::invalid_argument);
        EXPECT_EQ(snapshot(), before);
        const float sourceFatigue = service.getNonPlayerValue(a, 10);
        const float targetFatigue = service.getNonPlayerValue(b, 10);
        EXPECT_TRUE(api.commitOblivionPhysicalContact(id, a, b, {-7, -3, -1}));
        EXPECT_EQ(service.getNonPlayerValue(a, 10), sourceFatigue - 7);
        EXPECT_EQ(service.getNonPlayerValue(b, 10), targetFatigue - 1);
        EXPECT_EQ(service.getNonPlayerValue(b, 8), 97);
        EXPECT_TRUE(service.isActionConsumed(id));
        const auto hit = snapshot();
        EXPECT_FALSE(api.commitOblivionPhysicalContact(id, a, b, {-7, -3, -1}));
        EXPECT_EQ(snapshot(), hit);
        const auto miss = api.beginOblivionPhysicalAction(a);
        EXPECT_TRUE(api.commitOblivionPhysicalContact(miss, a, {}, {-2, 0, 0}));
        EXPECT_EQ(service.getNonPlayerValue(a, 10), sourceFatigue - 9);
        EXPECT_EQ(service.getNonPlayerValue(b, 8), 97);
        const auto cancel = api.beginOblivionPhysicalAction(a);
        const auto retained = api.beginOblivionPhysicalAction(a);
        const auto anotherOwner = api.beginOblivionPhysicalAction(b);
        EXPECT_TRUE(api.cancelOblivionPhysicalAction(cancel, a));
        EXPECT_FALSE(api.cancelOblivionPhysicalAction(cancel, a));
        EXPECT_FALSE(api.commitOblivionPhysicalContact(cancel, a, b, {-7, -3, -1}));
        EXPECT_TRUE(service.isActionPending(retained, key));
        EXPECT_TRUE(service.isActionPending(anotherOwner, other));
        const auto restored = ESM4::RuntimeState::deserializeBinary(snapshot());
        MWMechanics::OblivionCombatService resumed; resumed.restore(restored);
        EXPECT_TRUE(resumed.isActionPending(retained, key));
        EXPECT_TRUE(resumed.isActionConsumed(cancel));
        ASSERT_TRUE(world.killOblivionActor(a, {}));
        EXPECT_EQ(api.beginOblivionPhysicalAction(a), 0);
        EXPECT_FALSE(api.commitOblivionPhysicalContact(retained, a, b, {-7, -3, -1}));
        EXPECT_TRUE(service.isActionPending(anotherOwner, other));
    }

    TEST(OblivionWorldTest, physicalWorldContactResolvesEssentialAndPlayerGodMode)
    {
        for (bool essential : {false, true})
            for (bool godMode : {false, true})
            {
                NativeWorldFixture fixture;
                auto& world = fixture.mWorld;
                MWBase::World& api = world;
                MWClass::Npc::registerSelf(); world.setupPlayer();
                auto& service = *world.getOblivionCombatService();
                const auto actor = addNativeNpc(fixture, 0x900);
                ASSERT_TRUE(world.activateOblivionActor(actor));
                const auto key = actor.getCellRef().getFormKey();
                ESM4::RuntimeActorValues values;
                values.mActor = ESM::FormKey::dynamic("player", 1);
                values.mBase = ESM::FormKey::dynamic("player-base", 1);
                values.mOwner = ESM4::ActorValueOwner::Player;
                values.mPlayerFormValues = {{10, 0, 0, 0}};
                for (std::size_t i = 0; i < 8; ++i) values.mValues[i].mBase = 50;
                service.publishPlayerValues(world.getPlayer(), values, MWWorld::resolveOblivionPlayerDynamicBaseSettings(world.getStore()));
                service.publishPlayerLife(world.getPlayer(), {values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
                auto player = world.getPlayerPtr();
                if (essential)
                {
                    auto record = *player.get<ESM::NPC>()->mBase;
                    record.mFlags |= ESM::NPC::Essential;
                    world.getStore().getWritable<ESM::NPC>().insert(record);
                }
                ASSERT_EQ(player.getClass().isEssential(player), essential);
                if (godMode)
                {
                    ASSERT_TRUE(world.toggleGodMode());
                }
                const float sourceFatigue = service.getNonPlayerValue(actor, 10);
                const float playerHealth = service.getPlayerValue(8);
                const float playerFatigue = service.getPlayerValue(10);
                const auto id = api.beginOblivionPhysicalAction(actor);
                const auto playerAction = api.beginOblivionPhysicalAction(player);
                ASSERT_TRUE(api.commitOblivionPhysicalContact(id, actor, player, {-7, -1000, -1}));
                EXPECT_EQ(service.getNonPlayerValue(actor, 10), sourceFatigue - 7);
                EXPECT_EQ(service.getPlayerValue(10), godMode ? playerFatigue : playerFatigue - 1);
                const auto* life = service.findActorLife(values.mActor);
                ASSERT_NE(life, nullptr);
                EXPECT_EQ(life->mPhase, godMode ? ESM4::ActorLifePhase::Alive
                    : essential ? ESM4::ActorLifePhase::EssentialUnconscious : ESM4::ActorLifePhase::Dead);
                if (godMode)
                {
                    EXPECT_EQ(service.getPlayerValue(8), playerHealth);
                    EXPECT_TRUE(service.isActionPending(playerAction, values.mActor));
                    EXPECT_TRUE(api.commitOblivionPhysicalContact(playerAction, player, actor, {-7, -3, -1}));
                    EXPECT_EQ(service.getPlayerValue(10), playerFatigue);
                    EXPECT_EQ(service.getNonPlayerValue(actor, 8), 97);
                }
                else
                {
                    EXPECT_FALSE(service.isActionPending(playerAction, values.mActor));
                    EXPECT_EQ(api.beginOblivionPhysicalAction(player), 0);
                }
                const auto event = service.takeNextDeathEvent();
                if (!godMode && !essential)
                {
                    ASSERT_TRUE(event); EXPECT_EQ(event->mActor, values.mActor); EXPECT_EQ(event->mKiller, key);
                }
                else EXPECT_FALSE(event);
            }
        MWWorld::World legacy(nullptr, -1, "", {}, ESM::GameProfile::Morrowind);
        EXPECT_EQ(legacy.beginOblivionPhysicalAction({}), 0);
        EXPECT_FALSE(legacy.cancelOblivionPhysicalAction(1, {}));
        EXPECT_FALSE(legacy.commitOblivionPhysicalContact(1, {}, {}, {}));
    }

    TEST(OblivionWorldTest, NativeProcessActionConstructorQueryAndSignedTransitions)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addNativeNpc(fixture, 0x801);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        auto& service = *world.getOblivionCombatService();
        const auto key = actor.getCellRef().getFormKey();
        EXPECT_EQ(service.findActorValues(key)->mProcessAction, std::optional<std::int16_t>{-1});
        const auto original = *service.findActorValues(key);
        for (int raw = -32768; raw <= 32767; ++raw)
        {
            service.setProcessAction(key, static_cast<std::int16_t>(raw));
            EXPECT_EQ(service.getProcessAction(key), raw);
            EXPECT_EQ(MWMechanics::oblivionBlockingPosture(world, actor), raw == 6);
            EXPECT_EQ(MWMechanics::oblivionProcessAction(world, actor), raw);
            auto expected = original;
            expected.mProcessAction = static_cast<std::int16_t>(raw);
            ASSERT_EQ(*service.findActorValues(key), expected) << raw;
        }
    }

    TEST(OblivionWorldTest, NativeKnockedStateConstructorQueryAndSignedTransitions)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addNativeNpc(fixture, 0x801);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        auto& service = *world.getOblivionCombatService();
        const auto key = actor.getCellRef().getFormKey();
        EXPECT_EQ(service.findActorValues(key)->mProcessKnockedState, std::optional<std::int8_t>{0});
        const auto original = *service.findActorValues(key);
        for (int raw = -128; raw <= 127; ++raw)
        {
            service.setProcessKnockedState(key, static_cast<std::int8_t>(raw));
            EXPECT_EQ(service.getProcessKnockedState(key), raw);
            EXPECT_EQ(MWMechanics::oblivionKnockedState(world, actor), raw);
            auto expected = original;
            expected.mProcessKnockedState = static_cast<std::int8_t>(raw);
            ASSERT_EQ(*service.findActorValues(key), expected) << raw;
        }
    }

    TEST(OblivionWorldTest, NativeProcessActionLowResetAndLegacyUnknownStayExplicit)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addNativeNpc(fixture, 0x801);
        auto& service = *world.getOblivionCombatService();
        const auto key = actor.getCellRef().getFormKey();
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(actor, ESM4::ActorValueProcess::Low));
        EXPECT_FALSE(service.findActorValues(key)->mProcessAction);
        EXPECT_EQ(MWMechanics::oblivionProcessAction(world, actor), -1);
        auto before = captureNativeActorState(fixture, actor).serializeBinary();
        for (int raw = -32768; raw <= 32767; ++raw)
            service.setProcessAction(key, static_cast<std::int16_t>(raw));
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), before);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        EXPECT_EQ(service.findActorValues(key)->mProcessAction, std::optional<std::int16_t>{-1});
        service.setProcessAction(key, -128);
        const auto saved = captureNativeActorState(fixture, actor);
        const auto binary = saved.serializeBinary();
        service.restore(ESM4::RuntimeState::deserializeBinary(binary), world.getStore());
        EXPECT_EQ(MWMechanics::oblivionProcessAction(world, actor), -128);
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), binary);
        auto old = saved;
        old.mVersion = 25;
        old.mNativeActorValues[0].mProcessAction.reset();
        service.restore(ESM4::RuntimeState::deserializeBinary(old.serializeBinary()), world.getStore());
        EXPECT_THROW(service.getProcessAction(key), std::invalid_argument);
        EXPECT_THROW(MWMechanics::oblivionProcessAction(world, actor), std::invalid_argument);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        EXPECT_FALSE(service.findActorValues(key)->mProcessAction); // Same process does not invent state.
        service.setProcessAction(key, 3);
        EXPECT_EQ(MWMechanics::oblivionProcessAction(world, actor), 3);
        ESM4::RuntimeState downgrade;
        downgrade.mVersion = 25;
        EXPECT_THROW(service.capture(downgrade), std::invalid_argument);
        service.resetNonPlayerForResurrection(actor);
        EXPECT_FALSE(service.findActorValues(key)->mProcessAction);
        EXPECT_EQ(service.findActorValues(key)->mProcess, ESM4::ActorValueProcess::Low);
        EXPECT_EQ(MWMechanics::oblivionProcessAction(world, actor), -1);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        EXPECT_EQ(MWMechanics::oblivionProcessAction(world, actor), -1);
        const auto missing = ESM::FormKey::dynamic("missing", 1);
        EXPECT_THROW(service.getProcessAction(missing), std::invalid_argument);
        EXPECT_THROW(service.setProcessAction(missing, 1), std::invalid_argument);
        EXPECT_THROW(MWMechanics::oblivionProcessAction(world, {}), std::invalid_argument);
        MWWorld::World foreign(nullptr, -1, "", {}, ESM::GameProfile::Morrowind);
        EXPECT_THROW(MWMechanics::oblivionProcessAction(foreign, actor), std::invalid_argument);
    }

    TEST(OblivionWorldTest, NativeParalysisIntegerAuthorityMatchesOriginalProfilesForPlayerAndNpc)
    {
        struct Case
        {
            std::int32_t mBase;
            float mMaximum, mScript, mDamage;
            std::int32_t mLow, mHigh, mPlayer;
            bool mCommonHigh;
        };
        static constexpr Case cases[] = {
#include "paralysis_expected.inc"
        };
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
        MWClass::Npc::registerSelf();
        world.setupPlayer();
        const auto base = ESM::FormKey::content("Oblivion.esm", 7);
        ESM4::Npc native{};
        native.mId = {7, 1}; native.mFormKey = base; native.mIsTES4 = true; native.mData.health = 100;
        store.getWritable<ESM4::Npc>().insertStatic(native, base);
        ASSERT_TRUE(world.initializeOblivionPlayerActor());
        const auto actor = addNativeNpc(fixture, 0x801), player = world.getPlayerPtr();
        ASSERT_TRUE(world.activateOblivionActor(actor));
        auto& service = *world.getOblivionCombatService();
        const auto npcKey = actor.getCellRef().getFormKey(), playerKey = ESM::FormKey::dynamic("player", 1);
        const auto source = captureNativeActorState(fixture, actor);
        for (const auto& test : cases)
            for (auto process : {ESM4::ActorValueProcess::Low, ESM4::ActorValueProcess::Active})
            {
                auto state = source;
                for (auto& values : state.mNativeActorValues)
                {
                    values.mValues[48] = {static_cast<float>(test.mBase),
                        {test.mMaximum, test.mScript, test.mDamage}};
                    if (values.mActor == npcKey)
                    {
                        values.mProcess = process;
                        if (process == ESM4::ActorValueProcess::Low)
                        {
                            values.mProcessAction.reset();
                            values.mProcessKnockedState.reset();
                        }
                    }
                }
                service.restore(ESM4::RuntimeState::deserializeBinary(state.serializeBinary()), store);
                const auto before = captureNativeActorState(fixture, actor).serializeBinary();
                const auto expected = process == ESM4::ActorValueProcess::Low ? test.mLow : test.mHigh;
                ASSERT_EQ(service.getProcessParalysis(npcKey), expected);
                EXPECT_EQ(MWMechanics::oblivionParalyzed(world, actor), expected != 0);
                if (process == ESM4::ActorValueProcess::Active)
                {
                    EXPECT_EQ(MWMechanics::oblivionParalyzed(world, actor), test.mCommonHigh);
                }
                ASSERT_EQ(service.getProcessParalysis(playerKey), test.mPlayer);
                EXPECT_EQ(MWMechanics::oblivionParalyzed(world, player), test.mPlayer != 0);
                EXPECT_FLOAT_EQ(service.getPlayerValue(48),
                    static_cast<float>(double(test.mBase) + test.mMaximum + test.mScript + test.mDamage));
                EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), before);
            }
    }

    TEST(OblivionWorldTest, NativeParalysisQueryRequiresActualBindingAndDoesNotInventActorState)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addNativeNpc(fixture, 0x801);
        auto& service = *world.getOblivionCombatService();
        const auto key = actor.getCellRef().getFormKey();
        EXPECT_THROW(MWMechanics::oblivionParalyzed(world, actor), std::invalid_argument);
        EXPECT_THROW(service.getProcessParalysis(key), std::invalid_argument);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        auto state = captureNativeActorState(fixture, actor);
        state.mNativeActorValues[0].mValues[48] = {0, {std::nullopt, .75f, std::nullopt}};
        state.mNativeActorValues[0].mProcessAction.reset();
        state.mNativeActorValues[0].mProcessKnockedState.reset();
        service.restore(state, world.getStore());
        EXPECT_FALSE(MWMechanics::oblivionParalyzed(world, actor));
        EXPECT_FALSE(service.findActorValues(key)->mProcessAction);
        EXPECT_FALSE(service.findActorValues(key)->mProcessKnockedState);
        state.mNativeActorValues[0].mValues[48].mModifiers[1] = -1.25f;
        service.restore(state, world.getStore());
        EXPECT_TRUE(MWMechanics::oblivionParalyzed(world, actor));
        EXPECT_EQ(service.getProcessParalysis(key), -1);
        EXPECT_EQ(service.findActorLife(key)->mPhase, ESM4::ActorLifePhase::Alive);
        EXPECT_THROW(MWMechanics::oblivionParalyzed(world, {}), std::invalid_argument);
        MWWorld::World foreign(nullptr, -1, "", {}, ESM::GameProfile::Morrowind);
        EXPECT_THROW(MWMechanics::oblivionParalyzed(foreign, actor), std::invalid_argument);
        state.mNativeActorValues[0].mValues[48].mBase = 3e38f;
        service.restore(state, world.getStore());
        const auto before = captureNativeActorState(fixture, actor).serializeBinary();
        EXPECT_THROW(MWMechanics::oblivionParalyzed(world, actor), std::invalid_argument);
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), before);
    }

    TEST(OblivionWorldTest, NativeCreatureParalysisUsesIntegerCompositionAndSharedBaseOverride)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
        MWClass::ESM4Creature::registerSelf();
        ESM4::Creature creature{};
        creature.mId = {0x820, 0}; creature.mFormKey = ESM::FormKey::content("headless.esm", 0x820);
        creature.mAttackReach = 64; creature.mBaseConfig.tes4.levelOrOffset = 1; creature.mData.health = 99;
        store.getWritable<ESM4::Creature>().insertStatic(creature, creature.mFormKey);
        ESM4::ActorCreature reference{};
        reference.mId = {0x920, 0}; reference.mFormKey = ESM::FormKey::content("headless.esm", 0x920);
        reference.mBaseKey = creature.mFormKey;
        store.getWritable<ESM4::ActorCreature>().insertStatic(reference, reference.mFormKey);
        MWWorld::LiveCellRef<ESM4::Creature> live(reference, store.search<ESM4::Creature>(creature.mFormKey));
        auto& cell = world.getWorldModel().getDraftCell();
        const MWWorld::Ptr actor(cell.insert(&live), &cell);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        auto& service = *world.getOblivionCombatService();
        auto state = captureNativeActorState(fixture, actor);
        state.mNativeActorValues[0].mValues[48] = {0, {.75f, -.75f, -.75f}};
        service.restore(state, store);
        EXPECT_EQ(service.getProcessParalysis(reference.mFormKey), 0);
        EXPECT_FALSE(MWMechanics::oblivionParalyzed(world, actor));
        auto base = std::find_if(state.mNativeActorBases.begin(), state.mNativeActorBases.end(),
            [&](const auto& value) { return value.mBase == creature.mFormKey; });
        if (base == state.mNativeActorBases.end())
        {
            state.mNativeActorBases.push_back({creature.mFormKey, ESM4::ActorBaseKind::Creature, {}});
            base = std::prev(state.mNativeActorBases.end());
        }
        auto entry = std::find_if(base->mValues.begin(), base->mValues.end(),
            [](const auto& value) { return value.mActorValue == 48; });
        if (entry == base->mValues.end()) base->mValues.push_back({48, 3.25f});
        else entry->mValue = 3.25f;
        std::sort(base->mValues.begin(), base->mValues.end(),
            [](const auto& lhs, const auto& rhs) { return lhs.mActorValue < rhs.mActorValue; });
        EXPECT_THROW(service.restore(state, store), std::invalid_argument);
        EXPECT_EQ(service.getProcessParalysis(reference.mFormKey), 0);
        // The stored float base override is queried as integer3. Both the
        // shared override and the resolved actor snapshot must agree.
        state.mNativeActorValues[0].mValues[48].mBase = 3;
        service.restore(ESM4::RuntimeState::deserializeBinary(state.serializeBinary()), store);
        const auto before = captureNativeActorState(fixture, actor).serializeBinary();
        EXPECT_EQ(service.getProcessParalysis(reference.mFormKey), 1);
        EXPECT_TRUE(MWMechanics::oblivionParalyzed(world, actor));
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), before);
    }

    TEST(OblivionWorldTest, NativeMeleePlaybackPublishesAttackContactFollowthroughAndCompletion)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addNativeNpc(fixture, 0x801);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        auto& service = *world.getOblivionCombatService();
        const auto key = actor.getCellRef().getFormKey();
        service.initializeConstructedActorProcess(key);
        const auto id = service.beginMeleeStrike(key, ESM4::MeleeStrikeKind::Left, "handtohandattackleft", 1, {});
        auto expected = captureNativeActorState(fixture, actor);
        ASSERT_TRUE(service.bindMeleePlayback(id, key));
        expected.mNativeActorValues[0].mProcessAction = 2;
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), expected.serializeBinary());
        const auto before = expected.serializeBinary();
        EXPECT_THROW(world.commitOblivionPhysicalContact(id, actor, {},
            {std::numeric_limits<float>::quiet_NaN(), 0, 0}), std::invalid_argument);
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), before);
        ASSERT_TRUE(world.commitOblivionPhysicalContact(id, actor, {}, {-7, 0, 0}));
        EXPECT_EQ(service.getProcessAction(key), 3);
        const auto contact = captureNativeActorState(fixture, actor);
        service.restore(ESM4::RuntimeState::deserializeBinary(contact.serializeBinary()), world.getStore());
        ASSERT_TRUE(service.bindMeleePlayback(id, key));
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), contact.serializeBinary());
        EXPECT_FALSE(world.commitOblivionPhysicalContact(id, actor, {}, {-7, 0, 0}));
        ASSERT_TRUE(service.finishMeleeStrike(id, key));
        EXPECT_EQ(service.getProcessAction(key), -1);
        EXPECT_FALSE(service.bindMeleePlayback(id, key));
        EXPECT_FALSE(service.finishMeleeStrike(id, key));
    }

    TEST(OblivionWorldTest, NativeMeleePlaybackRejectsForeignBindingsAndPreservesEveryOtherRawAction)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addNativeNpc(fixture, 0x801);
        const auto peer = addNativeNpc(fixture, 0x802);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        ASSERT_TRUE(world.activateOblivionActor(peer));
        auto& service = *world.getOblivionCombatService();
        const auto key = actor.getCellRef().getFormKey();
        service.initializeConstructedActorProcess(key);
        const auto id = service.beginMeleeStrike(key, ESM4::MeleeStrikeKind::Left, "handtohandattackleft", 1, {});
        const auto snapshot = [&] {
            auto state = captureNativeActorState(fixture, actor);
            state.mReferences.push_back(captureNativeActorState(fixture, peer).mReferences.front());
            return state;
        };
        const auto source = snapshot();
        EXPECT_FALSE(service.bindMeleePlayback(id + 1, key));
        EXPECT_FALSE(service.bindMeleePlayback(id, peer.getCellRef().getFormKey()));
        EXPECT_FALSE(service.bindMeleePlayback(id, ESM::FormKey::dynamic("missing", 1)));
        EXPECT_EQ(snapshot().serializeBinary(), source.serializeBinary());
        for (int code = -32768; code <= 32767; ++code)
        {
            service.setProcessAction(key, static_cast<std::int16_t>(code));
            const bool allowed = code == -1 || code == 2 || code == 3;
            EXPECT_EQ(service.bindMeleePlayback(id, key), allowed);
            EXPECT_EQ(service.getProcessAction(key), allowed ? 2 : code);
        }
        for (int code : {-32768, 0, 2, 3, 4, 5, 6, 32767})
        {
            service.restore(source, world.getStore());
            service.setProcessAction(key, static_cast<std::int16_t>(code));
            ASSERT_TRUE(service.cancelMeleeStrike(id, key));
            EXPECT_EQ(service.getProcessAction(key), code == 2 || code == 3 ? -1 : code);
        }
    }

    TEST(OblivionWorldTest, NativeMeleePlaybackCancellationClearsOnlyAnOwnedPublishedAction)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addNativeNpc(fixture, 0x801);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        auto& service = *world.getOblivionCombatService();
        const auto key = actor.getCellRef().getFormKey();
        service.initializeConstructedActorProcess(key);
        for (int transition = 0; transition != 3; ++transition)
        {
            const auto id = service.beginMeleeStrike(key, ESM4::MeleeStrikeKind::Right, "handtohandattackright", 1, {});
            ASSERT_TRUE(service.bindMeleePlayback(id, key));
            EXPECT_EQ(service.getProcessAction(key), 2);
            if (transition == 0) { ASSERT_TRUE(service.consumeAction(id, key)); }
            if (transition == 1) { EXPECT_EQ(service.cancelActorActions(key), 1); }
            if (transition == 2) { ASSERT_TRUE(service.cancelMeleeStrike(id, key)); }
            EXPECT_EQ(service.getProcessAction(key), -1);
        }
        service.setProcessAction(key, 3);
        EXPECT_EQ(service.cancelActorActions(key), 0);
        EXPECT_EQ(service.getProcessAction(key), 3); // No owned strike: do not infer ownership.
        auto unknown = captureNativeActorState(fixture, actor);
        unknown.mNativeActorValues[0].mProcessAction.reset();
        service.restore(unknown, world.getStore());
        const auto id = service.beginMeleeStrike(key, ESM4::MeleeStrikeKind::Left, "handtohandattackleft", 1, {});
        const auto before = captureNativeActorState(fixture, actor).serializeBinary();
        EXPECT_FALSE(service.bindMeleePlayback(id, key));
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), before);
    }

    TEST(OblivionWorldTest, NativeBlockingAdmissionUsesRawPostureKnockedAndIntegerParalysis)
    {
        NativeWorldFixture fixture;
        const auto actor = addNativeNpc(fixture, 0x801);
        auto& world = fixture.mWorld;
        ASSERT_TRUE(world.activateOblivionActor(actor));
        auto& service = *world.getOblivionCombatService();
        const auto key = actor.getCellRef().getFormKey();
        const auto source = captureNativeActorState(fixture, actor);
        for (int knocked = -128; knocked <= 127; ++knocked)
            for (int action : {-32768, -1, 0, 2, 3, 6, 32767})
                for (float paralysis : {-.75f, -1.25f, 0.f, .75f, 1.25f})
                {
                    auto state = source;
                    auto& values = state.mNativeActorValues[0];
                    values.mProcessKnockedState = static_cast<std::int8_t>(knocked);
                    values.mProcessAction = static_cast<std::int16_t>(action);
                    values.mValues[48].mModifiers[1] = paralysis;
                    service.restore(state, world.getStore());
                    const bool allowed = knocked == 0 && (action == -1 || action == 6)
                        && std::abs(paralysis) < 1;
                    EXPECT_EQ(service.beginBlocking(key), allowed);
                    if (allowed)
                        state.mNativeActorValues[0].mProcessAction = 6;
                    EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), state.serializeBinary());
                }
    }

    TEST(OblivionWorldTest, NativeBlockingReleasePreservesForeignActionsAndOtherAuthority)
    {
        NativeWorldFixture fixture;
        const auto actor = addNativeNpc(fixture, 0x801);
        auto& world = fixture.mWorld;
        ASSERT_TRUE(world.activateOblivionActor(actor));
        auto& service = *world.getOblivionCombatService();
        const auto key = actor.getCellRef().getFormKey();
        const auto source = captureNativeActorState(fixture, actor);
        for (int action = -32768; action <= 32767; ++action)
        {
            service.setProcessAction(key, static_cast<std::int16_t>(action));
            EXPECT_EQ(service.endBlocking(key), action == 6);
            EXPECT_EQ(service.getProcessAction(key), action == 6 ? -1 : action);
        }
        service.restore(source, world.getStore());
        service.initializeConstructedActorProcess(key);
        ASSERT_TRUE(service.beginBlocking(key));
        auto blocked = captureNativeActorState(fixture, actor);
        service.restore(ESM4::RuntimeState::deserializeBinary(blocked.serializeBinary()), world.getStore());
        EXPECT_EQ(service.getProcessAction(key), 6);
        ASSERT_TRUE(service.endBlocking(key));
        blocked.mNativeActorValues[0].mProcessAction = -1;
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), blocked.serializeBinary());
        EXPECT_FALSE(service.endBlocking(key));
        EXPECT_FALSE(service.endBlocking(ESM::FormKey::dynamic("missing", 1)));
    }

    TEST(OblivionWorldTest, NativeBlockingRejectsMissingLegacyLowDeadAndOwnedStrike)
    {
        NativeWorldFixture fixture;
        const auto actor = addNativeNpc(fixture, 0x801);
        auto& world = fixture.mWorld;
        ASSERT_TRUE(world.activateOblivionActor(actor));
        auto& service = *world.getOblivionCombatService();
        const auto key = actor.getCellRef().getFormKey();
        auto source = captureNativeActorState(fixture, actor);
        source.mNativeActorValues[0].mProcessKnockedState = 0;
        source.mNativeActorValues[0].mProcessAction = -1;
        EXPECT_FALSE(service.beginBlocking(ESM::FormKey::dynamic("missing", 1)));
        for (int fault = 0; fault != 4; ++fault)
        {
            auto state = source;
            if (fault == 0) state.mNativeActorValues[0].mProcessKnockedState.reset();
            if (fault == 1) state.mNativeActorValues[0].mProcessAction.reset();
            if (fault == 2)
            {
                state.mNativeActorValues[0].mProcess = ESM4::ActorValueProcess::Low;
                state.mNativeActorValues[0].mProcessKnockedState.reset();
                state.mNativeActorValues[0].mProcessAction.reset();
            }
            if (fault == 3) state.mNativeActorLife[0].mPhase = ESM4::ActorLifePhase::Dead;
            service.restore(state, world.getStore());
            EXPECT_FALSE(service.beginBlocking(key));
            EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), state.serializeBinary());
        }
        service.restore(source, world.getStore());
        const auto id = service.beginMeleeStrike(key, ESM4::MeleeStrikeKind::Left, "handtohandattackleft", 1, {});
        const auto before = captureNativeActorState(fixture, actor).serializeBinary();
        EXPECT_FALSE(service.beginBlocking(key));
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), before);
        ASSERT_TRUE(service.cancelMeleeStrike(id, key));
        ASSERT_TRUE(service.beginBlocking(key));
        EXPECT_EQ(service.getProcessAction(key), 6);
    }

    TEST(OblivionWorldTest, ConstructedActiveProcessInitializesUnknownWithoutChangingOtherAuthority)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addNativeNpc(fixture, 0x801);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        auto& service = *world.getOblivionCombatService();
        const auto key = actor.getCellRef().getFormKey();
        auto old = captureNativeActorState(fixture, actor);
        old.mVersion = 24;
        old.mNativeActorValues[0].mProcessKnockedState.reset();
        old.mNativeActorValues[0].mProcessAction.reset();
        service.restore(ESM4::RuntimeState::deserializeBinary(old.serializeBinary()), world.getStore());
        ASSERT_TRUE(world.activateOblivionActor(actor));
        EXPECT_THROW(service.getProcessAction(key), std::invalid_argument);
        EXPECT_THROW(service.getProcessKnockedState(key), std::invalid_argument);
        auto expected = captureNativeActorState(fixture, actor);
        expected.mNativeActorValues[0].mProcessAction = -1;
        expected.mNativeActorValues[0].mProcessKnockedState = 0;
        service.initializeConstructedActorProcess(key);
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), expected.serializeBinary());
        service.initializeConstructedActorProcess(key);
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), expected.serializeBinary());
        service.restore(ESM4::RuntimeState::deserializeBinary(expected.serializeBinary()), world.getStore());
        EXPECT_EQ(MWMechanics::oblivionProcessAction(world, actor), -1);
        EXPECT_EQ(MWMechanics::oblivionKnockedState(world, actor), 0);
    }

    TEST(OblivionWorldTest, ConstructedActiveProcessOverlaysAllKnownKnockedBytesAndSignedActions)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addNativeNpc(fixture, 0x801);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        auto& service = *world.getOblivionCombatService();
        const auto key = actor.getCellRef().getFormKey();
        const auto source = captureNativeActorState(fixture, actor);
        for (int knocked = -128; knocked <= 127; ++knocked)
            for (int action : {-32768, -1, 0, 2, 3, 6, 32767})
                for (int absent = 0; absent != 3; ++absent)
                {
                    auto state = source;
                    state.mNativeActorValues[0].mProcessKnockedState = static_cast<std::int8_t>(knocked);
                    state.mNativeActorValues[0].mProcessAction = static_cast<std::int16_t>(action);
                    if (absent == 1) state.mNativeActorValues[0].mProcessKnockedState.reset();
                    if (absent == 2) state.mNativeActorValues[0].mProcessAction.reset();
                    service.restore(state, world.getStore());
                    auto expected = state;
                    if (absent == 1) expected.mNativeActorValues[0].mProcessKnockedState = 0;
                    if (absent == 2) expected.mNativeActorValues[0].mProcessAction = -1;
                    service.initializeConstructedActorProcess(key);
                    ASSERT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), expected.serializeBinary())
                        << knocked << ' ' << action << ' ' << absent;
                }
    }

    TEST(OblivionWorldTest, ConstructedProcessRejectsLowOrMissingAndDoesNotInferLifeOrResources)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addNativeNpc(fixture, 0x801);
        auto& service = *world.getOblivionCombatService();
        const auto key = actor.getCellRef().getFormKey();
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(actor, ESM4::ActorValueProcess::Low));
        const auto low = captureNativeActorState(fixture, actor).serializeBinary();
        EXPECT_THROW(service.initializeConstructedActorProcess(key), std::invalid_argument);
        EXPECT_THROW(service.initializeConstructedActorProcess(ESM::FormKey::dynamic("missing", 1)), std::invalid_argument);
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), low);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        ASSERT_TRUE(world.killOblivionActor(actor, {}));
        auto dead = captureNativeActorState(fixture, actor);
        dead.mNativeActorValues[0].mProcessAction.reset();
        dead.mNativeActorValues[0].mProcessKnockedState.reset();
        service.restore(dead, world.getStore());
        auto expected = dead;
        expected.mNativeActorValues[0].mProcessAction = -1;
        expected.mNativeActorValues[0].mProcessKnockedState = 0;
        service.initializeConstructedActorProcess(key);
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), expected.serializeBinary());
        EXPECT_EQ(service.findActorLife(key)->mPhase, ESM4::ActorLifePhase::Dead);
    }

    TEST(OblivionWorldTest, NativeKnockedStateLowResetAndLegacyUnknownStayExplicit)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addNativeNpc(fixture, 0x801);
        auto& service = *world.getOblivionCombatService();
        const auto key = actor.getCellRef().getFormKey();
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(actor, ESM4::ActorValueProcess::Low));
        EXPECT_FALSE(service.findActorValues(key)->mProcessKnockedState);
        EXPECT_EQ(MWMechanics::oblivionKnockedState(world, actor), 0);
        auto before = captureNativeActorState(fixture, actor).serializeBinary();
        for (int raw = -128; raw <= 127; ++raw)
            service.setProcessKnockedState(key, static_cast<std::int8_t>(raw));
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), before);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        EXPECT_EQ(service.findActorValues(key)->mProcessKnockedState, std::optional<std::int8_t>{0});
        service.setProcessKnockedState(key, -128);
        const auto saved = captureNativeActorState(fixture, actor);
        const auto binary = saved.serializeBinary();
        service.restore(ESM4::RuntimeState::deserializeBinary(binary), world.getStore());
        EXPECT_EQ(MWMechanics::oblivionKnockedState(world, actor), -128);
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), binary);
        auto old = saved;
        old.mVersion = 24;
        old.mNativeActorValues[0].mProcessKnockedState.reset();
        old.mNativeActorValues[0].mProcessAction.reset();
        service.restore(ESM4::RuntimeState::deserializeBinary(old.serializeBinary()), world.getStore());
        EXPECT_THROW(service.getProcessKnockedState(key), std::invalid_argument);
        EXPECT_THROW(MWMechanics::oblivionKnockedState(world, actor), std::invalid_argument);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        EXPECT_FALSE(service.findActorValues(key)->mProcessKnockedState); // Same process does not invent state.
        service.setProcessKnockedState(key, 3);
        EXPECT_EQ(MWMechanics::oblivionKnockedState(world, actor), 3);
        ESM4::RuntimeState downgrade;
        downgrade.mVersion = 24;
        EXPECT_THROW(service.capture(downgrade), std::invalid_argument);
        service.resetNonPlayerForResurrection(actor);
        EXPECT_FALSE(service.findActorValues(key)->mProcessKnockedState);
        EXPECT_EQ(service.findActorValues(key)->mProcess, ESM4::ActorValueProcess::Low);
        EXPECT_EQ(MWMechanics::oblivionKnockedState(world, actor), 0);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        EXPECT_EQ(MWMechanics::oblivionKnockedState(world, actor), 0);
        const auto missing = ESM::FormKey::dynamic("missing", 1);
        EXPECT_THROW(service.getProcessKnockedState(missing), std::invalid_argument);
        EXPECT_THROW(service.setProcessKnockedState(missing, 1), std::invalid_argument);
        EXPECT_THROW(MWMechanics::oblivionKnockedState(world, {}), std::invalid_argument);
        MWWorld::World foreign(nullptr, -1, "", {}, ESM::GameProfile::Morrowind);
        EXPECT_THROW(MWMechanics::oblivionKnockedState(foreign, actor), std::invalid_argument);
    }

    TEST(OblivionWorldTest, NativeProcessActionPlayerAndCreatureBindActualAuthority)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
        MWClass::Npc::registerSelf();
        world.setupPlayer();
        const auto base = ESM::FormKey::content("Oblivion.esm", 7);
        ESM4::Npc native{};
        native.mId = {7, 1}; native.mFormKey = base; native.mIsTES4 = true; native.mData.health = 100;
        store.getWritable<ESM4::Npc>().insertStatic(native, base);
        ASSERT_TRUE(world.initializeOblivionPlayerActor());
        const auto player = world.getPlayerPtr();
        auto& service = *world.getOblivionCombatService();
        EXPECT_EQ(MWMechanics::oblivionProcessAction(world, player), -1);
        MWClass::ESM4Creature::registerSelf();
        ESM4::Creature creature{};
        creature.mId = {0x820, 0}; creature.mFormKey = ESM::FormKey::content("headless.esm", 0x820);
        creature.mAttackReach = 64; creature.mBaseConfig.tes4.levelOrOffset = 1; creature.mData.health = 99;
        store.getWritable<ESM4::Creature>().insertStatic(creature, creature.mFormKey);
        ESM4::ActorCreature reference{};
        reference.mId = {0x920, 0}; reference.mFormKey = ESM::FormKey::content("headless.esm", 0x920);
        reference.mBaseKey = creature.mFormKey;
        store.getWritable<ESM4::ActorCreature>().insertStatic(reference, reference.mFormKey);
        MWWorld::LiveCellRef<ESM4::Creature> live(reference, store.search<ESM4::Creature>(creature.mFormKey));
        auto& cell = world.getWorldModel().getDraftCell();
        const MWWorld::Ptr actor(cell.insert(&live), &cell);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        EXPECT_EQ(MWMechanics::oblivionProcessAction(world, actor), -1);
        const auto playerKey = ESM::FormKey::dynamic("player", 1);
        const auto creatureKey = actor.getCellRef().getFormKey();
        const auto originalPlayer = *service.findActorValues(playerKey);
        const auto originalCreature = *service.findActorValues(creatureKey);
        for (int raw : {-32768, -1, 0, 4, 5, 6, 32767})
        {
            service.setProcessAction(playerKey, static_cast<std::int16_t>(raw));
            service.setProcessAction(creatureKey, static_cast<std::int16_t>(-raw - 1));
            EXPECT_EQ(MWMechanics::oblivionProcessAction(world, player), raw);
            EXPECT_EQ(MWMechanics::oblivionProcessAction(world, actor), -raw - 1);
            auto expectedPlayer = originalPlayer, expectedCreature = originalCreature;
            expectedPlayer.mProcessAction = static_cast<std::int16_t>(raw);
            expectedCreature.mProcessAction = static_cast<std::int16_t>(-raw - 1);
            EXPECT_EQ(*service.findActorValues(playerKey), expectedPlayer);
            EXPECT_EQ(*service.findActorValues(creatureKey), expectedCreature);
        }
    }

    TEST(OblivionWorldTest, NativeKnockedStatePlayerAndCreatureBindActualAuthority)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
        MWClass::Npc::registerSelf();
        world.setupPlayer();
        const auto base = ESM::FormKey::content("Oblivion.esm", 7);
        ESM4::Npc native{};
        native.mId = {7, 1}; native.mFormKey = base; native.mIsTES4 = true; native.mData.health = 100;
        store.getWritable<ESM4::Npc>().insertStatic(native, base);
        ASSERT_TRUE(world.initializeOblivionPlayerActor());
        const auto player = world.getPlayerPtr();
        auto& service = *world.getOblivionCombatService();
        EXPECT_EQ(MWMechanics::oblivionKnockedState(world, player), 0);
        MWClass::ESM4Creature::registerSelf();
        ESM4::Creature creature{};
        creature.mId = {0x820, 0}; creature.mFormKey = ESM::FormKey::content("headless.esm", 0x820);
        creature.mAttackReach = 64; creature.mBaseConfig.tes4.levelOrOffset = 1; creature.mData.health = 99;
        store.getWritable<ESM4::Creature>().insertStatic(creature, creature.mFormKey);
        ESM4::ActorCreature reference{};
        reference.mId = {0x920, 0}; reference.mFormKey = ESM::FormKey::content("headless.esm", 0x920);
        reference.mBaseKey = creature.mFormKey;
        store.getWritable<ESM4::ActorCreature>().insertStatic(reference, reference.mFormKey);
        MWWorld::LiveCellRef<ESM4::Creature> live(reference, store.search<ESM4::Creature>(creature.mFormKey));
        auto& cell = world.getWorldModel().getDraftCell();
        const MWWorld::Ptr actor(cell.insert(&live), &cell);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        EXPECT_EQ(MWMechanics::oblivionKnockedState(world, actor), 0);
        const auto playerKey = ESM::FormKey::dynamic("player", 1);
        const auto creatureKey = actor.getCellRef().getFormKey();
        const auto originalPlayer = *service.findActorValues(playerKey);
        const auto originalCreature = *service.findActorValues(creatureKey);
        for (int raw : {-128, -1, 0, 1, 3, 127})
        {
            service.setProcessKnockedState(playerKey, static_cast<std::int8_t>(raw));
            service.setProcessKnockedState(creatureKey, static_cast<std::int8_t>(-raw - 1));
            EXPECT_EQ(MWMechanics::oblivionKnockedState(world, player), raw);
            EXPECT_EQ(MWMechanics::oblivionKnockedState(world, actor), -raw - 1);
            auto expectedPlayer = originalPlayer, expectedCreature = originalCreature;
            expectedPlayer.mProcessKnockedState = static_cast<std::int8_t>(raw);
            expectedCreature.mProcessKnockedState = static_cast<std::int8_t>(-raw - 1);
            EXPECT_EQ(*service.findActorValues(playerKey), expectedPlayer);
            EXPECT_EQ(*service.findActorValues(creatureKey), expectedCreature);
        }
    }

    TEST(OblivionWorldTest, NativeHandContactReadsIntegerAuthorityAndAllSignedVictimStates)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto attacker = addNativeNpc(fixture, 0x801);
        const auto victim = addNativeNpc(fixture, 0x802);
        ASSERT_TRUE(world.activateOblivionActor(attacker));
        ASSERT_TRUE(world.activateOblivionActor(victim));
        auto& service = *world.getOblivionCombatService();
        const auto set = [&](unsigned av, int value) {
            return world.executeOblivionActorValueCommand(attacker, av, ESM4::ActorValueCommand::Set,
                ESM4::ActorValueCommandSource::Script, value);
        };
        ASSERT_TRUE(set(17, 11)); ASSERT_TRUE(set(7, 50)); ASSERT_TRUE(set(0, 40)); ASSERT_TRUE(set(10, 140));
        ASSERT_TRUE(world.requestOblivionStatModifier(attacker, 17, true, .75f));
        ASSERT_TRUE(world.requestOblivionStatModifier(attacker, 17, false, .75f));
        ASSERT_EQ(service.getNonPlayerValue(attacker, 17), 11);
        ASSERT_EQ(service.getNonPlayerIntegerValue(attacker, 17), 10);
        ASSERT_EQ(service.getNonPlayerBaseValue(attacker.getCellRef().getFormKey(), 10, world.getStore()), 140);
        const auto snapshot = [&] {
            auto state = captureNativeActorState(fixture, attacker);
            state.mReferences.push_back(captureNativeActorState(fixture, victim).mReferences[0]);
            return state.serializeBinary();
        };
        unsigned settingId = 0x950;
        const auto setting = [&](const char* name, float value) {
            ESM4::GameSetting record{}; record.mId = {settingId, 0}; record.mEditorId = name; record.mData = value;
            world.getStore().getWritable<ESM4::GameSetting>().insertStatic(record,
                ESM::FormKey::content("headless.esm", settingId++));
        };
        setting("fFatigueBase", 1); setting("fHandHealthMax", 15);
        setting("fHandFatigueDamageBase", 1); setting("fHandFatigueDamageMult", .5f);
        struct Row { float current, health, fatigue; };
        // Recorded original caller values, shared with the independent oracle50.
        const Row rows[] = {
            {0, 0x1.35c28ep+0f, 0x1.9ae148p+0f},
            {70, 0x1.50a3d6p+0f, 0x1.a851ecp+0f},
            {140, 0x1.6b851ep+0f, 0x1.b5c29p+0f},
            {-14, 0x1.30624ep+0f, 0x1.983128p+0f},
        };
        for (const auto& row : rows)
        {
            service.changeNonPlayerValue(attacker, 10, ESM4::ActorValueModifier::Damage,
                row.current - service.getNonPlayerValue(attacker, 10));
            ASSERT_EQ(service.getNonPlayerValue(attacker, 10), row.current);
            for (int raw = -128; raw <= 127; ++raw)
            {
                service.setProcessKnockedState(victim.getCellRef().getFormKey(), static_cast<std::int8_t>(raw));
                const auto before = snapshot();
                const auto damage = MWMechanics::oblivionHandToHandContactDamage(world, attacker, victim);
                EXPECT_EQ(damage.mHealth, row.health);
                EXPECT_EQ(damage.mFatigue, raw == 0 ? row.fatigue : 0.f);
                EXPECT_EQ(snapshot(), before);
            }
        }
        // Winning override affects the result immediately; malformed type is
        // diagnosed without publishing actor state.
        setting("fHandHealthMin", 15);
        service.setProcessKnockedState(victim.getCellRef().getFormKey(), 0);
        EXPECT_EQ(MWMechanics::oblivionHandToHandContactDamage(world, attacker, victim).mHealth, 15);
        ESM4::GameSetting bad{}; bad.mId = {0x951, 0}; bad.mEditorId = "fHandHealthMax"; bad.mData = std::int32_t{15};
        world.getStore().getWritable<ESM4::GameSetting>().insertStatic(bad,
            ESM::FormKey::content("headless.esm", 0x951));
        const auto before = snapshot();
        try
        {
            (void)MWMechanics::oblivionHandToHandContactDamage(world, attacker, victim);
            FAIL() << "native hand damage accepted an integer Health maximum";
        }
        catch (const std::invalid_argument& error)
        {
            EXPECT_STREQ(error.what(), "incorrect native combat setting type: fHandHealthMax");
        }
        EXPECT_EQ(snapshot(), before);
    }

    TEST(OblivionWorldTest, NativeHandContactRejectsUnavailableAuthorityAndUnknownLegacyVictim)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto attacker = addNativeNpc(fixture, 0x801);
        const auto victim = addNativeNpc(fixture, 0x802);
        EXPECT_THROW(MWMechanics::oblivionHandToHandContactDamage(world, attacker, victim), std::invalid_argument);
        ASSERT_TRUE(world.activateOblivionActor(attacker));
        EXPECT_THROW(MWMechanics::oblivionHandToHandContactDamage(world, attacker, victim), std::invalid_argument);
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(victim, ESM4::ActorValueProcess::Low));
        EXPECT_GT(MWMechanics::oblivionHandToHandContactDamage(world, attacker, victim).mFatigue, 0);
        ASSERT_TRUE(world.activateOblivionActor(victim));
        auto state = captureNativeActorState(fixture, attacker);
        state.mReferences.push_back(captureNativeActorState(fixture, victim).mReferences[0]);
        for (auto& values : state.mNativeActorValues) values.mProcessKnockedState.reset();
        world.getOblivionCombatService()->restore(state, world.getStore());
        const auto snapshot = [&] {
            auto complete = captureNativeActorState(fixture, attacker);
            complete.mReferences.push_back(captureNativeActorState(fixture, victim).mReferences[0]);
            return complete.serializeBinary();
        };
        const auto before = snapshot();
        EXPECT_THROW(MWMechanics::oblivionHandToHandContactDamage(world, attacker, victim), std::invalid_argument);
        EXPECT_THROW(MWMechanics::oblivionHandToHandContactDamage(world, {}, victim), std::invalid_argument);
        EXPECT_THROW(MWMechanics::oblivionHandToHandContactDamage(world, attacker, {}), std::invalid_argument);
        MWWorld::World legacy(nullptr, -1, "", {}, ESM::GameProfile::Morrowind);
        EXPECT_THROW(MWMechanics::oblivionHandToHandContactDamage(legacy, attacker, victim), std::invalid_argument);
        EXPECT_EQ(snapshot(), before);
    }

    TEST(OblivionWorldTest, NativeHandContactBindsActualPlayerAndCreatureVictim)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
        MWClass::Npc::registerSelf();
        world.setupPlayer();
        const auto base = ESM::FormKey::content("Oblivion.esm", 7);
        ESM4::Npc native{};
        native.mId = {7, 1}; native.mFormKey = base; native.mIsTES4 = true; native.mData.health = 100;
        store.getWritable<ESM4::Npc>().insertStatic(native, base);
        ASSERT_TRUE(world.initializeOblivionPlayerActor());
        const auto player = world.getPlayerPtr();
        auto& service = *world.getOblivionCombatService();
        EXPECT_EQ(MWMechanics::oblivionKnockedState(world, player), 0);
        MWClass::ESM4Creature::registerSelf();
        ESM4::Creature creature{};
        creature.mId = {0x820, 0}; creature.mFormKey = ESM::FormKey::content("headless.esm", 0x820);
        creature.mAttackReach = 64; creature.mBaseConfig.tes4.levelOrOffset = 1; creature.mData.health = 99;
        store.getWritable<ESM4::Creature>().insertStatic(creature, creature.mFormKey);
        ESM4::ActorCreature reference{};
        reference.mId = {0x920, 0}; reference.mFormKey = ESM::FormKey::content("headless.esm", 0x920);
        reference.mBaseKey = creature.mFormKey;
        store.getWritable<ESM4::ActorCreature>().insertStatic(reference, reference.mFormKey);
        MWWorld::LiveCellRef<ESM4::Creature> live(reference, store.search<ESM4::Creature>(creature.mFormKey));
        auto& cell = world.getWorldModel().getDraftCell();
        const MWWorld::Ptr actor(cell.insert(&live), &cell);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        EXPECT_EQ(MWMechanics::oblivionKnockedState(world, actor), 0);
        for (auto [av, value] : {std::pair{17, 10}, {7, 50}, {0, 40}, {2, 30}, {3, 30}, {5, 40}})
            ASSERT_TRUE(world.executeOblivionActorValueCommand(player, av, ESM4::ActorValueCommand::Set,
                ESM4::ActorValueCommandSource::Script, value));
        ASSERT_EQ(service.getPlayerBaseValue(10), 140);
        ASSERT_EQ(service.getPlayerValue(10), 140);
        unsigned id = 0x960;
        for (auto [name, value] : {std::pair{"fFatigueBase", 1.f}, {"fHandHealthMax", 15.f},
            {"fHandFatigueDamageBase", 1.f}, {"fHandFatigueDamageMult", .5f}})
        {
            ESM4::GameSetting setting{}; setting.mId = {id, 0}; setting.mEditorId = name; setting.mData = value;
            store.getWritable<ESM4::GameSetting>().insertStatic(setting, ESM::FormKey::content("headless.esm", id++));
        }
        const auto originalPlayer = *service.findActorValues(ESM::FormKey::dynamic("player", 1));
        for (int raw = -128; raw <= 127; ++raw)
        {
            service.setProcessKnockedState(actor.getCellRef().getFormKey(), static_cast<std::int8_t>(raw));
            const auto originalCreature = *service.findActorValues(actor.getCellRef().getFormKey());
            const auto damage = MWMechanics::oblivionHandToHandContactDamage(world, player, actor);
            EXPECT_EQ(damage.mHealth, 0x1.6b851ep+0f);
            EXPECT_EQ(damage.mFatigue, raw == 0 ? 0x1.b5c29p+0f : 0.f);
            EXPECT_EQ(*service.findActorValues(ESM::FormKey::dynamic("player", 1)), originalPlayer);
            EXPECT_EQ(*service.findActorValues(actor.getCellRef().getFormKey()), originalCreature);
        }
        EXPECT_EQ(MWMechanics::oblivionHandToHandContactDamage(world, player, player).mFatigue, 0x1.b5c29p+0f);
        service.setProcessKnockedState(ESM::FormKey::dynamic("player", 1), -1);
        EXPECT_EQ(MWMechanics::oblivionHandToHandContactDamage(world, player, player).mFatigue, 0);
        EXPECT_THROW(MWMechanics::oblivionHandToHandContactDamage(world, actor, player), std::invalid_argument);
    }

    TEST(OblivionWorldTest, NativeArmorQueryReadsDefenseAuthorityWithoutProjectedItemRatings)
    {
        NativeWorldFixture fixture;
        const auto actor = addNativeNpc(fixture, 0x801);
        ASSERT_TRUE(fixture.mWorld.activateOblivionActor(actor));
        EXPECT_FLOAT_EQ(actor.getClass().getArmorRating(actor, false), 0);
        ASSERT_TRUE(fixture.mWorld.executeOblivionActorValueCommand(actor, 43,
            ESM4::ActorValueCommand::Mod, ESM4::ActorValueCommandSource::Script, 12));
        EXPECT_FLOAT_EQ(actor.getClass().getArmorRating(actor, false), 12);
        ASSERT_TRUE(fixture.mWorld.executeOblivionActorValueCommand(actor, 43,
            ESM4::ActorValueCommand::Mod, ESM4::ActorValueCommandSource::Script, -20));
        EXPECT_FLOAT_EQ(actor.getClass().getArmorRating(actor, false), -8);
    }

    TEST(OblivionWorldTest, NativeArmorQueryUsesFractionalHealthCoverageMasteryAndWinningSettings)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
        MWClass::Armor::registerSelf();
        const auto actor = addNativeNpc(fixture, 0x801);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        for (auto [value, amount] : {std::pair{std::uint8_t{7}, 50}, {std::uint8_t{27}, 50}, {std::uint8_t{18}, 50}})
            ASSERT_TRUE(world.executeOblivionActorValueCommand(actor, value, ESM4::ActorValueCommand::Set,
                ESM4::ActorValueCommandSource::Script, amount));
        const auto armor = [&](std::uint32_t id, std::uint32_t mask, bool heavy, int type, std::uint16_t rating) {
            ESM4::Armor native{};
            native.mId = {id, 0}; native.mArmorFlags = mask;
            native.mGeneralFlags = ESM4::Armor::TYPE_TES4 | (heavy ? ESM4::Armor::TES4_HeavyArmor : 0);
            native.mData.health = 100; native.mData.armor = rating;
            store.getWritable<ESM4::Armor>().insertStatic(native, ESM::FormKey::content("headless.esm", id));
            ESM::Armor projected; projected.blank(); projected.mId = ESM::RefId(native.mId);
            projected.mData.mType = type; projected.mData.mHealth = 100;
            // Deliberately incompatible projected rating: the native query must ignore it.
            projected.mData.mArmor = 60000; store.insertStatic(projected);
        };
        armor(0x940, ESM4::Armor::TES4_UpperBody | ESM4::Armor::TES4_LowerBody, false, ESM::Armor::Cuirass, 1499);
        armor(0x941, ESM4::Armor::TES4_Feet, true, ESM::Armor::Boots, 1000);
        ESM4::RuntimeInventoryItem body;
        body.mBase = ESM::FormKey::content("headless.esm", 0x940); body.mCount = 1;
        body.mCondition = 50; body.mEquippedSlots = ESM4::Armor::TES4_UpperBody | ESM4::Armor::TES4_LowerBody;
        const auto install = [&](std::vector<ESM4::RuntimeInventoryItem> items) {
            const auto prepared = MWWorld::OblivionProfileServices::prepareActorInventory(
                store, ESM::FormKeyResolver({"headless.esm"}), items);
            auto staged = MWWorld::OblivionProfileServices::stageActorInventory(prepared);
            // Headless structural publication below GUI/pointer registration;
            // actual class/inventory query, not gameplay equipment acceptance.
            actor.getClass().getInventoryStore(actor).swapPreparedContents(*staged);
        };
        install({body});
        // Full original488CB0 entry rounds4.5 upwards before aggregation.
        EXPECT_FLOAT_EQ(actor.getClass().getArmorRating(actor, false), 5.f);
        auto& inventory = actor.getClass().getInventoryStore(actor);
        auto item = inventory.getSlot(MWWorld::InventoryStore::Slot_Cuirass);
        ASSERT_NE(item, inventory.end());
        item->getCellRef().setNativeItemCondition(std::bit_cast<float>(0x42c7ffffu));
        // Full original entry rounds8.999999 to9, not a fractional aggregate.
        EXPECT_EQ(std::bit_cast<std::uint32_t>(actor.getClass().getArmorRating(actor, false)), 0x41100000u);
        item->getCellRef().setNativeItemCondition(100);
        EXPECT_FLOAT_EQ(actor.getClass().getArmorRating(actor, false), 9);
        // Float AV composition truncates after summing modifiers for armor.
        ASSERT_TRUE(world.executeOblivionActorValueCommand(actor, 27, ESM4::ActorValueCommand::Set,
            ESM4::ActorValueCommandSource::Script, 56));
        world.getOblivionCombatService()->changeNonPlayerValue(actor, 27, ESM4::ActorValueModifier::Script, .75f);
        ASSERT_TRUE(world.requestOblivionStatModifier(actor, 27, false, .75f));
        EXPECT_FLOAT_EQ(actor.getClass().getArmorRating(actor, false), 10);
        ASSERT_TRUE(world.executeOblivionActorValueCommand(actor, 27, ESM4::ActorValueCommand::Set,
            ESM4::ActorValueCommandSource::Script, 100));
        EXPECT_FLOAT_EQ(actor.getClass().getArmorRating(actor, false), 21);
        ESM4::RuntimeInventoryItem boots;
        boots.mBase = ESM::FormKey::content("headless.esm", 0x941); boots.mCount = 1;
        boots.mCondition = 100; boots.mEquippedSlots = ESM4::Armor::TES4_Feet;
        body.mCondition = 100; install({body, boots});
        EXPECT_FLOAT_EQ(actor.getClass().getArmorRating(actor, false), 20); // Heavy coverage denies Light Master.
        ESM4::GameSetting cap{}; cap.mId = {0x942, 0}; cap.mEditorId = "fMaxArmorRating"; cap.mData = 12.f;
        store.getWritable<ESM4::GameSetting>().insertStatic(cap, ESM::FormKey::content("headless.esm", 0x942));
        EXPECT_FLOAT_EQ(actor.getClass().getArmorRating(actor, false), 12);
        cap.mData = 0.f; store.getWritable<ESM4::GameSetting>().insertStatic(cap,
            ESM::FormKey::content("headless.esm", 0x942));
        EXPECT_FLOAT_EQ(actor.getClass().getArmorRating(actor, false), 20);
        body.mCondition = boots.mCondition = 0; install({body, boots});
        EXPECT_FLOAT_EQ(actor.getClass().getArmorRating(actor, false), 0);
    }

    TEST(OblivionWorldTest, NativeArmorQuerySeparatesPlayerCapCreatureDefenseAndForeignProfile)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
        MWClass::Npc::registerSelf();
        world.setupPlayer();
        const auto base = ESM::FormKey::content("Oblivion.esm", 7);
        ESM4::Npc native{};
        native.mId = {7, 1}; native.mFormKey = base; native.mIsTES4 = true; native.mData.health = 100;
        store.getWritable<ESM4::Npc>().insertStatic(native, base);
        ASSERT_TRUE(world.initializeOblivionPlayerActor());
        const auto player = world.getPlayerPtr();
        ASSERT_TRUE(world.executeOblivionActorValueCommand(player, 43, ESM4::ActorValueCommand::Mod,
            ESM4::ActorValueCommandSource::Script, 100));
        EXPECT_FLOAT_EQ(player.getClass().getArmorRating(player, false), 90);
        ASSERT_TRUE(world.executeOblivionActorValueCommand(player, 43, ESM4::ActorValueCommand::Mod,
            ESM4::ActorValueCommandSource::Script, -200));
        EXPECT_FLOAT_EQ(player.getClass().getArmorRating(player, false), -100);
        MWClass::ESM4Creature::registerSelf();
        ESM4::Creature creature{};
        creature.mId = {0x820, 0}; creature.mFormKey = ESM::FormKey::content("headless.esm", 0x820);
        creature.mAttackReach = 64;
        creature.mBaseConfig.tes4.levelOrOffset = 1; creature.mData.health = 99;
        store.getWritable<ESM4::Creature>().insertStatic(creature, creature.mFormKey);
        ESM4::ActorCreature reference{};
        reference.mId = {0x920, 0}; reference.mFormKey = ESM::FormKey::content("headless.esm", 0x920);
        reference.mBaseKey = creature.mFormKey;
        store.getWritable<ESM4::ActorCreature>().insertStatic(reference, reference.mFormKey);
        MWWorld::LiveCellRef<ESM4::Creature> live(reference, store.search<ESM4::Creature>(creature.mFormKey));
        auto& cell = world.getWorldModel().getDraftCell();
        const MWWorld::Ptr actor(cell.insert(&live), &cell);
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(actor, ESM4::ActorValueProcess::Active));
        ASSERT_TRUE(world.executeOblivionActorValueCommand(actor, 43, ESM4::ActorValueCommand::Mod,
            ESM4::ActorValueCommandSource::Script, 100));
        EXPECT_FLOAT_EQ(actor.getClass().getArmorRating(actor, false), 100); // Creature getter has no NPC total cap.
        ASSERT_TRUE(world.executeOblivionActorValueCommand(actor, 43, ESM4::ActorValueCommand::Mod,
            ESM4::ActorValueCommandSource::Script, -200));
        EXPECT_FLOAT_EQ(actor.getClass().getArmorRating(actor, false), -100);
        EXPECT_THROW(MWMechanics::oblivionArmorRating(world, {}), std::invalid_argument);
        MWWorld::World legacy(nullptr, -1, "", {}, ESM::GameProfile::Morrowind);
        EXPECT_THROW(MWMechanics::oblivionArmorRating(legacy, actor), std::invalid_argument);
    }

    TEST(OblivionWorldTest, MeleeCancellationClearsHeldAndQueuedInputWithoutSpendingResources)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addNativeNpc(fixture, 0x801), other = addNativeNpc(fixture, 0x802);
        ASSERT_TRUE(world.activateOblivionActor(actor)); ASSERT_TRUE(world.activateOblivionActor(other));
        auto& service = *world.getOblivionCombatService();
        const auto key = actor.getCellRef().getFormKey(), peer = other.getCellRef().getFormKey();
        const ESM4::RuntimeMeleeInput held{.35f, true, false, ESM4::MeleeQueuedStrike::Power};
        service.setMeleeInput(key, held); service.setMeleeInput(peer, held);
        const auto id = service.beginMeleeStrike(key, ESM4::MeleeStrikeKind::Left, "handtohandattackleft");
        const auto peerId = service.beginMeleeStrike(peer, ESM4::MeleeStrikeKind::Right, "handtohandattackright");
        const auto snapshot = [&] {
            auto state = captureNativeActorState(fixture, actor);
            state.mReferences.push_back(captureNativeActorState(fixture, other).mReferences[0]);
            return state;
        };
        const auto before = snapshot();
        EXPECT_FALSE(service.cancelMeleeStrike(peerId, key));
        EXPECT_FALSE(service.cancelMeleeStrike(id, peer));
        EXPECT_FALSE(service.cancelMeleeStrike(0, key));
        EXPECT_EQ(snapshot().serializeBinary(), before.serializeBinary());
        ASSERT_TRUE(service.cancelMeleeStrike(id, key));
        ASSERT_NE(service.findMeleeState(key), nullptr);
        EXPECT_FALSE(service.findMeleeState(key)->mStrike);
        EXPECT_EQ(service.findMeleeState(key)->mInput, ESM4::RuntimeMeleeInput{});
        EXPECT_TRUE(service.isActionConsumed(id));
        EXPECT_TRUE(service.isActionPending(peerId, peer));
        EXPECT_EQ(service.findMeleeState(peer)->mInput, held);
        const auto after = snapshot();
        EXPECT_EQ(after.mNativeActorValues, before.mNativeActorValues);
        EXPECT_EQ(after.mNativeActorLife, before.mNativeActorLife);
        EXPECT_EQ(after.mPendingDeathEvents, before.mPendingDeathEvents);
        EXPECT_EQ(after.mPhysicalActions.mNext, before.mPhysicalActions.mNext);
        service.restore(ESM4::RuntimeState::deserializeBinary(after.serializeBinary()), world.getStore());
        EXPECT_EQ(service.findMeleeState(key)->mInput, ESM4::RuntimeMeleeInput{});
        EXPECT_FALSE(service.findMeleeState(key)->mStrike);
        EXPECT_TRUE(service.isActionPending(peerId, peer));
        EXPECT_FALSE(service.cancelMeleeStrike(id, key));
        EXPECT_EQ(snapshot().serializeBinary(), after.serializeBinary());
    }

    TEST(OblivionWorldTest, MeleeCancellationAfterCommittedContactDoesNotDebitAgainOrCreateInput)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addNativeNpc(fixture, 0x801);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        auto& service = *world.getOblivionCombatService();
        const auto key = actor.getCellRef().getFormKey();
        const auto id = service.beginMeleeStrike(key, ESM4::MeleeStrikeKind::Left, "handtohandattackleft");
        ASSERT_TRUE(service.advanceOrdinaryMeleePhase(id, key, 0, .3f, {0, .2f, .6f, 1}));
        service.setMeleeInput(key, {.35f, true, false, ESM4::MeleeQueuedStrike::Power});
        ASSERT_TRUE(world.commitOblivionPhysicalContact(id, actor, {}, {-7, 0, 0}));
        ASSERT_TRUE(service.findMeleeState(key)->mStrike->mContactCommitted);
        const auto values = *service.findActorValues(key);
        const auto life = *service.findActorLife(key);
        ASSERT_TRUE(service.cancelMeleeStrike(id, key));
        EXPECT_TRUE(service.isActionConsumed(id));
        EXPECT_FALSE(service.findMeleeState(key)->mStrike);
        EXPECT_EQ(service.findMeleeState(key)->mInput, ESM4::RuntimeMeleeInput{});
        EXPECT_EQ(*service.findActorValues(key), values);
        EXPECT_EQ(*service.findActorLife(key), life);
        const auto before = captureNativeActorState(fixture, actor).serializeBinary();
        EXPECT_FALSE(service.cancelMeleeStrike(id, key));
        service.clearMeleeInput(ESM::FormKey::dynamic("missing", 1));
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), before);
        ASSERT_TRUE(world.killOblivionActor(actor, {}));
        const auto dead = captureNativeActorState(fixture, actor).serializeBinary();
        EXPECT_NO_THROW(service.clearMeleeInput(key));
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), dead);
    }

    TEST(OblivionWorldTest, OrdinaryContactGateUsesPriorPhaseAndCannotReplayAfterMiss)
    {
        NativeWorldFixture fixture;
        const auto actor = addNativeNpc(fixture, 0x801);
        ASSERT_TRUE(fixture.mWorld.activateOblivionActor(actor));
        auto& service = *fixture.mWorld.getOblivionCombatService();
        const auto key = actor.getCellRef().getFormKey();
        const std::array<float, 4> keys{0, .2f, .6f, 1};
        for (const auto kind : {ESM4::MeleeStrikeKind::Left, ESM4::MeleeStrikeKind::Right})
        {
            const auto id = service.beginMeleeStrike(key, kind, "handtohandattackleft");
            EXPECT_FALSE(service.isOrdinaryMeleeContactPending(id, key));
            ASSERT_TRUE(service.advanceOrdinaryMeleePhase(id, key, 0, .3f, keys));
            ASSERT_TRUE(service.isOrdinaryMeleeContactPending(id, key));
            EXPECT_FALSE(service.isOrdinaryMeleeContactPending(id + 1, key));
            EXPECT_FALSE(service.isOrdinaryMeleeContactPending(id, ESM::FormKey::dynamic("other", 1)));
            EXPECT_TRUE(service.isOrdinaryMeleeContactPending(id, key)); // Read-only, no spending.
            const float fatigue = service.getNonPlayerValue(actor, 10);
            ASSERT_TRUE(fixture.mWorld.commitOblivionPhysicalContact(id, actor, {}, {-7, 0, 0}));
            EXPECT_FLOAT_EQ(service.getNonPlayerValue(actor, 10), fatigue - 7);
            EXPECT_FALSE(service.isOrdinaryMeleeContactPending(id, key));
            EXPECT_FALSE(fixture.mWorld.commitOblivionPhysicalContact(id, actor, {}, {-7, 0, 0}));
            EXPECT_FLOAT_EQ(service.getNonPlayerValue(actor, 10), fatigue - 7);
            ASSERT_TRUE(service.advanceOrdinaryMeleePhase(id, key, 0, .7f, keys));
            EXPECT_EQ(service.findMeleeState(key)->mStrike->mOrdinaryPhase, ESM4::OrdinaryMeleePhase::Queue);
            EXPECT_FALSE(service.isOrdinaryMeleeContactPending(id, key));
            EXPECT_TRUE(service.finishMeleeStrike(id, key));
            EXPECT_FALSE(service.isOrdinaryMeleeContactPending(id, key));
        }
        const auto power = service.beginMeleeStrike(key, ESM4::MeleeStrikeKind::StandingPower,
            "handtohandattackpower");
        EXPECT_FALSE(service.isOrdinaryMeleeContactPending(power, key));
        ASSERT_TRUE(service.finishMeleeStrike(power, key));
        const auto queued = service.beginMeleeStrike(key, ESM4::MeleeStrikeKind::Left,
            "handtohandattackleft");
        ASSERT_TRUE(service.advanceOrdinaryMeleePhase(queued, key, 0, 2, keys));
        ASSERT_TRUE(service.advanceOrdinaryMeleePhase(queued, key, 0, 2, keys));
        EXPECT_FALSE(service.isOrdinaryMeleeContactPending(queued, key));
    }

    TEST(OblivionWorldTest, NativeOrdinaryWeaponQueryMatchesOriginalEntryOnPlayerAndNpcBindings)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
        MWClass::Npc::registerSelf(); MWClass::Weapon::registerSelf(); world.setupPlayer();
        const auto base = ESM::FormKey::content("Oblivion.esm", 7);
        ESM4::Npc nativePlayer{}; nativePlayer.mId = {7, 1}; nativePlayer.mFormKey = base;
        nativePlayer.mIsTES4 = true; nativePlayer.mData.health = 100;
        store.getWritable<ESM4::Npc>().insertStatic(nativePlayer, base);
        ASSERT_TRUE(world.initializeOblivionPlayerActor());
        const auto player = world.getPlayerPtr(), npc = addNativeNpc(fixture, 0x801);
        ASSERT_TRUE(world.activateOblivionActor(npc));
        auto& service = *world.getOblivionCombatService();
        unsigned id = 0x950;
        for (auto [name, value] : {std::pair{"fFatigueBase", 1.f}, {"fFatigueMult", .5f},
            {"fDamageWeaponMult", .5f}, {"fDamageSkillBase", .2f}, {"fDamageSkillMult", 1.5f},
            {"fDamageWeaponConditionBase", .5f}, {"fDamageWeaponConditionMult", .5f},
            {"fDamageStrengthBase", .75f}, {"fDamageStrengthMult", .5f}, {"fActorLuckSkillMult", .4f}})
        {
            ESM4::GameSetting setting{}; setting.mId = {id, 0}; setting.mEditorId = name; setting.mData = value;
            store.getWritable<ESM4::GameSetting>().insertStatic(setting, ESM::FormKey::content("headless.esm", id++));
        }
        for (unsigned type = 0; type < 4; ++type)
        {
            ESM4::Weapon native{}; native.mId = {0x940 + type, 0}; native.mData.type = type;
            native.mData.health = 100; native.mData.damage = 20; native.mData.speed = native.mData.reach = 1;
            store.getWritable<ESM4::Weapon>().insertStatic(native,
                ESM::FormKey::content("headless.esm", 0x940 + type));
            ESM::Weapon projected; projected.blank(); projected.mId = ESM::RefId(native.mId);
            projected.mData.mType = ESM::Weapon::ShortBladeOneHand;
            projected.mData.mHealth = 1; // Deliberately wrong: native definition must supply maximum/damage/type.
            projected.mData.mChop[0] = projected.mData.mChop[1] = 255;
            store.insertStatic(projected);
        }
        struct Row { unsigned type; float skill, luck, strength, condition, fatigue; int bonus; std::uint32_t health; };
        const Row rows[] = {
#include "weaponentry_expected.inc"
        };
        const auto initial = captureNativeActorState(fixture, npc);
        for (const auto actor : {player, npc})
        {
            unsigned installed = 4;
            for (const auto& row : rows)
            {
                SCOPED_TRACE(::testing::Message() << (actor == player ? "Player" : "NPC") << ','
                    << row.type << ',' << row.skill << ',' << row.luck << ',' << row.strength << ','
                    << row.condition << ',' << row.fatigue << ',' << row.bonus);
                if (row.type != installed)
                {
                    ESM4::RuntimeInventoryItem item;
                    item.mBase = ESM::FormKey::content("headless.esm", 0x940 + row.type);
                    item.mCount = 1; item.mCondition = row.condition; item.mEquippedSlots = ESM4::InventorySlotWeapon;
                    const auto prepared = MWWorld::OblivionProfileServices::prepareActorInventory(
                        store, ESM::FormKeyResolver({"headless.esm"}), {item});
                    auto staged = MWWorld::OblivionProfileServices::stageActorInventory(prepared);
                    actor.getClass().getInventoryStore(actor).swapPreparedContents(*staged);
                    installed = row.type;
                }
                auto state = initial;
                const auto key = actor == player ? ESM::FormKey::dynamic("player", 1) : actor.getCellRef().getFormKey();
                auto& values = *std::find_if(state.mNativeActorValues.begin(), state.mNativeActorValues.end(),
                    [&](const auto& x) { return x.mActor == key; });
                for (auto [av, amount] : {std::pair{0, row.strength}, {7, row.luck}, {14, row.skill}, {16, row.skill}})
                {
                    values.mValues[av] = {};
                    values.mValues[av].mBase = std::trunc(amount) - 1;
                    // Split across native modifier channels: NPC integer AV
                    // truncation differs from float composition then conversion.
                    const float part = (amount - std::trunc(amount) + 1) / 2;
                    values.mValues[av].mModifiers[0] = part;
                    values.mValues[av].mModifiers[1] = part;
                }
                values.mValues[10] = {}; values.mValues[10].mBase = 140;
                values.mValues[10].mModifiers[2] = row.fatigue - 140;
                values.mValues[42] = {};
                if (actor == npc)
                {
                    const float part = row.bonus < 0 ? -.875f : .875f;
                    values.mValues[42].mModifiers[0] = part;
                    values.mValues[42].mModifiers[1] = static_cast<float>(row.bonus) + part;
                }
                else values.mValues[42].mModifiers[1] = static_cast<float>(row.bonus);
                service.restore(state, store);
                ASSERT_EQ(actor == player ? service.getPlayerIntegerValue(42)
                    : service.getNonPlayerIntegerValue(actor, 42), row.bonus);
                ASSERT_EQ(actor == player ? service.getPlayerValue(14) : service.getNonPlayerValue(actor, 14), row.skill);
                ASSERT_EQ(actor == player ? service.getPlayerValue(7) : service.getNonPlayerValue(actor, 7), row.luck);
                ASSERT_EQ(actor == player ? service.getPlayerValue(0) : service.getNonPlayerValue(actor, 0), row.strength);
                auto& inventory = actor.getClass().getInventoryStore(actor);
                const auto weapon = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
                ASSERT_NE(weapon, inventory.end()); weapon->getCellRef().setNativeItemCondition(row.condition);
                const auto before = captureNativeActorState(fixture, npc).serializeBinary();
                EXPECT_EQ(std::bit_cast<std::uint32_t>(MWMechanics::oblivionOrdinaryWeaponContactDamage(
                    world, actor, *weapon)), row.health);
                EXPECT_EQ(captureNativeActorState(fixture, npc).serializeBinary(), before);
                EXPECT_EQ(weapon->getCellRef().getNativeItemCondition(), row.condition);
            }
        }
        const auto playerWeapon = *player.getClass().getInventoryStore(player).getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
        const auto npcWeapon = *npc.getClass().getInventoryStore(npc).getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
        EXPECT_THROW(MWMechanics::oblivionOrdinaryWeaponContactDamage(world, npc, playerWeapon), std::invalid_argument);
        EXPECT_THROW(MWMechanics::oblivionOrdinaryWeaponContactDamage(world, player, npcWeapon), std::invalid_argument);
        EXPECT_THROW(MWMechanics::oblivionOrdinaryWeaponContactDamage(world, {}, npcWeapon), std::invalid_argument);
        EXPECT_THROW(MWMechanics::oblivionOrdinaryWeaponContactDamage(world, npc, {}), std::invalid_argument);
        const auto unavailable = addNativeNpc(fixture, 0x803);
        ASSERT_EQ(unavailable.getRefData().getCustomData(), nullptr);
        EXPECT_THROW(MWMechanics::oblivionOrdinaryWeaponContactDamage(world, unavailable, npcWeapon), std::invalid_argument);
        EXPECT_EQ(unavailable.getRefData().getCustomData(), nullptr);
        const auto beforeErrors = captureNativeActorState(fixture, npc).serializeBinary();
        EXPECT_THROW(MWMechanics::oblivionOrdinaryWeaponContactDamage(world, npc, npc), std::invalid_argument);
        auto invalid = *store.get<ESM4::Weapon>().search(ESM::FormId{0x943, 0});
        invalid.mData.health = 0;
        store.getWritable<ESM4::Weapon>().insertStatic(invalid, ESM::FormKey::content("headless.esm", 0x943));
        EXPECT_THROW(MWMechanics::oblivionOrdinaryWeaponContactDamage(world, npc, npcWeapon), std::invalid_argument);
        invalid.mData.health = 100; invalid.mData.type = 5;
        store.getWritable<ESM4::Weapon>().insertStatic(invalid, ESM::FormKey::content("headless.esm", 0x943));
        EXPECT_THROW(MWMechanics::oblivionOrdinaryWeaponContactDamage(world, npc, npcWeapon), std::invalid_argument);
        invalid.mData.type = 3;
        store.getWritable<ESM4::Weapon>().insertStatic(invalid, ESM::FormKey::content("headless.esm", 0x943));
        ESM4::GameSetting bad{}; bad.mId = {0x952, 0}; bad.mEditorId = "fDamageWeaponMult";
        bad.mData = std::int32_t{1};
        store.getWritable<ESM4::GameSetting>().insertStatic(bad, ESM::FormKey::content("headless.esm", 0x952));
        EXPECT_THROW(MWMechanics::oblivionOrdinaryWeaponContactDamage(world, npc, npcWeapon), std::invalid_argument);
        EXPECT_EQ(captureNativeActorState(fixture, npc).serializeBinary(), beforeErrors);
        MWWorld::World legacy(nullptr, -1, "", {}, ESM::GameProfile::Morrowind);
        EXPECT_THROW(MWMechanics::oblivionOrdinaryWeaponContactDamage(legacy, npc, npcWeapon), std::invalid_argument);
    }

    TEST(OblivionWorldTest, OrdinaryUnarmedBlockMitigatesAfterArmorAndPreparesSeparateNoviceCost)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto attacker = addNativeNpc(fixture, 0x801), victim = addNativeNpc(fixture, 0x802);
        ASSERT_TRUE(world.activateOblivionActor(attacker)); ASSERT_TRUE(world.activateOblivionActor(victim));
        auto& service = *world.getOblivionCombatService();
        unsigned id = 0x950;
        for (auto [name, value] : {std::pair{"fFatigueBase", 1.f}, {"fHandHealthMin", 20.f}, {"fHandHealthMax", 20.f},
            {"fHandFatigueDamageBase", 10.f}, {"fHandFatigueDamageMult", 0.f},
            {"fBlockSkillBase", .5f}, {"fBlockSkillMult", 0.f},
            {"fBlockAmountHandToHandMult", 1.f}, {"fFatigueBlockBase", 0.f},
            {"fFatigueBlockMult", 1.f}, {"fFatigueBlockSkillBase", 20.f},
            {"fFatigueBlockSkillMult", 0.f}})
        {
            ESM4::GameSetting setting{}; setting.mId = {id, 0}; setting.mEditorId = name; setting.mData = value;
            world.getStore().getWritable<ESM4::GameSetting>().insertStatic(setting,
                ESM::FormKey::content("headless.esm", id++));
        }
        ASSERT_TRUE(world.executeOblivionActorValueCommand(victim, 43, ESM4::ActorValueCommand::Mod,
            ESM4::ActorValueCommandSource::Script, 25));
        service.setProcessAction(victim.getCellRef().getFormKey(), 6);
        const auto sourceValues = *service.findActorValues(attacker.getCellRef().getFormKey());
        const auto targetValues = *service.findActorValues(victim.getCellRef().getFormKey());
        const auto damage = MWMechanics::resolveOblivionOrdinaryUnarmedContact(world, attacker, victim, 0, false);
        ASSERT_TRUE(damage);
        EXPECT_EQ(damage->mBlockAbsorbedFraction, .5f);
        EXPECT_EQ(damage->mHealth, 7.5f); // 20 * .75 armor remainder * .5 block remainder.
        EXPECT_EQ(damage->mFatigue, 3.75f); // 10 * 7.5/20, before difficulty.
        EXPECT_EQ(damage->mBlockFatigueDebit, 20.5f); // Separate earlier Novice writer.
        EXPECT_EQ(*service.findActorValues(attacker.getCellRef().getFormKey()), sourceValues);
        EXPECT_EQ(*service.findActorValues(victim.getCellRef().getFormKey()), targetValues);
        for (const int base : {0, 24, 25, 49, 50, 100})
        {
            ASSERT_TRUE(world.executeOblivionActorValueCommand(victim, 15, ESM4::ActorValueCommand::Set,
                ESM4::ActorValueCommandSource::Script, base));
            const auto candidate = *service.findActorValues(victim.getCellRef().getFormKey());
            const auto blocked = MWMechanics::resolveOblivionOrdinaryUnarmedContact(world, attacker, victim, 0, false);
            ASSERT_TRUE(blocked);
            EXPECT_EQ(blocked->mHealth, 7.5f); EXPECT_EQ(blocked->mFatigue, 3.75f);
            EXPECT_EQ(blocked->mBlockFatigueDebit, base < 25 ? 20.5f : 0.f);
            EXPECT_EQ(*service.findActorValues(victim.getCellRef().getFormKey()), candidate);
        }
        auto position = victim.getRefData().getPosition();
        position.rot[2] = 1; victim.getRefData().setPosition(position);
        const auto outside = MWMechanics::resolveOblivionOrdinaryUnarmedContact(world, attacker, victim, 0, false);
        ASSERT_TRUE(outside); EXPECT_EQ(outside->mHealth, 15); EXPECT_EQ(outside->mFatigue, 7.5f);
        EXPECT_EQ(outside->mBlockAbsorbedFraction, 0); EXPECT_EQ(outside->mBlockFatigueDebit, 0);
        position.rot[2] = 0; victim.getRefData().setPosition(position);
        auto state = captureNativeActorState(fixture, attacker);
        state.mReferences.push_back(captureNativeActorState(fixture, victim).mReferences[0]);
        auto& values = *std::find_if(state.mNativeActorValues.begin(), state.mNativeActorValues.end(),
            [&](const auto& x) { return x.mActor == victim.getCellRef().getFormKey(); });
        values.mValues[48].mModifiers[1] = 1.75f;
        service.restore(state, world.getStore());
        const auto paralyzed = MWMechanics::resolveOblivionOrdinaryUnarmedContact(world, attacker, victim, 0, false);
        ASSERT_TRUE(paralyzed); EXPECT_EQ(paralyzed->mHealth, 15); EXPECT_EQ(paralyzed->mFatigue, 7.5f);
        EXPECT_EQ(paralyzed->mBlockAbsorbedFraction, 0); EXPECT_EQ(paralyzed->mBlockFatigueDebit, 0);
    }

    TEST(OblivionWorldTest, OrdinaryWeaponPolicyPreparesNativeWearBeforeOwnedResourcePublication)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
        MWClass::Npc::registerSelf(); MWClass::Weapon::registerSelf(); world.setupPlayer();
        const auto base = ESM::FormKey::content("Oblivion.esm", 7);
        ESM4::Npc nativePlayer{}; nativePlayer.mId = {7, 1}; nativePlayer.mFormKey = base;
        nativePlayer.mIsTES4 = true; nativePlayer.mData.health = 100;
        store.getWritable<ESM4::Npc>().insertStatic(nativePlayer, base);
        ASSERT_TRUE(world.initializeOblivionPlayerActor());
        const auto player = world.getPlayerPtr(), npc = addNativeNpc(fixture, 0x801);
        const auto victim = addNativeNpc(fixture, 0x802);
        ASSERT_TRUE(world.activateOblivionActor(npc)); ASSERT_TRUE(world.activateOblivionActor(victim));
        auto& service = *world.getOblivionCombatService();
        unsigned id = 0x950;
        for (auto [name, value] : {std::pair{"fFatigueBase", 1.f}, {"fFatigueMult", .5f},
            {"fDamageWeaponMult", .5f}, {"fDamageSkillBase", .2f}, {"fDamageSkillMult", 1.5f},
            {"fDamageWeaponConditionBase", .5f}, {"fDamageWeaponConditionMult", .5f},
            {"fDamageStrengthBase", .75f}, {"fDamageStrengthMult", .5f}, {"fActorLuckSkillMult", .4f},
            {"fDamageToWeaponPercentage", .06f}, {"fDamageToArmorPercentage", 9.f},
            {"fArmorRatingMax", .85f}, {"fDifficultyDamageMultiplier", 5.f}})
        {
            ESM4::GameSetting setting{}; setting.mId = {id, 0}; setting.mEditorId = name; setting.mData = value;
            store.getWritable<ESM4::GameSetting>().insertStatic(setting, ESM::FormKey::content("headless.esm", id++));
        }
        ESM4::Weapon native{}; native.mId = {0x940, 0}; native.mData.health = 1000;
        native.mData.damage = 100; native.mData.speed = native.mData.reach = 1;
        const auto weaponBase = ESM::FormKey::content("headless.esm", 0x940);
        store.getWritable<ESM4::Weapon>().insertStatic(native, weaponBase);
        ESM::Weapon projected; projected.blank(); projected.mId = ESM::RefId(native.mId);
        projected.mData.mHealth = 1; projected.mData.mChop[0] = projected.mData.mChop[1] = 255;
        store.insertStatic(projected);
        const auto install = [&](const MWWorld::Ptr& owner) {
            ESM4::RuntimeInventoryItem item; item.mBase = weaponBase; item.mCount = 1;
            item.mCondition = 1000; item.mEquippedSlots = ESM4::InventorySlotWeapon;
            const auto prepared = MWWorld::OblivionProfileServices::prepareActorInventory(
                store, ESM::FormKeyResolver({"headless.esm"}), {item});
            auto staged = MWWorld::OblivionProfileServices::stageActorInventory(prepared);
            auto& inventory = owner.getClass().getInventoryStore(owner);
            inventory.swapPreparedContents(*staged);
            return *inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
        };
        auto state = captureNativeActorState(fixture, npc);
        state.mReferences.push_back(captureNativeActorState(fixture, victim).mReferences[0]);
        for (auto& values : state.mNativeActorValues)
            for (auto [av, amount] : {std::pair{0, 40.f}, {7, 50.f}, {14, 10.f}, {10, 140.f}, {42, 0.f}, {43, 0.f}})
            { values.mValues[av] = {}; values.mValues[av].mBase = amount; }
        service.restore(state, store);
        const auto source = install(npc);
        const auto snapshot = [&] {
            auto saved = captureNativeActorState(fixture, npc);
            saved.mReferences.push_back(captureNativeActorState(fixture, victim).mReferences[0]);
            return saved.serializeBinary();
        };
        const auto before = snapshot();
        const auto prepared = MWMechanics::resolveOblivionOrdinaryWeaponContact(world, npc, victim, source, 1, false);
        ASSERT_TRUE(prepared);
        EXPECT_EQ(prepared->mHealth, 16.625f); // Original sword control, NPC role ignores difficulty.
        ASSERT_TRUE(prepared->mConditionAfterWear);
        EXPECT_EQ(*prepared->mConditionAfterWear, 994.f); // Native damage100 * .06, before mitigation.
        EXPECT_EQ(snapshot(), before); EXPECT_EQ(source.getCellRef().getNativeItemCondition(), 1000.f);
        const auto attack = world.beginOblivionPhysicalAction(npc);
        MWMechanics::OblivionPhysicalContactDeltas deltas{-7, -prepared->mHealth, 0, 0, {
            MWMechanics::captureOblivionPhysicalConditionChange(npc, source, *prepared->mConditionAfterWear)}};
        ASSERT_TRUE(world.commitOblivionPhysicalContact(attack, npc, victim, deltas));
        EXPECT_EQ(source.getCellRef().getNativeItemCondition(), 994.f);
        EXPECT_EQ(service.findActorValues(victim.getCellRef().getFormKey())->mValues[8].mModifiers[2], -16.625f);
        EXPECT_EQ(service.findActorValues(npc.getCellRef().getFormKey())->mValues[10].mModifiers[2], -7.f);
        EXPECT_FALSE(world.commitOblivionPhysicalContact(attack, npc, victim, deltas));

        service.restore(state, store); source.getCellRef().setNativeItemCondition(1000);
        const auto playerSource = install(player);
        const auto out = MWMechanics::resolveOblivionOrdinaryWeaponContact(world, player, victim, playerSource, 1, false);
        ASSERT_TRUE(out); EXPECT_EQ(std::bit_cast<std::uint32_t>(out->mHealth), 1076974933u); EXPECT_EQ(out->mConditionAfterWear, 994.f);
        const auto in = MWMechanics::resolveOblivionOrdinaryWeaponContact(world, npc, player, source, 1, false);
        ASSERT_TRUE(in); EXPECT_EQ(std::bit_cast<std::uint32_t>(in->mHealth), 1120370688u); EXPECT_EQ(in->mConditionAfterWear, 994.f);
        EXPECT_FALSE(MWMechanics::resolveOblivionOrdinaryWeaponContact(world, npc, victim, source, 0, true));
        source.getCellRef().setNativeItemCondition(6);
        EXPECT_FALSE(MWMechanics::resolveOblivionOrdinaryWeaponContact(world, npc, victim, source, 0, false));
        source.getCellRef().setNativeItemCondition(1000);
        ASSERT_TRUE(world.executeOblivionActorValueCommand(victim, 43, ESM4::ActorValueCommand::Mod,
            ESM4::ActorValueCommandSource::Script, 25));
        const auto bareArmor = MWMechanics::resolveOblivionOrdinaryWeaponContact(world, npc, victim, source, 0, false);
        ASSERT_TRUE(bareArmor); EXPECT_EQ(bareArmor->mHealth, 12.46875f);
        EXPECT_TRUE(bareArmor->mArmorConditionChanges.empty());
        ASSERT_TRUE(bareArmor->mRandomTransition);
        EXPECT_EQ(bareArmor->mRandomTransition->mDraws, 7u);
        EXPECT_EQ(service.combatRandomState(), 1u); // Preparation does not advance.
        ESM4::GameSetting noArmorWear{}; noArmorWear.mId = {0x95b, 0};
        noArmorWear.mEditorId = "fDamageToArmorPercentage"; noArmorWear.mData = 0.f;
        store.getWritable<ESM4::GameSetting>().insertStatic(noArmorWear,
            ESM::FormKey::content("headless.esm", 0x95b));
        const auto noSelection = MWMechanics::resolveOblivionOrdinaryWeaponContact(world, npc, victim, source, 0, false);
        ASSERT_TRUE(noSelection); EXPECT_EQ(noSelection->mHealth, 12.46875f);
        EXPECT_EQ(noSelection->mConditionAfterWear, 994.f);
        ASSERT_TRUE(world.executeOblivionActorValueCommand(victim, 43, ESM4::ActorValueCommand::Mod,
            ESM4::ActorValueCommandSource::Script, -33));
        const auto amplified = MWMechanics::resolveOblivionOrdinaryWeaponContact(world, npc, victim, source, 0, false);
        ASSERT_TRUE(amplified); EXPECT_EQ(amplified->mHealth, 17.955f);
        EXPECT_EQ(amplified->mConditionAfterWear, 994.f);
        // The runtime rusty shortsword profile is independently frozen in
        // S4/native-weapon-runtime-oracle-01 (both original x87 words).
        source.getCellRef().setNativeItemCondition(55.875f);
        native.mData.health = 56; native.mData.damage = 5;
        store.getWritable<ESM4::Weapon>().insertStatic(native, weaponBase);
        const auto rusty = MWMechanics::resolveOblivionOrdinaryWeaponContact(world, npc, player, source, 0, false);
        ASSERT_TRUE(rusty);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(rusty->mHealth), 1062506496u);
        ASSERT_TRUE(rusty->mConditionAfterWear);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(*rusty->mConditionAfterWear), 1113476301u);
        const auto noEnchantment = native.mEnchantment;
        native.mEnchantment = {0x944, 0};
        store.getWritable<ESM4::Weapon>().insertStatic(native, weaponBase);
        EXPECT_FALSE(MWMechanics::resolveOblivionOrdinaryWeaponContact(world, npc, player, source, 0, false));
        native.mEnchantment = noEnchantment;
        store.getWritable<ESM4::Weapon>().insertStatic(native, weaponBase);
        ASSERT_TRUE(world.toggleGodMode());
        const auto god = MWMechanics::resolveOblivionOrdinaryWeaponContact(world, player, npc, playerSource, 0, false);
        ASSERT_TRUE(god); EXPECT_FALSE(god->mConditionAfterWear);
    }

    TEST(OblivionWorldTest, NativeContactPublishesPreparedConditionsAtomicallyAndOnce)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
        MWClass::Weapon::registerSelf();
        const auto attacker = addNativeNpc(fixture, 0x801), victim = addNativeNpc(fixture, 0x802);
        ASSERT_TRUE(world.activateOblivionActor(attacker)); ASSERT_TRUE(world.activateOblivionActor(victim));
        ESM4::Weapon native{}; native.mId = {0x940, 0}; native.mData.health = 100;
        native.mData.speed = native.mData.reach = 1;
        const auto base = ESM::FormKey::content("headless.esm", 0x940);
        store.getWritable<ESM4::Weapon>().insertStatic(native, base);
        ESM::Weapon projected; projected.blank(); projected.mId = ESM::RefId(native.mId);
        projected.mData.mHealth = 100; store.insertStatic(projected);
        const auto install = [&](const MWWorld::Ptr& owner, float condition) {
            ESM4::RuntimeInventoryItem item; item.mBase = base; item.mCount = 1;
            item.mCondition = condition; item.mEquippedSlots = ESM4::InventorySlotWeapon;
            const auto prepared = MWWorld::OblivionProfileServices::prepareActorInventory(
                store, ESM::FormKeyResolver({"headless.esm"}), {item});
            auto staged = MWWorld::OblivionProfileServices::stageActorInventory(prepared);
            auto& inventory = owner.getClass().getInventoryStore(owner);
            inventory.swapPreparedContents(*staged);
            return *inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
        };
        auto source = install(attacker, 125.5f), target = install(victim, 50.25f);
        auto& service = *world.getOblivionCombatService();
        const auto snapshot = [&] {
            auto state = captureNativeActorState(fixture, attacker);
            state.mReferences.push_back(captureNativeActorState(fixture, victim).mReferences[0]);
            // Headless inventory fixture: encode the equipped instances using
            // their native condition and metadata, then hydrate through the
            // production prepare/stage bridge below.
            for (unsigned i = 0; i < 2; ++i)
            {
                const auto item = i == 0 ? source : target;
                ESM4::RuntimeInventoryItem saved; saved.mBase = base;
                saved.mCount = item.getCellRef().getCount();
                saved.mCondition = *item.getCellRef().getNativeItemCondition();
                saved.mCharge = item.getCellRef().getEnchantmentCharge();
                saved.mEquippedSlots = ESM4::InventorySlotWeapon;
                state.mReferences[i].mInventory.push_back(saved);
            }
            return state.serializeBinary();
        };
        const auto id = world.beginOblivionPhysicalAction(attacker);
        MWMechanics::OblivionPhysicalContactDeltas deltas{-7, -1, -1, -1};
        deltas.mConditionChanges = {
            MWMechanics::captureOblivionPhysicalConditionChange(attacker, source, 124.25f),
            MWMechanics::captureOblivionPhysicalConditionChange(victim, target, 50.125f)};
        const auto before = snapshot();
        const auto reject = [&](const MWMechanics::OblivionPhysicalContactDeltas& bad) {
            EXPECT_THROW(world.commitOblivionPhysicalContact(id, attacker, victim, bad), std::invalid_argument);
            EXPECT_EQ(snapshot(), before);
            EXPECT_TRUE(service.isActionPending(id, attacker.getCellRef().getFormKey()));
        };
        auto bad = deltas; bad.mConditionChanges.back().mCondition = std::numeric_limits<float>::quiet_NaN();
        reject(bad);
        bad = deltas; bad.mConditionChanges.back().mExpectedCharge += 1; reject(bad);
        bad = deltas; bad.mConditionChanges.back().mExpectedRemainder += .125f; reject(bad);
        bad = deltas; bad.mConditionChanges.back().mExpectedNativeCondition = 50.5f; reject(bad);
        bad = deltas; bad.mConditionChanges.back().mOwner = attacker; reject(bad);
        bad = deltas; bad.mConditionChanges.back().mExpectedCount = 2; reject(bad);
        MWMechanics::OblivionPhysicalContactDeltas miss{-7, 0, 0, 0, deltas.mConditionChanges};
        EXPECT_THROW(world.commitOblivionPhysicalContact(id, attacker, {}, miss), std::invalid_argument);
        EXPECT_EQ(snapshot(), before);
        ASSERT_TRUE(world.commitOblivionPhysicalContact(id, attacker, victim, deltas));
        EXPECT_EQ(source.getCellRef().getNativeItemCondition(), 124.25f);
        EXPECT_EQ(target.getCellRef().getNativeItemCondition(), 50.125f);
        EXPECT_EQ(source.getCellRef().getCharge(), 125);
        EXPECT_EQ(target.getCellRef().getCharge(), 51);
        const auto committed = snapshot();
        EXPECT_FALSE(world.commitOblivionPhysicalContact(id, attacker, victim, deltas));
        EXPECT_EQ(snapshot(), committed);
        service.restore(ESM4::RuntimeState::deserializeBinary(committed), store);
        EXPECT_FALSE(world.commitOblivionPhysicalContact(id, attacker, victim, deltas));
        EXPECT_EQ(snapshot(), committed);

        // A shield can receive two separately rounded writes during one contact.
        // Later requests compare against the earlier prepared state, not the live item.
        const auto next = world.beginOblivionPhysicalAction(attacker);
        auto first = MWMechanics::captureOblivionPhysicalConditionChange(victim, target, 50.f);
        auto second = first; second.mExpectedNativeCondition = 50.f; second.mExpectedCharge = 50;
        second.mCondition = std::bit_cast<float>(0x4247ffffu);
        MWMechanics::OblivionPhysicalContactDeltas ordered{0, 0, 0, 0, {first, second}};
        const auto orderedBefore = snapshot();
        bad = ordered; bad.mConditionChanges.back().mExpectedNativeCondition = 50.125f;
        EXPECT_THROW(world.commitOblivionPhysicalContact(next, attacker, victim, bad), std::invalid_argument);
        EXPECT_EQ(snapshot(), orderedBefore);
        ASSERT_TRUE(world.commitOblivionPhysicalContact(next, attacker, victim, ordered));
        EXPECT_EQ(std::bit_cast<std::uint32_t>(*target.getCellRef().getNativeItemCondition()), 0x4247ffffu);
        const auto orderedCommitted = snapshot();
        const auto decoded = ESM4::RuntimeState::deserializeBinary(orderedCommitted);
        ASSERT_EQ(decoded.mReferences.size(), 2u);
        ASSERT_EQ(decoded.mReferences[1].mInventory.size(), 1u);
        EXPECT_EQ(decoded.mReferences[1].mInventory[0].mCondition, second.mCondition);
        for (unsigned i = 0; i < 2; ++i)
        {
            const auto owner = i == 0 ? attacker : victim;
            const auto prepared = MWWorld::OblivionProfileServices::prepareActorInventory(
                store, ESM::FormKeyResolver({"headless.esm"}), decoded.mReferences[i].mInventory);
            auto staged = MWWorld::OblivionProfileServices::stageActorInventory(prepared);
            auto& inventory = owner.getClass().getInventoryStore(owner);
            inventory.swapPreparedContents(*staged);
            (i == 0 ? source : target) = *inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
        }
        service.restore(decoded, store);
        MWMechanics::OblivionPhysicalContactDeltas replay{0, 0, 0, 0, {
            MWMechanics::captureOblivionPhysicalConditionChange(victim, target, 49.f)}};
        EXPECT_FALSE(world.commitOblivionPhysicalContact(next, attacker, victim, replay));
        EXPECT_EQ(snapshot(), orderedCommitted);
    }

    TEST(OblivionWorldTest, NativeContactConditionAdmissionAndPlayerGodMode)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
        MWClass::Npc::registerSelf(); MWClass::Armor::registerSelf(); world.setupPlayer();
        const auto playerBase = ESM::FormKey::content("Oblivion.esm", 7);
        ESM4::Npc nativePlayer{}; nativePlayer.mId = {7, 1}; nativePlayer.mFormKey = playerBase;
        nativePlayer.mIsTES4 = true; nativePlayer.mData.health = 100;
        store.getWritable<ESM4::Npc>().insertStatic(nativePlayer, playerBase);
        ASSERT_TRUE(world.initializeOblivionPlayerActor());
        const auto player = world.getPlayerPtr(), victim = addNativeNpc(fixture, 0x802);
        const auto stranger = addNativeNpc(fixture, 0x803);
        ASSERT_TRUE(world.activateOblivionActor(victim));
        ESM4::Armor native{}; native.mId = {0x941, 0}; native.mData.health = 100;
        native.mArmorFlags = ESM4::Armor::TES4_UpperBody; native.mGeneralFlags = ESM4::Armor::TYPE_TES4;
        const auto base = ESM::FormKey::content("headless.esm", 0x941);
        store.getWritable<ESM4::Armor>().insertStatic(native, base);
        ESM::Armor projected; projected.blank(); projected.mId = ESM::RefId(native.mId);
        projected.mData.mType = ESM::Armor::Cuirass; projected.mData.mHealth = 100;
        store.insertStatic(projected);
        const auto install = [&](const MWWorld::Ptr& owner) {
            ESM4::RuntimeInventoryItem item; item.mBase = base; item.mCount = 1;
            item.mCondition = 80.25f; item.mEquippedSlots = ESM4::Armor::TES4_UpperBody;
            const auto prepared = MWWorld::OblivionProfileServices::prepareActorInventory(
                store, ESM::FormKeyResolver({"headless.esm"}), {item});
            auto staged = MWWorld::OblivionProfileServices::stageActorInventory(prepared);
            auto& inventory = owner.getClass().getInventoryStore(owner);
            inventory.swapPreparedContents(*staged);
            return *inventory.getSlot(MWWorld::InventoryStore::Slot_Cuirass);
        };
        const auto source = install(player), target = install(victim);
        auto& service = *world.getOblivionCombatService();
        const auto id = world.beginOblivionPhysicalAction(player);
        MWMechanics::OblivionPhysicalContactDeltas deltas{-7, -1, 0, 0, {
            MWMechanics::captureOblivionPhysicalConditionChange(player, source, 79.125f),
            MWMechanics::captureOblivionPhysicalConditionChange(victim, target, 79.125f)}};
        const auto before = captureNativeActorState(fixture, victim).serializeBinary();
        auto bad = deltas; bad.mConditionChanges.back().mOwner = stranger;
        ASSERT_FALSE(stranger.getRefData().getCustomData());
        EXPECT_THROW(world.commitOblivionPhysicalContact(id, player, victim, bad), std::invalid_argument);
        EXPECT_FALSE(stranger.getRefData().getCustomData());
        EXPECT_EQ(captureNativeActorState(fixture, victim).serializeBinary(), before);
        target.getCellRef().setCount(2);
        bad = deltas; bad.mConditionChanges.back().mExpectedCount = 2;
        EXPECT_THROW(world.commitOblivionPhysicalContact(id, player, victim, bad), std::invalid_argument);
        target.getCellRef().setCount(1);
        ASSERT_TRUE(world.toggleGodMode());
        ASSERT_TRUE(world.commitOblivionPhysicalContact(id, player, victim, deltas));
        EXPECT_EQ(source.getCellRef().getNativeItemCondition(), 80.25f);
        EXPECT_EQ(target.getCellRef().getNativeItemCondition(), 79.125f);
        EXPECT_EQ(service.findActorValues(ESM::FormKey::dynamic("player", 1))->mValues[10].mModifiers[2], 0);
        EXPECT_EQ(service.findActorValues(victim.getCellRef().getFormKey())->mValues[8].mModifiers[2], -1);
    }

    TEST(OblivionWorldTest, NativePhysicalBlockCostKeepsSeparateFloatStoresAndAtomicRejection)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto attacker = addNativeNpc(fixture, 0x801), victim = addNativeNpc(fixture, 0x802);
        ASSERT_TRUE(world.activateOblivionActor(attacker)); ASSERT_TRUE(world.activateOblivionActor(victim));
        auto& service = *world.getOblivionCombatService();
        const auto snapshot = [&] {
            auto state = captureNativeActorState(fixture, attacker);
            state.mReferences.push_back(captureNativeActorState(fixture, victim).mReferences[0]);
            return state.serializeBinary();
        };
        service.changeNonPlayerValue(victim, 10, ESM4::ActorValueModifier::Damage, -16777216.f);
        const auto id = world.beginOblivionPhysicalAction(attacker);
        const auto before = snapshot();
        EXPECT_THROW(world.commitOblivionPhysicalContact(id, attacker, {}, {0, 0, 0, -1}), std::invalid_argument);
        EXPECT_EQ(snapshot(), before);
        EXPECT_THROW(world.commitOblivionPhysicalContact(id, attacker, victim,
            {0, 0, 0, std::numeric_limits<float>::quiet_NaN()}), std::invalid_argument);
        EXPECT_EQ(snapshot(), before);
        ASSERT_TRUE(world.commitOblivionPhysicalContact(id, attacker, victim, {-7, -1, -1, -1}));
        // Native Damage stores -16777216-1 twice: each ties back to -16777216.
        // Combining the two debits would incorrectly publish -16777218.
        EXPECT_EQ(service.findActorValues(victim.getCellRef().getFormKey())->mValues[10].mModifiers[2], -16777216.f);
        const auto committed = snapshot();
        EXPECT_FALSE(world.commitOblivionPhysicalContact(id, attacker, victim, {-7, -1, -1, -1}));
        EXPECT_EQ(snapshot(), committed);
        service.restore(ESM4::RuntimeState::deserializeBinary(committed), world.getStore());
        EXPECT_FALSE(world.commitOblivionPhysicalContact(id, attacker, victim, {-7, -1, -1, -1}));
        EXPECT_EQ(snapshot(), committed);
    }

    TEST(OblivionWorldTest, OrdinaryUnarmedVictimPolicyMatchesOriginalMitigationAndIdentityCorpus)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        MWClass::Npc::registerSelf(); world.setupPlayer();
        const auto base = ESM::FormKey::content("Oblivion.esm", 7);
        ESM4::Npc native{}; native.mId = {7, 1}; native.mFormKey = base; native.mIsTES4 = true;
        native.mData.health = 100;
        world.getStore().getWritable<ESM4::Npc>().insertStatic(native, base);
        ASSERT_TRUE(world.initializeOblivionPlayerActor());
        const auto player = world.getPlayerPtr();
        const auto source = addNativeNpc(fixture, 0x801), target = addNativeNpc(fixture, 0x802);
        ASSERT_TRUE(world.activateOblivionActor(source)); ASSERT_TRUE(world.activateOblivionActor(target));
        auto& service = *world.getOblivionCombatService();
        unsigned id = 0x950;
        for (auto [name, value] : {std::pair{"fHandHealthMin", 20.f}, {"fHandHealthMax", 20.f},
            {"fHandFatigueDamageBase", 10.f}, {"fHandFatigueDamageMult", 0.f},
            {"fArmorRatingMax", .85f}, {"fDifficultyDamageMultiplier", 5.f}})
        {
            ESM4::GameSetting setting{}; setting.mId = {id, 0}; setting.mEditorId = name; setting.mData = value;
            world.getStore().getWritable<ESM4::GameSetting>().insertStatic(setting,
                ESM::FormKey::content("headless.esm", id++));
        }
        struct Row { int role; float difficulty; int rating; std::uint32_t health, fatigue; };
        const Row rows[] = {
#include "unarmedmitigation_expected.inc"
        };
        const auto snapshot = [&] {
            auto state = captureNativeActorState(fixture, source);
            state.mReferences.push_back(captureNativeActorState(fixture, target).mReferences[0]);
            return state.serializeBinary();
        };
        for (const auto& row : rows)
        {
            SCOPED_TRACE(::testing::Message() << row.role << ',' << row.difficulty << ',' << row.rating);
            const auto attacker = row.role == 1 || row.role == 3 ? player : source;
            const auto victim = row.role >= 2 ? player : target;
            const float old = victim == player ? service.getPlayerValue(43) : service.getNonPlayerValue(victim, 43);
            ASSERT_TRUE(world.executeOblivionActorValueCommand(victim, 43, ESM4::ActorValueCommand::Mod,
                ESM4::ActorValueCommandSource::Script, row.rating - static_cast<int>(old)));
            const auto before = snapshot();
            const auto damage = MWMechanics::resolveOblivionOrdinaryUnarmedContact(world,
                attacker, victim, row.difficulty, false);
            ASSERT_TRUE(damage);
            EXPECT_EQ(std::bit_cast<std::uint32_t>(damage->mHealth), row.health);
            EXPECT_EQ(std::bit_cast<std::uint32_t>(damage->mFatigue), row.fatigue);
            EXPECT_EQ(snapshot(), before);
        }
        const auto before = snapshot();
        EXPECT_FALSE(MWMechanics::resolveOblivionOrdinaryUnarmedContact(world, source, target, 0, true));
        EXPECT_FALSE(MWMechanics::resolveOblivionOrdinaryUnarmedContact(world, {}, target, 0, false));
        EXPECT_FALSE(MWMechanics::resolveOblivionOrdinaryUnarmedContact(world, source, {}, 0, false));
        EXPECT_THROW(MWMechanics::resolveOblivionOrdinaryUnarmedContact(world, source, target, 1.01f, false),
            std::invalid_argument);
        EXPECT_THROW(MWMechanics::resolveOblivionOrdinaryUnarmedContact(world, source, target,
            std::numeric_limits<float>::quiet_NaN(), false), std::invalid_argument);
        EXPECT_EQ(snapshot(), before);
        const auto key = target.getCellRef().getFormKey();
        service.setProcessAction(key, 6);
        const auto blocking = snapshot();
        EXPECT_TRUE(MWMechanics::resolveOblivionOrdinaryUnarmedContact(world, source, target, 0, false));
        EXPECT_EQ(snapshot(), blocking);
        service.setProcessAction(key, -1); service.setProcessKnockedState(key, 1);
        const auto knocked = snapshot();
        EXPECT_FALSE(MWMechanics::resolveOblivionOrdinaryUnarmedContact(world, source, target, 0, false));
        EXPECT_EQ(snapshot(), knocked);
        service.setProcessKnockedState(key, 0);
        ASSERT_TRUE(world.executeOblivionActorValueCommand(target, 65, ESM4::ActorValueCommand::Mod,
            ESM4::ActorValueCommandSource::Script, 100));
        const auto immune = snapshot();
        EXPECT_FALSE(MWMechanics::resolveOblivionOrdinaryUnarmedContact(world, source, target, 0, false));
        EXPECT_EQ(snapshot(), immune);
        EXPECT_EQ(MWMechanics::oblivionNormalizedDifficulty(-500), -1);
        EXPECT_EQ(MWMechanics::oblivionNormalizedDifficulty(500), 1);
        EXPECT_EQ(MWMechanics::oblivionNormalizedDifficulty(0), 0);
        EXPECT_EQ(MWMechanics::oblivionNormalizedDifficulty(20), .2f);
        EXPECT_EQ(MWMechanics::oblivionNormalizedDifficulty(-50), -.5f);
    }

    TEST(OblivionWorldTest, OrdinaryUnarmedVictimPolicyUsesNativeArmorAndCommitsOnce)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto attacker = addNativeNpc(fixture, 0x801);
        const auto victim = addNativeNpc(fixture, 0x802);
        ASSERT_TRUE(world.activateOblivionActor(attacker));
        ASSERT_TRUE(world.activateOblivionActor(victim));
        auto& service = *world.getOblivionCombatService();
        unsigned id = 0x950;
        for (auto [name, value] : {std::pair{"fHandHealthMin", 20.f}, {"fHandHealthMax", 20.f},
            {"fHandFatigueDamageBase", 10.f}, {"fHandFatigueDamageMult", 0.f},
            {"fArmorRatingMax", .85f}, {"fDifficultyDamageMultiplier", 5.f}})
        {
            ESM4::GameSetting setting{}; setting.mId = {id, 0}; setting.mEditorId = name; setting.mData = value;
            world.getStore().getWritable<ESM4::GameSetting>().insertStatic(setting,
                ESM::FormKey::content("headless.esm", id++));
        }
        ASSERT_TRUE(world.executeOblivionActorValueCommand(victim, 43, ESM4::ActorValueCommand::Mod,
            ESM4::ActorValueCommandSource::Script, 25));
        const auto snapshot = [&] {
            auto state = captureNativeActorState(fixture, attacker);
            state.mReferences.push_back(captureNativeActorState(fixture, victim).mReferences[0]);
            return state;
        };
        const auto key = attacker.getCellRef().getFormKey();
        const auto strike = service.beginMeleeStrike(key, ESM4::MeleeStrikeKind::Left, "handtohandattackleft");
        ASSERT_TRUE(service.bindMeleePlayback(strike, key));
        ASSERT_TRUE(service.advanceOrdinaryMeleePhase(strike, key, 0, .3f, {0, .2f, .6f, 1}));
        const auto before = snapshot().serializeBinary();
        const auto damage = MWMechanics::resolveOblivionOrdinaryUnarmedContact(world, attacker, victim, 1, false);
        ASSERT_TRUE(damage);
        EXPECT_EQ(damage->mHealth, 15); // 20*(1-.25), NPC-to-NPC ignores difficulty.
        EXPECT_EQ(damage->mFatigue, 7.5f); // 10*(15/20), before health-only difficulty.
        EXPECT_EQ(snapshot().serializeBinary(), before);
        const float health = service.getNonPlayerValue(victim, 8);
        const float fatigue = service.getNonPlayerValue(victim, 10);
        const float attackerFatigue = service.getNonPlayerValue(attacker, 10);
        ASSERT_TRUE(world.commitOblivionPhysicalContact(strike, attacker, victim,
            {-7, -damage->mHealth, -damage->mFatigue}));
        EXPECT_EQ(service.getNonPlayerValue(victim, 8), health - 15);
        EXPECT_EQ(service.getNonPlayerValue(victim, 10), fatigue - 7.5f);
        EXPECT_EQ(service.getNonPlayerValue(attacker, 10), attackerFatigue - 7);
        EXPECT_EQ(service.getProcessAction(key), 3);
        const auto committed = snapshot().serializeBinary();
        EXPECT_FALSE(world.commitOblivionPhysicalContact(strike, attacker, victim, {-7, -15, -7.5f}));
        EXPECT_EQ(snapshot().serializeBinary(), committed);
        service.restore(ESM4::RuntimeState::deserializeBinary(committed), world.getStore());
        EXPECT_FALSE(world.commitOblivionPhysicalContact(strike, attacker, victim, {-7, -15, -7.5f}));
        EXPECT_EQ(snapshot().serializeBinary(), committed);
        ASSERT_TRUE(service.finishMeleeStrike(strike, key));
        EXPECT_EQ(service.getProcessAction(key), -1);
    }

    TEST(OblivionWorldTest, NativeMeleeQueryCannotConvertUnavailablePhysicsIntoContactOrMiss)
    {
        NativeWorldFixture fixture;
        const auto actor = addNativeNpc(fixture, 0x801);
        const auto target = addNativeNpc(fixture, 0x802);
        ASSERT_TRUE(fixture.mWorld.activateOblivionActor(actor));
        ASSERT_TRUE(fixture.mWorld.activateOblivionActor(target));
        auto& world = fixture.mWorld;
        const auto id = world.beginOblivionPhysicalAction(actor);
        const auto unrelated = world.beginOblivionPhysicalAction(target);
        const auto snapshot = [&] {
            auto state = captureNativeActorState(fixture, actor);
            state.mReferences.push_back(captureNativeActorState(fixture, target).mReferences[0]);
            return state.serializeBinary();
        };
        const auto before = snapshot();
        EXPECT_FALSE(MWMechanics::acquireOblivionMeleeContact(world, id, actor, target, 64));
        EXPECT_FALSE(MWMechanics::acquireOblivionMeleeContact(world, id, actor, {}, 64));
        EXPECT_FALSE(MWMechanics::acquireOblivionMeleeContact(world, id, target, actor, 64));
        EXPECT_FALSE(MWMechanics::acquireOblivionMeleeContact(world, id, {}, target, 64));
        EXPECT_THROW(MWMechanics::acquireOblivionMeleeContact(world, id, actor, target, -1), std::invalid_argument);
        EXPECT_THROW(MWMechanics::acquireOblivionMeleeContact(world, id, actor, target,
            std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);
        EXPECT_EQ(snapshot(), before);
        auto& service = *world.getOblivionCombatService();
        const auto strike = service.beginMeleeStrike(actor.getCellRef().getFormKey(),
            ESM4::MeleeStrikeKind::Left, "handtohandattackleft");
        ASSERT_TRUE(service.advanceOrdinaryMeleePhase(strike, actor.getCellRef().getFormKey(),
            0, .3f, {0, .2f, .6f, 1}));
        const auto pending = snapshot();
        EXPECT_FALSE(MWMechanics::commitOblivionOrdinaryMeleeContact(world, strike, actor, {}, 64, 0, 0, false));
        EXPECT_EQ(snapshot(), pending);
        EXPECT_TRUE(service.isOrdinaryMeleeContactPending(strike, actor.getCellRef().getFormKey()));
        ASSERT_TRUE(service.finishMeleeStrike(strike, actor.getCellRef().getFormKey()));
        EXPECT_TRUE(world.getOblivionCombatService()->isActionPending(id, actor.getCellRef().getFormKey()));
        EXPECT_TRUE(world.getOblivionCombatService()->isActionPending(unrelated, target.getCellRef().getFormKey()));
        ASSERT_TRUE(world.cancelOblivionPhysicalAction(id, actor));
        EXPECT_FALSE(MWMechanics::acquireOblivionMeleeContact(world, id, actor, target, 64));
        MWWorld::World legacy(nullptr, -1, "", {}, ESM::GameProfile::Morrowind);
        EXPECT_FALSE(MWMechanics::acquireOblivionMeleeContact(legacy, 1, actor, target, 64));
    }

    TEST(OblivionWorldTest, NativeMeleeAcquisitionSettingsUseCompiledSlopeDefaultAndWinningOverrides)
    {
        const auto defaults = MWMechanics::resolveOblivionMeleeAcquisitionSettings({});
        EXPECT_FLOAT_EQ(defaults.mConeDegrees, 20);
        EXPECT_FLOAT_EQ(defaults.mSlopeDifference, 48);
        ESM4::GameSetting slope{}, cone{};
        slope.mEditorId = "FAICOMBATSLOPEDIFFERENCE";
        slope.mData = 1.5f;
        cone.mEditorId = "fCombatHitConeAngle";
        cone.mData = 30.f;
        std::array<const ESM4::GameSetting*, 2> settings{&slope, &cone};
        auto actual = MWMechanics::resolveOblivionMeleeAcquisitionSettings(settings);
        EXPECT_FLOAT_EQ(actual.mSlopeDifference, 1.5f);
        EXPECT_FLOAT_EQ(actual.mConeDegrees, 30);
        slope.mData = std::int32_t{48};
        EXPECT_THROW(MWMechanics::resolveOblivionMeleeAcquisitionSettings(settings), std::invalid_argument);
        slope.mData = -1.f;
        EXPECT_THROW(MWMechanics::resolveOblivionMeleeAcquisitionSettings(settings), std::invalid_argument);
        slope.mData = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(MWMechanics::resolveOblivionMeleeAcquisitionSettings(settings), std::invalid_argument);
        slope.mData = 0.f;
        actual = MWMechanics::resolveOblivionMeleeAcquisitionSettings(settings);
        EXPECT_FLOAT_EQ(actual.mSlopeDifference, 0);
        auto duplicate = slope;
        duplicate.mEditorId = "fAICombatSlopeDifference";
        settings[1] = &duplicate;
        EXPECT_THROW(MWMechanics::resolveOblivionMeleeAcquisitionSettings(settings), std::invalid_argument);
    }
    TEST(OblivionWorld, BareMeleeKeysUsePlayingGroupAndWinningSource)
    {
        NativeWorldFixture fixture;
        MWClass::Npc::registerSelf();
        fixture.mWorld.setupPlayer();
        ASSERT_FALSE(fixture.mWorld.getPlayerPtr().isEmpty());
        const auto bytes = [](const auto& value) {
            return std::string(reinterpret_cast<const char*>(&value), sizeof(value));
        };
        const auto string = [&](std::string_view value) {
            return bytes(static_cast<std::uint32_t>(value.size())) + std::string(value);
        };
        const auto writeKeys = [&](const std::string& file, const std::string& group, float hit, float queue) {
            std::string data = "NetImmerse File Format, Version 4.0.0.2\n";
            data += bytes(std::uint32_t{0x04000002}) + bytes(std::uint32_t{5});
            data += string("NiSequenceStreamHelper") + string("SyntheticMelee")
                + bytes(std::int32_t{1}) + bytes(std::int32_t{3});
            data += string("NiTextKeyExtraData") + bytes(std::int32_t{2}) + bytes(std::uint32_t{0});
            const std::vector<std::pair<float, std::string>> keys = hit < 0
                ? std::vector<std::pair<float, std::string>>{{0, group + ": start"}, {1, group + ": stop"}}
                : std::vector<std::pair<float, std::string>>{{0, group + ": start"}, {hit, "hit"},
                    {queue, "a:r"}, {1, group + ": stop"}};
            data += bytes(static_cast<std::uint32_t>(keys.size()));
            for (const auto& [time, text] : keys)
                data += bytes(time) + string(text);
            data += string("NiStringExtraData") + bytes(std::int32_t{-1}) + bytes(std::uint32_t{0}) + string("Bip01");
            data += string("NiKeyframeController") + bytes(std::int32_t{-1}) + bytes(std::uint16_t{8})
                + bytes(1.f) + bytes(0.f) + bytes(0.f) + bytes(1.f) + bytes(std::int32_t{-1}) + bytes(std::int32_t{4});
            data += string("NiKeyframeData") + bytes(std::uint32_t{0}) + bytes(std::uint32_t{0}) + bytes(std::uint32_t{0});
            data += bytes(std::uint32_t{1}) + bytes(std::int32_t{0});
            std::ofstream stream(fixture.mDirectory / file, std::ios::binary);
            stream.write(data.data(), data.size());
            if (!stream)
                throw std::runtime_error("failed to write synthetic melee keyframe");
        };
        writeKeys("left.kf", "handtohandattackleft", .2f, .6f);
        writeKeys("right.kf", "handtohandattackright", .4f, .8f);
        writeKeys("override.kf", "handtohandattackleft", -1, -1);
        writeKeys("handtohandattackpower.kf", "handtohandattackpower", .3f, .7f);
        fixture.mVfs.addArchive(std::make_unique<VFS::FileSystemArchive>(fixture.mDirectory));
        fixture.mVfs.buildIndex();
        class KeyAnimation : public MWRender::Animation
        {
        public:
            using MWRender::Animation::Animation;
            void lazyDirectory() { addAnimDirectory(VFS::Path::Normalized("")); }
            void source(std::string_view path)
            {
                if (!mObjectRoot)
                {
                    mObjectRoot = new osg::Group;
                    osg::ref_ptr<osg::MatrixTransform> bone = new osg::MatrixTransform;
                    bone->setName("Bip01");
                    mObjectRoot->addChild(bone);
                }
                if (!addSingleAnimSource(VFS::Path::Normalized(path), "synthetic", {}, false))
                    throw std::runtime_error("synthetic melee source failed to load");
            }
        };
        osg::ref_ptr<KeyAnimation> animation = new KeyAnimation(
            fixture.mWorld.getPlayerPtr(), new osg::Group, &fixture.mResources);
        animation->source("left.kf");
        animation->source("right.kf");
        EXPECT_FLOAT_EQ(animation->getTextKeyTime("hit"), .4f);
        EXPECT_EQ(animation->getControllerSequenceMetadata("handtohandattackleft"), nullptr);
        EXPECT_EQ(animation->getControllerSequenceMetadata("missing"), nullptr);
        animation->lazyDirectory();
        ASSERT_TRUE(animation->hasAnimation("handtohandattackpower"));
        EXPECT_EQ(animation->getTextKeyTime("handtohandattackpower: start"), -1);
        EXPECT_FLOAT_EQ(animation->getTextKeyTimeInGroup("handtohandattackpower", "handtohandattackpower: start"), 0);
        EXPECT_FLOAT_EQ(animation->getTextKeyTimeInGroup("handtohandattackpower", "hit"), .3f);
        EXPECT_FLOAT_EQ(animation->getTextKeyTimeInGroup("handtohandattackleft", "hit"), .2f);
        EXPECT_FLOAT_EQ(animation->getTextKeyTimeInGroup("handtohandattackright", "a:"), .8f);
        EXPECT_EQ(animation->getTextKeyTimeInGroup("missing", "hit"), -1);
        EXPECT_EQ(animation->getTextKeyTimeInGroup("handtohandattackleft", "missing"), -1);
        animation->play("handtohandattackleft", MWRender::AnimPriority(1), MWRender::BlendMask_All,
            false, 1.25f, "start", "stop", .65f, 0);
        EXPECT_FLOAT_EQ(animation->getCurrentTime("handtohandattackleft"), .65f);
        EXPECT_FLOAT_EQ(animation->getTextKeyTimeInGroup("handtohandattackleft", "a:"), .6f);
        animation->source("override.kf");
        EXPECT_FLOAT_EQ(animation->getTextKeyTimeInGroup("handtohandattackleft", "hit"), .2f);
        animation->disable("handtohandattackleft");
        EXPECT_EQ(animation->getTextKeyTimeInGroup("handtohandattackleft", "hit"), -1);
        EXPECT_FLOAT_EQ(animation->getTextKeyTimeInGroup("handtohandattackright", "hit"), .4f);
    }

    TEST(OblivionWorldTest, MeleeInputResolvesActualWinningNativeSettingsWithoutCachingOrLegacyAliases)
    {
        NativeWorldFixture fixture;
        auto& store = fixture.mWorld.getStore();
        const auto initial = MWWorld::resolveOblivionMeleeInputSettings(store);
        EXPECT_FLOAT_EQ(initial.mPowerAttackDelay, .3f);
        ESM4::GameSetting delay{}, apprentice{}, journeyman{};
        delay.mId = {0x3333, 0};
        delay.mEditorId = "fPowerAttackDelay";
        delay.mData = -.1f;
        apprentice.mId = {0x3334, 0};
        apprentice.mEditorId = "iSkillApprenticeMin";
        apprentice.mData = std::int32_t{5};
        journeyman.mId = {0x3335, 0};
        journeyman.mEditorId = "iSkillJourneymanMin";
        journeyman.mData = std::int32_t{10};
        for (const auto& value : {delay, apprentice, journeyman})
            store.getWritable<ESM4::GameSetting>().insertStatic(value);
        const auto first = MWWorld::resolveOblivionMeleeInputSettings(store);
        EXPECT_FLOAT_EQ(first.mPowerAttackDelay, -.1f);
        EXPECT_EQ(first.mMastery.mMinimumSkill, (std::array<std::int32_t, 4>{5, 10, 75, 100}));
        EXPECT_TRUE(ESM4::heldPowerAttackAllowed(10, false, true, first));
        EXPECT_FALSE(ESM4::heldPowerAttackAllowed(9, false, true, first));
        delay.mEditorId = "FPOWERATTACKDELAY";
        delay.mData = .5f;
        store.getWritable<ESM4::GameSetting>().insertStatic(delay);
        EXPECT_FLOAT_EQ(MWWorld::resolveOblivionMeleeInputSettings(store).mPowerAttackDelay, .5f);
        EXPECT_FLOAT_EQ(first.mPowerAttackDelay, -.1f);
        EXPECT_FLOAT_EQ(initial.mPowerAttackDelay, .3f);
        delay.mData = std::int32_t{1};
        store.getWritable<ESM4::GameSetting>().insertStatic(delay);
        EXPECT_THROW(MWWorld::resolveOblivionMeleeInputSettings(store), std::invalid_argument);
        delay.mData = .2f;
        store.getWritable<ESM4::GameSetting>().insertStatic(delay);
        apprentice.mData = std::int32_t{20};
        store.getWritable<ESM4::GameSetting>().insertStatic(apprentice);
        EXPECT_THROW(MWWorld::resolveOblivionMeleeInputSettings(store), std::invalid_argument);
    }

}

namespace
{
    TEST(OblivionWorldTest, AnimationSoundResolvesWinningNativeFormIdAndRenamedOverrides)
    {
        NativeWorldFixture fixture;
        auto& store = fixture.mWorld.getStore();
        ESM4::Sound sound{};
        sound.mId = {0x3340, 0};
        sound.mEditorId = "NativeSwing";
        sound.mSoundFile = "fx/original.wav";
        store.getWritable<ESM4::Sound>().insertStatic(sound);
        const auto* first = MWSound::resolveNativeAnimationSound(store, "nAtIvEsWiNg");
        ASSERT_NE(first, nullptr);
        EXPECT_EQ(first->mId, sound.mId);
        EXPECT_EQ(first->mSoundFile, "fx/original.wav");
        sound.mSoundFile = "fx/winning.wav";
        store.getWritable<ESM4::Sound>().insertStatic(sound);
        const auto* winner = MWSound::resolveNativeAnimationSound(store, "NativeSwing");
        ASSERT_NE(winner, nullptr);
        EXPECT_EQ(winner, store.get<ESM4::Sound>().search(sound.mId));
        EXPECT_EQ(winner->mSoundFile, "fx/winning.wav");
        sound.mEditorId = "RenamedSwing";
        store.getWritable<ESM4::Sound>().insertStatic(sound);
        EXPECT_EQ(MWSound::resolveNativeAnimationSound(store, "NativeSwing"), nullptr);
        ASSERT_NE(MWSound::resolveNativeAnimationSound(store, "RENAMEDSWING"), nullptr);
    }

    TEST(OblivionWorldTest, AnimationSoundRejectsMissingAndAmbiguousNativeEditorIds)
    {
        NativeWorldFixture fixture;
        auto& store = fixture.mWorld.getStore();
        ESM::Sound legacy{};
        legacy.mId = ESM::RefId::stringRefId("LegacySwing");
        legacy.mSound = "legacy.wav";
        store.getWritable<ESM::Sound>().insertStatic(legacy);
        EXPECT_EQ(MWSound::resolveNativeAnimationSound(store, "LegacySwing"), nullptr);
        EXPECT_EQ(MWSound::resolveNativeAnimationSound(store, "Player"), nullptr);
        EXPECT_EQ(MWSound::resolveNativeAnimationSound(store, "missing"), nullptr);
        EXPECT_EQ(MWSound::resolveNativeAnimationSound(store, ""), nullptr);
        EXPECT_EQ(MWSound::resolveNativeAnimationSound(store, std::string_view("sound\0tail", 10)), nullptr);
        ESM4::Sound sound{};
        sound.mId = {0x3341, 0}; sound.mEditorId = "DuplicateSwing";
        store.getWritable<ESM4::Sound>().insertStatic(sound);
        sound.mId = {0x3342, 0}; sound.mEditorId = "duplicateswing";
        store.getWritable<ESM4::Sound>().insertStatic(sound);
        EXPECT_THROW(MWSound::resolveNativeAnimationSound(store, "DuplicateSwing"), std::invalid_argument);
    }
}

namespace
{
    TEST(OblivionWorldTest, CharacterTextKeysDispatchNativeSoundFormIdsWithCaseInsensitivePrefix)
    {
        NativeWorldFixture fixture;
        MWClass::Npc::registerSelf();
        fixture.mWorld.setupPlayer();
        ESM4::Sound sound{};
        sound.mId = {0x3343, 0}; sound.mEditorId = "NativeTextKeySwing";
        fixture.mWorld.getStore().getWritable<ESM4::Sound>().insertStatic(sound);
        struct ObservedSoundManager : MWSound::SoundManager
        {
            using MWSound::SoundManager::SoundManager;
            std::vector<ESM::RefId> mPlayed;
            MWSound::Sound* playSound3D(const MWWorld::ConstPtr& actor, const ESM::RefId& id,
                float volume, float pitch, MWSound::Type type, MWSound::PlayMode mode, float offset) override
            {
                EXPECT_FALSE(actor.isEmpty());
                EXPECT_EQ(volume, 1); EXPECT_EQ(pitch, 1);
                EXPECT_EQ(type, MWSound::Type::Sfx); EXPECT_EQ(mode, MWSound::PlayMode::Normal);
                EXPECT_EQ(offset, 0);
                mPlayed.push_back(id);
                return nullptr;
            }
        };
        ObservedSoundManager observed(&fixture.mVfs, false);
        fixture.mEnvironment.setSoundManager(observed);
        // The headless fixture has no physics system. An animated native
        // activator exercises the same profile/text-key dispatcher without
        // invoking actor swimming/recoil during controller construction.
        MWClass::ESM4Activator::registerSelf();
        ESM4::Activator base{};
        base.mId = {0x3350, 0};
        ESM4::Reference reference{};
        reference.mId = {0x3351, 0};
        reference.mFormKey = ESM::FormKey::content("headless.esm", 0x3351);
        reference.mBaseObj = base.mId;
        reference.mBaseKey = ESM::FormKey::content("headless.esm", 0x3350);
        MWWorld::LiveCellRef<ESM4::Activator> live(reference, &base);
        MWWorld::Ptr animated(&live);
        osg::ref_ptr<MWRender::Animation> animation = new MWRender::Animation(
            animated, new osg::Group, &fixture.mResources);
        {
            MWMechanics::CharacterController controller(animated, *animation);
            SceneUtil::TextKeyMap keys;
            keys.emplace(0.f, "Sound: NATIVEtextKEYswing");
            controller.handleTextKey("handtohandattackleft", keys.begin(), keys);
            ASSERT_EQ(observed.mPlayed.size(), 1);
            EXPECT_EQ(observed.mPlayed.back(), ESM::RefId(sound.mId));
            keys = SceneUtil::TextKeyMap{}; keys.emplace(0.f, "sound: NativeTextKeySwing");
            controller.handleTextKey("handtohandattackright", keys.begin(), keys);
            ASSERT_EQ(observed.mPlayed.size(), 2);
            keys = SceneUtil::TextKeyMap{}; keys.emplace(0.f, "Sound: missing");
            controller.handleTextKey("handtohandattackright", keys.begin(), keys);
            EXPECT_EQ(observed.mPlayed.size(), 2);
        }
        fixture.mEnvironment.setSoundManager(*fixture.mSoundManager);
    }
}

namespace
{
    TEST(OblivionWorld, ControllerSequenceMetadataUsesActualParserPlayingWinnerAndAliasCopy)
    {
        NativeWorldFixture fixture;
        MWClass::Npc::registerSelf();
        fixture.mWorld.setupPlayer();
        const auto bytes = [](const auto& value) {
            return std::string(reinterpret_cast<const char*>(&value), sizeof(value));
        };
        const auto string = [&](std::string_view value) {
            return bytes(static_cast<std::uint32_t>(value.size())) + std::string(value);
        };
        const auto writeNative = [&](std::string_view path, float begin, float hit, float frequency,
                                     std::string_view rawHit, bool duplicate = false) {
            std::string data = "Gamebryo File Format, Version 20.0.0.5\n";
            data += bytes(std::uint32_t{0x14000005}) + bytes(std::uint8_t{1}) + bytes(std::uint32_t{10})
                + bytes(std::uint32_t{duplicate ? 5u : 4u}) + bytes(std::uint32_t{0});
            data += std::string(3, '\0'); // Three empty export strings.
            data += bytes(std::uint16_t{4});
            for (const auto type : {"NiControllerSequence", "NiTextKeyExtraData", "NiTransformInterpolator", "NiStringPalette"})
                data += string(type);
            for (std::uint16_t i = 0; i < 4; ++i) data += bytes(i);
            if (duplicate) data += bytes(std::uint16_t{0});
            data += bytes(std::uint32_t{0}); // No groups.
            const auto sequenceBegin = data.size();
            data += string("InternalAttackLabel") + bytes(std::uint32_t{1}) + bytes(std::uint32_t{1});
            data += bytes(std::int32_t{2}) + bytes(std::int32_t{-1}); // Interpolator/controller.
            data += bytes(std::int32_t{3}) + bytes(std::uint32_t{0}); // Palette/Bip01 name.
            for (unsigned i = 0; i < 4; ++i) data += bytes(std::uint32_t{0xffffffff});
            data += bytes(1.f) + bytes(std::int32_t{1}) + bytes(std::uint32_t{2}) + bytes(frequency)
                + bytes(begin) + bytes(begin + 2) + bytes(std::int32_t{-1}) + string("Bip01") + bytes(std::int32_t{3});
            const auto sequence = data.substr(sequenceBegin);
            data += string("") + bytes(std::uint32_t{4});
            for (const auto& [time, key] : std::vector<std::pair<float, std::string>>{
                     {begin, "Start"}, {hit, std::string(rawHit)}, {begin+1, "a:R"}, {begin+2, "End"}})
                data += bytes(time) + string(key);
            // Default transform, no keyed data.
            for (float value : {0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f}) data += bytes(value);
            data += bytes(std::int32_t{-1});
            const std::string palette("Bip01\0", 6);
            data += string(palette) + bytes(std::uint32_t{6});
            if (duplicate) data += sequence;
            data += bytes(std::uint32_t{1}) + bytes(std::int32_t{0});
            const auto file = fixture.mDirectory / path;
            std::filesystem::create_directories(file.parent_path());
            std::ofstream out(file, std::ios::binary); out.write(data.data(), data.size());
            if (!out) throw std::runtime_error("failed to write native controller-sequence fixture");
        };
        const auto writeKeys = [&](const std::string& file, const std::string& group, float hit, float queue) {
            std::string data = "NetImmerse File Format, Version 4.0.0.2\n";
            data += bytes(std::uint32_t{0x04000002}) + bytes(std::uint32_t{5});
            data += string("NiSequenceStreamHelper") + string("SyntheticMelee")
                + bytes(std::int32_t{1}) + bytes(std::int32_t{3});
            data += string("NiTextKeyExtraData") + bytes(std::int32_t{2}) + bytes(std::uint32_t{0});
            const std::vector<std::pair<float, std::string>> keys = hit < 0
                ? std::vector<std::pair<float, std::string>>{{0, group + ": start"}, {1, group + ": stop"}}
                : std::vector<std::pair<float, std::string>>{{0, group + ": start"}, {hit, "hit"},
                    {queue, "a:r"}, {1, group + ": stop"}};
            data += bytes(static_cast<std::uint32_t>(keys.size()));
            for (const auto& [time, text] : keys)
                data += bytes(time) + string(text);
            data += string("NiStringExtraData") + bytes(std::int32_t{-1}) + bytes(std::uint32_t{0}) + string("Bip01");
            data += string("NiKeyframeController") + bytes(std::int32_t{-1}) + bytes(std::uint16_t{8})
                + bytes(1.f) + bytes(0.f) + bytes(0.f) + bytes(1.f) + bytes(std::int32_t{-1}) + bytes(std::int32_t{4});
            data += string("NiKeyframeData") + bytes(std::uint32_t{0}) + bytes(std::uint32_t{0}) + bytes(std::uint32_t{0});
            data += bytes(std::uint32_t{1}) + bytes(std::int32_t{0});
            const auto path = fixture.mDirectory / file;
            std::filesystem::create_directories(path.parent_path());
            std::ofstream stream(path, std::ios::binary);
            stream.write(data.data(), data.size());
            if (!stream)
                throw std::runtime_error("failed to write synthetic melee keyframe");
        };
        writeNative("handtohandattackleft.kf", 10, 10.25f, 1.25f, "HiT");
        writeNative("override/handtohandattackleft.kf", 20, 20.5f, .75f, " Hit ");
        writeNative("duplicate/handtohandattackleft.kf", 30, 30.25f, 1, "Hit", true);
        writeNative("exact/handtohandattackleft.kf", 0, .00007f, 1, "Hit");
        writeKeys("legacy/handtohandattackleft.kf", "handtohandattackleft", .2f, .6f);
        fixture.mVfs.addArchive(std::make_unique<VFS::FileSystemArchive>(fixture.mDirectory));
        fixture.mVfs.buildIndex();
        class MetadataAnimation : public MWRender::Animation
        {
        public:
            using MWRender::Animation::Animation;
            void root()
            {
                if (!mObjectRoot)
                {
                    mObjectRoot = new osg::Group;
                    osg::ref_ptr<osg::MatrixTransform> bone = new osg::MatrixTransform;
                    bone->setName("Bip01"); mObjectRoot->addChild(bone);
                }
            }
            void lazyDirectory() { root(); addAnimDirectory(VFS::Path::Normalized("")); }
            void source(std::string_view path, std::string_view alias = {})
            {
                root();
                if (!addSingleAnimSource(VFS::Path::Normalized(path), "synthetic", alias, false))
                    throw std::runtime_error("native metadata fixture failed to load");
            }
        };
        osg::ref_ptr<MetadataAnimation> animation = new MetadataAnimation(
            fixture.mWorld.getPlayerPtr(), new osg::Group, &fixture.mResources);
        animation->source("handtohandattackleft.kf");
        const auto original = animation->getControllerSequenceMetadata("handtohandattackleft");
        ASSERT_NE(original, nullptr);
        EXPECT_EQ(original->mStartTime, 10);
        EXPECT_EQ(original->mStopTime, 12);
        EXPECT_EQ(original->mFrequency, 1.25f);
        EXPECT_EQ(original->mCycleType, 2u);
        EXPECT_EQ(original->mTimelineStart, 0);
        EXPECT_EQ(original->mTimelineStop, 2);
        EXPECT_EQ(original->mTextKeys[1], (std::pair<float, std::string>{10.25f, "HiT"}));
        const auto rawOrdinaryKeys = [](const SceneUtil::ControllerSequenceMetadata& metadata) {
            std::vector<ESM4::MeleeTextKey> keys;
            for (const auto& [time, text] : metadata.mTextKeys) keys.push_back({time, text});
            return ESM4::ordinaryMeleeKeyTimes(keys);
        };
        EXPECT_EQ(rawOrdinaryKeys(*original), (ESM4::OrdinaryMeleeKeys{{10, 10.25f, 11, 12}, 4}));
        EXPECT_FLOAT_EQ(animation->getTextKeyTimeInGroup("handtohandattackleft", "hit"), .25f);
        EXPECT_TRUE(animation->getActiveAnimationGroup(MWRender::BoneGroup_RightArm).empty());
        EXPECT_THROW(animation->getActiveAnimationGroup(static_cast<MWRender::BoneGroup>(99)), std::invalid_argument);
        animation->play("handtohandattackleft", MWRender::AnimPriority(1), MWRender::BlendMask_All,
            false, 1, "start", "stop", 0, 0);
        EXPECT_EQ(animation->getActiveAnimationGroup(MWRender::BoneGroup_RightArm), "handtohandattackleft");
        animation->source("override/handtohandattackleft.kf");
        EXPECT_EQ(animation->getControllerSequenceMetadata("handtohandattackleft"), original);
        animation->disable("handtohandattackleft");
        const auto winning = animation->getControllerSequenceMetadata("handtohandattackleft");
        ASSERT_NE(winning, nullptr);
        EXPECT_NE(winning, original);
        EXPECT_EQ(winning->mStartTime, 20);
        EXPECT_EQ(winning->mFrequency, .75f);
        EXPECT_EQ(winning->mTextKeys[1], (std::pair<float, std::string>{20.5f, " Hit "}));
        EXPECT_EQ(rawOrdinaryKeys(*winning), (ESM4::OrdinaryMeleeKeys{{20, 0, 0, 0}, 1}));
        animation->source("handtohandattackleft.kf", "aliasattack");
        const auto alias = animation->getControllerSequenceMetadata("aliasattack");
        ASSERT_NE(alias, nullptr);
        EXPECT_EQ(alias->mGroup, "aliasattack");
        EXPECT_EQ(alias->mTextKeys, original->mTextKeys);
        EXPECT_EQ(alias->mTimelineStart, original->mTimelineStart);
        EXPECT_EQ(alias->mTimelineStop, original->mTimelineStop);
        EXPECT_EQ(alias->mCycleType, original->mCycleType);
        EXPECT_EQ(original->mGroup, "handtohandattackleft"); // Resource metadata remains immutable.
        animation->play("aliasattack", MWRender::AnimPriority(2), MWRender::BlendMask_LeftArm,
            false, 1, "start", "stop", 0, 0);
        EXPECT_EQ(animation->getActiveAnimationGroup(MWRender::BoneGroup_LeftArm), "aliasattack");
        EXPECT_TRUE(animation->getActiveAnimationGroup(MWRender::BoneGroup_RightArm).empty());
        animation->disable("aliasattack");
        EXPECT_EQ(animation->getControllerSequenceMetadata("missing"), nullptr);
        animation->play("handtohandattackleft", MWRender::AnimPriority(1), MWRender::BlendMask_All,
            false, 1, "start", "stop", 0, 0);
        animation->source("legacy/handtohandattackleft.kf");
        EXPECT_EQ(animation->getControllerSequenceMetadata("handtohandattackleft"), winning);
        animation->disable("handtohandattackleft");
        EXPECT_EQ(animation->getControllerSequenceMetadata("handtohandattackleft"), nullptr);
        // A newer source without native metadata must never borrow the older native coordinates.
        animation->source("duplicate/handtohandattackleft.kf");
        EXPECT_EQ(animation->getControllerSequenceMetadata("handtohandattackleft"), nullptr);
        EXPECT_TRUE(animation->hasAnimation("handtohandattackleft"));
        // Lazy loading must preserve the same original, unnormalized metadata.
        osg::ref_ptr<MetadataAnimation> lazy = new MetadataAnimation(
            fixture.mWorld.getPlayerPtr(), new osg::Group, &fixture.mResources);
        lazy->lazyDirectory();
        const auto lazyMetadata = lazy->getControllerSequenceMetadata("handtohandattackleft");
        ASSERT_NE(lazyMetadata, nullptr);
        EXPECT_EQ(*lazyMetadata, *original);

        // Exercise the real renderer with parser-loaded tracks and callbacks.
        // The small hit time exposes float remainder/readdition drift.
        struct Listener : MWRender::Animation::TextKeyListener
        {
            std::vector<std::string> mKeys;
            void handleTextKey(std::string_view, SceneUtil::TextKeyMap::ConstIterator key,
                const SceneUtil::TextKeyMap&) override { mKeys.push_back(key->second); }
        } listener;
        animation->source("exact/handtohandattackleft.kf");
        animation->setTextKeyListener(&listener);
        const std::string group = "handtohandattackleft";
        animation->play(group, MWRender::AnimPriority(1), MWRender::BlendMask_All,
            false, 2, "start", "stop", 0, 0);
        listener.mKeys.clear();
        EXPECT_FALSE(animation->setAnimationFrameTime("missing", .1f));
        EXPECT_TRUE(animation->setAnimationFrameTime(group, .0002f));
        EXPECT_EQ(animation->getCurrentTime(group), 0); // Publication is deferred.
        EXPECT_THROW(animation->setAnimationFrameTime(group, -.1f), std::invalid_argument);
        EXPECT_THROW(animation->setAnimationFrameTime(group, 3), std::invalid_argument);
        EXPECT_THROW(animation->setAnimationFrameTime(group,
            std::numeric_limits<float>::infinity()), std::invalid_argument);
        EXPECT_THROW(animation->setAnimationFrameTime(group,
            std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);
        animation->runAnimation(.5f); // Supplied time takes precedence over duration and speed.
        EXPECT_EQ(animation->getCurrentTime(group), .0002f);
        EXPECT_EQ(listener.mKeys, (std::vector<std::string>{"hit"}));
        EXPECT_TRUE(animation->setAnimationFrameTime(group, .0002f));
        animation->runAnimation(.5f);
        EXPECT_EQ(animation->getCurrentTime(group), .0002f);
        EXPECT_EQ(listener.mKeys, (std::vector<std::string>{"hit"}));
        animation->runAnimation(.01f); // One-call override, then shared progression resumes.
        EXPECT_EQ(animation->getCurrentTime(group), .0002f + .02f);
        EXPECT_THROW(animation->setAnimationFrameTime(group, .0002f), std::invalid_argument);
        EXPECT_TRUE(animation->setAnimationFrameTime(group, 2));
        animation->runAnimation(0);
        EXPECT_EQ(animation->getCurrentTime(group), 2);
        EXPECT_EQ(listener.mKeys, (std::vector<std::string>{"hit", "a:r", group + ": stop", "end"}));
        EXPECT_TRUE(animation->setAnimationFrameTime(group, 2));
        animation->runAnimation(.1f);
        EXPECT_EQ(listener.mKeys.size(), 4u); // The retained final pose replays no keys.
        animation->disable(group);
        EXPECT_FALSE(animation->setAnimationFrameTime(group, 0));
        animation->play(group, MWRender::AnimPriority(1), MWRender::BlendMask_All,
            false, 1, "start", "stop", 0, 1);
        EXPECT_THROW(animation->setAnimationFrameTime(group, .2f), std::invalid_argument);
        animation->setTextKeyListener(nullptr);
    }
}

namespace
{
    TEST(OblivionWorldTest, NativeInventoryHydrationAndStackCopiesRetainFractionalHealth)
    {
        NativeWorldFixture fixture;
        auto& store = fixture.mWorld.getStore();
        MWClass::Weapon::registerSelf();
        const auto key = ESM::FormKey::content("headless.esm", 0x940);
        ESM4::Weapon native{};
        native.mId = {0x940, 0};
        native.mData.health = 100;
        native.mData.speed = native.mData.reach = 1;
        store.getWritable<ESM4::Weapon>().insertStatic(native, key);
        ESM::Weapon projected;
        projected.blank();
        projected.mId = ESM::RefId(native.mId);
        projected.mData.mHealth = 100;
        store.insertStatic(projected);
        ESM4::RuntimeInventoryItem item;
        item.mBase = key;
        item.mCount = 2;
        item.mCondition = std::bit_cast<float>(0x42c7ffffu); // ceil is100, actual health is below full.
        const auto prepared = MWWorld::OblivionProfileServices::prepareActorInventory(
            store, ESM::FormKeyResolver({"headless.esm"}), {item});
        auto source = MWWorld::OblivionProfileServices::stageActorInventory(prepared);
        ASSERT_NE(source->begin(), source->end());
        const auto original = *source->begin();
        ASSERT_TRUE(original.getCellRef().getNativeItemCondition());
        EXPECT_EQ(std::bit_cast<std::uint32_t>(*original.getCellRef().getNativeItemCondition()), 0x42c7ffffu);
        EXPECT_EQ(original.getClass().getItemHealth(original), 100);
        EXPECT_EQ(original.getCellRef().getCount(), 2);
        // Headless structural test: use the production clone/stack path below
        // public add/remove presentation callbacks, which require a live GUI.
        struct CopyInventory : MWWorld::InventoryStore
        {
            CopyInventory() { readState({}); }
            MWWorld::ContainerStoreIterator copyStack(const MWWorld::ConstPtr& ptr, int count)
            { return addNewStack(ptr, count); }
        };
        CopyInventory destination;
        const auto transferred = *destination.copyStack(original, 1);
        EXPECT_EQ(original.getCellRef().getCount(), 2);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(*transferred.getCellRef().getNativeItemCondition()), 0x42c7ffffu);
        const auto another = *destination.copyStack(original, 1);
        EXPECT_FALSE(destination.stacks(transferred, another)); // Exact health, not rounded display.
        another.getCellRef().setNativeItemCondition(std::bit_cast<float>(1u));
        CopyInventory tinyStore;
        const auto tiny = *tinyStore.copyStack(another, 1);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(*tiny.getCellRef().getNativeItemCondition()), 1u);
        EXPECT_EQ(tiny.getClass().getItemHealth(tiny), 1); // Positive native health is not a broken item.
        auto bad = item;
        bad.mCondition = std::numeric_limits<double>::quiet_NaN();
        EXPECT_THROW(MWWorld::OblivionProfileServices::prepareActorInventory(
            store, ESM::FormKeyResolver({"headless.esm"}), {bad}), std::invalid_argument);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(*transferred.getCellRef().getNativeItemCondition()), 0x42c7ffffu);
    }
    TEST(OblivionWorldTest, NativeContactRandomStateIsAtomicOwnedAndRestartSafe)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto actor = addNativeNpc(fixture, 0x801), victim = addNativeNpc(fixture, 0x802);
        ASSERT_TRUE(world.activateOblivionActor(actor));
        ASSERT_TRUE(world.activateOblivionActor(victim));
        auto& service = *world.getOblivionCombatService();
        const auto snapshot = [&] {
            auto state = captureNativeActorState(fixture, actor);
            state.mReferences.push_back(captureNativeActorState(fixture, victim).mReferences[0]);
            return state;
        };
        auto initial = snapshot();
        initial.mCombatRngState = 0xffffffffu;
        initial.mAiRngState = 0x123456789abcdef0ull;
        service.restore(initial, world.getStore());
        EXPECT_EQ(service.combatRandomState(), 0xffffffffu);
        EXPECT_THROW(service.prepareCombatRandom(0), std::invalid_argument);
        EXPECT_THROW(service.prepareCombatRandom(MWMechanics::MaxPhysicalContactRandomDraws + 1), std::invalid_argument);
        const auto id = service.allocateAction(actor.getCellRef().getFormKey());
        MWMechanics::OblivionPhysicalContactDeltas deltas{-7, -1, 0};
        deltas.mRandomTransition = service.prepareCombatRandom(7);
        const auto before = snapshot().serializeBinary();
        EXPECT_EQ(service.combatRandomState(), 0xffffffffu); // preparation is read-only
        for (unsigned bad = 0; bad < 6; ++bad)
        {
            auto request = deltas;
            if (bad == 0) ++request.mRandomTransition->mExpectedState;
            if (bad == 1) ++request.mRandomTransition->mNextState;
            if (bad == 2) request.mRandomTransition->mDraws = 0;
            if (bad == 3) request.mRandomTransition->mDraws = MWMechanics::MaxPhysicalContactRandomDraws + 1;
            if (bad == 4) request.mVictimHealth = std::numeric_limits<float>::quiet_NaN();
            if (bad == 5) request.mConditionChanges.push_back({actor, {}, std::nullopt, -1, 0, 1, 1.f});
            EXPECT_THROW(world.commitOblivionPhysicalContact(id, actor, victim, request), std::invalid_argument);
            EXPECT_EQ(snapshot().serializeBinary(), before);
            EXPECT_TRUE(service.isActionPending(id, actor.getCellRef().getFormKey()));
        }
        EXPECT_THROW(world.commitOblivionPhysicalContact(id, actor, {}, deltas), std::invalid_argument);
        EXPECT_EQ(snapshot().serializeBinary(), before);
        EXPECT_FALSE(world.commitOblivionPhysicalContact(id, victim, actor, deltas));
        EXPECT_EQ(snapshot().serializeBinary(), before);
        ASSERT_TRUE(world.commitOblivionPhysicalContact(id, actor, victim, deltas));
        EXPECT_EQ(service.combatRandomState(), deltas.mRandomTransition->mNextState);
        const auto committed = snapshot();
        EXPECT_EQ(committed.mNativeActorValues[0].mValues[10].mModifiers[2], -7.f);
        EXPECT_FALSE(world.commitOblivionPhysicalContact(id, actor, victim, deltas));
        EXPECT_EQ(snapshot().serializeBinary(), committed.serializeBinary());
        service.clear();
        EXPECT_EQ(service.combatRandomState(), 1u);
        service.restore(ESM4::RuntimeState::deserializeBinary(committed.serializeBinary()), world.getStore());
        EXPECT_EQ(service.combatRandomState(), committed.mCombatRngState);
        EXPECT_FALSE(world.commitOblivionPhysicalContact(id, actor, victim, deltas));
        EXPECT_EQ(snapshot().serializeBinary(), committed.serializeBinary());
        const auto nextId = service.allocateAction(actor.getCellRef().getFormKey());
        EXPECT_THROW(world.commitOblivionPhysicalContact(nextId, actor, victim, deltas), std::invalid_argument);
        deltas.mRandomTransition = service.prepareCombatRandom(1);
        ASSERT_TRUE(world.commitOblivionPhysicalContact(nextId, actor, victim, deltas));
        auto lossy = initial;
        lossy.mCombatRngState = 1;
        lossy.mVersion = 26;
        const auto untouched = lossy.serializeBinary();
        EXPECT_THROW(service.capture(lossy), std::invalid_argument);
        EXPECT_EQ(lossy.serializeBinary(), untouched);
    }

    TEST(OblivionWorldTest, PlayerGodModePreservesArmorButPublishesOwnedSelectionRandomState)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
        MWClass::Weapon::registerSelf(); MWClass::Armor::registerSelf();
        MWClass::Npc::registerSelf(); world.setupPlayer();
        const auto playerBase = ESM::FormKey::content("Oblivion.esm", 7);
        ESM4::Npc nativePlayer{}; nativePlayer.mId = {7, 1}; nativePlayer.mFormKey = playerBase;
        nativePlayer.mIsTES4 = true; nativePlayer.mData.health = 100;
        store.getWritable<ESM4::Npc>().insertStatic(nativePlayer, playerBase);
        ASSERT_TRUE(world.initializeOblivionPlayerActor());
        const auto attacker = addNativeNpc(fixture, 0x801), victim = world.getPlayerPtr();
        ASSERT_TRUE(world.activateOblivionActor(attacker));
        auto& service = *world.getOblivionCombatService();
        unsigned id = 0x950;
        for (auto [name, value] : {std::pair{"fFatigueBase", 1.f}, {"fFatigueMult", .5f},
            {"fDamageWeaponMult", .5f}, {"fDamageSkillBase", .2f}, {"fDamageSkillMult", 1.5f},
            {"fDamageWeaponConditionBase", .5f}, {"fDamageWeaponConditionMult", .5f},
            {"fDamageStrengthBase", .75f}, {"fDamageStrengthMult", .5f}, {"fActorLuckSkillMult", .4f},
            {"fDamageToWeaponPercentage", .06f}, {"fDamageToArmorPercentage", 9.f},
            {"fArmorRatingMax", .85f}, {"fDifficultyDamageMultiplier", 5.f},
            {"fPerkHeavyArmorNoviceDamageMult", 1.5f}, {"fArmorRatingConditionMult", 0.f}})
        {
            ESM4::GameSetting setting{}; setting.mId = {id, 0}; setting.mEditorId = name; setting.mData = value;
            store.getWritable<ESM4::GameSetting>().insertStatic(setting, ESM::FormKey::content("headless.esm", id++));
        }
        ESM4::Weapon weapon{}; weapon.mId = {0x940, 0}; weapon.mData.health = 1000;
        weapon.mData.damage = 100; weapon.mData.speed = weapon.mData.reach = 1;
        const auto weaponBase = ESM::FormKey::content("headless.esm", 0x940);
        store.getWritable<ESM4::Weapon>().insertStatic(weapon, weaponBase);
        ESM::Weapon weaponView; weaponView.blank(); weaponView.mId = ESM::RefId(weapon.mId);
        weaponView.mData.mHealth = 1; weaponView.mData.mChop[0] = weaponView.mData.mChop[1] = 255;
        store.insertStatic(weaponView);
        ESM4::Armor armor{}; armor.mId = {0x941, 0}; armor.mArmorFlags = ESM4::Armor::TES4_UpperBody;
        armor.mGeneralFlags = ESM4::Armor::TYPE_TES4 | ESM4::Armor::TES4_HeavyArmor;
        armor.mData.health = 100; armor.mData.armor = 0;
        const auto armorBase = ESM::FormKey::content("headless.esm", 0x941);
        store.getWritable<ESM4::Armor>().insertStatic(armor, armorBase);
        ESM::Armor armorView; armorView.blank(); armorView.mId = ESM::RefId(armor.mId);
        armorView.mData.mType = ESM::Armor::Cuirass; armorView.mData.mHealth = 1;
        armorView.mData.mArmor = 60000; store.insertStatic(armorView);
        const auto install = [&](const MWWorld::Ptr& owner, ESM4::RuntimeInventoryItem item, int slot) {
            const auto prepared = MWWorld::OblivionProfileServices::prepareActorInventory(
                store, ESM::FormKeyResolver({"headless.esm"}), {item});
            auto staged = MWWorld::OblivionProfileServices::stageActorInventory(prepared);
            auto& inventory = owner.getClass().getInventoryStore(owner);
            inventory.swapPreparedContents(*staged);
            return *inventory.getSlot(slot);
        };
        ESM4::RuntimeInventoryItem sword; sword.mBase = weaponBase; sword.mCount = 1;
        sword.mCondition = 1000; sword.mEquippedSlots = ESM4::InventorySlotWeapon;
        ESM4::RuntimeInventoryItem body; body.mBase = armorBase; body.mCount = 1;
        body.mCondition = 100.125; body.mEquippedSlots = ESM4::Armor::TES4_UpperBody;
        auto source = install(attacker, sword, MWWorld::InventoryStore::Slot_CarriedRight);
        auto target = install(victim, body, MWWorld::InventoryStore::Slot_Cuirass);
        const auto snapshot = [&] {
            auto state = captureNativeActorState(fixture, attacker);
            sword.mCondition = *source.getCellRef().getNativeItemCondition();
            body.mCondition = *target.getCellRef().getNativeItemCondition();
            state.mReferences[0].mInventory = {sword}; state.mPlayer.mInventory = {body};
            return state;
        };
        auto initial = snapshot();
        for (auto& values : initial.mNativeActorValues)
        {
            for (auto [av, amount] : {std::pair{0, 40.f}, {7, 50.f}, {14, 10.f}, {10, 140.f}, {42, 0.f}, {43, 0.f}})
            { values.mValues[av] = {}; values.mValues[av].mBase = amount; }
            if (values.mActor == ESM::FormKey::dynamic("player", 1))
            {
                // Player Fatigue derives from Strength/Willpower/Agility/Endurance.
                for (auto [av, amount] : {std::pair{2, 30.f}, {3, 30.f}, {5, 40.f}})
                { values.mValues[av] = {}; values.mValues[av].mBase = amount; }
                values.mValues[43].mBase = 25;
                values.mValues[18] = {}; values.mValues[18].mBase = 5;
                values.mValues[18].mModifiers[1] = 95; // Current100, original base mastery5.
            }
        }
        initial.mCombatRngState = 1; service.restore(initial, store);
        // Publish through the production dynamic-base calculation before the baseline.
        service.publishPlayerValues(world.getPlayer(),
            *service.findActorValues(ESM::FormKey::dynamic("player", 1)),
            MWWorld::resolveOblivionPlayerDynamicBaseSettings(store));
        initial = snapshot();
        ASSERT_EQ(service.getPlayerValue(10), 140.f);
        EXPECT_EQ(MWMechanics::oblivionArmorRating(world, victim), 25.f);
        const auto before = snapshot().serializeBinary();
        const auto ordinary = MWMechanics::resolveOblivionOrdinaryWeaponContact(world, attacker, victim, source, 0, false);
        ASSERT_TRUE(ordinary); ASSERT_EQ(ordinary->mArmorConditionChanges.size(), 1u);
        EXPECT_EQ(ordinary->mArmorConditionChanges[0].mCondition, 44.015625f);
        EXPECT_EQ(snapshot().serializeBinary(), before);
        ASSERT_TRUE(world.toggleGodMode());
        const auto guarded = MWMechanics::resolveOblivionOrdinaryWeaponContact(world, attacker, victim, source, 0, false);
        ASSERT_TRUE(guarded); EXPECT_EQ(guarded->mHealth, 12.46875f);
        EXPECT_EQ(guarded->mConditionAfterWear, 994.f); // NPC weapon still wears.
        EXPECT_TRUE(guarded->mArmorConditionChanges.empty());
        ASSERT_TRUE(guarded->mRandomTransition);
        // Full original5FFBF9..5FFC6D/65FF10 caller probe02: draws precede god guard.
        EXPECT_EQ(guarded->mRandomTransition->mExpectedState, 1u);
        EXPECT_EQ(guarded->mRandomTransition->mNextState, 415139642u);
        EXPECT_EQ(guarded->mRandomTransition->mDraws, 3u);
        EXPECT_EQ(snapshot().serializeBinary(), before);
        const auto health = service.getPlayerValue(8), fatigue = service.getPlayerValue(10);
        const auto action = world.beginOblivionPhysicalAction(attacker);
        MWMechanics::OblivionPhysicalContactDeltas deltas{-7, -guarded->mHealth, 0, 0, {
            MWMechanics::captureOblivionPhysicalConditionChange(attacker, source, *guarded->mConditionAfterWear)}};
        deltas.mRandomTransition = guarded->mRandomTransition;
        const auto pending = snapshot().serializeBinary();
        auto bad = deltas; ++bad.mRandomTransition->mNextState;
        EXPECT_THROW(world.commitOblivionPhysicalContact(action, attacker, victim, bad), std::invalid_argument);
        EXPECT_EQ(snapshot().serializeBinary(), pending);
        ASSERT_TRUE(world.commitOblivionPhysicalContact(action, attacker, victim, deltas));
        EXPECT_EQ(service.getPlayerValue(8), health); EXPECT_EQ(service.getPlayerValue(10), fatigue);
        EXPECT_EQ(target.getCellRef().getNativeItemCondition(), 100.125f);
        EXPECT_EQ(source.getCellRef().getNativeItemCondition(), 994.f);
        EXPECT_EQ(service.findActorValues(attacker.getCellRef().getFormKey())->mValues[10].mModifiers[2], -7);
        EXPECT_EQ(service.combatRandomState(), 415139642u);
        const auto committed = snapshot().serializeBinary();
        EXPECT_FALSE(world.commitOblivionPhysicalContact(action, attacker, victim, deltas));
        EXPECT_EQ(snapshot().serializeBinary(), committed);
        const auto saved = ESM4::RuntimeState::deserializeBinary(committed);
        service.clear(); service.restore(saved, store);
        EXPECT_EQ(snapshot().serializeBinary(), committed);
        EXPECT_FALSE(world.commitOblivionPhysicalContact(action, attacker, victim, deltas));
        // God guard precedes reading/snap-to-zero, even with positive selection.
        target.getCellRef().setNativeItemCondition(.5f);
        const auto low = snapshot().serializeBinary();
        const auto belowOne = MWMechanics::resolveOblivionOrdinaryWeaponContact(world, attacker, victim, source, 0, false);
        ASSERT_TRUE(belowOne); EXPECT_TRUE(belowOne->mArmorConditionChanges.empty());
        ASSERT_TRUE(belowOne->mRandomTransition); EXPECT_EQ(snapshot().serializeBinary(), low);
    }

    TEST(OblivionWorldTest, OrdinaryWeaponArmorWearPublishesConditionsAndRandomStateOnce)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
        MWClass::Weapon::registerSelf(); MWClass::Armor::registerSelf();
        const auto attacker = addNativeNpc(fixture, 0x801), victim = addNativeNpc(fixture, 0x802);
        ASSERT_TRUE(world.activateOblivionActor(attacker)); ASSERT_TRUE(world.activateOblivionActor(victim));
        auto& service = *world.getOblivionCombatService();
        unsigned id = 0x950;
        for (auto [name, value] : {std::pair{"fFatigueBase", 1.f}, {"fFatigueMult", .5f},
            {"fDamageWeaponMult", .5f}, {"fDamageSkillBase", .2f}, {"fDamageSkillMult", 1.5f},
            {"fDamageWeaponConditionBase", .5f}, {"fDamageWeaponConditionMult", .5f},
            {"fDamageStrengthBase", .75f}, {"fDamageStrengthMult", .5f}, {"fActorLuckSkillMult", .4f},
            {"fDamageToWeaponPercentage", .06f}, {"fDamageToArmorPercentage", 9.f},
            {"fArmorRatingMax", .85f}, {"fDifficultyDamageMultiplier", 5.f},
            {"fPerkHeavyArmorNoviceDamageMult", 1.5f}, {"fArmorRatingConditionMult", 0.f}})
        {
            ESM4::GameSetting setting{}; setting.mId = {id, 0}; setting.mEditorId = name; setting.mData = value;
            store.getWritable<ESM4::GameSetting>().insertStatic(setting, ESM::FormKey::content("headless.esm", id++));
        }
        ESM4::Weapon weapon{}; weapon.mId = {0x940, 0}; weapon.mData.health = 1000;
        weapon.mData.damage = 100; weapon.mData.speed = weapon.mData.reach = 1;
        const auto weaponBase = ESM::FormKey::content("headless.esm", 0x940);
        store.getWritable<ESM4::Weapon>().insertStatic(weapon, weaponBase);
        ESM::Weapon weaponView; weaponView.blank(); weaponView.mId = ESM::RefId(weapon.mId);
        weaponView.mData.mHealth = 1; weaponView.mData.mChop[0] = weaponView.mData.mChop[1] = 255;
        store.insertStatic(weaponView);
        ESM4::Armor armor{}; armor.mId = {0x941, 0}; armor.mArmorFlags = ESM4::Armor::TES4_UpperBody;
        armor.mGeneralFlags = ESM4::Armor::TYPE_TES4 | ESM4::Armor::TES4_HeavyArmor;
        armor.mData.health = 100; armor.mData.armor = 0;
        const auto armorBase = ESM::FormKey::content("headless.esm", 0x941);
        store.getWritable<ESM4::Armor>().insertStatic(armor, armorBase);
        ESM::Armor armorView; armorView.blank(); armorView.mId = ESM::RefId(armor.mId);
        armorView.mData.mType = ESM::Armor::Cuirass; armorView.mData.mHealth = 1;
        armorView.mData.mArmor = 60000; store.insertStatic(armorView);
        const auto install = [&](const MWWorld::Ptr& owner, ESM4::RuntimeInventoryItem item, int slot) {
            const auto prepared = MWWorld::OblivionProfileServices::prepareActorInventory(
                store, ESM::FormKeyResolver({"headless.esm"}), {item});
            auto staged = MWWorld::OblivionProfileServices::stageActorInventory(prepared);
            auto& inventory = owner.getClass().getInventoryStore(owner);
            inventory.swapPreparedContents(*staged);
            return *inventory.getSlot(slot);
        };
        ESM4::RuntimeInventoryItem sword; sword.mBase = weaponBase; sword.mCount = 1;
        sword.mCondition = 1000; sword.mEquippedSlots = ESM4::InventorySlotWeapon;
        ESM4::RuntimeInventoryItem body; body.mBase = armorBase; body.mCount = 1;
        body.mCondition = 100.125; body.mEquippedSlots = ESM4::Armor::TES4_UpperBody;
        auto source = install(attacker, sword, MWWorld::InventoryStore::Slot_CarriedRight);
        auto target = install(victim, body, MWWorld::InventoryStore::Slot_Cuirass);
        const auto snapshot = [&] {
            auto state = captureNativeActorState(fixture, attacker);
            state.mReferences.push_back(captureNativeActorState(fixture, victim).mReferences[0]);
            sword.mCondition = *source.getCellRef().getNativeItemCondition();
            body.mCondition = *target.getCellRef().getNativeItemCondition();
            state.mReferences[0].mInventory = {sword}; state.mReferences[1].mInventory = {body};
            return state;
        };
        auto initial = snapshot();
        for (auto& values : initial.mNativeActorValues)
        {
            for (auto [av, amount] : {std::pair{0, 40.f}, {7, 50.f}, {14, 10.f}, {10, 140.f}, {42, 0.f}, {43, 0.f}})
            { values.mValues[av] = {}; values.mValues[av].mBase = amount; }
            if (values.mActor == victim.getCellRef().getFormKey())
            {
                values.mValues[43].mBase = 25;
                values.mValues[18] = {}; values.mValues[18].mBase = 5;
                values.mValues[18].mModifiers[1] = 95; // Current100, original base mastery5.
            }
        }
        struct Row { std::uint32_t seed, next; unsigned draws; };
        // Frozen concatenated original mitigation/selection/condition paths,
        // both x87 words: S4/native-armor-contact-oracle-01.
        for (const auto row : {Row{0, 505908858, 2}, Row{1, 415139642, 3}, Row{0x15a4, 1188163031, 1}})
        {
            initial.mCombatRngState = row.seed;
            service.restore(initial, store);
            source.getCellRef().setNativeItemCondition(1000); target.getCellRef().setNativeItemCondition(100.125f);
            EXPECT_EQ(MWMechanics::oblivionArmorRating(world, victim), 25.f);
            const auto before = snapshot().serializeBinary();
            const auto prepared = MWMechanics::resolveOblivionOrdinaryWeaponContact(world, attacker, victim, source, 0, false);
            ASSERT_TRUE(prepared);
            EXPECT_EQ(std::bit_cast<std::uint32_t>(prepared->mHealth), 1095204864u); //12.46875
            EXPECT_EQ(prepared->mConditionAfterWear, 994.f);
            ASSERT_EQ(prepared->mArmorConditionChanges.size(), 1u);
            EXPECT_EQ(prepared->mArmorConditionChanges[0].mItem, target);
            EXPECT_EQ(std::bit_cast<std::uint32_t>(prepared->mArmorConditionChanges[0].mCondition), 1110446080u); //44.015625
            ASSERT_TRUE(prepared->mRandomTransition);
            EXPECT_EQ(prepared->mRandomTransition->mExpectedState, row.seed);
            EXPECT_EQ(prepared->mRandomTransition->mNextState, row.next);
            EXPECT_EQ(prepared->mRandomTransition->mDraws, row.draws);
            EXPECT_EQ(snapshot().serializeBinary(), before);
        }
        initial.mCombatRngState = 1; service.restore(initial, store);
        const auto action = world.beginOblivionPhysicalAction(attacker);
        const auto prepared = MWMechanics::resolveOblivionOrdinaryWeaponContact(world, attacker, victim, source, 0, false);
        ASSERT_TRUE(prepared);
        MWMechanics::OblivionPhysicalContactDeltas deltas{-7, -prepared->mHealth, 0, 0, {
            MWMechanics::captureOblivionPhysicalConditionChange(attacker, source, *prepared->mConditionAfterWear)}};
        deltas.mConditionChanges.insert(deltas.mConditionChanges.end(),
            prepared->mArmorConditionChanges.begin(), prepared->mArmorConditionChanges.end());
        deltas.mRandomTransition = prepared->mRandomTransition;
        const auto pending = snapshot().serializeBinary();
        for (unsigned invalid = 0; invalid < 3; ++invalid)
        {
            auto bad = deltas;
            if (invalid == 0) bad.mConditionChanges.back().mExpectedNativeCondition = 100.25f;
            if (invalid == 1) bad.mConditionChanges.back().mCondition = std::numeric_limits<float>::quiet_NaN();
            if (invalid == 2) ++bad.mRandomTransition->mNextState;
            EXPECT_THROW(world.commitOblivionPhysicalContact(action, attacker, victim, bad), std::invalid_argument);
            EXPECT_EQ(snapshot().serializeBinary(), pending);
        }
        ASSERT_TRUE(world.commitOblivionPhysicalContact(action, attacker, victim, deltas));
        EXPECT_EQ(source.getCellRef().getNativeItemCondition(), 994.f);
        EXPECT_EQ(target.getCellRef().getNativeItemCondition(), 44.015625f);
        EXPECT_EQ(service.combatRandomState(), 415139642u);
        EXPECT_EQ(service.findActorValues(victim.getCellRef().getFormKey())->mValues[8].mModifiers[2], -12.46875f);
        EXPECT_EQ(service.findActorValues(attacker.getCellRef().getFormKey())->mValues[10].mModifiers[2], -7.f);
        const auto committed = snapshot();
        EXPECT_FALSE(world.commitOblivionPhysicalContact(action, attacker, victim, deltas));
        EXPECT_EQ(snapshot().serializeBinary(), committed.serializeBinary());
        const auto loaded = ESM4::RuntimeState::deserializeBinary(committed.serializeBinary());
        source = install(attacker, loaded.mReferences[0].mInventory[0], MWWorld::InventoryStore::Slot_CarriedRight);
        target = install(victim, loaded.mReferences[1].mInventory[0], MWWorld::InventoryStore::Slot_Cuirass);
        service.clear(); service.restore(loaded, store);
        EXPECT_EQ(snapshot().serializeBinary(), committed.serializeBinary());
        EXPECT_FALSE(world.commitOblivionPhysicalContact(action, attacker, victim, deltas));
        const auto beforeBreak = snapshot().serializeBinary();
        target.getCellRef().setNativeItemCondition(1);
        const auto broken = snapshot().serializeBinary();
        EXPECT_FALSE(MWMechanics::resolveOblivionOrdinaryWeaponContact(world, attacker, victim, source, 0, false));
        EXPECT_EQ(snapshot().serializeBinary(), broken);
        target.getCellRef().setNativeItemCondition(44.015625f);
        EXPECT_EQ(snapshot().serializeBinary(), beforeBreak);
    }

}

namespace
{
    ESM::FormKey installMeleeAiStyle(NativeWorldFixture& fixture)
    {
        auto& store = fixture.mWorld.getStore();
        ESM4::CombatStyle style{};
        style.mId = {0x880, 0};
        style.mStandard.emplace();
        style.mStandard->mAttackChance = 100;
        style.mStandard->mFlags = 34;
        style.mStandard->mDoNotAcquire = true;
        const auto key = ESM::FormKey::content("headless.esm", 0x880);
        store.getWritable<ESM4::CombatStyle>().insertStatic(style, key);
        const auto base = ESM::FormKey::content("headless.esm", 0x800);
        auto npc = *store.search<ESM4::Npc>(base);
        npc.mCombatStyle = style.mId;
        store.getWritable<ESM4::Npc>().insertStatic(npc, base);
        return key;
    }

    TEST(OblivionWorldTest, MeleeAiIntentBindsWinningContentAndCancelsRetargetWithoutSpending)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto style = installMeleeAiStyle(fixture);
        const auto a = addNativeNpc(fixture, 0x801), b = addNativeNpc(fixture, 0x802), c = addNativeNpc(fixture, 0x803);
        for (auto actor : {a, b, c}) ASSERT_TRUE(world.activateOblivionActor(actor));
        auto& service = *world.getOblivionCombatService();
        const auto key = a.getCellRef().getFormKey(), target = b.getCellRef().getFormKey(), other = c.getCellRef().getFormKey();
        const auto snapshot = [&] {
            auto state = captureNativeActorState(fixture, a);
            for (auto actor : {b, c}) state.mReferences.push_back(captureNativeActorState(fixture, actor).mReferences[0]);
            return state;
        };
        const auto idle = snapshot().serializeBinary();
        EXPECT_THROW(service.setMeleeAiIntent(key, {target, style}, world.getStore()), std::invalid_argument);
        EXPECT_EQ(snapshot().serializeBinary(), idle);
        ASSERT_TRUE(service.engage(key, target));
        ASSERT_TRUE(service.engage(key, other));
        const auto before = snapshot();
        EXPECT_THROW(service.setMeleeAiIntent(key, {key, style}, world.getStore()), std::invalid_argument);
        EXPECT_THROW(service.setMeleeAiIntent(key, {target, ESM::FormKey::content("missing.esm", 1)}, world.getStore()), std::invalid_argument);
        EXPECT_EQ(snapshot().serializeBinary(), before.serializeBinary());
        ASSERT_NO_THROW(service.setMeleeAiIntent(key, {target, style}, world.getStore()));
        // The selected melee target does not capture unrelated physical actions.
        const auto unrelated = service.allocateAction(key);
        ASSERT_TRUE(world.commitOblivionPhysicalContact(unrelated, a, c, {0, 0, 0, 0}));
        EXPECT_TRUE(service.isActionConsumed(unrelated));
        EXPECT_EQ(service.findMeleeState(key)->mAiIntent->mTarget, target);
        const auto id = service.beginMeleeStrike(key, ESM4::MeleeStrikeKind::Left, "handtohandattackleft");
        service.setMeleeInput(key, {.5f, true, false, ESM4::MeleeQueuedStrike::Power});
        ASSERT_TRUE(service.bindMeleePlayback(id, key));
        const auto owned = snapshot();
        ASSERT_NO_THROW(service.setMeleeAiIntent(key, {target, style}, world.getStore()));
        EXPECT_EQ(snapshot().serializeBinary(), owned.serializeBinary());
        EXPECT_THROW(world.commitOblivionPhysicalContact(id, a, c, {-7, -1, 0, 0}), std::invalid_argument);
        EXPECT_EQ(snapshot().serializeBinary(), owned.serializeBinary());
        auto lossy = owned; lossy.mVersion = 27;
        const auto lossyBefore = lossy;
        EXPECT_THROW(service.capture(lossy), std::invalid_argument);
        EXPECT_EQ(lossy, lossyBefore);
        ASSERT_NO_THROW(service.restore(ESM4::RuntimeState::deserializeBinary(owned.serializeBinary()), world.getStore()));
        EXPECT_EQ(snapshot().serializeBinary(), owned.serializeBinary());
        auto wrongStyle = owned;
        wrongStyle.mNativeMeleeStates.at(key).mAiIntent->mStyle = ESM::FormKey::content("missing.esm", 1);
        EXPECT_THROW(service.restore(wrongStyle, world.getStore()), std::invalid_argument);
        EXPECT_EQ(snapshot().serializeBinary(), owned.serializeBinary());
        ASSERT_NO_THROW(service.setMeleeAiIntent(key, {other, style}, world.getStore()));
        EXPECT_TRUE(service.isActionConsumed(id));
        ASSERT_TRUE(service.findMeleeState(key)->mAiIntent);
        EXPECT_EQ(service.findMeleeState(key)->mAiIntent->mTarget, other);
        EXPECT_FALSE(service.findMeleeState(key)->mStrike);
        EXPECT_EQ(service.findMeleeState(key)->mInput, ESM4::RuntimeMeleeInput{});
        EXPECT_EQ(service.getProcessAction(key), -1);
        const auto changed = snapshot();
        EXPECT_EQ(changed.mPhysicalActions.mNext, owned.mPhysicalActions.mNext);
        EXPECT_EQ(changed.mCombatRngState, owned.mCombatRngState);
        for (std::size_t i = 0; i < changed.mNativeActorValues.size(); ++i)
            EXPECT_EQ(changed.mNativeActorValues[i].mValues, owned.mNativeActorValues[i].mValues);
        EXPECT_TRUE(service.clearMeleeAiIntent(key));
        EXPECT_FALSE(service.clearMeleeAiIntent(key));
        EXPECT_FALSE(service.clearMeleeAiIntent(ESM::FormKey::content("missing.esm", 1)));
        const auto scripted = service.beginMeleeStrike(key, ESM4::MeleeStrikeKind::Left, "handtohandattackleft");
        const auto scriptedBefore = snapshot().serializeBinary();
        EXPECT_THROW(service.setMeleeAiIntent(key, {target, style}, world.getStore()), std::invalid_argument);
        EXPECT_EQ(snapshot().serializeBinary(), scriptedBefore);
        EXPECT_TRUE(service.isActionPending(scripted, key));
    }

    TEST(OblivionWorldTest, MeleeAiStopAndTargetIncapacitationCancelAllOwnedInputsWithoutReplay)
    {
        for (int mode : {0, 1, 2})
        {
            SCOPED_TRACE(mode); // stop, death, essential knockout
            NativeWorldFixture fixture;
            auto& world = fixture.mWorld;
            const auto style = installMeleeAiStyle(fixture);
            const auto a = addNativeNpc(fixture, 0x801), b = addNativeNpc(fixture, 0x802), c = addNativeNpc(fixture, 0x803);
            for (auto actor : {a, b, c}) ASSERT_TRUE(world.activateOblivionActor(actor));
            auto& service = *world.getOblivionCombatService();
            const auto key = a.getCellRef().getFormKey(), target = b.getCellRef().getFormKey(), other = c.getCellRef().getFormKey();
            const auto snapshot = [&] {
                auto state = captureNativeActorState(fixture, a);
                for (auto actor : {b, c}) state.mReferences.push_back(captureNativeActorState(fixture, actor).mReferences[0]);
                return state;
            };
            ASSERT_TRUE(service.engage(key, target)); ASSERT_TRUE(service.engage(other, target));
            service.setMeleeAiIntent(key, {target, style}, world.getStore());
            service.setMeleeAiIntent(other, {target, style}, world.getStore());
            const auto id = service.beginMeleeStrike(key, ESM4::MeleeStrikeKind::Left, "handtohandattackleft");
            const auto peer = service.beginMeleeStrike(other, ESM4::MeleeStrikeKind::Left, "handtohandattackleft");
            service.setMeleeInput(key, {.2f, true, false, ESM4::MeleeQueuedStrike::Power});
            service.setMeleeInput(other, {.2f, true, false, ESM4::MeleeQueuedStrike::Power});
            ASSERT_TRUE(service.bindMeleePlayback(id, key)); ASSERT_TRUE(service.bindMeleePlayback(peer, other));
            const auto before = snapshot();
            if (mode == 0)
                ASSERT_TRUE(service.stopCombat(target));
            else
                ASSERT_TRUE(service.commitPhysicalContact(id, a, b, {-7, -100, 0, 0}, nullptr,
                    mode == 2, {10.f, .2f}, {}));
            for (auto owner : {key, other})
            {
                const auto* melee = service.findMeleeState(owner);
                ASSERT_NE(melee, nullptr);
                EXPECT_FALSE(melee->mAiIntent); EXPECT_FALSE(melee->mStrike);
                EXPECT_EQ(melee->mInput, ESM4::RuntimeMeleeInput{});
                EXPECT_EQ(service.getProcessAction(owner), -1);
            }
            EXPECT_TRUE(service.isActionConsumed(id)); EXPECT_TRUE(service.isActionConsumed(peer));
            const auto after = snapshot();
            EXPECT_EQ(after.mCombatRngState, before.mCombatRngState);
            EXPECT_EQ(after.mPhysicalActions.mNext, before.mPhysicalActions.mNext);
            const auto previousPeer = std::find_if(before.mNativeActorValues.begin(), before.mNativeActorValues.end(),
                [&](const auto& actor) { return actor.mActor == other; });
            ASSERT_NE(previousPeer, before.mNativeActorValues.end());
            EXPECT_EQ(service.findActorValues(other)->mValues, previousPeer->mValues);
            if (mode == 0)
                for (std::size_t i = 0; i < after.mNativeActorValues.size(); ++i)
                    EXPECT_EQ(after.mNativeActorValues[i].mValues, before.mNativeActorValues[i].mValues);
            else
                EXPECT_EQ(service.findActorLife(target)->mPhase, mode == 2
                    ? ESM4::ActorLifePhase::EssentialUnconscious : ESM4::ActorLifePhase::Dead);
            ASSERT_NO_THROW(service.restore(ESM4::RuntimeState::deserializeBinary(after.serializeBinary()), world.getStore()));
            EXPECT_FALSE(world.commitOblivionPhysicalContact(id, a, b, {-7, -100, 0, 0}));
            EXPECT_EQ(snapshot().serializeBinary(), after.serializeBinary());
        }
    }

    TEST(OblivionWorldTest, StationaryMeleeAiRejectsUnsupportedStyleAndNeverInventsHeadlessContact)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto style = installMeleeAiStyle(fixture);
        const auto a = addNativeNpc(fixture, 0x801), b = addNativeNpc(fixture, 0x802);
        for (auto actor : {a, b}) { world.getWorldModel().registerPtr(actor); ASSERT_TRUE(world.activateOblivionActor(actor)); }
        auto& service = *world.getOblivionCombatService();
        const auto key = a.getCellRef().getFormKey(), target = b.getCellRef().getFormKey();
        ASSERT_TRUE(service.engage(key, target));
        const auto snapshot = [&] {
            auto state = captureNativeActorState(fixture, a);
            state.mReferences.push_back(captureNativeActorState(fixture, b).mReferences[0]);
            return state;
        };
        const auto before = snapshot();
        EXPECT_TRUE(MWMechanics::updateOblivionStationaryMeleeAi(world, a, true));
        EXPECT_FALSE(a.getClass().getCreatureStats(a).getAttackingOrSpell());
        EXPECT_EQ(snapshot().serializeBinary(), before.serializeBinary());
        EXPECT_FALSE(MWMechanics::canReachOblivionMeleeTarget(world, a, b, 128));
        EXPECT_FALSE(MWMechanics::canReachOblivionMeleeTarget(world, a, {}, 128));
        auto record = *world.getStore().search<ESM4::CombatStyle>(style);
        record.mStandard->mAttackChance = 50;
        world.getStore().getWritable<ESM4::CombatStyle>().insertStatic(record, style);
        EXPECT_THROW(MWMechanics::updateOblivionStationaryMeleeAi(world, a, true), std::runtime_error);
        EXPECT_EQ(snapshot().serializeBinary(), before.serializeBinary());
        EXPECT_TRUE(MWMechanics::updateOblivionStationaryMeleeAi(world, a, false));
        EXPECT_EQ(snapshot().serializeBinary(), before.serializeBinary());
    }
}
