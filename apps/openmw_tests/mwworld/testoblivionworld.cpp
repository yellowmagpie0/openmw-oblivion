#include <components/esm4/common.hpp>
#include "apps/openmw/mwworld/projectilemanager.hpp"
#include "apps/openmw/mwworld/manualref.hpp"
#include "apps/openmw/mwphysics/collisiontype.hpp"
#include <components/esm3/inventorystate.hpp>
#include <components/esm3/objectstate.hpp>
#include <components/esm3/npcstate.hpp>
#include <components/esm3/cellstate.hpp>
#include <components/esm3/fogstate.hpp>
#include <bit>
#include <cstddef>
#include <cstring>
#include <new>
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
#include <components/misc/rng.hpp>
#include "apps/openmw/mwworld/weather.hpp"
#include <components/sceneutil/keyframe.hpp>
#include <components/settings/values.hpp>
#include <components/vfs/filesystemarchive.hpp>
#include <osg/MatrixTransform>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Stats>
#include <osg/FrameStamp>
#include <osgUtil/UpdateVisitor>
#include <osgDB/WriteFile>
#include <osgDB/Registry>
#include "apps/openmw/mwphysics/actor.hpp"
#include "apps/openmw/mwphysics/physicssystem.hpp"
#include "apps/openmw/mwphysics/oblivionragdoll.hpp"
#include "apps/openmw/mwworld/oblivionphysicalpose.hpp"
#include <components/sceneutil/skeleton.hpp>
#include <components/nifosg/matrixtransform.hpp>
#include <components/misc/osguservalues.hpp>
#include "apps/openmw/mwmechanics/oblivionmelee.hpp"
#include "apps/openmw/mwmechanics/oblivionranged.hpp"
#include <components/esm4/loadammo.hpp>
#include <components/esm4/loadbsgn.hpp>
#include "apps/openmw/mwworld/player.hpp"
#include "apps/openmw/mwworld/datetimemanager.hpp"
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <fstream>
#include <functional>
#include <limits>
#include <stdexcept>
#include <sstream>

#include <components/esm/records.hpp>
#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm4/loadglob.hpp>
#include <components/esm4/loadlvli.hpp>
#include <components/esm4/loadcont.hpp>
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
#include "apps/openmw/mwrender/esm4npcanimation.hpp"
#include <osgAnimation/Bone>
#include "apps/openmw/mwclass/npc.hpp"
#include "apps/openmw/mwclass/esm4interactive.hpp"
#include "apps/openmw/mwmechanics/oblivioncombat.hpp"
#include "apps/openmw/mwmechanics/oblivionai.hpp"
#include "apps/openmw/mwmechanics/actors.hpp"
#include "apps/openmw/mwmechanics/mechanicsmanagerimp.hpp"
#include "apps/openmw/mwlua/context.hpp"
#include "apps/openmw/mwlua/localscripts.hpp"
#include "apps/openmw/mwlua/globalscripts.hpp"
#include "apps/openmw/mwlua/engineevents.hpp"
#include "apps/openmw/mwlua/luamanagerimp.hpp"
#include "apps/openmw/mwlua/stats.hpp"
#include "apps/openmw/mwlua/types/types.hpp"
#include "apps/openmw/mwsound/soundmanagerimp.hpp"
#include "apps/openmw/mwworld/worldimp.hpp"
#include "apps/openmw/mwworld/oblivionscriptmanager.hpp"
#include "apps/openmw/mwworld/oblivionactorstats.hpp"
#include "apps/openmw/mwworld/timestamp.hpp"
#include "apps/openmw/mwstate/saveadmission.hpp"

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

    void prepareNativeSnapshotPlayer(NativeWorldFixture& fixture, const ESM4::RuntimeState& state)
    {
        auto& world = fixture.mWorld;
        MWClass::Npc::registerSelf();
        if (world.getPlayerPtr().isEmpty())
            world.setupPlayer();
        auto& store = world.getStore();
        ESM::Race race{};
        race.blank();
        race.mId = ESM::RefId(ESM::FormId{0x810, 0});
        if (!store.get<ESM::Race>().search(race.mId))
            store.getWritable<ESM::Race>().insertStatic(race);
        if (!world.getPlayerPtr().get<ESM::NPC>()->mBase->mRace.getIf<ESM::FormId>())
        {
            auto player = *world.getPlayerPtr().get<ESM::NPC>()->mBase;
            player.mRace = race.mId;
            auto metadata = store.preparePlayerRecord(player);
            world.getPlayer().set(metadata.commit());
        }
        const ESM::FormKeyResolver resolver({"headless.esm"});
        const auto prepareCell = [&](const ESM::FormKey& key) {
            if (store.get<ESM4::Cell>().search(key))
                return;
            const auto id = resolver.toFormId(key);
            if (!id)
                throw std::runtime_error("snapshot fixture requires a native cell");
            ESM4::Cell cell{};
            cell.mId = ESM::RefId(*id);
            cell.mFormKey = key;
            cell.mCellFlags = ESM4::CELL_Interior;
            cell.mEditorId = "AcceptedSnapshotFixture";
            store.getWritable<ESM4::Cell>().insertStatic(cell, key);
        };
        prepareCell(state.mPlayer.mCell);
        for (const auto& reference : state.mReferences)
            prepareCell(reference.mCell);
    }

    void acceptNativeSnapshot(NativeWorldFixture& fixture, const ESM4::RuntimeState& state)
    {
        prepareNativeSnapshotPlayer(fixture, state);
        readNativeSnapshot(fixture, state);
        fixture.mWorld.applyOblivionRuntimeState();
    }

    void admitNativeSnapshot(NativeWorldFixture& fixture, const ESM4::RuntimeState& state)
    {
        ESM::ESMWriter writer;
        writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion);
        auto stream = std::make_unique<std::stringstream>();
        writer.save(*stream);
        ESM::SavedGame profile{};
        profile.mGameProfile = ESM::GameProfile::Oblivion;
        profile.mRuntimeStateVersion = state.mVersion;
        writer.startRecord(ESM::REC_SAVE);
        profile.save(writer);
        writer.endRecord(ESM::REC_SAVE);
        writer.startRecord(ESM::REC_T4ST);
        state.save(writer);
        writer.endRecord(ESM::REC_T4ST);
        ESM::ESMReader reader;
        reader.open(std::move(stream), "native-semantic-admission");
        static_cast<void>(MWState::admitSave(reader, fixture.mWorld.getGameProfile(),
            [&](const auto& incoming) { fixture.mWorld.validateOblivionSaveState(incoming); },
            &fixture.mWorld.getStore()));
        EXPECT_EQ(reader.getRecName(), ESM::REC_SAVE);
    }
    class NativePhysicalPoseTestAnimation : public MWRender::Animation
    {
    public:
        unsigned mPhysicalRebuilds = 0;
        bool mThrowRebuild = false;
        NativePhysicalPoseTestAnimation(osg::Group* parent, SceneUtil::Skeleton* root)
            : Animation({}, parent, nullptr)
        {
            mObjectRoot = root;
            mSkeleton = root;
            parent->addChild(root);
            resetActiveGroups();
        }
    private:
        void addControllers() override
        {
            if (hasPhysicalPose())
                ++mPhysicalRebuilds;
            if (mThrowRebuild)
                throw std::runtime_error("renderer controller rebuild rejected");
        }
    };

    TEST(OblivionWorld, PhysicalConfigurationOwnsResolvedTablesAndRejectsLateReload)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        const auto& initial = world.getOblivionPhysicalBlendConfiguration();
        EXPECT_THROW(world.prepareOblivionActorKnockdownControllers({}, {}), std::logic_error);
        EXPECT_THROW(world.prepareOblivionActorHitBlendControllers({}, {}, false), std::logic_error);
        EXPECT_THROW(world.prepareOblivionActorHitControllers({}, {}, false, {}), std::logic_error);
        EXPECT_EQ(initial.mHit.mGains[1].mHierarchy, .4f);
        EXPECT_EQ(initial.mQuadHit[1].mHierarchy, .3f);
        ESM4::PhysicalBlendProfilesValues values;
        std::string tail = ".53125, .875";
        values.mDefaultGains[16] = tail;
        values.mHit.mKnockdownTime = ".625";
        values.mPassOutForce = "-10";
        values.mPassOutTime = "2";
        world.loadOblivionPhysicalBlendConfiguration(14, values);
        tail.assign("changed by input owner");
        const auto accepted = world.getOblivionPhysicalBlendConfiguration();
        EXPECT_EQ(accepted.mProfiles.mDefault.mGains[16], ".53125, .875");
        EXPECT_EQ(accepted.mPostLink[17].mHierarchy, .53125f);
        EXPECT_EQ(accepted.mPostLink[17].mVelocity, .875f);
        EXPECT_EQ(accepted.mDurations.mKnockdown[17], .625f);
        EXPECT_EQ(accepted.mProfiles.mDefault.mPassOutTime, 2.f);
        values.mDefaultGains[16] = "0, 0";
        values.mQuadHitGains.back() = "1, 1e1000";
        EXPECT_THROW(world.loadOblivionPhysicalBlendConfiguration(14, values), std::invalid_argument);
        EXPECT_EQ(world.getOblivionPhysicalBlendConfiguration().mPostLink[17].mHierarchy, .53125f);
        EXPECT_EQ(world.getOblivionPhysicalBlendConfiguration().mProfiles.mDefault.mGains[16],
            accepted.mProfiles.mDefault.mGains[16]);
        EXPECT_EQ(world.getOblivionPhysicalBlendConfiguration().mDurations.mKnockdown, accepted.mDurations.mKnockdown);
        world.loadOblivionPhysicalBlendConfiguration(13, values);
        EXPECT_EQ(world.getOblivionPhysicalBlendConfiguration().mPostLink[17].mHierarchy, .53125f);
        MWWorld::World legacy(&fixture.mResources, -1, "", fixture.mDirectory, ESM::GameProfile::Morrowind);
        EXPECT_THROW(legacy.getOblivionPhysicalBlendConfiguration(), std::logic_error);
        EXPECT_THROW(legacy.prepareOblivionActorHitBlendControllers({}, {}, true), std::logic_error);
        EXPECT_THROW(legacy.prepareOblivionActorHitControllers({}, {}, true, {}), std::logic_error);
        EXPECT_THROW(legacy.loadOblivionPhysicalBlendConfiguration(14, {}), std::logic_error);
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
            auto& physics = fixture.mWorld.initializePhysics(new osg::Group);
            NifBullet::ActorRagdollDefinition graph;
            graph.mSourceHash = std::string(16, 'a');
            NifBullet::RagdollBodyDefinition body{};
            body.mRecord = 12;
            body.mNodeRecord = 8;
            body.mMass = 2;
            body.mInertia = {1, 0, 0, 0, 1, 0, 0, 0, 1};
            body.mShape = NifBullet::RagdollSphere{.5f};
            body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .9f, .8f};
            body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
                78, 8, 0xd, 1.f, 0.f, 0.f, .25f, {{.25f, 1.f, 1.f}}};
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
            EXPECT_TRUE(physics.actorRagdollOwners().empty());
            physics.addActor(ptr, path);
            EXPECT_TRUE(physics.actorRagdollOwners().empty()); // Capsules are not physical pose owners.
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
            EXPECT_EQ(physics.actorRagdollOwners(), std::vector<MWWorld::Ptr>{ptr});
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
            EXPECT_EQ(physics.actorRagdollOwners(), std::vector<MWWorld::Ptr>{updated});
            EXPECT_THROW(physics.captureActorRagdoll(previous), std::invalid_argument);
            physics.remove(previous); // A stale reference cannot remove the new owner.
            EXPECT_EQ(physics.getActor(updated), capsule);
            EXPECT_TRUE(physics.hasActorRagdoll(updated));
            physics.updatePtr(updated, updated);
            ptr = updated;
            EXPECT_EQ(physics.actorRagdollDefinition(ptr).mSourceHash, graph.mSourceHash);
            const auto initialCache = physics.captureNativeBlendTimeCache();
            const ESM4::PhysicalBlendTimeCache savedCache{0xffffffffu, 1, -1, -0.f, -.25f};
            physics.restoreNativeBlendTimeCache(savedCache);
            EXPECT_EQ(physics.captureNativeBlendTimeCache(), savedCache);
            EXPECT_EQ(std::bit_cast<std::uint32_t>(physics.captureNativeBlendTimeCache().mKeyTime), 0x80000000u);
            auto invalidCache = savedCache; invalidCache.mResult = std::numeric_limits<float>::quiet_NaN();
            EXPECT_THROW(physics.restoreNativeBlendTimeCache(invalidCache), std::invalid_argument);
            EXPECT_EQ(physics.captureNativeBlendTimeCache(), savedCache);
            physics.restoreNativeBlendTimeCache(initialCache);
            const auto base = ESM::FormKey::content("headless.esm", 0x800);
            const std::array<MWPhysics::NativeRagdollSnapshotBinding, 1> groupBindings{{
                {ptr, ESM::FormKey::content("headless.esm", 0x900), base, path.value()}}};
            const auto groupBefore = physics.captureActorRagdollSnapshots(groupBindings);
            ASSERT_EQ(groupBefore.mActors.size(), 1u); ASSERT_TRUE(groupBefore.mTimeCache);
            auto groupChanged = groupBefore; groupChanged.mActors.begin()->second.mBodies[0].mPosition = {10, 20, 30};
            groupChanged.mTimeCache = ESM4::PhysicalBlendTimeCache{2, 4, 0, 1, 3};
            auto preparedGroup = physics.prepareActorRagdollSnapshots(groupChanged, groupBindings);
            ASSERT_NE(preparedGroup, nullptr);
            EXPECT_EQ(physics.captureActorRagdollSnapshots(groupBindings), groupBefore);
            physics.commitActorRagdollSnapshots(*preparedGroup);
            EXPECT_EQ(physics.captureActorRagdollSnapshots(groupBindings), groupChanged);
            EXPECT_THROW(physics.commitActorRagdollSnapshots(*preparedGroup), std::invalid_argument);
            auto groupInvalid = groupChanged; groupInvalid.mActors.begin()->second.mBodies[0].mPosition = {99, 99, 99};
            groupInvalid.mTimeCache->mResult = std::numeric_limits<float>::quiet_NaN();
            EXPECT_THROW(physics.restoreActorRagdollSnapshots(groupInvalid, groupBindings), std::invalid_argument);
            EXPECT_EQ(physics.captureActorRagdollSnapshots(groupBindings), groupChanged);
            physics.restoreActorRagdollSnapshots(groupBefore, groupBindings);
            const auto original = physics.captureActorRagdollSnapshot(ptr, base, path.value());
            ASSERT_TRUE(original.mBodies[0].mNativePackedVelocity);
            auto packedSave = physics.captureActorRagdollNativePackedVelocities(ptr);
            packedSave[0].mVelocities = {{1, 2, 3, -0.f}, {4, 5, 6, 8}};
            physics.restoreActorRagdollNativePackedVelocities(ptr, packedSave);
            const auto packedSnapshot = physics.captureActorRagdollSnapshot(ptr, base, path.value());
            ASSERT_TRUE(packedSnapshot.mBodies[0].mNativePackedVelocity);
            EXPECT_EQ(*packedSnapshot.mBodies[0].mNativePackedVelocity, packedSave[0].mVelocities);
            ASSERT_TRUE(packedSnapshot.mBodies[0].mNativeMotion);
            EXPECT_EQ(*packedSnapshot.mBodies[0].mNativeMotion, ESM4::RuntimeRagdollMotion::Dynamic);
            auto keyModeSnapshot = packedSnapshot;
            keyModeSnapshot.mBodies[0].mNativeMotion = ESM4::RuntimeRagdollMotion::Keyframed;
            physics.restoreActorRagdollSnapshot(ptr, keyModeSnapshot, base, path.value());
            EXPECT_EQ(physics.captureActorRagdollNativeMotionModes(ptr)[0].mMotion, NifBullet::RagdollNativeMotion::Keyframed);
            EXPECT_EQ(physics.captureActorRagdollSnapshot(ptr, base, path.value()), keyModeSnapshot);
            ASSERT_TRUE(keyModeSnapshot.mNativeBlends); ASSERT_EQ(keyModeSnapshot.mNativeBlends->size(), 1u);
            (*keyModeSnapshot.mNativeBlends)[0] = {12, 0xf123, 0xffffffffu, -0.f, -2.f};
            physics.restoreActorRagdollSnapshot(ptr, keyModeSnapshot, base, path.value());
            EXPECT_EQ(physics.captureActorRagdollSnapshot(ptr, base, path.value()), keyModeSnapshot);
            ASSERT_TRUE(keyModeSnapshot.mNativeControllers);
            ASSERT_EQ(keyModeSnapshot.mNativeControllers->mBlends.size(), 1u);
            auto& controllerState = keyModeSnapshot.mNativeControllers->mBlends[0].mState;
            controllerState.mClock = {10, 11, 1}; controllerState.mKeys.clear();
            controllerState.mCursor = 0xffffffffu; controllerState.mSetupState = 0xffffffffu;
            controllerState.mCachedGains = {-0.f, -2};
            physics.restoreActorRagdollSnapshot(ptr, keyModeSnapshot, base, path.value());
            EXPECT_EQ(physics.captureActorRagdollSnapshot(ptr, base, path.value()), keyModeSnapshot);
            for (unsigned field = 0; field < 2; ++field)
            {
                auto badController = keyModeSnapshot; badController.mBodies[0].mPosition = {10, 20, 30};
                if (field == 0)
                {
                    badController.mNativeControllers.emplace();
                    EXPECT_THROW(physics.restoreActorRagdollSnapshot(ptr, badController, base, path.value()), std::invalid_argument);
                }
                else
                {
                    badController.mNativeControllers->mBlends[0].mState.mClock.mElapsed = std::numeric_limits<float>::quiet_NaN();
                    EXPECT_THROW(physics.restoreActorRagdollSnapshot(ptr, badController, base, path.value()), std::runtime_error);
                }
                EXPECT_EQ(physics.captureActorRagdollSnapshot(ptr, base, path.value()), keyModeSnapshot);
            }
            const auto savedBlend = physics.captureActorRagdollBlendStates(ptr);
            EXPECT_EQ(savedBlend[0].mRequestedMotion, 0xffffffffu); EXPECT_EQ(savedBlend[0].mCollisionFlags, 0xf123);
            EXPECT_TRUE(std::signbit(savedBlend[0].mGains.mHierarchy)); EXPECT_EQ(savedBlend[0].mGains.mVelocity, -2.f);
            auto invalidBlend = keyModeSnapshot; invalidBlend.mBodies[0].mPosition = {10, 20, 30};
            invalidBlend.mNativeBlends->clear();
            EXPECT_THROW(physics.restoreActorRagdollSnapshot(ptr, invalidBlend, base, path.value()), std::invalid_argument);
            EXPECT_EQ(physics.captureActorRagdollSnapshot(ptr, base, path.value()), keyModeSnapshot);
            invalidBlend = keyModeSnapshot; (*invalidBlend.mNativeBlends)[0].mVelocityGain = std::numeric_limits<float>::infinity();
            EXPECT_THROW(physics.restoreActorRagdollSnapshot(ptr, invalidBlend, base, path.value()), std::runtime_error);
            EXPECT_EQ(physics.captureActorRagdollSnapshot(ptr, base, path.value()), keyModeSnapshot);
            auto invalidModeSnapshot = keyModeSnapshot;
            invalidModeSnapshot.mBodies[0].mPosition = {10, 20, 30};
            invalidModeSnapshot.mBodies[0].mNativeMotion = static_cast<ESM4::RuntimeRagdollMotion>(2);
            EXPECT_THROW(physics.restoreActorRagdollSnapshot(ptr, invalidModeSnapshot, base, path.value()), std::runtime_error);
            EXPECT_EQ(physics.captureActorRagdollSnapshot(ptr, base, path.value()), keyModeSnapshot);
            physics.restoreActorRagdollSnapshot(ptr, packedSnapshot, base, path.value());
            auto invalidPackedSnapshot = packedSnapshot;
            invalidPackedSnapshot.mBodies[0].mPosition = {10, 20, 30};
            invalidPackedSnapshot.mBodies[0].mNativePackedVelocity->mAngular[3] = std::numeric_limits<float>::quiet_NaN();
            EXPECT_THROW(physics.restoreActorRagdollSnapshot(ptr, invalidPackedSnapshot, base, path.value()), std::runtime_error);
            EXPECT_EQ(physics.captureActorRagdollSnapshot(ptr, base, path.value()), packedSnapshot);
            physics.restoreActorRagdollSnapshot(ptr, original, base, path.value());
            EXPECT_EQ(physics.captureActorRagdoll(ptr)[0].mPose, poses[0]);
            const std::array<NifBullet::RagdollNativeMotionRequest, 1> keyframed{{
                {12, NifBullet::RagdollNativeMotion::Keyframed}}};
            const std::array<NifBullet::RagdollNativeMotionRequest, 1> dynamic{{
                {12, NifBullet::RagdollNativeMotion::Dynamic}}};
            EXPECT_THROW(physics.setActorRagdollNativeMotionModes(previous, keyframed), std::invalid_argument);
            EXPECT_THROW(physics.captureActorRagdollNativeMotionModes(previous), std::invalid_argument);
            physics.setActorRagdollNativeMotionModes(ptr, keyframed);
            EXPECT_EQ(physics.captureActorRagdollNativeMotionModes(ptr),
                std::vector<NifBullet::RagdollNativeMotionRequest>(keyframed.begin(), keyframed.end()));
            EXPECT_EQ(physics.captureActorRagdoll(ptr)[0].mPose, poses[0]);
            EXPECT_TRUE(capsule->isCollisionSuspended());
            const osg::Matrixf sceneTarget = osg::Matrixf::translate(70, 0, 140);
            const std::array<NifBullet::RagdollNativeScenePoseRequest, 1> scenePoses{{{12, sceneTarget}}};
            EXPECT_THROW(physics.synchronizeActorRagdollKeyframedPoses(previous, scenePoses), std::invalid_argument);
            physics.synchronizeActorRagdollKeyframedPoses(ptr, scenePoses);
            EXPECT_EQ(physics.captureActorRagdoll(ptr)[0].mPose,
                NifBullet::ragdollNativePoseFromBoneWorld(sceneTarget));
            EXPECT_TRUE(capsule->isCollisionSuspended());
            physics.restoreActorRagdollSnapshot(ptr, original, base, path.value());
            physics.setActorRagdollNativeMotionModes(ptr, dynamic);
            EXPECT_THROW(physics.synchronizeActorRagdollKeyframedPoses(ptr, scenePoses), std::invalid_argument);
            EXPECT_EQ(physics.captureActorRagdollNativeMotionModes(ptr),
                std::vector<NifBullet::RagdollNativeMotionRequest>(dynamic.begin(), dynamic.end()));
            std::array<NifBullet::RagdollNativeBlendUpdate, 1> blendUpdates{{{12, sceneTarget, 1, .5f, 8}}};
            EXPECT_THROW(physics.updateActorRagdollBlends(previous, blendUpdates, 1.f/120, 0), std::invalid_argument);
            const auto keyPublication = physics.updateActorRagdollBlends(ptr, blendUpdates, 1.f/120, 0);
            ASSERT_EQ(keyPublication.size(), 1); EXPECT_EQ(keyPublication[0].mCollisionFlags, 0);
            EXPECT_FALSE(keyPublication[0].mSceneTarget); EXPECT_TRUE(capsule->isCollisionSuspended());
            EXPECT_EQ(physics.captureActorRagdollNativeMotionModes(ptr)[0].mMotion, NifBullet::RagdollNativeMotion::Keyframed);
            blendUpdates[0].mHierarchyGain = 0; blendUpdates[0].mVelocityGain = 0;
            blendUpdates[0].mCollisionFlags = keyPublication[0].mCollisionFlags;
            const auto dynamicPublication = physics.updateActorRagdollBlends(ptr, blendUpdates, 1.f/120, 0);
            ASSERT_EQ(dynamicPublication.size(), 1); EXPECT_EQ(dynamicPublication[0].mCollisionFlags, 8);
            EXPECT_TRUE(dynamicPublication[0].mSceneTarget); EXPECT_TRUE(capsule->isCollisionSuspended());
            EXPECT_EQ(physics.captureActorRagdollNativeMotionModes(ptr)[0].mMotion, NifBullet::RagdollNativeMotion::Dynamic);
            physics.restoreActorRagdollSnapshot(ptr, original, base, path.value());
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
            // The preceding dynamic pose drive compensates gravity. Set an
            // explicit zero-Z velocity to isolate keyframed gravity exclusion.
            auto keyInitial = original;
            keyInitial.mBodies[0].mLinearVelocity = {60, 0, 0};
            // This fixture admits length scale1: native60 is exactly60 world
            // units. Keep the authoritative snapshot lanes consistent.
            ASSERT_TRUE(keyInitial.mBodies[0].mNativePackedVelocity);
            keyInitial.mBodies[0].mNativePackedVelocity->mLinear = {60, 0, 0, 0};
            physics.restoreActorRagdollSnapshot(ptr, keyInitial, base, path.value());
            physics.setActorRagdollNativeMotionModes(ptr, keyframed);
            osg::ref_ptr<osg::Stats> schedulerStats = new osg::Stats("native-keyframed-scheduler");
            physics.stepSimulation(.05f, false, osg::Timer::instance()->tick(), 1, *schedulerStats);
            const auto steppedKey = physics.captureActorRagdoll(ptr)[0];
            EXPECT_GT(steppedKey.mPose.getOrigin().x(), 0);
            EXPECT_LE(steppedKey.mPose.getOrigin().x(), 3.001);
            EXPECT_NEAR(steppedKey.mPose.getOrigin().z(), 20, 1e-6);
            EXPECT_NEAR(steppedKey.mLinearVelocity.x(), 60, 1e-6);
            EXPECT_NEAR(steppedKey.mLinearVelocity.z(), 0, 1e-6);
            EXPECT_TRUE(capsule->isCollisionSuspended());
            physics.setActorRagdollNativeMotionModes(ptr, dynamic);
            physics.restoreActorRagdollSnapshot(ptr, original, base, path.value());
            EXPECT_EQ(physics.captureActorRagdollSnapshot(ptr, base, path.value()), original);
            auto bad = original;
            bad.mAssetHash[0] = '0';
            EXPECT_THROW(physics.restoreActorRagdollSnapshot(ptr, bad, base, path.value()), std::invalid_argument);
            EXPECT_EQ(physics.captureActorRagdollSnapshot(ptr, base, path.value()), original);
            const auto ownedControllers = physics.captureActorRagdollBlendControllers(ptr);
            ASSERT_EQ(ownedControllers.size(), 1u);
            EXPECT_EQ(ownedControllers[0].mRecord, 78u);
            EXPECT_FLOAT_EQ(ownedControllers[0].mState.mClock.mPreviousTime, -std::numeric_limits<float>::max());
            EXPECT_THROW(physics.captureActorRagdollBlendControllers(previous), std::invalid_argument);
            EXPECT_THROW(physics.captureActorRagdollBlendStates(previous), std::invalid_argument);
            ASSERT_EQ(physics.captureActorRagdollBlendStates(ptr).size(), 1u);
            EXPECT_EQ(physics.captureNativeBlendTimeCache().mCycle, 0xffffffffu);
            const std::array<NifBullet::RagdollNativeBlendControllerTarget, 1> ownedTargets{{{78, sceneTarget}}};
            EXPECT_THROW(physics.updateActorRagdollBlendControllers(previous, ownedTargets, 1.f, 1.f / 120, 0),
                std::invalid_argument);
            const auto ownedPublication = physics.updateActorRagdollBlendControllers(ptr, ownedTargets, 1.f, 1.f / 120, 0);
            ASSERT_EQ(ownedPublication.size(), 1u);
            EXPECT_EQ(ownedPublication[0].mRecord, 12u);
            EXPECT_FALSE(ownedPublication[0].mSceneTarget);
            EXPECT_EQ(physics.captureActorRagdollNativeMotionModes(ptr)[0].mMotion,
                NifBullet::RagdollNativeMotion::Keyframed);
            EXPECT_TRUE(capsule->isCollisionSuspended());
            EXPECT_FLOAT_EQ(physics.captureActorRagdollBlendControllers(ptr)[0].mState.mClock.mPreviousTime, 1.f);
            EXPECT_FLOAT_EQ(physics.captureActorRagdollBlendStates(ptr)[0].mGains.mHierarchy, 1.f);
            EXPECT_EQ(physics.captureNativeBlendTimeCache().mCycle, 2u);
            const std::array<NifBullet::RagdollBoneWorldPose, 1> frameBones{{{8, sceneTarget}}};
            const std::array<std::uint32_t, 1> frameOrder{{78}};
            unsigned scenePublications = 0;
            const auto publishScene = [&](std::span<const NifBullet::RagdollNativeBlendPublication> publications) {
                ++scenePublications;
                EXPECT_EQ(publications.size(), 1u);
                if (!publications.empty())
                {
                    EXPECT_EQ(publications[0].mRecord, 12u);
                }
            };
            ASSERT_EQ(physics.updateActorRagdollBlendFrame(ptr, frameBones, frameOrder,
                1.f, 1.f / 120, 0, publishScene).size(), 1u);
            EXPECT_EQ(scenePublications, 1u);
            EXPECT_TRUE(capsule->isCollisionSuspended());
            EXPECT_THROW(physics.updateActorRagdollBlendFrame(previous, frameBones, frameOrder,
                1.f, 1.f / 120, 0, publishScene), std::invalid_argument);
            EXPECT_EQ(scenePublications, 1u);
            const auto beforeFramePose = physics.captureActorRagdoll(ptr)[0].mPose;
            EXPECT_THROW(physics.updateActorRagdollBlendFrame(ptr, frameBones, frameOrder,
                1.125f, 1.f / 120, 0,
                [](auto) { throw std::runtime_error("scene publication rejected"); }), std::runtime_error);
            EXPECT_EQ(physics.captureActorRagdoll(ptr)[0].mPose, beforeFramePose);
            EXPECT_FLOAT_EQ(physics.captureActorRagdollBlendControllers(ptr)[0].mState.mClock.mPreviousTime, 1.f);
            EXPECT_FLOAT_EQ(physics.captureNativeBlendTimeCache().mKeyTime, 0.f);
            const std::array<NifBullet::RagdollNativeBlendControllerTarget, 1> badOwnedTargets{{
                {78, osg::Matrixf::scale(2, 2, 2)}}};
            const auto beforeOwnedPose = physics.captureActorRagdoll(ptr)[0].mPose;
            EXPECT_THROW(physics.updateActorRagdollBlendControllers(ptr, badOwnedTargets, 1.125f, 1.f / 120, 0),
                std::invalid_argument);
            EXPECT_EQ(physics.captureActorRagdoll(ptr)[0].mPose, beforeOwnedPose);
            EXPECT_FLOAT_EQ(physics.captureActorRagdollBlendControllers(ptr)[0].mState.mClock.mPreviousTime, 1.f);
            EXPECT_FLOAT_EQ(physics.captureNativeBlendTimeCache().mKeyTime, 0.f);
            // The scheduler cache is shared across physical owners. The native
            // cache identity omits reverse, so this second actor stays active.
            physics.addActor(duplicate, path);
            auto reverseGraph = graph;
            reverseGraph.mBodies[0].mBlendController->mRecord = 79;
            reverseGraph.mBodies[0].mBlendController->mFlags = 0x1d;
            physics.addActorRagdoll(duplicate, reverseGraph, 1.f, poses,
                MWPhysics::CollisionType_Actor, MWPhysics::CollisionType_World, &internalFilter);
            const std::array<NifBullet::RagdollNativeBlendControllerTarget, 1> reverseTargets{{{79, sceneTarget}}};
            ASSERT_EQ(physics.updateActorRagdollBlendControllers(duplicate, reverseTargets, 1.f, 1.f / 120, 0).size(), 1u);
            EXPECT_EQ(physics.captureActorRagdollBlendControllers(duplicate)[0].mState.mTiming.mFlags, 0x1d);
            EXPECT_TRUE(physics.getActor(duplicate)->isCollisionSuspended());
            physics.remove(duplicate);
            const std::array<NifBullet::RagdollNativeKnockdownBlendRequest, 1> downRequests{{{8, .25f}}};
            EXPECT_THROW(physics.prepareActorRagdollKnockdownBlends(previous, downRequests), std::invalid_argument);
            const std::array<NifBullet::RagdollNativeKnockdownBlendRequest, 2> badDown{{{8, .25f}, {999, .25f}}};
            EXPECT_THROW(physics.prepareActorRagdollKnockdownBlends(ptr, badDown), std::invalid_argument);
            EXPECT_EQ(physics.captureActorRagdollBlendControllers(ptr)[0].mState.mKeys.size(), 1u);
            EXPECT_EQ(physics.captureActorRagdollBlendControllers(ptr)[0].mState.mSetupState, 0u);
            const auto beforeSetupPose = physics.captureActorRagdoll(ptr)[0].mPose;
            const auto beforeSetupCache = physics.captureNativeBlendTimeCache();
            const auto prepared = physics.prepareActorRagdollKnockdownBlends(ptr, downRequests);
            ASSERT_EQ(prepared.size(), 1u);
            ASSERT_EQ(prepared[0], NifBullet::RagdollNativeKnockdownBlendDisposition::Started);
            const auto configured = physics.captureActorRagdollBlendControllers(ptr)[0];
            EXPECT_EQ(configured.mAttachedNode, 8u);
            EXPECT_EQ(configured.mState.mKeys.size(), 2u);
            EXPECT_EQ(configured.mState.mSetupState, 2u);
            EXPECT_EQ(configured.mState.mTiming.mFlags, 0xcd);
            EXPECT_FLOAT_EQ(configured.mState.mKeys[0].mGains.mHierarchy, 1.f);
            EXPECT_FLOAT_EQ(configured.mState.mClock.mPreviousTime, -std::numeric_limits<float>::max());
            EXPECT_EQ(physics.captureActorRagdoll(ptr)[0].mPose, beforeSetupPose);
            EXPECT_EQ(physics.captureNativeBlendTimeCache().mCycle, beforeSetupCache.mCycle);
            EXPECT_FLOAT_EQ(physics.captureNativeBlendTimeCache().mKeyTime, beforeSetupCache.mKeyTime);
            EXPECT_TRUE(capsule->isCollisionSuspended());
            ASSERT_EQ(physics.updateActorRagdollBlendControllers(ptr, ownedTargets,
                2.f, 1.f / 120, 0).size(), 1u);
            ASSERT_EQ(physics.updateActorRagdollBlendControllers(ptr, ownedTargets,
                2.25f, 1.f / 120, 0).size(), 1u);
            EXPECT_TRUE(physics.captureActorRagdollBlendControllers(ptr)[0].mState.mKeys.empty());
            EXPECT_EQ(physics.captureActorRagdollBlendControllers(ptr)[0].mState.mSetupState, 0u);
            EXPECT_EQ(physics.captureActorRagdollNativeMotionModes(ptr)[0].mMotion,
                NifBullet::RagdollNativeMotion::Dynamic);
            EXPECT_TRUE(capsule->isCollisionSuspended());
            const std::array<NifBullet::RagdollNativeVelocitySetupRequest, 1> velocitySetup{{{8, {1, 0, 0, 0}, .25f}}};
            const std::array<std::uint32_t, 1> physicalNodes{8};
            const std::array<NifBullet::RagdollNativeControllerReference, 2> expectedOrder{{
                {NifBullet::RagdollNativeControllerKind::Velocity, 8}, {NifBullet::RagdollNativeControllerKind::Blend, 78}}};
            const std::array<NifBullet::RagdollNativeForceRequest, 1> directForce{{{12, {2, 0, 0}, .25f}}};
            EXPECT_THROW(physics.prepareActorRagdollVelocityControllers(previous, velocitySetup), std::invalid_argument);
            EXPECT_THROW(physics.captureActorRagdollVelocityControllers(previous), std::invalid_argument);
            EXPECT_THROW(physics.restoreActorRagdollVelocityControllers(previous, {}), std::invalid_argument);
            EXPECT_THROW(physics.captureActorRagdollControllerOrder(previous, physicalNodes), std::invalid_argument);
            EXPECT_THROW(physics.advanceActorRagdollPhysicalControllers(previous, expectedOrder, 3.f), std::invalid_argument);
            EXPECT_THROW(physics.applyActorRagdollNativeForces(previous, directForce), std::invalid_argument);
            const std::array<NifBullet::RagdollNativeVelocitySetupRequest, 2> badVelocitySetup{{
                {8, {1, 0, 0, 0}, .25f}, {999, {}, .25f}}};
            EXPECT_THROW(physics.prepareActorRagdollVelocityControllers(ptr, badVelocitySetup), std::invalid_argument);
            EXPECT_TRUE(physics.captureActorRagdollVelocityControllers(ptr).empty());
            const auto beforeVelocityCache = physics.captureNativeBlendTimeCache();
            physics.prepareActorRagdollVelocityControllers(ptr, velocitySetup);
            const auto velocities = physics.captureActorRagdollVelocityControllers(ptr);
            ASSERT_EQ(velocities.size(), 1u);
            EXPECT_EQ(velocities[0].mTargetNode, 8u);
            EXPECT_EQ(velocities[0].mState.mForceVector, (std::array<float, 4>{2, 0, 0, 0}));
            EXPECT_EQ(physics.captureActorRagdollControllerOrder(ptr, physicalNodes),
                (std::vector<NifBullet::RagdollNativeControllerReference>(expectedOrder.begin(), expectedOrder.end())));
            EXPECT_EQ(physics.captureNativeBlendTimeCache().mCycle, beforeVelocityCache.mCycle);
            EXPECT_EQ(physics.captureNativeBlendTimeCache().mKeyTime, beforeVelocityCache.mKeyTime);
            auto redirected = velocities; redirected[0].mTargetNode.reset();
            redirected[0].mState.mClock.mElapsed = 7.f; redirected[0].mState.mFrameDelta = 99.f;
            physics.restoreActorRagdollVelocityControllers(ptr, redirected);
            physics.prepareActorRagdollVelocityControllers(ptr, velocitySetup);
            const auto reused = physics.captureActorRagdollVelocityControllers(ptr)[0];
            EXPECT_FALSE(reused.mTargetNode);
            EXPECT_EQ(reused.mState.mClock.mElapsed, 7.f);
            EXPECT_EQ(reused.mState.mFrameDelta, 99.f);
            auto invalidVelocity = velocities; invalidVelocity[0].mTargetNode = 999;
            EXPECT_THROW(physics.restoreActorRagdollVelocityControllers(ptr, invalidVelocity), std::invalid_argument);
            EXPECT_FALSE(physics.captureActorRagdollVelocityControllers(ptr)[0].mTargetNode);
            const std::array<NifBullet::RagdollNativeHitVelocitySetupRequest, 1> hitVelocitySetup{{{8, {1, -2, .5f, 0}, .5f}}};
            EXPECT_THROW(physics.prepareActorRagdollHitVelocityControllers(previous, hitVelocitySetup), std::invalid_argument);
            const std::array<NifBullet::RagdollNativeHitVelocitySetupRequest, 2> badHitVelocitySetup{{
                {8, {2, 0, 0, 0}, 1}, {999, {}, 1}}};
            const auto beforeHit = physics.captureActorRagdollVelocityControllers(ptr)[0];
            EXPECT_THROW(physics.prepareActorRagdollHitVelocityControllers(ptr, badHitVelocitySetup), std::invalid_argument);
            EXPECT_EQ(physics.captureActorRagdollVelocityControllers(ptr)[0].mState, beforeHit.mState);
            const auto beforeHitPose = physics.captureActorRagdoll(ptr)[0];
            physics.prepareActorRagdollHitVelocityControllers(ptr, hitVelocitySetup);
            const auto reusedHit = physics.captureActorRagdollVelocityControllers(ptr)[0];
            EXPECT_FALSE(reusedHit.mTargetNode);
            EXPECT_EQ(reusedHit.mState.mClock.mElapsed, 7.f);
            EXPECT_EQ(reusedHit.mState.mFrameDelta, 99.f);
            EXPECT_EQ(reusedHit.mState.mTiming.mStopKey, .2f);
            EXPECT_EQ(reusedHit.mState.mForceVector, (std::array<float, 4>{1, -2, .5f, 0}));
            EXPECT_EQ(physics.captureActorRagdoll(ptr)[0].mPose, beforeHitPose.mPose);
            EXPECT_EQ(physics.captureActorRagdoll(ptr)[0].mLinearVelocity, beforeHitPose.mLinearVelocity);
            physics.restoreActorRagdollVelocityControllers(ptr, {});
            physics.prepareActorRagdollHitVelocityControllers(ptr, hitVelocitySetup);
            EXPECT_EQ(physics.captureActorRagdollVelocityControllers(ptr)[0].mTargetNode, 8u);
            EXPECT_TRUE(physics.captureActorRagdollVelocityControllers(ptr)[0].mPrecedesBlend);
            physics.restoreActorRagdollVelocityControllers(ptr, velocities);
            const auto beforeDirectForce = physics.captureActorRagdoll(ptr)[0];
            const std::array<NifBullet::RagdollNativeForceRequest, 2> badDirectForce{{
                {12, {2, 0, 0}, .25f}, {999, {}, .25f}}};
            EXPECT_THROW(physics.applyActorRagdollNativeForces(ptr, badDirectForce), std::invalid_argument);
            EXPECT_EQ(physics.captureActorRagdoll(ptr)[0].mLinearVelocity, beforeDirectForce.mLinearVelocity);
            physics.applyActorRagdollNativeForces(ptr, directForce);
            EXPECT_FLOAT_EQ(float(physics.captureActorRagdoll(ptr)[0].mLinearVelocity.x()),
                float(beforeDirectForce.mLinearVelocity.x()) + .25f);
            EXPECT_EQ(physics.captureActorRagdoll(ptr)[0].mPose, beforeDirectForce.mPose);
            const auto beforeControllerForce = physics.captureActorRagdoll(ptr)[0];
            auto badPhysicalOrder = expectedOrder; badPhysicalOrder[1].mIdentity = 999;
            EXPECT_THROW(physics.advanceActorRagdollPhysicalControllers(ptr, badPhysicalOrder, 3.f), std::invalid_argument);
            EXPECT_EQ(physics.captureActorRagdollVelocityControllers(ptr)[0].mState.mClock.mPreviousTime,
                -std::numeric_limits<float>::max());
            physics.advanceActorRagdollPhysicalControllers(ptr, expectedOrder, 3.f);
            EXPECT_FLOAT_EQ(float(physics.captureActorRagdoll(ptr)[0].mLinearVelocity.x()),
                float(beforeControllerForce.mLinearVelocity.x()) + 1.6f);
            EXPECT_EQ(physics.captureActorRagdoll(ptr)[0].mPose, beforeControllerForce.mPose);
            EXPECT_EQ(physics.captureActorRagdollVelocityControllers(ptr)[0].mState.mClock.mPreviousTime, 3.f);
            physics.advanceActorRagdollPhysicalControllers(ptr, expectedOrder, 3.25f);
            EXPECT_FALSE(physics.captureActorRagdollVelocityControllers(ptr)[0].mState.mTiming.mFlags & 8);
            const std::array<NifBullet::RagdollNativeKnockdownBlendRequest, 1> immediateFinish{{{8, 0.f}}};
            ASSERT_EQ(physics.prepareActorRagdollKnockdownBlends(ptr, immediateFinish)[0],
                NifBullet::RagdollNativeKnockdownBlendDisposition::Started);
            physics.advanceActorRagdollPhysicalControllers(ptr, expectedOrder, 3.25f);
            EXPECT_TRUE(physics.captureActorRagdollVelocityControllers(ptr).empty());
            const std::array<NifBullet::RagdollNativeKnockdownControllerSetupRequest, 1> compoundDown{{{8, {1, -2, .5f}, .25f}}};
            EXPECT_THROW(physics.prepareActorRagdollKnockdownControllerSetup(previous, compoundDown, {-10.f, 2.f}),
                std::invalid_argument);
            const float invalidPassOut = std::numeric_limits<float>::quiet_NaN();
            EXPECT_THROW(physics.prepareActorRagdollKnockdownControllerSetup(ptr, compoundDown, {invalidPassOut, 2.f}),
                std::invalid_argument);
            EXPECT_TRUE(physics.captureActorRagdollVelocityControllers(ptr).empty());
            EXPECT_EQ(physics.captureActorRagdollBlendControllers(ptr)[0].mState.mSetupState, 0u);
            const std::array<NifBullet::RagdollNativeKnockdownControllerSetupRequest, 2> badCompound{{
                {8, {1, 0, 0}, .25f}, {999, {}, .25f}}};
            EXPECT_THROW(physics.prepareActorRagdollKnockdownControllerSetup(ptr, badCompound, {-10.f, 2.f}),
                std::invalid_argument);
            EXPECT_TRUE(physics.captureActorRagdollVelocityControllers(ptr).empty());
            const auto compoundPose = physics.captureActorRagdoll(ptr)[0].mPose;
            const auto compoundCache = physics.captureNativeBlendTimeCache();
            ASSERT_EQ(physics.prepareActorRagdollKnockdownControllerSetup(ptr, compoundDown, {-10.f, 2.f})[0],
                NifBullet::RagdollNativeKnockdownBlendDisposition::Started);
            const auto compoundVelocity = physics.captureActorRagdollVelocityControllers(ptr);
            ASSERT_EQ(compoundVelocity.size(), 1u);
            EXPECT_EQ(compoundVelocity[0].mTargetNode, 8u);
            EXPECT_TRUE(compoundVelocity[0].mPrecedesBlend);
            EXPECT_EQ(compoundVelocity[0].mState.mTiming.mStopKey, 2.f);
            EXPECT_EQ(physics.captureActorRagdollBlendControllers(ptr)[0].mState.mTiming.mStopKey, .25f);
            // Original oracle04 initialized force-10, mass2, damping0, body time.25.
            const std::array<std::uint32_t, 4> compoundBits{3224822233u, 1085727193u, 3216433625u, 1056964608u};
            for (unsigned i = 0; i < 4; ++i)
                EXPECT_EQ(std::bit_cast<std::uint32_t>(compoundVelocity[0].mState.mForceVector[i]), compoundBits[i]);
            EXPECT_EQ(physics.captureActorRagdoll(ptr)[0].mPose, compoundPose);
            EXPECT_EQ(physics.captureNativeBlendTimeCache().mKeyTime, compoundCache.mKeyTime);
            auto retainedCompound = compoundVelocity; retainedCompound[0].mTargetNode.reset();
            retainedCompound[0].mState.mClock.mElapsed = 7.f;
            retainedCompound[0].mState.mFrameDelta = 99.f;
            physics.restoreActorRagdollVelocityControllers(ptr, retainedCompound);
            ASSERT_EQ(physics.prepareActorRagdollKnockdownControllerSetup(ptr, compoundDown,
                {invalidPassOut, invalidPassOut})[0], NifBullet::RagdollNativeKnockdownBlendDisposition::Started);
            const auto preservedCompound = physics.captureActorRagdollVelocityControllers(ptr)[0];
            EXPECT_FALSE(preservedCompound.mTargetNode);
            EXPECT_EQ(preservedCompound.mState.mForceVector, compoundVelocity[0].mState.mForceVector);
            EXPECT_EQ(preservedCompound.mState.mTiming.mStopKey, 2.f);
            EXPECT_EQ(preservedCompound.mState.mClock.mElapsed, 7.f);
            EXPECT_EQ(preservedCompound.mState.mFrameDelta, 99.f);
            physics.restoreActorRagdollVelocityControllers(ptr, compoundVelocity);
            physics.advanceActorRagdollPhysicalControllers(ptr, expectedOrder, 4.f);
            EXPECT_TRUE(physics.captureActorRagdollVelocityControllers(ptr)[0].mState.mTiming.mFlags & 8);
            physics.advanceActorRagdollPhysicalControllers(ptr, expectedOrder, 4.25f);
            EXPECT_TRUE(physics.captureActorRagdollVelocityControllers(ptr).empty());
            EXPECT_EQ(physics.captureActorRagdollBlendControllers(ptr)[0].mState.mSetupState, 0u);
            EXPECT_TRUE(capsule->isCollisionSuspended());
            // Public packed World-scene publication uses real scheduler owner
            // binding for its guards, independently of caller fixture booleans.
            auto packed = physics.captureActorRagdollNativePackedVelocities(ptr);
            ASSERT_EQ(packed.size(), 1u);
            packed[0].mVelocities = {{1, 2, 3, -0.f}, {5, 6, 7, 8}};
            physics.restoreActorRagdollNativePackedVelocities(ptr, packed);
            auto observed = physics.captureActorRagdollNativePackedVelocities(ptr);
            EXPECT_TRUE(std::signbit(observed[0].mVelocities.mLinear[3]));
            EXPECT_EQ(observed[0].mVelocities.mAngular, packed[0].mVelocities.mAngular);
            const std::array packedDynamic{NifBullet::RagdollNativeMotionRequest{12, NifBullet::RagdollNativeMotion::Dynamic}};
            physics.setActorRagdollNativeMotionModes(ptr, packedDynamic);
            const std::array<NifBullet::RagdollNativeForceRequest, 1> packedForce{{{12, {}, .25f, 8.f}}};
            physics.applyActorRagdollNativeForces(ptr, packedForce);
            observed = physics.captureActorRagdollNativePackedVelocities(ptr);
            EXPECT_EQ(observed[0].mVelocities.mLinear[3], 1.f);
            EXPECT_EQ(observed[0].mVelocities.mAngular[3], 8.f);
            physics.restoreActorRagdollNativePackedVelocities(ptr, packed);
            EXPECT_THROW(physics.captureActorRagdollNativePackedVelocities(previous), std::invalid_argument);
            EXPECT_THROW(physics.restoreActorRagdollNativePackedVelocities(previous, packed), std::invalid_argument);
            auto badPacked = packed; badPacked[0].mVelocities.mAngular[3] = std::numeric_limits<float>::quiet_NaN();
            EXPECT_THROW(physics.restoreActorRagdollNativePackedVelocities(ptr, badPacked), std::invalid_argument);
            NifBullet::RagdollNativeWorldSceneRequest drive{12, {}};
            drive.mInput.mCurrentRotation = {0, 0, 0, 1};
            drive.mInput.mTargetRotation = {0, 0, .70710677f, .70710677f};
            drive.mInput.mTargetPosition = {142.87672424316406f, -285.7534484863281f, 71.43836212158203f, 0};
            drive.mInput.mFrameSeconds = .016f;
            const std::array worldRequests{drive};
            EXPECT_THROW(physics.synchronizeActorRagdollWorldScenes(previous, worldRequests), std::invalid_argument);
            const auto scenePose = physics.captureActorRagdoll(ptr)[0].mPose;
            const auto sceneModes = physics.captureActorRagdollNativeMotionModes(ptr);
            const auto sceneCache = physics.captureNativeBlendTimeCache();
            bool sceneHook = false;
            auto lateBadScene = drive; lateBadScene.mRecord = 999;
            const std::array invalidScenes{drive, lateBadScene};
            EXPECT_THROW(physics.synchronizeActorRagdollWorldScenes(ptr, invalidScenes, [&](auto) { sceneHook = true; }), std::invalid_argument);
            EXPECT_FALSE(sceneHook);
            EXPECT_THROW(physics.synchronizeActorRagdollWorldScenes(ptr, worldRequests, [](auto) { throw std::runtime_error("renderer failure"); }), std::runtime_error);
            observed = physics.captureActorRagdollNativePackedVelocities(ptr);
            EXPECT_EQ(observed[0].mVelocities.mLinear, packed[0].mVelocities.mLinear);
            EXPECT_EQ(observed[0].mVelocities.mAngular, packed[0].mVelocities.mAngular);
            const auto written = physics.synchronizeActorRagdollWorldScenes(ptr, worldRequests, [&](auto ids) {
                sceneHook = true; ASSERT_EQ(ids.size(), 1u); EXPECT_EQ(ids[0], 12u);
            });
            EXPECT_TRUE(sceneHook); ASSERT_EQ(written.size(), 1u);
            observed = physics.captureActorRagdollNativePackedVelocities(ptr);
            EXPECT_EQ(observed[0].mVelocities.mLinear[0], 8929.794921875f);
            EXPECT_EQ(observed[0].mVelocities.mLinear[1], -17859.58984375f);
            EXPECT_NEAR(observed[0].mVelocities.mAngular[3], 98.17475891113281f, .001f);
            EXPECT_EQ(physics.captureActorRagdoll(ptr)[0].mPose, scenePose);
            EXPECT_EQ(physics.captureActorRagdollNativeMotionModes(ptr), sceneModes);
            EXPECT_EQ(physics.captureNativeBlendTimeCache().mKeyTime, sceneCache.mKeyTime);
            auto noStep = drive; noStep.mInput.mFrameSeconds = 0;
            const std::array zeroFrameScenes{noStep};
            EXPECT_TRUE(physics.synchronizeActorRagdollWorldScenes(ptr, zeroFrameScenes).empty());
            EXPECT_EQ(physics.captureActorRagdollNativePackedVelocities(ptr)[0].mVelocities.mAngular, observed[0].mVelocities.mAngular);
            auto nearScene = noStep; nearScene.mInput.mTargetPosition = {}; nearScene.mInput.mTargetRotation = {0, 0, 0, 1};
            const std::array nearScenes{nearScene};
            EXPECT_EQ(physics.synchronizeActorRagdollWorldScenes(ptr, nearScenes).size(), 1u);
            EXPECT_EQ(physics.captureActorRagdollNativePackedVelocities(ptr)[0].mVelocities.mAngular, (std::array<float, 4>{}));
            physics.removeActorRagdoll(ptr);
            physics.removeActorRagdoll(ptr);
            EXPECT_FALSE(physics.hasActorRagdoll(ptr));
            EXPECT_FALSE(capsule->isCollisionSuspended());
            ASSERT_NE(capsule->getCollisionObject()->getBroadphaseHandle(), nullptr);
            // Join real renderer ownership with the public physical owner.
            auto renderedGraph = graph;
            renderedGraph.mBodies[0].mBone = "Pelvis";
            renderedGraph.mBodies[0].mBlend->mFlags = 1;
            osg::ref_ptr<osg::Group> physicalParent = new osg::Group;
            osg::ref_ptr<SceneUtil::Skeleton> physicalRoot = new SceneUtil::Skeleton;
            physicalRoot->setUserValue(Misc::OsgUserValues::sFileHash, renderedGraph.mSourceHash);
            osg::ref_ptr<NifOsg::MatrixTransform> physicalBone
                = new NifOsg::MatrixTransform(Nif::NiTransform::getIdentity());
            physicalBone->setName("Pelvis");
            physicalBone->setUserValue("recordIndex", 8u);
            physicalBone->setTranslation({1, 0, 0});
            physicalRoot->addChild(physicalBone);
            NativePhysicalPoseTestAnimation animation(physicalParent, physicalRoot);
            auto placement = Nif::NiTransform::getIdentity();
            placement.mScale = 2.f;
            placement.mTranslation = {100, 20, 30};
            const auto originalBone = physicalBone->getMatrix();
            const std::array<std::uint32_t, 1> linkedFilters{0x1108};
            auto linkedGains = ESM4::InitialPhysicalBlendGainTable;
            linkedGains[17] = {.53125f, .875f};
            const auto beginPhysical = [&](const auto& definition) {
                MWWorld::beginNativeActorPhysicalPose(physics, animation, ptr, definition, placement, linkedFilters, linkedGains,
                    MWPhysics::CollisionType_Actor, MWPhysics::CollisionType_World, &internalFilter);
            };
            linkedGains[17].mHierarchy = std::numeric_limits<float>::quiet_NaN();
            EXPECT_THROW(beginPhysical(renderedGraph), std::invalid_argument);
            EXPECT_FALSE(animation.hasPhysicalPose());
            EXPECT_FALSE(physics.hasActorRagdoll(ptr));
            EXPECT_FALSE(capsule->isCollisionSuspended());
            EXPECT_EQ(animation.mPhysicalRebuilds, 0u);
            EXPECT_EQ(physicalBone->getMatrix(), originalBone);
            linkedGains[17].mHierarchy = .53125f;
            linkedGains[6].mHierarchy = std::numeric_limits<float>::quiet_NaN();
            auto rejectedGraph = renderedGraph;
            rejectedGraph.mBodies[0].mInertia = {};
            EXPECT_THROW(beginPhysical(rejectedGraph), std::invalid_argument);
            EXPECT_EQ(animation.mPhysicalRebuilds, 1u);
            EXPECT_FALSE(animation.hasPhysicalPose());
            EXPECT_FALSE(physics.hasActorRagdoll(ptr));
            EXPECT_FALSE(capsule->isCollisionSuspended());
            EXPECT_EQ(physicalBone->getMatrix(), originalBone);
            beginPhysical(renderedGraph);
            ASSERT_TRUE(animation.hasPhysicalPose());
            ASSERT_TRUE(physics.hasActorRagdoll(ptr));
            EXPECT_TRUE(capsule->isCollisionSuspended());
            EXPECT_FLOAT_EQ(physics.actorRagdollDefinition(ptr).mBodies[0].mMass, 4.f);
            EXPECT_FLOAT_EQ(std::get<NifBullet::RagdollSphere>(
                physics.actorRagdollDefinition(ptr).mBodies[0].mShape).mRadius, 1.f);
            const auto linkedState = physics.captureActorRagdollBlendStates(ptr)[0];
            EXPECT_EQ(linkedState.mCollisionFlags, 9u);
            EXPECT_EQ(linkedState.mGains.mHierarchy, .53125f);
            EXPECT_EQ(linkedState.mGains.mVelocity, .875f);
            EXPECT_EQ(linkedState.mRequestedMotion, 8u);
            EXPECT_EQ(renderedGraph.mBodies[0].mBlend->mFlags, 1u);
            EXPECT_EQ(renderedGraph.mBodies[0].mBlend->mHierarchyGain, .9f);
            EXPECT_EQ(renderedGraph.mBodies[0].mBlend->mVelocityGain, .8f);
            EXPECT_NEAR(physics.captureActorRagdoll(ptr)[0].mPose.getOrigin().x(), 102, .001);
            EXPECT_EQ(physicalBone->getMatrix(), originalBone);
            EXPECT_THROW(beginPhysical(renderedGraph), std::invalid_argument);
            EXPECT_TRUE(animation.hasPhysicalPose());
            EXPECT_TRUE(physics.hasActorRagdoll(ptr));
            MWWorld::endNativeActorPhysicalPose(physics, animation, ptr);
            MWWorld::endNativeActorPhysicalPose(physics, animation, ptr);
            EXPECT_FALSE(animation.hasPhysicalPose());
            EXPECT_FALSE(physics.hasActorRagdoll(ptr));
            EXPECT_FALSE(capsule->isCollisionSuspended());
            EXPECT_EQ(physicalBone->getMatrix(), originalBone);
            EXPECT_FLOAT_EQ(renderedGraph.mBodies[0].mMass, 2.f);
            // World admission consumes its owned DEFAULT table. Down consumes
            // the same resolved configuration through the actual scheduler.
            ESM4::PhysicalBlendProfilesValues worldConfigured;
            worldConfigured.mDefaultGains[16] = ".53125, .875";
            worldConfigured.mHit.mKnockdownTime = ".25";
            worldConfigured.mPassOutForce = "-10";
            worldConfigured.mPassOutTime = "2";
            fixture.mWorld.loadOblivionPhysicalBlendConfiguration(14, worldConfigured);
            fixture.mWorld.beginOblivionActorPhysicalPose(animation, ptr, renderedGraph, placement, linkedFilters,
                MWPhysics::CollisionType_Actor, MWPhysics::CollisionType_World, &internalFilter);
            EXPECT_EQ(physics.captureActorRagdollBlendStates(ptr)[0].mGains.mHierarchy, .53125f);
            EXPECT_EQ(physics.captureActorRagdollBlendStates(ptr)[0].mGains.mVelocity, .875f);
            const auto beforeWorldHitPose = physics.captureActorRagdoll(ptr)[0].mPose;
            const auto beforeWorldHit = physics.captureActorRagdollSnapshot(ptr, base, path.value());
            const std::array worldHitRequests{MWWorld::OblivionPhysicalHitBlendRequest{8, 0xffffe108u}};
            EXPECT_THROW(fixture.mWorld.prepareOblivionActorHitBlendControllers(previous, worldHitRequests, false),
                std::invalid_argument);
            const std::array badWorldHit{worldHitRequests[0], MWWorld::OblivionPhysicalHitBlendRequest{999, 0x1108}};
            EXPECT_THROW(fixture.mWorld.prepareOblivionActorHitBlendControllers(ptr, badWorldHit, false), std::invalid_argument);
            EXPECT_EQ(physics.captureActorRagdollSnapshot(ptr, base, path.value()), beforeWorldHit);
            auto hitResult = fixture.mWorld.prepareOblivionActorHitBlendControllers(ptr, worldHitRequests, false);
            ASSERT_EQ(hitResult.size(), 1u);
            EXPECT_EQ(hitResult[0], NifBullet::RagdollNativeHitBlendDisposition::Started);
            auto worldHitState = physics.captureActorRagdollBlendControllers(ptr)[0].mState;
            ASSERT_EQ(worldHitState.mKeys.size(), 3u);
            EXPECT_EQ(worldHitState.mKeys[0].mGains, (ESM4::PhysicalBlendGains{.4f, .6f}));
            EXPECT_EQ(worldHitState.mKeys[1].mGains, (ESM4::PhysicalBlendGains{.4f, .6f}));
            EXPECT_EQ(worldHitState.mTiming.mStopKey, 1.f);
            EXPECT_EQ(worldHitState.mSetupState, 1u);
            // The same owner selects the distinct QUADHIT table; high bits are masked.
            hitResult = fixture.mWorld.prepareOblivionActorHitBlendControllers(ptr, worldHitRequests, true);
            EXPECT_EQ(hitResult[0], NifBullet::RagdollNativeHitBlendDisposition::Started);
            worldHitState = physics.captureActorRagdollBlendControllers(ptr)[0].mState;
            EXPECT_EQ(worldHitState.mKeys[0].mGains, (ESM4::PhysicalBlendGains{.3f, .875f}));
            EXPECT_EQ(worldHitState.mKeys[1].mGains, (ESM4::PhysicalBlendGains{.3f, .9f}));
            EXPECT_EQ(physics.captureActorRagdoll(ptr)[0].mPose, beforeWorldHitPose);
            const auto beforeCompoundHit = physics.captureActorRagdollSnapshot(ptr, base, path.value());
            const std::array worldHitVelocities{MWWorld::OblivionPhysicalHitVelocityRequest{8, {1, -2, .5f, 0}, .5f}};
            EXPECT_THROW(fixture.mWorld.prepareOblivionActorHitControllers(previous, worldHitRequests, false, worldHitVelocities),
                std::invalid_argument);
            const std::array badWorldHitVelocities{worldHitVelocities[0],
                MWWorld::OblivionPhysicalHitVelocityRequest{999, {}, 1}};
            EXPECT_THROW(fixture.mWorld.prepareOblivionActorHitControllers(ptr, worldHitRequests, false, badWorldHitVelocities),
                std::invalid_argument);
            EXPECT_EQ(physics.captureActorRagdollSnapshot(ptr, base, path.value()), beforeCompoundHit);
            hitResult = fixture.mWorld.prepareOblivionActorHitControllers(ptr, worldHitRequests, false, worldHitVelocities);
            ASSERT_EQ(hitResult.size(), 1u);
            EXPECT_EQ(hitResult[0], NifBullet::RagdollNativeHitBlendDisposition::Started);
            const auto compoundHitVelocity = physics.captureActorRagdollVelocityControllers(ptr);
            ASSERT_EQ(compoundHitVelocity.size(), 1u);
            EXPECT_EQ(compoundHitVelocity[0].mState.mTiming.mStopKey, .2f);
            EXPECT_EQ(compoundHitVelocity[0].mState.mForceVector, (std::array<float, 4>{2, -4, 1, 0}));
            EXPECT_EQ(compoundHitVelocity[0].mTargetNode, 8u);
            EXPECT_EQ(physics.captureActorRagdollBlendControllers(ptr)[0].mState.mTiming.mStopKey, 1.f);
            EXPECT_EQ(physics.captureActorRagdoll(ptr)[0].mPose, beforeWorldHitPose);
            physics.restoreActorRagdollSnapshot(ptr, beforeWorldHit, base, path.value());
            const std::array worldDownRequests{MWWorld::OblivionPhysicalDownRequest{8, 0x1108, {1, -2, .5}}};
            const auto beforeDown = physics.captureActorRagdollSnapshot(ptr, base, path.value());
            // The same owner selects disabled per-body durations and validates
            // admitted pass-out settings before publishing either controller.
            const std::array disabledWorldDown{
                MWWorld::OblivionPhysicalDownRequest{8, 0xfffff908u, {1, -2, .5}}};
            const auto disabledResult
                = fixture.mWorld.prepareOblivionActorKnockdownControllers(ptr, disabledWorldDown);
            ASSERT_EQ(disabledResult.size(), 1u);
            EXPECT_EQ(disabledResult[0], NifBullet::RagdollNativeKnockdownBlendDisposition::Disabled);
            EXPECT_EQ(physics.captureActorRagdollSnapshot(ptr, base, path.value()), beforeDown);
            worldConfigured.mPassOutTime = "-2";
            fixture.mWorld.loadOblivionPhysicalBlendConfiguration(14, worldConfigured);
            EXPECT_THROW(fixture.mWorld.prepareOblivionActorKnockdownControllers(ptr, worldDownRequests),
                std::invalid_argument);
            EXPECT_EQ(physics.captureActorRagdollSnapshot(ptr, base, path.value()), beforeDown);
            worldConfigured.mPassOutTime = "2";
            fixture.mWorld.loadOblivionPhysicalBlendConfiguration(14, worldConfigured);
            EXPECT_EQ(physics.captureActorRagdollSnapshot(ptr, base, path.value()), beforeDown);
            const std::array lateBadDown{worldDownRequests[0],
                MWWorld::OblivionPhysicalDownRequest{999, 0x1108, {1, 0, 0}}};
            EXPECT_THROW(fixture.mWorld.prepareOblivionActorKnockdownControllers(ptr, lateBadDown),
                std::invalid_argument);
            EXPECT_EQ(physics.captureActorRagdollSnapshot(ptr, base, path.value()), beforeDown);
            auto downResult = fixture.mWorld.prepareOblivionActorKnockdownControllers(ptr, worldDownRequests);
            ASSERT_EQ(downResult.size(), 1u);
            EXPECT_EQ(downResult[0], NifBullet::RagdollNativeKnockdownBlendDisposition::Started);
            const auto afterWorldDown = physics.captureActorRagdollSnapshot(ptr, base, path.value());
            hitResult = fixture.mWorld.prepareOblivionActorHitBlendControllers(ptr, worldHitRequests, false);
            ASSERT_EQ(hitResult.size(), 1u);
            EXPECT_EQ(hitResult[0], NifBullet::RagdollNativeHitBlendDisposition::StrongerSetup);
            EXPECT_EQ(physics.captureActorRagdollSnapshot(ptr, base, path.value()), afterWorldDown);
            EXPECT_EQ(physics.captureActorRagdollBlendControllers(ptr)[0].mState.mTiming.mStopKey, .25f);
            const auto worldConfiguredVelocity = physics.captureActorRagdollVelocityControllers(ptr);
            ASSERT_EQ(worldConfiguredVelocity.size(), 1u);
            EXPECT_EQ(worldConfiguredVelocity[0].mState.mTiming.mStopKey, 2.f);
            // Native oracle210 mass2 values scale by2 for the admitted mass4.
            const std::array<std::uint32_t, 4> worldConfiguredBits{
                3233210841u, 1094115801u, 3224822233u, 1065353216u};
            for (unsigned axis = 0; axis < 4; ++axis)
                EXPECT_EQ(std::bit_cast<std::uint32_t>(worldConfiguredVelocity[0].mState.mForceVector[axis]),
                    worldConfiguredBits[axis]);
            MWWorld::endNativeActorPhysicalPose(physics, animation, ptr);
            // Teardown still releases bodies/capsule if renderer rebuilding fails.
            beginPhysical(renderedGraph);
            animation.mThrowRebuild = true;
            EXPECT_THROW(MWWorld::endNativeActorPhysicalPose(physics, animation, ptr), std::runtime_error);
            EXPECT_FALSE(animation.hasPhysicalPose());
            EXPECT_FALSE(physics.hasActorRagdoll(ptr));
            EXPECT_FALSE(capsule->isCollisionSuspended());
            animation.mThrowRebuild = false;
            beginPhysical(renderedGraph);
            physicalRoot->removeChild(physicalBone);
            EXPECT_THROW(MWWorld::endNativeActorPhysicalPose(physics, animation, ptr), std::invalid_argument);
            EXPECT_FALSE(animation.hasPhysicalPose());
            EXPECT_FALSE(physics.hasActorRagdoll(ptr));
            EXPECT_FALSE(capsule->isCollisionSuspended());
            physicalRoot->addChild(physicalBone);
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
        acceptNativeSnapshot(fixture, saved);
        actor.getRefData().setCustomData(nullptr);
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
            acceptNativeSnapshot(fixture, saved);
            actor.getRefData().setCustomData(nullptr);
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
            prepareNativeSnapshotPlayer(fixture, saved);
            readNativeSnapshot(fixture, saved);
            EXPECT_EQ(fixture.mWorld.prepareOblivionSavedActorInventory(actor), nullptr);
            const auto before = captureNativeActorState(fixture, actor).serializeBinary();
            for (int attempt = 0; attempt < 2; ++attempt)
            {
                EXPECT_THROW(fixture.mWorld.applyOblivionRuntimeState(), std::runtime_error);
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
            prepareNativeSnapshotPlayer(fixture, saved);
            readNativeSnapshot(fixture, saved);
            EXPECT_EQ(fixture.mWorld.prepareOblivionSavedActorInventory(actor), nullptr);
            const auto before = captureNativeActorState(fixture, actor).serializeBinary();
            for (int attempt = 0; attempt < 2; ++attempt)
            {
                EXPECT_THROW(fixture.mWorld.applyOblivionRuntimeState(), std::runtime_error);
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
                acceptNativeSnapshot(fixture, saved);
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
        acceptNativeSnapshot(fixture, saved);
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

    TEST(OblivionWorldTest, EmptyPhysicalRetentionDoesNotRequirePlayerOrInventProjections)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        ESM4::RuntimeState before;
        auto& combat = *world.getOblivionCombatService();
        combat.capture(before);
        ASSERT_NO_THROW(world.retainOblivionPhysicalState());
        ESM4::RuntimeState after;
        combat.capture(after);
        EXPECT_EQ(after, before);
        auto& physics = world.initializePhysics(new osg::Group);
        physics.restoreNativeBlendTimeCache({0xffffffffu, -2, 7, -0.f, -0.f});
        ASSERT_NO_THROW(world.retainOblivionPhysicalState());
        combat.capture(after);
        EXPECT_EQ(after, before);
        EXPECT_TRUE(physics.actorRagdollOwners().empty());
        EXPECT_EQ(physics.captureNativeBlendTimeCache().mCycle, 0xffffffffu);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(physics.captureNativeBlendTimeCache().mResult),
            0x80000000u);
    }

    TEST(OblivionWorldTest, RetainedPhysicalStateSurvivesBodyReleaseAndRejectsBadBindingsAtomically)
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
            const auto draft = addNativeNpc(fixture, 0x900);
            const auto actor = draft.getCell()->moveTo(draft, &residentCell);
            const auto retainedDraft = addNativeNpc(fixture, 0x901);
            const auto retained = retainedDraft.getCell()->moveTo(retainedDraft, &residentCell);
            world.getWorldModel().registerPtr(actor);
            world.getWorldModel().registerPtr(retained);
            const auto baseKey = actor.get<ESM4::Npc>()->mBase->mFormKey;
            auto npc = *actor.get<ESM4::Npc>()->mBase;
            npc.mModel = "physical-owner.osgt";
            store.getWritable<ESM4::Npc>().insertStatic(npc, baseKey);
            ASSERT_TRUE(world.initializeOblivionNonPlayerActor(actor, ESM4::ActorValueProcess::Active));
            ASSERT_TRUE(world.initializeOblivionNonPlayerActor(retained, ESM4::ActorValueProcess::Active));
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
            std::filesystem::create_directories(fixture.mDirectory / "meshes");
            ASSERT_TRUE(osgDB::writeNodeFile(*model, (fixture.mDirectory / "meshes/physical-owner.osgt").string()));
            fixture.mVfs.addArchive(std::make_unique<VFS::FileSystemArchive>(fixture.mDirectory));
            fixture.mVfs.buildIndex();
            const VFS::Path::Normalized path("meshes/physical-owner.osgt");
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
            auto& physics = world.initializePhysics(new osg::Group);
            physics.addActor(actor, path);
            NifBullet::ActorRagdollDefinition graph;
            graph.mSourceHash = std::string(16, 'a');
            NifBullet::RagdollBodyDefinition body{};
            body.mRecord = 12; body.mNodeRecord = 8;
            body.mMass = 2; body.mInertia = {1, 0, 0, 0, 1, 0, 0, 0, 1};
            body.mShape = NifBullet::RagdollSphere{.5f};
            body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .9f, .8f};
            body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
                78, 8, 0xd, 1.f, 0.f, 0.f, .25f, {{.25f, 1.f, 1.f}}};
            graph.mBodies.push_back(body);
            const std::array<btTransform, 1> poses{
                btTransform(btQuaternion::getIdentity(), btVector3(0, 0, 20))};
            NifBullet::RagdollInternalCollisionFilter internalFilter;
            internalFilter.mSystemGroup = 10;
            physics.addActorRagdoll(actor, graph, 1, poses,
                MWPhysics::CollisionType_Actor, MWPhysics::CollisionType_World, &internalFilter);
            const auto actorKey = actor.getCellRef().getFormKey();
            const auto retainedKey = retained.getCellRef().getFormKey();
            const std::string modelName(path.value());
            const auto cached = physics.captureActorRagdollSnapshot(actor, baseKey, modelName);
            ASSERT_TRUE(combat.syncActorRagdoll(actorKey, std::nullopt, cached));
            auto retainedPose = cached;
            retainedPose.mBodies[0].mPosition = {777, 888, 999};
            ASSERT_TRUE(combat.syncActorRagdoll(retainedKey, std::nullopt, retainedPose));
            auto projectedPlayer = *world.getPlayerPtr().get<ESM::NPC>()->mBase;
            projectedPlayer.mId = ESM::RefId::stringRefId("Player");
            projectedPlayer.mModel = "physical-owner.osgt";
            world.getPlayerPtr().get<ESM::NPC>()->mBase = store.insert(projectedPlayer);
            // Headless capsule construction has no camera/view-dependent race
            // scaling. Bind its ready cell after admission, before saving.
            world.getPlayer().setCell(nullptr);
            const auto playerAdmission = world.getPlayerPtr();
            physics.addActor(playerAdmission, path);
            world.getPlayer().setCell(&residentCell);
            const auto playerPtr = world.getPlayerPtr();
            physics.updatePtr(playerAdmission, playerPtr);
            internalFilter.mSystemGroup = 11;
            physics.addActorRagdoll(playerPtr, graph, 1, poses,
                MWPhysics::CollisionType_Actor, MWPhysics::CollisionType_World, &internalFilter);
            const auto playerKey = ESM::FormKey::dynamic("player", 1);
            const auto playerCached = physics.captureActorRagdollSnapshot(playerPtr, values.mBase, modelName);
            ASSERT_TRUE(combat.syncActorRagdoll(playerKey, std::nullopt, playerCached));
            const std::array<MWPhysics::NativeRagdollSnapshotBinding, 2> bindings{{
                {actor, actorKey, baseKey, modelName},
                {playerPtr, playerKey, values.mBase, modelName}}};
            auto current = physics.captureActorRagdollSnapshots(bindings);
            current.mActors.at(actorKey).mBodies[0].mPosition = {99, 88, 77};
            current.mActors.at(actorKey).mBodies[0].mNativePackedVelocity->mLinear[3] = 8;
            current.mActors.at(playerKey).mBodies[0].mPosition = {66, 55, 44};
            current.mActors.at(playerKey).mBodies[0].mNativePackedVelocity->mAngular[3] = -0.f;
            current.mTimeCache = ESM4::PhysicalBlendTimeCache{2, 4, 0, 1, 3};
            for (auto& [key, pose] : current.mActors)
            {
                auto& authored = pose.mNativeControllers->mBlends[0].mState;
                authored.mClock = {10, 11, 1};
                authored.mCachedGains = {-0.f, -2};
                authored.mCursor = 0xffffffffu; // A single key retains the opaque cursor.
                authored.mSetupState = 0xffffffffu;
                ESM4::PhysicalVelocityControllerState velocity;
                velocity.mTiming = {0xd, 1, 0, 0, 4};
                velocity.mClock = {10, 11, 1};
                velocity.mForceVector = {1, 2, 3, 8};
                velocity.mFrameDelta = .1f;
                pose.mNativeControllers->mVelocities.push_back({8, 8, false, velocity});
                pose.mNativeBlends->at(0).mRequestedMotion = 1;
                pose.mNativeBlends->at(0).mHierarchyGain = .25f;
                pose.mNativeBlends->at(0).mVelocityGain = .75f;
            }
            physics.restoreActorRagdollSnapshots(current, bindings);
            const auto owners = physics.actorRagdollOwners();
            ASSERT_EQ(owners.size(), 2u);
            const auto lateKey = owners.back() == world.getPlayerPtr() ? playerKey
                : owners.back().getCellRef().getFormKey();
            auto oldState = world.captureOblivionRuntimeState();
            combat.capture(oldState); // Replace fresh save projections with old native caches.
            const auto originalLate = oldState.mNativeActorRagdolls.at(lateKey);
            ASSERT_TRUE(combat.syncActorRagdoll(lateKey, originalLate, std::nullopt));
            auto wrong = originalLate;
            wrong.mAssetHash.back() = 'b';
            ASSERT_TRUE(combat.syncActorRagdoll(lateKey, std::nullopt, wrong));
            EXPECT_ANY_THROW(world.retainOblivionPhysicalState());
            auto after = oldState;
            combat.capture(after);
            auto expected = oldState;
            expected.mNativeActorRagdolls.at(lateKey) = wrong;
            EXPECT_EQ(after.mNativeActorRagdolls, expected.mNativeActorRagdolls);
            EXPECT_EQ(physics.captureActorRagdollSnapshots(bindings), current);
            ASSERT_TRUE(combat.syncActorRagdoll(lateKey, wrong, std::nullopt));
            ASSERT_TRUE(combat.syncActorRagdoll(lateKey, std::nullopt, originalLate));
            ASSERT_NO_THROW(world.retainOblivionPhysicalState());
            for (const auto& [key, pose] : current.mActors)
                EXPECT_EQ(combat.actorRagdoll(key), pose);
            EXPECT_EQ(combat.actorRagdoll(retainedKey), retainedPose);
            const auto retainedSnapshot = world.captureOblivionRuntimeState();
            // The real physical release removes both owners/capsules. Native
            // projection retention must survive without any body to recapture.
            physics.remove(actor);
            physics.remove(world.getPlayerPtr());
            EXPECT_TRUE(physics.actorRagdollOwners().empty());
            ASSERT_NO_THROW(world.retainOblivionPhysicalState());
            const auto released = world.captureOblivionRuntimeState();
            EXPECT_EQ(released.mNativeActorRagdolls, retainedSnapshot.mNativeActorRagdolls);
            EXPECT_EQ(released.mNativePhysicalBlendTimeCache, retainedSnapshot.mNativePhysicalBlendTimeCache);
            const auto decoded = ESM4::RuntimeState::deserializeBinary(released.serializeBinary());
            EXPECT_EQ(decoded.mNativeActorRagdolls, released.mNativeActorRagdolls);
            EXPECT_EQ(decoded.mNativePhysicalBlendTimeCache, released.mNativePhysicalBlendTimeCache);
            EXPECT_EQ(std::bit_cast<std::uint32_t>(
                decoded.mNativeActorRagdolls.at(playerKey).mBodies[0].mNativePackedVelocity->mAngular[3]),
                0x80000000u);
        }
    }

    TEST(OblivionWorldTest, WorldApplyStagesLoadedPhysicalOwnersBeforeWorldPublication)
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
            const auto draft = addNativeNpc(fixture, 0x900);
            const auto actor = draft.getCell()->moveTo(draft, &residentCell);
            const auto retainedDraft = addNativeNpc(fixture, 0x901);
            const auto retained = retainedDraft.getCell()->moveTo(retainedDraft, &residentCell);
            world.getWorldModel().registerPtr(actor);
            world.getWorldModel().registerPtr(retained);
            const auto baseKey = actor.get<ESM4::Npc>()->mBase->mFormKey;
            auto npc = *actor.get<ESM4::Npc>()->mBase;
            npc.mModel = "physical-owner.osgt";
            store.getWritable<ESM4::Npc>().insertStatic(npc, baseKey);
            ASSERT_TRUE(world.initializeOblivionNonPlayerActor(actor, ESM4::ActorValueProcess::Active));
            ASSERT_TRUE(world.initializeOblivionNonPlayerActor(retained, ESM4::ActorValueProcess::Active));
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
            std::filesystem::create_directories(fixture.mDirectory / "meshes");
            ASSERT_TRUE(osgDB::writeNodeFile(*model, (fixture.mDirectory / "meshes/physical-owner.osgt").string()));
            fixture.mVfs.addArchive(std::make_unique<VFS::FileSystemArchive>(fixture.mDirectory));
            fixture.mVfs.buildIndex();
            const VFS::Path::Normalized path("meshes/physical-owner.osgt");
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
            auto& physics = world.initializePhysics(new osg::Group);
            physics.addActor(actor, path);
            NifBullet::ActorRagdollDefinition graph;
            graph.mSourceHash = std::string(16, 'a');
            NifBullet::RagdollBodyDefinition body{};
            body.mRecord = 12; body.mNodeRecord = 8;
            body.mMass = 2; body.mInertia = {1, 0, 0, 0, 1, 0, 0, 0, 1};
            body.mShape = NifBullet::RagdollSphere{.5f};
            body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .9f, .8f};
            body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
                78, 8, 0xd, 1.f, 0.f, 0.f, .25f, {{.25f, 1.f, 1.f}}};
            graph.mBodies.push_back(body);
            const std::array<btTransform, 1> poses{
                btTransform(btQuaternion::getIdentity(), btVector3(0, 0, 20))};
            NifBullet::RagdollInternalCollisionFilter internalFilter;
            internalFilter.mSystemGroup = 10;
            physics.addActorRagdoll(actor, graph, 1, poses,
                MWPhysics::CollisionType_Actor, MWPhysics::CollisionType_World, &internalFilter);
            const auto actorKey = actor.getCellRef().getFormKey();
            const auto retainedKey = retained.getCellRef().getFormKey();
            const std::string modelName(path.value());
            const auto cached = physics.captureActorRagdollSnapshot(actor, baseKey, modelName);
            ASSERT_TRUE(combat.syncActorRagdoll(actorKey, std::nullopt, cached));
            auto retainedPose = cached;
            retainedPose.mBodies[0].mPosition = {777, 888, 999};
            ASSERT_TRUE(combat.syncActorRagdoll(retainedKey, std::nullopt, retainedPose));
            auto projectedPlayer = *world.getPlayerPtr().get<ESM::NPC>()->mBase;
            projectedPlayer.mId = ESM::RefId::stringRefId("Player");
            projectedPlayer.mModel = "physical-owner.osgt";
            world.getPlayerPtr().get<ESM::NPC>()->mBase = store.insert(projectedPlayer);
            // Headless capsule construction has no camera/view-dependent race
            // scaling. Bind its ready cell after admission, before saving.
            world.getPlayer().setCell(nullptr);
            const auto playerAdmission = world.getPlayerPtr();
            physics.addActor(playerAdmission, path);
            world.getPlayer().setCell(&residentCell);
            const auto playerPtr = world.getPlayerPtr();
            physics.updatePtr(playerAdmission, playerPtr);
            internalFilter.mSystemGroup = 11;
            physics.addActorRagdoll(playerPtr, graph, 1, poses,
                MWPhysics::CollisionType_Actor, MWPhysics::CollisionType_World, &internalFilter);
            const auto playerKey = ESM::FormKey::dynamic("player", 1);
            const auto playerCached = physics.captureActorRagdollSnapshot(playerPtr, values.mBase, modelName);
            ASSERT_TRUE(combat.syncActorRagdoll(playerKey, std::nullopt, playerCached));
            const std::array<MWPhysics::NativeRagdollSnapshotBinding, 2> bindings{{
                {actor, actorKey, baseKey, modelName},
                {playerPtr, playerKey, values.mBase, modelName}}};
            auto current = physics.captureActorRagdollSnapshots(bindings);
            current.mActors.at(actorKey).mBodies[0].mPosition = {99, 88, 77};
            current.mActors.at(actorKey).mBodies[0].mNativePackedVelocity->mLinear[3] = 8;
            current.mActors.at(playerKey).mBodies[0].mPosition = {66, 55, 44};
            current.mActors.at(playerKey).mBodies[0].mNativePackedVelocity->mAngular[3] = -0.f;
            current.mTimeCache = ESM4::PhysicalBlendTimeCache{2, 4, 0, 1, 3};
            for (auto& [key, pose] : current.mActors)
            {
                auto& authored = pose.mNativeControllers->mBlends[0].mState;
                authored.mClock = {10, 11, 1};
                authored.mCachedGains = {-0.f, -2};
                authored.mCursor = 0xffffffffu; // A single key retains the opaque cursor.
                authored.mSetupState = 0xffffffffu;
                ESM4::PhysicalVelocityControllerState velocity;
                velocity.mTiming = {0xd, 1, 0, 0, 4};
                velocity.mClock = {10, 11, 1};
                velocity.mForceVector = {1, 2, 3, 8};
                velocity.mFrameDelta = .1f;
                pose.mNativeControllers->mVelocities.push_back({8, 8, false, velocity});
                pose.mNativeBlends->at(0).mRequestedMotion = 1;
                pose.mNativeBlends->at(0).mHierarchyGain = .25f;
                pose.mNativeBlends->at(0).mVelocityGain = .75f;
            }
            physics.restoreActorRagdollSnapshots(current, bindings);
            const auto saved = world.captureOblivionRuntimeState();
            auto moved = current;
            for (auto& [key, pose] : moved.mActors)
            {
                pose.mBodies[0].mPosition = {123, 234, 345};
                pose.mBodies[0].mNativePackedVelocity->mLinear[3] = 99;
                pose.mNativeControllers->mBlends[0].mState.mClock = {20, 21, 2};
                pose.mNativeControllers->mVelocities.clear();
                pose.mNativeBlends->at(0).mRequestedMotion = 8;
            }
            moved.mTimeCache = ESM4::PhysicalBlendTimeCache{1, 8, 0, 2, 4};
            physics.restoreActorRagdollSnapshots(moved, bindings);
            readNativeSnapshot(fixture, saved);
            ASSERT_NO_THROW(world.applyOblivionRuntimeState());
            EXPECT_EQ(physics.captureActorRagdollSnapshots(bindings), current);
            EXPECT_EQ(world.captureOblivionRuntimeState().mNativeActorRagdolls, saved.mNativeActorRagdolls);
            EXPECT_EQ(combat.actorRagdoll(retainedKey), retainedPose);
            // Use actual scheduler owner order so the last body's bad binding
            // is reached only after any earlier owner is staged.
            const auto owners = physics.actorRagdollOwners();
            ASSERT_EQ(owners.size(), 2u);
            const auto lateKey = owners.back() == world.getPlayerPtr() ? playerKey
                : owners.back().getCellRef().getFormKey();
            auto otherCell = cell;
            otherCell.mId = ESM::RefId(ESM::FormId{2, 0});
            otherCell.mFormKey = ESM::FormKey::content("headless.esm", 2);
            otherCell.mEditorId = "OtherPhysicalRestoreCell";
            store.getWritable<ESM4::Cell>().insertStatic(otherCell, otherCell.mFormKey);
            for (unsigned field = 0; field < 9; ++field)
            {
                const auto previous = physics.captureActorRagdollSnapshots(bindings);
                const auto previousClock = world.getTimeStamp();
                const auto previousValues = *combat.findActorValues(playerKey);
                const auto previousPosition = actor.getRefData().getPosition();
                const auto previousScale = actor.getCellRef().getScale();
                const auto previousEnabled = actor.getRefData().isEnabled();
                auto bad = saved;
                for (auto& actorValues : bad.mNativeActorValues)
                    if (actorValues.mActor == playerKey)
                        actorValues.mValues[8].mModifiers[1] = -7;
                bad.mClock.mHour = 9;
                bad.mNativePhysicalBlendTimeCache = ESM4::PhysicalBlendTimeCache{3, 8, 0, 2, 123};
                for (auto& [key, pose] : bad.mNativeActorRagdolls)
                    pose.mBodies[0].mPosition = {999, 999, 999};
                auto& late = bad.mNativeActorRagdolls.at(lateKey);
                if (field == 0) late.mAssetHash.back() = 'b';
                if (field == 1) late.mNativeControllers->mBlends[0].mRecord = 79;
                if (field == 2) late.mModel = "meshes/other.osgt";
                auto npcReference = std::find_if(bad.mReferences.begin(), bad.mReferences.end(),
                    [&](const auto& ref) { return ref.mKey == actorKey; });
                ASSERT_NE(npcReference, bad.mReferences.end());
                if (field == 3) npcReference->mCustomState["scale"] = 2.;
                if (field == 4) npcReference->mEnabled = false;
                if (field == 5) npcReference->mCell = otherCell.mFormKey;
                if (field == 6) bad.mPlayer.mCell = otherCell.mFormKey;
                if (field == 7) bad.mPlayer.mRace = ESM::FormKey::content("headless.esm", 0x812);
                if (field == 8) bad.mPlayer.mFemale = !bad.mPlayer.mFemale;
                ASSERT_NO_THROW(bad.validate());
                readNativeSnapshot(fixture, bad);
                EXPECT_ANY_THROW(world.applyOblivionRuntimeState());
                EXPECT_EQ(physics.captureActorRagdollSnapshots(bindings), previous);
                EXPECT_EQ(world.getTimeStamp(), previousClock);
                EXPECT_EQ(*combat.findActorValues(playerKey), previousValues);
                EXPECT_EQ(actor.getRefData().getPosition(), previousPosition);
                EXPECT_EQ(actor.getCellRef().getScale(), previousScale);
                EXPECT_EQ(actor.getRefData().isEnabled(), previousEnabled);
                readNativeSnapshot(fixture, saved);
                ASSERT_NO_THROW(world.applyOblivionRuntimeState());
            }
        }
    }

    TEST(OblivionWorldTest, WorldSaveRefreshesLivePhysicalOwnersAndPreservesRetainedPoses)
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
            const auto draft = addNativeNpc(fixture, 0x900);
            const auto actor = draft.getCell()->moveTo(draft, &residentCell);
            const auto retainedDraft = addNativeNpc(fixture, 0x901);
            const auto retained = retainedDraft.getCell()->moveTo(retainedDraft, &residentCell);
            world.getWorldModel().registerPtr(actor);
            world.getWorldModel().registerPtr(retained);
            const auto baseKey = actor.get<ESM4::Npc>()->mBase->mFormKey;
            auto npc = *actor.get<ESM4::Npc>()->mBase;
            npc.mModel = "physical-owner.osgt";
            store.getWritable<ESM4::Npc>().insertStatic(npc, baseKey);
            ASSERT_TRUE(world.initializeOblivionNonPlayerActor(actor, ESM4::ActorValueProcess::Active));
            ASSERT_TRUE(world.initializeOblivionNonPlayerActor(retained, ESM4::ActorValueProcess::Active));
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
            std::filesystem::create_directories(fixture.mDirectory / "meshes");
            ASSERT_TRUE(osgDB::writeNodeFile(*model, (fixture.mDirectory / "meshes/physical-owner.osgt").string()));
            fixture.mVfs.addArchive(std::make_unique<VFS::FileSystemArchive>(fixture.mDirectory));
            fixture.mVfs.buildIndex();
            const VFS::Path::Normalized path("meshes/physical-owner.osgt");
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
            auto& physics = world.initializePhysics(new osg::Group);
            physics.addActor(actor, path);
            NifBullet::ActorRagdollDefinition graph;
            graph.mSourceHash = std::string(16, 'a');
            NifBullet::RagdollBodyDefinition body{};
            body.mRecord = 12; body.mNodeRecord = 8;
            body.mMass = 2; body.mInertia = {1, 0, 0, 0, 1, 0, 0, 0, 1};
            body.mShape = NifBullet::RagdollSphere{.5f};
            graph.mBodies.push_back(body);
            const std::array<btTransform, 1> poses{
                btTransform(btQuaternion::getIdentity(), btVector3(0, 0, 20))};
            NifBullet::RagdollInternalCollisionFilter internalFilter;
            internalFilter.mSystemGroup = 10;
            physics.addActorRagdoll(actor, graph, 1, poses,
                MWPhysics::CollisionType_Actor, MWPhysics::CollisionType_World, &internalFilter);
            const auto actorKey = actor.getCellRef().getFormKey();
            const auto retainedKey = retained.getCellRef().getFormKey();
            const std::string modelName(path.value());
            const auto cached = physics.captureActorRagdollSnapshot(actor, baseKey, modelName);
            ASSERT_TRUE(combat.syncActorRagdoll(actorKey, std::nullopt, cached));
            auto retainedPose = cached;
            retainedPose.mBodies[0].mPosition = {777, 888, 999};
            ASSERT_TRUE(combat.syncActorRagdoll(retainedKey, std::nullopt, retainedPose));
            auto projectedPlayer = *world.getPlayerPtr().get<ESM::NPC>()->mBase;
            projectedPlayer.mId = ESM::RefId::stringRefId("NativePhysicalCapturePlayer");
            projectedPlayer.mModel = "physical-owner.osgt";
            store.insertStatic(projectedPlayer);
            world.getPlayerPtr().get<ESM::NPC>()->mBase = store.get<ESM::NPC>().find(projectedPlayer.mId);
            // Headless capsule construction has no camera/view-dependent race
            // scaling. Bind its ready cell after admission, before saving.
            world.getPlayer().setCell(nullptr);
            const auto playerAdmission = world.getPlayerPtr();
            physics.addActor(playerAdmission, path);
            world.getPlayer().setCell(&residentCell);
            const auto playerPtr = world.getPlayerPtr();
            physics.updatePtr(playerAdmission, playerPtr);
            internalFilter.mSystemGroup = 11;
            physics.addActorRagdoll(playerPtr, graph, 1, poses,
                MWPhysics::CollisionType_Actor, MWPhysics::CollisionType_World, &internalFilter);
            const auto playerKey = ESM::FormKey::dynamic("player", 1);
            const auto playerCached = physics.captureActorRagdollSnapshot(playerPtr, values.mBase, modelName);
            ASSERT_TRUE(combat.syncActorRagdoll(playerKey, std::nullopt, playerCached));
            const std::array<MWPhysics::NativeRagdollSnapshotBinding, 2> bindings{{
                {actor, actorKey, baseKey, modelName},
                {playerPtr, playerKey, values.mBase, modelName}}};
            auto current = physics.captureActorRagdollSnapshots(bindings);
            current.mActors.at(actorKey).mBodies[0].mPosition = {99, 88, 77};
            current.mActors.at(actorKey).mBodies[0].mNativePackedVelocity->mLinear[3] = 8;
            current.mActors.at(playerKey).mBodies[0].mPosition = {66, 55, 44};
            current.mActors.at(playerKey).mBodies[0].mNativePackedVelocity->mAngular[3] = -0.f;
            current.mTimeCache = ESM4::PhysicalBlendTimeCache{2, 4, 0, 1, 3};
            physics.restoreActorRagdollSnapshots(current, bindings);
            const auto captured = world.captureOblivionRuntimeState();
            EXPECT_EQ(captured.mNativeActorRagdolls.at(actorKey), current.mActors.at(actorKey));
            EXPECT_EQ(captured.mNativeActorRagdolls.at(playerKey), current.mActors.at(playerKey));
            EXPECT_EQ(std::bit_cast<std::uint32_t>(
                captured.mNativeActorRagdolls.at(playerKey).mBodies[0].mNativePackedVelocity->mAngular[3]),
                0x80000000u);
            EXPECT_EQ(captured.mNativeActorRagdolls.at(retainedKey), retainedPose);
            EXPECT_EQ(captured.mNativePhysicalBlendTimeCache, current.mTimeCache);
            // Save is read-only: stale service projections remain independently detectable.
            EXPECT_EQ(combat.actorRagdoll(actorKey), cached);
            EXPECT_EQ(combat.actorRagdoll(playerKey), playerCached);
            const auto decoded = ESM4::RuntimeState::deserializeBinary(captured.serializeBinary());
            EXPECT_EQ(decoded.mNativeActorRagdolls, captured.mNativeActorRagdolls);
            EXPECT_EQ(decoded.mNativePhysicalBlendTimeCache, captured.mNativePhysicalBlendTimeCache);
            ASSERT_TRUE(combat.syncActorRagdoll(actorKey, cached, std::nullopt));
            EXPECT_ANY_THROW(world.captureOblivionRuntimeState());
            auto wrong = cached; wrong.mAssetHash.back() = 'b';
            ASSERT_TRUE(combat.syncActorRagdoll(actorKey, std::nullopt, wrong));
            EXPECT_ANY_THROW(world.captureOblivionRuntimeState());
            ASSERT_TRUE(combat.syncActorRagdoll(actorKey, wrong, std::nullopt));
            wrong = cached; wrong.mModel = "meshes/other.osgt";
            ASSERT_TRUE(combat.syncActorRagdoll(actorKey, std::nullopt, wrong));
            EXPECT_ANY_THROW(world.captureOblivionRuntimeState());
            ASSERT_TRUE(combat.syncActorRagdoll(actorKey, wrong, std::nullopt));
            for (unsigned field = 0; field < 3; ++field)
            {
                wrong = cached;
                if (field == 0) wrong.mBodies[0].mRecord = 13;
                if (field == 1) wrong.mBodies[0].mNodeRecord = 9;
                if (field == 2)
                {
                    auto extra = wrong.mBodies[0]; extra.mRecord = 13; extra.mNodeRecord = 9;
                    wrong.mBodies.push_back(extra);
                }
                ASSERT_TRUE(combat.syncActorRagdoll(actorKey, std::nullopt, wrong));
                EXPECT_ANY_THROW(world.captureOblivionRuntimeState());
                ASSERT_TRUE(combat.syncActorRagdoll(actorKey, wrong, std::nullopt));
            }
            ASSERT_TRUE(combat.syncActorRagdoll(actorKey, std::nullopt, cached));
        }
    }

    TEST(OblivionWorldTest, WorldApplyRestoresPhysicalCacheAfterValidationAndPreservesLegacyAbsence)
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
            auto saved = world.captureOblivionRuntimeState();
            const ESM4::PhysicalBlendTimeCache expected{0xffffffffu, 1, -1, -0.f, -.25f};
            saved.mNativePhysicalBlendTimeCache = expected;
            readNativeSnapshot(fixture, saved);
            auto& physics = world.initializePhysics(new osg::Group);
            EXPECT_NE(physics.captureNativeBlendTimeCache(), expected);
            const ESM4::PhysicalBlendTimeCache prior{2, 4, 0, 1, 3};
            physics.restoreNativeBlendTimeCache(prior);
            ASSERT_NO_THROW(world.applyOblivionRuntimeState());
            EXPECT_EQ(physics.captureNativeBlendTimeCache(), expected);
            EXPECT_EQ(std::bit_cast<std::uint32_t>(physics.captureNativeBlendTimeCache().mKeyTime), 0x80000000u);
            EXPECT_EQ(world.captureOblivionRuntimeState().mNativePhysicalBlendTimeCache,
                saved.mNativePhysicalBlendTimeCache);
            physics.restoreNativeBlendTimeCache(prior);
            auto legacy = saved;
            legacy.mVersion = 35;
            legacy.mNativePhysicalBlendTimeCache.reset();
            readNativeSnapshot(fixture, legacy);
            ASSERT_NO_THROW(world.applyOblivionRuntimeState());
            EXPECT_EQ(physics.captureNativeBlendTimeCache(), prior);
            auto bad = saved;
            bad.mClock.mHour = 9;
            ASSERT_EQ(bad.mNativeActorValues.size(), 1u);
            bad.mNativeActorValues[0].mValues[33].mModifiers[1] = 1e32f;
            ASSERT_NO_THROW(bad.validate());
            const auto previousClock = world.getTimeStamp();
            const auto previousValues = *combat.findActorValues(ESM::FormKey::dynamic("player", 1));
            readNativeSnapshot(fixture, bad);
            EXPECT_THROW(world.applyOblivionRuntimeState(), std::invalid_argument);
            EXPECT_EQ(physics.captureNativeBlendTimeCache(), prior);
            EXPECT_EQ(world.getTimeStamp(), previousClock);
            EXPECT_EQ(*combat.findActorValues(ESM::FormKey::dynamic("player", 1)), previousValues);
        }
    }

    TEST(OblivionWorldTest, WorldSaveCapturesPhysicalCacheWithNoLoadedRagdolls)
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
            const ESM4::PhysicalBlendTimeCache retainedCache{3, -2, -4, -0.f, .75f};
            auto retainedState = world.captureOblivionRuntimeState();
            EXPECT_FALSE(retainedState.mNativePhysicalBlendTimeCache);
            retainedState.mNativePhysicalBlendTimeCache = retainedCache;
            readNativeSnapshot(fixture, retainedState);
            EXPECT_FALSE(world.captureOblivionRuntimeState().mNativePhysicalBlendTimeCache);
            ASSERT_NO_THROW(world.applyOblivionRuntimeState());
            const auto withoutPhysics = world.captureOblivionRuntimeState();
            EXPECT_EQ(withoutPhysics.mNativePhysicalBlendTimeCache, retainedState.mNativePhysicalBlendTimeCache);
            auto* physics = &world.initializePhysics(new osg::Group);
            ASSERT_NE(physics, nullptr);
            EXPECT_EQ(physics->captureNativeBlendTimeCache(), retainedCache);
            EXPECT_THROW(world.initializePhysics(new osg::Group), std::logic_error);
            EXPECT_EQ(world.getRayCasting(), physics);
            const ESM4::PhysicalBlendTimeCache saved{0xffffffffu, 1, -1, -0.f, -.25f};
            physics->restoreNativeBlendTimeCache(saved);
            const auto captured = world.captureOblivionRuntimeState();
            ASSERT_TRUE(captured.mNativePhysicalBlendTimeCache);
            EXPECT_EQ(*captured.mNativePhysicalBlendTimeCache, saved);
            EXPECT_EQ(std::bit_cast<std::uint32_t>(captured.mNativePhysicalBlendTimeCache->mKeyTime), 0x80000000u);
            EXPECT_TRUE(captured.mNativeActorRagdolls.empty());
            const auto decoded = ESM4::RuntimeState::deserializeBinary(captured.serializeBinary());
            ASSERT_TRUE(decoded.mNativePhysicalBlendTimeCache);
            EXPECT_EQ(*decoded.mNativePhysicalBlendTimeCache, saved);
        }
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

    TEST(OblivionWorldTest, NativeItemAdditionPublishesBeforeObserversAndCapturesTheirFinalInventory)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        MWClass::Npc::registerSelf();
        world.setupPlayer();
        const auto weapon = addEquipmentWeapon(fixture);
        auto player = world.getPlayerPtr();
        auto& inventory = player.getClass().getInventoryStore(player);
        ESM4::RuntimeInventoryItem item; item.mBase = weapon; item.mCount = 2;
        item.mCondition = 43.125; item.mCharge = 7.25f;
        item.mOwner = ESM::FormKey::content("headless.esm", 0x800);
        item.mOwnershipRank = -1;
        // Install a real state cache, then replace it inside itemAdded.
        ESM4::RuntimeState cache;
        cache.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        cache.mPlayer.mCell = ESM::FormKey::content("headless.esm", 1);
        cache.mPlayer.mRace = ESM::FormKey::content("headless.esm", 0x810);
        cache.mPlayer.mClass = ESM::FormKey::dynamic("fixture-class", 1);
        readNativeSnapshot(fixture, cache);
        struct Listener : MWWorld::ContainerStoreListener
        {
            std::function<void(const MWWorld::ConstPtr&, int)> mCallback;
            int mCalls = 0;
            void itemAdded(const MWWorld::ConstPtr& ptr, int count) override
            {
                ++mCalls;
                mCallback(ptr, count);
            }
        } listener;
        listener.mCallback = [&](const MWWorld::ConstPtr& ptr, int count) {
            EXPECT_EQ(count, 2);
            EXPECT_EQ(ptr.getContainerStore(), &inventory);
            EXPECT_EQ(world.getWorldModel().getPtr(ptr.getCellRef().getRefNum()), ptr);
            EXPECT_EQ(inventory.count(ESM::RefId(ESM::FormId{0x940, 0})), 2);
            EXPECT_EQ(ptr.getCellRef().getNativeItemCondition(), 43.125f);
            EXPECT_EQ(ptr.getCellRef().getEnchantmentCharge(), 7.25f);
            EXPECT_EQ(ptr.getCellRef().getOwner(), ESM::RefId(ESM::FormId{0x800, 0}));
            EXPECT_EQ(ptr.getCellRef().getNativeOwnershipRank(), -1);
            readNativeSnapshot(fixture, cache);
            // Observer edits are authoritative; the old cache must not receive
            // a second independent add after the callback.
            const_cast<MWWorld::CellRef&>(ptr.getCellRef()).setCount(3);
        };
        inventory.setContListener(&listener);
        EXPECT_EQ(world.oblivionAddPlayerInventoryItem(item), 2);
        EXPECT_EQ(listener.mCalls, 1);
        const auto captured = world.captureOblivionActorInventory(player);
        ASSERT_EQ(captured.size(), 1u);
        EXPECT_EQ(captured.front().mCount, 3);
        EXPECT_EQ(captured.front().mCondition, 43.125);
        EXPECT_EQ(captured.front().mOwner, item.mOwner);
        EXPECT_EQ(captured.front().mOwnershipRank, -1);
        inventory.setContListener(nullptr);
    }

    TEST(OblivionWorldTest, NativeItemAdditionRejectsMalformedMetadataBeforeAnyPublication)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        MWClass::Npc::registerSelf();
        world.setupPlayer();
        const auto weapon = addEquipmentWeapon(fixture);
        const auto player = world.getPlayerPtr();
        ESM4::RuntimeInventoryItem item; item.mBase = weapon; item.mCount = 1; item.mCondition = 43.125;
        // Finish ordinary lazy Player/inventory admission before recording
        // the mutation boundary; observation itself must not register Player.
        EXPECT_TRUE(world.captureOblivionActorInventory(player).empty());
        const auto revision = world.getWorldModel().getPtrRegistryRevision();
        const auto serial = world.getWorldModel().getLastGeneratedRefNum();
        for (int invalid = 0; invalid < 12; ++invalid)
        {
            SCOPED_TRACE(invalid);
            auto bad = item;
            if (invalid == 0) bad.mCount = 0;
            if (invalid == 1) bad.mCount = -1;
            if (invalid == 2) bad.mCondition = -2;
            if (invalid == 3) bad.mCondition = std::numeric_limits<double>::infinity();
            if (invalid == 4) bad.mCharge = std::numeric_limits<float>::quiet_NaN();
            if (invalid == 5) bad.mCharge = -2.f;
            if (invalid == 6) bad.mRemainingUsageTime = std::numeric_limits<float>::infinity();
            if (invalid == 7) bad.mRemainingUsageTime = -2.f;
            if (invalid == 8) bad.mBase.mNamespace = "HEADLESS.ESM";
            if (invalid == 9) bad.mOwner = ESM::FormKey::content("missing.esm", 2);
            if (invalid == 10) bad.mOwner.mValue = 1;
            if (invalid == 11) bad.mOwnershipGlobal.mNamespace = "dirty-null";
            EXPECT_EQ(world.oblivionAddPlayerInventoryItem(bad), 0);
            EXPECT_TRUE(world.captureOblivionActorInventory(player).empty());
            EXPECT_EQ(world.getWorldModel().getPtrRegistryRevision(), revision);
            EXPECT_EQ(world.getWorldModel().getLastGeneratedRefNum(), serial);
        }
        // Canonical keys of the wrong winning type are a preflight error.
        item.mOwnershipGlobal = weapon;
        EXPECT_THROW(world.oblivionAddPlayerInventoryItem(item), std::invalid_argument);
        EXPECT_TRUE(world.captureOblivionActorInventory(player).empty());
        EXPECT_EQ(world.getWorldModel().getPtrRegistryRevision(), revision);
        EXPECT_EQ(world.getWorldModel().getLastGeneratedRefNum(), serial);
    }

    TEST(OblivionWorldTest, PreparedNativeItemAdditionCancelsPreservesEquipmentAndRejectsStaleTargets)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        MWClass::Npc::registerSelf();
        world.setupPlayer();
        const auto weapon = addEquipmentWeapon(fixture);
        const auto player = world.getPlayerPtr();
        auto& inventory = player.getClass().getInventoryStore(player);
        ESM4::RuntimeInventoryItem held; held.mBase = weapon; held.mCount = 1;
        held.mCondition = 43.125; held.mEquippedSlots = ESM4::InventorySlotWeapon;
        installEquipmentInventory(fixture, player, {held});
        const auto equipped = *inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
        ESM4::RuntimeInventoryItem item = held; item.mCondition = 100; item.mEquippedSlots = 0;
        const auto sources = MWWorld::OblivionProfileServices::prepareActorInventory(
            world.getStore(), ESM::FormKeyResolver({"headless.esm"}), {item});
        const auto source = sources.front().mReference.getPtr();
        const auto revision = world.getWorldModel().getPtrRegistryRevision();
        const auto serial = world.getWorldModel().getLastGeneratedRefNum();
        {
            auto cancelled = inventory.prepareItemAddition(source, 2);
            EXPECT_TRUE(cancelled->isValid());
            EXPECT_TRUE(cancelled->needsRegistration());
            EXPECT_EQ(inventory.count(source.getCellRef().getRefId()), 1);
            EXPECT_FALSE(cancelled->notify());
        }
        EXPECT_EQ(world.getWorldModel().getPtrRegistryRevision(), revision);
        EXPECT_EQ(world.getWorldModel().getLastGeneratedRefNum(), serial);
        ASSERT_EQ(world.oblivionAddPlayerInventoryItem(item), 1);
        EXPECT_EQ(*inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight), equipped);
        auto stale = inventory.prepareItemAddition(source, 2);
        ASSERT_FALSE(stale->needsRegistration());
        const auto target = stale->getItem();
        target.getCellRef().setCount(4);
        EXPECT_FALSE(stale->isValid());
        EXPECT_TRUE(stale->commit().isEmpty());
        EXPECT_EQ(target.getCellRef().getCount(), 4);
        auto valid = inventory.prepareItemAddition(source, 2);
        ASSERT_TRUE(valid->isValid());
        EXPECT_EQ(valid->commit(), target);
        EXPECT_EQ(target.getCellRef().getCount(), 6);
        EXPECT_TRUE(valid->notify());
        EXPECT_FALSE(valid->notify());
        EXPECT_EQ(*inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight), equipped);
        target.getCellRef().setCount(std::numeric_limits<int>::max());
        EXPECT_THROW(inventory.prepareItemAddition(source, 1), std::overflow_error);
        target.getCellRef().setCount(6);
        auto invalidated = inventory.prepareItemAddition(source, 1);
        inventory.clear();
        EXPECT_FALSE(invalidated->ownerIsCurrent());
        EXPECT_FALSE(invalidated->isValid());
        EXPECT_TRUE(invalidated->commit().isEmpty());
        EXPECT_TRUE(invalidated->getItem().isEmpty());
    }

    TEST(OblivionWorldTest, PreparedNativeAdditionListenerCanDestroyItsInventoryOwner)
    {
        NativeWorldFixture fixture;
        const auto weapon = addEquipmentWeapon(fixture);
        ESM4::RuntimeInventoryItem item; item.mBase = weapon; item.mCount = 1; item.mCondition = 43.125;
        auto sources = MWWorld::OblivionProfileServices::prepareActorInventory(
            fixture.mWorld.getStore(), ESM::FormKeyResolver({"headless.esm"}), {item});
        auto inventory = std::make_unique<MWWorld::InventoryStore>();
        struct Listener : MWWorld::ContainerStoreListener
        {
            std::unique_ptr<MWWorld::InventoryStore>* mOwner = nullptr;
            int mCalls = 0;
            void itemAdded(const MWWorld::ConstPtr&, int) override { ++mCalls; mOwner->reset(); }
        } listener;
        listener.mOwner = &inventory;
        inventory->setContListener(&listener);
        auto addition = inventory->prepareItemAddition(sources.front().mReference.getPtr(), 1);
        ASSERT_FALSE(addition->commit().isEmpty());
        EXPECT_TRUE(addition->notify());
        EXPECT_EQ(listener.mCalls, 1);
        EXPECT_EQ(inventory, nullptr);
        EXPECT_FALSE(addition->ownerIsCurrent());
        EXPECT_FALSE(addition->isValid());
        EXPECT_FALSE(addition->notify());
        EXPECT_TRUE(addition->getItem().isEmpty());
    }

    TEST(OblivionWorldTest, NativeAdditionObserverCanClearWorldWithoutAccessingRetiredPlayer)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        MWClass::Npc::registerSelf();
        world.setupPlayer();
        const auto weapon = addEquipmentWeapon(fixture);
        const auto player = world.getPlayerPtr();
        auto& inventory = player.getClass().getInventoryStore(player);
        struct Listener : MWWorld::ContainerStoreListener
        {
            MWWorld::World* mWorld = nullptr;
            int mCalls = 0;
            void itemAdded(const MWWorld::ConstPtr&, int) override
            {
                ++mCalls;
                mWorld->clear();
            }
        } listener;
        listener.mWorld = &world;
        inventory.setContListener(&listener);
        ESM4::RuntimeInventoryItem item; item.mBase = weapon; item.mCount = 1; item.mCondition = 43.125;
        EXPECT_EQ(world.oblivionAddPlayerInventoryItem(item), 1);
        EXPECT_EQ(listener.mCalls, 1);
        EXPECT_TRUE(world.captureOblivionActorInventory(world.getPlayerPtr()).empty());
    }

    TEST(OblivionWorldTest, PreparedNativeAdditionRejectsReplacedMovedAndReboundStores)
    {
        NativeWorldFixture fixture;
        const auto weapon = addEquipmentWeapon(fixture);
        ESM4::RuntimeInventoryItem item; item.mBase = weapon; item.mCount = 1; item.mCondition = 43.125;
        const auto sources = MWWorld::OblivionProfileServices::prepareActorInventory(
            fixture.mWorld.getStore(), ESM::FormKeyResolver({"headless.esm"}), {item});
        for (int invalidation = 0; invalidation < 5; ++invalidation)
        {
            SCOPED_TRACE(invalidation);
            MWWorld::InventoryStore live, replacement;
            auto addition = live.prepareItemAddition(sources.front().mReference.getPtr(), 1);
            ASSERT_TRUE(addition->isValid());
            if (invalidation == 0) live = replacement;
            if (invalidation == 1) live = std::move(replacement);
            if (invalidation == 2) replacement = std::move(live);
            if (invalidation == 3) live.swapPreparedContents(replacement);
            if (invalidation == 4)
            {
                MWWorld::ManualRef owner(fixture.mWorld.getStore(), ESM::RefId(ESM::FormId{0x940, 0}));
                live.setPtr(owner.getPtr());
                EXPECT_FALSE(addition->isValid());
                continue;
            }
            EXPECT_FALSE(addition->ownerIsCurrent());
            EXPECT_FALSE(addition->isValid());
            EXPECT_TRUE(addition->commit().isEmpty());
            EXPECT_FALSE(addition->notify());
        }
    }

    TEST(OblivionWorldTest, NativeAdditionClearedInventoryDoesNotRetainCachedItemsForHotkeys)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        MWClass::Npc::registerSelf();
        world.setupPlayer();
        const auto weapon = addEquipmentWeapon(fixture);
        const auto player = world.getPlayerPtr();
        ESM4::RuntimeInventoryItem item; item.mBase = weapon; item.mCount = 1; item.mCondition = 43.125;
        ESM4::RuntimeState cache;
        cache.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        cache.mPlayer.mCell = ESM::FormKey::content("headless.esm", 1);
        cache.mPlayer.mRace = ESM::FormKey::content("headless.esm", 0x810);
        cache.mPlayer.mClass = ESM::FormKey::dynamic("fixture-class", 1);
        cache.mPlayer.mInventory = {item};
        acceptNativeSnapshot(fixture, cache);
        installEquipmentInventory(fixture, player, {item});
        auto& inventory = player.getClass().getInventoryStore(player);
        struct Listener : MWWorld::ContainerStoreListener
        {
            MWWorld::InventoryStore* mInventory = nullptr;
            int mCalls = 0;
            void itemAdded(const MWWorld::ConstPtr&, int) override
            {
                ++mCalls;
                mInventory->clear();
            }
        } listener;
        listener.mInventory = &inventory;
        inventory.setContListener(&listener);
        EXPECT_EQ(world.oblivionAddPlayerInventoryItem(item), 1);
        EXPECT_EQ(listener.mCalls, 1);
        EXPECT_TRUE(world.captureOblivionActorInventory(player).empty());
        EXPECT_FALSE(world.oblivionSetPlayerHotkey(ESM::RefId(ESM::FormId{0x940, 0}), 1));
        inventory.setContListener(nullptr);
    }

    TEST(OblivionWorldTest, NativeAdditionObserverFailureStillRefreshesCommittedLiveInventory)
    {
        for (const bool clear : {false, true})
        {
            SCOPED_TRACE(clear);
            NativeWorldFixture fixture;
            auto& world = fixture.mWorld;
            MWClass::Npc::registerSelf();
            world.setupPlayer();
            const auto weapon = addEquipmentWeapon(fixture);
            const auto player = world.getPlayerPtr();
            auto& inventory = player.getClass().getInventoryStore(player);
            ESM4::RuntimeInventoryItem item; item.mBase = weapon; item.mCount = 1; item.mCondition = 43.125;
            ESM4::RuntimeState cache;
            cache.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
            cache.mPlayer.mCell = ESM::FormKey::content("headless.esm", 1);
            cache.mPlayer.mRace = ESM::FormKey::content("headless.esm", 0x810);
            cache.mPlayer.mClass = ESM::FormKey::dynamic("fixture-class", 1);
            if (clear)
            {
                cache.mPlayer.mInventory = {item};
                installEquipmentInventory(fixture, player, {item});
            }
            acceptNativeSnapshot(fixture, cache);
            struct Listener : MWWorld::ContainerStoreListener
            {
                MWWorld::InventoryStore* mInventory = nullptr;
                bool mClear = false;
                int mCalls = 0;
                void itemAdded(const MWWorld::ConstPtr&, int) override
                {
                    ++mCalls;
                    if (mClear) mInventory->clear();
                    throw std::runtime_error("injected inventory observer failure");
                }
            } listener;
            listener.mInventory = &inventory;
            listener.mClear = clear;
            inventory.setContListener(&listener);
            EXPECT_THROW(world.oblivionAddPlayerInventoryItem(item), std::runtime_error);
            EXPECT_EQ(listener.mCalls, 1);
            const auto actual = world.captureOblivionActorInventory(player);
            EXPECT_EQ(actual.empty(), clear);
            EXPECT_EQ(world.oblivionSetPlayerHotkey(ESM::RefId(ESM::FormId{0x940, 0}), 1), !clear);
            inventory.setContListener(nullptr);
        }
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
        auto original = *inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
        const auto baseline = captureNativeActorState(fixture, actor).serializeBinary();
        auto cached = captureNativeActorState(fixture, actor);
        cached.mReferences.front().mInventory = {item};
        acceptNativeSnapshot(fixture, cached);
        original = *inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
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
            acceptNativeSnapshot(fixture, saved);
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
        for (auto& values : legacy.mNativeActorValues) values.mBounty.reset();
        acceptNativeSnapshot(fixture, legacy);
        actor.getClass().getCreatureStats(actor).setDrawState(MWMechanics::DrawState::Weapon);
        EXPECT_FALSE(world.restoreOblivionActorDrawState(actor));
        EXPECT_EQ(actor.getClass().getCreatureStats(actor).getDrawState(), MWMechanics::DrawState::Weapon);
        actor.getRefData().setCustomData(nullptr);
        EXPECT_EQ(actor.getClass().getCreatureStats(actor).getDrawState(), MWMechanics::DrawState::Nothing);
        auto expectedLegacy = ESM4::RuntimeState::deserializeBinary(baseline);
        for (auto& values : expectedLegacy.mNativeActorValues) values.mBounty.reset();
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), expectedLegacy.serializeBinary());
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
        reference.mBaseObj = creature.mId;
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
            acceptNativeSnapshot(fixture, saved);
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
        acceptNativeSnapshot(fixture, saved);
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
        EXPECT_EQ(actor.getClass().getCreatureStats(actor).getDrawState(), MWMechanics::DrawState::Weapon);
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
        acceptNativeSnapshot(fixture, state);
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
        EXPECT_TRUE(world.initializeOblivionNonPlayerActor(ptr, ESM4::ActorValueProcess::Low));
        EXPECT_EQ(*service.findActorValues(key), original);
        EXPECT_EQ(*service.findActorLife(key), alive);
        service.clear();
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(ptr, ESM4::ActorValueProcess::Low));
        EXPECT_EQ(service.findActorLife(key)->mPhase, ESM4::ActorLifePhase::Alive);
        for (const auto av : {9, 10})
            EXPECT_EQ(service.findActorValues(key)->mValues[av].mModifiers,
                (ESM4::ActorValueModifiers{std::nullopt, 0.f, 0.f}));
        const auto freshLow = captureNativeActorState(fixture, ptr);
        const auto decodedLow = ESM4::RuntimeState::deserializeBinary(freshLow.serializeBinary());
        EXPECT_EQ(decodedLow.mNativeActorValues, freshLow.mNativeActorValues);
        state.mReferences[0].mCustomState["obscript.dead"]=std::int64_t{42};
        readNativeSnapshot(fixture, state);
        service.clear();
        EXPECT_THROW(world.applyOblivionRuntimeState(), std::invalid_argument);
        EXPECT_EQ(service.findActorValues(key), nullptr);
        EXPECT_EQ(service.findActorLife(key), nullptr);
        EXPECT_THROW(world.applyOblivionRuntimeState(), std::invalid_argument);
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
                ptr.getRefData().setCustomData(nullptr);
                ASSERT_NO_FATAL_FAILURE(acceptNativeSnapshot(fixture, state));
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
        EXPECT_TRUE(world.initializeOblivionPlayerActor()); // Incoming marker is still pending.
        EXPECT_EQ(*service.findActorValues(actor), before);
        EXPECT_EQ(service.findActorLife(actor)->mPhase, ESM4::ActorLifePhase::Alive);
        service.clear();
        world.getPlayerPtr().getRefData().setCustomData(nullptr);
        acceptNativeSnapshot(fixture, state);
        native.mIsTES4 = false;
        world.getStore().getWritable<ESM4::Npc>().insertStatic(native, base);
        EXPECT_THROW(world.initializeOblivionPlayerActor(), std::invalid_argument);
        EXPECT_EQ(service.findActorValues(actor), nullptr);
        native.mIsTES4 = true;
        world.getStore().getWritable<ESM4::Npc>().insertStatic(native, base);
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
        EXPECT_THROW(world.applyOblivionRuntimeState(), std::invalid_argument);
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
            acceptNativeSnapshot(fixture, state);
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
        acceptNativeSnapshot(fixture, state);
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
        prepareNativeSnapshotPlayer(fixture, saved);
        ASSERT_NO_FATAL_FAILURE(readNativeSnapshot(fixture, saved));
        MWMechanics::Actors actors;
        EXPECT_THROW(world.applyOblivionRuntimeState(), std::invalid_argument);
        EXPECT_EQ(service.findActorValues(key), nullptr);
        EXPECT_EQ(service.findActorLife(key), nullptr);
        EXPECT_EQ(actors.size(), 0);
        EXPECT_THROW(world.applyOblivionRuntimeState(), std::invalid_argument);
        saved.mReferences[0].mCustomState["obscript.dead"] = true;
        ASSERT_NO_FATAL_FAILURE(acceptNativeSnapshot(fixture, saved));
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
        for (auto& values : old.mNativeActorValues) values.mBounty.reset();
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
        for (auto& values : old.mNativeActorValues) values.mBounty.reset();
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
        for (auto& values : old.mNativeActorValues) values.mBounty.reset();
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
        for (auto& values : lossy.mNativeActorValues) values.mBounty.reset();
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
    TEST(OblivionWorldTest, NativeBowLaunchSamplesEquippedInstancesAndActualAvAuthorityWithoutWrites)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
        MWClass::Npc::registerSelf(); MWClass::Weapon::registerSelf(); world.setupPlayer();
        const auto playerBase = ESM::FormKey::content("Oblivion.esm", 7);
        ESM4::Npc nativePlayer{}; nativePlayer.mId = {7, 1}; nativePlayer.mFormKey = playerBase;
        nativePlayer.mIsTES4 = true; nativePlayer.mData.health = 100;
        store.getWritable<ESM4::Npc>().insertStatic(nativePlayer, playerBase);
        ASSERT_TRUE(world.initializeOblivionPlayerActor());
        const auto player = world.getPlayerPtr(), npc = addNativeNpc(fixture, 0x801);
        ASSERT_TRUE(world.activateOblivionActor(npc));
        auto& service = *world.getOblivionCombatService();
        const auto bowKey = ESM::FormKey::content("headless.esm", 0x940);
        const auto arrowKey = ESM::FormKey::content("headless.esm", 0x941);
        ESM4::Weapon bow{}; bow.mId = {0x940, 0}; bow.mData.type = 5;
        bow.mData.health = 100; bow.mData.damage = 20;
        store.getWritable<ESM4::Weapon>().insertStatic(bow, bowKey);
        ESM4::Ammunition arrow{}; arrow.mId = {0x941, 0};
        arrow.mData.mDamage = 5; arrow.mData.mSpeed = 1;
        store.getWritable<ESM4::Ammunition>().insertStatic(arrow, arrowKey);
        for (unsigned id : {0x940u, 0x941u})
        {
            ESM::Weapon shared; shared.blank(); shared.mId = ESM::RefId(ESM::FormId{id, 0});
            shared.mData.mType = id == 0x940 ? ESM::Weapon::MarksmanCrossbow : ESM::Weapon::Arrow;
            shared.mData.mHealth = 1;
            shared.mData.mChop[0] = shared.mData.mChop[1] = 255; // Must not supply native damage.
            store.insertStatic(shared);
        }
        unsigned id = 0x950;
        for (auto [name, value] : {std::pair{"fFatigueBase", 1.f},
                 {"fDamageWeaponMult", .5f}, {"fDamageSkillMult", 1.5f},
                 {"fDamageWeaponConditionBase", .5f}, {"fDamageWeaponConditionMult", .5f},
                 {"fDamageStrengthBase", .75f}, {"fDamageStrengthMult", .5f}})
        {
            ESM4::GameSetting setting{}; setting.mId = {id, 0};
            setting.mEditorId = name; setting.mData = value;
            store.getWritable<ESM4::GameSetting>().insertStatic(setting,
                ESM::FormKey::content("headless.esm", id++));
        }
        const auto initial = captureNativeActorState(fixture, npc);
        for (const auto actor : {player, npc})
        {
            ESM4::RuntimeInventoryItem weapon; weapon.mBase = bowKey;
            weapon.mCount = 1; weapon.mCondition = 50; weapon.mEquippedSlots = ESM4::InventorySlotWeapon;
            ESM4::RuntimeInventoryItem ammunition; ammunition.mBase = arrowKey;
            ammunition.mCount = 3; ammunition.mEquippedSlots = ESM4::InventorySlotAmmunition;
            const auto prepared = MWWorld::OblivionProfileServices::prepareActorInventory(
                store, ESM::FormKeyResolver({"headless.esm"}), {weapon, ammunition});
            auto staged = MWWorld::OblivionProfileServices::stageActorInventory(prepared);
            actor.getClass().getInventoryStore(actor).swapPreparedContents(*staged);
            auto state = initial;
            const auto key = actor == player ? ESM::FormKey::dynamic("player", 1) : actor.getCellRef().getFormKey();
            auto& values = *std::find_if(state.mNativeActorValues.begin(), state.mNativeActorValues.end(),
                [&](const auto& x) { return x.mActor == key; });
            for (unsigned av : {3u, 7u, 28u})
            {
                values.mValues[av] = {};
                values.mValues[av].mBase = 50;
            }
            values.mValues[10] = {}; values.mValues[10].mBase = 140;
            values.mValues[10].mModifiers[2] = -70;
            values.mValues[42] = {};
            service.restore(state, store);
            const auto before = captureNativeActorState(fixture, npc).serializeBinary();
            const auto shot = MWMechanics::sampleOblivionEquippedArrowLaunch(world, actor,
                actor == player ? .625f : std::numeric_limits<float>::quiet_NaN());
            EXPECT_EQ(shot.mBow, bowKey);
            EXPECT_EQ(shot.mAmmunition, arrowKey);
            EXPECT_EQ(shot.mDrawFraction, actor == player ? .5f : 1.f);
            EXPECT_NEAR(shot.mDamage, actor == player ? 3.5625f : 7.125f, .000001f);
            EXPECT_EQ(shot.mSpeed, actor == player ? 757.5f : 1500.f);
            EXPECT_EQ(shot.mShotFatigueDebit, 0);
            EXPECT_EQ(captureNativeActorState(fixture, npc).serializeBinary(), before);
            auto& inventory = actor.getClass().getInventoryStore(actor);
            const auto equipped = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
            ASSERT_NE(equipped, inventory.end());
            EXPECT_EQ(equipped->getCellRef().getNativeItemCondition(), 50);
            const auto ammo = inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition);
            ASSERT_NE(ammo, inventory.end()); EXPECT_EQ(ammo->getCellRef().getCount(), 3);
            if (actor == npc)
            {
                values.mValues[28] = {};
                values.mValues[28].mBase = 24;
                values.mValues[28].mModifiers[0] = .75f;
                values.mValues[28].mModifiers[1] = .75f;
                service.restore(state, store);
                ASSERT_EQ(service.getNonPlayerValue(npc, 28), 25.5f);
                ASSERT_EQ(service.getNonPlayerIntegerValue(npc, 28), 24);
                const auto beforeBoundary = captureNativeActorState(fixture, npc).serializeBinary();
                const auto boundary = MWMechanics::sampleOblivionEquippedArrowLaunch(world, npc,
                    std::numeric_limits<float>::quiet_NaN());
                EXPECT_NEAR(boundary.mDamage, 4.3125f, .000001f);
                EXPECT_NEAR(boundary.mGravityFactor, .25f, .000001f);
                EXPECT_EQ(boundary.mShotFatigueDebit, 5);
                EXPECT_EQ(captureNativeActorState(fixture, npc).serializeBinary(), beforeBoundary);
            }
        }
        EXPECT_THROW(MWMechanics::sampleOblivionEquippedArrowLaunch(world, {}, 0), std::invalid_argument);
        const auto unavailable = addNativeNpc(fixture, 0x802);
        EXPECT_THROW(MWMechanics::sampleOblivionEquippedArrowLaunch(world, unavailable, 0), std::invalid_argument);
        const auto item = *npc.getClass().getInventoryStore(npc).getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
        EXPECT_THROW(MWMechanics::sampleOblivionEquippedArrowLaunch(world, item, 0), std::invalid_argument);
        auto empty = MWWorld::OblivionProfileServices::stageActorInventory({});
        npc.getClass().getInventoryStore(npc).swapPreparedContents(*empty);
        EXPECT_THROW(MWMechanics::sampleOblivionEquippedArrowLaunch(world, npc, 0), std::invalid_argument);
    }

}

namespace
{
    TEST(OblivionWorldTest, NativeBowHoldFrameClockWinningRateGodModeAndRestartDoNotReplayDebit)
    {
        NativeWorldFixture fixture; auto& world = fixture.mWorld; auto& store = world.getStore();
        MWClass::Npc::registerSelf(); world.setupPlayer();
        ESM4::Npc native{}; native.mId = {7, 1}; native.mFormKey = ESM::FormKey::content("Oblivion.esm", 7);
        native.mIsTES4 = true; native.mData.health = 100;
        store.getWritable<ESM4::Npc>().insertStatic(native, native.mFormKey);
        ASSERT_TRUE(world.initializeOblivionPlayerActor());
        const auto ptr = world.getPlayerPtr(); auto& service = *world.getOblivionCombatService();
        unsigned id = 0x960;
        for (auto [name, value] : {std::pair{"fMarksmanFatigueBurnPerSecond", 20.f},
                 {"fFatigueReturnBase", 0.f}, {"fFatigueReturnMult", 0.f}})
        {
            ESM4::GameSetting setting{}; setting.mId = {id, 0}; setting.mEditorId = name; setting.mData = value;
            store.getWritable<ESM4::GameSetting>().insertStatic(setting, ESM::FormKey::content("headless.esm", id++));
        }
        ESM4::RuntimeState state;
        state.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        state.mPlayer.mCell = ESM::FormKey::content("headless.esm", 1);
        state.mPlayer.mRace = ESM::FormKey::content("headless.esm", 0x810);
        state.mPlayer.mClass = ESM::FormKey::dynamic("fixture-class", 1);
        const auto key = ESM::FormKey::dynamic("player", 1);
        auto values = *service.findActorValues(key);
        values.mPlayerFormValues = {{100, 0, 40, 0}};
        for (auto av : {0, 3, 5, 6, 28}) values.mValues[av] = {};
        values.mValues[10].mModifiers = {}; values.mProcessAction = 5;
        service.publishPlayerValues(world.getPlayer(), values, MWWorld::resolveOblivionPlayerDynamicBaseSettings(store));
        service.advanceFrameClock(.125f);
        ASSERT_TRUE(world.updateOblivionFrameResources(ptr, 999.f, false)); // Actor clock owns duration.
        EXPECT_EQ(service.getPlayerValue(10), 37.5f);
        ASSERT_TRUE(world.updateOblivionFrameResources(ptr, 999.f, false));
        EXPECT_EQ(service.getPlayerValue(10), 37.5f);
        service.capture(state);
        const auto wire = state.serializeBinary();
        MWMechanics::OblivionCombatService replacement;
        replacement.restore(ESM4::RuntimeState::deserializeBinary(wire), store);
        const std::array<MWWorld::Ptr, 1> residents{ptr};
        service.installRestoredActorState(std::move(replacement), residents, &world.getPlayer());
        ASSERT_TRUE(world.updateOblivionFrameResources(ptr, 999.f, false));
        EXPECT_EQ(service.getPlayerValue(10), 37.5f);
        service.advanceFrameClock(.125f);
        ASSERT_TRUE(world.updateOblivionFrameResources(ptr, 999.f, false));
        EXPECT_EQ(service.getPlayerValue(10), 35);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getFatigue().getCurrent(), 35);
        EXPECT_TRUE(world.toggleGodMode()); service.advanceFrameClock(.125f);
        ASSERT_TRUE(world.updateOblivionFrameResources(ptr, 999.f, false)); EXPECT_EQ(service.getPlayerValue(10), 35);
        EXPECT_FALSE(world.toggleGodMode());
        service.setProcessAction(key, 3); service.advanceFrameClock(.125f);
        ASSERT_TRUE(world.updateOblivionFrameResources(ptr, 999.f, false)); EXPECT_EQ(service.getPlayerValue(10), 35);
    }

    TEST(OblivionWorldTest, EquipmentRefreshBindsLateBowTracksAndRemovesDetachedTargetsWithoutRestarting)
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
        // Editable, synthetic KF fixture: one Bow:0 target absent when loaded.
        std::string data = "NetImmerse File Format, Version 4.0.0.2\n";
        data += bytes(std::uint32_t{0x04000002}) + bytes(std::uint32_t{5});
        data += string("NiSequenceStreamHelper") + string("SyntheticBow")
            + bytes(std::int32_t{1}) + bytes(std::int32_t{3});
        data += string("NiTextKeyExtraData") + bytes(std::int32_t{2}) + bytes(std::uint32_t{0});
        data += bytes(std::uint32_t{2}) + bytes(0.f) + string("attackbow: start")
            + bytes(2.f) + string("attackbow: stop");
        data += string("NiStringExtraData") + bytes(std::int32_t{-1}) + bytes(std::uint32_t{0}) + string("Bow:0");
        data += string("NiKeyframeController") + bytes(std::int32_t{-1}) + bytes(std::uint16_t{8})
            + bytes(1.f) + bytes(0.f) + bytes(0.f) + bytes(2.f) + bytes(std::int32_t{-1}) + bytes(std::int32_t{4});
        data += string("NiKeyframeData") + bytes(std::uint32_t{0}); // No rotation keys.
        data += bytes(std::uint32_t{2}) + bytes(std::uint32_t{1}); // Linear translation.
        for (float value : {0.f, 0.f, 0.f, 0.f, 2.f, 8.f, 0.f, 0.f})
            data += bytes(value);
        data += bytes(std::uint32_t{0}); // No scale keys.
        data += bytes(std::uint32_t{1}) + bytes(std::int32_t{0});
        {
            std::ofstream stream(fixture.mDirectory / "attackbow.kf", std::ios::binary);
            stream.write(data.data(), data.size());
            ASSERT_TRUE(stream);
        }
        fixture.mVfs.addArchive(std::make_unique<VFS::FileSystemArchive>(fixture.mDirectory));
        fixture.mVfs.buildIndex();
        class EquippedAnimation : public MWRender::Animation
        {
        public:
            using Animation::Animation;
            using Animation::refreshAnimationBindings;
            void load()
            {
                mObjectRoot = new osg::Group;
                osg::ref_ptr<osg::MatrixTransform> arm = new osg::MatrixTransform;
                arm->setName("Bip01 L Clavicle"); mObjectRoot->addChild(arm);
                mArm = arm;
                if (!addSingleAnimSource(VFS::Path::Normalized("attackbow.kf"), "synthetic", {}, false))
                    throw std::runtime_error("bow source failed to load");
            }
            void attach(osg::Node* node) { mArm->addChild(node); }
            void detach(osg::Node* node) { mArm->removeChild(node); }
            std::size_t callbacks() const { return mActiveControllers.size(); }
            void sample(unsigned frame)
            {
                osg::ref_ptr<osg::FrameStamp> stamp = new osg::FrameStamp;
                stamp->setFrameNumber(frame);
                stamp->setSimulationTime(frame);
                osgUtil::UpdateVisitor visitor;
                visitor.setFrameStamp(stamp);
                mObjectRoot->accept(visitor);
            }
        private:
            osg::ref_ptr<osg::Group> mArm;
        };
        osg::ref_ptr<EquippedAnimation> animation = new EquippedAnimation(
            fixture.mWorld.getPlayerPtr(), new osg::Group, &fixture.mResources);
        animation->load();
        animation->play("attackbow", MWRender::AnimPriority(1), MWRender::BlendMask_All,
            false, 1, "start", "stop", .25f, 0);
        EXPECT_EQ(animation->callbacks(), 0u);
        EXPECT_FLOAT_EQ(animation->getCurrentTime("attackbow"), .5f);
        osg::ref_ptr<NifOsg::MatrixTransform> first = new NifOsg::MatrixTransform(Nif::NiTransform::getIdentity());
        first->setName("Bow:0"); animation->attach(first);
        animation->refreshAnimationBindings();
        EXPECT_EQ(animation->getNode("bow:0"), first.get());
        ASSERT_NE(first->getUpdateCallback(), nullptr);
        EXPECT_EQ(animation->callbacks(), 1u);
        EXPECT_FLOAT_EQ(animation->getCurrentTime("attackbow"), .5f);
        animation->sample(1);
        EXPECT_DOUBLE_EQ(first->getMatrix().getTrans().x(), 2);
        const auto* same = first->getUpdateCallback();
        animation->refreshAnimationBindings();
        EXPECT_EQ(first->getUpdateCallback(), same);
        EXPECT_EQ(animation->callbacks(), 1u);
        animation->runAnimation(.125f);
        const float time = animation->getCurrentTime("attackbow");
        EXPECT_FLOAT_EQ(time, .625f);
        animation->sample(2);
        EXPECT_DOUBLE_EQ(first->getMatrix().getTrans().x(), 2.5);
        animation->detach(first);
        osg::ref_ptr<NifOsg::MatrixTransform> second = new NifOsg::MatrixTransform(Nif::NiTransform::getIdentity());
        second->setName("Bow:0"); animation->attach(second);
        animation->refreshAnimationBindings();
        EXPECT_EQ(first->getUpdateCallback(), nullptr);
        ASSERT_NE(second->getUpdateCallback(), nullptr);
        EXPECT_EQ(animation->getNode("bow:0"), second.get());
        EXPECT_EQ(animation->callbacks(), 1u);
        EXPECT_FLOAT_EQ(animation->getCurrentTime("attackbow"), time);
        animation->sample(3);
        EXPECT_DOUBLE_EQ(second->getMatrix().getTrans().x(), 2.5);
        EXPECT_DOUBLE_EQ(first->getMatrix().getTrans().x(), 2.5);
        animation->detach(second);
        animation->refreshAnimationBindings();
        EXPECT_EQ(second->getUpdateCallback(), nullptr);
        EXPECT_EQ(animation->callbacks(), 0u);
        EXPECT_EQ(animation->getNode("bow:0"), nullptr);
        EXPECT_TRUE(animation->isPlaying("attackbow"));
        EXPECT_FLOAT_EQ(animation->getCurrentTime("attackbow"), time);
        animation->attach(first);
        animation->refreshAnimationBindings();
        EXPECT_EQ(animation->callbacks(), 1u);
        ASSERT_NE(first->getUpdateCallback(), nullptr);
        animation->disable("attackbow");
        EXPECT_EQ(first->getUpdateCallback(), nullptr);
        EXPECT_EQ(animation->callbacks(), 0u);
    }

    TEST(OblivionWorldTest, NativeHeldArrowReadsWinningAmmoModelAndNeverDebitsEquippedInstances)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld; auto& store = world.getStore();
        MWClass::Npc::registerSelf(); MWClass::Weapon::registerSelf(); world.setupPlayer();
        const auto npc = addNativeNpc(fixture, 0x801);
        const auto bowKey = ESM::FormKey::content("headless.esm", 0x940);
        const auto ammoKey = ESM::FormKey::content("headless.esm", 0x941);
        ESM4::Weapon bow{}; bow.mId = {0x940, 0}; bow.mData.type = 5; bow.mData.health = 100;
        store.getWritable<ESM4::Weapon>().insertStatic(bow, bowKey);
        ESM4::Ammunition ammo{}; ammo.mId = {0x941, 0}; ammo.mModel = "first.osgt";
        store.getWritable<ESM4::Ammunition>().insertStatic(ammo, ammoKey);
        for (unsigned id : {0x940u, 0x941u})
        {
            ESM::Weapon shared; shared.blank(); shared.mId = ESM::RefId(ESM::FormId{id, 0});
            shared.mData.mType = id == 0x940 ? ESM::Weapon::MarksmanCrossbow : ESM::Weapon::Arrow;
            shared.mData.mHealth = 1; shared.mModel = "wrong-shared-model.nif";
            store.insertStatic(shared);
        }
        std::filesystem::create_directories(fixture.mDirectory / "meshes");
        for (const auto& [file, name] : {std::pair{"first.osgt", "first-arrow"},
                 {"second.osgt", "second-arrow"}, {"missing.osgt", "not-the-arrow"}})
        {
            osg::ref_ptr<osg::Group> model = new osg::Group;
            model->setName("ArrowQuiver");
            osg::ref_ptr<osg::MatrixTransform> shape = new osg::MatrixTransform;
            shape->setName(std::string_view(file) == "missing.osgt" ? "Arrow1:0" : "Arrow:0");
            shape->setUserValue("fixture", std::string(name));
            model->addChild(shape);
            ASSERT_TRUE(osgDB::writeNodeFile(*model, (fixture.mDirectory / "meshes" / file).string()));
        }
        fixture.mVfs.addArchive(std::make_unique<VFS::FileSystemArchive>(fixture.mDirectory));
        fixture.mVfs.buildIndex();
        class ArrowAnimation : public MWRender::Animation
        {
        public:
            using Animation::Animation;
            using Animation::attachOblivionArrow;
            using Animation::detachOblivionArrow;
            osg::ref_ptr<NifOsg::MatrixTransform> mBone;
            void root()
            {
                mObjectRoot = new osg::Group; mInsert->addChild(mObjectRoot);
                mBone = new NifOsg::MatrixTransform(Nif::NiTransform::getIdentity());
                mBone->setName("ArrowBone"); mObjectRoot->addChild(mBone);
            }
        };
        for (const auto actor : {world.getPlayerPtr(), npc})
        {
            ammo.mModel = "first.osgt";
            store.getWritable<ESM4::Ammunition>().insertStatic(ammo, ammoKey);
            ESM4::RuntimeInventoryItem weapon; weapon.mBase = bowKey; weapon.mCount = 1;
            weapon.mCondition = 50; weapon.mEquippedSlots = ESM4::InventorySlotWeapon;
            ESM4::RuntimeInventoryItem arrows; arrows.mBase = ammoKey; arrows.mCount = 3;
            arrows.mEquippedSlots = ESM4::InventorySlotAmmunition;
            const auto prepared = MWWorld::OblivionProfileServices::prepareActorInventory(
                store, ESM::FormKeyResolver({"headless.esm"}), {weapon, arrows});
            auto staged = MWWorld::OblivionProfileServices::stageActorInventory(prepared);
            auto& inventory = actor.getClass().getInventoryStore(actor);
            inventory.swapPreparedContents(*staged);
            const auto equipped = inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition);
            osg::ref_ptr<ArrowAnimation> animation = new ArrowAnimation(actor, new osg::Group, &fixture.mResources);
            EXPECT_FALSE(animation->attachOblivionArrow()); // No ArrowBone.
            animation->root();
            equipped->getCellRef().setCount(0);
            EXPECT_FALSE(animation->attachOblivionArrow()); // No ammunition to nock.
            EXPECT_FALSE(animation->hasOblivionHeldArrow());
            EXPECT_EQ(animation->mBone->getNumChildren(), 0u);
            equipped->getCellRef().setCount(3);
            bow.mData.type = 3;
            store.getWritable<ESM4::Weapon>().insertStatic(bow, bowKey);
            EXPECT_FALSE(animation->attachOblivionArrow()); // Shared crossbow type cannot override native WEAP.
            EXPECT_FALSE(animation->hasOblivionHeldArrow());
            bow.mData.type = 5;
            store.getWritable<ESM4::Weapon>().insertStatic(bow, bowKey);
            ASSERT_TRUE(animation->attachOblivionArrow());
            ASSERT_TRUE(animation->hasOblivionHeldArrow());
            ASSERT_EQ(animation->mBone->getNumChildren(), 1u);
            osg::ref_ptr<osg::Node> old = animation->mBone->getChild(0);
            EXPECT_EQ(old->getName(), "Arrow:0");
            std::string marker; ASSERT_TRUE(old->getUserValue("fixture", marker)); EXPECT_EQ(marker, "first-arrow");
            ammo.mModel = "second.osgt";
            store.getWritable<ESM4::Ammunition>().insertStatic(ammo, ammoKey);
            ASSERT_TRUE(animation->attachOblivionArrow());
            EXPECT_EQ(old->getNumParents(), 0u);
            ASSERT_EQ(animation->mBone->getNumChildren(), 1u);
            ASSERT_TRUE(animation->mBone->getChild(0)->getUserValue("fixture", marker));
            EXPECT_EQ(marker, "second-arrow");
            EXPECT_EQ(equipped->getCellRef().getCount(), 3);
            ammo.mModel = "missing.osgt";
            store.getWritable<ESM4::Ammunition>().insertStatic(ammo, ammoKey);
            EXPECT_FALSE(animation->attachOblivionArrow());
            EXPECT_TRUE(animation->hasOblivionHeldArrow());
            EXPECT_EQ(animation->mBone->getNumChildren(), 1u);
            EXPECT_EQ(equipped->getCellRef().getCount(), 3);
            animation->detachOblivionArrow();
            animation->detachOblivionArrow(); // Repeated cancellation is harmless.
            EXPECT_FALSE(animation->hasOblivionHeldArrow());
            EXPECT_EQ(animation->mBone->getNumChildren(), 0u);
            ammo.mModel = "second.osgt";
            store.getWritable<ESM4::Ammunition>().insertStatic(ammo, ammoKey);
            ASSERT_TRUE(animation->attachOblivionArrow());
            animation->removeFromScene();
            EXPECT_FALSE(animation->hasOblivionHeldArrow());
            EXPECT_EQ(animation->mBone->getNumChildren(), 0u);
            EXPECT_EQ(equipped->getCellRef().getCount(), 3);
        }
    }

    TEST(OblivionWorldTest, NativeWeatherCatalogRestorePreservesEveryRngBucketAcrossRepeatedReordering)
    {
        NativeWorldFixture fixture;
        const auto region = ESM::RefId::stringRefId("catalog-climate");
        const std::vector<ESM::FormKey> original{ESM::FormKey::content("a.esm", 1),
            ESM::FormKey::content("b.esp", 2), ESM::FormKey::content("a.esm", 3),
            ESM::FormKey::content("b.esp", 4)};
        const std::vector<ESM::FormKey> firstKeys{original[3], original[2], original[0], original[1]};
        const std::vector<ESM::FormKey> secondKeys{original[1], original[0], original[3], original[2]};
        ESM::WeatherState saved{};
        saved.mWeatherIdentities = original;
        saved.mCurrentWeather = 0; saved.mNextWeather = 2; saved.mQueuedWeather = 1;
        saved.mRegions[region] = {-1, {10, 0, 25, 0}};
        std::map<ESM::RefId, MWWorld::RegionWeather> defaults;
        defaults.emplace(region, MWWorld::RegionWeather(std::vector<uint8_t>{100}));
        const auto beforePreparation = fixture.mWorld.getPrng();
        const auto first = MWWorld::prepareWeatherRestore(saved, firstKeys.size(), defaults, firstKeys);
        const auto second = MWWorld::prepareWeatherRestore(first.mState, secondKeys.size(), defaults, secondKeys);
        EXPECT_EQ(fixture.mWorld.getPrng(), beforePreparation);
        EXPECT_EQ(first.mState.mCurrentWeather, 2); EXPECT_EQ(first.mState.mNextWeather, 1);
        EXPECT_EQ(first.mState.mQueuedWeather, 3);
        EXPECT_EQ(second.mState.mCurrentWeather, 1); EXPECT_EQ(second.mState.mNextWeather, 3);
        EXPECT_EQ(second.mState.mQueuedWeather, 0);
        EXPECT_EQ(first.mState.mRegions.at(region).mChances, saved.mRegions.at(region).mChances);
        EXPECT_EQ(second.mState.mRegions.at(region).mChances, saved.mRegions.at(region).mChances);
        EXPECT_EQ(std::vector<uint8_t>(first.mRegions.at(region).getChances().begin(),
                      first.mRegions.at(region).getChances().end()), (std::vector<uint8_t>{0, 25, 10, 0}));
        EXPECT_EQ(std::vector<uint8_t>(second.mRegions.at(region).getChances().begin(),
                      second.mRegions.at(region).getChances().end()), (std::vector<uint8_t>{0, 10, 0, 25}));
        std::array<bool, 100> covered{};
        int count = 0;
        for (unsigned int seed = 1; seed <= 100000 && count != 100; ++seed)
        {
            const Misc::Rng::Generator initial(seed);
            auto expectedGenerator = initial;
            const int roll = Misc::Rng::rollDice(100, expectedGenerator) + 1;
            if (covered[roll - 1]) continue;
            covered[roll - 1] = true; ++count;
            SCOPED_TRACE(roll);
            // Original buckets: 1..10 -> A, 11..35 -> C, 36..100 ->
            // original catalog's fallback A. Check actual RegionWeather draws
            // and the World RNG after both restore generations.
            const auto& expectedKey = original[roll > 10 && roll <= 35 ? 2 : 0];
            for (int generation : {0, 1})
            {
                const auto& prepared = generation == 0 ? first : second;
                const auto& keys = generation == 0 ? firstKeys : secondKeys;
                auto current = prepared.mRegions.at(region);
                fixture.mWorld.getPrng() = initial;
                const int selected = current.getWeather();
                ASSERT_GE(selected, 0); ASSERT_LT(std::size_t(selected), keys.size());
                EXPECT_EQ(keys[selected], expectedKey);
                EXPECT_EQ(fixture.mWorld.getPrng(), expectedGenerator);
            }
        }
        EXPECT_EQ(count, 100);
    }

    TEST(OblivionWorldTest, PreparedProjectileAudioDecodesBeforeClearAndPublishesOnceFromPinnedStaticBuffers)
    {
        NativeWorldFixture fixture;
        std::filesystem::create_directories(fixture.mDirectory / "sound");
        std::string wav = "RIFF";
        const auto integer = [&](auto value) { wav.append(reinterpret_cast<const char*>(&value), sizeof(value)); };
        integer(std::uint32_t{548}); wav += "WAVEfmt "; integer(std::uint32_t{16});
        integer(std::uint16_t{1}); integer(std::uint16_t{1}); integer(std::uint32_t{8000});
        integer(std::uint32_t{16000}); integer(std::uint16_t{2}); integer(std::uint16_t{16});
        wav += "data"; integer(std::uint32_t{512}); wav.append(512, '\0');
        const auto path = fixture.mDirectory / "sound/prepared-loop.wav";
        { std::ofstream output(path, std::ios::binary); output.write(wav.data(), wav.size()); }
        fixture.mVfs.addArchive(std::make_unique<VFS::FileSystemArchive>(fixture.mDirectory));
        fixture.mVfs.buildIndex();
        ESM::Sound definition{}; definition.blank();
        definition.mId = ESM::RefId::stringRefId("prepared-projectile-loop");
        definition.mSound = "prepared-loop.wav"; definition.mData.mVolume = 255;
        definition.mData.mMinRange = 1; definition.mData.mMaxRange = 100;
        fixture.mWorld.getStore().getWritable<ESM::Sound>().insertStatic(definition);
        // Retire the old output before creating a new current OpenAL context.
        fixture.mSoundManager.reset();
        fixture.mSoundManager = std::make_unique<MWSound::SoundManager>(&fixture.mVfs, true);
        fixture.mEnvironment.setSoundManager(*fixture.mSoundManager);
        auto& sound = *fixture.mSoundManager;
        ASSERT_TRUE(sound.isEnabled()); // Test runner supplies ALSOFT_DRIVERS=null.
        using Type = MWSound::Type;
        using Mode = MWSound::PlayMode;
        auto* original = sound.playSound3D(osg::Vec3f{}, definition.mId, 1.f, 1.f, Type::Sfx, Mode::Loop);
        ASSERT_NE(original, nullptr);
        ASSERT_TRUE(sound.getSoundPlaying({}, definition.mId));
        const auto prepare = [&] { return sound.prepareSound3D(osg::Vec3f(5, 0, 0), definition.mId,
            1.f, 1.f, Type::Sfx, Mode::Loop); };
        { auto cancelled = prepare(); ASSERT_TRUE(cancelled); EXPECT_EQ(cancelled(), nullptr); }
        EXPECT_TRUE(sound.getSoundPlaying({}, definition.mId));
        auto stale = prepare(); sound.clear(); sound.clear(); EXPECT_EQ(stale(), nullptr);
        // A dynamic outgoing definition must not supply the restore resource.
        definition.mSound = "outgoing-missing.wav";
        fixture.mWorld.getStore().getWritable<ESM::Sound>().insert(definition);
        auto plan = prepare(); ASSERT_TRUE(plan); auto copy = plan;
        std::filesystem::remove(path);
        sound.clear();
        auto* restored = plan(); ASSERT_NE(restored, nullptr);
        EXPECT_TRUE(sound.getSoundPlaying({}, definition.mId));
        EXPECT_EQ(plan(), nullptr); EXPECT_EQ(copy(), nullptr);
        sound.fadeOutSound3D({}, definition.mId, 0.f);
        sound.stopSound3D({}, definition.mId);
        EXPECT_FALSE(sound.getSoundPlaying({}, definition.mId));
        sound.clear();
        definition.mId = ESM::RefId::stringRefId("outgoing-only-projectile-loop");
        fixture.mWorld.getStore().getWritable<ESM::Sound>().insert(definition);
        EXPECT_FALSE(sound.prepareSound3D({}, definition.mId, 1.f, 1.f, Type::Sfx, Mode::Loop));

        // Exercise actual backend refusal, then prove the active owner and
        // source pool still admit a valid playback after rollback.
        definition.mId = ESM::RefId::stringRefId("prepared-projectile-loop");
        auto refused = sound.prepareSound3D({}, definition.mId, 1.f, -1.f, Type::Sfx, Mode::Loop);
        ASSERT_TRUE(refused);
        sound.clear();
        EXPECT_EQ(refused(), nullptr);
        EXPECT_EQ(refused(), nullptr);
        EXPECT_FALSE(sound.getSoundPlaying({}, definition.mId));
        auto recovered = prepare(); ASSERT_TRUE(recovered);
        sound.clear(); ASSERT_NE(recovered(), nullptr);
        EXPECT_TRUE(sound.getSoundPlaying({}, definition.mId));
        sound.clear();

        // Native directory selection during admission must not consume the
        // outgoing world's RNG, including when the plan is discarded.
        std::filesystem::create_directories(fixture.mDirectory / "sound/prepared-directory");
        for (const auto* name : {"one.wav", "two.wav"})
        {
            std::ofstream output(fixture.mDirectory / "sound/prepared-directory" / name, std::ios::binary);
            output.write(wav.data(), wav.size());
        }
        fixture.mVfs.addArchive(std::make_unique<VFS::FileSystemArchive>(fixture.mDirectory));
        fixture.mVfs.buildIndex();
        ESM4::Sound native{}; native.mId = {0x880, 0};
        native.mSoundFile = "prepared-directory/";
        fixture.mWorld.getStore().getWritable<ESM4::Sound>().insertStatic(native);
        const auto rng = Misc::Rng::getGenerator();
        auto directory = sound.prepareSound3D({}, native.mId, 1.f, 1.f, Type::Sfx, Mode::Loop);
        ASSERT_TRUE(directory);
        EXPECT_EQ(Misc::Rng::getGenerator(), rng);
        directory = {};
        EXPECT_EQ(Misc::Rng::getGenerator(), rng);
    }

    TEST(OblivionWorldTest, PreparedProjectileAudioHandlesExpireAfterSoundOwnerTeardown)
    {
        NativeWorldFixture fixture;
        std::filesystem::create_directories(fixture.mDirectory / "sound");
        std::string wav = "RIFF";
        const auto integer = [&](auto value) { wav.append(reinterpret_cast<const char*>(&value), sizeof(value)); };
        integer(std::uint32_t{548}); wav += "WAVEfmt "; integer(std::uint32_t{16});
        integer(std::uint16_t{1}); integer(std::uint16_t{1}); integer(std::uint32_t{8000});
        integer(std::uint32_t{16000}); integer(std::uint16_t{2}); integer(std::uint16_t{16});
        wav += "data"; integer(std::uint32_t{512}); wav.append(512, '\0');
        { std::ofstream output(fixture.mDirectory / "sound/orphan-loop.wav", std::ios::binary);
            output.write(wav.data(), wav.size()); }
        fixture.mVfs.addArchive(std::make_unique<VFS::FileSystemArchive>(fixture.mDirectory));
        fixture.mVfs.buildIndex();
        ESM::Sound definition{}; definition.blank(); definition.mId = ESM::RefId::stringRefId("orphan-loop");
        definition.mSound = "orphan-loop.wav"; definition.mData.mVolume = 255;
        fixture.mWorld.getStore().getWritable<ESM::Sound>().insertStatic(definition);
        std::function<MWBase::Sound*()> orphan;
        {
            MWSound::SoundManager temporary(&fixture.mVfs, true);
            ASSERT_TRUE(temporary.isEnabled());
            orphan = temporary.prepareSound3D({}, definition.mId, 1.f, 1.f,
                MWSound::Type::Sfx, MWSound::PlayMode::Loop);
            ASSERT_TRUE(orphan);
        }
        EXPECT_EQ(orphan(), nullptr);
        orphan = {};
    }

    TEST(OblivionWorldTest, ProjectileLaunchPreparesBeforePublicationAndRollsBackSceneFailures)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        MWClass::Npc::registerSelf(); MWClass::Weapon::registerSelf(); world.setupPlayer();
        std::filesystem::create_directories(fixture.mDirectory / "meshes");
        osg::ref_ptr<osg::Geode> model = new osg::Geode;
        osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
        vertices->push_back({0,0,0}); vertices->push_back({1,0,0}); vertices->push_back({0,1,0});
        geometry->setVertexArray(vertices);
        geometry->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLES, 0, 3));
        model->addDrawable(geometry);
        ASSERT_TRUE(osgDB::writeNodeFile(*model, (fixture.mDirectory / "meshes/arrow.osgt").string()));
        osg::ref_ptr<osg::Group> empty = new osg::Group;
        ASSERT_TRUE(osgDB::writeNodeFile(*empty, (fixture.mDirectory / "meshes/empty.osgt").string()));
        fixture.mVfs.addArchive(std::make_unique<VFS::FileSystemArchive>(fixture.mDirectory));
        fixture.mVfs.buildIndex();
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
        auto& physics = world.initializePhysics(new osg::Group);
        ESM::Weapon arrow; arrow.blank(); arrow.mId = ESM::RefId::stringRefId("prepared-arrow");
        arrow.mData.mType = ESM::Weapon::Arrow; arrow.mModel = "arrow.osgt";
        world.getStore().insertStatic(arrow);
        ESM::Weapon noShape = arrow; noShape.mId = ESM::RefId::stringRefId("empty-arrow");
        noShape.mModel = "empty.osgt"; world.getStore().insertStatic(noShape);
        MWWorld::ManualRef item(world.getStore(), arrow.mId);
        MWWorld::ManualRef emptyItem(world.getStore(), noShape.mId);
        class Parent : public osg::Group
        {
        public:
            enum Mode { Accept, Reject, ThrowAfterAttach } mMode = Accept;
            bool addChild(osg::Node* child) override
            {
                if (mMode == Reject) return false;
                const bool result = osg::Group::addChild(child);
                if (mMode == ThrowAfterAttach) throw std::runtime_error("injected scene failure");
                return result;
            }
        };
        osg::ref_ptr<Parent> parent = new Parent;
        MWWorld::ProjectileManager manager(parent, &fixture.mResources, nullptr, &physics);
        const auto actor = world.getPlayerPtr();
        const auto launch = [&](const MWWorld::Ptr& ammunition) {
            manager.launchProjectile(actor, ammunition, osg::Vec3f(5,0,0),
                osg::Quat{}, item.getPtr(), 100, .5f, .25f);
        };
        const auto ray = [&] { return physics.castRay(osg::Vec3f(0,0,0), osg::Vec3f(10,0,0),
            {}, {}, MWPhysics::CollisionType_Projectile, MWPhysics::CollisionType_Projectile).mHit; };
        EXPECT_THROW(launch(emptyItem.getPtr()), std::invalid_argument);
        EXPECT_EQ(parent->getNumChildren(), 0u); EXPECT_FALSE(ray());
        EXPECT_EQ(manager.countSavedGameRecords(), 0u); EXPECT_EQ(physics.getProjectile(1), nullptr);
        parent->mMode = Parent::Reject;
        EXPECT_THROW(launch(item.getPtr()), std::runtime_error);
        EXPECT_EQ(parent->getNumChildren(), 0u); EXPECT_FALSE(ray());
        EXPECT_EQ(manager.countSavedGameRecords(), 0u); EXPECT_EQ(physics.getProjectile(1), nullptr);
        parent->mMode = Parent::ThrowAfterAttach;
        EXPECT_THROW(launch(item.getPtr()), std::runtime_error);
        EXPECT_EQ(parent->getNumChildren(), 0u); EXPECT_FALSE(ray());
        EXPECT_EQ(manager.countSavedGameRecords(), 0u); EXPECT_EQ(physics.getProjectile(1), nullptr);
        parent->mMode = Parent::Accept;
        launch(item.getPtr());
        EXPECT_EQ(parent->getNumChildren(), 1u); EXPECT_TRUE(ray());
        EXPECT_EQ(manager.countSavedGameRecords(), 1u); EXPECT_NE(physics.getProjectile(1), nullptr);
        EXPECT_EQ(item.getPtr().getCellRef().getCount(), 1);
        manager.clear();
        EXPECT_EQ(parent->getNumChildren(), 0u); EXPECT_FALSE(ray());
        EXPECT_EQ(manager.countSavedGameRecords(), 0u); EXPECT_EQ(physics.getProjectile(1), nullptr);
        launch(item.getPtr());
        EXPECT_NE(physics.getProjectile(2), nullptr);
        manager.clear();

        ESM::ProjectileState saved{};
        saved.mId = arrow.mId;
        saved.mPosition = ESM::Vector3(osg::Vec3f(5, 0, 0));
        saved.mOrientation = ESM::Quaternion(osg::Quat{});
        saved.mVelocity = ESM::Vector3(osg::Vec3f(100, 0, 0));
        saved.mAttackStrength = .375f;
        saved.mAttackWindUp = .125f;
        MWWorld::ESMStore incoming;
        arrow.mModel = "arrow.osgt";
        incoming.getWritable<ESM::Weapon>().insert(arrow);
        const auto prepare = [&] {
            return manager.prepareRead({saved}, {}, world.getStore(), incoming);
        };
        {
            auto discarded = prepare();
            EXPECT_FALSE(discarded()); // No publication before its clear boundary.
        }
        EXPECT_EQ(parent->getNumChildren(), 0u);
        EXPECT_FALSE(ray());
        EXPECT_EQ(physics.getProjectile(3), nullptr);
        auto stale = prepare();
        manager.clear();
        manager.clear();
        EXPECT_FALSE(stale());
        auto plan = prepare();
        auto copied = plan;
        // Resource objects are already detached: the incoming model source may
        // change and the source node may disappear before installation.
        arrow.mModel = "missing.osgt";
        incoming.getWritable<ESM::Weapon>().insert(arrow);
        std::filesystem::remove(fixture.mDirectory / "meshes/arrow.osgt");
        manager.clear();
        ASSERT_TRUE(plan());
        EXPECT_FALSE(plan());
        EXPECT_FALSE(copied());
        EXPECT_EQ(parent->getNumChildren(), 1u);
        EXPECT_TRUE(ray());
        EXPECT_EQ(manager.countSavedGameRecords(), 1u);
        EXPECT_NE(physics.getProjectile(3), nullptr);
        manager.clear();
        EXPECT_FALSE(ray());
        EXPECT_EQ(parent->getNumChildren(), 0u);
        EXPECT_EQ(physics.getProjectile(3), nullptr);
        // Incoming spell data, model and light geometry are also prepared
        // without sound/scene/collision publication.
        ASSERT_TRUE(osgDB::writeNodeFile(*model, (fixture.mDirectory / "meshes/arrow.osgt").string()));
        arrow.mModel = "arrow.osgt";
        incoming.getWritable<ESM::Weapon>().insert(arrow);
        ESM::MagicEffect effect{}; effect.blank(); effect.mId = ESM::MagicEffect::FireDamage;
        effect.mBolt = arrow.mId;
        effect.mBoltSound = ESM::RefId::stringRefId("fixture-bolt-sound");
        effect.mData.mRed = 255; effect.mData.mGreen = 127; effect.mData.mBlue = 0;
        effect.mData.mSpeed = 1.f;
        world.getStore().getWritable<ESM::MagicEffect>().insertStatic(effect);
        ESM::Spell spell{}; spell.blank(); spell.mId = ESM::RefId::stringRefId("incoming-bolt-spell");
        ESM::IndexedENAMstruct target{}; target.mData.mEffectID = effect.mId;
        target.mData.mRange = ESM::RT_Target;
        spell.mEffects.mList.push_back(target);
        incoming.getWritable<ESM::Spell>().insert(spell);
        ESM::MagicBoltState bolt{}; bolt.mId = arrow.mId; bolt.mSpellId = spell.mId;
        bolt.mPosition = saved.mPosition; bolt.mOrientation = saved.mOrientation;
        bolt.mSpeed = 13.25f;
        auto magic = manager.prepareRead({}, {bolt}, world.getStore(), incoming);
        EXPECT_EQ(parent->getNumChildren(), 0u);
        EXPECT_FALSE(ray());
        manager.clear();
        ASSERT_TRUE(magic());
        EXPECT_EQ(manager.countSavedGameRecords(), 1u);
        EXPECT_EQ(parent->getNumChildren(), 1u);
        EXPECT_TRUE(ray());
        EXPECT_FALSE(magic());
        manager.clear();
        EXPECT_FALSE(ray());
        for (auto mode : {Parent::Reject, Parent::ThrowAfterAttach})
        {
            auto rejected = prepare();
            manager.clear();
            auto awaitingClear = prepare();
            auto awaitingCopy = awaitingClear;
            parent->mMode = mode;
            EXPECT_THROW(rejected(), std::runtime_error);
            EXPECT_FALSE(rejected());
            EXPECT_EQ(parent->getNumChildren(), 0u);
            EXPECT_FALSE(ray());
            EXPECT_EQ(manager.countSavedGameRecords(), 0u);
            parent->mMode = Parent::Accept;
            // Failure must invalidate future-clear plans, not enable them in
            // the now-empty manager without an actual clear.
            EXPECT_FALSE(awaitingClear());
            EXPECT_FALSE(awaitingCopy());
            EXPECT_EQ(parent->getNumChildren(), 0u);
            EXPECT_FALSE(ray());
            EXPECT_EQ(manager.countSavedGameRecords(), 0u);
            manager.clear();
            EXPECT_FALSE(awaitingClear());
        }
        // Definitions removed by content changes retain the legacy skip path.
        auto skipped = saved;
        skipped.mId = ESM::RefId::stringRefId("removed-before-publication");
        auto emptyPublication = manager.prepareRead({skipped}, {}, world.getStore(), incoming);
        manager.clear();
        auto awaitingEmptyClear = prepare();
        ASSERT_TRUE(emptyPublication());
        EXPECT_FALSE(awaitingEmptyClear());
        EXPECT_EQ(parent->getNumChildren(), 0u);
        EXPECT_FALSE(ray());
        EXPECT_EQ(manager.countSavedGameRecords(), 0u);
        manager.clear();
        EXPECT_FALSE(awaitingEmptyClear());
        saved.mId = ESM::RefId::stringRefId("removed-projectile");
        bolt.mSpellId = ESM::RefId::stringRefId("removed-spell");
        auto removed = manager.prepareRead({saved}, {bolt}, world.getStore(), incoming);
        manager.clear();
        EXPECT_TRUE(removed());
        EXPECT_EQ(manager.countSavedGameRecords(), 0u);
        std::function<bool()> orphan;
        {
            MWWorld::ProjectileManager temporary(parent, &fixture.mResources, nullptr, &physics);
            orphan = temporary.prepareRead({}, {}, world.getStore(), incoming);
        }
        EXPECT_FALSE(orphan());
        effect.mId = ESM::RefId::stringRefId("outgoing-only-projectile-effect");
        ASSERT_EQ(world.getStore().get<ESM::MagicEffect>().searchStatic(effect.mId), nullptr);
        world.getStore().getWritable<ESM::MagicEffect>().insert(effect);
        spell.mEffects.mList.front().mData.mEffectID = effect.mId;
        incoming.getWritable<ESM::Spell>().insert(spell);
        bolt.mSpellId = spell.mId;
        auto unavailableEffect = manager.prepareRead({}, {bolt}, world.getStore(), incoming);
        manager.clear();
        EXPECT_TRUE(unavailableEffect());
        EXPECT_EQ(manager.countSavedGameRecords(), 0u);
        EXPECT_EQ(parent->getNumChildren(), 0u);
        EXPECT_FALSE(ray());
    }

}

namespace
{
    MWWorld::Ptr installPreparedDebitAmmunition(NativeWorldFixture& fixture, int count)
    {
        MWClass::Weapon::registerSelf();
        const auto actor = addNativeNpc(fixture, 0x900);
        auto& store = fixture.mWorld.getStore();
        const auto key = ESM::FormKey::content("headless.esm", 0x941);
        ESM4::Ammunition ammo{}; ammo.mId = {0x941, 0}; ammo.mData.mWeight = 2;
        store.getWritable<ESM4::Ammunition>().insertStatic(ammo, key);
        ESM::Weapon shared; shared.blank(); shared.mId = ESM::RefId(ammo.mId);
        shared.mData.mType = ESM::Weapon::Arrow; shared.mData.mWeight = 2;
        store.insertStatic(shared);
        ESM4::RuntimeInventoryItem item; item.mBase = key; item.mCount = count;
        item.mEquippedSlots = ESM4::InventorySlotAmmunition;
        installEquipmentInventory(fixture, actor, {item});
        return actor;
    }
}

TEST(OblivionWorldTest, PreparedAmmoCountAndLastSlotPublishOnceBeforeObservers)
{
    NativeWorldFixture fixture;
    const auto actor = installPreparedDebitAmmunition(fixture, 2);
    auto& inventory = actor.getClass().getInventoryStore(actor);
    const auto item = *inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition);
    struct Observer : MWWorld::InventoryStoreListener, MWWorld::ContainerStoreListener
    {
        MWWorld::InventoryStore& inventory;
        int equipment = 0, removed = 0;
        explicit Observer(MWWorld::InventoryStore& value) : inventory(value) {}
        void equipmentChanged() override
        {
            ++equipment;
            EXPECT_EQ(inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition), inventory.end());
        }
        void itemRemoved(const MWWorld::ConstPtr& item, int count) override
        {
            ++removed; EXPECT_EQ(count, 1);
            EXPECT_EQ(item.getCellRef().getCount(), removed == 1 ? 1u : 0u);
        }
    } observer(inventory);
    inventory.setInvListener(&observer); inventory.setContListener(&observer);
    EXPECT_EQ(inventory.getWeight(), 4);
    auto cancelled = inventory.prepareAmmunitionDebit(); cancelled.reset();
    EXPECT_EQ(item.getCellRef().getCount(), 2u); EXPECT_EQ(observer.removed, 0);
    auto first = inventory.prepareAmmunitionDebit();
    EXPECT_FALSE(inventory.notifyPreparedAmmunitionDebit(*first));
    ASSERT_TRUE(inventory.commitPreparedAmmunitionDebit(*first));
    EXPECT_FALSE(inventory.commitPreparedAmmunitionDebit(*first));
    EXPECT_EQ(item.getCellRef().getCount(), 1u); EXPECT_EQ(inventory.getWeight(), 2);
    EXPECT_EQ(*inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition), item);
    EXPECT_EQ(observer.removed, 0); EXPECT_EQ(observer.equipment, 0);
    ASSERT_TRUE(inventory.notifyPreparedAmmunitionDebit(*first));
    EXPECT_FALSE(inventory.notifyPreparedAmmunitionDebit(*first));
    auto last = inventory.prepareAmmunitionDebit();
    inventory.setSelectedEnchantItem(inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition));
    ASSERT_TRUE(inventory.commitPreparedAmmunitionDebit(*last));
    EXPECT_EQ(item.getCellRef().getCount(), 0u); EXPECT_EQ(inventory.getWeight(), 0);
    EXPECT_EQ(inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition), inventory.end());
    EXPECT_EQ(inventory.getSelectedEnchantItem(), inventory.end());
    EXPECT_EQ(observer.equipment, 0); EXPECT_EQ(observer.removed, 1);
    ASSERT_TRUE(inventory.notifyPreparedAmmunitionDebit(*last));
    EXPECT_EQ(observer.equipment, 1); EXPECT_EQ(observer.removed, 2);
    EXPECT_FALSE(inventory.notifyPreparedAmmunitionDebit(*last));
    EXPECT_THROW(inventory.prepareAmmunitionDebit(), std::invalid_argument);
    inventory.setInvListener(nullptr); inventory.setContListener(nullptr);
}

TEST(OblivionWorldTest, PreparedAmmoRejectsForeignStaleAndReplacedInventoryContents)
{
    NativeWorldFixture fixture; const auto actor = installPreparedDebitAmmunition(fixture, 2);
    auto& inventory = actor.getClass().getInventoryStore(actor);
    const auto item = *inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition);
    auto stale = inventory.prepareAmmunitionDebit();
    MWWorld::InventoryStore copy(inventory);
    EXPECT_FALSE(copy.commitPreparedAmmunitionDebit(*stale));
    item.getCellRef().setCount(3);
    EXPECT_FALSE(inventory.commitPreparedAmmunitionDebit(*stale)); EXPECT_EQ(item.getCellRef().getCount(), 3u);
    auto copied = copy.prepareAmmunitionDebit(); ASSERT_TRUE(copy.commitPreparedAmmunitionDebit(*copied));
    EXPECT_EQ(item.getCellRef().getCount(), 3u);
    auto replaced = inventory.prepareAmmunitionDebit();
    inventory = copy;
    EXPECT_FALSE(inventory.commitPreparedAmmunitionDebit(*replaced));
    auto cleared = inventory.prepareAmmunitionDebit(); inventory.clear();
    EXPECT_FALSE(inventory.commitPreparedAmmunitionDebit(*cleared));
    inventory = copy;
    auto swapped = inventory.prepareAmmunitionDebit(); inventory.swapPreparedContents(copy);
    EXPECT_FALSE(inventory.commitPreparedAmmunitionDebit(*swapped));
    EXPECT_FALSE(copy.commitPreparedAmmunitionDebit(*swapped));
}

TEST(OblivionWorldTest, PreparedAmmoOwnerLifetimeCannotAliasFreshInventoryAtSameAddress)
{
    NativeWorldFixture fixture; const auto actor = installPreparedDebitAmmunition(fixture, 2);
    auto& source = actor.getClass().getInventoryStore(actor);
    alignas(MWWorld::InventoryStore) std::array<std::byte, sizeof(MWWorld::InventoryStore)> memory;
    auto* owner = new (memory.data()) MWWorld::InventoryStore(source);
    auto old = owner->prepareAmmunitionDebit();
    owner->~InventoryStore();
    owner = new (memory.data()) MWWorld::InventoryStore(source);
    auto current = owner->prepareAmmunitionDebit();
    EXPECT_FALSE(owner->commitPreparedAmmunitionDebit(*old));
    ASSERT_TRUE(owner->commitPreparedAmmunitionDebit(*current));
    owner->~InventoryStore();
    // Tokens can be discarded after the borrowed owner's lifetime.
    old.reset(); current.reset();
}

TEST(OblivionWorldTest, PreparedAmmoObserversCannotReplayOrDereferenceReplacedItems)
{
    for (bool throws : {false, true})
    {
        NativeWorldFixture fixture; const auto actor = installPreparedDebitAmmunition(fixture, 1);
        auto& inventory = actor.getClass().getInventoryStore(actor);
        struct Observer : MWWorld::InventoryStoreListener, MWWorld::ContainerStoreListener
        {
            MWWorld::InventoryStore& inventory; bool throws; int calls = 0, items = 0;
            Observer(MWWorld::InventoryStore& value, bool shouldThrow) : inventory(value), throws(shouldThrow) {}
            void equipmentChanged() override
            {
                ++calls;
                if (throws) throw std::runtime_error("prepared ammo observer failure");
                inventory.clear();
            }
            void itemRemoved(const MWWorld::ConstPtr&, int) override { ++items; }
        } observer(inventory, throws);
        inventory.setInvListener(&observer); inventory.setContListener(&observer);
        auto debit = inventory.prepareAmmunitionDebit();
        ASSERT_TRUE(inventory.commitPreparedAmmunitionDebit(*debit));
        if (throws) EXPECT_THROW(inventory.notifyPreparedAmmunitionDebit(*debit), std::runtime_error);
        else EXPECT_TRUE(inventory.notifyPreparedAmmunitionDebit(*debit));
        EXPECT_FALSE(inventory.commitPreparedAmmunitionDebit(*debit));
        EXPECT_FALSE(inventory.notifyPreparedAmmunitionDebit(*debit));
        EXPECT_EQ(observer.calls, 1); EXPECT_EQ(observer.items, 0);
        EXPECT_EQ(inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition), inventory.end());
        inventory.setInvListener(nullptr); inventory.setContListener(nullptr);
    }
}

namespace
{
    struct PreparedBowActors { MWWorld::Ptr actor, npc; };
    PreparedBowActors installPreparedBowRelease(NativeWorldFixture& fixture, bool player, int ammunition = 2)
    {
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
        MWClass::Npc::registerSelf(); MWClass::Weapon::registerSelf(); world.setupPlayer();
        const auto playerBase = ESM::FormKey::content("Oblivion.esm", 7);
        ESM4::Npc definition{}; definition.mId = {7, 1}; definition.mFormKey = playerBase;
        definition.mIsTES4 = true; definition.mData.health = 100;
        definition.mData.attribs = {40,40,40,40,40,40,40,40};
        store.getWritable<ESM4::Npc>().insertStatic(definition, playerBase);
        if (!world.initializeOblivionPlayerActor())
            throw std::runtime_error("prepared release Player setup failed");
        const auto npc = addNativeNpc(fixture, 0x801);
        if (!world.activateOblivionActor(npc))
            throw std::runtime_error("prepared release NPC setup failed");
        const auto actor = player ? world.getPlayerPtr() : npc;
        const auto bowKey = ESM::FormKey::content("headless.esm", 0x940);
        const auto ammoKey = ESM::FormKey::content("headless.esm", 0x941);
        ESM4::Weapon bow{}; bow.mId = {0x940,0}; bow.mData.type = 5;
        bow.mData.health = 100; bow.mData.damage = 20;
        store.getWritable<ESM4::Weapon>().insertStatic(bow, bowKey);
        ESM4::Ammunition arrow{}; arrow.mId = {0x941,0};
        arrow.mData.mDamage = 5; arrow.mData.mSpeed = 1;
        store.getWritable<ESM4::Ammunition>().insertStatic(arrow, ammoKey);
        for (unsigned id : {0x940u,0x941u})
        {
            ESM::Weapon shared; shared.blank(); shared.mId = ESM::RefId(ESM::FormId{id,0});
            shared.mData.mType = id == 0x940 ? ESM::Weapon::MarksmanCrossbow : ESM::Weapon::Arrow;
            shared.mData.mHealth = 100;
            store.insertStatic(shared);
        }
        ESM4::RuntimeInventoryItem item; item.mBase = bowKey; item.mCount = 1;
        item.mCondition = 50; item.mEquippedSlots = ESM4::InventorySlotWeapon;
        ESM4::RuntimeInventoryItem ammo; ammo.mBase = ammoKey; ammo.mCount = ammunition;
        ammo.mEquippedSlots = ESM4::InventorySlotAmmunition;
        installEquipmentInventory(fixture, actor, {item,ammo});
        auto state = captureNativeActorState(fixture, npc);
        const auto key = player ? ESM::FormKey::dynamic("player",1) : actor.getCellRef().getFormKey();
        auto& values = *std::find_if(state.mNativeActorValues.begin(),state.mNativeActorValues.end(),
            [&](const auto& x) { return x.mActor == key; });
        values.mValues[28] = {}; values.mValues[28].mBase = 5;
        world.getOblivionCombatService()->restore(state,store);
        return {actor,npc};
    }

    std::uint64_t admitPreparedBowRelease(NativeWorldFixture& fixture, const MWWorld::Ptr& actor)
    {
        auto& service = *fixture.mWorld.getOblivionCombatService();
        const auto key = actor == fixture.mWorld.getPlayerPtr()
            ? ESM::FormKey::dynamic("player",1) : actor.getCellRef().getFormKey();
        ESM4::RuntimeBowState bow;
        bow.mBowBase = ESM::FormKey::content("headless.esm",0x940);
        bow.mAmmoBase = ESM::FormKey::content("headless.esm",0x941);
        bow.mAnimationGroup = "attackbow";
        bow.mKeyTimes = {0,.25f,1,1.25f,2};
        const auto id = service.beginBowDraw(key,bow);
        if (!service.advanceBowPlayback(id,key,.3f,false,true)
            || !service.confirmBowAttachment(id,key)
            || !service.advanceBowPlayback(id,key,1,false,true)
            || !service.advanceBowPlayback(id,key,.3f,false,true)
            || service.pendingBowEvent(key,true,true) != ESM4::BowActionEvent::Release)
            throw std::runtime_error("prepared release phase setup failed");
        return id;
    }
}

TEST(OblivionWorldTest, PreparedBowReleasePublishesResourcesAndActionBeforeLastAmmoObservers)
{
    NativeWorldFixture fixture;
    const auto actors = installPreparedBowRelease(fixture,true,1);
    const auto actor = actors.actor;
    auto& world = fixture.mWorld;
    auto& service = *world.getOblivionCombatService();
    auto& inventory = actor.getClass().getInventoryStore(actor);
    const auto bow = *inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
    const auto ammo = *inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition);
    const auto id = admitPreparedBowRelease(fixture,actor);
    const auto key = ESM::FormKey::dynamic("player",1);
    const auto before = captureNativeActorState(fixture,actors.npc).serializeBinary();
    const auto fatigue = service.getPlayerValue(10);
    ASSERT_GT(fatigue,5);
    auto release = service.prepareBowRelease(world,actor,id);
    const auto damage = release->launch().mDamage;
    ASSERT_EQ(release->launch().mShotFatigueDebit,5);
    EXPECT_EQ(captureNativeActorState(fixture,actors.npc).serializeBinary(),before);
    EXPECT_EQ(bow.getCellRef().getNativeItemCondition(),50);
    EXPECT_EQ(ammo.getCellRef().getCount(),1);
    EXPECT_FALSE(service.notifyBowRelease(*release,inventory));
    struct Observer : MWWorld::InventoryStoreListener, MWWorld::ContainerStoreListener
    {
        MWMechanics::OblivionCombatService& service;
        MWWorld::Ptr actor,bow;
        std::uint64_t id;
        float fatigue;
        int equipment = 0,removed = 0;
        Observer(MWMechanics::OblivionCombatService& s,MWWorld::Ptr a,MWWorld::Ptr b,
            std::uint64_t action,float expected) : service(s),actor(a),bow(b),id(action),fatigue(expected) {}
        void check()
        {
            EXPECT_TRUE(service.isActionConsumed(id));
            EXPECT_EQ(service.getProcessAction(ESM::FormKey::dynamic("player",1)),3);
            EXPECT_EQ(service.getPlayerValue(10),fatigue);
            EXPECT_EQ(actor.getClass().getCreatureStats(actor).getFatigue().getCurrent(),fatigue);
            EXPECT_LT(*bow.getCellRef().getNativeItemCondition(),50);
        }
        void equipmentChanged() override { ++equipment; check(); }
        void itemRemoved(const MWWorld::ConstPtr&,int count) override { ++removed; EXPECT_EQ(count,1); check(); }
    } observer(service,actor,bow,id,std::max(0.f,fatigue-5));
    inventory.setInvListener(&observer); inventory.setContListener(&observer);
    ASSERT_TRUE(service.validateBowRelease(*release,inventory));
    ASSERT_TRUE(service.commitBowRelease(*release,inventory));
    EXPECT_FALSE(service.commitBowRelease(*release,inventory));
    EXPECT_EQ(observer.equipment,0); EXPECT_EQ(observer.removed,0);
    EXPECT_EQ(ammo.getCellRef().getCount(),0);
    EXPECT_TRUE(service.findBowState(key)->mReleaseCommitted);
    EXPECT_EQ(release->launch().mDamage,damage);
    ASSERT_TRUE(service.notifyBowRelease(*release,inventory));
    EXPECT_FALSE(service.notifyBowRelease(*release,inventory));
    EXPECT_EQ(observer.equipment,1); EXPECT_EQ(observer.removed,1);
    inventory.setInvListener(nullptr); inventory.setContListener(nullptr);
}

TEST(OblivionWorldTest, PreparedBowReleasePreservesNpcAmmoAndPlayerGodModeResources)
{
    for (bool player : {false,true})
    {
        NativeWorldFixture fixture;
        const auto actors = installPreparedBowRelease(fixture,player);
        const auto actor = actors.actor;
        auto& world = fixture.mWorld; auto& service = *world.getOblivionCombatService();
        if (player) world.toggleGodMode();
        auto& inventory = actor.getClass().getInventoryStore(actor);
        const auto bow = *inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
        const auto ammo = *inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition);
        const auto id = admitPreparedBowRelease(fixture,actor);
        const float fatigue = player ? service.getPlayerValue(10) : service.getNonPlayerValue(actor,10);
        auto release = service.prepareBowRelease(world,actor,id);
        ASSERT_TRUE(service.commitBowRelease(*release,inventory));
        EXPECT_EQ(ammo.getCellRef().getCount(),2);
        EXPECT_EQ(bow.getCellRef().getNativeItemCondition(),player ? 50.f : 49.8f);
        EXPECT_EQ(player ? service.getPlayerValue(10) : service.getNonPlayerValue(actor,10),
            player ? fatigue : std::max(0.f,fatigue-5));
        EXPECT_EQ(actor.getClass().getCreatureStats(actor).getFatigue().getCurrent(),
            player ? fatigue : std::max(0.f,fatigue-5));
        EXPECT_TRUE(service.isActionConsumed(id));
        EXPECT_TRUE(service.notifyBowRelease(*release,inventory));
        EXPECT_FALSE(service.notifyBowRelease(*release,inventory));
    }
}

TEST(OblivionWorldTest, PreparedBowReleaseRejectsStaleAuthorityInventoryEquipmentAndGodMode)
{
    for (unsigned change = 0; change < 9; ++change)
    {
        NativeWorldFixture fixture;
        const auto actors = installPreparedBowRelease(fixture,true);
    const auto actor = actors.actor;
        auto& world = fixture.mWorld; auto& service = *world.getOblivionCombatService();
        auto& inventory = actor.getClass().getInventoryStore(actor);
        const auto id = admitPreparedBowRelease(fixture,actor);
        auto release = service.prepareBowRelease(world,actor,id);
        switch (change)
        {
            case 0: world.toggleGodMode(); break;
            case 1: service.cancelBowDraw(id,ESM::FormKey::dynamic("player",1)); break;
            case 2: inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight)->getCellRef().setNativeItemCondition(25); break;
            case 3: inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition)->getCellRef().setCount(3); break;
            case 4: service.restore(captureNativeActorState(fixture,actors.npc),world.getStore()); break;
            case 5: inventory.clear(); break;
            case 6: { auto copied = service; service = copied; break; }
            case 7: { auto copied = service; service = std::move(copied); break; }
            case 8: service.changePlayerValue(world.getPlayer(),10,ESM4::ActorValueModifier::Damage,-1,
                MWWorld::resolveOblivionPlayerDynamicBaseSettings(world.getStore())); break;
        }
        const auto before = captureNativeActorState(fixture,actors.npc).serializeBinary();
        EXPECT_FALSE(service.validateBowRelease(*release,inventory));
        EXPECT_FALSE(service.commitBowRelease(*release,inventory));
        EXPECT_FALSE(service.notifyBowRelease(*release,inventory));
        EXPECT_EQ(captureNativeActorState(fixture,actors.npc).serializeBinary(),before);
    }
}

TEST(OblivionWorldTest, PreparedBowReleaseRejectsChangedChargeAndOwnershipBeforeResourcePublication)
{
    // Ownership and fractional charge belong to this item instance. They can
    // change without changing count, condition, equipment or actor authority.
    for (bool player : {false, true})
        for (unsigned change = 0; change < 8; ++change)
        {
            SCOPED_TRACE(player);
            SCOPED_TRACE(change);
            NativeWorldFixture fixture;
            const auto actors = installPreparedBowRelease(fixture, player, 2);
            auto& world = fixture.mWorld;
            if (change == 7)
            {
                ESM4::GlobalVariable permission{};
                permission.mId = {0x970, 0};
                permission.mType = 'f'; permission.mValue = 1.f;
                world.getStore().getWritable<ESM4::GlobalVariable>().insertStatic(
                    permission, ESM::FormKey::content("headless.esm", 0x970));
            }
            auto& service = *world.getOblivionCombatService();
            auto& inventory = actors.actor.getClass().getInventoryStore(actors.actor);
            const auto bow = *inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
            const auto ammo = *inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition);
            auto& ref = bow.getCellRef();
            ref.setEnchantmentCharge(change == 1 ? -0.f : 7.25f);
            if (change == 5)
            {
                ESM::ObjectState state;
                ref.writeState(state);
                state.mRef.mGlobalVariable = "prepared_owner_permission";
                ref = MWWorld::CellRef(state.mRef);
            }
            const auto id = admitPreparedBowRelease(fixture, actors.actor);
            auto stale = service.prepareBowRelease(world, actors.actor, id);
            switch (change)
            {
                case 0: ref.setEnchantmentCharge(6.5f); break;
                case 1:
                    // The shared TES3 setter treats signed zero as numerically
                    // equal. Pass through the absent sentinel so this fixture
                    // actually changes the stored negative-zero bits.
                    ref.setEnchantmentCharge(-1.f);
                    ref.setEnchantmentCharge(0.f);
                    break;
                case 2: ref.setOwner(ESM::RefId(ESM::FormId{0x801, 0})); break;
                case 3: ref.setFaction(ESM::RefId(ESM::FormId{0x960, 0})); break;
                case 4: ref.setFactionRank(5); break;
                case 5: ref.resetGlobalVariable(); break;
                case 6: ref.setNativeOwnershipRank(-1); break;
                case 7: ref.setNativeOwnershipGlobal(ESM::RefId(ESM::FormId{0x970, 0})); break;
            }
            const auto before = captureNativeActorState(fixture, actors.npc).serializeBinary();
            const auto charge = std::bit_cast<std::uint32_t>(ref.getEnchantmentCharge());
            const auto owner = ref.getOwner(), faction = ref.getFaction();
            const auto rank = ref.getFactionRank();
            const auto global = ref.getGlobalVariable();
            const auto condition = ref.getNativeItemCondition();
            const auto nativeRank = ref.getNativeOwnershipRank();
            const auto nativeGlobal = ref.getNativeOwnershipGlobal();
            EXPECT_FALSE(service.validateBowRelease(*stale, inventory));
            EXPECT_FALSE(service.commitBowRelease(*stale, inventory));
            EXPECT_FALSE(service.notifyBowRelease(*stale, inventory));
            EXPECT_EQ(captureNativeActorState(fixture, actors.npc).serializeBinary(), before);
            EXPECT_EQ(std::bit_cast<std::uint32_t>(ref.getEnchantmentCharge()), charge);
            EXPECT_EQ(ref.getOwner(), owner); EXPECT_EQ(ref.getFaction(), faction);
            EXPECT_EQ(ref.getFactionRank(), rank);
            EXPECT_EQ(ref.getGlobalVariable(), global);
            EXPECT_EQ(ref.getNativeOwnershipRank(), nativeRank);
            EXPECT_EQ(ref.getNativeOwnershipGlobal(), nativeGlobal);
            EXPECT_EQ(ref.getNativeItemCondition(), condition);
            EXPECT_EQ(ammo.getCellRef().getCount(), 2);
            // Retrying captures the new extras and consumes the same release
            // once, preserving them through the final condition publication.
            stale.reset();
            auto fresh = service.prepareBowRelease(world, actors.actor, id);
            ASSERT_TRUE(service.validateBowRelease(*fresh, inventory));
            ASSERT_TRUE(service.commitBowRelease(*fresh, inventory));
            EXPECT_FALSE(service.commitBowRelease(*fresh, inventory));
            EXPECT_EQ(std::bit_cast<std::uint32_t>(ref.getEnchantmentCharge()), charge);
            EXPECT_EQ(ref.getOwner(), owner); EXPECT_EQ(ref.getFaction(), faction);
            EXPECT_EQ(ref.getFactionRank(), rank);
            EXPECT_EQ(ref.getGlobalVariable(), global);
            EXPECT_EQ(ref.getNativeOwnershipRank(), nativeRank);
            EXPECT_EQ(ref.getNativeOwnershipGlobal(), nativeGlobal);
            EXPECT_LT(*ref.getNativeItemCondition(), *condition);
            EXPECT_EQ(ammo.getCellRef().getCount(), player ? 1 : 2);
        }
}

TEST(OblivionWorldTest, PreparedBowReleasePublishesOnlyWearAndPreservesIndependentPlacementChanges)
{
    NativeWorldFixture fixture;
    const auto actors = installPreparedBowRelease(fixture, true, 2);
    auto& world = fixture.mWorld;
    auto& service = *world.getOblivionCombatService();
    auto& inventory = actors.actor.getClass().getInventoryStore(actors.actor);
    const auto bow = *inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
    auto& ref = bow.getCellRef();
    const auto id = admitPreparedBowRelease(fixture, actors.actor);
    auto release = service.prepareBowRelease(world, actors.actor, id);
    // Inventory placement metadata is independent of the sampled native
    // damage/resources. Publishing wear must not replace the whole CellRef.
    ESM::Position position = ref.getPosition();
    position.pos[0] = 17.f; position.rot[2] = .25f;
    ref.setPosition(position); ref.setScale(1.25f);
    const auto sourceRefNum = ref.getRefNum();
    ASSERT_TRUE(service.validateBowRelease(*release, inventory));
    ASSERT_TRUE(service.commitBowRelease(*release, inventory));
    EXPECT_FLOAT_EQ(ref.getPosition().pos[0], 17.f);
    EXPECT_FLOAT_EQ(ref.getPosition().rot[2], .25f);
    EXPECT_FLOAT_EQ(ref.getScale(), 1.25f);
    EXPECT_EQ(ref.getRefNum(), sourceRefNum);
    EXPECT_LT(*ref.getNativeItemCondition(), 50.f);
    EXPECT_EQ(ref.getCount(), 1);
}

TEST(OblivionWorldTest, PreparedBowReleaseCancellationForeignOwnersAndBrokenBowAreAtomic)
{
    NativeWorldFixture fixture;
    const auto actors = installPreparedBowRelease(fixture,true);
    const auto actor = actors.actor;
    auto& world = fixture.mWorld; auto& service = *world.getOblivionCombatService();
    auto& inventory = actor.getClass().getInventoryStore(actor);
    const auto bow = *inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
    const auto id = admitPreparedBowRelease(fixture,actor);
    const auto before = captureNativeActorState(fixture,actors.npc).serializeBinary();
    auto cancelled = service.prepareBowRelease(world,actor,id); cancelled.reset();
    EXPECT_EQ(captureNativeActorState(fixture,actors.npc).serializeBinary(),before);
    EXPECT_EQ(bow.getCellRef().getNativeItemCondition(),50);
    auto release = service.prepareBowRelease(world,actor,id);
    MWMechanics::OblivionCombatService foreign(service);
    MWWorld::InventoryStore copied(inventory);
    EXPECT_FALSE(foreign.commitBowRelease(*release,inventory));
    EXPECT_FALSE(service.commitBowRelease(*release,copied));
    bow.getCellRef().setNativeItemCondition(0);
    const auto broken = captureNativeActorState(fixture,actors.npc).serializeBinary();
    EXPECT_THROW(service.prepareBowRelease(world,actor,id),std::invalid_argument);
    EXPECT_EQ(captureNativeActorState(fixture,actors.npc).serializeBinary(),broken);
    EXPECT_TRUE(service.isActionPending(id,ESM::FormKey::dynamic("player",1)));
    EXPECT_EQ(inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition)->getCellRef().getCount(),2);
}

TEST(OblivionWorldTest, PreparedBowReleaseLateFailuresAndDiscardAfterWorldDestructionPreserveResources)
{
    std::unique_ptr<MWMechanics::OblivionCombatService::PreparedBowRelease> retained;
    for (bool invalidSetting : {false,true})
    {
        NativeWorldFixture fixture;
        const auto actors = installPreparedBowRelease(fixture,invalidSetting);
        auto& world = fixture.mWorld;
        auto& service = *world.getOblivionCombatService();
        auto& inventory = actors.actor.getClass().getInventoryStore(actors.actor);
        const auto bow = *inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
        const auto ammo = *inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition);
        actors.actor.getClass().getCreatureStats(actors.actor).setDrawState(MWMechanics::DrawState::Weapon);
        const auto id = admitPreparedBowRelease(fixture,actors.actor);
        retained = service.prepareBowRelease(world,actors.actor,id);
        if (invalidSetting)
        {
            ESM4::GameSetting setting{}; setting.mId = {0x970,0};
            setting.mEditorId = "fDamageToWeaponPercentage";
            setting.mData = std::numeric_limits<float>::quiet_NaN();
            world.getStore().getWritable<ESM4::GameSetting>().insertStatic(
                setting,ESM::FormKey::content("headless.esm",0x970));
        }
        else
            bow.getCellRef().setNativeItemCondition(.1f);
        const auto before = captureNativeActorState(fixture,actors.npc).serializeBinary();
        const auto condition = bow.getCellRef().getNativeItemCondition();
        EXPECT_THROW(service.prepareBowRelease(world,actors.actor,id),std::invalid_argument);
        EXPECT_EQ(captureNativeActorState(fixture,actors.npc).serializeBinary(),before);
        EXPECT_EQ(bow.getCellRef().getNativeItemCondition(),condition);
        EXPECT_EQ(ammo.getCellRef().getCount(),2);
        const auto key = invalidSetting ? ESM::FormKey::dynamic("player",1) : actors.actor.getCellRef().getFormKey();
        EXPECT_TRUE(service.isActionPending(id,key));
    }
    // Immutable launch data and token destruction never dereference the dead
    // service, world, projection target or inventory.
    EXPECT_EQ(retained->launch().mAmmunition,ESM::FormKey::content("headless.esm",0x941));
    retained.reset();
}

TEST(OblivionWorldTest, PreparedUnequipPublishesSlotsWithoutQuantityChangesBeforeObservers)
{
    for (const int slot : {MWWorld::InventoryStore::Slot_CarriedRight, MWWorld::InventoryStore::Slot_Ammunition})
    {
        NativeWorldFixture fixture;
        const auto actors = installPreparedBowRelease(fixture, true);
        auto& inventory = actors.actor.getClass().getInventoryStore(actors.actor);
        const auto item = *inventory.getSlot(slot);
        const auto count = item.getCellRef().getCount();
        const auto weight = inventory.getWeight();
        struct Observer : MWWorld::InventoryStoreListener, MWWorld::ContainerStoreListener
        {
            MWWorld::InventoryStore& inventory;
            MWWorld::Ptr item;
            int slot, changed = 0, removed = 0;
            unsigned count;
            Observer(MWWorld::InventoryStore& inv, MWWorld::Ptr ptr, int s)
                : inventory(inv), item(ptr), slot(s), count(ptr.getCellRef().getCount()) {}
            void equipmentChanged() override
            {
                ++changed;
                EXPECT_EQ(inventory.getSlot(slot), inventory.end());
                EXPECT_EQ(item.getCellRef().getCount(), count);
                EXPECT_EQ(inventory.getSelectedEnchantItem(), inventory.end());
            }
            void itemRemoved(const MWWorld::ConstPtr&, int) override { ++removed; }
        } observer(inventory, item, slot);
        inventory.setInvListener(&observer); inventory.setContListener(&observer);
        inventory.setSelectedEnchantItem(inventory.getSlot(slot));
        auto cancelled = inventory.prepareUnequip(slot); cancelled.reset();
        EXPECT_EQ(*inventory.getSlot(slot), item);
        auto prepared = inventory.prepareUnequip(slot);
        EXPECT_FALSE(inventory.notifyPreparedUnequip(*prepared));
        ASSERT_TRUE(inventory.validatePreparedUnequip(*prepared));
        ASSERT_TRUE(inventory.commitPreparedUnequip(*prepared));
        EXPECT_FALSE(inventory.commitPreparedUnequip(*prepared));
        EXPECT_EQ(inventory.getSlot(slot), inventory.end());
        EXPECT_EQ(inventory.getSelectedEnchantItem(), inventory.end());
        EXPECT_EQ(item.getCellRef().getCount(), count);
        EXPECT_EQ(inventory.getWeight(), weight);
        EXPECT_EQ(observer.changed, 0); EXPECT_EQ(observer.removed, 0);
        ASSERT_TRUE(inventory.notifyPreparedUnequip(*prepared));
        EXPECT_EQ(observer.changed, 1); EXPECT_EQ(observer.removed, 0);
        EXPECT_FALSE(inventory.notifyPreparedUnequip(*prepared));
        EXPECT_THROW(inventory.prepareUnequip(slot), std::invalid_argument);
        inventory.setInvListener(nullptr); inventory.setContListener(nullptr);
    }
}

TEST(OblivionWorldTest, PreparedUnequipRejectsStaleForeignAndReplacedSlotsBeforeDereferencingItems)
{
    NativeWorldFixture fixture;
    const auto actor = installPreparedDebitAmmunition(fixture, 2);
    auto& inventory = actor.getClass().getInventoryStore(actor);
    const int slot = MWWorld::InventoryStore::Slot_Ammunition;
    EXPECT_THROW(inventory.prepareUnequip(-1), std::invalid_argument);
    EXPECT_THROW(inventory.prepareUnequip(MWWorld::InventoryStore::Slots), std::invalid_argument);
    EXPECT_THROW(inventory.prepareUnequip(MWWorld::InventoryStore::Slot_CarriedRight), std::invalid_argument);
    auto prepared = inventory.prepareUnequip(slot);
    MWWorld::InventoryStore copy(inventory);
    EXPECT_FALSE(copy.validatePreparedUnequip(*prepared));
    EXPECT_FALSE(copy.commitPreparedUnequip(*prepared));
    auto item = *inventory.getSlot(slot);
    item.getCellRef().setCount(3);
    EXPECT_FALSE(inventory.commitPreparedUnequip(*prepared));
    EXPECT_EQ(*inventory.getSlot(slot), item);
    auto cleared = inventory.prepareUnequip(slot);
    inventory.clear();
    EXPECT_FALSE(inventory.validatePreparedUnequip(*cleared));
    EXPECT_FALSE(inventory.commitPreparedUnequip(*cleared));
    inventory = copy;
    auto replaced = inventory.prepareUnequip(slot);
    inventory = copy;
    EXPECT_FALSE(inventory.commitPreparedUnequip(*replaced));
    auto swapped = inventory.prepareUnequip(slot);
    inventory.swapPreparedContents(copy);
    EXPECT_FALSE(inventory.commitPreparedUnequip(*swapped));
    EXPECT_FALSE(copy.commitPreparedUnequip(*swapped));
    auto moved = inventory.prepareUnequip(slot);
    MWWorld::InventoryStore::PreparedUnequip retained(std::move(*moved));
    EXPECT_FALSE(inventory.commitPreparedUnequip(*moved));
    EXPECT_TRUE(inventory.commitPreparedUnequip(retained));
}

TEST(OblivionWorldTest, PreparedUnequipObserverFailureOrInventoryClearCannotReplay)
{
    for (bool clear : {false, true})
    {
        NativeWorldFixture fixture;
        const auto actor = installPreparedDebitAmmunition(fixture, 2);
        auto& inventory = actor.getClass().getInventoryStore(actor);
        struct Observer : MWWorld::InventoryStoreListener
        {
            MWWorld::InventoryStore& inventory;
            bool clear;
            int calls = 0;
            Observer(MWWorld::InventoryStore& value, bool flag) : inventory(value), clear(flag) {}
            void equipmentChanged() override
            {
                ++calls;
                EXPECT_EQ(inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition), inventory.end());
                if (clear) inventory.clear();
                throw std::runtime_error("injected prepared unequip observer failure");
            }
        } observer(inventory, clear);
        inventory.setInvListener(&observer);
        auto prepared = inventory.prepareUnequip(MWWorld::InventoryStore::Slot_Ammunition);
        ASSERT_TRUE(inventory.commitPreparedUnequip(*prepared));
        EXPECT_THROW(inventory.notifyPreparedUnequip(*prepared), std::runtime_error);
        EXPECT_EQ(observer.calls, 1);
        EXPECT_FALSE(inventory.notifyPreparedUnequip(*prepared));
        EXPECT_FALSE(inventory.commitPreparedUnequip(*prepared));
        inventory.setInvListener(nullptr);
    }
}

TEST(OblivionWorldTest, PreparedUnequipOwnerReuseAndDiscardAfterDestructionAreSafe)
{
    NativeWorldFixture fixture;
    const auto actor = installPreparedDebitAmmunition(fixture, 2);
    auto& source = actor.getClass().getInventoryStore(actor);
    alignas(MWWorld::InventoryStore) std::array<std::byte, sizeof(MWWorld::InventoryStore)> memory;
    auto* owner = new (memory.data()) MWWorld::InventoryStore(source);
    auto old = owner->prepareUnequip(MWWorld::InventoryStore::Slot_Ammunition);
    owner->~InventoryStore();
    owner = new (memory.data()) MWWorld::InventoryStore(source);
    auto current = owner->prepareUnequip(MWWorld::InventoryStore::Slot_Ammunition);
    EXPECT_FALSE(owner->validatePreparedUnequip(*old));
    EXPECT_FALSE(owner->commitPreparedUnequip(*old));
    ASSERT_TRUE(owner->commitPreparedUnequip(*current));
    owner->~InventoryStore();
    old.reset(); current.reset();
}

TEST(OblivionWorldTest, PreparedPlayerBowBreakPublishesAllResourcesBeforeEquipmentObservers)
{
    NativeWorldFixture fixture;
    auto& world = fixture.mWorld;
    const auto actors = installPreparedBowRelease(fixture, true, 1);
    auto& inventory = actors.actor.getClass().getInventoryStore(actors.actor);
    auto& service = *world.getOblivionCombatService();
    const auto key = ESM::FormKey::dynamic("player", 1);
    const auto bow = *inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
    const auto ammo = *inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition);
    bow.getCellRef().setNativeItemCondition(.1f);
    actors.actor.getClass().getCreatureStats(actors.actor).setDrawState(MWMechanics::DrawState::Weapon);
    const auto id = admitPreparedBowRelease(fixture, actors.actor);
    const float fatigue = service.getPlayerValue(10);
    auto cancelled = service.prepareBowRelease(world, actors.actor, id); cancelled.reset();
    EXPECT_EQ(bow.getCellRef().getNativeItemCondition(), .1f);
    EXPECT_EQ(ammo.getCellRef().getCount(), 1);
    EXPECT_TRUE(service.isActionPending(id, key));
    struct Observer : MWWorld::InventoryStoreListener, MWWorld::ContainerStoreListener
    {
        MWWorld::InventoryStore& inventory;
        MWMechanics::OblivionCombatService& service;
        MWWorld::Ptr bow, ammo, actor;
        ESM::FormKey key;
        std::uint64_t id;
        float fatigue;
        int equipment = 0, removed = 0;
        Observer(MWWorld::InventoryStore& inv, MWMechanics::OblivionCombatService& authority,
            MWWorld::Ptr weapon, MWWorld::Ptr arrow, MWWorld::Ptr owner, ESM::FormKey identity,
            std::uint64_t action, float before)
            : inventory(inv), service(authority), bow(weapon), ammo(arrow), actor(owner),
              key(identity), id(action), fatigue(before) {}
        void coherent()
        {
            EXPECT_EQ(inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight), inventory.end());
            EXPECT_EQ(inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition), inventory.end());
            EXPECT_EQ(inventory.getSelectedEnchantItem(), inventory.end());
            EXPECT_EQ(bow.getCellRef().getNativeItemCondition(), 0.f);
            EXPECT_EQ(bow.getCellRef().getCount(), 1);
            EXPECT_EQ(ammo.getCellRef().getCount(), 0);
            EXPECT_FALSE(service.isActionPending(id, key));
            EXPECT_TRUE(service.findBowState(key)->mReleaseCommitted);
            EXPECT_EQ(service.findBowState(key)->mAction, 3);
            EXPECT_EQ(service.getPlayerValue(10), fatigue - 5);
            EXPECT_EQ(actor.getClass().getCreatureStats(actor).getFatigue().getCurrent(), fatigue - 5);
        }
        void equipmentChanged() override { ++equipment; coherent(); }
        void itemRemoved(const MWWorld::ConstPtr& item, int quantity) override
        {
            ++removed; EXPECT_EQ(item, ammo); EXPECT_EQ(quantity, 1); coherent();
        }
    } observer(inventory, service, bow, ammo, actors.actor, key, id, fatigue);
    inventory.setInvListener(&observer); inventory.setContListener(&observer);
    inventory.setSelectedEnchantItem(inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight));
    auto prepared = service.prepareBowRelease(world, actors.actor, id);
    ASSERT_TRUE(service.commitBowRelease(*prepared, inventory));
    EXPECT_EQ(observer.equipment, 0); EXPECT_EQ(observer.removed, 0);
    observer.coherent();
    EXPECT_FALSE(service.commitBowRelease(*prepared, inventory));
    ASSERT_TRUE(service.notifyBowRelease(*prepared, inventory));
    EXPECT_EQ(observer.equipment, 2); EXPECT_EQ(observer.removed, 1);
    EXPECT_FALSE(service.notifyBowRelease(*prepared, inventory));
    inventory.setInvListener(nullptr); inventory.setContListener(nullptr);
}

TEST(OblivionWorldTest, PreparedNpcQuestBowBreakKeepsAmmoAndOrdinaryBowDropRemainsAtomic)
{
    for (bool quest : {false, true})
    {
        NativeWorldFixture fixture; auto& world = fixture.mWorld;
        const auto actors = installPreparedBowRelease(fixture, false);
        auto& inventory = actors.actor.getClass().getInventoryStore(actors.actor);
        auto& service = *world.getOblivionCombatService();
        const auto key = actors.actor.getCellRef().getFormKey();
        const auto bow = *inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
        const auto ammo = *inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition);
        auto definition = *world.getStore().get<ESM4::Weapon>().search(ESM::FormId{0x940, 0});
        definition.mFlags = quest ? static_cast<std::uint32_t>(ESM4::Rec_Persistent) : 0u;
        world.getStore().getWritable<ESM4::Weapon>().insertStatic(
            definition, ESM::FormKey::content("headless.esm", 0x940));
        bow.getCellRef().setNativeItemCondition(.1f);
        actors.actor.getClass().getCreatureStats(actors.actor).setDrawState(MWMechanics::DrawState::Weapon);
        const auto id = admitPreparedBowRelease(fixture, actors.actor);
        const auto before = captureNativeActorState(fixture, actors.npc).serializeBinary();
        const float fatigue = service.getNonPlayerValue(actors.actor, 10);
        if (!quest)
        {
            EXPECT_THROW(service.prepareBowRelease(world, actors.actor, id), std::invalid_argument);
            EXPECT_EQ(captureNativeActorState(fixture, actors.npc).serializeBinary(), before);
            EXPECT_EQ(*inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight), bow);
            EXPECT_EQ(bow.getCellRef().getNativeItemCondition(), .1f);
            EXPECT_TRUE(service.isActionPending(id, key));
        }
        else
        {
            auto prepared = service.prepareBowRelease(world, actors.actor, id);
            ASSERT_TRUE(service.commitBowRelease(*prepared, inventory));
            EXPECT_EQ(inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight), inventory.end());
            EXPECT_EQ(bow.getCellRef().getNativeItemCondition(), 0.f);
            EXPECT_EQ(bow.getCellRef().getCount(), 1);
            EXPECT_EQ(service.getNonPlayerValue(actors.actor, 10), fatigue - 5);
            EXPECT_FALSE(service.isActionPending(id, key));
            EXPECT_TRUE(service.notifyBowRelease(*prepared, inventory));
        }
        EXPECT_EQ(ammo.getCellRef().getCount(), 2);
        EXPECT_EQ(*inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition), ammo);
    }
}

TEST(OblivionWorldTest, PreparedUnreadyBowBreakPreservesEquippedSlotAndGodModePreservesCondition)
{
    for (bool player : {false, true})
        for (auto draw : {MWMechanics::DrawState::Nothing, MWMechanics::DrawState::Spell})
        {
            NativeWorldFixture fixture; auto& world = fixture.mWorld;
            const auto actors = installPreparedBowRelease(fixture, player);
            auto& inventory = actors.actor.getClass().getInventoryStore(actors.actor);
            auto& service = *world.getOblivionCombatService();
            const auto bow = *inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
            const auto ammo = *inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition);
            bow.getCellRef().setNativeItemCondition(.1f);
            actors.actor.getClass().getCreatureStats(actors.actor).setDrawState(draw);
            const auto id = admitPreparedBowRelease(fixture, actors.actor);
            auto prepared = service.prepareBowRelease(world, actors.actor, id);
            ASSERT_TRUE(service.commitBowRelease(*prepared, inventory));
            EXPECT_EQ(*inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight), bow);
            EXPECT_EQ(bow.getCellRef().getNativeItemCondition(), 0.f);
            EXPECT_EQ(ammo.getCellRef().getCount(), player ? 1 : 2);
            EXPECT_TRUE(service.notifyBowRelease(*prepared, inventory));
        }
    NativeWorldFixture fixture; auto& world = fixture.mWorld;
    const auto actors = installPreparedBowRelease(fixture, true);
    auto& inventory = actors.actor.getClass().getInventoryStore(actors.actor);
    const auto bow = *inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
    bow.getCellRef().setNativeItemCondition(.1f);
    actors.actor.getClass().getCreatureStats(actors.actor).setDrawState(MWMechanics::DrawState::Weapon);
    const auto id = admitPreparedBowRelease(fixture, actors.actor);
    world.toggleGodMode();
    auto& service = *world.getOblivionCombatService();
    auto prepared = service.prepareBowRelease(world, actors.actor, id);
    ASSERT_TRUE(service.commitBowRelease(*prepared, inventory));
    EXPECT_EQ(bow.getCellRef().getNativeItemCondition(), .1f);
    EXPECT_EQ(*inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight), bow);
}

TEST(OblivionWorldTest, PreparedBowBreakRejectsChangedDrawStateBeforeAnyResourcePublication)
{
    NativeWorldFixture fixture; auto& world = fixture.mWorld;
    const auto actors = installPreparedBowRelease(fixture, true);
    auto& inventory = actors.actor.getClass().getInventoryStore(actors.actor);
    auto& service = *world.getOblivionCombatService();
    const auto bow = *inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
    const auto ammo = *inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition);
    bow.getCellRef().setNativeItemCondition(.1f);
    auto& stats = actors.actor.getClass().getCreatureStats(actors.actor);
    stats.setDrawState(MWMechanics::DrawState::Weapon);
    const auto id = admitPreparedBowRelease(fixture, actors.actor);
    auto prepared = service.prepareBowRelease(world, actors.actor, id);
    stats.setDrawState(MWMechanics::DrawState::Nothing);
    const auto before = captureNativeActorState(fixture, actors.npc).serializeBinary();
    EXPECT_FALSE(service.validateBowRelease(*prepared, inventory));
    EXPECT_FALSE(service.commitBowRelease(*prepared, inventory));
    EXPECT_EQ(captureNativeActorState(fixture, actors.npc).serializeBinary(), before);
    EXPECT_EQ(bow.getCellRef().getNativeItemCondition(), .1f);
    EXPECT_EQ(ammo.getCellRef().getCount(), 2);
    EXPECT_TRUE(service.isActionPending(id, ESM::FormKey::dynamic("player", 1)));
}

TEST(OblivionWorldTest, PreparedBowBreakObserversCannotReplayAfterThrowingOrClearingInventory)
{
    for (bool clear : {false, true})
    {
        NativeWorldFixture fixture; auto& world = fixture.mWorld;
        const auto actors = installPreparedBowRelease(fixture, true, 1);
        auto& inventory = actors.actor.getClass().getInventoryStore(actors.actor);
        auto& service = *world.getOblivionCombatService();
        const auto bow = *inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
        bow.getCellRef().setNativeItemCondition(.1f);
        actors.actor.getClass().getCreatureStats(actors.actor).setDrawState(MWMechanics::DrawState::Weapon);
        const auto id = admitPreparedBowRelease(fixture, actors.actor);
        struct Observer : MWWorld::InventoryStoreListener
        {
            MWWorld::InventoryStore& inventory;
            bool clear;
            int calls = 0;
            Observer(MWWorld::InventoryStore& value, bool flag) : inventory(value), clear(flag) {}
            void equipmentChanged() override
            {
                ++calls;
                EXPECT_EQ(inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight), inventory.end());
                EXPECT_EQ(inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition), inventory.end());
                if (clear) inventory.clear();
                throw std::runtime_error("injected bow break observer failure");
            }
        } observer(inventory, clear);
        inventory.setInvListener(&observer);
        auto prepared = service.prepareBowRelease(world, actors.actor, id);
        ASSERT_TRUE(service.commitBowRelease(*prepared, inventory));
        EXPECT_THROW(service.notifyBowRelease(*prepared, inventory), std::runtime_error);
        EXPECT_EQ(observer.calls, 1);
        EXPECT_FALSE(service.notifyBowRelease(*prepared, inventory));
        EXPECT_FALSE(service.commitBowRelease(*prepared, inventory));
        EXPECT_FALSE(service.isActionPending(id, ESM::FormKey::dynamic("player", 1)));
        inventory.setInvListener(nullptr);
    }
}

TEST(OblivionWorldTest, PreparedBowBreakClearingObserverCanReturnWithoutTouchingDeletedAmmunition)
{
    NativeWorldFixture fixture; auto& world = fixture.mWorld;
    const auto actors = installPreparedBowRelease(fixture, true, 1);
    auto& inventory = actors.actor.getClass().getInventoryStore(actors.actor);
    auto& service = *world.getOblivionCombatService();
    const auto bow = *inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
    bow.getCellRef().setNativeItemCondition(.1f);
    actors.actor.getClass().getCreatureStats(actors.actor).setDrawState(MWMechanics::DrawState::Weapon);
    const auto id = admitPreparedBowRelease(fixture, actors.actor);
    struct Observer : MWWorld::InventoryStoreListener, MWWorld::ContainerStoreListener
    {
        MWWorld::InventoryStore& inventory;
        int calls = 0, removed = 0;
        explicit Observer(MWWorld::InventoryStore& value) : inventory(value) {}
        void equipmentChanged() override
        {
            ++calls;
            EXPECT_EQ(inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight), inventory.end());
            EXPECT_EQ(inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition), inventory.end());
            inventory.clear();
        }
        void itemRemoved(const MWWorld::ConstPtr&, int) override { ++removed; }
    } observer(inventory);
    inventory.setInvListener(&observer); inventory.setContListener(&observer);
    auto prepared = service.prepareBowRelease(world, actors.actor, id);
    ASSERT_TRUE(service.commitBowRelease(*prepared, inventory));
    ASSERT_TRUE(service.notifyBowRelease(*prepared, inventory));
    EXPECT_EQ(observer.calls, 1);
    EXPECT_EQ(observer.removed, 0); // The cleared owner's old ammunition token is rejected.
    EXPECT_FALSE(service.notifyBowRelease(*prepared, inventory));
    EXPECT_FALSE(service.commitBowRelease(*prepared, inventory));
    EXPECT_FALSE(service.isActionPending(id, ESM::FormKey::dynamic("player", 1)));
    prepared.reset(); // Discard both borrowed-item tokens after clear destroyed their instances.
    inventory.setInvListener(nullptr); inventory.setContListener(nullptr);
}

TEST(OblivionWorldTest, NativePlacedItemExtrasPreserveIdentityExactConditionAndChargeAcrossCopies)
{
    ESM4::Reference authored{};
    authored.mFormKey = ESM::FormKey::dynamic("dropped-item", 1);
    authored.mBaseKey = ESM::FormKey::content("oblivion.esm", 0x200);
    authored.mBaseObj = ESM::FormId{0x200, 0};
    MWWorld::CellRef live(authored);
    EXPECT_FALSE(live.hasChanged());
    EXPECT_FALSE(live.getNativeItemCondition());
    EXPECT_EQ(live.getCharge(), -1);
    EXPECT_EQ(live.getItemCondition(50.f), 50.f);
    EXPECT_EQ(live.getEnchantmentCharge(), -1.f);
    for (const float condition : {0.f, -0.f, .1f, 49.8f, std::numeric_limits<float>::denorm_min(),
            std::numeric_limits<float>::max()})
    {
        SCOPED_TRACE(condition);
        live.setNativeItemCondition(condition);
        live.setEnchantmentCharge(7.25f);
        MWWorld::CellRef copied(live);
        EXPECT_EQ(copied.getFormKey(), authored.mFormKey);
        EXPECT_EQ(copied.getRefId(), ESM::RefId(authored.mBaseObj));
        ASSERT_TRUE(copied.getNativeItemCondition());
        EXPECT_EQ(std::bit_cast<std::uint32_t>(*copied.getNativeItemCondition()),
            std::bit_cast<std::uint32_t>(condition));
        EXPECT_EQ(copied.getItemCondition(50.f), condition);
        EXPECT_EQ(copied.getCharge(), static_cast<int>(std::min(double(std::numeric_limits<int>::max()),
            std::ceil(double(condition)))));
        EXPECT_EQ(copied.getEnchantmentCharge(), 7.25f);
        MWWorld::CellRef fresh(authored);
        std::swap(copied, fresh);
        EXPECT_FALSE(copied.getNativeItemCondition());
        EXPECT_EQ(copied.getEnchantmentCharge(), -1.f);
        EXPECT_EQ(fresh.getNativeItemCondition(), condition);
        EXPECT_EQ(fresh.getEnchantmentCharge(), 7.25f);
        fresh.setCharge(-1);
        EXPECT_FALSE(fresh.getNativeItemCondition());
        EXPECT_EQ(fresh.getItemCondition(50.f), 50.f);
        EXPECT_EQ(fresh.getEnchantmentCharge(), 7.25f);
        live.resetNativeItemCondition();
        EXPECT_FALSE(live.getNativeItemCondition());
    }
    MWWorld::CellRef independent(authored);
    EXPECT_FALSE(independent.getNativeItemCondition());
    EXPECT_EQ(independent.getEnchantmentCharge(), -1.f);
    for (const float charge : {0.f, -0.f, .1f, std::numeric_limits<float>::denorm_min(),
            std::numeric_limits<float>::max(), 0.f})
    {
        live.setEnchantmentCharge(charge);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(live.getEnchantmentCharge()), std::bit_cast<std::uint32_t>(charge));
        MWWorld::CellRef copied(live);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(copied.getEnchantmentCharge()), std::bit_cast<std::uint32_t>(charge));
    }
    live.setCharge(17);
    EXPECT_EQ(live.getNativeItemCondition(), 17.f);
    live.setEnchantmentCharge(-1.f);
    EXPECT_EQ(live.getEnchantmentCharge(), -1.f);
}

TEST(OblivionWorldTest, NativePlacedItemExtrasRejectInvalidValuesBeforeChangingAnInstance)
{
    const ESM4::Reference authored{};
    for (const float invalid : {-1.f, -2.f, std::numeric_limits<float>::infinity(),
            -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        MWWorld::CellRef fresh(authored);
        EXPECT_THROW(fresh.setNativeItemCondition(invalid), std::invalid_argument);
        EXPECT_FALSE(fresh.hasChanged());
        EXPECT_FALSE(fresh.getNativeItemCondition());
    }
    for (const float invalid : {-.5f, -2.f, std::numeric_limits<float>::infinity(),
            -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        MWWorld::CellRef fresh(authored);
        EXPECT_THROW(fresh.setEnchantmentCharge(invalid), std::invalid_argument);
        EXPECT_FALSE(fresh.hasChanged());
        EXPECT_EQ(fresh.getEnchantmentCharge(), -1.f);
    }
    MWWorld::CellRef fresh(authored);
    EXPECT_THROW(fresh.setCharge(-2), std::invalid_argument);
    EXPECT_FALSE(fresh.hasChanged());
    fresh.setNativeItemCondition(0.f);
    fresh.setEnchantmentCharge(0.f);
    EXPECT_THROW(fresh.setNativeItemCondition(-1.f), std::invalid_argument);
    EXPECT_THROW(fresh.setEnchantmentCharge(-2.f), std::invalid_argument);
    EXPECT_EQ(fresh.getNativeItemCondition(), 0.f);
    EXPECT_EQ(fresh.getEnchantmentCharge(), 0.f);
    ESM4::ActorCharacter actor{};
    MWWorld::CellRef actorRef(actor);
    EXPECT_THROW(actorRef.setNativeItemCondition(1.f), std::invalid_argument);
    EXPECT_FALSE(actorRef.hasChanged());
    EXPECT_FALSE(actorRef.getNativeItemCondition());
}

TEST(OblivionWorldTest, ProjectedInventoryConditionStorageRetainsLegacyChargeResetSemantics)
{
    ESM::CellRef item;
    item.blank();
    item.mChargeInt = 13;
    item.mChargeIntRemainder = .25f;
    MWWorld::CellRef live(item);
    EXPECT_EQ(live.getItemCondition(50.f), 13.25f);
    live.setNativeItemCondition(.1f);
    EXPECT_EQ(live.getItemCondition(50.f), .1f);
    live.setCharge(9);
    EXPECT_FALSE(live.getNativeItemCondition());
    EXPECT_EQ(live.getChargeIntRemainder(), 0.f);
    EXPECT_EQ(live.getItemCondition(50.f), 9.f);
    live.setNativeItemCondition(0.f);
    live.resetNativeItemCondition();
    EXPECT_EQ(live.getItemCondition(50.f), 9.f);
}

namespace
{
    MWWorld::Ptr installNativeLooseItemCapture(NativeWorldFixture& fixture)
    {
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
        MWClass::Npc::registerSelf();
        MWClass::ESM4Takeable<ESM4::Weapon>::registerSelf();
        world.setupPlayer();
        ESM::Race race{}; race.blank(); race.mId = ESM::RefId(ESM::FormId{0x810, 0});
        store.getWritable<ESM::Race>().insertStatic(race);
        auto playerBase = *world.getPlayerPtr().get<ESM::NPC>()->mBase;
        playerBase.mId = ESM::RefId::stringRefId("LooseItemCapturePlayer");
        playerBase.mRace = race.mId;
        store.insertStatic(playerBase);
        world.getPlayerPtr().get<ESM::NPC>()->mBase = store.get<ESM::NPC>().find(playerBase.mId);
        ESM4::Cell cell{}; cell.mId = ESM::RefId(ESM::FormId{1, 0});
        cell.mFormKey = ESM::FormKey::content("headless.esm", 1);
        cell.mCellFlags = ESM4::CELL_Interior; cell.mEditorId = "NativeLooseItemCaptureCell";
        store.getWritable<ESM4::Cell>().insertStatic(cell, cell.mFormKey);
        auto& residentCell = world.getWorldModel().getCell(cell.mId);
        world.getPlayer().setCell(&residentCell);
        ESM4::RuntimeActorValues values;
        values.mActor = ESM::FormKey::dynamic("player", 1);
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mPlayerFormValues = {{10, 0, 0, 0}};
        for (std::size_t i = 0; i < 8; ++i) values.mValues[i].mBase = 50;
        auto& combat = *world.getOblivionCombatService();
        combat.publishPlayerValues(world.getPlayer(), values,
            MWWorld::resolveOblivionPlayerDynamicBaseSettings(store));
        ESM4::RuntimeActorLife life; life.mActor = values.mActor; life.mBase = values.mBase;
        combat.publishPlayerLife(world.getPlayer(), life);
        ESM4::Weapon native{};
        native.mId = {0x940, 0}; const auto nativeKey = ESM::FormKey::content("headless.esm", 0x940);
        native.mData.type = 5; native.mData.health = 100;
        native.mEnchantment = {0x950, 0}; native.mEnchantmentPoints = 20;
        store.getWritable<ESM4::Weapon>().insertStatic(native, nativeKey);
        ESM4::Reference placed{};
        placed.mId = {0x960, 0}; placed.mFormKey = ESM::FormKey::content("headless.esm", 0x960);
        placed.mBaseObj = native.mId; placed.mBaseKey = nativeKey;
        placed.mParent = cell.mId; placed.mParentKey = cell.mFormKey;
        store.getWritable<ESM4::Reference>().insertStatic(placed, placed.mFormKey);
        MWWorld::LiveCellRef<ESM4::Weapon> live(placed, store.search<ESM4::Weapon>(nativeKey));
        const MWWorld::Ptr ptr(residentCell.insert(&live), &residentCell);
        world.getWorldModel().registerPtr(ptr);
        return ptr;
    }
}

TEST(OblivionWorldTest, NativeLooseItemSaveCaptureAndApplyPreserveBrokenFractionalAndAbsentExtras)
{
    NativeWorldFixture fixture;
    const auto item = installNativeLooseItemCapture(fixture);
    auto& world = fixture.mWorld;
    auto& ref = item.getCellRef();
    for (const auto value : {std::optional<float>{}, std::optional<float>{0.f},
            std::optional<float>{-0.f}, std::optional<float>{.1f}, std::optional<float>{49.8f},
            std::optional<float>{std::numeric_limits<float>::denorm_min()}})
    {
        ref.resetNativeItemCondition();
        if (value) ref.setNativeItemCondition(*value);
        ref.setEnchantmentCharge(value.value_or(-1.f));
        const auto saved = world.captureOblivionRuntimeState();
        ASSERT_EQ(saved.mVersion, ESM4::CurrentRuntimeStateVersion);
        ASSERT_EQ(saved.mReferences.size(), 1u);
        EXPECT_EQ(saved.mReferences.front().mItemCondition, value);
        EXPECT_EQ(saved.mReferences.front().mItemCharge, value);
        const auto key = ref.getFormKey();
        const auto number = ref.getRefNum();
        ref.setNativeItemCondition(75.f); ref.setEnchantmentCharge(19.f);
        readNativeSnapshot(fixture, ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        EXPECT_EQ(ref.getFormKey(), key); EXPECT_EQ(ref.getRefNum(), number);
        EXPECT_EQ(ref.getNativeItemCondition(), value);
        EXPECT_EQ(ref.getEnchantmentCharge(), value.value_or(-1.f));
        if (value)
        {
            EXPECT_EQ(std::bit_cast<std::uint32_t>(*ref.getNativeItemCondition()),
                std::bit_cast<std::uint32_t>(*value));
            EXPECT_EQ(std::bit_cast<std::uint32_t>(ref.getEnchantmentCharge()),
                std::bit_cast<std::uint32_t>(*value));
        }
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), saved.serializeBinary());
    }
    ref.resetNativeItemCondition(); ref.setEnchantmentCharge(-1.f);
    auto legacy = world.captureOblivionRuntimeState(); legacy.mVersion = 39;
    ref.setNativeItemCondition(0.f); ref.setEnchantmentCharge(0.f);
    readNativeSnapshot(fixture, legacy);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    EXPECT_FALSE(ref.getNativeItemCondition());
    EXPECT_EQ(ref.getItemCondition(100.f), 100.f);
    EXPECT_EQ(ref.getEnchantmentCharge(), -1.f);
}

TEST(OblivionWorldTest, NativeLooseItemRestoreRejectsWinningCategoryBeforePublishingEarlierResources)
{
    for (const bool condition : {false, true})
    {
        NativeWorldFixture fixture;
        const auto item = installNativeLooseItemCapture(fixture);
        auto& world = fixture.mWorld;
        item.getCellRef().setNativeItemCondition(.1f);
        item.getCellRef().setEnchantmentCharge(7.25f);
        auto saved = world.captureOblivionRuntimeState();
        saved.mClock.mHour = 9;
        saved.mReferences.front().mItemCondition = 50.f;
        saved.mReferences.front().mItemCharge = 15.f;
        auto base = *item.get<ESM4::Weapon>()->mBase;
        if (condition) base.mData.health = 0;
        else { base.mEnchantment = {}; base.mEnchantmentPoints = 0; }
        world.getStore().getWritable<ESM4::Weapon>().insertStatic(base, ESM::FormKey::content("headless.esm", 0x940));
        const auto before = world.getTimeStamp();
        const auto position = item.getRefData().getPosition();
        const auto conditionBefore = item.getCellRef().getNativeItemCondition();
        const auto chargeBefore = item.getCellRef().getEnchantmentCharge();
        readNativeSnapshot(fixture, saved);
        EXPECT_THROW(world.applyOblivionRuntimeState(), std::invalid_argument);
        EXPECT_EQ(world.getTimeStamp(), before);
        EXPECT_EQ(item.getRefData().getPosition(), position);
        EXPECT_EQ(item.getCellRef().getNativeItemCondition(), conditionBefore);
        EXPECT_EQ(item.getCellRef().getEnchantmentCharge(), chargeBefore);
    }
}

#include <components/esm3/readerscache.hpp>
#include <components/sceneutil/positionattitudetransform.hpp>

namespace
{
    MWWorld::LiveCellRef<ESM4::Weapon> makeDetachedNativeLooseItem(const MWWorld::Ptr& item,
        const ESM::FormKey& identity)
    {
        ESM4::Reference reference{};
        reference.mFormKey = identity;
        reference.mBaseObj = item.get<ESM4::Weapon>()->mBase->mId;
        reference.mBaseKey = ESM::FormKey::content("headless.esm", 0x940);
        reference.mParent = item.getCell()->getCell()->getId();
        reference.mParentKey = ESM::FormKey::content("headless.esm", 1);
        reference.mPos = item.getRefData().getPosition();
        MWWorld::LiveCellRef<ESM4::Weapon> result(reference, item.get<ESM4::Weapon>()->mBase);
        result.mRef.setNativeItemCondition(-0.f);
        result.mRef.setEnchantmentCharge(7.25f);
        return result;
    }
}

TEST(OblivionWorldTest, PreparedCellInsertionKeepsNativeNodeDetachedUntilRegistryAndCellPublication)
{
    NativeWorldFixture fixture;
    const auto item = installNativeLooseItemCapture(fixture);
    auto& world = fixture.mWorld;
    auto& model = world.getWorldModel();
    auto& cell = *item.getCell();
    const auto key = ESM::FormKey::dynamic("native-loose-item", 2);
    auto source = makeDetachedNativeLooseItem(item, key);
    const auto count = cell.count();
    const auto changed = cell.hasState();
    const auto revision = model.getPtrRegistryRevision();
    const auto last = model.getLastGeneratedRefNum();
    {
        auto cancelled = cell.prepareInsertion(source);
        ASSERT_NE(cancelled->get(), nullptr);
        EXPECT_EQ(cell.count(), count);
        EXPECT_EQ(cell.hasState(), changed);
        EXPECT_EQ(model.getPtrRegistryRevision(), revision);
        const std::array<MWWorld::Ptr, 1> inserted{MWWorld::Ptr(cancelled->get(), &cell)};
        auto registry = model.preparePtrReplacement({}, inserted);
        EXPECT_TRUE(inserted.front().getCellRef().getRefNum().isSet());
        EXPECT_EQ(model.getLastGeneratedRefNum(), last);
        EXPECT_TRUE(model.getPtr(inserted.front().getCellRef().getRefNum()).isEmpty());
        EXPECT_EQ(cell.count(), count);
    }
    EXPECT_EQ(model.getLastGeneratedRefNum(), last);
    EXPECT_EQ(model.getPtrRegistryRevision(), revision);
    EXPECT_EQ(cell.count(), count);
    EXPECT_FALSE(source.mRef.getRefNum().isSet());
    auto staged = cell.prepareInsertion(source);
    auto* node = staged->get();
    ASSERT_NE(node, nullptr);
    const MWWorld::Ptr projected(node, &cell);
    const std::array<MWWorld::Ptr, 1> inserted{projected};
    auto registry = model.preparePtrReplacement({}, inserted);
    const auto reserved = projected.getCellRef().getRefNum();
    ASSERT_TRUE(cell.validatePreparedInsertion(*staged));
    registry.commit();
    ASSERT_EQ(cell.commitPreparedInsertion(*staged), node);
    EXPECT_EQ(model.getPtr(reserved), projected);
    EXPECT_EQ(cell.count(), count + 1);
    EXPECT_TRUE(cell.hasState());
    EXPECT_EQ(projected.getCellRef().getFormKey(), key);
    ASSERT_TRUE(projected.getCellRef().getNativeItemCondition());
    EXPECT_EQ(std::bit_cast<std::uint32_t>(*projected.getCellRef().getNativeItemCondition()),
        std::bit_cast<std::uint32_t>(-0.f));
    EXPECT_EQ(projected.getCellRef().getEnchantmentCharge(), 7.25f);
    EXPECT_EQ(projected.get<ESM4::Weapon>()->mBase, source.mBase);
    EXPECT_FALSE(staged->get());
    EXPECT_FALSE(cell.validatePreparedInsertion(*staged));
    EXPECT_EQ(cell.commitPreparedInsertion(*staged), nullptr);
    EXPECT_EQ(cell.count(), count + 1);
    EXPECT_FALSE(source.mRef.getRefNum().isSet());
    EXPECT_EQ(item.getCellRef().getFormKey(), ESM::FormKey::content("headless.esm", 0x960));
}

TEST(OblivionWorldTest, PreparedCellInsertionRejectsForeignMovedAndSceneOwnedSources)
{
    NativeWorldFixture fixture;
    const auto item = installNativeLooseItemCapture(fixture);
    auto& cell = *item.getCell();
    auto& other = fixture.mWorld.getWorldModel().getDraftCell();
    auto source = makeDetachedNativeLooseItem(item, ESM::FormKey::dynamic("native-loose-item", 3));
    auto staged = cell.prepareInsertion(source);
    auto* node = staged->get();
    const auto count = cell.count();
    const auto otherCount = other.count();
    EXPECT_FALSE(other.validatePreparedInsertion(*staged));
    EXPECT_EQ(other.commitPreparedInsertion(*staged), nullptr);
    EXPECT_EQ(other.count(), otherCount);
    using Token = MWWorld::CellStore::PreparedInsertion<ESM4::Weapon>;
    static_assert(std::is_nothrow_move_constructible_v<Token>);
    static_assert(std::is_nothrow_move_assignable_v<Token>);
    static_assert(!std::is_copy_constructible_v<Token>);
    Token moved(std::move(*staged));
    EXPECT_FALSE(cell.validatePreparedInsertion(*staged));
    EXPECT_EQ(cell.commitPreparedInsertion(*staged), nullptr);
    EXPECT_TRUE(cell.validatePreparedInsertion(moved));
    EXPECT_EQ(moved.get(), node);
    EXPECT_EQ(cell.commitPreparedInsertion(moved), node);
    EXPECT_EQ(cell.count(), count + 1);
    EXPECT_THROW(cell.prepareInsertion(*item.get<ESM4::Weapon>()), std::invalid_argument);
    source.mRef.setRefNum(ESM::RefNum{0x980, 0});
    EXPECT_THROW(cell.prepareInsertion(source), std::invalid_argument);
    source.mRef.setRefNum({});
    source.mData.setBaseNode(new SceneUtil::PositionAttitudeTransform);
    EXPECT_THROW(cell.prepareInsertion(source), std::invalid_argument);
    EXPECT_EQ(cell.count(), count + 1);
}

TEST(OblivionWorldTest, PreparedCellInsertionLifetimeIdentityRejectsOwnerAddressReuse)
{
    NativeWorldFixture fixture;
    const auto item = installNativeLooseItemCapture(fixture);
    auto source = makeDetachedNativeLooseItem(item, ESM::FormKey::dynamic("native-loose-item", 4));
    ESM::Cell record{};
    record.blank();
    record.mId = ESM::RefId::stringRefId("PreparedCellLifetime");
    ESM::ReadersCache readers;
    alignas(MWWorld::CellStore) std::array<std::byte, sizeof(MWWorld::CellStore)> storage{};
    auto* owner = std::construct_at(reinterpret_cast<MWWorld::CellStore*>(storage.data()),
        MWWorld::Cell(record), fixture.mWorld.getStore(), readers);
    owner->load();
    ASSERT_EQ(owner->getState(), MWWorld::CellStore::State_Loaded);
    EXPECT_FALSE(owner->hasState());
    auto staged = owner->prepareInsertion(source);
    EXPECT_FALSE(owner->hasState());
    EXPECT_EQ(owner->count(), 0u);
    std::destroy_at(owner);
    owner = std::construct_at(reinterpret_cast<MWWorld::CellStore*>(storage.data()),
        MWWorld::Cell(record), fixture.mWorld.getStore(), readers);
    owner->load();
    auto fresh = owner->prepareInsertion(source);
    EXPECT_FALSE(owner->validatePreparedInsertion(*staged));
    EXPECT_EQ(owner->commitPreparedInsertion(*staged), nullptr);
    EXPECT_EQ(owner->count(), 0u);
    EXPECT_FALSE(owner->hasState());
    staged.reset();
    EXPECT_TRUE(owner->validatePreparedInsertion(*fresh));
    EXPECT_NE(owner->commitPreparedInsertion(*fresh), nullptr);
    EXPECT_TRUE(owner->hasState());
    EXPECT_EQ(owner->count(), 1u);
    std::destroy_at(owner);
    fresh.reset();
}

TEST(OblivionWorldTest, PreparedCellInsertionRejectsUnloadedTargetAndPreservesProjectedInventoryExtras)
{
    NativeWorldFixture fixture;
    const auto item = installNativeLooseItemCapture(fixture);
    auto native = makeDetachedNativeLooseItem(item, ESM::FormKey::dynamic("native-loose-item", 5));
    ESM::Cell record{};
    record.blank();
    record.mId = ESM::RefId::stringRefId("PreparedCellProjected");
    ESM::ReadersCache readers;
    MWWorld::CellStore owner(MWWorld::Cell(record), fixture.mWorld.getStore(), readers);
    EXPECT_THROW(owner.prepareInsertion(native), std::invalid_argument);
    EXPECT_EQ(owner.getState(), MWWorld::CellStore::State_Unloaded);
    EXPECT_FALSE(owner.hasState());
    owner.load();
    MWClass::Weapon::registerSelf();
    ESM::Weapon base{};
    base.blank();
    base.mId = ESM::RefId::stringRefId("PreparedLegacyWeapon");
    base.mData.mHealth = 100;
    fixture.mWorld.getStore().insertStatic(base);
    MWWorld::ManualRef source(fixture.mWorld.getStore(), base.mId, 2);
    source.getPtr().getCellRef().setNativeItemCondition(12.5f);
    source.getPtr().getCellRef().setEnchantmentCharge(7.25f);
    auto staged = owner.prepareInsertion(*source.getPtr().get<ESM::Weapon>());
    auto* node = staged->get();
    ASSERT_NE(node, nullptr);
    EXPECT_FALSE(owner.hasState());
    EXPECT_EQ(owner.count(), 0u);
    EXPECT_EQ(owner.commitPreparedInsertion(*staged), node);
    EXPECT_EQ(owner.count(), 1u);
    EXPECT_EQ(node->mRef.getRefId(), base.mId);
    EXPECT_EQ(node->mRef.getCount(), 2);
    EXPECT_EQ(node->mRef.getNativeItemCondition(), std::optional<float>{12.5f});
    EXPECT_EQ(node->mRef.getEnchantmentCharge(), 7.25f);
    EXPECT_EQ(source.getPtr().getCellRef().getNativeItemCondition(), std::optional<float>{12.5f});
    EXPECT_EQ(source.getPtr().getCellRef().getCount(), 2);
}

#include "apps/openmw/mwworld/oblivioncombatdata.hpp"
#include <components/esm4/loadgmst.hpp>
#include <components/esm3/loadgmst.hpp>

TEST(OblivionWorldTest, DropExtraOwnerResolverUsesFreshNativeSettingsAndRejectsLegacyFallback)
{
    using Choice = ESM4::DropExtraOwnerSelection;
    NativeWorldFixture fixture;
    auto& store = fixture.mWorld.getStore();
    const auto query = [&] {
        return MWWorld::resolveOblivionDropExtraOwner(store, ESM::GameProfile::Oblivion, 0, false, true, 46);
    };
    ESM::GameSetting legacy{};
    legacy.mId = ESM::RefId::stringRefId("fValueofItemForNoOwnership");
    legacy.mValue = ESM::Variant(100.f);
    store.getWritable<ESM::GameSetting>().insertStatic(legacy);
    EXPECT_EQ(query(), Choice::KeepExisting); // Native absent initializer45, never TES3 value100.
    ESM4::GameSetting native{};
    native.mId = {0x990, 0};
    native.mEditorId = "fValueofItemForNoOwnership";
    native.mData = 60.f;
    const auto key = ESM::FormKey::content("headless.esm", 0x990);
    store.getWritable<ESM4::GameSetting>().insertStatic(native, key);
    const auto earlier = query();
    EXPECT_EQ(earlier, Choice::PlayerBase);
    native.mData = 42.f;
    store.getWritable<ESM4::GameSetting>().insertStatic(native, key);
    EXPECT_EQ(query(), Choice::KeepExisting);
    EXPECT_EQ(earlier, Choice::PlayerBase);
    native.mData = std::int32_t{45};
    store.getWritable<ESM4::GameSetting>().insertStatic(native, key);
    EXPECT_THROW(query(), std::invalid_argument);
    native.mData = std::numeric_limits<float>::quiet_NaN();
    store.getWritable<ESM4::GameSetting>().insertStatic(native, key);
    EXPECT_THROW(query(), std::invalid_argument);
    ASSERT_TRUE(store.getWritable<ESM4::GameSetting>().eraseStatic(ESM::RefId(native.mId)));
    EXPECT_EQ(query(), Choice::KeepExisting);
    EXPECT_THROW(MWWorld::resolveOblivionDropExtraOwner(store, ESM::GameProfile::Morrowind,
        0, false, true, 1), std::invalid_argument);
}

TEST(OblivionWorldTest, PreparedWeaponRemovalPublishesFinalWearSlotAndQuantityBeforeObservers)
{
    NativeWorldFixture fixture;
    const auto actors = installPreparedBowRelease(fixture, false);
    auto& inventory = actors.actor.getClass().getInventoryStore(actors.actor);
    const auto bow = *inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
    const auto ammo = *inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition);
    const int ammoCount = ammo.getCellRef().getCount();
    bow.getCellRef().setEnchantmentCharge(7.25f);
    struct Observer : MWWorld::InventoryStoreListener, MWWorld::ContainerStoreListener
    {
        MWWorld::InventoryStore& inventory;
        MWWorld::Ptr bow, ammo;
        int ammoCount, equipment = 0, removed = 0;
        Observer(MWWorld::InventoryStore& store, MWWorld::Ptr b, MWWorld::Ptr a, int count)
            : inventory(store), bow(b), ammo(a), ammoCount(count) {}
        void check()
        {
            EXPECT_EQ(bow.getCellRef().getCount(), 0);
            EXPECT_EQ(bow.getCellRef().getNativeItemCondition(), 0);
            EXPECT_FLOAT_EQ(bow.getCellRef().getEnchantmentCharge(), 7.25f);
            EXPECT_EQ(inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight), inventory.end());
            EXPECT_EQ(inventory.getSelectedEnchantItem(), inventory.end());
            EXPECT_EQ(*inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition), ammo);
            EXPECT_EQ(ammo.getCellRef().getCount(), ammoCount);
        }
        void equipmentChanged() override { ++equipment; check(); }
        void itemRemoved(const MWWorld::ConstPtr& item, int count) override
        { ++removed; EXPECT_EQ(item, bow); EXPECT_EQ(count, 1); check(); }
    } observer(inventory, bow, ammo, ammoCount);
    inventory.setInvListener(&observer);
    inventory.setContListener(&observer);
    auto cancelled = inventory.prepareEquippedWeaponRemoval();
    cancelled.reset();
    EXPECT_EQ(bow.getCellRef().getCount(), 1);
    EXPECT_EQ(*inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight), bow);
    auto removal = inventory.prepareEquippedWeaponRemoval();
    EXPECT_FALSE(inventory.notifyPreparedEquippedWeaponRemoval(*removal));
    // The compound caller publishes final wear first. This primitive must
    // preserve that newer state instead of swapping an earlier item snapshot.
    bow.getCellRef().setNativeItemCondition(0);
    inventory.setSelectedEnchantItem(inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight));
    ASSERT_TRUE(inventory.commitPreparedEquippedWeaponRemoval(*removal));
    EXPECT_EQ(observer.equipment, 0);
    EXPECT_EQ(observer.removed, 0);
    observer.check();
    EXPECT_FALSE(inventory.commitPreparedEquippedWeaponRemoval(*removal));
    ASSERT_TRUE(inventory.notifyPreparedEquippedWeaponRemoval(*removal));
    EXPECT_EQ(observer.equipment, 1);
    EXPECT_EQ(observer.removed, 1);
    EXPECT_FALSE(inventory.notifyPreparedEquippedWeaponRemoval(*removal));
    EXPECT_THROW(inventory.prepareEquippedWeaponRemoval(), std::invalid_argument);
    inventory.setInvListener(nullptr);
    inventory.setContListener(nullptr);
}

TEST(OblivionWorldTest, PreparedWeaponRemovalRejectsChangedCountsCopiedClearedAndReusedInventories)
{
    NativeWorldFixture fixture;
    const auto actors = installPreparedBowRelease(fixture, false);
    auto& inventory = actors.actor.getClass().getInventoryStore(actors.actor);
    const auto bow = *inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
    auto removal = inventory.prepareEquippedWeaponRemoval();
    MWWorld::InventoryStore copy(inventory);
    EXPECT_FALSE(copy.commitPreparedEquippedWeaponRemoval(*removal));
    bow.getCellRef().setCount(2);
    EXPECT_FALSE(inventory.commitPreparedEquippedWeaponRemoval(*removal));
    EXPECT_EQ(bow.getCellRef().getCount(), 2);
    EXPECT_THROW(inventory.prepareEquippedWeaponRemoval(), std::invalid_argument);
    bow.getCellRef().setCount(1);
    inventory.clear();
    EXPECT_FALSE(inventory.commitPreparedEquippedWeaponRemoval(*removal));
    inventory = copy;
    auto assigned = inventory.prepareEquippedWeaponRemoval();
    inventory = copy;
    EXPECT_FALSE(inventory.commitPreparedEquippedWeaponRemoval(*assigned));
    auto swapped = inventory.prepareEquippedWeaponRemoval();
    inventory.swapPreparedContents(copy);
    EXPECT_FALSE(inventory.commitPreparedEquippedWeaponRemoval(*swapped));
    EXPECT_FALSE(copy.commitPreparedEquippedWeaponRemoval(*swapped));
    using Inventory = MWWorld::InventoryStore;
    alignas(Inventory) std::byte storage[sizeof(Inventory)];
    auto* owner = new (storage) Inventory(inventory);
    auto retired = owner->prepareEquippedWeaponRemoval();
    owner->~Inventory();
    owner = new (storage) Inventory(inventory);
    EXPECT_FALSE(owner->commitPreparedEquippedWeaponRemoval(*retired));
    EXPECT_FALSE(owner->notifyPreparedEquippedWeaponRemoval(*retired));
    auto current = owner->prepareEquippedWeaponRemoval();
    ASSERT_TRUE(owner->commitPreparedEquippedWeaponRemoval(*current));
    owner->~Inventory();
    retired.reset();
    current.reset();
}

TEST(OblivionWorldTest, PreparedWeaponRemovalObserversCannotReplayAfterThrowingOrDeletingInventory)
{
    for (bool throws : {false, true})
    {
        NativeWorldFixture fixture;
        const auto actors = installPreparedBowRelease(fixture, false);
        auto& inventory = actors.actor.getClass().getInventoryStore(actors.actor);
        struct Observer : MWWorld::InventoryStoreListener, MWWorld::ContainerStoreListener
        {
            MWWorld::InventoryStore& inventory;
            bool throws;
            int equipment = 0, removed = 0;
            Observer(MWWorld::InventoryStore& store, bool shouldThrow) : inventory(store), throws(shouldThrow) {}
            void equipmentChanged() override
            {
                ++equipment;
                inventory.clear();
                if (throws) throw std::runtime_error("weapon-removal observer failed after deleting item");
            }
            void itemRemoved(const MWWorld::ConstPtr&, int) override { ++removed; }
        } observer(inventory, throws);
        inventory.setInvListener(&observer);
        inventory.setContListener(&observer);
        auto removal = inventory.prepareEquippedWeaponRemoval();
        ASSERT_TRUE(inventory.commitPreparedEquippedWeaponRemoval(*removal));
        if (throws)
            EXPECT_THROW(inventory.notifyPreparedEquippedWeaponRemoval(*removal), std::runtime_error);
        else
            EXPECT_TRUE(inventory.notifyPreparedEquippedWeaponRemoval(*removal));
        EXPECT_FALSE(inventory.notifyPreparedEquippedWeaponRemoval(*removal));
        EXPECT_FALSE(inventory.commitPreparedEquippedWeaponRemoval(*removal));
        EXPECT_EQ(observer.equipment, 1);
        EXPECT_EQ(observer.removed, 0);
        inventory.setInvListener(nullptr);
        inventory.setContListener(nullptr);
    }
}

TEST(OblivionWorldTest, NativeReferenceKeyPreparationCancellationAndCommitUseTheSavedSerial)
{
    NativeWorldFixture fixture;
    installNativeLooseItemCapture(fixture);
    auto& world = fixture.mWorld;
    const auto before = world.captureOblivionRuntimeState();
    ASSERT_EQ(before.mNextDynamicSerial, 1u);
    {
        const auto cancelled = world.prepareOblivionDynamicReferenceKey();
        EXPECT_EQ(cancelled->key(), ESM::FormKey::dynamic("native-reference", 1));
        ASSERT_TRUE(cancelled->isValid());
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before.serializeBinary());
    }
    auto first = world.prepareOblivionDynamicReferenceKey();
    auto competing = world.prepareOblivionDynamicReferenceKey();
    EXPECT_EQ(first->key(), competing->key());
    ASSERT_TRUE(first->commit());
    EXPECT_FALSE(first->isValid());
    EXPECT_FALSE(first->commit());
    EXPECT_FALSE(competing->isValid());
    EXPECT_FALSE(competing->commit());
    const auto saved = world.captureOblivionRuntimeState();
    EXPECT_EQ(saved.mNextDynamicSerial, 2u);
    EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()).mNextDynamicSerial, 2u);
    const auto next = world.prepareOblivionDynamicReferenceKey();
    EXPECT_EQ(next->key(), ESM::FormKey::dynamic("native-reference", 2));
}

TEST(OblivionWorldTest, NativeReferenceKeyRegistryAndClearChangesRetireBorrowedPreparations)
{
    NativeWorldFixture fixture;
    const auto item = installNativeLooseItemCapture(fixture);
    auto& world = fixture.mWorld;
    auto stale = world.prepareOblivionDynamicReferenceKey();
    world.getWorldModel().registerPtr(item);
    EXPECT_FALSE(stale->isValid());
    EXPECT_FALSE(stale->commit());
    EXPECT_EQ(world.captureOblivionRuntimeState().mNextDynamicSerial, 1u);
    auto fresh = world.prepareOblivionDynamicReferenceKey();
    ASSERT_TRUE(fresh->isValid());
    world.clear();
    EXPECT_FALSE(fresh->isValid());
    EXPECT_FALSE(fresh->commit());
    const auto reset = world.prepareOblivionDynamicReferenceKey();
    EXPECT_EQ(reset->key(), ESM::FormKey::dynamic("native-reference", 1));
}

TEST(OblivionWorldTest, NativeReferenceKeySuccessfulRestoreRetiresSameSerialAndContinuesFreshSnapshot)
{
    NativeWorldFixture fixture;
    installNativeLooseItemCapture(fixture);
    auto& world = fixture.mWorld;
    const auto saved = world.captureOblivionRuntimeState();
    auto beforeRestore = world.prepareOblivionDynamicReferenceKey();
    readNativeSnapshot(fixture, saved);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    EXPECT_FALSE(beforeRestore->commit());
    auto first = world.prepareOblivionDynamicReferenceKey();
    ASSERT_TRUE(first->commit());
    const auto advanced = world.captureOblivionRuntimeState();
    auto pending = world.prepareOblivionDynamicReferenceKey();
    readNativeSnapshot(fixture, advanced);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    EXPECT_FALSE(pending->commit());
    auto continuation = world.prepareOblivionDynamicReferenceKey();
    EXPECT_EQ(continuation->key(), ESM::FormKey::dynamic("native-reference", 2));
    ASSERT_TRUE(continuation->commit());
    EXPECT_EQ(world.captureOblivionRuntimeState().mNextDynamicSerial, 3u);
}

TEST(OblivionWorldTest, NativeReferenceKeyRejectsSavedHighWaterCollisionsAndExhaustionBeforeMutation)
{
    NativeWorldFixture fixture;
    installNativeLooseItemCapture(fixture);
    auto& world = fixture.mWorld;
    const auto before = world.captureOblivionRuntimeState();
    auto pending = world.prepareOblivionDynamicReferenceKey();
    auto bad = before;
    bad.mClock.mHour = 9;
    bad.mReferences.front().mKey = ESM::FormKey::dynamic("native-reference", 1);
    readNativeSnapshot(fixture, bad);
    const auto time = world.getTimeStamp();
    EXPECT_THROW(world.applyOblivionRuntimeState(), std::invalid_argument);
    EXPECT_EQ(world.getTimeStamp(), time);
    EXPECT_EQ(world.captureOblivionRuntimeState().mNextDynamicSerial, 1u);
    EXPECT_TRUE(pending->isValid());
    EXPECT_NO_THROW(world.prepareOblivionDynamicReferenceKey()); // Pending identities are invisible.
    auto exhausted = before;
    exhausted.mNextDynamicSerial = std::numeric_limits<std::uint64_t>::max();
    readNativeSnapshot(fixture, exhausted);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    EXPECT_FALSE(pending->isValid());
    EXPECT_THROW(world.prepareOblivionDynamicReferenceKey(), std::overflow_error);
    EXPECT_EQ(world.captureOblivionRuntimeState().mNextDynamicSerial, exhausted.mNextDynamicSerial);
}

TEST(OblivionWorldTest, NativeReferenceKeyOwnerDestructionRejectsBeforeBorrowedWorldAccess)
{
    std::unique_ptr<MWWorld::World::PreparedOblivionDynamicReferenceKey> retained;
    {
        NativeWorldFixture fixture;
        installNativeLooseItemCapture(fixture);
        retained = fixture.mWorld.prepareOblivionDynamicReferenceKey();
        ASSERT_TRUE(retained->isValid());
    }
    EXPECT_FALSE(retained->isValid());
    EXPECT_FALSE(retained->commit());
}

TEST(OblivionWorldTest, NativeReferenceKeyResidentIdentityCollisionIncludesDisabledDeletedAndUnregisteredNodes)
{
    for (const std::uint32_t flags : {0u, std::uint32_t(ESM4::Rec_Disabled), std::uint32_t(ESM4::Rec_Deleted)})
    {
        NativeWorldFixture fixture;
        const auto item = installNativeLooseItemCapture(fixture);
        auto& world = fixture.mWorld;
        auto& cell = *item.getCell();
        cell.load();
        ESM4::Reference placed = *item.getCellRef().getNativeReference();
        placed.mId = {};
        placed.mFormKey = ESM::FormKey::dynamic("native-reference", 1);
        placed.mFlags = flags;
        MWWorld::LiveCellRef<ESM4::Weapon> duplicate(placed, item.get<ESM4::Weapon>()->mBase);
        cell.insert(&duplicate);
        EXPECT_THROW(world.prepareOblivionDynamicReferenceKey(), std::invalid_argument);
        EXPECT_EQ(world.captureOblivionRuntimeState().mNextDynamicSerial, 1u);
    }
}

#include "apps/openmw/mwrender/objects.hpp"
#include <components/sceneutil/unrefqueue.hpp>
#include "apps/openmw/mwrender/vismask.hpp"
#include <components/nifbullet/actorragdoll.hpp>

namespace
{
    struct CompoundWeaponSceneRoot : osg::Group
    {
        enum class Failure { None, Reject, InsertThrow };
        Failure mFailure = Failure::None;
        std::function<void()> mAfterInsert;
        bool addChild(osg::Node* child) override
        {
            if (mFailure == Failure::Reject) return false;
            const bool added = osg::Group::addChild(child);
            if (mAfterInsert) mAfterInsert();
            if (mFailure == Failure::InsertThrow) throw std::runtime_error("compound scene insertion failed");
            return added;
        }
    };

    struct CompoundEquippedWeaponPublication
    {
        MWWorld::InventoryStore& mInventory;
        MWWorld::InventoryStore::PreparedEquippedWeaponRemoval& mRemoval;
        MWWorld::World::PreparedOblivionDynamicReferenceKey& mKey;
        MWWorld::Ptr mSource;
        MWWorld::CellRef mFinalCondition;
        unsigned mValidations = 0, mPublications = 0;
        bool mSerialCommitted = false, mRemoved = false;
        CompoundEquippedWeaponPublication(MWWorld::InventoryStore& inventory,
            MWWorld::InventoryStore::PreparedEquippedWeaponRemoval& removal,
            MWWorld::World::PreparedOblivionDynamicReferenceKey& key, MWWorld::Ptr source)
            : mInventory(inventory), mRemoval(removal), mKey(key), mSource(source),
              mFinalCondition(source.getCellRef())
        { mFinalCondition.setNativeItemCondition(0); }
        static bool validate(void* opaque) noexcept
        {
            auto& c = *static_cast<CompoundEquippedWeaponPublication*>(opaque);
            ++c.mValidations;
            return c.mKey.isValid() && c.mInventory.validatePreparedEquippedWeaponRemoval(c.mRemoval)
                && c.mSource.getCellRef().getNativeItemCondition() == std::optional<float>(50)
                && c.mSource.getCellRef().getEnchantmentCharge() == 7.25f;
        }
        static void publish(void* opaque) noexcept
        {
            auto& c = *static_cast<CompoundEquippedWeaponPublication*>(opaque);
            ++c.mPublications;
            c.mSerialCommitted = c.mKey.commit();
            (void)c.mSource.getCellRef().swapNativeItemCondition(c.mFinalCondition);
            c.mRemoved = c.mInventory.commitPreparedEquippedWeaponRemoval(c.mRemoval);
        }
    };
}

TEST(OblivionWorldTest, CompoundNativeWeaponAdmissionPublishesActualNpcInventoryAndWorldSerialOrRollsBack)
{
    // Owned OSGT and a synthetic one-body definition isolate external asset
    // loading. This exercises actual World/InventoryStore/Objects/PhysicsSystem
    // publication, not the stock bow factory, native ownership or bow release.
    for (int outcome = 0; outcome < 5; ++outcome)
    {
        SCOPED_TRACE(outcome);
        NativeWorldFixture fixture;
        const auto resident = installNativeLooseItemCapture(fixture);
        auto& world = fixture.mWorld;
        const auto detachedNpc = addNativeNpc(fixture, 0x801);
        const auto npc = detachedNpc.getCell()->moveTo(detachedNpc, resident.getCell());
        auto& inventory = npc.getClass().getInventoryStore(npc);
        ESM::Weapon projection; projection.blank();
        projection.mId = ESM::RefId(ESM::FormId{0x940, 0});
        projection.mData.mType = ESM::Weapon::MarksmanCrossbow;
        projection.mData.mHealth = 100;
        world.getStore().insertStatic(projection);
        MWClass::Weapon::registerSelf();
        ESM4::RuntimeInventoryItem item;
        item.mBase = ESM::FormKey::content("headless.esm", 0x940);
        item.mCount = 1; item.mCondition = 50; item.mCharge = 7.25f;
        item.mEquippedSlots = ESM4::InventorySlotWeapon;
        installEquipmentInventory(fixture, npc, {item});
        const auto bow = *inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
        auto key = world.prepareOblivionDynamicReferenceKey();
        auto removal = inventory.prepareEquippedWeaponRemoval();

        osg::ref_ptr<osg::Group> model = new osg::Group;
        model->setName("Owned compound native weapon model");
        ASSERT_TRUE(osgDB::writeNodeFile(*model, (fixture.mDirectory / "compound-weapon.osgt").string()));
        fixture.mVfs.addArchive(std::make_unique<VFS::FileSystemArchive>(fixture.mDirectory));
        fixture.mVfs.buildIndex();
        SceneUtil::UnrefQueue unref;
        osg::ref_ptr<CompoundWeaponSceneRoot> root = new CompoundWeaponSceneRoot;
        MWRender::Objects objects(&fixture.mResources, root, unref);
        auto& physics = world.initializePhysics(new osg::Group);
        ESM4::Reference placed = *resident.getCellRef().getNativeReference();
        placed.mId = {}; placed.mFormKey = key->key();
        placed.mPos.pos[2] = 2;
        MWWorld::LiveCellRef<ESM4::Weapon> drop(placed, resident.get<ESM4::Weapon>()->mBase);
        drop.mRef.setNativeItemCondition(0);
        drop.mRef.setEnchantmentCharge(7.25f);
        NifBullet::ActorRagdollDefinition definition;
        NifBullet::RagdollBodyDefinition body{};
        body.mRecord = 12; body.mMass = 2; body.mInertia = {1,0,0,0,1,0,0,0,1};
        body.mShape = NifBullet::RagdollSphere{.5f};
        definition.mBodies.push_back(body);
        const std::array<btTransform, 1> poses{btTransform(btQuaternion::getIdentity(), btVector3(0,0,2))};
        auto prepared = world.getWorldModel().prepareLooseWeaponAdmission(*resident.getCell(), drop,
            objects, physics, "compound-weapon.osgt", osg::Quat(), MWRender::Mask_Object,
            definition, 1.f, poses, 1, -1);
        CompoundEquippedWeaponPublication context(inventory, *removal, *key, bow);
        MWWorld::WorldModel::LooseWeaponPublicationHooks hooks{
            &context, CompoundEquippedWeaponPublication::validate, CompoundEquippedWeaponPublication::publish};
        const auto cellCount = resident.getCell()->count();
        if (outcome == 1) root->mAfterInsert = [&] { bow.getCellRef().setNativeItemCondition(49); };
        if (outcome == 2) root->mAfterInsert = [&] { EXPECT_TRUE(key->commit()); };
        if (outcome == 3) root->mFailure = CompoundWeaponSceneRoot::Failure::Reject;
        if (outcome == 4) root->mFailure = CompoundWeaponSceneRoot::Failure::InsertThrow;
        if (outcome == 0)
        {
            const auto published = prepared->commit(hooks);
            ASSERT_FALSE(published.isEmpty());
            EXPECT_EQ(context.mValidations, 2u);
            EXPECT_EQ(context.mPublications, 1u);
            EXPECT_TRUE(context.mSerialCommitted); EXPECT_TRUE(context.mRemoved);
            EXPECT_EQ(bow.getCellRef().getCount(false), 0);
            EXPECT_EQ(bow.getCellRef().getNativeItemCondition(), std::optional<float>(0));
            EXPECT_EQ(inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight), inventory.end());
            EXPECT_EQ(published.getCellRef().getFormKey(), key->key());
            EXPECT_EQ(published.getCellRef().getNativeItemCondition(), std::optional<float>(0));
            EXPECT_FLOAT_EQ(published.getCellRef().getEnchantmentCharge(), 7.25f);
            EXPECT_EQ(resident.getCell()->count(), cellCount + 1);
            EXPECT_TRUE(physics.hasLooseObject(published));
            EXPECT_EQ(world.captureOblivionRuntimeState().mNextDynamicSerial, 2u);
            EXPECT_TRUE(prepared->commit(hooks).isEmpty());
            EXPECT_EQ(context.mPublications, 1u);
        }
        else
        {
            EXPECT_ANY_THROW(prepared->commit(hooks));
            EXPECT_EQ(context.mPublications, 0u);
            EXPECT_EQ(bow.getCellRef().getCount(false), 1);
            EXPECT_EQ(bow.getCellRef().getNativeItemCondition(), std::optional<float>(outcome == 1 ? 49 : 50));
            EXPECT_FLOAT_EQ(bow.getCellRef().getEnchantmentCharge(), 7.25f);
            EXPECT_EQ(*inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight), bow);
            EXPECT_EQ(resident.getCell()->count(), cellCount);
            EXPECT_EQ(root->getNumChildren(), 0u);
            EXPECT_TRUE(physics.looseObjectOwners().empty());
            EXPECT_EQ(world.captureOblivionRuntimeState().mNextDynamicSerial, outcome == 2 ? 2u : 1u);
            EXPECT_FALSE(prepared->isValid());
        }
    }
}

#include "apps/openmw/mwworld/oblivionownership.hpp"
#include <components/esm4/loadfact.hpp>

namespace
{
#include "native_cell_claim_expected.inc"

    struct NativeCellClaimFixture : NativeWorldFixture
    {
        MWWorld::Ptr mItem = installNativeLooseItemCapture(*this);
        MWWorld::Ptr mNpc;

        NativeCellClaimFixture()
        {
            auto& store = mWorld.getStore();
            auto base = *store.get<ESM4::Npc>().search(ESM::RefId(ESM::FormId{0x800, 0}));
            base.mId = {0x840, 0}; base.mFormKey = ESM::FormKey::content("headless.esm", 0x840);
            base.mEditorId = "CellClaimNpc";
            store.getWritable<ESM4::Npc>().insertStatic(base, base.mFormKey);
            MWClass::ESM4Npc::registerSelf();
            ESM4::ActorCharacter placed{}; placed.mId = {0x841, 0};
            placed.mFormKey = ESM::FormKey::content("headless.esm", 0x841);
            placed.mBaseObj = base.mId; placed.mBaseKey = base.mFormKey;
            placed.mParent = ESM::RefId(ESM::FormId{1, 0}); placed.mParentKey = ESM::FormKey::content("headless.esm", 1);
            store.getWritable<ESM4::ActorCharacter>().insertStatic(placed, placed.mFormKey);
            MWWorld::LiveCellRef<ESM4::Npc> live(placed, store.search<ESM4::Npc>(base.mFormKey));
            mNpc = MWWorld::Ptr(mItem.getCell()->insert(&live), mItem.getCell());
            mWorld.getWorldModel().registerPtr(mNpc);
            for (const std::uint32_t id : {0x850u, 0x851u})
            {
                ESM4::Faction faction{}; faction.mId = {id, 0};
                faction.mFormKey = ESM::FormKey::content("headless.esm", id);
                store.getWritable<ESM4::Faction>().insertStatic(faction, faction.mFormKey);
            }
            store.rebuildIdsIndex();
        }

        void configure(bool player, int owner, std::optional<std::int32_t> required,
            int membership = 0, std::int8_t rank = 0, std::uint8_t flags = 0)
        {
            auto& store = mWorld.getStore();
            const ESM::FormId baseId{player ? 0x800u : 0x840u, 0};
            auto base = *store.get<ESM4::Npc>().search(ESM::RefId(baseId));
            base.mFactions.clear();
            if (membership == 2) base.mFactions.push_back({0x851, 127, 0, 0, 0});
            if (membership != 0) base.mFactions.push_back({0x850, rank, 0, 0, 0});
            // The primary compatibility field must not replace the full list.
            base.mFaction = {0x851, -128, 0, 0, 0};
            store.getWritable<ESM4::Npc>().insertStatic(base,
                ESM::FormKey::content("headless.esm", baseId.mIndex));
            auto faction = *store.get<ESM4::Faction>().search(ESM::RefId(ESM::FormId{0x850, 0}));
            faction.mFactionFlags = flags;
            store.getWritable<ESM4::Faction>().insertStatic(faction, faction.mFormKey);
            auto cell = *store.get<ESM4::Cell>().search(ESM::RefId(ESM::FormId{1, 0}));
            switch (owner)
            {
                case 0: cell.mOwner = {}; break;
                case 1: cell.mOwner = baseId; break;
                case 2: cell.mOwner = {player ? 0x840u : 0x800u, 0}; break;
                case 3: cell.mOwner = {0x940, 0}; break;
                case 4: cell.mOwner = {0x850, 0}; break;
                default: throw std::invalid_argument("invalid test owner kind");
            }
            cell.mOwnershipRank = required;
            store.getWritable<ESM4::Cell>().insertStatic(cell, cell.mFormKey);
        }

        bool claim(bool player)
        {
            return MWWorld::oblivionActorHasCellOwnershipClaim(
                mWorld, player ? mWorld.getPlayerPtr() : mNpc, *mItem.getCell());
        }
    };
}

TEST(OblivionWorldTest, NativeCellClaimMatches3376IndependentOriginalInstructionResults)
{
    NativeCellClaimFixture fixture;
    const auto revision = fixture.mWorld.getWorldModel().getPtrRegistryRevision();
    std::size_t index = 0;
    const auto check = [&](bool player, int owner, std::optional<std::int32_t> required,
        int membership = 0, std::int8_t rank = 0, std::uint8_t flags = 0) {
        SCOPED_TRACE(index);
        fixture.configure(player, owner, required, membership, rank, flags);
        const auto digit = nativeCellClaimExpectedBits.at(index / 4);
        const auto nibble = digit <= '9' ? digit - '0' : digit - 'a' + 10;
        const bool expected = (nibble & (1 << (index % 4))) != 0;
        EXPECT_EQ(fixture.claim(player), expected);
        ++index;
    };
    // The two oracle x87 controls have separate recorded expected bits; this
    // integer-only adapter does not change or depend on the host x87 control.
    for (bool player : {false, true})
        for (int owner = 0; owner != 4; ++owner)
            for (int control = 0; control != 2; ++control)
                check(player, owner, {});
    const std::array<std::optional<std::int32_t>, 10> required{
        std::nullopt, std::numeric_limits<std::int32_t>::min(), -2, -1, 0,
        1, 5, 127, 128, std::numeric_limits<std::int32_t>::max()};
    for (bool player : {false, true})
        for (const auto requirement : required)
            for (int membership = 0; membership != 3; ++membership)
                for (const std::int8_t rank : {-128, -2, -1, 0, 1, 5, 127})
                    for (const std::uint8_t flags : {0, 1, 8, 9})
                        for (int control = 0; control != 2; ++control)
                            check(player, 4, requirement, membership, rank, flags);
    ASSERT_EQ(index, 3376u);
    ASSERT_EQ(index, nativeCellClaimExpectedBits.size() * 4);
    EXPECT_EQ(fixture.mWorld.getWorldModel().getPtrRegistryRevision(), revision);
}

TEST(OblivionWorldTest, NativeCellClaimReadsWinningRecordsWithoutCachingFactionOrRank)
{
    NativeCellClaimFixture fixture;
    for (bool player : {false, true})
    {
        fixture.configure(player, 4, 5, 2, 5, 0);
        EXPECT_TRUE(fixture.claim(player));
        fixture.configure(player, 4, 5, 2, 4, 0);
        EXPECT_FALSE(fixture.claim(player));
        fixture.configure(player, 4, {}, 1, -1, 0);
        EXPECT_FALSE(fixture.claim(player));
        fixture.configure(player, 4, -2, 0);
        EXPECT_TRUE(fixture.claim(player));
        fixture.configure(player, 1, {});
        EXPECT_TRUE(fixture.claim(player));
        fixture.configure(player, 2, {});
        EXPECT_FALSE(fixture.claim(player));
    }
}

TEST(OblivionWorldTest, NativeCellClaimRejectsInvalidWorldAndRecordBindings)
{
    NativeCellClaimFixture fixture;
    auto& world = fixture.mWorld;
    auto& cell = *fixture.mItem.getCell();
    EXPECT_THROW(MWWorld::oblivionActorHasCellOwnershipClaim(world, {}, cell), std::invalid_argument);
    EXPECT_THROW(MWWorld::oblivionActorHasCellOwnershipClaim(world, fixture.mItem, cell), std::invalid_argument);
    EXPECT_THROW(MWWorld::oblivionActorHasCellOwnershipClaim(
        world, fixture.mNpc, world.getWorldModel().getDraftCell()), std::invalid_argument);
    auto* live = fixture.mNpc.get<ESM4::Npc>();
    const auto* valid = live->mBase;
    auto stale = *valid; live->mBase = &stale;
    EXPECT_THROW(fixture.claim(false), std::invalid_argument);
    live->mBase = nullptr;
    EXPECT_THROW(fixture.claim(false), std::invalid_argument);
    live->mBase = valid;
    auto& store = world.getStore();
    auto broken = *store.get<ESM4::Cell>().search(ESM::RefId(ESM::FormId{1, 0}));
    broken.mOwner = {0xdead, 0};
    store.getWritable<ESM4::Cell>().insertStatic(broken, broken.mFormKey);
    EXPECT_THROW(fixture.claim(false), std::invalid_argument);
    fixture.configure(false, 1, {});
    EXPECT_TRUE(fixture.claim(false));
    world.getWorldModel().deregisterLiveCellRef(*fixture.mNpc.get<ESM4::Npc>());
    EXPECT_THROW(fixture.claim(false), std::invalid_argument);
}

TEST(OblivionWorldTest, NativeCellClaimValidCreatureBaseHasNoNpcOwnershipClaim)
{
    NativeCellClaimFixture fixture;
    const auto detached = addEquipmentCreature(fixture);
    const auto creature = detached.getCell()->moveTo(detached, fixture.mItem.getCell());
    fixture.mWorld.getWorldModel().registerPtr(creature);
    fixture.configure(false, 4, std::numeric_limits<std::int32_t>::min(), 1, 127, 0);
    EXPECT_FALSE(MWWorld::oblivionActorHasCellOwnershipClaim(
        fixture.mWorld, creature, *fixture.mItem.getCell()));
}

TEST(OblivionWorldTest, NativeCellClaimDistinguishesNonOwnerRecordsFromMissingAndDeletedOwners)
{
    NativeCellClaimFixture fixture;
    auto& store = fixture.mWorld.getStore();
    ESM4::Quest quest{};
    quest.mId = {0x860, 0}; quest.mFormKey = ESM::FormKey::content("headless.esm", 0x860);
    store.getWritable<ESM4::Quest>().insertStatic(quest, quest.mFormKey);
    store.rebuildIdsIndex();
    // QUST is deliberately omitted from the placeable-object ID cache.
    ASSERT_EQ(store.find(ESM::RefId(quest.mId)), 0);
    ASSERT_TRUE(store.hasEsm4ContentRecord(ESM::RefId(quest.mId)));
    auto cell = *store.get<ESM4::Cell>().search(ESM::RefId(ESM::FormId{1, 0}));
    cell.mOwner = quest.mId;
    store.getWritable<ESM4::Cell>().insertStatic(cell, cell.mFormKey);
    EXPECT_FALSE(fixture.claim(false));
    ASSERT_TRUE(store.getWritable<ESM4::Quest>().eraseStatic(ESM::RefId(quest.mId)));
    EXPECT_FALSE(store.hasEsm4ContentRecord(ESM::RefId(quest.mId)));
    EXPECT_THROW(fixture.claim(false), std::invalid_argument);
    fixture.configure(false, 4, 0, 1, 5);
    EXPECT_TRUE(fixture.claim(false));
    ASSERT_TRUE(store.getWritable<ESM4::Faction>().eraseStatic(ESM::RefId(ESM::FormId{0x850, 0})));
    EXPECT_THROW(fixture.claim(false), std::invalid_argument);
}

TEST(OblivionWorldTest, NativeCarriedWeaponPoseUsesTheLiveRenderedItemAndRejectsStaleBindings)
{
    NativeWorldFixture fixture;
    const auto actor = addNativeNpc(fixture, 0x900);
    auto& store = fixture.mWorld.getStore();
    auto npc = *actor.get<ESM4::Npc>()->mBase;
    npc.mModel = "m15-carried-skeleton.osgt";
    store.getWritable<ESM4::Npc>().insertStatic(npc, npc.mFormKey);
    MWClass::Weapon::registerSelf();
    const auto key = ESM::FormKey::content("headless.esm", 0x940);
    ESM4::Weapon native{}; native.mId = {0x940, 0};
    native.mData.health = 100; native.mData.type = 5;
    native.mModel = "m15-carried-bow.osgt";
    store.getWritable<ESM4::Weapon>().insertStatic(native, key);
    ESM::Weapon shared; shared.blank(); shared.mId = ESM::RefId(native.mId);
    shared.mData.mType = ESM::Weapon::MarksmanBow; shared.mData.mHealth = 100;
    shared.mModel = "m15-carried-bow.osgt";
    store.insertStatic(shared);

    osg::ref_ptr<osg::Group> skeleton = new osg::Group;
    skeleton->setName("Owned skeleton");
    osg::ref_ptr<osgAnimation::Bone> bone = new osgAnimation::Bone;
    bone->setName("weapon");
    bone->setDataVariance(osg::Object::DYNAMIC);
    bone->setMatrix(osg::Matrix::translate(3, -4, 5));
    skeleton->addChild(bone);
    osg::ref_ptr<osg::MatrixTransform> model = new osg::MatrixTransform;
    model->setName("Bow");
    model->setDataVariance(osg::Object::DYNAMIC);
    model->setMatrix(osg::Matrix::translate(1, 2, 3));
    std::filesystem::create_directories(fixture.mDirectory / "meshes");
    ASSERT_TRUE(osgDB::writeNodeFile(*skeleton,
        (fixture.mDirectory / "meshes/m15-carried-skeleton.osgt").string()));
    ASSERT_TRUE(osgDB::writeNodeFile(*model,
        (fixture.mDirectory / "meshes/m15-carried-bow.osgt").string()));
    fixture.mVfs.addArchive(std::make_unique<VFS::FileSystemArchive>(fixture.mDirectory));
    fixture.mVfs.buildIndex();
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
    ESM4::RuntimeInventoryItem saved;
    saved.mBase = key; saved.mCount = 1; saved.mCondition = 0.f; saved.mCharge = 7.25f;
    saved.mEquippedSlots = ESM4::InventorySlotWeapon;
    installEquipmentInventory(fixture, actor, {saved});
    auto& inventory = actor.getClass().getInventoryStore(actor);
    const auto item = *inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
    ASSERT_FLOAT_EQ(item.getCellRef().getEnchantmentCharge(), 7.25f);
    osg::ref_ptr<SceneUtil::PositionAttitudeTransform> placement = new SceneUtil::PositionAttitudeTransform;
    placement->setPosition({100, 20, 30});
    actor.getRefData().setBaseNode(placement);
    std::optional<MWRender::ESM4NpcAnimation::CarriedWeaponPose> retained;
    {
        MWRender::ESM4NpcAnimation animation(actor, placement, &fixture.mResources);
        EXPECT_FALSE(animation.captureCarriedWeaponPose(item));
        animation.showWeapons(true);
        auto pose = animation.captureCarriedWeaponPose(item);
        ASSERT_TRUE(pose);
        EXPECT_EQ(pose->mItem, item);
        EXPECT_EQ(pose->mModel, VFS::Path::Normalized("meshes/m15-carried-bow.osgt"));
        EXPECT_EQ(pose->mScene.getWorldMatrix().getTrans(), osg::Vec3(104, 18, 38));
        EXPECT_TRUE(animation.isCurrentCarriedWeaponPose(*pose));
        EXPECT_FALSE(animation.captureCarriedWeaponPose(actor));
        EXPECT_FALSE(animation.captureCarriedWeaponPose(MWWorld::ConstPtr()));
        placement->setPosition({101, 20, 30});
        EXPECT_FALSE(animation.isCurrentCarriedWeaponPose(*pose));
        placement->setPosition({100, 20, 30});
        EXPECT_TRUE(animation.isCurrentCarriedWeaponPose(*pose));
        item.getCellRef().setCount(2);
        EXPECT_FALSE(animation.isCurrentCarriedWeaponPose(*pose));
        EXPECT_FALSE(animation.captureCarriedWeaponPose(item));
        item.getCellRef().setCount(1);
        animation.refreshEquipment();
        EXPECT_FALSE(animation.isCurrentCarriedWeaponPose(*pose));
        auto refreshed = animation.captureCarriedWeaponPose(item);
        ASSERT_TRUE(refreshed);
        EXPECT_TRUE(animation.isCurrentCarriedWeaponPose(*refreshed));
        animation.showWeapons(false);
        EXPECT_FALSE(animation.isCurrentCarriedWeaponPose(*refreshed));
        EXPECT_FALSE(animation.captureCarriedWeaponPose(item));
        animation.showWeapons(true);
        retained = animation.captureCarriedWeaponPose(item);
        ASSERT_TRUE(retained);
        const auto inventoryBefore = fixture.mWorld.captureOblivionActorInventory(actor);
        EXPECT_FLOAT_EQ(inventoryBefore.front().mCondition, 0.f);
        EXPECT_FLOAT_EQ(item.getCellRef().getEnchantmentCharge(), 7.25f);
        EXPECT_EQ(item.getCellRef().getCount(), 1);
        EXPECT_EQ(*inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight), item);
    }
    EXPECT_TRUE(retained->mBindingIdentity.expired());
    EXPECT_FALSE(retained->mScene.matchesCurrentScene());
}

TEST(OblivionWorldTest, NativeOwnershipExtrasCaptureAndRestoreExactPresenceWhileOlderSavesRetainLiveExtras)
{
    NativeWorldFixture fixture;
    const auto item = installNativeLooseItemCapture(fixture);
    auto& world = fixture.mWorld;
    auto& ref = item.getCellRef();
    ESM4::GlobalVariable permission{};
    permission.mId = {0x970, 0};
    permission.mType = 'f'; permission.mValue = 1.f;
    const auto permissionKey = ESM::FormKey::content("headless.esm", 0x970);
    world.getStore().getWritable<ESM4::GlobalVariable>().insertStatic(permission, permissionKey);
    const ESM::RefId permissionId(permission.mId);
    const std::array<std::optional<std::int32_t>, 8> ranks{std::nullopt,
        std::numeric_limits<std::int32_t>::min(), -2, -1, 0, 1, 9,
        std::numeric_limits<std::int32_t>::max()};
    for (const auto rank : ranks)
    for (const bool withGlobal : {false, true})
    {
        ref.setNativeOwnershipRank(rank);
        ref.setNativeOwnershipGlobal(withGlobal ? permissionId : ESM::RefId{});
        ref.setNativeItemCondition(0.f);
        ref.setEnchantmentCharge(7.25f);
        const auto saved = world.captureOblivionRuntimeState();
        ASSERT_EQ(saved.mVersion, ESM4::CurrentRuntimeStateVersion);
        ASSERT_EQ(saved.mReferences.size(), 1u);
        EXPECT_EQ(saved.mReferences.front().mOwnershipRank, rank);
        EXPECT_EQ(saved.mReferences.front().mOwnershipGlobal, withGlobal ? permissionKey : ESM::FormKey{});
        const auto key = ref.getFormKey();
        const auto number = ref.getRefNum();
        ref.setNativeOwnershipRank(3);
        ref.setNativeOwnershipGlobal({});
        ref.setNativeItemCondition(75.f);
        readNativeSnapshot(fixture, ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        EXPECT_EQ(ref.getFormKey(), key);
        EXPECT_EQ(ref.getRefNum(), number);
        EXPECT_EQ(ref.getNativeOwnershipRank(), rank);
        EXPECT_EQ(ref.getNativeOwnershipGlobal(), withGlobal ? permissionId : ESM::RefId{});
        EXPECT_EQ(ref.getNativeItemCondition(), 0.f);
        EXPECT_EQ(ref.getEnchantmentCharge(), 7.25f);
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), saved.serializeBinary());
    }
    auto old = world.captureOblivionRuntimeState();
    old.mVersion = 40;
    old.mReferences.front().mOwnershipRank.reset();
    old.mReferences.front().mOwnershipGlobal = {};
    ref.setNativeOwnershipRank(-2);
    ref.setNativeOwnershipGlobal(permissionId);
    readNativeSnapshot(fixture, ESM4::RuntimeState::deserializeBinary(old.serializeBinary()));
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    EXPECT_EQ(ref.getNativeOwnershipRank(), -2);
    EXPECT_EQ(ref.getNativeOwnershipGlobal(), permissionId);
}

TEST(OblivionWorldTest, NativeOwnershipRestoreRejectsMissingOrWrongGlobalBeforePublishingAnyReference)
{
    for (const auto& global : {ESM::FormKey::content("headless.esm", 0x999),
            ESM::FormKey::content("headless.esm", 0x940)})
    {
        NativeWorldFixture fixture;
        const auto item = installNativeLooseItemCapture(fixture);
        auto& world = fixture.mWorld;
        auto saved = world.captureOblivionRuntimeState();
        saved.mClock.mHour = 9;
        saved.mReferences.front().mOwnershipRank = -1;
        saved.mReferences.front().mOwnershipGlobal = global;
        const auto before = world.getTimeStamp();
        const auto position = item.getRefData().getPosition();
        const auto condition = item.getCellRef().getNativeItemCondition();
        const auto rank = item.getCellRef().getNativeOwnershipRank();
        const auto ownershipGlobal = item.getCellRef().getNativeOwnershipGlobal();
        readNativeSnapshot(fixture, saved);
        EXPECT_THROW(world.applyOblivionRuntimeState(), std::invalid_argument);
        EXPECT_EQ(world.getTimeStamp(), before);
        EXPECT_EQ(item.getRefData().getPosition(), position);
        EXPECT_EQ(item.getCellRef().getNativeItemCondition(), condition);
        EXPECT_EQ(item.getCellRef().getNativeOwnershipRank(), rank);
        EXPECT_EQ(item.getCellRef().getNativeOwnershipGlobal(), ownershipGlobal);
    }
}

TEST(OblivionWorldTest, ActualInventoryStackingDistinguishesNativeRankPresenceAndGlobal)
{
    NativeWorldFixture fixture;
    const auto actors = installPreparedBowRelease(fixture, true);
    auto& world = fixture.mWorld;
    auto& inventory = actors.actor.getClass().getInventoryStore(actors.actor);
    const auto id = ESM::RefId(ESM::FormId{0x940, 0});
    MWWorld::ManualRef first(world.getStore(), id, 1), second(world.getStore(), id, 1);
    auto& left = first.getPtr().getCellRef();
    auto& right = second.getPtr().getCellRef();
    left.setNativeItemCondition(100.f);
    right.setNativeItemCondition(100.f);
    ASSERT_TRUE(inventory.stacks(first.getPtr(), second.getPtr()));
    right.setNativeOwnershipRank(-1);
    EXPECT_FALSE(inventory.stacks(first.getPtr(), second.getPtr()));
    left.setNativeOwnershipRank(-1);
    EXPECT_TRUE(inventory.stacks(first.getPtr(), second.getPtr()));
    right.setNativeOwnershipRank(-2);
    EXPECT_FALSE(inventory.stacks(first.getPtr(), second.getPtr()));
    left.setNativeOwnershipRank(-2);
    EXPECT_TRUE(inventory.stacks(first.getPtr(), second.getPtr()));
    const ESM::RefId global(ESM::FormId{0x970, 0});
    right.setNativeOwnershipGlobal(global);
    EXPECT_FALSE(inventory.stacks(first.getPtr(), second.getPtr()));
    left.setNativeOwnershipGlobal(global);
    EXPECT_TRUE(inventory.stacks(first.getPtr(), second.getPtr()));
    right.setNativeOwnershipGlobal(ESM::RefId(ESM::FormId{0x971, 0}));
    EXPECT_FALSE(inventory.stacks(first.getPtr(), second.getPtr()));
}

    TEST(OblivionWorldTest, NativeRemovalPublishesAllStacksAndEquipmentBeforeObservers)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        MWClass::Npc::registerSelf(); world.setupPlayer();
        const auto key = addEquipmentWeapon(fixture);
        const auto player = world.getPlayerPtr();
        ESM4::RuntimeInventoryItem a; a.mBase = key; a.mCount = 1; a.mCondition = 40;
        a.mEquippedSlots = ESM4::InventorySlotWeapon;
        auto b = a; b.mCount = 2; b.mCondition = 50; b.mEquippedSlots = 0;
        installEquipmentInventory(fixture, player, {a, b});
        auto& inventory = player.getClass().getInventoryStore(player);
        struct Listener : MWWorld::ContainerStoreListener, MWWorld::InventoryStoreListener
        {
            MWWorld::World* mWorld;
            MWWorld::InventoryStore* mInventory;
            ESM::FormKey mKey;
            int mCalls = 0, mEquipment = 0, mRemoved = 0;
            void equipmentChanged() override
            {
                ++mEquipment;
                EXPECT_EQ(mWorld->oblivionPlayerItemCount(mKey), 0);
                EXPECT_EQ(mInventory->getSlot(MWWorld::InventoryStore::Slot_CarriedRight), mInventory->end());
                EXPECT_EQ(mCalls, 0);
            }
            void itemRemoved(const MWWorld::ConstPtr&, int count) override
            {
                ++mCalls; mRemoved += count;
                EXPECT_EQ(mEquipment, 1);
                EXPECT_EQ(mWorld->oblivionPlayerItemCount(mKey), 0);
            }
        } listener;
        listener.mWorld = &world; listener.mInventory = &inventory; listener.mKey = key;
        inventory.setContListener(&listener); inventory.setInvListener(&listener);
        EXPECT_EQ(world.oblivionChangePlayerInventory(key, -3), 3);
        EXPECT_EQ(listener.mCalls, 2); EXPECT_EQ(listener.mRemoved, 3);
        EXPECT_EQ(listener.mEquipment, 1);
        inventory.setContListener(nullptr); inventory.setInvListener(nullptr);
    }

    TEST(OblivionWorldTest, NativeRemovalClearCallbackStopsRetiredNotificationsAndRefreshesCache)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        MWClass::Npc::registerSelf(); world.setupPlayer();
        const auto key = addEquipmentWeapon(fixture);
        const auto player = world.getPlayerPtr();
        ESM4::RuntimeInventoryItem a; a.mBase = key; a.mCount = 1; a.mCondition = 40;
        auto b = a; b.mCount = 2; b.mCondition = 50;
        ESM4::RuntimeState cache;
        cache.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        cache.mPlayer.mCell = ESM::FormKey::content("headless.esm", 1);
        cache.mPlayer.mRace = ESM::FormKey::content("headless.esm", 0x810);
        cache.mPlayer.mClass = ESM::FormKey::dynamic("fixture-class", 1);
        cache.mPlayer.mInventory = {a, b};
        acceptNativeSnapshot(fixture, cache);
        installEquipmentInventory(fixture, player, {a, b});
        auto& inventory = player.getClass().getInventoryStore(player);
        struct Listener : MWWorld::ContainerStoreListener
        {
            MWWorld::InventoryStore* mInventory;
            int mCalls = 0;
            void itemRemoved(const MWWorld::ConstPtr&, int) override { ++mCalls; mInventory->clear(); }
        } listener;
        listener.mInventory = &inventory; inventory.setContListener(&listener);
        EXPECT_EQ(world.oblivionChangePlayerInventory(key, -3), 3);
        EXPECT_EQ(listener.mCalls, 1);
        EXPECT_TRUE(world.captureOblivionActorInventory(player).empty());
        EXPECT_FALSE(world.oblivionSetPlayerHotkey(ESM::RefId(ESM::FormId{0x940, 0}), 1));
        inventory.setContListener(nullptr);
    }

    TEST(OblivionWorldTest, NativeRemovalObserverFailuresDoNotReplayAndCaptureFinalLiveCounts)
    {
        for (const bool restore : {false, true})
        {
            SCOPED_TRACE(restore);
            NativeWorldFixture fixture;
            auto& world = fixture.mWorld;
            MWClass::Npc::registerSelf(); world.setupPlayer();
            const auto key = addEquipmentWeapon(fixture);
            const auto player = world.getPlayerPtr();
            ESM4::RuntimeInventoryItem a; a.mBase = key; a.mCount = 1; a.mCondition = 40;
            auto b = a; b.mCount = 2; b.mCondition = 50;
            ESM4::RuntimeState cache;
            cache.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
            cache.mPlayer.mCell = ESM::FormKey::content("headless.esm", 1);
            cache.mPlayer.mRace = ESM::FormKey::content("headless.esm", 0x810);
            cache.mPlayer.mClass = ESM::FormKey::dynamic("fixture-class", 1);
            cache.mPlayer.mInventory = {a, b};
            acceptNativeSnapshot(fixture, cache);
            installEquipmentInventory(fixture, player, {a, b});
            auto& inventory = player.getClass().getInventoryStore(player);
            struct Listener : MWWorld::ContainerStoreListener
            {
                bool mRestore = false;
                MWWorld::Ptr mRestorePtr;
                int mCalls = 0;
                void itemRemoved(const MWWorld::ConstPtr&, int) override
                {
                    ++mCalls;
                    if (mRestore && mCalls == 2) mRestorePtr.getCellRef().setCount(5);
                    throw std::runtime_error("removal observer failed");
                }
            } listener;
            listener.mRestore = restore;
            auto second = inventory.begin(); ++second; listener.mRestorePtr = *second;
            inventory.setContListener(&listener);
            try { world.oblivionChangePlayerInventory(key, -3); FAIL() << "observer failure was swallowed"; }
            catch (const std::runtime_error& e) { EXPECT_STREQ(e.what(), "removal observer failed"); }
            EXPECT_EQ(listener.mCalls, 2);
            EXPECT_EQ(world.oblivionPlayerItemCount(key), restore ? 5 : 0);
            EXPECT_EQ(world.oblivionSetPlayerHotkey(ESM::RefId(ESM::FormId{0x940, 0}), 1), restore);
            inventory.setContListener(nullptr);
        }
    }

    TEST(OblivionWorldTest, PreparedNativeRemovalSurvivesDestructionByEquipmentOrItemObserver)
    {
        for (const bool equipment : {false, true})
        {
            SCOPED_TRACE(equipment);
            NativeWorldFixture fixture;
            const auto key = addEquipmentWeapon(fixture);
            ESM4::RuntimeInventoryItem item; item.mBase = key; item.mCount = 1; item.mCondition = 40;
            item.mEquippedSlots = ESM4::InventorySlotWeapon;
            const auto sources = MWWorld::OblivionProfileServices::prepareActorInventory(
                fixture.mWorld.getStore(), ESM::FormKeyResolver({"headless.esm"}), {item});
            auto owner = MWWorld::OblivionProfileServices::stageActorInventory(sources);
            struct Listener : MWWorld::ContainerStoreListener, MWWorld::InventoryStoreListener
            {
                std::unique_ptr<MWWorld::InventoryStore>* mOwner;
                bool mEquipment;
                int mCalls = 0;
                void equipmentChanged() override { if (mEquipment) { ++mCalls; mOwner->reset(); } }
                void itemRemoved(const MWWorld::ConstPtr&, int) override
                { if (!mEquipment) { ++mCalls; mOwner->reset(); } }
            } listener;
            listener.mOwner = &owner; listener.mEquipment = equipment;
            owner->setInvListener(&listener); owner->setContListener(&listener);
            auto removal = owner->prepareItemRemoval(sources.front().mReference.getPtr().getCellRef().getRefId(), 1);
            ASSERT_TRUE(removal->commit());
            EXPECT_TRUE(removal->notify());
            EXPECT_EQ(listener.mCalls, 1); EXPECT_EQ(owner, nullptr);
            EXPECT_FALSE(removal->ownerIsCurrent());
            EXPECT_FALSE(removal->isValid()); EXPECT_FALSE(removal->commit()); EXPECT_FALSE(removal->notify());
        }
    }

    TEST(OblivionWorldTest, PreparedNativeRemovalRejectsStaleCountsSlotsAndStorage)
    {
        NativeWorldFixture fixture;
        const auto key = addEquipmentWeapon(fixture);
        ESM4::RuntimeInventoryItem item; item.mBase = key; item.mCount = 3; item.mCondition = 40;
        item.mEquippedSlots = ESM4::InventorySlotWeapon;
        const auto sources = MWWorld::OblivionProfileServices::prepareActorInventory(
            fixture.mWorld.getStore(), ESM::FormKeyResolver({"headless.esm"}), {item});
        const auto base = sources.front().mReference.getPtr().getCellRef().getRefId();
        for (int invalidation = 0; invalidation < 8; ++invalidation)
        {
            SCOPED_TRACE(invalidation);
            auto owner = MWWorld::OblivionProfileServices::stageActorInventory(sources);
            auto replacement = MWWorld::OblivionProfileServices::stageActorInventory(sources);
            auto removal = owner->prepareItemRemoval(base, 2);
            ASSERT_TRUE(removal->isValid());
            const auto held = *owner->getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
            if (invalidation == 0) held.getCellRef().setCount(4);
            if (invalidation == 1) owner->unequipSlot(MWWorld::InventoryStore::Slot_CarriedRight, false);
            if (invalidation == 2) owner->clear();
            if (invalidation == 3) *owner = *replacement;
            if (invalidation == 4) *owner = std::move(*replacement);
            if (invalidation == 5) *replacement = std::move(*owner);
            if (invalidation == 6) owner->swapPreparedContents(*replacement);
            if (invalidation == 7)
            {
                MWWorld::ManualRef actor(fixture.mWorld.getStore(), ESM::RefId(ESM::FormId{0x940, 0}));
                owner->setPtr(actor.getPtr());
                EXPECT_FALSE(removal->isValid()); EXPECT_FALSE(removal->commit());
                continue;
            }
            EXPECT_FALSE(removal->isValid()); EXPECT_FALSE(removal->commit()); EXPECT_FALSE(removal->notify());
        }
        auto owner = MWWorld::OblivionProfileServices::stageActorInventory(sources);
        const auto held = *owner->getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
        { auto cancelled = owner->prepareItemRemoval(base, 2); EXPECT_TRUE(cancelled->isValid()); }
        // Native restore splits a nonstackable equipped weapon out of its
        // three-item source stack. Cancellation preserves both live stacks.
        EXPECT_EQ(held.getCellRef().getCount(), 1);
        EXPECT_EQ(owner->count(base), 3);
        auto partial = owner->prepareItemRemoval(base, 2);
        EXPECT_FALSE(partial->depletesEquipment()); ASSERT_TRUE(partial->commit());
        EXPECT_EQ(held.getCellRef().getCount(), 1);
        EXPECT_EQ(*owner->getSlot(MWWorld::InventoryStore::Slot_CarriedRight), held);
        EXPECT_FALSE(partial->commit());
    }

    TEST(OblivionWorldTest, NativeRemovalWorldClearDoesNotCaptureRetiredActor)
    {
        NativeWorldFixture fixture;
        auto& world = fixture.mWorld;
        MWClass::Npc::registerSelf(); world.setupPlayer();
        const auto key = addEquipmentWeapon(fixture);
        const auto player = world.getPlayerPtr();
        ESM4::RuntimeInventoryItem item; item.mBase = key; item.mCount = 2; item.mCondition = 40;
        installEquipmentInventory(fixture, player, {item});
        struct Listener : MWWorld::ContainerStoreListener
        {
            MWWorld::World* mWorld; int mCalls = 0;
            void itemRemoved(const MWWorld::ConstPtr&, int) override { ++mCalls; mWorld->clear(); }
        } listener;
        listener.mWorld = &world;
        player.getClass().getInventoryStore(player).setContListener(&listener);
        EXPECT_EQ(world.oblivionChangePlayerInventory(key, -2), 2);
        EXPECT_EQ(listener.mCalls, 1);
        EXPECT_TRUE(world.captureOblivionActorInventory(world.getPlayerPtr()).empty());
    }

    TEST(OblivionWorldTest, PreparedNativeRemovalPreservesSignedStackAndWrappedCountRules)
    {
        NativeWorldFixture fixture;
        const auto key = addEquipmentWeapon(fixture);
        ESM4::RuntimeInventoryItem item; item.mBase = key; item.mCount = 7; item.mCondition = 40;
        const auto sources = MWWorld::OblivionProfileServices::prepareActorInventory(
            fixture.mWorld.getStore(), ESM::FormKeyResolver({"headless.esm"}), {item});
        const auto base = sources.front().mReference.getPtr().getCellRef().getRefId();
        auto owner = MWWorld::OblivionProfileServices::stageActorInventory(sources);
        const auto held = *owner->begin(); held.getCellRef().setCount(-7);
        auto negative = owner->prepareItemRemoval(base, 3);
        EXPECT_EQ(negative->getCount(), 3); ASSERT_TRUE(negative->commit());
        EXPECT_EQ(held.getCellRef().getCount(false), -4);
        held.getCellRef().setCount(1);
        auto second = item; second.mCondition = 50; second.mCount = std::numeric_limits<int>::max();
        const auto projected = MWWorld::OblivionProfileServices::prepareActorInventory(
            fixture.mWorld.getStore(), ESM::FormKeyResolver({"headless.esm"}), {second});
        auto addition = owner->prepareItemAddition(projected.front().mReference.getPtr(), second.mCount);
        ASSERT_FALSE(addition->commit().isEmpty());
        auto wrapped = owner->prepareItemRemoval(base, 4);
        EXPECT_EQ(wrapped->getCount(), 0); ASSERT_TRUE(wrapped->commit());
        EXPECT_EQ(held.getCellRef().getCount(), 1);
        held.getCellRef().setCount(std::numeric_limits<int>::max());
        auto wrappedMagnitude = owner->prepareItemRemoval(base, 4);
        EXPECT_EQ(wrappedMagnitude->getCount(), 2);
        ASSERT_TRUE(wrappedMagnitude->commit());
        EXPECT_EQ(held.getCellRef().getCount(), std::numeric_limits<int>::max() - 2);
    }

TEST(OblivionWorldTest, NativeRemovalPartialNpcAmmunitionPreservesInstanceUntilDepletion)
{
    NativeWorldFixture fixture;
    auto& world = fixture.mWorld;
    const auto actor = installPreparedDebitAmmunition(fixture, 3);
    world.getWorldModel().registerPtr(actor);
    auto& inventory = actor.getClass().getInventoryStore(actor);
    const auto ammunition = *inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition);
    const auto key = ESM::FormKey::content("headless.esm", 0x941);
    struct Listener : MWWorld::InventoryStoreListener, MWWorld::ContainerStoreListener
    {
        MWWorld::InventoryStore* mInventory;
        int mEquipment = 0, mCalls = 0, mRemoved = 0;
        void equipmentChanged() override
        {
            ++mEquipment;
            EXPECT_EQ(mInventory->getSlot(MWWorld::InventoryStore::Slot_Ammunition), mInventory->end());
        }
        void itemRemoved(const MWWorld::ConstPtr&, int count) override
        { ++mCalls; mRemoved += count; }
    } listener;
    listener.mInventory = &inventory;
    inventory.setInvListener(&listener); inventory.setContListener(&listener);
    EXPECT_EQ(world.oblivionRemoveActorItem(actor, key, 1), 1);
    EXPECT_EQ(ammunition.getCellRef().getCount(), 2);
    EXPECT_EQ(*inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition), ammunition);
    EXPECT_EQ(listener.mEquipment, 0); EXPECT_EQ(listener.mCalls, 1); EXPECT_EQ(listener.mRemoved, 1);
    EXPECT_EQ(world.oblivionRemoveActorItem(actor, key, 9), 2);
    EXPECT_EQ(ammunition.getCellRef().getCount(), 0);
    EXPECT_EQ(inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition), inventory.end());
    EXPECT_EQ(listener.mEquipment, 1); EXPECT_EQ(listener.mCalls, 2); EXPECT_EQ(listener.mRemoved, 3);
    EXPECT_EQ(world.oblivionRemoveActorItem(actor, key, 1), 0);
    EXPECT_EQ(listener.mCalls, 2);
    inventory.setInvListener(nullptr); inventory.setContListener(nullptr);
}

TEST(OblivionWorldTest, NativePickupPublishesSourceDebitAndExactMetadataBeforeThrowingObserver)
{
    NativeWorldFixture fixture;
    auto& world = fixture.mWorld;
    const auto source = installNativeLooseItemCapture(fixture);
    MWClass::Weapon::registerSelf();
    ESM::Weapon projected; projected.blank();
    projected.mId = ESM::RefId(ESM::FormId{0x940, 0});
    projected.mData.mType = ESM::Weapon::MarksmanBow;
    projected.mData.mHealth = 100;
    world.getStore().insertStatic(projected);
    auto& ref = source.getCellRef();
    ref.setCount(2);
    ref.setNativeItemCondition(43.125f);
    ref.setEnchantmentCharge(7.25f);
    ref.setOwner(ESM::RefId(ESM::FormId{0x800, 0}));
    ref.setNativeOwnershipRank(-1);
    const auto cache = world.captureOblivionRuntimeState();
    readNativeSnapshot(fixture, cache);
    auto player = world.getPlayerPtr();
    auto& inventory = player.getClass().getInventoryStore(player);
    struct Listener : MWWorld::ContainerStoreListener
    {
        MWWorld::Ptr mSource;
        int mCalls = 0;
        void itemAdded(const MWWorld::ConstPtr& item, int count) override
        {
            ++mCalls;
            EXPECT_EQ(mSource.getCellRef().getCount(), 0);
            EXPECT_EQ(count, 2);
            EXPECT_EQ(item.getCellRef().getNativeItemCondition(), 43.125f);
            EXPECT_EQ(item.getCellRef().getEnchantmentCharge(), 7.25f);
            EXPECT_EQ(item.getCellRef().getOwner(), ESM::RefId(ESM::FormId{0x800, 0}));
            EXPECT_EQ(item.getCellRef().getNativeOwnershipRank(), -1);
            throw std::runtime_error("pickup observer boundary");
        }
    } listener;
    listener.mSource = source;
    inventory.setContListener(&listener);
    EXPECT_THROW(world.interactWithOblivionReference(source, MWWorld::OblivionInteractionKind::Take),
        std::runtime_error);
    EXPECT_EQ(listener.mCalls, 1);
    EXPECT_EQ(ref.getCount(), 0);
    EXPECT_EQ(inventory.count(ESM::RefId(ESM::FormId{0x940, 0})), 2);
    inventory.setContListener(nullptr);
}

namespace
{
    struct NativePickupFixture : NativeWorldFixture
    {
        MWWorld::Ptr mSource = installNativeLooseItemCapture(*this);
        NativePickupFixture()
        {
            MWClass::Weapon::registerSelf();
            ESM::Weapon projected; projected.blank();
            projected.mId = ESM::RefId(ESM::FormId{0x940, 0});
            projected.mData.mType = ESM::Weapon::MarksmanBow;
            projected.mData.mHealth = 100;
            mWorld.getStore().insertStatic(projected);
            mSource.getCellRef().setCount(2);
            mSource.getCellRef().setNativeItemCondition(43.125f);
            mSource.getCellRef().setEnchantmentCharge(7.25f);
            readNativeSnapshot(*this, mWorld.captureOblivionRuntimeState());
        }
        struct Listener : MWWorld::ContainerStoreListener
        {
            std::function<void(const MWWorld::ConstPtr&, int)> mCallback;
            int mCalls = 0;
            void itemAdded(const MWWorld::ConstPtr& ptr, int count) override
            {
                ++mCalls;
                mCallback(ptr, count);
            }
        };
    };
}

TEST(OblivionWorldTest, NativePickupRecursiveObserverCannotRepeatSourceTransfer)
{
    NativePickupFixture fixture;
    auto& world = fixture.mWorld;
    auto player = world.getPlayerPtr();
    auto& inventory = player.getClass().getInventoryStore(player);
    NativePickupFixture::Listener listener;
    listener.mCallback = [&](const MWWorld::ConstPtr&, int count) {
        EXPECT_EQ(count, 2);
        ASSERT_EQ(fixture.mSource.getCellRef().getCount(), 0);
        world.interactWithOblivionReference(fixture.mSource, MWWorld::OblivionInteractionKind::Take);
        EXPECT_EQ(inventory.count(ESM::RefId(ESM::FormId{0x940, 0})), 2);
    };
    inventory.setContListener(&listener);
    EXPECT_NO_THROW(world.interactWithOblivionReference(fixture.mSource, MWWorld::OblivionInteractionKind::Take));
    EXPECT_EQ(listener.mCalls, 1);
    world.interactWithOblivionReference(fixture.mSource, MWWorld::OblivionInteractionKind::Take);
    EXPECT_EQ(listener.mCalls, 1);
    EXPECT_EQ(inventory.count(ESM::RefId(ESM::FormId{0x940, 0})), 2);
    inventory.setContListener(nullptr);
}

TEST(OblivionWorldTest, NativePickupObserverCanClearWorldAfterCommittedTransfer)
{
    NativePickupFixture fixture;
    auto& world = fixture.mWorld;
    auto player = world.getPlayerPtr();
    auto& inventory = player.getClass().getInventoryStore(player);
    NativePickupFixture::Listener listener;
    listener.mCallback = [&](const MWWorld::ConstPtr& item, int count) {
        EXPECT_EQ(count, 2);
        EXPECT_EQ(fixture.mSource.getCellRef().getCount(), 0);
        EXPECT_EQ(item.getCellRef().getNativeItemCondition(), 43.125f);
        EXPECT_EQ(item.getCellRef().getEnchantmentCharge(), 7.25f);
        world.clear();
    };
    inventory.setContListener(&listener);
    EXPECT_NO_THROW(world.interactWithOblivionReference(fixture.mSource, MWWorld::OblivionInteractionKind::Take));
    EXPECT_EQ(listener.mCalls, 1);
    // World.clear retires the source; do not dereference it or the old store.
}

TEST(OblivionWorldTest, NativePickupCapturesObserverClearedInventoryBeforePropagatingFailure)
{
    NativePickupFixture fixture;
    auto& world = fixture.mWorld;
    auto player = world.getPlayerPtr();
    auto& inventory = player.getClass().getInventoryStore(player);
    NativePickupFixture::Listener listener;
    listener.mCallback = [&](const MWWorld::ConstPtr&, int) {
        EXPECT_EQ(fixture.mSource.getCellRef().getCount(), 0);
        inventory.clear();
        throw std::runtime_error("pickup cleared inventory");
    };
    inventory.setContListener(&listener);
    try
    {
        world.interactWithOblivionReference(fixture.mSource, MWWorld::OblivionInteractionKind::Take);
        FAIL() << "observer failure was swallowed";
    }
    catch (const std::runtime_error& error) { EXPECT_STREQ(error.what(), "pickup cleared inventory"); }
    EXPECT_EQ(listener.mCalls, 1);
    EXPECT_EQ(fixture.mSource.getCellRef().getCount(), 0);
    EXPECT_TRUE(world.captureOblivionActorInventory(world.getPlayerPtr()).empty());
    EXPECT_FALSE(world.oblivionSetPlayerHotkey(ESM::RefId(ESM::FormId{0x940, 0}), 1));
    inventory.setContListener(nullptr);
}

TEST(OblivionWorldTest, NativePickupInvalidLiveMetadataLeavesSourceAndInventoryUnchanged)
{
    for (int field = 0; field != 5; ++field)
    {
        NativePickupFixture fixture;
        auto& world = fixture.mWorld;
        auto player = world.getPlayerPtr();
        auto& inventory = player.getClass().getInventoryStore(player);
        auto& ref = fixture.mSource.getCellRef();
        if (field == 0) ref.setNativeOwnershipGlobal(ESM::RefId(ESM::FormId{0xdead, 0}));
        if (field == 1) ref.setNativeOwnershipGlobal(ESM::RefId(ESM::FormId{0x940, 0}));
        auto* native = const_cast<ESM4::Reference*>(ref.getNativeReference());
        ASSERT_NE(native, nullptr);
        if (field == 2) native->mOwner = ESM::FormId{1, 99};
        if (field == 3) native->mFormKey = {};
        if (field == 4) native->mBaseObj = ESM::FormId{0xdead, 0};
        NativePickupFixture::Listener listener;
        listener.mCallback = [](const MWWorld::ConstPtr&, int) { ADD_FAILURE() << "invalid pickup published"; };
        inventory.setContListener(&listener);
        EXPECT_THROW(world.interactWithOblivionReference(fixture.mSource, MWWorld::OblivionInteractionKind::Take),
            std::exception);
        EXPECT_EQ(listener.mCalls, 0);
        EXPECT_EQ(ref.getCount(), 2);
        EXPECT_EQ(inventory.count(ESM::RefId(ESM::FormId{0x940, 0})), 0);
        EXPECT_EQ(world.getWorldModel().getPtr(ref.getRefNum()), fixture.mSource);
        inventory.setContListener(nullptr);
    }
}

TEST(OblivionWorldTest, NativePickupPreservesRawAbsentZeroFractionalAndOwnershipGlobalExtras)
{
    const std::array<std::optional<float>, 6> conditions{std::nullopt, 0.f, -0.f, 43.125f, 100.f,
        std::numeric_limits<float>::max()};
    const std::array<float, 6> charges{-1.f, 0.f, -0.f, 7.25f, 20.f, std::numeric_limits<float>::max()};
    const std::array<std::int32_t, 6> ranks{std::numeric_limits<std::int32_t>::min(), -2, -1, 0, 1,
        std::numeric_limits<std::int32_t>::max()};
    for (std::size_t i = 0; i != conditions.size(); ++i)
    {
        SCOPED_TRACE(i);
        NativePickupFixture fixture;
        auto& world = fixture.mWorld;
        auto& ref = fixture.mSource.getCellRef();
        if (conditions[i]) ref.setNativeItemCondition(*conditions[i]);
        else ref.resetNativeItemCondition();
        ref.setEnchantmentCharge(charges[i]);
        ref.setNativeOwnershipRank(ranks[i]);
        ref.setOwner(ESM::RefId(ESM::FormId{0x800, 0}));
        ESM4::GlobalVariable permission{}; permission.mId = {0x970, 0}; permission.mType = 'f'; permission.mValue = 1;
        const auto permissionKey = ESM::FormKey::content("headless.esm", 0x970);
        world.getStore().getWritable<ESM4::GlobalVariable>().insertStatic(permission, permissionKey);
        ref.setNativeOwnershipGlobal(ESM::RefId(permission.mId));
        auto player = world.getPlayerPtr();
        auto& inventory = player.getClass().getInventoryStore(player);
        NativePickupFixture::Listener listener;
        listener.mCallback = [&](const MWWorld::ConstPtr& item, int count) {
            EXPECT_EQ(count, 2);
            EXPECT_EQ(ref.getCount(), 0);
            EXPECT_EQ(item.getCellRef().getNativeItemCondition().has_value(), conditions[i].has_value());
            if (conditions[i])
            {
                EXPECT_EQ(std::bit_cast<std::uint32_t>(*item.getCellRef().getNativeItemCondition()),
                    std::bit_cast<std::uint32_t>(*conditions[i]));
            }
            EXPECT_EQ(std::bit_cast<std::uint32_t>(item.getCellRef().getEnchantmentCharge()),
                std::bit_cast<std::uint32_t>(charges[i]));
            EXPECT_EQ(item.getCellRef().getNativeOwnershipRank(), ranks[i]);
            EXPECT_EQ(item.getCellRef().getNativeOwnershipGlobal(), ESM::RefId(permission.mId));
            EXPECT_EQ(item.getCellRef().getOwner(), ESM::RefId(ESM::FormId{0x800, 0}));
        };
        inventory.setContListener(&listener);
        ASSERT_NO_THROW(world.interactWithOblivionReference(fixture.mSource, MWWorld::OblivionInteractionKind::Take));
        EXPECT_EQ(listener.mCalls, 1);
        EXPECT_EQ(ref.getCount(), 0);
        const auto captured = world.captureOblivionActorInventory(player);
        ASSERT_EQ(captured.size(), 1u);
        EXPECT_EQ(captured.front().mOwnershipGlobal, permissionKey);
        EXPECT_EQ(captured.front().mOwnershipRank, ranks[i]);
        inventory.setContListener(nullptr);
    }
}

TEST(OblivionWorldTest, NativePickupSavedSourceDeletionAndInventoryExtrasRestoreIntoFreshWorld)
{
    std::vector<std::uint8_t> bytes;
    const auto permissionKey = ESM::FormKey::content("headless.esm", 0x970);
    const auto installPermission = [&](NativePickupFixture& fixture) {
        ESM4::GlobalVariable permission{}; permission.mId = {0x970, 0};
        permission.mType = 'f'; permission.mValue = 1;
        fixture.mWorld.getStore().getWritable<ESM4::GlobalVariable>().insertStatic(permission, permissionKey);
    };
    {
        NativePickupFixture fixture;
        installPermission(fixture);
        auto& ref = fixture.mSource.getCellRef();
        ref.setOwner(ESM::RefId(ESM::FormId{0x800, 0}));
        ref.setNativeOwnershipRank(-2);
        ref.setNativeOwnershipGlobal(ESM::RefId(ESM::FormId{0x970, 0}));
        ASSERT_NO_THROW(fixture.mWorld.interactWithOblivionReference(fixture.mSource, MWWorld::OblivionInteractionKind::Take));
        const auto saved = fixture.mWorld.captureOblivionRuntimeState();
        ASSERT_EQ(saved.mReferences.size(), 1u);
        EXPECT_TRUE(saved.mReferences[0].mDeleted);
        EXPECT_EQ(std::get<std::int64_t>(saved.mReferences[0].mCustomState.at("count")), 0);
        EXPECT_TRUE(std::get<bool>(saved.mReferences[0].mCustomState.at("taken")));
        ASSERT_EQ(saved.mPlayer.mInventory.size(), 1u);
        EXPECT_EQ(saved.mPlayer.mInventory[0].mCount, 2);
        EXPECT_EQ(saved.mPlayer.mInventory[0].mCondition, 43.125);
        EXPECT_EQ(saved.mPlayer.mInventory[0].mCharge, 7.25f);
        EXPECT_EQ(saved.mPlayer.mInventory[0].mOwnershipGlobal, permissionKey);
        bytes = saved.serializeBinary();
    }
    {
        NativePickupFixture fresh;
        installPermission(fresh);
        const auto saved = ESM4::RuntimeState::deserializeBinary(bytes);
        readNativeSnapshot(fresh, saved);
        ASSERT_NO_THROW(fresh.mWorld.applyOblivionRuntimeState());
        EXPECT_EQ(fresh.mSource.getCellRef().getCount(), 0);
        const auto inventory = fresh.mWorld.captureOblivionActorInventory(fresh.mWorld.getPlayerPtr());
        ASSERT_EQ(inventory.size(), 1u);
        EXPECT_EQ(inventory[0].mCount, 2);
        EXPECT_EQ(inventory[0].mCondition, 43.125);
        EXPECT_EQ(inventory[0].mCharge, 7.25f);
        EXPECT_EQ(inventory[0].mOwner, ESM::FormKey::content("headless.esm", 0x800));
        EXPECT_EQ(inventory[0].mOwnershipRank, -2);
        EXPECT_EQ(inventory[0].mOwnershipGlobal, permissionKey);
        EXPECT_NO_THROW(fresh.mWorld.interactWithOblivionReference(fresh.mSource, MWWorld::OblivionInteractionKind::Take));
        EXPECT_EQ(fresh.mWorld.captureOblivionActorInventory(fresh.mWorld.getPlayerPtr())[0].mCount, 2);
    }
}

TEST(OblivionWorldTest, NativePickupLuaPreparationPreservesInactiveEventAndRejectsStaleQueue)
{
    TestingOpenMW::VFSTestFile file{R"(local probe = require('pickup_probe')
return {engineHandlers = {onInactive = function() probe.inactive() end}})"};
    auto vfs = TestingOpenMW::createTestVFS({{VFS::Path::NormalizedView("pickup.lua"), &file}});
    ESM::LuaScriptCfg script{};
    script.mScriptPath = VFS::Path::Normalized("pickup.lua");
    script.mFlags = ESM::LuaScriptCfg::sCustom;
    ESM::LuaScriptsCfg config;
    config.mScripts.push_back(script);
    LuaUtil::ScriptsConfiguration configuration;
    configuration.init(std::move(config), false);
    LuaUtil::LuaState state(vfs.get(), &configuration);
    NativePickupFixture fixture;
    auto scripts = std::make_shared<MWLua::LocalScripts>(&state,
        MWLua::LObject(fixture.mSource.getCellRef().getRefNum()));
    int inactiveCalls = 0;
    state.protectedCall([&](LuaUtil::LuaView& view) {
        sol::table probe = view.newTable();
        probe["inactive"] = [&] {
            ++inactiveCalls;
            EXPECT_EQ(fixture.mSource.getCellRef().getCount(), 0);
        };
        scripts->addPackage("pickup_probe", LuaUtil::makeReadOnly(probe));
    });
    ASSERT_TRUE(scripts->addCustomScript(0));
    fixture.mSource.getRefData().setLuaScripts(std::shared_ptr<MWLua::LocalScripts>(scripts));
    scripts->setActive(true, false);
    auto& lua = *fixture.mLuaManager;
    lua.objectAddedToScene(fixture.mSource);
    ASSERT_NO_THROW(lua.update());
    ASSERT_TRUE(scripts->isActive());
    {
        auto cancelled = lua.prepareSceneRemoval(fixture.mSource);
        ASSERT_TRUE(cancelled->isValid());
    }
    EXPECT_TRUE(scripts->isActive());
    EXPECT_EQ(fixture.mSource.getCellRef().getCount(), 2);
    {
        auto stale = lua.prepareSceneRemoval(fixture.mSource);
        lua.objectTeleported(fixture.mSource);
        EXPECT_FALSE(stale->isValid());
        EXPECT_FALSE(stale->commit());
    }
    EXPECT_TRUE(scripts->isActive());
    ASSERT_NO_THROW(lua.update());
    auto removal = lua.prepareSceneRemoval(fixture.mSource);
    auto competing = lua.prepareSceneRemoval(fixture.mSource);
    fixture.mSource.getCellRef().setCount(0);
    ASSERT_TRUE(removal->commit());
    EXPECT_FALSE(removal->commit());
    // Committing enqueues the inactive event, invalidating another plan that
    // reserved against the same real manager queue. No synchronous handler.
    EXPECT_FALSE(competing->isValid());
    EXPECT_FALSE(competing->commit());
    EXPECT_TRUE(scripts->isActive());

    // This fixture does not initialize a Lua game session. Verify delivery and
    // consumption with the same engine dispatcher and live script container.
    MWLua::GlobalScripts globals(&state);
    MWLua::EngineEvents events(globals);
    events.addToQueue(MWLua::EngineEvents::OnInactive{fixture.mSource.getCellRef().getRefNum()});
    ASSERT_NO_THROW(events.callEngineHandlers());
    EXPECT_FALSE(scripts->isActive());
    EXPECT_EQ(inactiveCalls, 1);
    scripts->setActive(true, false);
    ASSERT_NO_THROW(events.callEngineHandlers());
    EXPECT_TRUE(scripts->isActive()); // A consumed inactive event cannot replay.
    EXPECT_EQ(inactiveCalls, 1);
}

TEST(OblivionWorldTest, NativeRestoreCharacterBindingsRejectBeforeWorldPublication)
{
    NativePickupFixture fixture;
    auto& world = fixture.mWorld;
    ASSERT_NO_THROW(world.interactWithOblivionReference(fixture.mSource, MWWorld::OblivionInteractionKind::Take));
    const auto saved = world.captureOblivionRuntimeState();
    const auto player = world.getPlayerPtr();
    const auto* record = player.get<ESM::NPC>()->mBase;
    const auto name = record->mName;
    const auto race = record->mRace;
    const auto characterClass = record->mClass;
    const auto male = record->isMale();
    const auto birthSign = world.getPlayer().getBirthSign();
    const auto inventory = world.captureOblivionActorInventory(player);
    const auto lastGenerated = world.getWorldModel().getLastGeneratedRefNum();
    const auto clock = world.getTimeStamp();
    auto pending = world.prepareOblivionDynamicReferenceKey();
    for (int invalid : {0, 1, 2, 3})
    {
        SCOPED_TRACE(invalid);
        auto candidate = saved;
        candidate.mClock.mHour = 9;
        candidate.mNextDynamicSerial += 20;
        candidate.mPlayer.mName = "Rejected character";
        candidate.mPlayer.mFemale = !candidate.mPlayer.mFemale;
        candidate.mPlayer.mInventory.clear();
        const auto missing = ESM::FormKey::content("headless.esm", 0x9ff);
        if (invalid == 0) candidate.mPlayer.mRace = missing;
        if (invalid == 1) candidate.mPlayer.mClass = missing;
        if (invalid == 2) candidate.mPlayer.mBirthSign = missing;
        if (invalid == 3) candidate.mPlayer.mBirthSign = candidate.mReferences.front().mBase;
        ASSERT_NO_THROW(candidate.validate());
        readNativeSnapshot(fixture, candidate);
        EXPECT_THROW(world.applyOblivionRuntimeState(), std::runtime_error);
        EXPECT_EQ(world.getTimeStamp(), clock);
        EXPECT_EQ(player.get<ESM::NPC>()->mBase, record);
        EXPECT_EQ(record->mName, name);
        EXPECT_EQ(record->mRace, race);
        EXPECT_EQ(record->mClass, characterClass);
        EXPECT_EQ(record->isMale(), male);
        EXPECT_EQ(world.getPlayer().getBirthSign(), birthSign);
        EXPECT_EQ(world.captureOblivionActorInventory(player), inventory);
        EXPECT_EQ(world.getWorldModel().getLastGeneratedRefNum(), lastGenerated);
        EXPECT_TRUE(pending->isValid());
    }
    readNativeSnapshot(fixture, saved);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    EXPECT_EQ(world.captureOblivionActorInventory(player), inventory);
    EXPECT_FALSE(pending->isValid());
}

TEST(OblivionWorldTest, NativeRestoreInventoryPublicationPreservesExtrasAndDoesNotReplayListeners)
{
    NativePickupFixture fixture;
    auto& world = fixture.mWorld;
    ASSERT_NO_THROW(world.interactWithOblivionReference(fixture.mSource, MWWorld::OblivionInteractionKind::Take));
    const auto saved = world.captureOblivionRuntimeState();
    const auto player = world.getPlayerPtr();
    auto& inventory = player.getClass().getInventoryStore(player);
    const auto old = *inventory.begin();
    const auto oldId = old.getCellRef().getRefNum();
    ASSERT_EQ(world.getWorldModel().getPtr(oldId), old);
    NativePickupFixture::Listener listener;
    listener.mCallback = [](const MWWorld::ConstPtr&, int) { ADD_FAILURE() << "restore replayed item acquisition"; };
    inventory.setContListener(&listener);
    for (int repeat = 0; repeat < 2; ++repeat)
    {
        SCOPED_TRACE(repeat);
        readNativeSnapshot(fixture, saved);
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        EXPECT_EQ(listener.mCalls, 0);
        EXPECT_EQ(world.captureOblivionActorInventory(player), saved.mPlayer.mInventory);
        EXPECT_TRUE(world.getWorldModel().getPtr(oldId).isEmpty());
        const auto item = *inventory.begin();
        EXPECT_EQ(item.getContainerStore(), &inventory);
        const auto registered = world.getWorldModel().getPtr(item.getCellRef().getRefNum());
        EXPECT_EQ(registered, item);
        EXPECT_EQ(registered.getContainerStore(), &inventory);
    }
    inventory.setContListener(nullptr);
}

TEST(OblivionWorldTest, NativeRestoreActorViewPreparationCancelsAndCommitsOnce)
{
    NativeWorldFixture fixture;
    auto& world = fixture.mWorld;
    const auto actor = addNativeNpc(fixture, 0x900);
    auto& service = *world.getOblivionCombatService();
    ASSERT_TRUE(world.initializeOblivionNonPlayerActor(actor, ESM4::ActorValueProcess::Active));
    const auto initial = captureNativeActorState(fixture, actor);
    const auto health = actor.getClass().getCreatureStats(actor).getHealth().getCurrent();
    auto changed = initial;
    changed.mNativeActorValues.front().mValues[8].mModifiers[2] = -9;
    const std::array residents{actor};
    {
        MWMechanics::OblivionCombatService replacement;
        replacement.restore(ESM4::RuntimeState::deserializeBinary(changed.serializeBinary()), world.getStore());
        auto plan = service.prepareRestoredActorState(std::move(replacement), residents);
        EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), initial.serializeBinary());
        EXPECT_EQ(actor.getClass().getCreatureStats(actor).getHealth().getCurrent(), health);
    }
    EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), initial.serializeBinary());
    MWMechanics::OblivionCombatService replacement;
    replacement.restore(changed, world.getStore());
    auto plan = service.prepareRestoredActorState(std::move(replacement), residents);
    auto moved = std::move(plan);
    EXPECT_FALSE(plan.commit());
    EXPECT_TRUE(moved.commit());
    EXPECT_FALSE(moved.commit());
    EXPECT_EQ(captureNativeActorState(fixture, actor).serializeBinary(), changed.serializeBinary());
    EXPECT_EQ(actor.getClass().getCreatureStats(actor).getHealth().getCurrent(), health - 9);
    EXPECT_THROW(service.prepareRestoredActorState(std::move(service), residents), std::invalid_argument);
}

TEST(OblivionWorldTest, NativeRestoreResidentProjectionRejectsBeforeInventoryClockAndIdentityChanges)
{
    NativePickupFixture fixture;
    auto& world = fixture.mWorld;
    auto npc = addNativeNpc(fixture, 0x900);
    npc = npc.getCell()->moveTo(npc, world.getPlayerPtr().getCell());
    ASSERT_TRUE(world.initializeOblivionNonPlayerActor(npc, ESM4::ActorValueProcess::Active));
    ASSERT_NO_THROW(world.interactWithOblivionReference(fixture.mSource, MWWorld::OblivionInteractionKind::Take));
    const auto saved = world.captureOblivionRuntimeState();
    const auto beforeInventory = world.captureOblivionActorInventory(world.getPlayerPtr());
    const auto beforeClock = world.getTimeStamp();
    const auto beforeHealth = npc.getClass().getCreatureStats(npc).getHealth().getCurrent();
    const auto beforeRegistry = world.getWorldModel().getLastGeneratedRefNum();
    auto pending = world.prepareOblivionDynamicReferenceKey();
    auto candidate = saved;
    candidate.mClock.mHour = 9;
    candidate.mNextDynamicSerial += 7;
    candidate.mPlayer.mInventory.clear();
    auto values = std::find_if(candidate.mNativeActorValues.begin(), candidate.mNativeActorValues.end(),
        [&](const auto& actor) { return actor.mActor == npc.getCellRef().getFormKey(); });
    ASSERT_NE(values, candidate.mNativeActorValues.end());
    values->mValues[8].mModifiers[2] = -5;
    values->mValues[33].mModifiers[1] = 1e32f; // Valid float authority, invalid shared integer AI view.
    ASSERT_NO_THROW(candidate.validate());
    readNativeSnapshot(fixture, candidate);
    EXPECT_THROW(world.applyOblivionRuntimeState(), std::invalid_argument);
    EXPECT_EQ(world.getTimeStamp(), beforeClock);
    EXPECT_EQ(world.captureOblivionActorInventory(world.getPlayerPtr()), beforeInventory);
    EXPECT_EQ(npc.getClass().getCreatureStats(npc).getHealth().getCurrent(), beforeHealth);
    EXPECT_EQ(world.getWorldModel().getLastGeneratedRefNum(), beforeRegistry);
    EXPECT_TRUE(pending->isValid());
    ESM4::RuntimeState authority = saved;
    world.getOblivionCombatService()->capture(authority);
    EXPECT_EQ(authority.serializeBinary(), saved.serializeBinary());
    readNativeSnapshot(fixture, saved);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    EXPECT_FALSE(pending->isValid());
}

TEST(OblivionWorldTest, PreparedLocalFogInstallsOnceWithoutDecodingWireImagesAndClearsWithModel)
{
    NativeWorldFixture fixture;
    auto& world = fixture.mWorld;
    ESM4::Cell cell{}; cell.mId = ESM::RefId(ESM::FormId{0xabc, 0});
    cell.mFormKey = ESM::FormKey::content("headless.esm", 0xabc);
    cell.mCellFlags = ESM4::CELL_Interior; cell.mEditorId = "PreparedFogCell";
    world.getStore().getWritable<ESM4::Cell>().insertStatic(cell, cell.mFormKey);
    osg::ref_ptr<osg::Image> image = new osg::Image;
    image->allocateImage(32, 32, 1, GL_RGBA, GL_UNSIGNED_BYTE);
    for (int y = 0; y != 32; ++y) for (int x = 0; x != 32; ++x)
    {
        auto* pixel = image->data(x, y); pixel[0] = pixel[1] = pixel[2] = 0; pixel[3] = y;
    }
    auto* codec = osgDB::Registry::instance()->getReaderWriterForExtension("png");
    ASSERT_NE(codec, nullptr);
    std::ostringstream encoded; ASSERT_TRUE(codec->writeImage(*image, encoded).success());
    const auto png = encoded.str();
    ESM::FogState fog{}; fog.mBounds = {0, 0, 512, 512}; fog.mCenterX = fog.mCenterY = 256;
    fog.mFogTextures.push_back({0, 0, {png.begin(), png.end()}});
    auto prepared = std::make_unique<ESM::FogState>(ESM::prepareFogState(fog, true));
    auto pinned = prepared->mFogTextures.at(0).mPreparedImage;
    ASSERT_TRUE(pinned); EXPECT_TRUE(ESM::isUsableFogImage(*pinned));
    EXPECT_EQ(prepared->mFogTextures.at(0).mImageData, fog.mFogTextures.at(0).mImageData);
    // Compare preparation with an independently decoded/flipped image.
    std::istringstream pngStream(png); auto raw = codec->readImage(pngStream);
    ASSERT_TRUE(raw.success()); raw.getImage()->flipVertical();
    EXPECT_EQ(std::memcmp(raw.getImage()->data(), pinned->data(), 4096), 0);
    std::map<ESM::RefId, std::unique_ptr<ESM::FogState>> states;
    states.emplace(cell.mId, std::move(prepared));
    world.getWorldModel().setPreparedFogStates(std::move(states));
    // The wire image is deliberately unreadable: the admitted image must supply restoration.
    fog.mFogTextures.at(0).mImageData = {'b','a','d'};
    std::stringstream stream; ESM::ESMWriter writer;
    writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion); writer.save(stream);
    writer.startRecord(ESM::REC_CSTA); writer.writeCellId(cell.mId);
    ESM::CellState state{}; state.mIsInterior = true; state.mHasFogOfWar = true; state.save(writer);
    fog.save(writer, true); writer.endRecord(ESM::REC_CSTA);
    const auto read = [&] {
        ESM::ESMReader reader; reader.open(std::make_unique<std::stringstream>(stream.str()), "prepared-fog");
        reader.getRecName(); reader.getRecHeader();
        EXPECT_TRUE(world.getWorldModel().readRecord(reader, ESM::REC_CSTA));
        EXPECT_FALSE(reader.hasMoreSubs());
    };
    read();
    auto* restored = world.getWorldModel().getCell(cell.mId).getFog(); ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->mFogTextures.at(0).mPreparedImage, pinned);
    EXPECT_EQ(restored->mFogTextures.at(0).mImageData, std::vector<char>(png.begin(), png.end()));
    read(); // The plan is consumed; ordinary rereading does not reuse its image.
    EXPECT_FALSE(world.getWorldModel().getCell(cell.mId).getFog()->mFogTextures.at(0).mPrepared);
    states.emplace(cell.mId, std::make_unique<ESM::FogState>(ESM::prepareFogState(
        ESM::FogState{}, true)));
    world.getWorldModel().setPreparedFogStates(std::move(states));
    world.getWorldModel().clear();
    read();
    EXPECT_FALSE(world.getWorldModel().getCell(cell.mId).getFog()->mFogTextures.at(0).mPrepared);
}

TEST(OblivionWorldTest, NativeRestorePreparesEmptyAuthorityFromEveryAcceptedRuntimeVersion)
{
    NativeWorldFixture fixture;
    auto& world = fixture.mWorld;
    MWClass::Npc::registerSelf();
    world.setupPlayer();
    ESM::Race race{}; race.blank(); race.mId = ESM::RefId(ESM::FormId{0x810, 0});
    world.getStore().getWritable<ESM::Race>().insertStatic(race);
    auto character = *world.getPlayerPtr().get<ESM::NPC>()->mBase;
    character.mRace = race.mId;
    auto metadata = world.getStore().preparePlayerRecord(character);
    world.getPlayer().set(metadata.commit());
    ESM4::Cell cell{}; cell.mId = ESM::RefId(ESM::FormId{1, 0});
    cell.mFormKey = ESM::FormKey::content("headless.esm", 1);
    cell.mCellFlags = ESM4::CELL_Interior; cell.mEditorId = "LegacyRestoreCell";
    world.getStore().getWritable<ESM4::Cell>().insertStatic(cell, cell.mFormKey);
    world.getPlayer().setCell(&world.getWorldModel().getCell(cell.mId));
    const auto current = world.captureOblivionRuntimeState();
    for (std::uint32_t version = 1; version <= ESM4::CurrentRuntimeStateVersion; ++version)
    {
        SCOPED_TRACE(version);
        ESM4::RuntimeState legacy;
        legacy.mVersion = version;
        legacy.mContent = current.mContent;
        legacy.mClock = current.mClock;
        legacy.mClock.mHour = 7;
        legacy.mPlayer = current.mPlayer;
        legacy.mPlayer.mInventory.clear();
        legacy.mPlayer.mActorValues["health.current"] = 67;
        if (version < 3)
        {
            legacy.mPlayer.mName.clear(); legacy.mPlayer.mRace = {}; legacy.mPlayer.mClass = {};
            legacy.mPlayer.mBirthSign = {}; legacy.mPlayer.mFemale = false;
            legacy.mPlayer.mCharacterGenerationFlags = 0;
        }
        ASSERT_NO_THROW(legacy.validate());
        readNativeSnapshot(fixture, legacy);
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        const auto captured = world.captureOblivionRuntimeState();
        EXPECT_EQ(captured.mVersion, ESM4::CurrentRuntimeStateVersion);
        EXPECT_EQ(captured.mClock.mHour, 7);
        EXPECT_EQ(captured.mPlayer.mActorValues.at("health.current"), 67);
        EXPECT_TRUE(captured.mNativeActorValues.empty());
        EXPECT_TRUE(captured.mNativeActorLife.empty());
        EXPECT_TRUE(captured.mPendingDeathEvents.empty());
        EXPECT_TRUE(captured.mPlayer.mInventory.empty());
        const auto decoded = ESM4::RuntimeState::deserializeBinary(captured.serializeBinary());
        readNativeSnapshot(fixture, decoded);
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), captured.serializeBinary());
    }
}

TEST(OblivionWorldTest, NativeRestoreRejectsInPlaceAuthorityDowngradeBeforeWorldChanges)
{
    NativePickupFixture fixture;
    auto& world = fixture.mWorld;
    const auto saved = world.captureOblivionRuntimeState();
    auto legacy = saved;
    legacy.mNativeActorValues.clear();
    legacy.mNativeActorLife.clear();
    legacy.mNativeActorBreath.clear();
    legacy.mClock.mHour = 9;
    legacy.mNextDynamicSerial += 10;
    ASSERT_NO_THROW(legacy.validate());
    const auto clock = world.getTimeStamp();
    const auto health = world.getPlayerPtr().getClass().getCreatureStats(world.getPlayerPtr()).getHealth();
    auto pending = world.prepareOblivionDynamicReferenceKey();
    readNativeSnapshot(fixture, legacy);
    EXPECT_THROW(world.applyOblivionRuntimeState(), std::invalid_argument);
    EXPECT_EQ(world.getTimeStamp(), clock);
    EXPECT_EQ(world.getPlayerPtr().getClass().getCreatureStats(world.getPlayerPtr()).getHealth(), health);
    EXPECT_TRUE(pending->isValid());
    ESM4::RuntimeState authority = saved;
    world.getOblivionCombatService()->capture(authority);
    EXPECT_EQ(authority.serializeBinary(), saved.serializeBinary());
}

namespace
{
    // Historical loads start with plain character data, rather than attempting
    // an unsupported in-place downgrade of an already published native Player.
    struct PopulatedMigrationFixture : NativeWorldFixture
    {
        MWWorld::Ptr mActor;
        explicit PopulatedMigrationFixture(bool activationScript = false)
            : NativeWorldFixture(false, false, activationScript)
        {
            MWClass::Npc::registerSelf();
            MWClass::Weapon::registerSelf();
            MWClass::ESM4Takeable<ESM4::Weapon>::registerSelf();
            mWorld.setupPlayer();
            ESM::Race race{};
            race.blank();
            race.mId = ESM::RefId(ESM::FormId{0x810, 0});
            mWorld.getStore().getWritable<ESM::Race>().insertStatic(race);
            auto character = *mWorld.getPlayerPtr().get<ESM::NPC>()->mBase;
            character.mRace = race.mId;
            auto metadata = mWorld.getStore().preparePlayerRecord(character);
            mWorld.getPlayer().set(metadata.commit());
            ESM4::Cell cell{};
            cell.mId = ESM::RefId(ESM::FormId{1, 0});
            cell.mFormKey = ESM::FormKey::content("headless.esm", 1);
            cell.mCellFlags = ESM4::CELL_Interior;
            cell.mEditorId = "PopulatedMigrationCell";
            mWorld.getStore().getWritable<ESM4::Cell>().insertStatic(cell, cell.mFormKey);
            auto& resident = mWorld.getWorldModel().getCell(cell.mId);
            mWorld.getPlayer().setCell(&resident);
            mActor = addNativeNpc(*this, 0x900);
            mActor = mActor.getCell()->moveTo(mActor, &resident);
            mWorld.getWorldModel().registerPtr(mActor);
            ESM4::Weapon weapon{};
            weapon.mId = {0x940, 0};
            weapon.mData.type = 5;
            weapon.mData.health = 100;
            weapon.mEnchantment = {0x950, 0};
            weapon.mEnchantmentPoints = 20;
            mWorld.getStore().getWritable<ESM4::Weapon>().insertStatic(
                weapon, ESM::FormKey::content("headless.esm", 0x940));
            ESM::Weapon projected;
            projected.blank();
            projected.mId = ESM::RefId(weapon.mId);
            projected.mData.mType = ESM::Weapon::MarksmanBow;
            projected.mData.mHealth = 100;
            mWorld.getStore().insertStatic(projected);
        }
    };
}

TEST(OblivionWorldTest, NativeRestoreMigratesPopulatedPlayerAndNpcInventoriesFromEveryVersion)
{
    for (std::uint32_t version = 1; version <= ESM4::CurrentRuntimeStateVersion; ++version)
    {
        SCOPED_TRACE(version);
        PopulatedMigrationFixture fixture;
        auto& world = fixture.mWorld;
        auto legacy = world.captureOblivionRuntimeState();
        legacy.mVersion = version;
        legacy.mClock.mHour = 7;
        legacy.mPlayer.mActorValues["health.current"] = 67;
        if (version < 3)
        {
            legacy.mPlayer.mName.clear();
            legacy.mPlayer.mRace = {};
            legacy.mPlayer.mClass = {};
            legacy.mPlayer.mBirthSign = {};
            legacy.mPlayer.mFemale = false;
            legacy.mPlayer.mCharacterGenerationFlags = 0;
        }
        ESM4::RuntimeInventoryItem item;
        item.mBase = ESM::FormKey::content("headless.esm", 0x940);
        item.mCount = version < 4 ? 3 : 1;
        if (version >= 4)
        {
            item.mCondition = version < 24 ? 43.f : 43.125f;
            item.mCharge = 7.25f;
            item.mEquippedSlots = ESM4::InventorySlotWeapon;
            item.mOwner = ESM::FormKey::content("headless.esm", 0x800);
        }
        legacy.mPlayer.mInventory = {item};
        ASSERT_EQ(legacy.mReferences.size(), 1u);
        legacy.mReferences.front().mInventory = {item};
        if (version >= 4)
        {
            item.mCount = 2;
            item.mCondition = 0.f;
            item.mCharge = 0.f;
            item.mEquippedSlots = 0;
            legacy.mPlayer.mInventory.push_back(item);
            legacy.mReferences.front().mInventory.push_back(item);
        }
        ASSERT_NO_THROW(legacy.validate());
        const auto decoded = ESM4::RuntimeState::deserializeBinary(legacy.serializeBinary());
        EXPECT_EQ(decoded.mNativeCrime, ESM4::CrimeStateContracts{});
        EXPECT_EQ(decoded.mPlayer.mInventory, legacy.mPlayer.mInventory);
        EXPECT_EQ(decoded.mReferences.front().mInventory, legacy.mReferences.front().mInventory);
        const auto beforeClock = world.getTimeStamp();
        const auto beforePlayer = world.captureOblivionActorInventory(world.getPlayerPtr());
        const auto beforeActor = world.captureOblivionActorInventory(fixture.mActor);
        auto pending = world.prepareOblivionDynamicReferenceKey();
        auto invalid = decoded;
        invalid.mNextDynamicSerial += 10;
        invalid.mReferences.front().mInventory.back().mBase = ESM::FormKey::content("headless.esm", 0xdead);
        readNativeSnapshot(fixture, invalid);
        EXPECT_THROW(world.applyOblivionRuntimeState(), std::runtime_error);
        EXPECT_EQ(world.getTimeStamp(), beforeClock);
        EXPECT_EQ(world.captureOblivionActorInventory(world.getPlayerPtr()), beforePlayer);
        EXPECT_EQ(world.captureOblivionActorInventory(fixture.mActor), beforeActor);
        EXPECT_TRUE(pending->isValid());
        readNativeSnapshot(fixture, decoded);
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        EXPECT_FALSE(pending->isValid());
        auto expectedPlayer = legacy.mPlayer.mInventory;
        auto expectedActor = legacy.mReferences.front().mInventory;
        if (version < 4)
        {
            // Quantity-only saves acquire winning full condition/charge. NPCs
            // also regain default gear; the Player chooses equipment explicitly.
            expectedPlayer.front().mCondition = 100.f;
            expectedPlayer.front().mCharge = 20.f;
            auto equipped = expectedPlayer.front();
            equipped.mCount = 1;
            equipped.mEquippedSlots = ESM4::InventorySlotWeapon;
            expectedActor = {expectedPlayer.front(), equipped};
            expectedActor.front().mCount = 2;
        }
        EXPECT_EQ(world.captureOblivionActorInventory(world.getPlayerPtr()), expectedPlayer);
        EXPECT_EQ(world.captureOblivionActorInventory(fixture.mActor), expectedActor);
        const auto migrated = world.captureOblivionRuntimeState();
        EXPECT_EQ(migrated.mVersion, ESM4::CurrentRuntimeStateVersion);
        EXPECT_EQ(migrated.mNativeCrime, ESM4::CrimeStateContracts{});
        EXPECT_FALSE(world.isPlayerInJail());
        EXPECT_EQ(migrated.mPlayer.mActorValues.at("health.current"), 67);
        EXPECT_EQ(migrated.mClock.mHour, 7);
        readNativeSnapshot(fixture, ESM4::RuntimeState::deserializeBinary(migrated.serializeBinary()));
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), migrated.serializeBinary());
    }
}

TEST(OblivionWorldTest, NativeRestoreMigratesPopulatedActorAuthorityFromEverySupportedVersion)
{
    for (std::uint32_t version = 9; version <= ESM4::CurrentRuntimeStateVersion; ++version)
    {
        SCOPED_TRACE(version);
        PopulatedMigrationFixture fixture;
        auto& world = fixture.mWorld;
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(fixture.mActor, ESM4::ActorValueProcess::Active));
        auto legacy = world.captureOblivionRuntimeState();
        legacy.mVersion = version;
        if (version < 29) legacy.mReferences.front().mActorDrawState.reset();
        ASSERT_EQ(legacy.mNativeActorValues.size(), 1u);
        auto& values = legacy.mNativeActorValues.front();
        if (version < 44) values.mBounty.reset();
        values.mValues[8].mModifiers[2] = -9.f;
        if (version < 17) values.mNonPlayerFormHealth.reset();
        if (version < 18) values.mPassiveAbilities.reset();
        if (version < 25) values.mProcessKnockedState.reset();
        else values.mProcessKnockedState = 0;
        if (version < 26) values.mProcessAction.reset();
        else values.mProcessAction = -1;
        if (version < 12) legacy.mNativeActorLife.clear();
        if (version < 14) legacy.mNativeActorBreath.clear();
        else legacy.mNativeActorBreath[values.mActor] = 13.25f;
        if (version >= 13) legacy.mNativeDeathCounts[values.mBase] = 3;
        if (version < 16)
        {
            legacy.mNativeActorManagerTime = 0.f;
            legacy.mNativeActorUpdateTimes.clear();
        }
        else
        {
            legacy.mNativeActorManagerTime = 12.5f;
            legacy.mNativeActorUpdateTimes[values.mActor] = 11.25f;
        }
        ASSERT_NO_THROW(legacy.validate());
        const auto decoded = ESM4::RuntimeState::deserializeBinary(legacy.serializeBinary());
        EXPECT_EQ(decoded.mNativeActorValues, legacy.mNativeActorValues);
        readNativeSnapshot(fixture, decoded);
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        const auto migrated = world.captureOblivionRuntimeState();
        EXPECT_EQ(migrated.mNativeActorValues, legacy.mNativeActorValues);
        EXPECT_EQ(migrated.mNativeActorLife, legacy.mNativeActorLife);
        EXPECT_EQ(migrated.mNativeActorBreath, legacy.mNativeActorBreath);
        EXPECT_EQ(migrated.mNativeDeathCounts, legacy.mNativeDeathCounts);
        EXPECT_EQ(migrated.mNativeActorManagerTime, legacy.mNativeActorManagerTime);
        EXPECT_EQ(migrated.mNativeActorUpdateTimes, legacy.mNativeActorUpdateTimes);
        EXPECT_EQ(fixture.mActor.getClass().getCreatureStats(fixture.mActor).getHealth().getCurrent(), 91.f);
        readNativeSnapshot(fixture, ESM4::RuntimeState::deserializeBinary(migrated.serializeBinary()));
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), migrated.serializeBinary());
    }
}

TEST(OblivionWorldTest, NativeScriptRestorePreparationDiscardsMovesAndCommitsOnceWithoutDispatch)
{
    PopulatedMigrationFixture fixture(true);
    auto& world = fixture.mWorld;
    auto& scripts = *world.getOblivionScriptManager();
    ASSERT_TRUE(scripts.dispatchObjectEvent(fixture.mActor, "onactivate", world.getPlayerPtr()));
    const auto before = world.captureOblivionRuntimeState();
    ASSERT_EQ(before.mScriptInstances.size(), 1u);
    auto changed = before;
    changed.mScriptEventSequence += 7;
    changed.mScriptInstances.front().mLocals.front() = std::int64_t{9};
    changed.mScriptInstances.front().mOnLoadFired = true;
    {
        auto cancelled = scripts.prepareRestore(changed);
        const auto after = world.captureOblivionRuntimeState();
        EXPECT_EQ(after.mScriptInstances, before.mScriptInstances);
        EXPECT_EQ(after.mScriptEventSequence, before.mScriptEventSequence);
    }
    EXPECT_EQ(world.captureOblivionRuntimeState().mScriptInstances, before.mScriptInstances);
    auto plan = scripts.prepareRestore(changed);
    auto moved = std::move(plan);
    EXPECT_FALSE(plan.commit());
    EXPECT_TRUE(moved.commit());
    EXPECT_FALSE(moved.commit());
    const auto restored = world.captureOblivionRuntimeState();
    EXPECT_EQ(restored.mScriptInstances, changed.mScriptInstances);
    EXPECT_EQ(restored.mScriptEventSequence, changed.mScriptEventSequence);
    ASSERT_TRUE(scripts.dispatchObjectEvent(fixture.mActor, "onactivate", world.getPlayerPtr()));
    EXPECT_EQ(std::get<std::int64_t>(world.captureOblivionRuntimeState().mScriptInstances.front().mLocals.front()), 10);
    // Empty early-development locals retain the existing zero-initialization
    // migration, but it is prepared before any state publication now.
    changed.mScriptInstances.front().mLocals.clear();
    ASSERT_NO_THROW(scripts.restore(changed));
    EXPECT_EQ(std::get<std::int64_t>(world.captureOblivionRuntimeState().mScriptInstances.front().mLocals.front()), 0);
}

TEST(OblivionWorldTest, NativeScriptBindingsRejectBeforeInventoryClockIdentityAndScriptPublication)
{
    PopulatedMigrationFixture fixture(true);
    auto& world = fixture.mWorld;
    auto& scripts = *world.getOblivionScriptManager();
    ASSERT_TRUE(scripts.dispatchObjectEvent(fixture.mActor, "onactivate", world.getPlayerPtr()));
    auto saved = world.captureOblivionRuntimeState();
    ASSERT_EQ(saved.mScriptInstances.size(), 1u);
    // Seed a real populated Player inventory so rejected loads cannot conceal
    // a destructive replacement with the same empty snapshot.
    ESM4::RuntimeInventoryItem item;
    item.mBase = ESM::FormKey::content("headless.esm", 0x940);
    item.mCount = 1;
    item.mCondition = 43.125f;
    item.mCharge = 7.25f;
    saved.mPlayer.mInventory = {item};
    readNativeSnapshot(fixture, saved);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    const auto before = world.captureOblivionRuntimeState();
    const auto clock = world.getTimeStamp();
    const auto inventory = world.captureOblivionActorInventory(world.getPlayerPtr());
    auto pending = world.prepareOblivionDynamicReferenceKey();
    for (int invalid : {0, 1, 2, 3, 4})
    {
        SCOPED_TRACE(invalid);
        auto candidate = before;
        candidate.mClock.mHour = 9;
        candidate.mNextDynamicSerial += 10;
        candidate.mPlayer.mInventory.clear();
        candidate.mScriptEventSequence += 7;
        if (invalid == 0) candidate.mScriptInstances.front().mUnit = "missing-program";
        if (invalid == 1) candidate.mScriptInstances.front().mLocals.push_back(std::int64_t{1});
        if (invalid == 4) candidate.mScriptInstances.front().mLocals.front() = std::string("not numeric");
        if (invalid == 2 || invalid == 3)
        {
            ESM4::RuntimeQuestState quest;
            quest.mQuest = ESM::FormKey::content("headless.esm", invalid == 2 ? 0xdead : 0x940);
            quest.mStage = 100;
            candidate.mQuests.push_back(quest);
        }
        ASSERT_NO_THROW(candidate.validate());
        EXPECT_ANY_THROW(admitNativeSnapshot(fixture, candidate));
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before.serializeBinary());
        EXPECT_TRUE(pending->isValid());
        readNativeSnapshot(fixture, candidate);
        EXPECT_THROW(world.applyOblivionRuntimeState(), std::invalid_argument);
        EXPECT_EQ(world.getTimeStamp(), clock);
        EXPECT_EQ(world.captureOblivionActorInventory(world.getPlayerPtr()), inventory);
        ESM4::RuntimeState scriptsAfter;
        scripts.capture(scriptsAfter);
        EXPECT_EQ(scriptsAfter.mScriptInstances, before.mScriptInstances);
        EXPECT_EQ(scriptsAfter.mScriptEventSequence, before.mScriptEventSequence);
        EXPECT_EQ(scriptsAfter.mQuests, before.mQuests);
        EXPECT_TRUE(pending->isValid());
    }
    readNativeSnapshot(fixture, before);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    EXPECT_FALSE(pending->isValid());
    EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before.serializeBinary());
}

TEST(OblivionWorldTest, NativeScriptRestoreOmittedQuestsUseWinningDefaultsWithoutReplayingCompletedStages)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    ESM4::Quest quest{};
    quest.mId = {0xa20, 0};
    quest.mFormKey = ESM::FormKey::content("headless.esm", 0xa20);
    quest.mData.flags = ESM4::Quest::Flag_StartGameEnabled;
    world.getStore().getWritable<ESM4::Quest>().insertStatic(quest, quest.mFormKey);
    auto saved = world.captureOblivionRuntimeState();
    ESM4::RuntimeQuestState completed;
    completed.mQuest = quest.mFormKey;
    completed.mStage = 100;
    completed.mCompletedStages = {50, 100};
    saved.mQuests = {completed};
    readNativeSnapshot(fixture, saved);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    EXPECT_EQ(world.captureOblivionRuntimeState().mQuests, saved.mQuests);
    saved.mQuests.clear();
    readNativeSnapshot(fixture, saved);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    const auto migrated = world.captureOblivionRuntimeState();
    ASSERT_EQ(migrated.mQuests.size(), 1u);
    EXPECT_EQ(migrated.mQuests.front().mQuest, quest.mFormKey);
    EXPECT_EQ(migrated.mQuests.front().mStage, 0);
    EXPECT_TRUE(migrated.mQuests.front().mCompletedStages.empty());
    EXPECT_TRUE(migrated.mQuests.front().mRunning);
    readNativeSnapshot(fixture, ESM4::RuntimeState::deserializeBinary(migrated.serializeBinary()));
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), migrated.serializeBinary());
}

namespace
{
    ESM4::RuntimeState populatedAiRestore(PopulatedMigrationFixture& fixture)
    {
        auto& world = fixture.mWorld;
        ESM4::AIPackage package{};
        package.mId = {0xa30, 0};
        package.mFormKey = ESM::FormKey::content("headless.esm", 0xa30);
        package.mPackageType = ESM4::AIPackageType::Travel;
        package.mScheduleData.mStartHour = 6;
        package.mScheduleData.mDuration = 4;
        world.getStore().getWritable<ESM4::AIPackage>().insertStatic(package, package.mFormKey);
        ESM4::Pathgrid graph{};
        graph.mId = {0xa40, 0};
        graph.mFormKey = ESM::FormKey::content("headless.esm", 0xa40);
        graph.mData = 2;
        graph.mNodes = {{0, 0, 0, 0, 0, 0}, {10, 0, 0, 0, 0, 0}};
        graph.mLinks = {{0, 1}};
        auto state = world.captureOblivionRuntimeState();
        world.getStore().getOblivionPathgridService().registerPathgrid(graph, state.mPlayer.mCell);
        ESM4::RuntimeActorAiState actor;
        actor.mActor = fixture.mActor.getCellRef().getFormKey();
        actor.mBase = fixture.mActor.get<ESM4::Npc>()->mBase->mFormKey;
        actor.mCell = state.mPlayer.mCell;
        actor.mLastValidCell = actor.mCell;
        actor.mTier = ESM4::ProcessTier::Low;
        actor.mSelectionGeneration = 3;
        state.mActorAi = {actor};
        state.mAiRngState = 41;
        state.mDetectionVectors = {{actor.mActor, state.mPlayer.mReference, 12.5, true, true}};
        state.mPathPoints = {{graph.mFormKey, 1, false}};
        state.mPendingPackageDone = {{actor.mActor, package.mFormKey}, {actor.mActor, package.mFormKey}};
        return state;
    }
}

TEST(OblivionWorldTest, NativeAiRestorePreparationDiscardsAndCommitsActorsEventsAndOverlaysOnce)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    auto& ai = *world.getOblivionAiService();
    const auto saved = populatedAiRestore(fixture);
    const auto before = world.captureOblivionRuntimeState();
    const auto* navigator = world.getStore().getOblivionPathgridService().navigatorPathgrid(saved.mPathPoints.front().mPathgrid);
    {
        auto cancelled = ai.prepareRestore(saved);
        const auto after = world.captureOblivionRuntimeState();
        EXPECT_EQ(after.mActorAi, before.mActorAi);
        EXPECT_EQ(after.mAiRngState, before.mAiRngState);
        EXPECT_EQ(after.mPendingPackageDone, before.mPendingPackageDone);
        EXPECT_EQ(after.mPathPoints, before.mPathPoints);
        EXPECT_EQ(after.mDetectionVectors, before.mDetectionVectors);
    }
    auto prepared = ai.prepareRestore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
    auto moved = std::move(prepared);
    EXPECT_FALSE(prepared.commit());
    EXPECT_TRUE(moved.commit());
    EXPECT_FALSE(moved.commit());
    const auto captured = world.captureOblivionRuntimeState();
    EXPECT_EQ(captured.mActorAi, saved.mActorAi);
    EXPECT_EQ(captured.mAiRngState, saved.mAiRngState);
    EXPECT_EQ(captured.mPendingPackageDone, saved.mPendingPackageDone);
    EXPECT_EQ(captured.mPathPoints, saved.mPathPoints);
    EXPECT_EQ(captured.mDetectionVectors, saved.mDetectionVectors);
    EXPECT_EQ(world.getStore().getOblivionPathgridService().navigatorPathgrid(saved.mPathPoints.front().mPathgrid), navigator);
    auto empty = saved;
    empty.mActorAi.clear(); empty.mPendingPackageDone.clear(); empty.mPathPoints.clear();
    ASSERT_NO_THROW(ai.restore(empty));
    EXPECT_TRUE(world.captureOblivionRuntimeState().mPathPoints.empty());
    EXPECT_TRUE(world.captureOblivionRuntimeState().mPendingPackageDone.empty());
}

TEST(OblivionWorldTest, NativeWorldClearResetsOverlaysAndEventsBeforeLegacyStateAdmission)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    auto& ai = *world.getOblivionAiService();
    const auto saved = populatedAiRestore(fixture);
    auto& pathgrids = world.getStore().getOblivionPathgridService();
    const auto pathgrid = saved.mPathPoints.front().mPathgrid;
    const ESM4::PathgridNodeKey node{pathgrid, 1};
    const auto* graph = pathgrids.graph(pathgrid);
    const auto* navigator = pathgrids.navigatorPathgrid(pathgrid);
    ASSERT_NE(graph, nullptr);
    ASSERT_NE(navigator, nullptr);
    for (int cycle = 0; cycle < 2; ++cycle)
    {
        SCOPED_TRACE(cycle);
        ASSERT_NO_THROW(ai.restore(saved));
        ASSERT_FALSE(pathgrids.isNodeEnabled(node));
        ASSERT_EQ(navigator->mPoints.size(), 2u);
        ASSERT_TRUE(navigator->mEdges.empty());
        const auto generation = pathgrids.generation(pathgrid);
        ASSERT_NO_THROW(world.clear());
        EXPECT_EQ(world.getOblivionAiService(), &ai);
        EXPECT_EQ(pathgrids.graph(pathgrid), graph);
        EXPECT_EQ(pathgrids.navigatorPathgrid(pathgrid), navigator);
        EXPECT_TRUE(pathgrids.isNodeEnabled(node));
        EXPECT_EQ(navigator->mPoints.size(), 2u);
        EXPECT_FALSE(navigator->mEdges.empty());
        EXPECT_EQ(pathgrids.generation(pathgrid), generation + 1);
        ESM4::RuntimeState cleared;
        ai.capture(cleared);
        EXPECT_TRUE(cleared.mPathPoints.empty());
        EXPECT_TRUE(cleared.mPendingPackageDone.empty());
        EXPECT_TRUE(cleared.mDetectionVectors.empty());
        EXPECT_EQ(cleared.mAiRngState, 1u);
        // The accepted legacy branch has no native record to replace overlays.
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        EXPECT_TRUE(pathgrids.isNodeEnabled(node));
        ASSERT_NO_THROW(world.clear());
        EXPECT_EQ(pathgrids.generation(pathgrid), generation + 1);
    }
}

TEST(OblivionWorldTest, NativeAiRestoreRejectsBindingsAndConversionsBeforeWorldPublication)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    auto saved = populatedAiRestore(fixture);
    ESM4::RuntimeInventoryItem item;
    item.mBase = ESM::FormKey::content("headless.esm", 0x940);
    item.mCount = 1; item.mCondition = 43.125f; item.mCharge = 7.25f;
    saved.mPlayer.mInventory = {item};
    readNativeSnapshot(fixture, saved);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    const auto before = world.captureOblivionRuntimeState();
    const auto clock = world.getTimeStamp();
    const auto inventory = world.captureOblivionActorInventory(world.getPlayerPtr());
    auto pending = world.prepareOblivionDynamicReferenceKey();
    for (int invalid : {0, 1, 2, 3, 4, 5, 6, 7, 8})
    {
        SCOPED_TRACE(invalid);
        auto candidate = before;
        candidate.mClock.mHour = 9;
        candidate.mNextDynamicSerial += 10;
        candidate.mPlayer.mInventory.clear();
        if (invalid == 0) candidate.mPathPoints.front().mPathgrid = ESM::FormKey::content("headless.esm", 0xdead);
        if (invalid == 1) candidate.mPathPoints.front().mNode = 99;
        if (invalid == 2) candidate.mActorAi.front().mBase = item.mBase;
        if (invalid == 3) candidate.mActorAi.front().mCell = item.mBase;
        if (invalid == 4) candidate.mActorAi.front().mActor = ESM::FormKey::content("headless.esm", 0xdead);
        if (invalid == 5)
        {
            auto& actor = candidate.mActorAi.front();
            actor.mScriptPackage = actor.mPackage = ESM::FormKey::dynamic("force-flee", 2);
            actor.mSource = ESM4::PackageSource::Script;
            actor.mPackageType = ESM4::AIPackageType::FleeNotCombat;
            actor.mProcedure = ESM4::packageProcedure(actor.mPackageType);
            actor.mDurationRemaining = 1e32f;
        }
        if (invalid == 6) candidate.mPendingPackageDone.front().mPackage = ESM::FormKey::content("headless.esm", 0xdead);
        if (invalid == 7) candidate.mDetectionVectors.front().mObserver = ESM::FormKey::content("headless.esm", 0xdead);
        if (invalid == 8) candidate.mPendingPackageDone.front().mActor = ESM::FormKey::content("headless.esm", 0xdead);
        ASSERT_NO_THROW(candidate.validate());
        EXPECT_ANY_THROW(admitNativeSnapshot(fixture, candidate));
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before.serializeBinary());
        EXPECT_TRUE(pending->isValid());
        readNativeSnapshot(fixture, candidate);
        EXPECT_ANY_THROW(world.applyOblivionRuntimeState());
        EXPECT_EQ(world.getTimeStamp(), clock);
        EXPECT_EQ(world.captureOblivionActorInventory(world.getPlayerPtr()), inventory);
        ESM4::RuntimeState after;
        world.getOblivionAiService()->capture(after);
        EXPECT_EQ(after.mActorAi, before.mActorAi);
        EXPECT_EQ(after.mAiRngState, before.mAiRngState);
        EXPECT_EQ(after.mPendingPackageDone, before.mPendingPackageDone);
        EXPECT_EQ(after.mPathPoints, before.mPathPoints);
        EXPECT_EQ(after.mDetectionVectors, before.mDetectionVectors);
        EXPECT_TRUE(pending->isValid());
    }
    readNativeSnapshot(fixture, before);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    EXPECT_FALSE(pending->isValid());
    EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before.serializeBinary());
}

TEST(OblivionWorldTest, NativeAiRestorePreservesLeaderGroupAndNonActorTargetBaseExactly)
{
    for (bool actorLeader : {false, true})
    {
        SCOPED_TRACE(actorLeader);
        PopulatedMigrationFixture fixture;
        auto& world = fixture.mWorld;
        auto saved = populatedAiRestore(fixture);
        auto package = *world.getStore().search<ESM4::AIPackage>(saved.mPendingPackageDone.front().mPackage);
        package.mPackageType = ESM4::AIPackageType::Follow;
        world.getStore().getWritable<ESM4::AIPackage>().insertStatic(package, package.mFormKey);
        ESM::FormKey leader, leaderBase;
        if (actorLeader)
        {
            const auto ptr = addNativeNpc(fixture, 0x910);
            leader = ptr.getCellRef().getFormKey();
            leaderBase = ptr.get<ESM4::Npc>()->mBase->mFormKey;
            auto reference = captureNativeActorState(fixture, ptr).mReferences.front();
            reference.mCell = saved.mPlayer.mCell;
            saved.mReferences.push_back(reference);
            auto state = saved.mActorAi.front();
            state.mActor = leader;
            state.mBase = leaderBase;
            state.mCompanionGroup = {}; // Explicit null must survive restore.
            saved.mActorAi.push_back(state);
        }
        else
        {
            ESM4::Reference reference{};
            reference.mId = {0xa50, 0};
            reference.mFormKey = ESM::FormKey::content("headless.esm", 0xa50);
            reference.mBaseObj = {0x940, 0};
            reference.mBaseKey = ESM::FormKey::content("headless.esm", 0x940);
            world.getStore().getWritable<ESM4::Reference>().insertStatic(reference, reference.mFormKey);
            leader = reference.mFormKey;
            leaderBase = reference.mBaseKey;
            // Deliberately not resident or in the snapshot: resolve winning
            // REFR metadata without creating a class/cell cache during prepare.
        }
        auto& member = saved.mActorAi.front();
        member.mPackage = package.mFormKey;
        member.mSource = ESM4::PackageSource::Base;
        member.mPackageType = package.mPackageType;
        member.mProcedure = ESM4::packageProcedure(member.mPackageType);
        member.mTarget = leader;
        member.mTargetBase = leaderBase;
        member.mCompanionGroup = leader;
        member.mFormationIndex = 0;
        saved.mCompanions = {{leader, member.mActor, leader, {}, 0}};
        ASSERT_NO_THROW(saved.validate());
        auto& ai = *world.getOblivionAiService();
        ASSERT_NO_THROW(ai.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary())));
        ESM4::RuntimeState captured;
        ai.capture(captured);
        EXPECT_EQ(captured.mActorAi, saved.mActorAi);
        EXPECT_EQ(captured.mCompanions, saved.mCompanions);
    }
}

TEST(OblivionWorldTest, NativeAiRestoreMigratesPopulatedQueuesOverlaysAndDetectionFromEverySupportedVersion)
{
    for (std::uint32_t version = 5; version <= ESM4::CurrentRuntimeStateVersion; ++version)
    {
        SCOPED_TRACE(version);
        PopulatedMigrationFixture fixture;
        auto& world = fixture.mWorld;
        auto saved = populatedAiRestore(fixture);
        saved.mVersion = version;
        if (version < 6) saved.mPendingPackageDone.clear();
        // Match the already defined wrap policy, including the maximum saved
        // generation. Restore must not narrow the accepted uint64 contract.
        saved.mActorAi.front().mSelectionGeneration = std::numeric_limits<std::uint64_t>::max();
        saved.mAiRngState = 1;
        ASSERT_NO_THROW(saved.validate());
        readNativeSnapshot(fixture, ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        const auto migrated = world.captureOblivionRuntimeState();
        EXPECT_EQ(migrated.mVersion, ESM4::CurrentRuntimeStateVersion);
        EXPECT_EQ(migrated.mActorAi, saved.mActorAi);
        EXPECT_EQ(migrated.mAiRngState, saved.mAiRngState);
        EXPECT_EQ(migrated.mPendingPackageDone, saved.mPendingPackageDone);
        EXPECT_EQ(migrated.mPathPoints, saved.mPathPoints);
        EXPECT_EQ(migrated.mDetectionVectors, saved.mDetectionVectors);
        readNativeSnapshot(fixture, ESM4::RuntimeState::deserializeBinary(migrated.serializeBinary()));
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), migrated.serializeBinary());
    }
}

TEST(OblivionWorldTest, NativeSaveAdmissionRejectsContentBeforeTouchingLiveWorldOrPendingIdentity)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    const auto before = world.captureOblivionRuntimeState();
    auto pending = world.prepareOblivionDynamicReferenceKey();
    const auto admit = [&](const ESM4::RuntimeState& state) {
        ESM::ESMWriter writer;
        writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion);
        auto stream = std::make_unique<std::stringstream>();
        writer.save(*stream);
        ESM::SavedGame profile{};
        profile.mGameProfile = ESM::GameProfile::Oblivion;
        profile.mRuntimeStateVersion = state.mVersion;
        writer.startRecord(ESM::REC_SAVE);
        profile.save(writer);
        writer.endRecord(ESM::REC_SAVE);
        writer.startRecord(ESM::REC_T4ST);
        state.save(writer);
        writer.endRecord(ESM::REC_T4ST);
        ESM::ESMReader reader;
        reader.open(std::move(stream), "world-save-admission");
        return MWState::admitSave(reader, world.getGameProfile(),
            [&](const auto& native) { world.validateOblivionSaveState(native); });
    };
    ASSERT_FALSE(before.mContent.empty());
    for (const bool missing : {false, true})
    {
        auto invalid = before;
        if (missing)
            invalid.mContent.front().mPlugin = "missing.esm";
        else
            invalid.mContent.front().mFingerprint = "sha256:" + std::string(64, '0');
        EXPECT_THROW(admit(invalid), std::runtime_error);
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before.serializeBinary());
        EXPECT_TRUE(pending->isValid());
    }
    EXPECT_NO_THROW(admit(before));
    EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before.serializeBinary());
    EXPECT_TRUE(pending->isValid());
    MWWorld::World legacy(nullptr, -1, "", {}, ESM::GameProfile::Morrowind);
    EXPECT_THROW(legacy.validateOblivionSaveState(before), std::runtime_error);
}

TEST(OblivionWorldTest, NativeSaveAdmissionChecksImmutableBindingsWithoutLoadingCells)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    const auto before = world.captureOblivionRuntimeState();
    auto pending = world.prepareOblivionDynamicReferenceKey();
    const auto loadedCells = [&] {
        std::size_t count = 0;
        world.getWorldModel().forEachLoadedCellStore([&](auto&) { ++count; });
        return count;
    };
    const auto beforeCells = loadedCells();
    ASSERT_EQ(before.mReferences.size(), 1u);
    const auto unchanged = [&] {
        EXPECT_EQ(loadedCells(), beforeCells);
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before.serializeBinary());
        EXPECT_TRUE(pending->isValid());
    };
    for (int fault = 0; fault != 4; ++fault)
    {
        SCOPED_TRACE(fault);
        auto invalid = before;
        const auto missing = ESM::FormKey::content("headless.esm", 0xdead);
        switch (fault)
        {
            case 0: invalid.mPlayer.mCell = missing; break;
            case 1: invalid.mReferences.front().mCell = missing; break;
            case 2: invalid.mReferences.front().mKey = missing; break;
            case 3: invalid.mReferences.front().mBase = ESM::FormKey::content("headless.esm", 0x940); break;
        }
        EXPECT_THROW(world.validateOblivionSaveState(invalid), std::runtime_error);
        unchanged();
    }
    // A valid destination is checked in the immutable store, never materialized.
    ESM4::Cell destination{};
    destination.mId = ESM::RefId(ESM::FormId{2, 0});
    destination.mFormKey = ESM::FormKey::content("headless.esm", 2);
    destination.mCellFlags = ESM4::CELL_Interior;
    destination.mEditorId = "UnloadedAdmissionDestination";
    world.getStore().getWritable<ESM4::Cell>().insertStatic(destination, destination.mFormKey);
    auto relocated = before;
    relocated.mPlayer.mCell = destination.mFormKey;
    relocated.mReferences.front().mCell = destination.mFormKey;
    EXPECT_NO_THROW(world.validateOblivionSaveState(relocated));
    unchanged();
    auto dynamic = relocated;
    dynamic.mReferences.front().mKey = ESM::FormKey::dynamic("admission", 1);
    dynamic.mNextDynamicSerial = 2;
    EXPECT_NO_THROW(world.validateOblivionSaveState(dynamic));
    unchanged();
    EXPECT_NO_THROW(world.validateOblivionSaveState(before));
    unchanged();
}

TEST(OblivionWorldTest, NativeSaveAdmissionRejectsActorStateOnAnObjectReference)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    const auto before = world.captureOblivionRuntimeState();
    ESM4::Reference object{};
    object.mId = {0x901, 0};
    object.mFormKey = ESM::FormKey::content("headless.esm", 0x901);
    object.mBaseObj = {0x940, 0};
    object.mBaseKey = ESM::FormKey::content("headless.esm", 0x940);
    world.getStore().getWritable<ESM4::Reference>().insertStatic(object, object.mFormKey);
    auto candidate = before;
    auto& saved = candidate.mReferences.front();
    saved.mKey = object.mFormKey;
    saved.mBase = object.mBaseKey;
    EXPECT_NO_THROW(world.validateOblivionSaveState(candidate));
    saved.mActorDrawState = ESM4::ActorDrawState::Weapon;
    ESM4::RuntimeActorValues values{};
    values.mActor = saved.mKey;
    values.mBase = saved.mBase;
    candidate.mNativeActorValues = {values};
    ESM4::RuntimeActorLife life{};
    life.mActor = saved.mKey;
    life.mBase = saved.mBase;
    candidate.mNativeActorLife = {life};
    ASSERT_NO_THROW(candidate.validate());
    EXPECT_THROW(world.validateOblivionSaveState(candidate), std::runtime_error);
    EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before.serializeBinary());
    saved.mActorDrawState.reset();
    candidate.mNativeActorValues.clear();
    candidate.mNativeActorLife.clear();
    saved.mBase = ESM::FormKey::content("headless.esm", 0x800);
    EXPECT_THROW(world.validateOblivionSaveState(candidate), std::runtime_error);
}

TEST(OblivionWorldTest, NativeSaveAdmissionRequiresTheWinningCreatureBase)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    const auto before = world.captureOblivionRuntimeState();
    ESM4::ActorCreature creature{};
    creature.mId = {0x902, 0};
    creature.mFormKey = ESM::FormKey::content("headless.esm", 0x902);
    creature.mBaseObj = {0x820, 0};
    creature.mBaseKey = ESM::FormKey::content("headless.esm", 0x820);
    world.getStore().getWritable<ESM4::ActorCreature>().insertStatic(creature, creature.mFormKey);
    auto candidate = before;
    auto& saved = candidate.mReferences.front();
    saved.mKey = creature.mFormKey;
    saved.mBase = creature.mBaseKey;
    EXPECT_THROW(world.validateOblivionSaveState(candidate), std::runtime_error);
    ESM4::Creature base{};
    base.mId = creature.mBaseObj;
    world.getStore().getWritable<ESM4::Creature>().insertStatic(base, creature.mBaseKey);
    EXPECT_NO_THROW(world.validateOblivionSaveState(candidate));
    saved.mBase = ESM::FormKey::content("headless.esm", 0x800);
    EXPECT_THROW(world.validateOblivionSaveState(candidate), std::runtime_error);
    EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before.serializeBinary());
}

TEST(OblivionWorldTest, PendingNativeSnapshotCannotReplaceAcceptedCachesOnPreparationFailure)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    ESM4::RuntimeInventoryItem item;
    item.mBase = ESM::FormKey::content("headless.esm", 0x940);
    item.mCount = 1;
    item.mCondition = 43.125f;
    item.mCharge = 7.25f;
    installEquipmentInventory(fixture, fixture.mActor, {item});
    installEquipmentInventory(fixture, world.getPlayerPtr(), {item});
    auto accepted = world.captureOblivionRuntimeState();
    accepted.mPlayer.mInventory.front().mHotkey = 2;
    accepted.mNativePhysicalBlendTimeCache = ESM4::PhysicalBlendTimeCache{3, -2, -4, -0.f, .75f};
    auto retained = accepted.mReferences.front();
    retained.mKey = ESM::FormKey::dynamic("retained-cache", 1);
    retained.mCustomState["cache-marker"] = std::int64_t(91);
    accepted.mReferences.push_back(retained);
    readNativeSnapshot(fixture, accepted);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    const auto before = world.captureOblivionRuntimeState();
    auto pending = before;
    pending.mClock.mHour = 9;
    pending.mPlayer.mInventory.front().mHotkey = 7;
    pending.mNativePhysicalBlendTimeCache->mKeyTime = .5f;
    pending.mReferences.back().mCustomState["cache-marker"] = std::int64_t(92);
    auto invalid = item;
    invalid.mBase = ESM::FormKey::content("headless.esm", 0xdead);
    pending.mReferences.front().mInventory.push_back(invalid);
    ASSERT_NO_THROW(pending.validate());
    readNativeSnapshot(fixture, pending);
    EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before.serializeBinary());
    // Rebuilding a cold class view reads only the previously accepted inventory.
    fixture.mActor.getRefData().setCustomData(nullptr);
    EXPECT_EQ(world.captureOblivionActorInventory(fixture.mActor), std::vector{item});
    EXPECT_EQ(world.captureOblivionActorInventory(world.getPlayerPtr()), before.mPlayer.mInventory);
    for (int attempt = 0; attempt != 2; ++attempt)
    {
        EXPECT_THROW(world.applyOblivionRuntimeState(), std::runtime_error);
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before.serializeBinary());
    }
    readNativeSnapshot(fixture, before);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    // No pending state means no replay of accepted inventory or cached metadata.
    fixture.mActor.getClass().getInventoryStore(fixture.mActor).begin()->getCellRef().setNativeItemCondition(25.125f);
    const auto changed = world.captureOblivionRuntimeState().serializeBinary();
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), changed);
}

TEST(OblivionWorldTest, NativeWorldClearDiscardsPendingAndAcceptedSnapshots)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    auto state = world.captureOblivionRuntimeState();
    readNativeSnapshot(fixture, state);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    state.mClock.mHour = 9;
    readNativeSnapshot(fixture, state);
    world.clear();
    EXPECT_NO_THROW(world.applyOblivionRuntimeState());
}

TEST(OblivionWorldTest, NativeClockRestorePreservesFractionalHourAndScaleForEverySchema)
{
    for (std::uint32_t version = 1; version <= ESM4::CurrentRuntimeStateVersion; ++version)
    for (const char type : {'s', 'l', 'f'})
    for (const bool signedZero : {false, true})
    {
        SCOPED_TRACE(version);
        SCOPED_TRACE(type);
        SCOPED_TRACE(signedZero);
        PopulatedMigrationFixture fixture;
        auto& world = fixture.mWorld;
        auto state = world.captureOblivionRuntimeState();
        state.mVersion = version;
        if (version < 3)
        {
            state.mPlayer.mName.clear();
            state.mPlayer.mRace = {};
            state.mPlayer.mClass = {};
            state.mPlayer.mBirthSign = {};
        }
        state.mClock.mHour = signedZero ? -0. : 1.0216666460037231;
        state.mClock.mTimeScale = signedZero ? -.125 : 2.125;
        // Exercise the declared script type independently of the clock owner.
        ESM::ESMWriter writer;
        writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion);
        auto stream = std::make_unique<std::stringstream>();
        writer.save(*stream);
        for (const auto name : {MWWorld::Globals::sGameHour, MWWorld::Globals::sTimeScale})
        {
            ESM::Global global{};
            global.mId = ESM::RefId::stringRefId(name.getValue());
            global.mValue.setType(type == 's' ? ESM::VT_Short : type == 'l' ? ESM::VT_Long : ESM::VT_Float);
            global.mValue.setInteger(1);
            writer.startRecord(ESM::REC_GLOB);
            global.save(writer);
            writer.endRecord(ESM::REC_GLOB);
        }
        ESM::ESMReader reader;
        reader.open(std::move(stream), "legacy-integer-clock-globals");
        while (reader.hasMoreRecs())
        {
            ASSERT_EQ(reader.getRecName(), ESM::REC_GLOB);
            reader.getRecHeader();
            world.readRecord(reader, ESM::REC_GLOB);
        }
        ASSERT_EQ(world.getGlobalVariableType(MWWorld::Globals::sGameHour), type);
        ASSERT_EQ(world.getGlobalVariableType(MWWorld::Globals::sTimeScale), type);
        readNativeSnapshot(fixture, state);
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        const auto restored = world.captureOblivionRuntimeState();
        EXPECT_EQ(restored.mClock.mHour, state.mClock.mHour);
        EXPECT_EQ(restored.mClock.mTimeScale, state.mClock.mTimeScale);
        EXPECT_EQ(std::bit_cast<std::uint64_t>(restored.mClock.mHour), std::bit_cast<std::uint64_t>(state.mClock.mHour));
        EXPECT_EQ(std::bit_cast<std::uint64_t>(restored.mClock.mTimeScale),
            std::bit_cast<std::uint64_t>(state.mClock.mTimeScale));
        EXPECT_EQ(world.getGlobalVariableType(MWWorld::Globals::sGameHour), type);
        EXPECT_EQ(world.getGlobalVariableType(MWWorld::Globals::sTimeScale), type);
        EXPECT_EQ(world.getGlobalFloat(MWWorld::GlobalVariableName(std::string_view("gamehour"))),
            type == 'f' ? static_cast<float>(state.mClock.mHour) : signedZero ? 0.f : 1.f);
        EXPECT_EQ(world.getGlobalFloat(MWWorld::GlobalVariableName(std::string_view("timescale"))),
            type == 'f' ? static_cast<float>(state.mClock.mTimeScale) : signedZero ? 0.f : 2.f);
        readNativeSnapshot(fixture, restored);
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        EXPECT_EQ(world.captureOblivionRuntimeState().mClock.mHour, state.mClock.mHour);
        EXPECT_EQ(world.captureOblivionRuntimeState().mClock.mTimeScale, state.mClock.mTimeScale);
    }
}

TEST(OblivionWorldTest, NativeClockCaptureUsesOwnedFractionsBeforeSaveAndAfterClear)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    for (int pass = 0; pass != 2; ++pass)
    {
        EXPECT_EQ(world.getGlobalVariableType(MWWorld::Globals::sGameHour), 'f');
        EXPECT_EQ(world.getGlobalVariableType(MWWorld::Globals::sTimeScale), 'f');
        world.setGlobalFloat(MWWorld::GlobalVariableName(std::string_view("gamehour")), 1.25f);
        world.setGlobalFloat(MWWorld::GlobalVariableName(std::string_view("timescale")), .125f);
        EXPECT_EQ(world.getTimeStamp().getHour(), 1.25);
        EXPECT_EQ(world.getGlobalFloat(MWWorld::Globals::sGameHour), 1.25f);
        EXPECT_EQ(world.getTimeManager()->getGameTimeScale(), .125f);
        if (pass == 0)
        {
            const auto snapshot = world.captureOblivionRuntimeState();
            EXPECT_EQ(snapshot.mClock.mHour, 1.25);
            EXPECT_EQ(snapshot.mClock.mTimeScale, .125);
        }
        EXPECT_EQ(world.getGlobalFloat(MWWorld::Globals::sTimeScale), .125f);
        world.clear();
    }
}

TEST(OblivionWorldTest, NativeSemanticAdmissionRejectsLateInventoryAndMetadataWithoutPublication)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    ESM4::RuntimeInventoryItem item;
    item.mBase = ESM::FormKey::content("headless.esm", 0x940);
    item.mCount = 1;
    item.mCondition = 43.125f;
    item.mCharge = 7.25f;
    installEquipmentInventory(fixture, fixture.mActor, {item});
    installEquipmentInventory(fixture, world.getPlayerPtr(), {item});
    const auto before = world.captureOblivionRuntimeState();
    const auto* cache = fixture.mActor.getRefData().getCustomData();
    const auto registrySerial = world.getWorldModel().getLastGeneratedRefNum();
    auto reservation = world.prepareOblivionDynamicReferenceKey();
    for (int fault = 0; fault != 21; ++fault)
    {
        SCOPED_TRACE(fault);
        auto candidate = before;
        candidate.mClock.mHour = 9;
        candidate.mNextDynamicSerial += 10;
        auto& reference = candidate.mReferences.front();
        auto badItem = item;
        switch (fault)
        {
            case 0: badItem.mBase = ESM::FormKey::content("headless.esm", 0xdead);
                candidate.mPlayer.mInventory.push_back(badItem); break;
            case 1: badItem.mBase = ESM::FormKey::content("headless.esm", 0xdead);
                reference.mInventory.push_back(badItem); break;
            case 2: badItem.mOwner = ESM::FormKey::content("missing.esm", 0x800);
                reference.mInventory.push_back(badItem); break;
            case 3: badItem.mOwnershipGlobal = item.mBase;
                reference.mInventory.push_back(badItem); break;
            case 4: reference.mOwnershipGlobal = item.mBase; break;
            case 5: reference.mCustomState["obscript.dead"] = std::int64_t{42}; break;
            case 6: reference.mCustomState["locked"] = std::string("invalid"); break;
            case 7: reference.mCustomState["scale"] = 1e300; break;
            case 8: reference.mCustomState["obscript.animation_scripted"] = std::string("invalid"); break;
            case 9: reference.mCustomState["obscript.animation_progress"] = std::string("invalid"); break;
            case 10: reference.mCustomState["obscript.animation_progress"] = .5;
                reference.mCustomState["obscript.animation_group"] = std::int64_t{1}; break;
            case 11: reference.mOwnershipRank = -1; break;
            case 12: reference.mItemCondition = 1.f; break;
            case 13: reference.mOwner = ESM::FormKey::dynamic("invalid-owner", 1); break;
            case 14: candidate.mPlayer.mRace = item.mBase; break;
            case 15: candidate.mPlayer.mBirthSign = item.mBase; break;
            case 16: candidate.mClock.mTimeScale = 1e300; break;
            case 17: candidate.mPlayer.mActorValues["health.current"] = 1e300; break;
            case 18: candidate.mPlayer.mActorValues["level"] = 1e300; break;
            case 19: reference.mKey = ESM::FormKey::dynamic("native-reference", candidate.mNextDynamicSerial); break;
            case 20: candidate.mGlobals[item.mBase] = std::int64_t{1}; break;
        }
        ASSERT_NO_THROW(candidate.validate());
        EXPECT_ANY_THROW(admitNativeSnapshot(fixture, candidate));
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before.serializeBinary());
        EXPECT_EQ(fixture.mActor.getRefData().getCustomData(), cache);
        EXPECT_EQ(world.getWorldModel().getLastGeneratedRefNum(), registrySerial);
        EXPECT_TRUE(reservation->isValid());
    }
    EXPECT_NO_THROW(admitNativeSnapshot(fixture, before));
    EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before.serializeBinary());
    EXPECT_TRUE(reservation->isValid());
}

TEST(OblivionWorldTest, NativeSemanticAdmissionValidatesDetachedActorAuthority)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    ASSERT_TRUE(world.activateOblivionActor(fixture.mActor));
    const auto before = world.captureOblivionRuntimeState();
    ASSERT_EQ(before.mNativeActorValues.size(), 1u);
    auto candidate = before;
    candidate.mNativeActorValues.front().mValues[33].mModifiers[1] = 1e32f;
    ASSERT_NO_THROW(candidate.validate());
    EXPECT_ANY_THROW(admitNativeSnapshot(fixture, candidate));
    EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before.serializeBinary());
    candidate = before;
    candidate.mNativeDeathCounts[ESM::FormKey::content("headless.esm", 0x940)] = 1;
    ASSERT_NO_THROW(candidate.validate());
    EXPECT_ANY_THROW(admitNativeSnapshot(fixture, candidate));
    EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before.serializeBinary());
    EXPECT_NO_THROW(admitNativeSnapshot(fixture, before));
    EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before.serializeBinary());
}

TEST(OblivionWorldTest, NativeSemanticAdmissionRetainsWinningContainerLeveledTemplates)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    const auto before = world.captureOblivionRuntimeState();
    ESM4::LevelledItem list{};
    list.mId = {0x980, 0};
    const auto listKey = ESM::FormKey::content("headless.esm", 0x980);
    world.getStore().getWritable<ESM4::LevelledItem>().insertStatic(list, listKey);
    ESM4::Container container{};
    container.mId = {0x982, 0};
    const auto containerKey = ESM::FormKey::content("headless.esm", 0x982);
    world.getStore().getWritable<ESM4::Container>().insertStatic(container, containerKey);
    ESM4::Reference object{};
    object.mId = {0x981, 0};
    object.mFormKey = ESM::FormKey::content("headless.esm", 0x981);
    object.mBaseObj = container.mId;
    object.mBaseKey = containerKey;
    world.getStore().getWritable<ESM4::Reference>().insertStatic(object, object.mFormKey);
    auto state = before;
    auto reference = state.mReferences.front();
    reference.mKey = object.mFormKey;
    reference.mBase = object.mBaseKey;
    ESM4::RuntimeInventoryItem item;
    item.mBase = listKey;
    item.mCount = 2;
    reference.mInventory = {item};
    state.mReferences.push_back(reference);
    ASSERT_NO_THROW(state.validate());
    EXPECT_NO_THROW(admitNativeSnapshot(fixture, state));
    state.mReferences.back().mInventory.front().mBase = ESM::FormKey::content("headless.esm", 0xdead);
    EXPECT_ANY_THROW(admitNativeSnapshot(fixture, state));
    state.mReferences.back().mInventory.front().mBase = listKey;
    state.mPlayer.mInventory = state.mReferences.back().mInventory;
    // Actor/Player snapshots contain actual stacks, not unexpanded templates.
    EXPECT_ANY_THROW(admitNativeSnapshot(fixture, state));
    EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before.serializeBinary());
}

TEST(OblivionWorldTest, NativeDynamicReferencesReconstructAfterClearAndRetainExactLooseExtras)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    MWClass::ESM4Takeable<ESM4::Weapon>::registerSelf();
    auto saved = world.captureOblivionRuntimeState();
    saved.mReferences.clear();
    saved.mNextDynamicSerial = 3;
    for (std::uint64_t serial = 1; serial != 3; ++serial)
    {
        ESM4::RuntimeReferenceState reference;
        reference.mKey = ESM::FormKey::dynamic("native-reference", serial);
        reference.mBase = ESM::FormKey::content("headless.esm", 0x940);
        reference.mCell = saved.mPlayer.mCell;
        reference.mPosition.pos[0] = 13.25f * serial;
        reference.mEnabled = serial == 1;
        reference.mDeleted = serial == 2;
        reference.mOwner = ESM::FormKey::content("headless.esm", 0x800);
        reference.mOwnershipRank = -7;
        reference.mItemCondition = serial == 1 ? 43.125f : -0.f;
        reference.mItemCharge = serial == 1 ? 7.25f : 0.f;
        reference.mCustomState["count"] = std::int64_t{3};
        reference.mCustomState["scale"] = 1.25;
        saved.mReferences.push_back(reference);
    }
    world.clear();
    acceptNativeSnapshot(fixture, ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
    const auto find = [&](const ESM::FormKey& key) {
        MWWorld::Ptr found;
        world.getWorldModel().forEachLoadedCellStore([&](MWWorld::CellStore& cell) {
            cell.forEach([&](const MWWorld::Ptr& ptr) {
                if (ptr.getCellRef().getFormKey() == key) found = ptr;
                return true;
            }, true);
        });
        return found;
    };
    for (const auto& reference : saved.mReferences)
    {
        const auto ptr = find(reference.mKey);
        ASSERT_FALSE(ptr.isEmpty());
        EXPECT_EQ(world.getWorldModel().getPtr(ptr.getCellRef().getRefNum()), ptr);
        EXPECT_EQ(world.getWorldModel().getDynamicNativePtr(reference.mKey), ptr);
        EXPECT_EQ(world.getOblivionAiService()->resolveReference(reference.mKey), ptr);
        EXPECT_EQ(ptr.getRefData().getPosition(), reference.mPosition);
        EXPECT_EQ(ptr.getRefData().isEnabled(), reference.mEnabled);
        EXPECT_EQ(ptr.mRef->isDeleted(), reference.mDeleted);
        EXPECT_EQ(ptr.getCellRef().getNativeOwnershipRank(), reference.mOwnershipRank);
        EXPECT_EQ(ptr.getCellRef().getNativeItemCondition(), reference.mItemCondition);
        EXPECT_EQ(std::signbit(*ptr.getCellRef().getNativeItemCondition()), std::signbit(*reference.mItemCondition));
        EXPECT_EQ(ptr.getCellRef().getEnchantmentCharge(), *reference.mItemCharge);
        EXPECT_EQ(ptr.getCellRef().getScale(), 1.25f);
    }
    const auto captured = world.captureOblivionRuntimeState();
    EXPECT_EQ(captured.mNextDynamicSerial, 3u);
    EXPECT_EQ(captured.mReferences.size(), 2u);
    const auto first = find(saved.mReferences.front().mKey);
    const auto id = first.getCellRef().getRefNum();
    readNativeSnapshot(fixture, captured);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    EXPECT_EQ(find(saved.mReferences.front().mKey), first);
    EXPECT_EQ(first.getCellRef().getRefNum(), id);
    world.clear();
    acceptNativeSnapshot(fixture, captured);
    EXPECT_FALSE(find(saved.mReferences.front().mKey).isEmpty());
    EXPECT_EQ(world.captureOblivionRuntimeState().mReferences, captured.mReferences);
    const auto restored = find(saved.mReferences.front().mKey);
    world.getWorldModel().deregisterLiveCellRef(*restored.mRef);
    EXPECT_TRUE(world.getWorldModel().getDynamicNativePtr(saved.mReferences.front().mKey).isEmpty());
    world.getWorldModel().registerPtr(restored);
    EXPECT_EQ(world.getWorldModel().getDynamicNativePtr(saved.mReferences.front().mKey), restored);
}

TEST(OblivionWorldTest, NativeDynamicActorReconstructsAuthorityInventoryAndDrawViewAfterClear)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    ASSERT_TRUE(world.initializeOblivionNonPlayerActor(fixture.mActor, ESM4::ActorValueProcess::Active));
    auto saved = world.captureOblivionRuntimeState();
    const auto key = ESM::FormKey::dynamic("native-reference", 1);
    saved.mNextDynamicSerial = 2;
    saved.mReferences.front().mKey = key;
    saved.mReferences.front().mActorDrawState = ESM4::ActorDrawState::Weapon;
    saved.mNativeActorValues.front().mActor = key;
    saved.mNativeActorLife.front().mActor = key;
    if (!saved.mNativeActorBreath.empty())
    {
        const auto breath = saved.mNativeActorBreath.begin()->second;
        saved.mNativeActorBreath.clear();
        saved.mNativeActorBreath.emplace(key, breath);
    }
    saved.mNativeActorValues.front().mValues[8].mModifiers[2] = -9;
    ESM4::RuntimeInventoryItem item;
    item.mBase = ESM::FormKey::content("headless.esm", 0x940);
    item.mCount = 1;
    item.mCondition = 43.125f;
    item.mCharge = 7.25f;
    item.mEquippedSlots = ESM4::InventorySlotWeapon;
    saved.mReferences.front().mInventory = {item};
    ASSERT_NO_THROW(saved.validate());
    world.clear();
    world.getStore().rebuildIdsIndex(); // Shared record restoration rebuilds the projected item index.
    acceptNativeSnapshot(fixture, saved);
    MWWorld::Ptr actor;
    world.getWorldModel().forEachLoadedCellStore([&](MWWorld::CellStore& cell) {
        cell.forEach([&](const MWWorld::Ptr& ptr) {
            if (ptr.getCellRef().getFormKey() == key) actor = ptr;
            return true;
        }, true);
    });
    ASSERT_FALSE(actor.isEmpty());
    EXPECT_EQ(actor.getType(), ESM::REC_NPC_4);
    EXPECT_EQ(world.captureOblivionActorInventory(actor), saved.mReferences.front().mInventory);
    EXPECT_EQ(world.getOblivionCombatService()->findActorValues(key)->mValues[8].mModifiers[2], -9);
    EXPECT_EQ(actor.getClass().getCreatureStats(actor).getDrawState(), MWMechanics::DrawState::Weapon);
    EXPECT_EQ(world.getWorldModel().getPtr(actor.getCellRef().getRefNum()), actor);
    EXPECT_EQ(world.getOblivionAiService()->resolveReference(key), actor);
}

TEST(OblivionWorldTest, DynamicNativeActorBountyAndModifierViewsSurviveRepeatedClearAndReconstruction)
{
    for (const bool creature : {false, true})
    for (const auto process : {ESM4::ActorValueProcess::Low, ESM4::ActorValueProcess::Active})
    {
        SCOPED_TRACE(creature);
        SCOPED_TRACE(static_cast<int>(process));
        PopulatedMigrationFixture fixture;
        auto& world = fixture.mWorld;
        auto source = fixture.mActor;
        if (creature)
        {
            source = addEquipmentCreature(fixture);
            source = source.getCell()->moveTo(source, fixture.mActor.getCell());
            world.getWorldModel().registerPtr(source);
        }
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(source, process));
        const auto originalKey = source.getCellRef().getFormKey();
        auto& service = *world.getOblivionCombatService();
        auto values = *service.findActorValues(originalKey);
        values.mBounty = ESM4::CrimeBountyState{10.5f, 0.f};
        values.mValues[37].mModifiers = {.5f, -3.25f, -.25f};
        service.publishNonPlayerValues(source, values);
        auto saved = world.captureOblivionRuntimeState();
        const auto key = ESM::FormKey::dynamic("native-reference", 1);
        saved.mNextDynamicSerial = 2;
        for (auto& reference : saved.mReferences)
            if (reference.mKey == originalKey) reference.mKey = key;
        for (auto& actor : saved.mNativeActorValues)
            if (actor.mActor == originalKey) actor.mActor = key;
        for (auto& life : saved.mNativeActorLife)
            if (life.mActor == originalKey) life.mActor = key;
        if (const auto found = saved.mNativeActorBreath.find(originalKey); found != saved.mNativeActorBreath.end())
        {
            const auto breath = found->second;
            saved.mNativeActorBreath.erase(found);
            saved.mNativeActorBreath.emplace(key, breath);
        }
        std::sort(saved.mReferences.begin(), saved.mReferences.end(),
            [](const auto& a, const auto& b) { return a.mKey < b.mKey; });
        std::sort(saved.mNativeActorValues.begin(), saved.mNativeActorValues.end(),
            [](const auto& a, const auto& b) { return a.mActor < b.mActor; });
        std::sort(saved.mNativeActorLife.begin(), saved.mNativeActorLife.end(),
            [](const auto& a, const auto& b) { return a.mActor < b.mActor; });
        values.mActor = key;
        for (int restart = 0; restart != 2; ++restart)
        {
            SCOPED_TRACE(restart);
            const bool loadedEnabled = restart == 0;
            for (auto& reference : saved.mReferences)
                if (reference.mKey == key) reference.mEnabled = loadedEnabled;
            saved = ESM4::RuntimeState::deserializeBinary(saved.serializeBinary());
            world.clear();
            world.getStore().rebuildIdsIndex();
            if (creature)
            {
                // The fixture's other NPC was placed manually, not through
                // CELL content loading. Restore that content binding as the
                // save loader does; the dynamic target must reconstruct itself.
                prepareNativeSnapshotPlayer(fixture, saved);
                const auto reference = std::find_if(saved.mReferences.begin(), saved.mReferences.end(),
                    [](const auto& value) { return value.mKey == ESM::FormKey::content("headless.esm", 0x900); });
                ASSERT_NE(reference, saved.mReferences.end());
                const ESM::FormKeyResolver resolver({"headless.esm"});
                auto& cell = world.getWorldModel().getCell(ESM::RefId(*resolver.toFormId(reference->mCell)));
                const auto* placed = world.getStore().search<ESM4::ActorCharacter>(reference->mKey);
                ASSERT_NE(placed, nullptr);
                MWWorld::LiveCellRef<ESM4::Npc> live(*placed, world.getStore().search<ESM4::Npc>(reference->mBase));
                fixture.mActor = MWWorld::Ptr(cell.insert(&live), &cell);
                world.getWorldModel().registerPtr(fixture.mActor);
            }
            acceptNativeSnapshot(fixture, saved);
            const auto actor = world.getWorldModel().getDynamicNativePtr(key);
            ASSERT_FALSE(actor.isEmpty());
            ASSERT_NE(service.findActorValues(key), nullptr);
            EXPECT_EQ(*service.findActorValues(key), values); // All72 channels and owned fields.
            EXPECT_EQ(service.crimeBounty(key), 10.5f);
            EXPECT_EQ(service.getNonPlayerValue(actor, 37), process == ESM4::ActorValueProcess::Low ? -3.5f : -3.f);
            EXPECT_EQ(service.getNonPlayerIntegerValue(actor, 37), process == ESM4::ActorValueProcess::Low ? -3 : -2);
            EXPECT_EQ(world.getOblivionScriptActorValue(key, 37, true), 10);
            const double current = process == ESM4::ActorValueProcess::Low ? -3.5 : -3.;
            EXPECT_EQ(actor.getRefData().isEnabled(), loadedEnabled);
            EXPECT_EQ(world.getOblivionScriptActorValue(key, 37, false), loadedEnabled ? current : 0.);
            actor.getRefData().enable();
            EXPECT_EQ(world.getOblivionScriptActorValue(key, 37, false), current);
            actor.getRefData().disable();
            EXPECT_EQ(world.getOblivionScriptActorValue(key, 37, false), 0.);
            EXPECT_EQ(world.getOblivionScriptActorValue(key, 37, true), 10);
            EXPECT_EQ(service.crimeBounty(key), 10.5f);
            actor.getRefData().enable();
            EXPECT_EQ(world.getOblivionScriptActorValue(key, 37, false), current);
            EXPECT_EQ(world.getOblivionAiService()->resolveReference(key), actor);
            if (!creature)
            {
                auto& stats = actor.getClass().getNpcStats(actor);
                EXPECT_EQ(stats.getBounty(), 10);
                EXPECT_EQ(stats.getReputation(), 0);
                EXPECT_THROW(stats.setBounty(99), std::logic_error);
                EXPECT_THROW(stats.setReputation(99), std::logic_error);
            }
            saved = world.captureOblivionRuntimeState();
            const auto captured = std::find_if(saved.mNativeActorValues.begin(), saved.mNativeActorValues.end(),
                [&](const auto& value) { return value.mActor == key; });
            ASSERT_NE(captured, saved.mNativeActorValues.end());
            EXPECT_EQ(*captured, values);
        }
    }
}

TEST(OblivionWorldTest, NativeDynamicReconstructionDiscardsNodesOnLateFailureBeforePublication)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    MWClass::ESM4Takeable<ESM4::Weapon>::registerSelf();
    const auto before = world.captureOblivionRuntimeState();
    const auto serial = world.getWorldModel().getLastGeneratedRefNum();
    auto candidate = before;
    candidate.mNextDynamicSerial = 2;
    auto reference = candidate.mReferences.front();
    reference.mKey = ESM::FormKey::dynamic("native-reference", 1);
    reference.mBase = ESM::FormKey::content("headless.esm", 0x940);
    reference.mActorDrawState.reset();
    candidate.mReferences.push_back(reference);
    candidate.mPlayer.mInventory.push_back({});
    candidate.mPlayer.mInventory.back().mBase = ESM::FormKey::content("headless.esm", 0xdead);
    candidate.mPlayer.mInventory.back().mCount = 1;
    for (int retry = 0; retry != 2; ++retry)
    {
        readNativeSnapshot(fixture, candidate);
        EXPECT_ANY_THROW(world.applyOblivionRuntimeState());
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before.serializeBinary());
        EXPECT_EQ(world.getWorldModel().getLastGeneratedRefNum(), serial);
        EXPECT_TRUE(world.getWorldModel().getDynamicNativePtr(reference.mKey).isEmpty());
    }
    candidate.mPlayer.mInventory.clear();
    readNativeSnapshot(fixture, candidate);
    EXPECT_NO_THROW(world.applyOblivionRuntimeState());
    EXPECT_EQ(world.captureOblivionRuntimeState().mReferences.size(), before.mReferences.size() + 1);
}

namespace
{
    MWWorld::CellStore& nativeMoveTestCell(NativeWorldFixture& fixture, std::uint32_t id)
    {
        ESM4::Cell cell{};
        cell.mId = ESM::RefId(ESM::FormId{id, 0});
        cell.mFormKey = ESM::FormKey::content("headless.esm", id);
        cell.mCellFlags = ESM4::CELL_Interior;
        cell.mEditorId = "NativePreparedMoveCell";
        fixture.mWorld.getStore().getWritable<ESM4::Cell>().insertStatic(cell, cell.mFormKey);
        return fixture.mWorld.getWorldModel().getCell(cell.mId);
    }

    std::map<ESM::RefNum, ESM::RefId> savedMovedReferenceTags(const MWWorld::CellStore& cell)
    {
        auto stream = std::make_unique<std::stringstream>();
        ESM::ESMWriter writer;
        writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion);
        writer.save(*stream);
        writer.startRecord(ESM::REC_CSTA);
        cell.writeReferences(writer);
        writer.endRecord(ESM::REC_CSTA);
        ESM::ESMReader reader;
        reader.open(std::move(stream), "prepared-cell-move-tags");
        reader.getRecName();
        reader.getRecHeader();
        std::map<ESM::RefNum, ESM::RefId> result;
        while (reader.hasMoreSubs())
        {
            if (reader.isNextSub("MVRF"))
            {
                reader.cacheSubName();
                const auto id = reader.getFormId(true, "MVRF");
                result.emplace(id, reader.getCellId());
            }
            else
            {
                reader.getSubName();
                reader.skipHSub();
            }
        }
        return result;
    }
}

TEST(OblivionWorldTest, PreparedCellMovesPreserveOriginalOwnershipAcrossTwoDestinationsAndReturn)
{
    NativeWorldFixture fixture;
    auto ptr = installNativeLooseItemCapture(fixture);
    auto& origin = *ptr.getCell();
    auto& second = nativeMoveTestCell(fixture, 2);
    auto& third = nativeMoveTestCell(fixture, 3);
    auto& model = fixture.mWorld.getWorldModel();
    const auto id = ptr.getCellRef().getRefNum();
    const auto serial = model.getLastGeneratedRefNum();
    const auto originalCount = origin.count();
    const auto secondHasState = second.hasState();
    const auto thirdHasState = third.hasState();
    for (auto* target : {&second, &third, &origin})
    {
        SCOPED_TRACE(target->getCell()->getId().toDebugString());
        const auto old = ptr;
        const auto before = savedMovedReferenceTags(origin);
        const std::array changes{std::pair{ptr, target}};
        {
            auto abandoned = MWWorld::CellStore::prepareMoves(changes);
            EXPECT_TRUE(abandoned.isValid());
            EXPECT_EQ(savedMovedReferenceTags(origin), before);
            EXPECT_EQ(model.getPtr(id), old);
            EXPECT_EQ(model.getLastGeneratedRefNum(), serial);
            if (target == &second)
            {
                EXPECT_EQ(second.hasState(), secondHasState);
                EXPECT_EQ(third.hasState(), thirdHasState);
            }
        }
        EXPECT_EQ(model.getPtr(id), old);
        auto cells = MWWorld::CellStore::prepareMoves(changes);
        const std::array relocated{MWWorld::Ptr(ptr.getBase(), target)};
        auto registry = model.preparePtrReplacement({}, {}, relocated);
        EXPECT_TRUE(cells.isValid());
        EXPECT_TRUE(registry.isValid());
        EXPECT_TRUE(cells.commit());
        EXPECT_FALSE(cells.commit());
        registry.commit();
        ptr = relocated.front();
        EXPECT_EQ(model.getPtr(id), ptr);
        EXPECT_EQ(model.getLastGeneratedRefNum(), serial);
        EXPECT_EQ(origin.count(), target == &origin ? originalCount : originalCount - 1);
        EXPECT_EQ(second.count(), target == &second ? 1u : 0u);
        EXPECT_EQ(third.count(), target == &third ? 1u : 0u);
        const auto tags = savedMovedReferenceTags(origin);
        if (target == &origin)
            EXPECT_TRUE(tags.empty());
        else
        {
            ASSERT_EQ(tags.size(), 1);
            EXPECT_EQ(tags.at(id), target->getCell()->getId());
        }
        EXPECT_TRUE(savedMovedReferenceTags(second).empty());
        EXPECT_TRUE(savedMovedReferenceTags(third).empty());
    }
}

TEST(OblivionWorldTest, PreparedCellMovesRejectStaleForeignDuplicateAndDestroyedOwners)
{
    NativeWorldFixture fixture;
    const auto ptr = installNativeLooseItemCapture(fixture);
    auto& target = nativeMoveTestCell(fixture, 2);
    const std::array changes{std::pair{ptr, &target}};
    auto stale = MWWorld::CellStore::prepareMoves(changes);
    const auto moved = ptr.getCell()->moveTo(ptr, &target);
    EXPECT_FALSE(stale.isValid());
    EXPECT_FALSE(stale.commit());
    EXPECT_EQ(fixture.mWorld.getWorldModel().getPtr(ptr.getCellRef().getRefNum()), moved);
    EXPECT_THROW(MWWorld::CellStore::prepareMoves(changes), std::invalid_argument);
    const std::array duplicate{std::pair{moved, ptr.getCell()}, std::pair{moved, ptr.getCell()}};
    EXPECT_THROW(MWWorld::CellStore::prepareMoves(duplicate), std::invalid_argument);
    const std::array same{std::pair{moved, &target}};
    EXPECT_THROW(MWWorld::CellStore::prepareMoves(same), std::invalid_argument);
    ESM::ReadersCache readers;
    ESM::Cell record{}; record.blank(); record.mId = ESM::RefId::stringRefId("PreparedMoveLifetime");
    alignas(MWWorld::CellStore) std::array<std::byte, sizeof(MWWorld::CellStore)> storage{};
    auto* other = std::construct_at(reinterpret_cast<MWWorld::CellStore*>(storage.data()),
        MWWorld::Cell(record), fixture.mWorld.getStore(), readers);
    const std::array unloaded{std::pair{moved, other}};
    EXPECT_THROW(MWWorld::CellStore::prepareMoves(unloaded), std::invalid_argument);
    other->load();
    auto lifetime = MWWorld::CellStore::prepareMoves(unloaded);
    std::destroy_at(other);
    EXPECT_FALSE(lifetime.isValid());
    EXPECT_FALSE(lifetime.commit());
    other = std::construct_at(reinterpret_cast<MWWorld::CellStore*>(storage.data()),
        MWWorld::Cell(record), fixture.mWorld.getStore(), readers);
    other->load();
    EXPECT_FALSE(lifetime.isValid());
    EXPECT_FALSE(lifetime.commit());
    EXPECT_EQ(other->count(), 0u);
    std::destroy_at(other);
}

TEST(OblivionWorldTest, NativeActorCellRestorePreparesTrackingAndRegistryBeforeLateFailure)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    const auto actor = fixture.mActor;
    const auto actorId = actor.getCellRef().getRefNum();
    ASSERT_TRUE(world.initializeOblivionNonPlayerActor(actor, ESM4::ActorValueProcess::Active));
    auto& inventory = actor.getClass().getInventoryStore(actor);
    const auto owner = inventory.getPtr();
    auto& target = nativeMoveTestCell(fixture, 2);
    const auto before = world.captureOblivionRuntimeState();
    const auto revision = world.getWorldModel().getPtrRegistryRevision();
    const auto serial = world.getWorldModel().getLastGeneratedRefNum();
    auto incoming = before;
    ASSERT_EQ(incoming.mReferences.size(), 1);
    incoming.mReferences.front().mCell = ESM::FormKey::content("headless.esm", 2);
    incoming.mPlayer.mInventory.push_back({});
    incoming.mPlayer.mInventory.back().mBase = ESM::FormKey::content("headless.esm", 0xdead);
    incoming.mPlayer.mInventory.back().mCount = 1;
    for (int retry = 0; retry != 2; ++retry)
    {
        readNativeSnapshot(fixture, incoming);
        EXPECT_ANY_THROW(world.applyOblivionRuntimeState());
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before.serializeBinary());
        EXPECT_EQ(world.getWorldModel().getPtr(actorId), actor);
        EXPECT_EQ(inventory.getPtr(), owner);
        EXPECT_EQ(world.getWorldModel().getPtrRegistryRevision(), revision);
        EXPECT_EQ(world.getWorldModel().getLastGeneratedRefNum(), serial);
        EXPECT_EQ(target.count(), 0u);
    }
    incoming.mPlayer.mInventory.clear();
    readNativeSnapshot(fixture, incoming);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    const auto restored = world.getWorldModel().getPtr(actorId);
    EXPECT_EQ(restored.getBase(), actor.getBase());
    EXPECT_EQ(restored.getCell(), &target);
    EXPECT_EQ(inventory.getPtr(), restored);
    EXPECT_EQ(world.getOblivionAiService()->resolveReference(incoming.mReferences.front().mKey), restored);
    EXPECT_EQ(world.captureOblivionRuntimeState().mReferences.front().mCell, incoming.mReferences.front().mCell);
    readNativeSnapshot(fixture, incoming);
    EXPECT_NO_THROW(world.applyOblivionRuntimeState());
    EXPECT_EQ(world.getWorldModel().getPtr(actorId), restored);
}

TEST(OblivionWorldTest, PreparedCellRelocationRegistersColdReferencesAndUpdatesDynamicNativeIndex)
{
    NativeWorldFixture fixture;
    const auto item = installNativeLooseItemCapture(fixture);
    auto& origin = *item.getCell();
    auto& target = nativeMoveTestCell(fixture, 2);
    auto& model = fixture.mWorld.getWorldModel();
    const auto key = ESM::FormKey::dynamic("native-reference", 1);
    auto source = makeDetachedNativeLooseItem(item, key);
    auto insertion = origin.prepareInsertion(source);
    const auto ptr = MWWorld::Ptr(origin.commitPreparedInsertion(*insertion), &origin);
    model.registerPtr(ptr);
    const auto id = ptr.getCellRef().getRefNum();
    const auto serial = model.getLastGeneratedRefNum();
    for (bool cold : {false, true})
    {
        SCOPED_TRACE(cold);
        const auto old = cold ? model.getPtr(id) : ptr;
        auto* destination = cold ? &origin : &target;
        if (cold)
        {
            model.deregisterLiveCellRef(*old.mRef);
            EXPECT_TRUE(model.getPtr(id).isEmpty());
            EXPECT_TRUE(model.getDynamicNativePtr(key).isEmpty());
        }
        const std::array moves{std::pair{old, destination}};
        auto cells = MWWorld::CellStore::prepareMoves(moves);
        const std::array relocated{MWWorld::Ptr(old.getBase(), destination)};
        auto registry = model.preparePtrReplacement({}, {}, relocated);
        EXPECT_EQ(old.mRef->mWorldModel, cold ? nullptr : &model);
        EXPECT_EQ(model.getLastGeneratedRefNum(), serial);
        EXPECT_TRUE(registry.isValid());
        EXPECT_TRUE(cells.isValid());
        static_assert(noexcept(cells.isValid()) && noexcept(cells.commit()));
        ASSERT_TRUE(cells.commit());
        registry.commit();
        EXPECT_EQ(model.getPtr(id), relocated.front());
        EXPECT_EQ(model.getDynamicNativePtr(key), relocated.front());
        EXPECT_EQ(old.mRef->mWorldModel, &model);
        EXPECT_EQ(model.getLastGeneratedRefNum(), serial);
        EXPECT_THROW(model.preparePtrReplacement({}, {},
            std::array{relocated.front(), relocated.front()}), std::invalid_argument);
        EXPECT_EQ(model.getPtr(id), relocated.front());
    }
}

TEST(OblivionWorldTest, PreparedCellMovesRejectReferenceAddressReuseAndChangedBaseIdentity)
{
    for (bool changeBase : {false, true})
    {
        SCOPED_TRACE(changeBase);
        NativeWorldFixture fixture;
        const auto item = installNativeLooseItemCapture(fixture);
        auto& target = nativeMoveTestCell(fixture, 2);
        const std::array moves{std::pair{item, &target}};
        auto prepared = MWWorld::CellStore::prepareMoves(moves);
        ASSERT_TRUE(prepared.isValid());
        auto* node = item.get<ESM4::Weapon>();
        auto replacement = *node;
        if (changeBase)
        {
            ESM4::Reference reference{};
            reference.mId = node->mRef.getRefNum();
            reference.mFormKey = node->mRef.getFormKey();
            reference.mBaseObj = {0x941, 0};
            reference.mBaseKey = ESM::FormKey::content("headless.esm", 0x941);
            replacement.mRef = MWWorld::CellRef(reference);
        }
        else
            replacement.mRef.setRefNum(ESM::RefNum{0x981, 0});
        replacement.mWorldModel = nullptr;
        std::destroy_at(node);
        std::construct_at(node, replacement);
        EXPECT_FALSE(prepared.isValid());
        EXPECT_FALSE(prepared.commit());
        EXPECT_EQ(target.count(), 0u);
        if (!changeBase)
        {
            EXPECT_TRUE(savedMovedReferenceTags(*item.getCell()).empty());
        }
    }
}

namespace
{
    void restorePreparedSaveActorFixture(PopulatedMigrationFixture& fixture, const ESM4::RuntimeState& state)
    {
        // The headless fixture authors its placed actor directly in a CellStore.
        // Supply that shared-cell binding again after clear, before native apply,
        // just as StateManager restores CSTA before saveLoaded in the real engine.
        prepareNativeSnapshotPlayer(fixture, state);
        auto& world = fixture.mWorld;
        const auto& actor = state.mReferences.front();
        const ESM::FormKeyResolver resolver({"headless.esm"});
        auto& cell = world.getWorldModel().getCell(ESM::RefId(*resolver.toFormId(actor.mCell)));
        const auto* reference = world.getStore().search<ESM4::ActorCharacter>(actor.mKey);
        MWWorld::LiveCellRef<ESM4::Npc> live(*reference, world.getStore().search<ESM4::Npc>(actor.mBase));
        fixture.mActor = MWWorld::Ptr(cell.insert(&live), &cell);
        world.getWorldModel().registerPtr(fixture.mActor);
    }
}

TEST(OblivionWorldTest, PreparedNativeSaveDiscardsAndInstallsAfterExactlyOneClearWithoutEarlyPublication)
{
    PopulatedMigrationFixture fixture(true);
    auto& world = fixture.mWorld;
    ASSERT_TRUE(world.getOblivionScriptManager()->dispatchObjectEvent(fixture.mActor, "onactivate", world.getPlayerPtr()));
    auto saved = populatedAiRestore(fixture);
    ESM4::AIPackage second = *world.getStore().search<ESM4::AIPackage>(saved.mPendingPackageDone.front().mPackage);
    second.mId = {0xa31, 0};
    second.mFormKey = ESM::FormKey::content("headless.esm", 0xa31);
    world.getStore().getWritable<ESM4::AIPackage>().insertStatic(second, second.mFormKey);
    saved.mPendingPackageDone.insert(saved.mPendingPackageDone.begin() + 1,
        {saved.mPendingPackageDone.front().mActor, second.mFormKey});
    ASSERT_TRUE(world.activateOblivionActor(fixture.mActor));
    saved.mNativeActorValues = world.captureOblivionRuntimeState().mNativeActorValues;
    // The incoming disabled mask deliberately equals the outgoing mask.
    world.getOblivionAiService()->restore(saved);
    const auto before = world.captureOblivionRuntimeState().serializeBinary();
    auto dynamicKey = world.prepareOblivionDynamicReferenceKey();
    {
        auto discarded = world.prepareOblivionSaveState(saved);
        EXPECT_FALSE(discarded->install());
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before);
        EXPECT_TRUE(dynamicKey->isValid());
    }
    auto prepared = world.prepareOblivionSaveState(saved);
    EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before);
    world.clear();
    EXPECT_FALSE(dynamicKey->isValid());
    EXPECT_TRUE(world.getStore().getOblivionPathgridService().disabledNodes().empty());
    ASSERT_TRUE(prepared->install());
    EXPECT_FALSE(prepared->install());
    ESM4::RuntimeState unpublished;
    world.getOblivionAiService()->capture(unpublished);
    EXPECT_TRUE(unpublished.mPathPoints.empty());
    EXPECT_TRUE(unpublished.mPendingPackageDone.empty());
    ESM4::RuntimeState unpublishedCombat;
    world.getOblivionCombatService()->capture(unpublishedCombat);
    EXPECT_TRUE(unpublishedCombat.mNativeActorValues.empty());
    restorePreparedSaveActorFixture(fixture, saved);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    const auto restored = world.captureOblivionRuntimeState();
    EXPECT_EQ(restored.mPathPoints, saved.mPathPoints);
    EXPECT_EQ(restored.mPendingPackageDone, saved.mPendingPackageDone);
    EXPECT_EQ(restored.mDetectionVectors, saved.mDetectionVectors);
    EXPECT_EQ(restored.mActorAi, saved.mActorAi);
    EXPECT_EQ(restored.mScriptInstances, saved.mScriptInstances);
    EXPECT_EQ(restored.mScriptEventSequence, saved.mScriptEventSequence);
    EXPECT_EQ(restored.mNativeActorValues, saved.mNativeActorValues);
    const auto accepted = restored.serializeBinary();
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), accepted);
}

TEST(OblivionWorldTest, PreparedNativeSaveRejectsStaleClearAndDestroyedWorldAndConflictingPendingState)
{
    std::unique_ptr<MWBase::World::PreparedOblivionSaveState> orphan;
    {
        PopulatedMigrationFixture fixture;
        auto& world = fixture.mWorld;
        const auto saved = world.captureOblivionRuntimeState();
        auto stale = world.prepareOblivionSaveState(saved);
        world.clear();
        world.clear();
        EXPECT_FALSE(stale->install());
        auto first = world.prepareOblivionSaveState(saved);
        auto second = world.prepareOblivionSaveState(saved);
        world.clear();
        ASSERT_TRUE(first->install());
        EXPECT_FALSE(second->install());
        orphan = world.prepareOblivionSaveState(saved);
    }
    EXPECT_FALSE(orphan->install());
}

TEST(OblivionWorldTest, PreparedNativeSaveRejectsBadBindingsBeforeClearAndDirectReadSupersedesPlans)
{
    PopulatedMigrationFixture fixture(true);
    auto& world = fixture.mWorld;
    ASSERT_TRUE(world.getOblivionScriptManager()->dispatchObjectEvent(fixture.mActor, "onactivate", world.getPlayerPtr()));
    const auto saved = populatedAiRestore(fixture);
    const auto before = world.captureOblivionRuntimeState().serializeBinary();
    for (int fault = 0; fault < 3; ++fault)
    {
        SCOPED_TRACE(fault);
        auto candidate = saved;
        if (fault == 0) candidate.mScriptInstances.front().mUnit = "missing-program";
        if (fault == 1) candidate.mPathPoints.front().mNode = 99;
        if (fault == 2) candidate.mNativeDeathCounts[ESM::FormKey::content("headless.esm", 0x940)] = 1;
        EXPECT_ANY_THROW(world.prepareOblivionSaveState(candidate));
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before);
    }
    auto prepared = world.prepareOblivionSaveState(saved);
    world.clear();
    ASSERT_TRUE(prepared->install());
    auto replacement = saved;
    replacement.mPathPoints.clear();
    replacement.mPendingPackageDone.clear();
    replacement.mScriptInstances.front().mLocals.front() = std::int64_t{9};
    restorePreparedSaveActorFixture(fixture, replacement);
    readNativeSnapshot(fixture, replacement);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    const auto captured = world.captureOblivionRuntimeState();
    EXPECT_TRUE(captured.mPathPoints.empty());
    EXPECT_TRUE(captured.mPendingPackageDone.empty());
    EXPECT_EQ(captured.mScriptInstances, replacement.mScriptInstances);
}

TEST(OblivionWorldTest, PreparedIncomingDefinitionsRebindPlayerAndKeepWinningItemAddressesWithoutRecordReplay)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    const auto saved = world.captureOblivionRuntimeState();
    const auto* oldPlayer = world.getPlayerPtr().get<ESM::NPC>()->mBase;
    auto definitions = std::make_unique<MWWorld::ESMStore>();
    for (int count = 0; count != 64; ++count) definitions->generateId();
    ESM::Class characterClass{}; characterClass.blank(); characterClass.mId = ESM::RefId::generated(42);
    characterClass.mName = "Incoming prepared class";
    definitions->getWritable<ESM::Class>().insert(characterClass);
    ESM::NPC player = *oldPlayer; player.mClass = characterClass.mId;
    const auto* incomingPlayer = definitions->getWritable<ESM::NPC>().insert(player);
    const auto id = ESM::RefId(ESM::FormId{0x940, 0});
    ESM::Weapon weapon = *world.getStore().get<ESM::Weapon>().searchStatic(id);
    weapon.mName = "Incoming prepared weapon";
    const auto* incomingWeapon = definitions->getWritable<ESM::Weapon>().insert(weapon);
    auto prepared = world.prepareOblivionSaveState(saved, std::move(definitions));
    EXPECT_EQ(world.getPlayerPtr().get<ESM::NPC>()->mBase, oldPlayer);
    EXPECT_NE(world.getStore().get<ESM::Weapon>().find(id), incomingWeapon);
    world.clear();
    ASSERT_TRUE(prepared->install());
    EXPECT_EQ(world.getPlayerPtr().get<ESM::NPC>()->mBase, incomingPlayer);
    EXPECT_EQ(world.getPlayerPtr().get<ESM::NPC>()->mBase->mClass, characterClass.mId);
    EXPECT_EQ(world.getStore().get<ESM::Weapon>().find(id), incomingWeapon);
    EXPECT_EQ(world.getStore().generateId(), ESM::RefId::generated(64));
    // The main record scan must skip the already published definition. A
    // deliberately different payload detects accidental second restoration.
    auto stream = std::make_unique<std::stringstream>();
    ESM::ESMWriter writer; writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion); writer.save(*stream);
    weapon.mName = "Unexpected second decode";
    writer.startRecord(ESM::REC_WEAP); weapon.save(writer); writer.endRecord(ESM::REC_WEAP);
    ESM::ESMReader reader; reader.open(std::move(stream), "already-prepared-shared-definition");
    ASSERT_EQ(reader.getRecName(), ESM::REC_WEAP); reader.getRecHeader();
    world.readRecord(reader, ESM::REC_WEAP);
    EXPECT_EQ(world.getStore().get<ESM::Weapon>().find(id), incomingWeapon);
    EXPECT_EQ(incomingWeapon->mName, "Incoming prepared weapon");
    restorePreparedSaveActorFixture(fixture, saved);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    EXPECT_EQ(world.getPlayerPtr().get<ESM::NPC>()->mBase->mClass, characterClass.mId);
    EXPECT_EQ(world.getStore().get<ESM::Weapon>().find(id), incomingWeapon);
    const auto accepted = world.captureOblivionRuntimeState().serializeBinary();
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), accepted);
}

TEST(OblivionWorldTest, PreparedActorInventoriesSurviveOutgoingOverridesAndPreserveEquipmentExtras)
{
    for (const bool incomingOverride : {false, true})
    {
        SCOPED_TRACE(incomingOverride);
        PopulatedMigrationFixture fixture;
        auto& world = fixture.mWorld;
        const auto id = ESM::RefId(ESM::FormId{0x940, 0});
        const auto* immutable = world.getStore().get<ESM::Weapon>().searchStatic(id);
        ESM::Weapon outgoing = *immutable; outgoing.mName = "Deleted outgoing weapon";
        world.getStore().getWritable<ESM::Weapon>().insert(outgoing);
        ESM4::RuntimeInventoryItem item;
        item.mBase = ESM::FormKey::content("headless.esm", 0x940);
        item.mCount = 1; item.mCondition = 43.125f; item.mCharge = 7.25f;
        item.mEquippedSlots = ESM4::InventorySlotWeapon;
        item.mOwner = ESM::FormKey::content("headless.esm", 0x800);
        installEquipmentInventory(fixture, fixture.mActor, {item});
        installEquipmentInventory(fixture, world.getPlayerPtr(), {item});
        const auto saved = world.captureOblivionRuntimeState();
        auto definitions = std::make_unique<MWWorld::ESMStore>();
        const ESM::Weapon* accepted = immutable;
        if (incomingOverride)
        {
            auto weapon = *immutable; weapon.mName = "Retained incoming weapon";
            accepted = definitions->getWritable<ESM::Weapon>().insert(weapon);
        }
        const auto before = world.captureOblivionRuntimeState().serializeBinary();
        auto plan = world.prepareOblivionSaveState(saved, std::move(definitions));
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before);
        world.clear();
        ASSERT_TRUE(plan->install());
        restorePreparedSaveActorFixture(fixture, saved);
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        for (const auto actor : {world.getPlayerPtr(), fixture.mActor})
        {
            auto& inventory = actor.getClass().getInventoryStore(actor);
            const auto held = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
            ASSERT_NE(held, inventory.end());
            const MWWorld::Ptr ptr = *held;
            EXPECT_EQ(ptr.get<ESM::Weapon>()->mBase, accepted);
            EXPECT_EQ(ptr.getCellRef().getNativeItemCondition(), item.mCondition);
            EXPECT_EQ(ptr.getCellRef().getEnchantmentCharge(), item.mCharge);
            EXPECT_EQ(ptr.getCellRef().getOwner(), ESM::RefId(ESM::FormId{0x800, 0}));
            EXPECT_EQ(ptr.getContainerStore(), &inventory);
            EXPECT_EQ(world.getWorldModel().getPtr(ptr.getCellRef().getRefNum()), ptr);
        }
        EXPECT_EQ(world.captureOblivionActorInventory(fixture.mActor), saved.mReferences.front().mInventory);
        EXPECT_EQ(world.captureOblivionRuntimeState().mPlayer.mInventory, saved.mPlayer.mInventory);
    }
}

TEST(OblivionWorldTest, PreparedInventoriesMatchDirectMigrationForEveryAcceptedVersion)
{
    for (std::uint32_t version = 1; version <= ESM4::CurrentRuntimeStateVersion; ++version)
    {
        SCOPED_TRACE(version);
        PopulatedMigrationFixture fixture;
        auto& world = fixture.mWorld;
        auto legacy = world.captureOblivionRuntimeState();
        legacy.mVersion = version;
        if (version < 3)
        {
            legacy.mPlayer.mName.clear(); legacy.mPlayer.mRace = {}; legacy.mPlayer.mClass = {};
            legacy.mPlayer.mBirthSign = {}; legacy.mPlayer.mFemale = false;
            legacy.mPlayer.mCharacterGenerationFlags = 0;
        }
        ESM4::RuntimeInventoryItem item;
        item.mBase = ESM::FormKey::content("headless.esm", 0x940);
        item.mCount = version < 4 ? 3 : 1;
        if (version >= 4)
        {
            item.mCondition = version < 24 ? 43.f : 43.125f;
            item.mCharge = 7.25f; item.mEquippedSlots = ESM4::InventorySlotWeapon;
            item.mOwner = ESM::FormKey::content("headless.esm", 0x800);
        }
        legacy.mPlayer.mInventory = {item};
        legacy.mReferences.front().mInventory = {item};
        const auto decoded = ESM4::RuntimeState::deserializeBinary(legacy.serializeBinary());
        readNativeSnapshot(fixture, decoded);
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        const auto expectedPlayer = world.captureOblivionActorInventory(world.getPlayerPtr());
        const auto expectedNpc = world.captureOblivionActorInventory(fixture.mActor);
        const auto before = world.captureOblivionRuntimeState().serializeBinary();
        auto invalid = decoded;
        invalid.mReferences.front().mInventory.front().mBase = ESM::FormKey::content("headless.esm", 0xdead);
        EXPECT_ANY_THROW(world.prepareOblivionSaveState(invalid));
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before);
        auto plan = world.prepareOblivionSaveState(decoded);
        world.clear();
        ASSERT_TRUE(plan->install());
        restorePreparedSaveActorFixture(fixture, decoded);
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        EXPECT_EQ(world.captureOblivionActorInventory(world.getPlayerPtr()), expectedPlayer);
        EXPECT_EQ(world.captureOblivionActorInventory(fixture.mActor), expectedNpc);
    }
}

TEST(OblivionWorldTest, PreparedCreatureInventorySurvivesClearAndDynamicReconstruction)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    const auto id = ESM::RefId(ESM::FormId{0x940, 0});
    auto creature = addEquipmentCreature(fixture);
    creature = creature.getCell()->moveTo(creature, fixture.mActor.getCell());
    world.getWorldModel().registerPtr(creature);
    ESM4::RuntimeInventoryItem item;
    item.mBase = ESM::FormKey::content("headless.esm", 0x940);
    item.mCount = 1; item.mCondition = 23.125f; item.mCharge = 4.25f;
    item.mEquippedSlots = ESM4::InventorySlotWeapon;
    installEquipmentInventory(fixture, creature, {item});
    auto saved = world.captureOblivionRuntimeState();
    ASSERT_EQ(saved.mReferences.size(), 2u);
    if (saved.mReferences.front().mBase == ESM::FormKey::content("headless.esm", 0x820))
        std::swap(saved.mReferences.front(), saved.mReferences.back());
    auto& reference = saved.mReferences.back();
    reference.mKey = ESM::FormKey::dynamic("native-reference", 1);
    saved.mNextDynamicSerial = 2;
    auto definitions = std::make_unique<MWWorld::ESMStore>();
    auto weapon = *world.getStore().get<ESM::Weapon>().searchStatic(id);
    weapon.mName = "Retained creature weapon";
    const auto* accepted = definitions->getWritable<ESM::Weapon>().insert(weapon);
    auto plan = world.prepareOblivionSaveState(saved, std::move(definitions));
    world.clear();
    ASSERT_TRUE(plan->install());
    restorePreparedSaveActorFixture(fixture, saved);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    const auto restored = world.getWorldModel().getDynamicNativePtr(reference.mKey);
    ASSERT_FALSE(restored.isEmpty());
    ASSERT_EQ(restored.getType(), ESM::REC_CREA4);
    auto& inventory = restored.getClass().getInventoryStore(restored);
    const auto held = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
    ASSERT_NE(held, inventory.end());
    EXPECT_EQ((*held).get<ESM::Weapon>()->mBase, accepted);
    EXPECT_EQ(world.captureOblivionActorInventory(restored), reference.mInventory);
    EXPECT_EQ(world.getWorldModel().getPtr((*held).getCellRef().getRefNum()), *held);
}

TEST(OblivionWorldTest, GeneratedSharedActorGearCapturesAndSurvivesAdmissionClearAndRestore)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    const auto nativeId = ESM::RefId(ESM::FormId{0x940, 0});
    ESM::Weapon weapon = *world.getStore().get<ESM::Weapon>().searchStatic(nativeId);
    weapon.mId = world.getStore().generateId(); weapon.mName = "Generated persistent weapon";
    const auto generatedId = weapon.mId.getIf<ESM::GeneratedRefId>()->getValue();
    world.getStore().getWritable<ESM::Weapon>().insert(weapon);
    world.getStore().rebuildIdsIndex();
    ESM4::RuntimeInventoryItem item;
    item.mBase = ESM::FormKey::dynamic("shared-item", generatedId + 1);
    item.mCount = 1; item.mCondition = 37.125; item.mCharge = 9.25f;
    item.mOwner = ESM::FormKey::content("headless.esm", 0x800);
    item.mOwnershipRank = -2;
    item.mEquippedSlots = ESM4::InventorySlotWeapon;
    installEquipmentInventory(fixture, world.getPlayerPtr(), {item});
    installEquipmentInventory(fixture, fixture.mActor, {item});
    EXPECT_EQ(world.captureOblivionActorInventory(world.getPlayerPtr()), (std::vector{item}));
    EXPECT_EQ(world.captureOblivionActorInventory(fixture.mActor), (std::vector{item}));
    // The generated item was added to the live store after the last accepted
    // native snapshot. The public quickkey adapter must use that live view.
    ASSERT_TRUE(world.oblivionSetPlayerHotkey(weapon.mId, 3));
    item.mHotkey = 3;
    EXPECT_EQ(world.captureOblivionActorInventory(world.getPlayerPtr()), (std::vector{item}));
    auto actorItem = item; actorItem.mHotkey = -1;
    EXPECT_EQ(world.captureOblivionActorInventory(fixture.mActor), (std::vector{actorItem}));
    auto saved = world.captureOblivionRuntimeState();
    saved = ESM4::RuntimeState::deserializeBinary(saved.serializeBinary());
    const auto before = world.captureOblivionRuntimeState().serializeBinary();
    // Outgoing definitions cannot satisfy an incoming generated identity.
    EXPECT_ANY_THROW(world.prepareOblivionSaveState(saved, std::make_unique<MWWorld::ESMStore>()));
    EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before);
    auto incoming = std::make_unique<MWWorld::ESMStore>();
    for (std::uint64_t i = 0; i <= generatedId; ++i) incoming->generateId();
    weapon.mName = "Incoming persistent weapon";
    const auto* accepted = incoming->getWritable<ESM::Weapon>().insert(weapon);
    auto plan = world.prepareOblivionSaveState(saved, std::move(incoming));
    world.clear();
    ASSERT_TRUE(plan->install());
    restorePreparedSaveActorFixture(fixture, saved);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    for (const auto actor : {world.getPlayerPtr(), fixture.mActor})
    {
        auto& inventory = actor.getClass().getInventoryStore(actor);
        const auto held = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
        ASSERT_NE(held, inventory.end());
        EXPECT_EQ((*held).get<ESM::Weapon>()->mBase, accepted);
        EXPECT_EQ(world.getWorldModel().getPtr((*held).getCellRef().getRefNum()), *held);
    }
    const auto captured = world.captureOblivionRuntimeState();
    EXPECT_EQ(captured.mPlayer.mInventory, saved.mPlayer.mInventory);
    EXPECT_EQ(captured.mReferences.front().mInventory, saved.mReferences.front().mInventory);
    // Direct readers and repeated publication preserve the same generated item.
    readNativeSnapshot(fixture, captured);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    EXPECT_EQ(world.captureOblivionRuntimeState().mPlayer.mInventory, saved.mPlayer.mInventory);
    ASSERT_TRUE(world.oblivionSetPlayerHotkey(weapon.mId, 7));
    EXPECT_EQ(world.captureOblivionActorInventory(world.getPlayerPtr()).front().mHotkey, 7);
    ASSERT_TRUE(world.oblivionSetPlayerHotkey(weapon.mId, -1));
    EXPECT_EQ(world.captureOblivionActorInventory(world.getPlayerPtr()).front().mHotkey, -1);
    const auto unchanged = world.captureOblivionRuntimeState().serializeBinary();
    EXPECT_FALSE(world.oblivionSetPlayerHotkey(weapon.mId, 8));
    EXPECT_FALSE(world.oblivionSetPlayerHotkey(weapon.mId, -2));
    EXPECT_FALSE(world.oblivionSetPlayerHotkey(ESM::RefId::generated(generatedId + 1), 3));
    EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), unchanged);
    // Removing a live generated item invalidates a stale cached assignment.
    world.getPlayerPtr().getClass().getInventoryStore(world.getPlayerPtr()).clear();
    EXPECT_FALSE(world.oblivionSetPlayerHotkey(weapon.mId, 3));
    EXPECT_TRUE(world.captureOblivionActorInventory(world.getPlayerPtr()).empty());
}

TEST(OblivionWorldTest, HotkeySlotClearingUsesLiveInventoryWithoutClearingMovedOrOtherAssignments)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    ESM::Weapon weapon = *world.getStore().get<ESM::Weapon>().searchStatic(ESM::RefId(ESM::FormId{0x940, 0}));
    weapon.mId = world.getStore().generateId();
    const auto firstId = weapon.mId;
    const auto firstKey = ESM::FormKey::dynamic("shared-item", firstId.getIf<ESM::GeneratedRefId>()->getValue() + 1);
    world.getStore().getWritable<ESM::Weapon>().insert(weapon);
    weapon.mId = world.getStore().generateId();
    const auto secondId = weapon.mId;
    const auto secondKey = ESM::FormKey::dynamic("shared-item", secondId.getIf<ESM::GeneratedRefId>()->getValue() + 1);
    world.getStore().getWritable<ESM::Weapon>().insert(weapon);
    world.getStore().rebuildIdsIndex();
    ESM4::RuntimeInventoryItem first;
    first.mBase = firstKey; first.mCount = 1; first.mCondition = 37.125f; first.mCharge = 9.25f;
    auto second = first; second.mBase = secondKey; second.mCondition = 43.5f;
    installEquipmentInventory(fixture, world.getPlayerPtr(), {first, second});
    installEquipmentInventory(fixture, fixture.mActor, {first});
    ASSERT_TRUE(world.oblivionSetPlayerHotkey(firstId, 2));
    ASSERT_TRUE(world.oblivionSetPlayerHotkey(secondId, 4));
    ASSERT_TRUE(world.oblivionSetPlayerHotkey(firstId, 7));
    const auto before = world.captureOblivionRuntimeState();
    const auto hotkeys = world.oblivionPlayerItemHotkeys();
    ASSERT_TRUE(hotkeys);
    EXPECT_EQ((*hotkeys)[7], firstId);
    EXPECT_EQ((*hotkeys)[4], secondId);
    for (int slot : {0, 1, 2, 3, 5, 6}) EXPECT_TRUE((*hotkeys)[slot].empty());
    EXPECT_FALSE(world.oblivionSetPlayerHotkey({}, 2));
    for (int invalid : {-2, -1, 8}) EXPECT_FALSE(world.oblivionSetPlayerHotkey({}, invalid));
    EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before.serializeBinary());
    ASSERT_TRUE(world.oblivionSetPlayerHotkey({}, 7));
    auto expected = before;
    for (auto& item : expected.mPlayer.mInventory)
        if (item.mBase == firstKey) item.mHotkey = -1;
    const auto cleared = world.captureOblivionRuntimeState();
    EXPECT_EQ(cleared.serializeBinary(), expected.serializeBinary());
    EXPECT_EQ(world.captureOblivionActorInventory(world.getPlayerPtr()), expected.mPlayer.mInventory);
    EXPECT_EQ(world.captureOblivionActorInventory(fixture.mActor), (std::vector{first}));
    EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(cleared.serializeBinary()).mPlayer.mInventory,
        expected.mPlayer.mInventory);
    EXPECT_FALSE(world.oblivionSetPlayerHotkey({}, 7));
    EXPECT_TRUE(world.oblivionSetPlayerHotkey({}, 4));
    const auto inventory = world.captureOblivionActorInventory(world.getPlayerPtr());
    for (const auto& item : inventory) EXPECT_EQ(item.mHotkey, -1);
}

TEST(OblivionWorldTest, SavedDryInteriorWaterIsInitializedAndWetInteriorLevelIsPreserved)
{
    NativeWorldFixture fixture;
    ESM::ReadersCache readers;
    for (bool hasWater : {false, true})
    {
        SCOPED_TRACE(hasWater);
        ESM4::Cell record{};
        record.mId = ESM::RefId(ESM::FormId{0xaaaa, 0});
        record.mCellFlags = ESM4::CELL_Interior | (hasWater ? ESM4::CELL_HasWater : 0);
        record.mWaterHeight = 64.f;
        MWWorld::CellStore cell(MWWorld::Cell(record), fixture.mWorld.getStore(), readers);
        ESM::CellState saved{};
        // Reproduce poisoned stack storage without reading uninitialized memory.
        saved.mWaterLevel = std::numeric_limits<float>::quiet_NaN();
        cell.saveState(saved);
        EXPECT_EQ(saved.mWaterLevel, hasWater ? 64.f : 0.f);
        EXPECT_TRUE(saved.mIsInterior);
        EXPECT_EQ(saved.mId, record.mId);
        cell.setWaterLevel(12.5f);
        cell.saveState(saved);
        EXPECT_EQ(saved.mWaterLevel, hasWater ? 12.5f : 0.f);
    }
}

TEST(OblivionWorldTest, CrimeContractsRejectMissingBindingsBeforeClearAndSurvivePreparedRestore)
{
    for (std::uint32_t version = 42; version <= ESM4::CurrentRuntimeStateVersion; ++version)
    {
        SCOPED_TRACE(version);
        PopulatedMigrationFixture fixture;
        auto& world = fixture.mWorld;
        auto saved = world.captureOblivionRuntimeState();
        saved.mVersion = version;
        ASSERT_FALSE(saved.mReferences.empty());
        saved.mPhysicalActions.mNext = 2;
        auto& crime = saved.mNativeCrime;
        crime.mNextIncident = crime.mNextTransaction = 2;
        ESM4::CrimeIncident incident;
        incident.mRequest.mAction = 1;
        incident.mRequest.mPerpetrator = saved.mPlayer.mReference;
        incident.mRequest.mVictim = saved.mReferences.front().mKey;
        incident.mRequest.mAffectedReference = saved.mReferences.front().mKey;
        incident.mRequest.mCell = saved.mPlayer.mCell;
        incident.mOutcome.mIncident = 1;
        incident.mOutcome.mConsequencesCommitted = true;
        // Schema42 stores an integer bounty delta; schema43 widened that wire
        // field. Exercise each representable envelope through the actual World.
        incident.mOutcome.mBountyDelta = version == 42 ? 9.0 : 9.5;
        crime.mIncidents = {incident};
        crime.mArrests = {{1, 1, saved.mPlayer.mReference, saved.mReferences.front().mKey,
            saved.mPlayer.mCell, ESM4::ArrestResolution::Jail, ESM4::ArrestPhase::Committed, 25, false, true, true}};
        ESM4::JailTransaction jail;
        jail.mTransaction = 1; jail.mActor = saved.mPlayer.mReference;
        jail.mPrison = jail.mEvidence = jail.mBelongings = jail.mRelease = saved.mReferences.front().mKey;
        jail.mPhase = ESM4::JailPhase::Serving; jail.mPropertyCommitted = true;
        jail.mRemainingHours = 3.5f; jail.mSentenceStart = 11.25;
        ESM4::CrimePropertyMetadata property;
        property.mInstance = ESM::FormKey::dynamic("historical-property", 1);
        property.mBase = ESM::FormKey::content("headless.esm", 0x940);
        property.mCount = 1; property.mCondition = 37.125f; property.mCharge = 9.25f;
        property.mOriginalOwner = ESM::FormKey::content("headless.esm", 0x800);
        property.mOriginalOwnershipRank = -2; property.mQuestItem = true;
        jail.mProperty = {property}; crime.mJails = {jail};
        saved = ESM4::RuntimeState::deserializeBinary(saved.serializeBinary());
        const auto before = world.captureOblivionRuntimeState().serializeBinary();
        const auto missing = ESM::FormKey::content("headless.esm", 0xfffffe);
        for (int field = 0; field < 8; ++field)
        {
            SCOPED_TRACE(field);
            auto bad = saved;
            switch (field)
            {
                case 0: bad.mNativeCrime.mIncidents.front().mRequest.mVictim = missing; break;
                case 1: bad.mNativeCrime.mIncidents.front().mRequest.mAffectedReference = missing; break;
                case 2: bad.mNativeCrime.mIncidents.front().mRequest.mCell = missing; break;
                case 3: bad.mNativeCrime.mIncidents.front().mRequest.mOwnership.mOwner = missing; break;
                case 4: bad.mNativeCrime.mIncidents.front().mOutcome.mFactionDeltas = {{missing, -1}}; break;
                case 5: bad.mNativeCrime.mArrests.front().mAuthority = missing; break;
                case 6: bad.mNativeCrime.mJails.front().mEvidence = missing; break;
                case 7: bad.mNativeCrime.mJails.front().mProperty.front().mBase = missing; break;
            }
            EXPECT_ANY_THROW(world.prepareOblivionSaveState(bad, std::make_unique<MWWorld::ESMStore>()));
            EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before);
        }
        EXPECT_FALSE(world.isPlayerInJail());
        auto plan = world.prepareOblivionSaveState(saved, std::make_unique<MWWorld::ESMStore>());
        world.clear();
        ASSERT_TRUE(plan->install());
        restorePreparedSaveActorFixture(fixture, saved);
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        EXPECT_EQ(world.captureOblivionRuntimeState().mNativeCrime, saved.mNativeCrime);
        EXPECT_TRUE(world.isPlayerInJail());
        const auto migrated = world.captureOblivionRuntimeState();
        EXPECT_EQ(migrated.mVersion, ESM4::CurrentRuntimeStateVersion);
        EXPECT_DOUBLE_EQ(migrated.mNativeCrime.mIncidents.front().mOutcome.mBountyDelta,
            version == 42 ? 9.0 : 9.5);
        // A fresh preparation/clear/install cycle must retain the same committed
        // transaction rather than consuming or duplicating it during restoration.
        auto repeated = world.prepareOblivionSaveState(
            ESM4::RuntimeState::deserializeBinary(migrated.serializeBinary()), std::make_unique<MWWorld::ESMStore>());
        world.clear();
        ASSERT_TRUE(repeated->install());
        restorePreparedSaveActorFixture(fixture, migrated);
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        EXPECT_EQ(world.captureOblivionRuntimeState().mNativeCrime, saved.mNativeCrime);
        EXPECT_TRUE(world.isPlayerInJail());
        world.clear();
        EXPECT_FALSE(world.isPlayerInJail());
    }
}

TEST(OblivionWorldTest, CrimePlayerAliasRestoresLosslesslyAndPlacedActorsRequireTheCorrectWinningBase)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    auto saved = world.captureOblivionRuntimeState();
    saved.mPhysicalActions.mNext = 2;
    saved.mNativeCrime.mNextIncident = saved.mNativeCrime.mNextTransaction = 2;
    const auto alias = ESM::FormKey::content("oblivion.esm", 0x14);
    ESM4::CrimeIncident incident;
    incident.mRequest.mAction = 1; incident.mRequest.mPerpetrator = alias;
    incident.mRequest.mAffectedReference = alias;
    incident.mRequest.mCell = saved.mPlayer.mCell;
    incident.mOutcome.mIncident = 1; incident.mOutcome.mConsequencesCommitted = true;
    saved.mNativeCrime.mIncidents = {incident};
    saved.mNativeCrime.mArrests = {{1, 1, saved.mPlayer.mReference, saved.mReferences.front().mKey,
        saved.mPlayer.mCell, ESM4::ArrestResolution::Jail, ESM4::ArrestPhase::Committed, 0, false, true, true}};
    ESM4::JailTransaction jail;
    jail.mTransaction = 1; jail.mActor = alias;
    jail.mPrison = jail.mEvidence = jail.mBelongings = jail.mRelease = saved.mReferences.front().mKey;
    jail.mPhase = ESM4::JailPhase::Serving; jail.mPropertyCommitted = true;
    saved.mNativeCrime.mJails = {jail};
    auto& store = world.getStore();
    const auto npcBase = ESM::FormKey::content("headless.esm", 0x800);
    const auto creatureBase = ESM::FormKey::content("headless.esm", 0x820);
    ESM4::Creature creature{};
    creature.mId = {0x820, 0}; creature.mFormKey = creatureBase; creature.mAttackReach = 64;
    store.getWritable<ESM4::Creature>().insertStatic(creature, creatureBase);
    const auto unchanged = world.captureOblivionRuntimeState().serializeBinary();
    const auto external = ESM::FormKey::content("headless.esm", 0xaaa);
    // The external actor is not in the saved snapshot, so admission must use
    // its placed record and winning typed base, without loading a CellStore.
    auto bad = saved; bad.mNativeCrime.mIncidents.front().mRequest.mVictim = external;
    for (const auto& base : {ESM::FormKey::content("headless.esm", 0xfffffe), creatureBase, npcBase})
    {
        ESM4::ActorCharacter placed{};
        placed.mId = {0xaaa, 0}; placed.mFormKey = external; placed.mBaseKey = base;
        store.getWritable<ESM4::ActorCharacter>().insertStatic(placed, external);
        if (base == npcBase)
            EXPECT_NO_THROW(world.prepareOblivionSaveState(bad, std::make_unique<MWWorld::ESMStore>()));
        else
            EXPECT_ANY_THROW(world.prepareOblivionSaveState(bad, std::make_unique<MWWorld::ESMStore>()));
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), unchanged);
    }
    ASSERT_TRUE(store.getWritable<ESM4::ActorCharacter>().eraseStatic(external));
    for (const auto& base : {ESM::FormKey::content("headless.esm", 0xfffffe), npcBase, creatureBase})
    {
        ESM4::ActorCreature placed{};
        placed.mId = {0xaaa, 0}; placed.mFormKey = external; placed.mBaseKey = base;
        store.getWritable<ESM4::ActorCreature>().insertStatic(placed, external);
        if (base == creatureBase)
            EXPECT_NO_THROW(world.prepareOblivionSaveState(bad, std::make_unique<MWWorld::ESMStore>()));
        else
            EXPECT_ANY_THROW(world.prepareOblivionSaveState(bad, std::make_unique<MWWorld::ESMStore>()));
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), unchanged);
    }
    // Unloaded actor checks must not publish or load new live actor state.
    bad.mNativeCrime.mArrests.front().mAuthority = ESM::FormKey::content("other.esm", 0x14);
    EXPECT_ANY_THROW(world.prepareOblivionSaveState(bad, std::make_unique<MWWorld::ESMStore>()));
    EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), unchanged);
    auto plan = world.prepareOblivionSaveState(saved, std::make_unique<MWWorld::ESMStore>());
    world.clear(); ASSERT_TRUE(plan->install());
    restorePreparedSaveActorFixture(fixture, saved);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    EXPECT_TRUE(world.isPlayerInJail());
    EXPECT_EQ(world.captureOblivionRuntimeState().mNativeCrime, saved.mNativeCrime);
}

TEST(OblivionWorldTest, BountyPlayerClassLuaAndGenericAVCommandsShareDistinctNativeStorage)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    EXPECT_FALSE(world.setOblivionPlayerCrimeLevel(9));
    EXPECT_THROW(world.goToJail(), std::logic_error); // Before even requiring a ready Player view.
    ESM4::Npc native{};
    native.mId = {7, 1}; native.mFormKey = ESM::FormKey::content("oblivion.esm", 7);
    native.mIsTES4 = true; native.mData.attribs = {50, 50, 50, 50, 50, 50, 50, 50};
    world.getStore().getWritable<ESM4::Npc>().insertStatic(native, native.mFormKey);
    ASSERT_TRUE(world.initializeOblivionPlayerActor());
    auto player = world.getPlayerPtr();
    auto& stats = player.getClass().getNpcStats(player);
    EXPECT_EQ(stats.getBounty(), 0);
    EXPECT_THROW(stats.setBounty(9), std::logic_error); // Fresh actors already own zero.
    auto legacyValues = *world.getOblivionCombatService()->findActorValues(ESM::FormKey::dynamic("player", 1));
    legacyValues.mBounty.reset();
    world.getOblivionCombatService()->publishPlayerValues(world.getPlayer(), legacyValues,
        MWWorld::resolveOblivionPlayerDynamicBaseSettings(world.getStore()));
    stats.setBounty(9); // An explicitly unowned legacy facade retains its contract.
    auto& service = *world.getOblivionCombatService();
    auto values = (*service.findActorValues(ESM::FormKey::dynamic("player", 1)));
    values.mBounty = ESM4::CrimeBountyState{.25f, -3.5f};
    service.publishPlayerValues(world.getPlayer(), values, MWWorld::resolveOblivionPlayerDynamicBaseSettings(world.getStore()));
    EXPECT_EQ(stats.getBounty(), 1);
    EXPECT_EQ(service.crimeBounty(values.mActor), 1.f);
    EXPECT_THROW(stats.setBounty(50), std::logic_error);
    EXPECT_EQ(world.getOblivionScriptActorValue(values.mActor, 37, false), .25);
    EXPECT_EQ(world.getOblivionScriptActorValue(values.mActor, 37, true), 0);
    ASSERT_TRUE(world.executeOblivionActorValueCommand(player, 37, ESM4::ActorValueCommand::Mod,
        ESM4::ActorValueCommandSource::Script, 2));
    ASSERT_TRUE(world.executeOblivionActorValueCommand(player, 37, ESM4::ActorValueCommand::Force,
        ESM4::ActorValueCommandSource::Console, 7));
    EXPECT_EQ(service.getPlayerValue(37), 2.25f); // Positive Damage is clamped at zero.
    ASSERT_TRUE(world.executeOblivionActorValueCommand(player, 37, ESM4::ActorValueCommand::Force,
        ESM4::ActorValueCommandSource::Script, 7));
    EXPECT_EQ(service.getPlayerValue(37), 7.f);
    EXPECT_EQ(service.getPlayerIntegerValue(37), 6);
    EXPECT_EQ((*service.findActorValues(ESM::FormKey::dynamic("player", 1))).mBounty->mNormal, .25f);
    EXPECT_EQ(stats.getBounty(), 1);

    LuaUtil::ScriptsConfiguration config;
    LuaUtil::LuaState luaState(&fixture.mVfs, &config);
    MWLua::Context context{MWLua::Context::Global};
    context.mLuaManager = fixture.mLuaManager.get(); context.mLua = &luaState;
    sol::state_view lua = luaState.unsafeState();
    lua.new_usertype<MWLua::Object>("Object", sol::no_constructor);
    lua.new_usertype<MWLua::GObject>("GObject", sol::no_constructor,
        sol::base_classes, sol::bases<MWLua::Object>());
    sol::table bindings(lua, sol::create);
    MWLua::addPlayerCrimeLevelBindings(bindings);
    lua["Player"] = bindings; lua["target"] = MWLua::GObject(player);
    auto result = lua.safe_script("assert(Player.getCrimeLevel(target) == 1); Player.setCrimeLevel(target, 25); "
        "assert(Player.getCrimeLevel(target) == 25)", sol::script_pass_on_error);
    ASSERT_TRUE(result.valid()) << sol::error(result).what();
    EXPECT_EQ((*service.findActorValues(ESM::FormKey::dynamic("player", 1))).mBounty->mNormal, 25.f);
    EXPECT_EQ(service.getPlayerValue(37), 31.75f);
    values = (*service.findActorValues(ESM::FormKey::dynamic("player", 1))); values.mPlayerInShiveringIsles = true;
    service.publishPlayerValues(world.getPlayer(), values, MWWorld::resolveOblivionPlayerDynamicBaseSettings(world.getStore()));
    EXPECT_EQ(service.crimeBounty(values.mActor), -3.5f);
    EXPECT_EQ(stats.getBounty(), -3);
    result = lua.safe_script("Player.setCrimeLevel(target, -4); assert(Player.getCrimeLevel(target) == -4)",
        sol::script_pass_on_error);
    ASSERT_TRUE(result.valid()) << sol::error(result).what();
    EXPECT_EQ((*service.findActorValues(ESM::FormKey::dynamic("player", 1))).mBounty->mNormal, 25.f);
    EXPECT_EQ((*service.findActorValues(ESM::FormKey::dynamic("player", 1))).mBounty->mShiveringIsles, -4.f);
    EXPECT_EQ(service.getPlayerValue(37), 31.75f);
    auto saved = world.captureOblivionRuntimeState();
    MWMechanics::MechanicsManager mechanics;
    EXPECT_FALSE(mechanics.commitCrime(player, fixture.mActor, MWBase::MechanicsManager::OT_Theft, {}, 25, true));
    EXPECT_THROW(world.goToJail(), std::logic_error);
    EXPECT_THROW(world.goToJail(), std::logic_error);
    EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), saved.serializeBinary());
    readNativeSnapshot(fixture, ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    EXPECT_EQ(service.crimeBounty(values.mActor), -4.f);
    EXPECT_EQ(world.getPlayerPtr().getClass().getNpcStats(world.getPlayerPtr()).getBounty(), -4);
    auto bad = (*service.findActorValues(ESM::FormKey::dynamic("player", 1)));
    bad.mBounty->mNormal = std::numeric_limits<float>::max();
    bad.mValues[37].mModifiers[0] = std::numeric_limits<float>::max();
    EXPECT_THROW(service.publishPlayerValues(world.getPlayer(), bad,
        MWWorld::resolveOblivionPlayerDynamicBaseSettings(world.getStore())), std::runtime_error);
    EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), saved.serializeBinary());
    ASSERT_NO_THROW(world.clear());
    EXPECT_FALSE(service.crimeBounty(values.mActor));
    MWWorld::World legacy(nullptr, -1, "", {}, ESM::GameProfile::Morrowind);
    EXPECT_FALSE(legacy.setOblivionPlayerCrimeLevel(3));
}

TEST(OblivionWorldTest, BountyNpcAndCreatureQueriesKeepReferenceBaseAndProcessModifiersSeparate)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    auto creature = addEquipmentCreature(fixture);
    creature = creature.getCell()->moveTo(creature, fixture.mActor.getCell());
    world.getWorldModel().registerPtr(creature);
    auto& service = *world.getOblivionCombatService();
    for (const auto& actor : {fixture.mActor, creature})
    {
        ASSERT_TRUE(world.initializeOblivionNonPlayerActor(actor, ESM4::ActorValueProcess::Active));
        auto values = *service.findActorValues(actor.getCellRef().getFormKey());
        values.mBounty = ESM4::CrimeBountyState{10.5f, 0.f};
        values.mValues[37].mModifiers = {.5f, -3.25f, -.25f};
        service.publishNonPlayerValues(actor, values);
        EXPECT_EQ(service.crimeBounty(values.mActor), 10.5f);
        EXPECT_EQ(service.getNonPlayerValue(actor, 37), -3.f);
        EXPECT_EQ(service.getNonPlayerIntegerValue(actor, 37), -2);
        EXPECT_EQ(world.getOblivionScriptActorValue(values.mActor, 37, true), 10);
        EXPECT_EQ(world.getOblivionScriptActorValue(values.mActor, 37, false), -3);
        if (actor.getType() == ESM::REC_NPC_4)
        {
            auto& stats = actor.getClass().getNpcStats(actor);
            EXPECT_EQ(stats.getBounty(), 10);
            EXPECT_THROW(stats.setBounty(99), std::logic_error);
        }
        auto saved = world.captureOblivionRuntimeState();
        readNativeSnapshot(fixture, saved); ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        EXPECT_EQ(service.crimeBounty(values.mActor), 10.5f);
    }
}

TEST(OblivionWorldTest, PlayerReputationOwnsRawCountersSharedFameAndExistingInfamyCommand)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    ESM4::Npc native{};
    native.mId = {7, 1}; native.mFormKey = ESM::FormKey::content("oblivion.esm", 7);
    native.mIsTES4 = true; native.mData.attribs = {50, 50, 50, 50, 50, 50, 50, 50};
    world.getStore().getWritable<ESM4::Npc>().insertStatic(native, native.mFormKey);
    ASSERT_TRUE(world.initializeOblivionPlayerActor());
    auto& service = *world.getOblivionCombatService();
    const auto player = world.getPlayerPtr();
    auto& stats = player.getClass().getNpcStats(player);
    EXPECT_EQ(stats.getReputation(), 0);
    EXPECT_THROW(stats.setReputation(17), std::logic_error);
    auto values = *service.findActorValues(ESM::FormKey::dynamic("player", 1));
    values.mReputation = ESM4::PlayerReputationState{16777217, -3, 777};
    values.mValues[38].mModifiers = {.5f, .75f, -.25f};
    values.mValues[39].mModifiers = {0.f, 1.f, 0.f};
    service.publishPlayerValues(world.getPlayer(), values, MWWorld::resolveOblivionPlayerDynamicBaseSettings(world.getStore()));
    EXPECT_EQ(stats.getReputation(), 16777217); // Raw Fame, independently of AV modifiers and rounding.
    EXPECT_EQ(service.getPlayerBaseValue(38), 16777216);
    EXPECT_EQ(service.getPlayerValue(38), 16777218.f);
    EXPECT_EQ(service.getPlayerIntegerValue(38), 16777217);
    EXPECT_EQ(service.getPlayerValue(39), -2.f);
    MWWorld::OblivionScriptManager host(world, world.getStore(), {"headless.esm"});
    EXPECT_EQ(ObScript::asInteger(host.call("GetPCInfamy", {}, {}, {}, {})), -3);
    EXPECT_EQ(ObScript::asInteger(host.call("ModPCInfamy", {}, {std::int64_t{0}}, {}, {})), 0);
    EXPECT_EQ(service.playerReputation()->mBountyAccumulator, 0);
    EXPECT_EQ(service.playerReputation()->mInfamy, -3);
    ASSERT_TRUE(world.requestOblivionReputation(player, -20));
    EXPECT_EQ(stats.getReputation(), -20); // Native signed counter, not TES3's 0..255 facade clamp.
    EXPECT_EQ(service.playerReputation()->mInfamy, -3);
    EXPECT_EQ(service.getPlayerValue(38), -19.f);
    LuaUtil::ScriptsConfiguration config;
    LuaUtil::LuaState luaState(&fixture.mVfs, &config);
    InspectableScripts scripts(&luaState, MWLua::LObject(player));
    MWLua::Context context{MWLua::Context::Local};
    context.mLuaManager = fixture.mLuaManager.get(); context.mLua = &luaState;
    sol::state_view lua = luaState.unsafeState();
    sol::table actor(lua, sol::create), npcType(lua, sol::create);
    MWLua::addActorStatsBindings(actor, context);
    npcType["baseType"] = actor;
    MWLua::addNpcStatsBindings(npcType, context);
    lua["NPC"] = npcType;
    lua.new_usertype<MWLua::SelfObject>("SelfObject", sol::no_constructor);
    lua["target"] = &scripts.self();
    auto result = lua.safe_script("NPC.stats.reputation(target).current = 16777219", sol::script_pass_on_error);
    ASSERT_TRUE(result.valid()) << sol::error(result).what();
    EXPECT_EQ(stats.getReputation(), -20); // Deferred request does not publish early.
    ASSERT_NO_THROW(scripts.applyStatsCache());
    EXPECT_EQ(stats.getReputation(), 16777219);
    ASSERT_TRUE(world.requestOblivionReputation(player, -20));
    const auto before = world.captureOblivionRuntimeState();
    EXPECT_THROW(host.call("ModPCInfamy", {}, {std::numeric_limits<std::int64_t>::max()}, {}, {}), ObScript::RuntimeError);
    EXPECT_EQ(world.captureOblivionRuntimeState().mNativeActorValues, before.mNativeActorValues);
    auto saved = ESM4::RuntimeState::deserializeBinary(before.serializeBinary());
    EXPECT_FALSE(saved.mPlayer.mActorValues.contains("infamy"));
    ESM::NpcState shared{}; shared.blank(); player.getClass().writeAdditionalState(player, shared);
    EXPECT_EQ(shared.mNpcStats.mReputation, -20);
    prepareNativeSnapshotPlayer(fixture, saved);
    readNativeSnapshot(fixture, saved);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    EXPECT_EQ(service.playerReputation(), (std::optional<ESM4::PlayerReputationState>{{-20, -3, 0}}));
}

TEST(OblivionWorldTest, ExplicitReputationWriteAdoptsLegacyInfamyWithoutGuessingAccumulator)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    ESM4::Npc native{};
    native.mId = {7, 1}; native.mFormKey = ESM::FormKey::content("oblivion.esm", 7);
    native.mIsTES4 = true; native.mData.attribs = {50, 50, 50, 50, 50, 50, 50, 50};
    world.getStore().getWritable<ESM4::Npc>().insertStatic(native, native.mFormKey);
    ASSERT_TRUE(world.initializeOblivionPlayerActor());
    auto& service = *world.getOblivionCombatService();
    auto saved = world.captureOblivionRuntimeState();
    auto& values = *std::find_if(saved.mNativeActorValues.begin(), saved.mNativeActorValues.end(),
        [](const auto& actor) { return actor.mOwner == ESM4::ActorValueOwner::Player; });
    values.mReputation.reset();
    saved.mPlayer.mActorValues["infamy"] = 100.75;
    prepareNativeSnapshotPlayer(fixture, saved);
    readNativeSnapshot(fixture, saved);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    EXPECT_FALSE(service.playerReputation());
    EXPECT_EQ(world.captureOblivionRuntimeState().mPlayer.mActorValues.at("infamy"), 100.75);
    const auto player = world.getPlayerPtr();
    ASSERT_TRUE(world.requestOblivionReputation(player, 16777217));
    EXPECT_EQ(service.playerReputation()->mFame, 16777217);
    EXPECT_EQ(service.playerReputation()->mInfamy, 100);
    EXPECT_FALSE(service.playerReputation()->mBountyAccumulator);
    auto adopted = world.captureOblivionRuntimeState();
    EXPECT_FALSE(adopted.mPlayer.mActorValues.contains("infamy"));
    EXPECT_EQ(adopted.mPlayer.mActorValues.at("legacy.infamy"), 100.75);
    ASSERT_TRUE(world.modifyOblivionPlayerInfamy(0));
    EXPECT_EQ(service.playerReputation()->mBountyAccumulator, 0);
    EXPECT_EQ(service.playerReputation()->mInfamy, 100);
    adopted = world.captureOblivionRuntimeState();
    auto legacy = saved;
    legacy.mPlayer.mActorValues["infamy"] = 0x1p40;
    readNativeSnapshot(fixture, legacy);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    const auto before = world.captureOblivionRuntimeState();
    EXPECT_THROW(world.modifyOblivionPlayerInfamy(0), std::invalid_argument);
    EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before.serializeBinary());
    auto downgrade = adopted;
    service.restore(adopted, world.getStore());
    downgrade.mVersion = 45;
    for (auto& actor : downgrade.mNativeActorValues) actor.mReputation.reset();
    const auto original = downgrade.serializeBinary();
    EXPECT_THROW(service.capture(downgrade), std::invalid_argument);
    EXPECT_EQ(downgrade.serializeBinary(), original);
}

TEST(OblivionWorldTest, LegacyPlayerCounterReadsCheckIntegerBoundsWithoutChangingLoadedAuthority)
{
    PopulatedMigrationFixture fixture;
    auto& world = fixture.mWorld;
    auto saved = world.captureOblivionRuntimeState();
    prepareNativeSnapshotPlayer(fixture, saved);
    MWWorld::OblivionScriptManager host(world, world.getStore(), {"headless.esm"});
    const std::array<std::pair<const char*, const char*>, 3> commands{{
        {"GetPCInfamy", "infamy"}, {"GetPCFactionMurder", "faction_murder"},
        {"GetPCFactionSteal", "faction_steal"}}};
    for (const auto& [command, field] : commands)
    {
        SCOPED_TRACE(command);
        EXPECT_EQ(ObScript::asInteger(host.call(command, {}, {}, {}, {})), 0);
        for (const auto& [number, expected] : std::array<std::pair<double, std::int64_t>, 6>{{
                 {100.75, 100}, {-100.75, -100}, {0x1p40, std::int64_t{1} << 40},
                 {-0x1p63, std::numeric_limits<std::int64_t>::min()},
                 {std::nextafter(0x1p63, 0.), std::numeric_limits<std::int64_t>::max() - 1023},
                 {-0.75, 0}}})
        {
            SCOPED_TRACE(number);
            saved.mPlayer.mActorValues[field] = number;
            readNativeSnapshot(fixture, saved);
            ASSERT_NO_THROW(world.applyOblivionRuntimeState());
            const auto before = world.captureOblivionRuntimeState().serializeBinary();
            EXPECT_EQ(ObScript::asInteger(host.call(command, {}, {}, {}, {})), expected);
            EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before);
        }
        for (const double number : {0x1p63, std::nextafter(-0x1p63, -INFINITY), 0x1p100, -0x1p100})
        {
            SCOPED_TRACE(number);
            saved.mPlayer.mActorValues[field] = number;
            readNativeSnapshot(fixture, saved);
            ASSERT_NO_THROW(world.applyOblivionRuntimeState());
            const auto before = world.captureOblivionRuntimeState().serializeBinary();
            EXPECT_THROW(host.call(command, {}, {}, {}, {}), ObScript::RuntimeError);
            EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before);
        }
        saved.mPlayer.mActorValues.erase(field);
        readNativeSnapshot(fixture, saved);
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    }
}


TEST(OblivionWorldTest, NativeItemHotkeyQueryPreservesEveryInventorySchemaAndBinaryState)
{
    for (std::uint32_t version = 1; version <= ESM4::CurrentRuntimeStateVersion; ++version)
    {
        SCOPED_TRACE(version);
        PopulatedMigrationFixture fixture;
        auto& world = fixture.mWorld;
        auto saved = world.captureOblivionRuntimeState();
        saved.mVersion = version;
        if (version < 3)
        {
            saved.mPlayer.mName.clear(); saved.mPlayer.mRace = {}; saved.mPlayer.mClass = {};
            saved.mPlayer.mBirthSign = {}; saved.mPlayer.mFemale = false;
            saved.mPlayer.mCharacterGenerationFlags = 0;
        }
        ESM4::RuntimeInventoryItem item;
        item.mBase = ESM::FormKey::content("headless.esm", 0x940);
        item.mCount = 1;
        if (version >= 4) item.mHotkey = 7;
        saved.mPlayer.mInventory = {item};
        const auto decoded = ESM4::RuntimeState::deserializeBinary(saved.serializeBinary());
        readNativeSnapshot(fixture, decoded);
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        const auto before = world.captureOblivionRuntimeState().serializeBinary();
        const auto hotkeys = world.oblivionPlayerItemHotkeys();
        if (version < 4) EXPECT_FALSE(hotkeys);
        else
        {
            ASSERT_TRUE(hotkeys);
            EXPECT_EQ((*hotkeys)[7], ESM::RefId(ESM::FormId{0x940, 0}));
            for (int i = 0; i < 7; ++i) EXPECT_TRUE((*hotkeys)[i].empty());
        }
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before);
    }
}
