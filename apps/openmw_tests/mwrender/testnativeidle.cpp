#include "apps/openmw_tests/mwworld/oblivionworldfixture.hpp"
#include "apps/openmw/mwclass/npc.hpp"
#include "apps/openmw/mwclass/esm4npc.hpp"
#include "apps/openmw/mwworld/oblivionnativeidle.hpp"
#include "apps/openmw/mwworld/worldmodel.hpp"
#include "apps/openmw/mwworld/cellstore.hpp"
#include "apps/openmw/mwmechanics/oblivioncombat.hpp"
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
    std::string idle(std::uint32_t id, std::uint32_t parent, const std::string& model)
    {
        return record(ESM4::REC_IDLE, id, sub(ESM::fourCC("EDID"), std::string("ExactIdle") + '\0')
            + sub(ESM::fourCC("MODL"), model + '\0') + sub(ESM::fourCC("ANAM"), std::string(1, 5))
            + sub(ESM::fourCC("DATA"), bytes(parent) + bytes(0u)));
    }
    std::string kf(float translation, std::string_view root = "Bip01", bool duplicate = false,
        bool trackless = false)
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
        data += text("AuthoredGetUpLabel") + bytes(trackless ? 0u : 1u) + bytes(1u);
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
