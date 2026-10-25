#include <components/esm4/nativesequencedeactivation.hpp>
#include "apps/openmw_tests/mwworld/oblivionworldfixture.hpp"
#include "apps/openmw/mwclass/npc.hpp"
#include "apps/openmw/mwclass/esm4npc.hpp"
#include "apps/openmw/mwworld/oblivionnativeidle.hpp"
#include "apps/openmw/mwworld/worldmodel.hpp"
#include "apps/openmw/mwworld/cellstore.hpp"
#include "apps/openmw/mwmechanics/oblivioncombat.hpp"
#include "apps/openmw/mwmechanics/creaturestats.hpp"
#include "apps/openmw/mwrender/animation.hpp"
#include <components/esm4/loadidle.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadrefr.hpp>
#include <components/resource/keyframemanager.hpp>
#include <components/resource/niffilemanager.hpp>
#include <components/nifosg/matrixtransform.hpp>
#include <components/sceneutil/skeleton.hpp>
#include <components/vfs/filesystemarchive.hpp>
#include <gtest/gtest.h>
#include <osg/FrameStamp>
#include <osgUtil/UpdateVisitor>
#include <fstream>
#include <bit>
#include <components/esm3/loadrace.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/files/hash.hpp>
#include "apps/openmw/mwworld/player.hpp"

namespace
{
    template <class T> std::string bytes(const T& value)
    { return {reinterpret_cast<const char*>(&value), sizeof(value)}; }
    std::string text(std::string_view value)
    { return bytes(static_cast<std::uint32_t>(value.size())) + std::string(value); }
    std::string sub(std::uint32_t tag, const std::string& value)
    { return bytes(tag) + bytes(static_cast<std::uint16_t>(value.size())) + value; }
    std::string record(std::uint32_t tag, std::uint32_t id, const std::string& value)
    { return bytes(tag) + bytes(static_cast<std::uint32_t>(value.size())) + bytes(0u) + bytes(id) + bytes(0u) + value; }
    std::string idle(std::uint32_t id, std::uint32_t parent, const std::string& model,
        const std::string& conditions = {})
    {
        return record(ESM4::REC_IDLE, id, sub(ESM::fourCC("EDID"), std::string("ExactIdle") + '\0')
            + sub(ESM::fourCC("MODL"), model + '\0') + sub(ESM::fourCC("ANAM"), std::string(1, 5))
            + sub(ESM::fourCC("DATA"), bytes(parent) + bytes(0u)) + conditions);
    }
    std::string kf(float translation, std::string_view root = "Bip01", bool duplicate = false,
        bool trackless = false, std::string_view name = "AuthoredGetUpLabel")
    {
        std::string data = "Gamebryo File Format, Version 20.0.0.5\n";
        data += bytes(std::uint32_t{0x14000005}) + bytes(std::uint8_t{1}) + bytes(std::uint32_t{10})
            + bytes(std::uint32_t{duplicate ? 5u : 4u}) + bytes(0u) + std::string(3, '\0') + bytes(std::uint16_t{4});
        for (const auto type : {"NiControllerSequence", "NiTextKeyExtraData", "NiTransformInterpolator", "NiStringPalette"})
            data += text(type);
        for (std::uint16_t i = 0; i < 4; ++i) data += bytes(i);
        if (duplicate) data += bytes(std::uint16_t{0});
        data += bytes(0u);
        const auto start = data.size();
        data += text(name) + bytes(trackless ? 0u : 1u) + bytes(1u);
        if (!trackless)
        {
            data += bytes(std::int32_t{2}) + bytes(std::int32_t{-1}) + bytes(std::int32_t{3}) + bytes(0u);
            for (int i = 0; i < 4; ++i) data += bytes(0xffffffffu);
        }
        data += bytes(1.f) + bytes(std::int32_t{1}) + bytes(2u) + bytes(1.f)
            + bytes(10.f) + bytes(12.f) + bytes(std::int32_t{-1}) + text(root) + bytes(std::int32_t{3});
        const auto sequence = data.substr(start);
        data += text("") + bytes(2u) + bytes(10.f) + text("Start") + bytes(12.f) + text("End");
        for (const float value : {translation, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f}) data += bytes(value);
        data += bytes(std::int32_t{-1}) + text(std::string("Bip01\0", 6)) + bytes(6u);
        if (duplicate) data += sequence;
        data += bytes(1u) + bytes(std::int32_t{0});
        return data;
    }

    class IdleAnimation : public MWRender::Animation
    {
    public:
        osg::ref_ptr<SceneUtil::Skeleton> mRoot = new SceneUtil::Skeleton;
        osg::ref_ptr<NifOsg::MatrixTransform> mBone = new NifOsg::MatrixTransform;
        IdleAnimation(const MWWorld::Ptr& actor, Resource::ResourceSystem& resources)
            : Animation(actor, new osg::Group, &resources)
        {
            mRoot->setName("ActorRoot"); mRoot->setUserValue("recordIndex", 0u);
            mBone->setName("Bip01"); mBone->setUserValue("recordIndex", 4u);
            mRoot->addChild(mBone);
            mObjectRoot = mRoot; mSkeleton = mRoot;
        }
        void directory() { addAnimDirectory(VFS::Path::Normalized("meshes")); }
        void source(std::string_view path, std::string_view alias = {})
        { ASSERT_TRUE(addSingleAnimSource(VFS::Path::Normalized(path), "fixture", alias, false)); }
        std::size_t sources() const { return mAnimSources.size(); }
        auto reactionRoot() const { return mNativeReactionRootRecord; }
        void clear() { clearAnimSources(); }
        void replaceRoot() { mObjectRoot = new osg::Group; }
        void sample(std::string_view group)
        {
            MWRender::Animation::NativeSequenceLayer layer;
            layer.mGroup = group; layer.mTime = .5f;
            ASSERT_TRUE(setNativeSequenceLayers(std::span(&layer, 1)));
            osg::ref_ptr<osg::FrameStamp> stamp = new osg::FrameStamp;
            stamp->setSimulationTime(.5);
            osgUtil::UpdateVisitor visitor; visitor.setFrameStamp(stamp);
            runAnimation(0);
            mRoot->accept(visitor);
        }
    };

    struct NativeIdleResourceTest : testing::Test
    {
        TestingOpenMW::NativeWorldFixture mFixture;
        NativeIdleResourceTest()
        {
            MWClass::Npc::registerSelf(); mFixture.mWorld.setupPlayer();
            write("meshes/same.kf", kf(1));
            write("meshes/one/same.kf", kf(9));
            write("meshes/two/same.kf", kf(17));
            write("meshes/missingroot.kf", kf(2, "Missing"));
            write("meshes/duplicate.kf", kf(3, "Bip01", true));
            write("meshes/empty.kf", kf(0, "Bip01", false, true));
            mFixture.mVfs.addArchive(std::make_unique<VFS::FileSystemArchive>(mFixture.mDirectory));
            mFixture.mVfs.buildIndex();
        }
        void write(std::string_view path, const std::string& data)
        {
            const auto file = mFixture.mDirectory / path;
            std::filesystem::create_directories(file.parent_path());
            std::ofstream out(file, std::ios::binary); out.write(data.data(), data.size());
            ASSERT_TRUE(out);
        }
        osg::ref_ptr<IdleAnimation> animation()
        { return new IdleAnimation(mFixture.mWorld.getPlayerPtr(), mFixture.mResources); }
    };
}

TEST_F(NativeIdleResourceTest, ExactPathsDoNotBorrowStemWinnerAndSampleRealParsedTracks)
{
    auto a = animation(); a->directory();
    const auto ordinary = a->getControllerSequenceMetadata("same"); ASSERT_TRUE(ordinary);
    const auto source = mFixture.mResources.getKeyframeManager()->get(VFS::Path::Normalized("meshes/one/same.kf"));
    const auto original = source->mControllerSequences;
    const auto before = a->sources(); const auto* callbacks = a->mBone->getUpdateCallback();
    auto selected = a->prepareNativeIdleSource("ONE\\SAME.KF"); ASSERT_TRUE(selected); ASSERT_TRUE(selected->isValid());
    EXPECT_EQ(selected->model().value(), "meshes/one/same.kf");
    EXPECT_EQ(selected->fileHash().size(), 16);
    EXPECT_EQ(selected->fileHash(), source->mFileHash);
    EXPECT_EQ(selected->fileHash(), mFixture.mResources.getNifFileManager()->get(selected->model())->mHash);
    EXPECT_EQ(selected->metadata().mSequenceName, "AuthoredGetUpLabel");
    EXPECT_EQ(selected->metadata().mStartTime, 10);
    EXPECT_EQ(selected->metadata().mTextKeys, original.front().mTextKeys);
    EXPECT_EQ(a->sources(), before);
    EXPECT_FALSE(a->hasAnimation(selected->group()));
    EXPECT_EQ(a->mBone->getUpdateCallback(), callbacks);
    const auto group = selected->group(); ASSERT_TRUE(selected->commit());
    EXPECT_FALSE(selected->isValid()); EXPECT_FALSE(selected->commit());
    EXPECT_TRUE(a->hasAnimation(group)); EXPECT_FALSE(a->isPlaying(group));
    EXPECT_EQ(source->mControllerSequences, original);
    EXPECT_EQ(a->getControllerSequenceMetadata("same"), ordinary);
    a->sample(group);
    EXPECT_FLOAT_EQ(a->mBone->getControllerTransform().mTranslation.x(), 9);
    auto reused = a->prepareNativeIdleSource("one/same.kf"); ASSERT_TRUE(reused);
    const auto count = a->sources(); EXPECT_EQ(reused->group(), group); ASSERT_TRUE(reused->commit());
    EXPECT_EQ(a->sources(), count);
}

TEST_F(NativeIdleResourceTest, MissingAndAmbiguousFilesCannotFallBackOrPublish)
{
    auto a = animation(); a->directory(); ASSERT_TRUE(a->getControllerSequenceMetadata("same"));
    const auto count = a->sources(); const auto root = a->reactionRoot();
    EXPECT_FALSE(a->prepareNativeIdleSource("absent/same.kf"));
    EXPECT_FALSE(a->prepareNativeIdleSource("duplicate.kf"));
    EXPECT_FALSE(a->prepareNativeIdleSource("same.nif"));
    EXPECT_THROW(a->prepareNativeIdleSource(std::string(260, 'x') + ".kf"), std::invalid_argument);
    EXPECT_EQ(a->sources(), count); EXPECT_EQ(a->reactionRoot(), root);
    EXPECT_FALSE(a->isPlaying("same"));
}

TEST_F(NativeIdleResourceTest, CompetingClearedAndExpiredCandidatesCannotPublish)
{
    auto a = animation();
    auto one = a->prepareNativeIdleSource("one/same.kf"); auto two = a->prepareNativeIdleSource("two/same.kf");
    ASSERT_TRUE(one); ASSERT_TRUE(two); ASSERT_TRUE(one->commit());
    EXPECT_FALSE(two->isValid()); EXPECT_FALSE(two->commit());
    EXPECT_FALSE(a->hasAnimation(two->group()));
    auto cleared = a->prepareNativeIdleSource("two/same.kf"); ASSERT_TRUE(cleared);
    a->clear(); EXPECT_FALSE(cleared->isValid()); EXPECT_FALSE(cleared->commit());
    auto replaced = a->prepareNativeIdleSource("two/same.kf"); ASSERT_TRUE(replaced);
    a->replaceRoot(); EXPECT_FALSE(replaced->commit());
    auto other = animation(); auto expired = other->prepareNativeIdleSource("one/same.kf"); ASSERT_TRUE(expired);
    other = nullptr; EXPECT_FALSE(expired->isValid()); EXPECT_FALSE(expired->commit());
}

TEST_F(NativeIdleResourceTest, FirstSequenceRootIncludingNullStaysBound)
{
    auto a = animation();
    auto first = a->prepareNativeIdleSource("missingroot.kf"); ASSERT_TRUE(first); ASSERT_TRUE(first->commit());
    EXPECT_FALSE(a->reactionRoot());
    auto next = a->prepareNativeIdleSource("one/same.kf"); ASSERT_TRUE(next); ASSERT_TRUE(next->commit());
    EXPECT_FALSE(a->reactionRoot());
    auto b = animation(); auto bound = b->prepareNativeIdleSource("one/same.kf"); ASSERT_TRUE(bound);
    EXPECT_FALSE(b->reactionRoot()); ASSERT_TRUE(bound->commit()); EXPECT_EQ(b->reactionRoot(), 4);
    auto missing = b->prepareNativeIdleSource("missingroot.kf"); ASSERT_TRUE(missing); ASSERT_TRUE(missing->commit());
    EXPECT_EQ(b->reactionRoot(), 4);
}

TEST_F(NativeIdleResourceTest, TracklessNativeSequenceKeepsClockAndFileIdentity)
{
    auto a = animation(); auto empty = a->prepareNativeIdleSource("empty.kf"); ASSERT_TRUE(empty);
    const auto group = empty->group(); EXPECT_EQ(empty->fileHash().size(), 16); ASSERT_TRUE(empty->commit());
    a->play(group, MWRender::AnimPriority(1), MWRender::BlendMask_All, false, 1, "start", "stop", 0, 0);
    a->runAnimation(.5f);
    EXPECT_FLOAT_EQ(a->getCurrentTime(group), .5f);
    EXPECT_EQ(a->getControllerSequenceMetadata(group)->mSequenceName, "AuthoredGetUpLabel");
}

TEST_F(NativeIdleResourceTest, OrdinaryAliasCollisionCannotReplaceExactIdentity)
{
    auto a = animation(); auto selected = a->prepareNativeIdleSource("one/same.kf"); ASSERT_TRUE(selected);
    const auto group = selected->group(); ASSERT_TRUE(selected->commit());
    a->source("meshes/two/same.kf", group);
    EXPECT_FALSE(a->prepareNativeIdleSource("one/same.kf"));
    auto b = animation(); auto pending = b->prepareNativeIdleSource("one/same.kf"); ASSERT_TRUE(pending);
    b->source("meshes/same.kf"); EXPECT_FALSE(pending->commit());
}

TEST_F(NativeIdleResourceTest, ChangedBytesAtSamePathKeepPublishedSourcesDistinct)
{
    auto a = animation(); auto first = a->prepareNativeIdleSource("one/same.kf"); ASSERT_TRUE(first);
    const auto oldGroup = first->group(); const auto oldHash = first->fileHash(); ASSERT_TRUE(first->commit());
    write("meshes/one/same.kf", kf(27));
    mFixture.mResources.clearCache();
    auto changed = a->prepareNativeIdleSource("one/same.kf"); ASSERT_TRUE(changed);
    EXPECT_NE(changed->fileHash(), oldHash); EXPECT_NE(changed->group(), oldGroup);
    const auto newGroup = changed->group(); ASSERT_TRUE(changed->commit());
    EXPECT_EQ(a->sources(), 2);
    a->sample(newGroup); EXPECT_FLOAT_EQ(a->mBone->getControllerTransform().mTranslation.x(), 27);
    a->sample(oldGroup); EXPECT_FLOAT_EQ(a->mBone->getControllerTransform().mTranslation.x(), 9);
}

TEST_F(NativeIdleResourceTest, AcquisitionOwnsParsedBytesWithoutRegisteringOrBindingTheScene)
{
    auto a = animation(); a->directory();
    const auto count = a->sources(); const auto root = a->reactionRoot();
    const auto* callbacks = a->mBone->getUpdateCallback();
    auto resource = a->acquireNativeIdleResource(std::string("ONE\\SAME.KF\0ignored.kf", 22));
    ASSERT_TRUE(resource);
    EXPECT_EQ(resource->model().value(), "meshes/one/same.kf");
    EXPECT_EQ(resource->fileHash().size(), 16);
    EXPECT_EQ(resource->metadata().mSequenceName, "AuthoredGetUpLabel");
    EXPECT_EQ(resource->metadata().mStartTime, 10);
    EXPECT_EQ(a->sources(), count); EXPECT_EQ(a->reactionRoot(), root);
    EXPECT_EQ(a->mBone->getUpdateCallback(), callbacks);
    EXPECT_FALSE(a->acquireNativeIdleResource("absent/same.kf"));
    EXPECT_FALSE(a->acquireNativeIdleResource("duplicate.kf"));
    EXPECT_FALSE(a->acquireNativeIdleResource("same.nif"));
    EXPECT_FALSE(a->acquireNativeIdleResource(""));
    EXPECT_THROW(a->acquireNativeIdleResource(std::string(260, 'x') + ".kf"), std::invalid_argument);
    const auto trackless = a->acquireNativeIdleResource("empty.kf"); ASSERT_TRUE(trackless);
    EXPECT_EQ(trackless->metadata().mStopTime, 12);
    EXPECT_EQ(a->sources(), count); EXPECT_EQ(a->reactionRoot(), root);
}

TEST_F(NativeIdleResourceTest, RetainedResourceRebasesAfterCompetingSourceAndCacheReplacement)
{
    auto a = animation();
    auto resource = a->acquireNativeIdleResource("one/same.kf"); ASSERT_TRUE(resource);
    const auto oldHash = resource->fileHash(); const auto original = resource->metadata();
    auto stale = a->prepareNativeIdleSource(*resource); ASSERT_TRUE(stale);
    const auto oldGroup = stale->group();
    auto competing = a->prepareNativeIdleSource("two/same.kf"); ASSERT_TRUE(competing);
    const auto competingGroup = competing->group(); ASSERT_TRUE(competing->commit());
    EXPECT_FALSE(stale->commit()); EXPECT_FALSE(a->hasAnimation(oldGroup));
    stale.reset();
    write("meshes/one/same.kf", kf(27)); mFixture.mResources.clearCache();
    auto replacement = a->acquireNativeIdleResource("one/same.kf"); ASSERT_TRUE(replacement);
    EXPECT_NE(replacement->fileHash(), oldHash);
    auto rebased = a->prepareNativeIdleSource(*resource); ASSERT_TRUE(rebased);
    EXPECT_EQ(rebased->fileHash(), oldHash); EXPECT_EQ(rebased->group(), oldGroup);
    EXPECT_EQ(rebased->metadata().mTextKeys, original.mTextKeys);
    EXPECT_EQ(a->sources(), 1); EXPECT_FALSE(a->hasAnimation(oldGroup));
    resource.reset(); ASSERT_TRUE(rebased->commit());
    EXPECT_EQ(a->sources(), 2); EXPECT_FALSE(a->isPlaying(oldGroup));
    a->sample(oldGroup); EXPECT_FLOAT_EQ(a->mBone->getControllerTransform().mTranslation.x(), 9);
    a->sample(competingGroup); EXPECT_FLOAT_EQ(a->mBone->getControllerTransform().mTranslation.x(), 17);
    auto current = a->prepareNativeIdleSource(*replacement); ASSERT_TRUE(current);
    const auto newGroup = current->group(); EXPECT_NE(newGroup, oldGroup); ASSERT_TRUE(current->commit());
    a->sample(newGroup); EXPECT_FLOAT_EQ(a->mBone->getControllerTransform().mTranslation.x(), 27);
}

TEST_F(NativeIdleResourceTest, RetainedResourceSurvivesAnimationDestructionAndMissingFile)
{
    auto a = animation(); auto resource = a->acquireNativeIdleResource("one/same.kf"); ASSERT_TRUE(resource);
    const auto hash = resource->fileHash();
    auto old = a->prepareNativeIdleSource(*resource); ASSERT_TRUE(old);
    a = nullptr; EXPECT_FALSE(old->isValid());
    old.reset();
    ASSERT_TRUE(std::filesystem::remove(mFixture.mDirectory / "meshes/one/same.kf"));
    mFixture.mResources.clearCache();
    auto b = animation(); b->source("meshes/two/same.kf");
    auto bound = b->prepareNativeIdleSource(*resource); ASSERT_TRUE(bound);
    EXPECT_EQ(bound->fileHash(), hash); const auto group = bound->group();
    resource.reset(); ASSERT_TRUE(bound->commit());
    EXPECT_EQ(b->sources(), 2); b->sample(group);
    EXPECT_FLOAT_EQ(b->mBone->getControllerTransform().mTranslation.x(), 9);
    b->clear();
    // A published source and candidate are the remaining owners after release
    // of the acquisition handle; neither depends on its originating Animation.
    EXPECT_EQ(bound->fileHash(), hash);
}

TEST(NativeIdleWorldResourceTest, WorldSelectedOutsideKfKeepsItsExactIdentityThroughSampling)
{
    TestingOpenMW::NativeWorldFixture fixture(false, false, false, {}, {},
        idle(0xa00, 0, "Dog\\IdleAnims") + idle(0xa01, 0xa00, "Outside\\same.kf"), 2);
    auto& world = fixture.mWorld; auto& store = world.getStore();
    ESM4::Race race{}; race.mId = {0x810, 0};
    race.mAttribMale = {40, 40, 40, 40, 40, 40, 40, 40}; race.mAttribFemale = race.mAttribMale;
    store.getWritable<ESM4::Race>().insertStatic(race, ESM::FormKey::content("headless.esm", 0x810));
    const auto baseKey = ESM::FormKey::content("headless.esm", 0x800);
    auto base = *store.search<ESM4::Npc>(baseKey); base.mModel = "DoG\\Skeleton.NIF"; base.mRace = race.mId;
    store.getWritable<ESM4::Npc>().insertStatic(base, baseKey);
    MWClass::ESM4Npc::registerSelf();
    ESM4::ActorCharacter reference{}; reference.mId = {0x900, 0};
    reference.mFormKey = ESM::FormKey::content("headless.esm", 0x900);
    reference.mBaseObj = base.mId; reference.mBaseKey = baseKey;
    store.getWritable<ESM4::ActorCharacter>().insertStatic(reference, reference.mFormKey);
    MWWorld::LiveCellRef<ESM4::Npc> live(reference, store.search<ESM4::Npc>(baseKey));
    auto& cell = world.getWorldModel().getDraftCell(); MWWorld::Ptr actor(cell.insert(&live), &cell);
    world.getWorldModel().registerPtr(actor); ASSERT_TRUE(world.activateOblivionActor(actor));
    auto& combat = *world.getOblivionCombatService(); combat.setProcessKnockedState(reference.mFormKey, 3);
    const auto write = [&](std::string_view name, float translation) {
        const auto path = fixture.mDirectory / name; std::filesystem::create_directories(path.parent_path());
        const auto data = kf(translation); std::ofstream out(path, std::ios::binary); out.write(data.data(), data.size());
        ASSERT_TRUE(out);
    };
    write("meshes/same.kf", 1); write("meshes/outside/same.kf", 9);
    fixture.mVfs.addArchive(std::make_unique<VFS::FileSystemArchive>(fixture.mDirectory)); fixture.mVfs.buildIndex();
    osg::ref_ptr<IdleAnimation> animation = new IdleAnimation(actor, fixture.mResources); animation->directory();
    ASSERT_TRUE(animation->getControllerSequenceMetadata("same"));
    ESM4::RuntimeState before, after; combat.capture(before);
    const auto selected = world.prepareOblivionGetUpIdle(actor);
    ASSERT_EQ(selected.mIdle, ESM::FormKey::content("headless.esm", 0xa01));
    ASSERT_EQ(selected.mModel, "Outside\\same.kf");
    const auto root = animation->reactionRoot();
    const auto acquired = animation->acquireNativeIdleResource(selected.mModel); ASSERT_TRUE(acquired);
    EXPECT_EQ(acquired->model().value(), "meshes/outside/same.kf");
    EXPECT_EQ(animation->reactionRoot(), root);
    auto resource = animation->prepareNativeIdleSource(*acquired); ASSERT_TRUE(resource);
    const auto group = resource->group(); ASSERT_TRUE(resource->commit()); animation->sample(group);
    EXPECT_FLOAT_EQ(animation->mBone->getControllerTransform().mTranslation.x(), 9);
    combat.capture(after); EXPECT_EQ(after, before);
}

namespace
{
    struct NativeGetUpRequestTest : testing::Test
    {
        static std::string records()
        {
            const std::array<std::uint32_t, 6> condition{0x60, std::bit_cast<std::uint32_t>(0.f), 77, 0, 0, 0};
            return idle(0xa00, 0, "Dog\\IdleAnims", sub(ESM::fourCC("CTDA"), bytes(condition)))
                + idle(0xa01, 0xa00, "Outside\\same.kf")
                + idle(0xb00, 0, "Cat\\IdleAnims\\Other.kf");
        }
        TestingOpenMW::NativeWorldFixture mFixture{false, false, false, {}, {}, records(), 3};
        MWWorld::Ptr mActor;
        osg::ref_ptr<IdleAnimation> mAnimation;
        NativeGetUpRequestTest()
        {
            auto& world = mFixture.mWorld; auto& store = world.getStore();
            MWClass::Npc::registerSelf(); world.setupPlayer();
            ESM4::Race race{}; race.mId = {0x810, 0};
            race.mAttribMale = {40, 40, 40, 40, 40, 40, 40, 40}; race.mAttribFemale = race.mAttribMale;
            store.getWritable<ESM4::Race>().insertStatic(race, ESM::FormKey::content("headless.esm", 0x810));
            ESM::Race shared{}; shared.blank(); shared.mId = ESM::RefId(race.mId);
            store.getWritable<ESM::Race>().insertStatic(shared);
            const auto baseKey = ESM::FormKey::content("headless.esm", 0x800);
            auto base = *store.search<ESM4::Npc>(baseKey); base.mModel = "DoG\\Skeleton.NIF"; base.mRace = race.mId;
            store.getWritable<ESM4::Npc>().insertStatic(base, baseKey);
            MWClass::ESM4Npc::registerSelf();
            ESM4::ActorCharacter reference{}; reference.mId = {0x900, 0};
            reference.mFormKey = ESM::FormKey::content("headless.esm", 0x900);
            reference.mBaseObj = base.mId; reference.mBaseKey = baseKey;
            store.getWritable<ESM4::ActorCharacter>().insertStatic(reference, reference.mFormKey);
            MWWorld::LiveCellRef<ESM4::Npc> live(reference, store.search<ESM4::Npc>(baseKey));
            auto& cell = world.getWorldModel().getDraftCell(); mActor = MWWorld::Ptr(cell.insert(&live), &cell);
            world.getWorldModel().registerPtr(mActor);
            if (!world.activateOblivionActor(mActor)) throw std::runtime_error("request fixture actor admission failed");
            combat().setProcessKnockedState(key(), 3);
            write(kf(9));
            mFixture.mVfs.addArchive(std::make_unique<VFS::FileSystemArchive>(mFixture.mDirectory));
            mFixture.mVfs.buildIndex();
            mAnimation = new IdleAnimation(mActor, mFixture.mResources);
            ESM4::Cell nativeCell{}; nativeCell.mId = ESM::RefId(ESM::FormId{1, 0});
            nativeCell.mFormKey = ESM::FormKey::content("headless.esm", 1);
            nativeCell.mCellFlags = ESM4::CELL_Interior;
            store.getWritable<ESM4::Cell>().insertStatic(nativeCell, nativeCell.mFormKey);
        }
        ESM::FormKey key() const { return mActor.getCellRef().getFormKey(); }
        MWMechanics::OblivionCombatService& combat() { return *mFixture.mWorld.getOblivionCombatService(); }
        void write(const std::string& data)
        {
            const auto path = mFixture.mDirectory / "meshes/outside/same.kf";
            std::filesystem::create_directories(path.parent_path());
            std::ofstream out(path, std::ios::binary); out.write(data.data(), data.size());
            if (!out) throw std::runtime_error("request fixture KF write failed");
        }
        ESM4::RuntimeState snapshot()
        {
            ESM4::RuntimeState state;
            const auto path = mFixture.mDirectory / "headless.esm";
            std::ifstream stream(path, std::ios::binary);
            state.mContent = {{"headless.esm", "sha256:" + Files::getSha256(path.string(), stream)}};
            state.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
            state.mPlayer.mCell = ESM::FormKey::content("headless.esm", 1);
            state.mPlayer.mRace = ESM::FormKey::content("headless.esm", 0x810);
            state.mPlayer.mClass = ESM::FormKey::dynamic("player-class", 1);
            ESM4::RuntimeReferenceState ref;
            ref.mKey = key(); ref.mBase = combat().findActorValues(key())->mBase; ref.mCell = state.mPlayer.mCell;
            state.mReferences.push_back(ref);
            combat().capture(state);
            state.validate();
            return state;
        }
    };
}

TEST_F(NativeGetUpRequestTest, PublishesSelectedResourceRawByteAndRngTogetherWithoutStartingPlayback)
{
    auto& world = mFixture.mWorld;
    const auto before = snapshot(); const auto count = mAnimation->sources();
    auto prepared = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor);
    ASSERT_TRUE(prepared); ASSERT_TRUE(prepared->isValid());
    const auto selection = prepared->selection(); EXPECT_EQ(selection.mRandomDraws, 1);
    EXPECT_EQ(snapshot(), before); EXPECT_EQ(mAnimation->sources(), count);
    EXPECT_FALSE(mAnimation->reactionRoot());
    auto competing = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor); ASSERT_TRUE(competing->isValid());
    ASSERT_TRUE(prepared->commit()); EXPECT_FALSE(prepared->commit()); EXPECT_FALSE(competing->commit());
    const auto selected = combat().getUpIdle(key()); ASSERT_TRUE(selected);
    EXPECT_EQ(selected->mActorModel, "DoG\\Skeleton.NIF");
    EXPECT_EQ(selected->mIdle, selection.mIdle); EXPECT_EQ(selected->mModel, selection.mModel);
    EXPECT_EQ(selected->mAssetHash.size(), 32);
    EXPECT_EQ(combat().getProcessKnockedState(key()), 5);
    EXPECT_EQ(combat().combatRandomState(), selection.mNextRandomState);
    EXPECT_EQ(combat().findActorValues(key())->mProcessAction, before.mNativeActorValues.front().mProcessAction);
    EXPECT_EQ(mAnimation->sources(), count); EXPECT_FALSE(mAnimation->reactionRoot());
    auto expected = before;
    expected.mNativeActorValues.front().mProcessKnockedState = 5;
    expected.mCombatRngState = selection.mNextRandomState;
    expected.mNativeGetUpIdles.emplace(key(), *selected);
    EXPECT_EQ(snapshot(), expected);
    const auto wire = snapshot().serializeBinary(); const auto loaded = ESM4::RuntimeState::deserializeBinary(wire);
    MWMechanics::OblivionCombatService restored; restored.restore(loaded, world.getStore());
    EXPECT_EQ(restored.getUpIdle(key()), selected);
    EXPECT_EQ(restored.combatRandomState(), selection.mNextRandomState);
    EXPECT_EQ(restored.getProcessKnockedState(key()), 5);
    auto legacy = before; legacy.mVersion = 70;
    EXPECT_THROW(combat().capture(legacy), std::invalid_argument);
    auto expectedLegacy = before; expectedLegacy.mVersion = 70;
    EXPECT_EQ(legacy, expectedLegacy);
}

TEST_F(NativeGetUpRequestTest, RejectsStaleDrawStateRendererAndServiceReplacementBeforePublication)
{
    auto& world = mFixture.mWorld;
    auto prepared = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor); ASSERT_TRUE(prepared->isValid());
    combat().drawNativeRandom(); const auto afterDraw = snapshot();
    EXPECT_FALSE(prepared->commit()); EXPECT_EQ(snapshot(), afterDraw);
    prepared = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor); ASSERT_TRUE(prepared->isValid());
    combat().setProcessKnockedState(key(), 0); combat().setProcessKnockedState(key(), 3);
    EXPECT_FALSE(prepared->commit()); EXPECT_FALSE(combat().getUpIdle(key()));
    prepared = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor); ASSERT_TRUE(prepared->isValid());
    mAnimation->clear(); EXPECT_FALSE(prepared->commit());
    prepared = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor); ASSERT_TRUE(prepared->isValid());
    const auto current = snapshot(); combat().restore(current, world.getStore());
    EXPECT_FALSE(prepared->commit()); EXPECT_EQ(snapshot(), current);
    osg::ref_ptr<IdleAnimation> foreign = new IdleAnimation(world.getPlayerPtr(), mFixture.mResources);
    EXPECT_THROW(world.prepareOblivionGetUpIdleRequest(*foreign, mActor), std::invalid_argument);
    prepared = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor); ASSERT_TRUE(prepared->isValid());
    mAnimation = nullptr; EXPECT_FALSE(prepared->commit());
    EXPECT_EQ(snapshot(), current);
}

TEST_F(NativeGetUpRequestTest, SavedResourceAdmissionChecksWinningPathsBankAndExactLoadedHash)
{
    auto& world = mFixture.mWorld;
    auto prepared = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor); ASSERT_TRUE(prepared->commit());
    const auto saved = snapshot(); ASSERT_NO_THROW(world.validateOblivionSaveState(saved));
    const auto reject = [&](const auto& change) {
        auto bad = saved; change(bad.mNativeGetUpIdles.at(key()));
        EXPECT_THROW(world.validateOblivionSaveState(bad), std::exception);
        EXPECT_EQ(snapshot(), saved);
    };
    reject([](auto& idle) { idle.mIdle = ESM::FormKey::content("headless.esm", 0xabc); });
    reject([](auto& idle) { idle.mActorModel = "Cat\\Skeleton.nif"; });
    reject([](auto& idle) { idle.mModel = "Missing\\same.kf"; });
    reject([](auto& idle) { idle.mAssetHash[0] = idle.mAssetHash[0] == '0' ? '1' : '0'; });
    reject([](auto& idle) { idle.mIdle = ESM::FormKey::content("headless.esm", 0xb00);
        idle.mModel = "Cat\\IdleAnims\\Other.kf"; });
    write(kf(27)); mFixture.mResources.clearCache();
    EXPECT_THROW(world.validateOblivionSaveState(saved), std::invalid_argument);
    EXPECT_EQ(snapshot(), saved);
}

TEST_F(NativeGetUpRequestTest, NullSelectionAndUnavailableKfCannotPublishStagedRandomOrReadyState)
{
    auto& world = mFixture.mWorld; auto& store = world.getStore();
    const auto idleKey = ESM::FormKey::content("headless.esm", 0xa01);
    auto child = *store.search<ESM4::IdleAnimation>(idleKey);
    ESM4::PackageCondition absentCallback{}; absentCallback.mFunction = 0;
    child.mConditions = {absentCallback};
    store.getWritable<ESM4::IdleAnimation>().insertStatic(child, idleKey);
    const auto before = snapshot();
    auto missing = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor);
    EXPECT_TRUE(missing->selection().mIdle.isNull()); EXPECT_EQ(missing->selection().mRandomDraws, 1);
    EXPECT_FALSE(missing->isValid()); EXPECT_FALSE(missing->commit()); EXPECT_EQ(snapshot(), before);
    child.mConditions.clear(); store.getWritable<ESM4::IdleAnimation>().insertStatic(child, idleKey);
    ASSERT_TRUE(std::filesystem::remove(mFixture.mDirectory / "meshes/outside/same.kf"));
    mFixture.mResources.clearCache();
    EXPECT_THROW(world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor), std::exception);
    EXPECT_EQ(snapshot(), before);
}

TEST_F(NativeGetUpRequestTest, ZeroDrawAndMalformedRandomTransitionsKeepSharedAuthorityConsistent)
{
    auto& world = mFixture.mWorld; auto& store = world.getStore();
    const auto rootKey = ESM::FormKey::content("headless.esm", 0xa00);
    auto root = *store.search<ESM4::IdleAnimation>(rootKey); root.mConditions.clear();
    store.getWritable<ESM4::IdleAnimation>().insertStatic(root, rootKey);
    auto ready = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor);
    EXPECT_EQ(ready->selection().mRandomDraws, 0); ASSERT_TRUE(ready->commit());
    const auto idle = *combat().getUpIdle(key());
    combat().setProcessKnockedState(key(), 3); const auto before = snapshot();
    const auto seed = combat().combatRandomState();
    EXPECT_THROW(combat().prepareGetUpIdle(key(), idle, {seed, seed ^ 1, 0}), std::invalid_argument);
    EXPECT_FALSE(combat().prepareGetUpIdle(key(), idle, {seed ^ 1, seed ^ 1, 0}));
    auto bad = idle; bad.mBase = ESM::FormKey::content("headless.esm", 0xabc);
    EXPECT_THROW(combat().prepareGetUpIdle(key(), bad, {seed, seed, 0}), std::invalid_argument);
    EXPECT_EQ(snapshot(), before);
    auto direct = combat().prepareGetUpIdle(key(), idle, {seed, seed, 0}); ASSERT_TRUE(direct->isValid());
    auto copy = combat(); combat() = std::move(copy);
    EXPECT_FALSE(direct->commit()); EXPECT_EQ(snapshot(), before);
    direct = combat().prepareGetUpIdle(key(), idle, {seed, seed, 0}); ASSERT_TRUE(direct->commit());
    auto expected = before; expected.mNativeActorValues.front().mProcessKnockedState = 5;
    EXPECT_EQ(snapshot(), expected);
}

TEST_F(NativeGetUpRequestTest, PreparedSelectionRejectsChangedDrawContextOutsideActorValueAuthority)
{
    auto& world = mFixture.mWorld; auto& store = world.getStore();
    const auto rootKey = ESM::FormKey::content("headless.esm", 0xa00);
    auto root = *store.search<ESM4::IdleAnimation>(rootKey);
    ESM4::PackageCondition sheathed{}; sheathed.mFunction = 101;
    sheathed.mOperator = ESM4::ConditionOperator::Equal; sheathed.mComparisonValue = 0;
    root.mConditions.push_back(sheathed); store.getWritable<ESM4::IdleAnimation>().insertStatic(root, rootKey);
    auto prepared = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor); ASSERT_TRUE(prepared->isValid());
    const auto before = snapshot(); const auto values = *combat().findActorValues(key());
    mActor.getClass().getCreatureStats(mActor).setDrawState(MWMechanics::DrawState::Weapon);
    EXPECT_EQ(*combat().findActorValues(key()), values);
    EXPECT_EQ(world.getOblivionScriptIsWeaponOut(key()), 1.0);
    EXPECT_FALSE(prepared->isValid()); EXPECT_FALSE(prepared->commit()); EXPECT_EQ(snapshot(), before);
    const auto next = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor);
    EXPECT_TRUE(next->selection().mIdle.isNull()); EXPECT_EQ(next->selection().mRandomDraws, 1);
    EXPECT_EQ(combat().combatRandomState(), before.mCombatRngState);
}

TEST_F(NativeGetUpRequestTest, PreparedSelectionRejectsChangedAuthoredActorAndIdleModels)
{
    auto& world = mFixture.mWorld; auto& store = world.getStore();
    auto prepared = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor); ASSERT_TRUE(prepared->isValid());
    const auto before = snapshot(); const auto baseKey = ESM::FormKey::content("headless.esm", 0x800);
    const auto original = *store.search<ESM4::Npc>(baseKey);
    auto changed = original; changed.mModel = "DoG\\Other.NIF";
    store.getWritable<ESM4::Npc>().insertStatic(changed, baseKey);
    EXPECT_FALSE(prepared->commit()); EXPECT_EQ(snapshot(), before);
    store.getWritable<ESM4::Npc>().insertStatic(original, baseKey);
    prepared = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor); ASSERT_TRUE(prepared->isValid());
    const auto idleKey = ESM::FormKey::content("headless.esm", 0xa01);
    auto child = *store.search<ESM4::IdleAnimation>(idleKey); child.mModel = "Outside\\Other.kf";
    store.getWritable<ESM4::IdleAnimation>().insertStatic(child, idleKey);
    EXPECT_FALSE(prepared->commit()); EXPECT_EQ(snapshot(), before);
}

TEST_F(NativeGetUpRequestTest, PreparedRequestKeepsAcquiredKfWhenCacheAndFileBytesChange)
{
    auto& world = mFixture.mWorld;
    auto prepared = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor); ASSERT_TRUE(prepared->commit());
    const auto original = *combat().getUpIdle(key());
    combat().setProcessKnockedState(key(), 3);
    prepared = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor); ASSERT_TRUE(prepared->isValid());
    write(kf(27)); mFixture.mResources.clearCache();
    ASSERT_TRUE(prepared->commit()); EXPECT_EQ(combat().getUpIdle(key()), original);
    combat().setProcessKnockedState(key(), 3);
    prepared = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor); ASSERT_TRUE(prepared->commit());
    ASSERT_TRUE(combat().getUpIdle(key()));
    EXPECT_NE(combat().getUpIdle(key())->mAssetHash, original.mAssetHash);
    EXPECT_EQ(mAnimation->sources(), 0); EXPECT_FALSE(mAnimation->reactionRoot());
}

TEST_F(NativeGetUpRequestTest, WorldClearInvalidatesPreparationBeforeReadingRetiredActorPointers)
{
    auto& world = mFixture.mWorld; const auto actor = key();
    auto prepared = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor); ASSERT_TRUE(prepared->isValid());
    world.clear();
    EXPECT_NE(mAnimation->getObjectRoot(), nullptr);
    EXPECT_FALSE(prepared->isValid()); EXPECT_FALSE(prepared->commit());
    EXPECT_EQ(world.getOblivionCombatService()->findActorValues(actor), nullptr);
    EXPECT_FALSE(world.getOblivionCombatService()->getUpIdle(actor));
}

TEST_F(NativeGetUpRequestTest, FrozenOutgoingSimulationRejectsReadyPublicationUntilRetentionIsCancelled)
{
    auto& world = mFixture.mWorld;
    auto prepared = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor);
    ASSERT_TRUE(prepared->isValid()); const auto before = snapshot();
    {
        auto archive = world.retainSimulation();
        EXPECT_FALSE(prepared->isValid()); ASSERT_FALSE(prepared->commit());
        auto during = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor);
        EXPECT_FALSE(during->isValid()); ASSERT_FALSE(during->commit());
        EXPECT_EQ(snapshot(), before);
    }
    ASSERT_TRUE(prepared->isValid()); ASSERT_TRUE(prepared->commit());
    EXPECT_EQ(combat().getProcessKnockedState(key()), 5);
    EXPECT_FALSE(prepared->commit());
}

TEST_F(NativeGetUpRequestTest, SharedPlaybackGraphRestoresAndReadmitsEveryRetainedResource)
{
    write(kf(9, "Bip01", false, false, "SpecialIdle_GetUp")); mFixture.mResources.clearCache();
    auto& world = mFixture.mWorld;
    auto prepared = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor); ASSERT_TRUE(prepared->commit());
    const auto before = snapshot(); auto saved = before;
    auto& graph = saved.mNativeGetUpPlayback[key()];
    graph.mNextIdentity = 100;
    graph.mResources.emplace(2, saved.mNativeGetUpIdles.at(key()));
    graph.mRequests.mPrimary = ESM4::NativeIdleRequest{4, 2, 3, 2, true};
    graph.mRequests.mRetired[0] = ESM4::NativeIdleRequest{5, 2, 3, 2, true};
    graph.mRequests.mRetired[1] = ESM4::NativeIdleRequest{6, 2, 3, 3, true};
    ESM4::RuntimeGetUpSequence sequence; sequence.mResource = 2;
    graph.mSequences.emplace(3, sequence);
    graph.mCachedSequence = graph.mResidentSequence = graph.mActionSequence = 3;
    saved.mNativeAnimationClocks[key()] = 0;
    ASSERT_NO_THROW(saved.validate());
    ASSERT_NO_THROW(world.validateOblivionSaveState(saved));
    MWMechanics::OblivionCombatService restored;
    ASSERT_NO_THROW(restored.restore(saved, world.getStore()));
    EXPECT_EQ(restored.getUpPlayback(key()), graph);
    auto captured = saved; restored.capture(captured); EXPECT_EQ(captured, saved);
    EXPECT_FALSE(combat().getUpPlayback(key())); EXPECT_EQ(snapshot(), before);
    // A resource reachable only from retained requests is still readmitted.
    graph.mResources.emplace(9, graph.mResources.at(2));
    graph.mRequests.mRetired[1] = ESM4::NativeIdleRequest{10, 9, 11, 3, true};
    sequence.mResource = 9; sequence.mManaged = false; sequence.mState.mInactive = true;
    graph.mSequences.emplace(11, sequence);
    ASSERT_NO_THROW(world.validateOblivionSaveState(saved));
    graph.mResources.at(9).mAssetHash[0] = graph.mResources.at(9).mAssetHash[0] == '0' ? '1' : '0';
    EXPECT_THROW(world.validateOblivionSaveState(saved), std::invalid_argument);
    EXPECT_EQ(snapshot(), before);
    restored.setProcessKnockedState(key(), 3);
    auto beforeGuard = saved; restored.capture(beforeGuard);
    const auto seed = restored.combatRandomState();
    EXPECT_THROW(restored.prepareGetUpIdle(key(), saved.mNativeGetUpIdles.at(key()), {seed, seed, 0}),
        std::invalid_argument);
    auto afterGuard = saved; restored.capture(afterGuard);
    EXPECT_EQ(afterGuard, beforeGuard);
    restored.clear(); EXPECT_FALSE(restored.getUpPlayback(key()));
}

TEST_F(NativeGetUpRequestTest, ReadyGraphDoesNotRequirePlayableMetadataButAttachedGraphDoes)
{
    auto& world = mFixture.mWorld;
    auto prepared = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor); ASSERT_TRUE(prepared->commit());
    auto saved = snapshot(); auto& graph = saved.mNativeGetUpPlayback[key()];
    graph.mNextIdentity = 10; graph.mResources.emplace(2, saved.mNativeGetUpIdles.at(key()));
    graph.mRequests.mPrimary = ESM4::makeNativeIdleRequest(4, 2);
    ASSERT_NO_THROW(world.validateOblivionSaveState(saved));
    graph.mRequests.mPrimary = ESM4::NativeIdleRequest{4, 2, 3, 2, true};
    ESM4::RuntimeGetUpSequence sequence; sequence.mResource = 2; graph.mSequences.emplace(3, sequence);
    saved.mNativeAnimationClocks[key()] = 0;
    ASSERT_NO_THROW(saved.validate());
    // Fixture's AuthoredGetUpLabel is not native SpecialIdle metadata.
    EXPECT_THROW(world.validateOblivionSaveState(saved), std::invalid_argument);
}

TEST_F(NativeGetUpRequestTest, ReadmissionPreservesColdPoseCancellationAndCachedInitialization)
{
    write(kf(9, "Bip01", false, true, "SpecialIdle_GetUp")); mFixture.mResources.clearCache();
    auto& world = mFixture.mWorld;
    auto ready = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor); ASSERT_TRUE(ready->commit());
    const auto before = snapshot();
    for (int stage = 0; stage < 3; ++stage)
    {
        SCOPED_TRACE(stage);
        auto saved = before; auto& graph = saved.mNativeGetUpPlayback[key()];
        graph.mNextIdentity = 20; graph.mResources.emplace(2, saved.mNativeGetUpIdles.at(key()));
        graph.mRequests.mPrimary = ESM4::NativeIdleRequest{4, 2, 3, 2, true};
        graph.mCachedSequence = graph.mActionSequence = 3;
        ESM4::RuntimeGetUpSequence sequence; sequence.mResource = 2;
        if (stage == 1)
        {
            sequence.mState.mTiming = {false, std::nullopt, std::nullopt, .1f, 0, 0, 0};
            graph.mResidentSequence = 3;
        }
        else
        {
            ESM4::NativeClampSequenceState pose;
            pose.mTiming = {true, 0, std::nullopt, .1f, std::nullopt, 0, 0};
            sequence.mState = ESM4::retireNativeClampSequence(pose, 1, stage == 0 ? .1f : 0.f, 0);
            if (stage == 0)
            {
                sequence.mTransitionFrame = 0;
                sequence.mPoseSnapshot = ESM4::RuntimeMeleePoseSnapshot{0, {}};
            }
        }
        graph.mSequences.emplace(3, sequence); saved.mNativeAnimationClocks[key()] = .5f;
        ASSERT_NO_THROW(saved.validate());
        ASSERT_NO_THROW(world.validateOblivionSaveState(saved));
        MWMechanics::OblivionCombatService restored; ASSERT_NO_THROW(restored.restore(saved, world.getStore()));
        auto captured = saved; restored.capture(captured);
        EXPECT_EQ(captured, saved); EXPECT_EQ(snapshot(), before);
    }
}
