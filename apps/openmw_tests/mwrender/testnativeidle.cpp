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
#include <components/nifbullet/actorragdollphysics.hpp>
#include <components/misc/osguservalues.hpp>
#include <components/sceneutil/skeleton.hpp>
#include <components/vfs/filesystemarchive.hpp>
#include <gtest/gtest.h>
#include <osg/FrameStamp>
#include <osgUtil/UpdateVisitor>
#include <fstream>
#include <future>
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
        std::string projectedGroup() const { return mNativeSequenceLayers.at(0).mGroup; }
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

TEST_F(NativeIdleResourceTest, NativeCacheSharesCaseOnlyKeysButKeepsSlashAliasesDistinct)
{
    MWRender::NativeIdleResourceCache cache(mFixture.mResources);
    auto one = cache.acquire("one\\same.kf"); ASSERT_TRUE(one);
    EXPECT_EQ(cache.acquire("ONE\\SAME.KF"), one);
    EXPECT_EQ(cache.acquire(std::string("one\\same.kf\0ignored", 19)), one);
    auto slash = cache.acquire("one/same.kf"); ASSERT_TRUE(slash);
    EXPECT_NE(slash, one);
    EXPECT_EQ(slash->model(), one->model());
    EXPECT_EQ(slash->fileHash(), one->fileHash());
    EXPECT_EQ(cache.size(), 2);
    one.reset(); EXPECT_EQ(cache.size(), 1);
    slash.reset(); EXPECT_EQ(cache.size(), 0);
}

TEST_F(NativeIdleResourceTest, NativeCacheRetainsInfoUntilLastOwnerButRendererAliasesDoNotOwnIt)
{
    MWRender::NativeIdleResourceCache cache(mFixture.mResources);
    auto resource = cache.acquire("one\\same.kf"); ASSERT_TRUE(resource);
    const auto oldHash = resource->fileHash();
    std::weak_ptr<const MWRender::Animation::NativeIdleResource> old = resource;
    auto a = animation(); auto binding = a->prepareNativeIdleSource(*resource);
    ASSERT_TRUE(binding); ASSERT_TRUE(binding->commit());
    write("meshes/one/same.kf", kf(27)); mFixture.mResources.clearCache();
    EXPECT_EQ(cache.acquire("ONE\\SAME.KF"), resource);
    resource.reset(); EXPECT_TRUE(old.expired()); EXPECT_EQ(cache.size(), 0);
    auto current = cache.acquire("one\\same.kf"); ASSERT_TRUE(current);
    EXPECT_NE(current->fileHash(), oldHash);
    EXPECT_EQ(cache.size(), 1);
    // The detached loaded-info identity expires even though backend source
    // data remains owned by an actor's registered alias and prepared token.
    a->sample(binding->group());
    EXPECT_FLOAT_EQ(a->mBone->getControllerTransform().mTranslation.x(), 9);
    auto b = animation(); auto next = b->prepareNativeIdleSource(*current);
    ASSERT_TRUE(next); ASSERT_TRUE(next->commit()); b->sample(next->group());
    EXPECT_FLOAT_EQ(b->mBone->getControllerTransform().mTranslation.x(), 27);
}

TEST_F(NativeIdleResourceTest, SourceArtifactRetainsExactBytesWithoutOwningLoadedInfo)
{
    MWRender::NativeIdleResourceCache cache(mFixture.mResources);
    auto info = cache.acquire("one\\same.kf"); ASSERT_TRUE(info);
    const auto hash = info->fileHash();
    const std::weak_ptr<const MWRender::Animation::NativeIdleResource> identity = info;
    write("meshes/one/same.kf", kf(27)); mFixture.mResources.clearCache();
    // Clone after backend eviction/replacement. The source must retain the
    // resident's exact frames, without acquiring a new native info lease.
    auto source = info->retainSourceArtifact(); ASSERT_TRUE(source);
    EXPECT_NE(source, info); EXPECT_EQ(source->model(), info->model());
    EXPECT_EQ(source->fileHash(), hash); EXPECT_EQ(cache.find("one\\same.kf"), info);
    info.reset(); EXPECT_TRUE(identity.expired()); EXPECT_EQ(cache.size(), 0);
    auto replacement = cache.acquire("one\\same.kf"); ASSERT_TRUE(replacement);
    EXPECT_NE(replacement->fileHash(), hash);
    auto oldActor = animation(); auto oldBinding = oldActor->prepareNativeIdleSource(*source);
    ASSERT_TRUE(oldBinding); ASSERT_TRUE(oldBinding->commit()); source.reset();
    oldActor->sample(oldBinding->group());
    EXPECT_FLOAT_EQ(oldActor->mBone->getControllerTransform().mTranslation.x(), 9);
    auto newActor = animation(); auto newBinding = newActor->prepareNativeIdleSource(*replacement);
    ASSERT_TRUE(newBinding); ASSERT_TRUE(newBinding->commit());
    replacement.reset(); EXPECT_EQ(cache.size(), 0);
    newActor->sample(newBinding->group());
    EXPECT_FLOAT_EQ(newActor->mBone->getControllerTransform().mTranslation.x(), 27);
}

TEST_F(NativeIdleResourceTest, NativeCacheColdJobPublicationUsesFirstResidentEvenWhenLoadedBytesDiffer)
{
    MWRender::NativeIdleResourceCache cache(mFixture.mResources);
    EXPECT_FALSE(cache.find("one\\same.kf"));
    auto firstLoad = MWRender::Animation::acquireNativeIdleResource(mFixture.mResources, "one/same.kf");
    ASSERT_TRUE(firstLoad); EXPECT_FALSE(cache.find("one\\same.kf"));
    write("meshes/one/same.kf", kf(27)); mFixture.mResources.clearCache();
    auto secondLoad = MWRender::Animation::acquireNativeIdleResource(mFixture.mResources, "one/same.kf");
    ASSERT_TRUE(secondLoad); ASSERT_NE(firstLoad->fileHash(), secondLoad->fileHash());
    auto first = cache.publishLoaded("one\\same.kf", *firstLoad); ASSERT_TRUE(first);
    auto second = cache.publishLoaded("ONE\\SAME.KF", *secondLoad);
    EXPECT_EQ(first, second); EXPECT_EQ(first->fileHash(), firstLoad->fileHash());
    EXPECT_EQ(cache.find("one\\same.kf"), first);
    // Save admission audits its own expected bytes. It must not silently adopt
    // a resident with another fingerprint, unlike native job reconciliation.
    EXPECT_THROW(cache.admit("one\\same.kf", *secondLoad), std::invalid_argument);
    EXPECT_EQ(cache.size(), 1);
    first.reset(); EXPECT_EQ(cache.size(), 1);
    second.reset(); EXPECT_EQ(cache.size(), 0);
    EXPECT_FALSE(cache.find("one\\same.kf"));
}

TEST_F(NativeIdleResourceTest, NativeCacheAdmissionChecksFreshBytesAndRejectsResidentConflicts)
{
    MWRender::NativeIdleResourceCache cache(mFixture.mResources);
    auto resident = cache.acquire("one\\same.kf"); ASSERT_TRUE(resident);
    auto validated = MWRender::Animation::acquireNativeIdleResource(mFixture.mResources, "one/same.kf");
    ASSERT_TRUE(validated);
    EXPECT_EQ(cache.admit("ONE\\SAME.KF", *validated), resident);
    auto foreign = MWRender::Animation::acquireNativeIdleResource(mFixture.mResources, "two/same.kf");
    ASSERT_TRUE(foreign);
    EXPECT_THROW(cache.admit("one\\same.kf", *foreign), std::invalid_argument);
    write("meshes/one/same.kf", kf(27)); mFixture.mResources.clearCache();
    validated = MWRender::Animation::acquireNativeIdleResource(mFixture.mResources, "one/same.kf");
    ASSERT_TRUE(validated); ASSERT_NE(validated->fileHash(), resident->fileHash());
    EXPECT_THROW(cache.admit("one\\same.kf", *validated), std::invalid_argument);
    EXPECT_EQ(cache.acquire("one\\same.kf"), resident); EXPECT_EQ(cache.size(), 1);
    resident.reset(); EXPECT_EQ(cache.size(), 0);
    auto admitted = cache.admit("one\\same.kf", *validated);
    ASSERT_TRUE(admitted); EXPECT_EQ(admitted->fileHash(), validated->fileHash());
    EXPECT_EQ(cache.acquire("ONE\\SAME.KF"), admitted);
}

TEST_F(NativeIdleResourceTest, NativeCacheResourcesCanOutliveProviderFacade)
{
    std::shared_ptr<const MWRender::Animation::NativeIdleResource> retained;
    {
        MWRender::NativeIdleResourceCache cache(mFixture.mResources);
        retained = cache.acquire("one\\same.kf"); ASSERT_TRUE(retained);
        EXPECT_EQ(cache.size(), 1);
    }
    auto a = animation(); auto binding = a->prepareNativeIdleSource(*retained);
    ASSERT_TRUE(binding); ASSERT_TRUE(binding->commit()); retained.reset();
    a->sample(binding->group());
    EXPECT_FLOAT_EQ(a->mBone->getControllerTransform().mTranslation.x(), 9);
}

TEST_F(NativeIdleResourceTest, NativeCacheDoesNotRetainMissingMalformedOrAmbiguousRequests)
{
    MWRender::NativeIdleResourceCache cache(mFixture.mResources);
    EXPECT_FALSE(cache.acquire("absent/same.kf"));
    EXPECT_FALSE(cache.acquire("duplicate.kf"));
    EXPECT_FALSE(cache.acquire("same.nif"));
    EXPECT_THROW(cache.acquire(std::string(260, 'x') + ".kf"), std::invalid_argument);
    EXPECT_THROW(cache.acquire("one/\xff.kf"), std::invalid_argument);
    EXPECT_EQ(cache.size(), 0);
    auto valid = cache.acquire("one/same.kf"); ASSERT_TRUE(valid); EXPECT_EQ(cache.size(), 1);
}

TEST_F(NativeIdleResourceTest, NativeCacheConcurrentAcquisitionSharesOneLiveInfoAndReclaimsEntries)
{
    MWRender::NativeIdleResourceCache cache(mFixture.mResources);
    using Resource = std::shared_ptr<const MWRender::Animation::NativeIdleResource>;
    std::vector<std::future<Resource>> workers;
    for (int i = 0; i < 4; ++i)
        workers.push_back(std::async(std::launch::async, [&cache, i] {
            Resource retained;
            for (int j = 0; j < 64; ++j)
                retained = cache.acquire(i % 2 ? "one\\same.kf" : "ONE\\SAME.KF");
            return retained;
        }));
    std::vector<Resource> resources;
    for (auto& worker : workers) resources.push_back(worker.get());
    for (const auto& resource : resources) { ASSERT_TRUE(resource); EXPECT_EQ(resource, resources.front()); }
    EXPECT_EQ(cache.size(), 1);
    resources.clear(); EXPECT_EQ(cache.size(), 0);
    // Exercise expiry/replacement while another thread may be waiting in the
    // old resource's destructor. The old identity must not erase its successor.
    workers.clear();
    for (int i = 0; i < 4; ++i)
        workers.push_back(std::async(std::launch::async, [&cache] {
            for (int j = 0; j < 64; ++j)
            {
                auto resource = cache.acquire("one\\same.kf");
                if (!resource) throw std::runtime_error("native cache churn lost its resource");
            }
            return Resource{};
        }));
    for (auto& worker : workers) EXPECT_FALSE(worker.get());
    EXPECT_EQ(cache.size(), 0);
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

TEST_F(NativeIdleResourceTest, BatchPublishesDistinctAndRepeatedAliasesTogetherWithoutPlayback)
{
    auto a = animation();
    auto one = a->acquireNativeIdleResource("one/same.kf");
    auto two = a->acquireNativeIdleResource("two/same.kf");
    ASSERT_TRUE(one); ASSERT_TRUE(two);
    const auto original = one->metadata();
    const MWRender::Animation::NativeIdleResource* resources[] = {one.get(), two.get(), one.get()};
    auto pending = a->prepareNativeIdleSources(resources); ASSERT_TRUE(pending);
    ASSERT_EQ(pending->size(), 3);
    EXPECT_NE(pending->group(0), pending->group(1));
    EXPECT_EQ(pending->group(0), pending->group(2));
    EXPECT_EQ(pending->model(1).value(), "meshes/two/same.kf");
    EXPECT_EQ(pending->fileHash(0), one->fileHash());
    auto boundMetadata = original;
    boundMetadata.mGroup = pending->group(0);
    EXPECT_EQ(pending->metadata(0), boundMetadata);
    EXPECT_THROW(pending->group(3), std::out_of_range);
    EXPECT_EQ(a->sources(), 0); EXPECT_FALSE(a->reactionRoot());
    EXPECT_FALSE(a->hasAnimation(pending->group(0)));
    EXPECT_FALSE(a->hasAnimation(pending->group(1)));
    const auto* callbacks = a->mBone->getUpdateCallback();
    ASSERT_TRUE(pending->commit());
    EXPECT_FALSE(pending->isValid()); EXPECT_FALSE(pending->commit());
    EXPECT_EQ(a->sources(), 2); EXPECT_EQ(a->reactionRoot(), 4);
    EXPECT_EQ(a->mBone->getUpdateCallback(), callbacks);
    EXPECT_FALSE(a->isPlaying(pending->group(0))); EXPECT_FALSE(a->isPlaying(pending->group(1)));
    a->sample(pending->group(0)); EXPECT_FLOAT_EQ(a->mBone->getControllerTransform().mTranslation.x(), 9);
    a->sample(pending->group(1)); EXPECT_FLOAT_EQ(a->mBone->getControllerTransform().mTranslation.x(), 17);
    auto reused = a->prepareNativeIdleSources(resources); ASSERT_TRUE(reused);
    auto independent = a->prepareNativeIdleSource(*one); ASSERT_TRUE(independent);
    ASSERT_TRUE(reused->commit()); EXPECT_EQ(a->sources(), 2);
    EXPECT_TRUE(independent->isValid()); EXPECT_TRUE(independent->commit());
    EXPECT_EQ(one->metadata(), original);
}

TEST_F(NativeIdleResourceTest, BatchKeepsFirstRootIncludingNullAndExistingBinding)
{
    auto a = animation();
    auto missing = a->acquireNativeIdleResource("missingroot.kf");
    auto one = a->acquireNativeIdleResource("one/same.kf");
    auto trackless = a->acquireNativeIdleResource("empty.kf");
    ASSERT_TRUE(missing); ASSERT_TRUE(one); ASSERT_TRUE(trackless);
    const MWRender::Animation::NativeIdleResource* nullFirst[] = {missing.get(), one.get()};
    auto unbound = a->prepareNativeIdleSources(nullFirst); ASSERT_TRUE(unbound); ASSERT_TRUE(unbound->commit());
    EXPECT_FALSE(a->reactionRoot());
    const MWRender::Animation::NativeIdleResource* rootFirst[] = {trackless.get(), missing.get(), one.get()};
    auto b = animation(); auto bound = b->prepareNativeIdleSources(rootFirst);
    ASSERT_TRUE(bound); ASSERT_TRUE(bound->commit()); EXPECT_EQ(b->reactionRoot(), 4);
    auto stillNull = a->prepareNativeIdleSources(rootFirst); ASSERT_TRUE(stillNull); ASSERT_TRUE(stillNull->commit());
    EXPECT_FALSE(a->reactionRoot());
}

TEST_F(NativeIdleResourceTest, FailedOrStaleBatchCannotPublishEarlierEntries)
{
    auto a = animation();
    auto one = a->acquireNativeIdleResource("one/same.kf");
    auto two = a->acquireNativeIdleResource("two/same.kf");
    ASSERT_TRUE(one); ASSERT_TRUE(two);
    const MWRender::Animation::NativeIdleResource* invalid[] = {one.get(), nullptr};
    EXPECT_FALSE(a->prepareNativeIdleSources(invalid));
    EXPECT_EQ(a->sources(), 0); EXPECT_FALSE(a->reactionRoot());
    const MWRender::Animation::NativeIdleResource* resources[] = {one.get(), two.get()};
    auto pending = a->prepareNativeIdleSources(resources); ASSERT_TRUE(pending);
    const auto oneGroup = pending->group(0); const auto twoGroup = pending->group(1);
    a->source("meshes/same.kf", twoGroup);
    const auto competitorRoot = a->reactionRoot();
    ASSERT_EQ(competitorRoot, 4);
    EXPECT_FALSE(pending->commit()); EXPECT_FALSE(a->prepareNativeIdleSources(resources));
    EXPECT_FALSE(a->hasAnimation(oneGroup)); EXPECT_EQ(a->sources(), 1); EXPECT_EQ(a->reactionRoot(), competitorRoot);
    auto b = animation(); auto cleared = b->prepareNativeIdleSources(resources); ASSERT_TRUE(cleared);
    b->clear(); EXPECT_FALSE(cleared->commit()); EXPECT_EQ(b->sources(), 0);
    auto replaced = b->prepareNativeIdleSources(resources); ASSERT_TRUE(replaced);
    b->replaceRoot(); EXPECT_FALSE(replaced->commit()); EXPECT_EQ(b->sources(), 0);
    auto c = animation(); auto expired = c->prepareNativeIdleSources(resources); ASSERT_TRUE(expired);
    c = nullptr; EXPECT_FALSE(expired->isValid()); EXPECT_FALSE(expired->commit());
}

TEST_F(NativeIdleResourceTest, BatchUsesBothRetainedVersionsWithoutLookingUpReplacedFile)
{
    auto a = animation(); auto old = a->acquireNativeIdleResource("one/same.kf"); ASSERT_TRUE(old);
    write("meshes/one/same.kf", kf(27)); mFixture.mResources.clearCache();
    auto current = a->acquireNativeIdleResource("one/same.kf"); ASSERT_TRUE(current);
    EXPECT_NE(old->fileHash(), current->fileHash());
    std::filesystem::remove(mFixture.mDirectory / "meshes/one/same.kf");
    mFixture.mResources.clearCache();
    const MWRender::Animation::NativeIdleResource* resources[] = {old.get(), current.get()};
    auto pending = a->prepareNativeIdleSources(resources); ASSERT_TRUE(pending);
    EXPECT_EQ(pending->model(0), pending->model(1));
    EXPECT_NE(pending->group(0), pending->group(1));
    EXPECT_NE(pending->fileHash(0), pending->fileHash(1));
    old.reset(); current.reset();
    ASSERT_TRUE(pending->commit()); EXPECT_EQ(a->sources(), 2);
    a->sample(pending->group(0)); EXPECT_FLOAT_EQ(a->mBone->getControllerTransform().mTranslation.x(), 9);
    a->sample(pending->group(1)); EXPECT_FLOAT_EQ(a->mBone->getControllerTransform().mTranslation.x(), 27);
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
        static std::string records(bool alias)
        {
            const std::array<std::uint32_t, 6> condition{0x60, std::bit_cast<std::uint32_t>(0.f), 77, 0, 0, 0};
            return idle(0xa00, 0, "Dog\\IdleAnims", sub(ESM::fourCC("CTDA"), bytes(condition)))
                + idle(0xa01, 0xa00, "Outside\\same.kf")
                + idle(0xb00, 0, "Cat\\IdleAnims\\Other.kf")
                + (alias ? idle(0xa02, 0xa00, "OUTSIDE\\SAME.KF",
                    sub(ESM::fourCC("CTDA"), bytes(std::array<std::uint32_t, 6>{0,
                        std::bit_cast<std::uint32_t>(1.f), 46, 0, 0, 0}))) : "");
        }
        TestingOpenMW::NativeWorldFixture mFixture;
        MWWorld::Ptr mActor;
        osg::ref_ptr<IdleAnimation> mAnimation;
        NativeGetUpRequestTest() : NativeGetUpRequestTest(false) {}
        explicit NativeGetUpRequestTest(bool alias)
            : mFixture{false, false, false, {}, {}, records(alias), alias ? 4u : 3u}
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

    struct NativeGetUpSelectionTest : NativeGetUpRequestTest
    {
        NativeGetUpSelectionTest() : NativeGetUpRequestTest(true) {}

        ESM4::RuntimeState selectedGraph()
        {
            write(kf(9, "Bip01", false, false, "SpecialIdle_GetUp")); mFixture.mResources.clearCache();
            auto prepared = mFixture.mWorld.prepareOblivionGetUpIdleRequest(*mAnimation, mActor);
            if (!prepared->commit()) throw std::runtime_error("selection fixture request failed");
            auto saved = snapshot(); auto& graph = saved.mNativeGetUpPlayback[key()];
            graph.mNextIdentity = 100; graph.mResources.emplace(2, saved.mNativeGetUpIdles.at(key()));
            graph.mRequests.mPrimary = ESM4::NativeIdleRequest{4, 2, 3, 2, true};
            graph.mRequests.mRetired[0] = ESM4::NativeIdleRequest{5, 2, 3, 2, true};
            ESM4::RuntimeGetUpSequence sequence; sequence.mResource = 2;
            graph.mSequences.emplace(3, sequence);
            graph.mCachedSequence = graph.mResidentSequence = graph.mActionSequence = 3;
            saved.mNativeAnimationClocks[key()] = 0;
            auto selection = graph.mResources.at(2);
            selection.mIdle = ESM::FormKey::content("headless.esm", 0xa02); selection.mModel = "OUTSIDE\\SAME.KF";
            graph.mRequestSelections.emplace(5, selection);
            return saved;
        }
    };

    struct NativeGetUpPlaybackTest : NativeGetUpRequestTest
    {
        NativeGetUpPlaybackTest()
        {
            write(kf(9, "Bip01", false, false, "SpecialIdle_GetUp")); mFixture.mResources.clearCache();
            auto ready = mFixture.mWorld.prepareOblivionGetUpIdleRequest(*mAnimation, mActor);
            if (!ready->commit()) throw std::runtime_error("playback fixture resource acquisition failed");
            combat().setProcessKnockedState(key(), 3);
        }
        MWMechanics::OblivionCombatService::GetUpPlaybackUpdate update()
        {
            MWMechanics::OblivionCombatService::GetUpPlaybackUpdate next;
            next.mIdle = combat().getUpIdle(key()); next.mClock = 0;
            next.mPlayback.mNextIdentity = 3;
            next.mPlayback.mResources.emplace(2, *next.mIdle);
            next.mPlayback.mRequests.mPrimary = ESM4::makeNativeIdleRequest(1, 2);
            next.mKnockedState = 5; next.mAction = -1;
            const auto seed = combat().combatRandomState(); next.mRandom = {seed, seed, 0};
            return next;
        }
        MWMechanics::OblivionCombatService::GetUpPlaybackUpdate pendingUpdate()
        {
            auto next = update(); const auto source = *next.mIdle;
            next.mIdle.reset(); next.mClock.reset(); next.mPlayback = {}; next.mPlayback.mNextIdentity = 2;
            next.mPlayback.mRequests.mPrimary = ESM4::makeNativeIdleRequest(1, 0);
            next.mPlayback.mPendingRequests.emplace(1, ESM4::RuntimeGetUpPendingRequest{
                source.mBase, source.mIdle, source.mActorModel, source.mModel, true});
            return next;
        }
    };
}

TEST_F(NativeGetUpPlaybackTest, PendingPublicationAndDeliveryRetainRequestSelectionWithoutReplayingRandom)
{
    auto pending = pendingUpdate(); auto ready = update(); const auto before = snapshot();
    const auto contact = combat().allocateAction(key()); const auto withContact = snapshot();
    auto prepared = combat().prepareGetUpPlayback(key(), pending); ASSERT_TRUE(prepared->isValid());
    auto competing = combat().prepareGetUpPlayback(key(), pending);
    EXPECT_EQ(snapshot(), withContact); ASSERT_TRUE(prepared->commit()); EXPECT_FALSE(competing->commit());
    EXPECT_TRUE(combat().isActionPending(contact, key()));
    EXPECT_TRUE(combat().getUpPlayback(key())->mResources.empty()); EXPECT_FALSE(combat().getUpIdle(key()));
    EXPECT_THROW(combat().getUpPlayback(key())->requestSelection(1), std::invalid_argument);
    const auto waiting = snapshot();
    EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(waiting.serializeBinary()), waiting);
    EXPECT_EQ(waiting.mCombatRngState, before.mCombatRngState);
    auto retarget = ready; retarget.mPlayback.mResources.at(2).mModel = "OUTSIDE\\SAME.KF";
    EXPECT_THROW(combat().prepareGetUpPlayback(key(), retarget), std::invalid_argument);
    auto changed = pending; changed.mPlayback.mPendingRequests.at(1).mActorModel = "Other\\Skeleton.NIF";
    EXPECT_THROW(combat().prepareGetUpPlayback(key(), changed), std::invalid_argument);
    EXPECT_EQ(snapshot(), waiting);
    prepared = combat().prepareGetUpPlayback(key(), ready); ASSERT_TRUE(prepared->isValid());
    EXPECT_EQ(snapshot(), waiting); ASSERT_TRUE(prepared->commit());
    const auto delivered = snapshot();
    EXPECT_EQ(combat().getUpPlayback(key())->mRequests.mPrimary->mIdentity, 1);
    EXPECT_EQ(combat().getUpPlayback(key())->mRequests.mPrimary->mStatus, 1);
    EXPECT_TRUE(combat().getUpPlayback(key())->mPendingRequests.empty());
    EXPECT_EQ(delivered.mCombatRngState, before.mCombatRngState);
    EXPECT_TRUE(combat().isActionPending(contact, key()));
    auto legacy = delivered; legacy.mVersion = 73;
    EXPECT_NO_THROW(legacy.serializeBinary()); // Loaded state has no v74 ownership to lose.
    EXPECT_THROW(combat().prepareGetUpPlayback(key(), pending), std::invalid_argument);
}

TEST_F(NativeGetUpPlaybackTest, CompletedEmptyLoadSurvivesRestoreWithoutRestartOrLateDelivery)
{
    auto pending = pendingUpdate(); const auto ready = update();
    auto prepared = combat().prepareGetUpPlayback(key(), pending); ASSERT_TRUE(prepared->commit());
    pending.mPlayback.mPendingRequests.at(1).mLoadPending = false;
    prepared = combat().prepareGetUpPlayback(key(), pending); ASSERT_TRUE(prepared->commit());
    const auto failed = snapshot(); combat().restore(failed, mFixture.mWorld.getStore());
    EXPECT_EQ(snapshot(), failed); EXPECT_FALSE(combat().getUpPlayback(key())->mPendingRequests.at(1).mLoadPending);
    auto retry = pending; retry.mPlayback.mPendingRequests.at(1).mLoadPending = true;
    EXPECT_THROW(combat().prepareGetUpPlayback(key(), retry), std::invalid_argument);
    EXPECT_THROW(combat().prepareGetUpPlayback(key(), ready), std::invalid_argument);
    auto legacy = failed; legacy.mVersion = 73; const auto unchanged = legacy;
    EXPECT_THROW(combat().capture(legacy), std::invalid_argument); EXPECT_EQ(legacy, unchanged);
    EXPECT_EQ(snapshot(), failed);
    // A new request is distinct from retrying the completed job on identity1.
    auto replacement = pending; replacement.mPlayback.mRequests.mPrimary = ESM4::makeNativeIdleRequest(2, 0);
    auto source = replacement.mPlayback.mPendingRequests.at(1); source.mLoadPending = true;
    replacement.mPlayback.mPendingRequests.clear(); replacement.mPlayback.mPendingRequests.emplace(2, source);
    replacement.mPlayback.mNextIdentity = 3;
    prepared = combat().prepareGetUpPlayback(key(), replacement); ASSERT_TRUE(prepared->commit());
    EXPECT_TRUE(combat().getUpPlayback(key())->mPendingRequests.at(2).mLoadPending);
}

TEST_F(NativeGetUpPlaybackTest, PendingOwnershipCannotBePublishedForLowProcess)
{
    auto pending = pendingUpdate(); auto low = snapshot();
    auto& values = low.mNativeActorValues.front(); values.mProcess = ESM4::ActorValueProcess::Low;
    values.mProcessKnockedState.reset(); values.mProcessAction.reset(); low.mNativeGetUpIdles.clear();
    combat().restore(low, mFixture.mWorld.getStore());
    pending.mKnockedState.reset(); pending.mAction.reset();
    EXPECT_THROW(combat().prepareGetUpPlayback(key(), pending), std::invalid_argument);
    EXPECT_EQ(snapshot(), low);
}

TEST_F(NativeGetUpRequestTest, PendingReadmissionChecksContentWithoutOpeningMissingKfOrStartingIo)
{
    auto& world = mFixture.mWorld;
    const auto selected = world.prepareOblivionGetUpIdle(mActor); const auto before = snapshot();
    auto saved = before; saved.mNativeActorValues.front().mProcessKnockedState = 5;
    auto& graph = saved.mNativeGetUpPlayback[key()]; graph.mNextIdentity = 2;
    graph.mRequests.mPrimary = ESM4::makeNativeIdleRequest(1, 0);
    graph.mPendingRequests.emplace(1, ESM4::RuntimeGetUpPendingRequest{combat().findActorValues(key())->mBase,
        selected.mIdle, selected.mActorModel, selected.mModel, true});
    ASSERT_TRUE(std::filesystem::remove(mFixture.mDirectory / "meshes/outside/same.kf"));
    mFixture.mResources.clearCache();
    for (const bool queued : {true, false})
    {
        graph.mPendingRequests.at(1).mLoadPending = queued;
        ASSERT_NO_THROW(saved.validate());
        EXPECT_NO_THROW(world.validateOblivionSaveState(saved));
        EXPECT_EQ(snapshot(), before); EXPECT_EQ(mAnimation->sources(), 0); EXPECT_FALSE(mAnimation->reactionRoot());
    }
    auto wrong = saved; wrong.mNativeGetUpPlayback.at(key()).mPendingRequests.at(1).mActorModel = "Other\\Skeleton.NIF";
    EXPECT_THROW(world.validateOblivionSaveState(wrong), std::invalid_argument);
    wrong = saved; auto& otherBank = wrong.mNativeGetUpPlayback.at(key()).mPendingRequests.at(1);
    otherBank.mIdle = ESM::FormKey::content("headless.esm", 0xb00); otherBank.mModel = "Cat\\IdleAnims\\Other.kf";
    EXPECT_THROW(world.validateOblivionSaveState(wrong), std::invalid_argument);
    EXPECT_EQ(snapshot(), before);
    combat().restore(saved, world.getStore());
    EXPECT_EQ(snapshot(), saved);
}

TEST_F(NativeGetUpPlaybackTest, PublishesRequestGraphProcessFieldsClockAndRandomTogether)
{
    const auto contact = combat().allocateAction(key());
    const auto before = snapshot(); auto next = update();
    next.mRandom.mDraws = 1;
    next.mRandom.mNextState = next.mRandom.mExpectedState * 214013u + 2531011u;
    const auto expectedGraph = next.mPlayback; const auto random = next.mRandom;
    auto prepared = combat().prepareGetUpPlayback(key(), next); ASSERT_TRUE(prepared); ASSERT_TRUE(prepared->isValid());
    auto competing = combat().prepareGetUpPlayback(key(), next); ASSERT_TRUE(competing->isValid());
    EXPECT_EQ(prepared->state(), expectedGraph); EXPECT_EQ(snapshot(), before);
    ASSERT_TRUE(prepared->commit()); EXPECT_FALSE(prepared->commit()); EXPECT_FALSE(competing->commit());
    auto expected = before;
    expected.mNativeGetUpPlayback.emplace(key(), expectedGraph); expected.mNativeAnimationClocks.emplace(key(), 0);
    expected.mNativeActorValues.front().mProcessKnockedState = 5;
    expected.mNativeActorValues.front().mProcessAction = -1;
    expected.mCombatRngState = random.mNextState;
    EXPECT_EQ(snapshot(), expected);
    EXPECT_TRUE(combat().isActionPending(contact, key()));
    EXPECT_THROW(prepared->state(), std::invalid_argument);
}

TEST_F(NativeGetUpPlaybackTest, ResolvedStartCompletionAndClearRetainIdentityFrontier)
{
    auto next = update(); auto ready = combat().prepareGetUpPlayback(key(), next); ASSERT_TRUE(ready->commit());
    ESM4::RuntimeGetUpSequence sequence; sequence.mResource = 2;
    next.mPlayback.mSequences.emplace(3, sequence); next.mPlayback.mNextIdentity = 4;
    const auto started = ESM4::startNativeIdleRequest(*next.mPlayback.mRequests.mPrimary, true, 3, -1, 0);
    next.mPlayback.mRequests.mPrimary = started.mRequest;
    next.mPlayback.mCachedSequence = next.mPlayback.mResidentSequence = next.mPlayback.mActionSequence = 3;
    next.mAction = started.mAction; next.mClock = .25f;
    auto start = combat().prepareGetUpPlayback(key(), next); ASSERT_TRUE(start->commit());
    EXPECT_EQ(combat().findActorValues(key())->mProcessAction, 11);
    const auto completed = ESM4::completeNativeIdleRequest(*next.mPlayback.mRequests.mPrimary, 0, 11);
    next.mPlayback.mRequests.mPrimary = completed.mRequest;
    next.mPlayback.mSequences.at(3).mState.mInactive = true;
    next.mPlayback.mResidentSequence = next.mPlayback.mActionSequence = 0;
    next.mAction = completed.mAction; next.mClock = .5f;
    auto completion = combat().prepareGetUpPlayback(key(), next); ASSERT_TRUE(completion->commit());
    EXPECT_EQ(combat().getUpPlayback(key())->mRequests.mPrimary->mStatus, 3);
    EXPECT_EQ(combat().getUpPlayback(key())->mCachedSequence, 3);
    const auto destruction = ESM4::destroyNativeIdleRequest(*next.mPlayback.mRequests.mPrimary, 2, 3, -1, 0);
    ASSERT_EQ(destruction.mDetachSequence, 3); ASSERT_TRUE(destruction.mEraseCacheGroup);
    next.mPlayback = {}; next.mPlayback.mNextIdentity = 4;
    next.mIdle.reset(); next.mKnockedState = 0;
    auto clear = combat().prepareGetUpPlayback(key(), next); ASSERT_TRUE(clear->commit());
    EXPECT_FALSE(combat().getUpIdle(key()));
    EXPECT_EQ(combat().getUpPlayback(key())->mNextIdentity, 4);
    EXPECT_TRUE(combat().getUpPlayback(key())->mResources.empty());
    EXPECT_EQ(combat().findActorValues(key())->mProcessAction, -1);
    EXPECT_EQ(combat().getProcessKnockedState(key()), 0);
    const auto wire = snapshot().serializeBinary();
    EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(wire), snapshot());
}

TEST_F(NativeGetUpPlaybackTest, RejectsReusedIdentitiesAndChangedRetainedBindings)
{
    auto next = update(); auto first = combat().prepareGetUpPlayback(key(), next); ASSERT_TRUE(first->commit());
    const auto before = snapshot();
    auto bad = next; bad.mPlayback.mResources.at(2).mModel = "OUTSIDE\\SAME.KF";
    EXPECT_THROW(combat().prepareGetUpPlayback(key(), bad), std::exception);
    bad = next; auto selection = *next.mIdle; selection.mModel = "OUTSIDE\\SAME.KF";
    bad.mPlayback.mRequestSelections.emplace(1, selection);
    EXPECT_THROW(combat().prepareGetUpPlayback(key(), bad), std::exception);
    bad = next; bad.mPlayback.mResources.clear(); bad.mPlayback.mResources.emplace(4, *next.mIdle);
    bad.mPlayback.mRequests.mPrimary = ESM4::makeNativeIdleRequest(2, 4); bad.mPlayback.mNextIdentity = 5;
    EXPECT_THROW(combat().prepareGetUpPlayback(key(), bad), std::exception);
    next.mPlayback = {}; next.mPlayback.mNextIdentity = 3;
    auto clear = combat().prepareGetUpPlayback(key(), next); ASSERT_TRUE(clear->commit());
    auto reuse = update(); reuse.mPlayback.mNextIdentity = 5;
    EXPECT_THROW(combat().prepareGetUpPlayback(key(), reuse), std::exception);
    reuse.mPlayback = {}; reuse.mPlayback.mNextIdentity = 2;
    EXPECT_THROW(combat().prepareGetUpPlayback(key(), reuse), std::exception);
    EXPECT_EQ(combat().getUpPlayback(key()), next.mPlayback);
    EXPECT_EQ(combat().combatRandomState(), before.mCombatRngState);
}

TEST_F(NativeGetUpPlaybackTest, RejectsStaleRandomClockProcessAndServiceBeforeAnyWrite)
{
    auto prepared = combat().prepareGetUpPlayback(key(), update()); ASSERT_TRUE(prepared->isValid());
    combat().drawNativeRandom(); const auto afterDraw = snapshot();
    EXPECT_FALSE(prepared->commit()); EXPECT_EQ(snapshot(), afterDraw);
    prepared = combat().prepareGetUpPlayback(key(), update());
    combat().setProcessKnockedState(key(), 0); combat().setProcessKnockedState(key(), 3);
    EXPECT_FALSE(prepared->commit());
    prepared = combat().prepareGetUpPlayback(key(), update());
    combat().advanceAnimationClock(key(), .25f); const auto afterClock = snapshot();
    EXPECT_FALSE(prepared->commit()); EXPECT_EQ(snapshot(), afterClock);
    auto next = update(); next.mClock = .25f;
    prepared = combat().prepareGetUpPlayback(key(), next); ASSERT_TRUE(prepared->isValid());
    const auto saved = snapshot(); combat().restore(saved, mFixture.mWorld.getStore());
    EXPECT_FALSE(prepared->commit()); EXPECT_EQ(snapshot(), saved);
    prepared = combat().prepareGetUpPlayback(key(), next);
    auto replacement = combat(); combat() = std::move(replacement);
    EXPECT_FALSE(prepared->commit()); EXPECT_EQ(snapshot(), saved);
}

TEST_F(NativeGetUpPlaybackTest, PublishesCompanionMeleeFrameAndClockOnceWithoutConsumingContact)
{
    combat().setProcessKnockedState(key(), 0);
    const auto contact = combat().beginMeleeStrike(key(), ESM4::MeleeStrikeKind::Counter, "handtohandblockattack");
    ASSERT_NE(contact, 0); combat().setProcessKnockedState(key(), 3);
    MWMechanics::OblivionMeleeSequenceMetadata source{"handtohandblockattack", 1, 0, 1, 0, 1};
    source.mPowerKeys = {0, .5f, 1};
    const auto metadata = std::span(&source, 1);
    auto frame = combat().prepareMeleeReactionFrame(key(), .125f, metadata); ASSERT_TRUE(frame);
    auto competing = combat().prepareMeleeReactionFrame(key(), .125f, metadata); ASSERT_TRUE(competing);
    auto next = update(); next.mClock = frame->clock(); next.mAction = combat().getProcessAction(key());
    const auto before = snapshot(); auto expected = before;
    expected.mNativeGetUpPlayback.emplace(key(), next.mPlayback);
    expected.mNativeAnimationClocks[key()] = *next.mClock;
    expected.mNativeMeleeStates.at(key()) = frame->state();
    expected.mNativeActorValues.front().mProcessKnockedState = 5;
    auto prepared = combat().prepareGetUpPlayback(key(), next, std::move(frame)); ASSERT_TRUE(prepared);
    EXPECT_EQ(snapshot(), before); ASSERT_TRUE(prepared->isValid());
    ASSERT_TRUE(prepared->commit()); EXPECT_FALSE(prepared->commit());
    EXPECT_EQ(snapshot(), expected); EXPECT_FLOAT_EQ(combat().animationClock(key()), .125f);
    EXPECT_TRUE(combat().isActionPending(contact, key()));
    EXPECT_FALSE(combat().commitMeleeReactionFrame(*competing));
    // A second frame must transfer with exactly the same actor clock. Omitting
    // it, mixing service owners or publishing a stale frame cannot write.
    auto conflict = next; conflict.mAction = 11;
    EXPECT_THROW(combat().prepareGetUpPlayback(key(), conflict), std::invalid_argument);
    frame = combat().prepareMeleeReactionFrame(key(), .125f, metadata); ASSERT_TRUE(frame);
    conflict.mClock = frame->clock();
    EXPECT_THROW(combat().prepareGetUpPlayback(key(), conflict, std::move(frame)), std::invalid_argument);
    EXPECT_EQ(snapshot(), expected);
    next.mClock = .25f;
    EXPECT_THROW(combat().prepareGetUpPlayback(key(), next), std::invalid_argument);
    frame = combat().prepareMeleeReactionFrame(key(), .125f, metadata); ASSERT_TRUE(frame);
    auto mismatched = next; mismatched.mClock = .5f;
    EXPECT_THROW(combat().prepareGetUpPlayback(key(), mismatched, std::move(frame)), std::invalid_argument);
    auto foreign = combat(); frame = foreign.prepareMeleeReactionFrame(key(), .125f, metadata); ASSERT_TRUE(frame);
    EXPECT_THROW(combat().prepareGetUpPlayback(key(), next, std::move(frame)), std::invalid_argument);
    frame = combat().prepareMeleeReactionFrame(key(), .125f, metadata); ASSERT_TRUE(frame);
    prepared = combat().prepareGetUpPlayback(key(), next, std::move(frame)); ASSERT_TRUE(prepared->isValid());
    competing = combat().prepareMeleeReactionFrame(key(), .125f, metadata);
    ASSERT_TRUE(combat().commitMeleeReactionFrame(*competing)); const auto advanced = snapshot();
    EXPECT_FALSE(prepared->commit()); EXPECT_EQ(snapshot(), advanced);
}

TEST_F(NativeGetUpPlaybackTest, RejectsReactionActionConflictAndActorClockRewind)
{
    const auto contact = combat().allocateAction(key()); ASSERT_TRUE(combat().consumeAction(contact, key()));
    auto saved = snapshot();
    ESM4::RuntimeMeleeReaction reaction{contact, ESM4::MeleeReactionKind::Stagger, "handtohandstagger"};
    saved.mNativeMeleeStates[key()].mReaction = reaction;
    saved.mNativeActorValues.front().mProcessAction = reaction.processAction();
    saved.mNativeAnimationClocks[key()] = .5f; saved.validate();
    combat().restore(saved, mFixture.mWorld.getStore());
    auto next = update(); next.mClock = .5f; next.mAction = reaction.processAction();
    const auto before = snapshot();
    auto conflict = next; conflict.mAction = 11;
    EXPECT_THROW(combat().prepareGetUpPlayback(key(), conflict), std::invalid_argument);
    MWMechanics::OblivionMeleeSequenceMetadata source{"handtohandstagger", 1, 0, 1, 0, 1};
    source.mReactionKeys = {0, 1};
    auto frame = combat().prepareMeleeReactionFrame(key(), .125f, std::span(&source, 1)); ASSERT_TRUE(frame);
    conflict.mClock = frame->clock();
    EXPECT_THROW(combat().prepareGetUpPlayback(key(), conflict, std::move(frame)), std::invalid_argument);
    EXPECT_EQ(snapshot(), before);
    auto prepared = combat().prepareGetUpPlayback(key(), next); ASSERT_TRUE(prepared->isValid());
    ASSERT_TRUE(prepared->commit());
    const auto after = snapshot();
    auto rewind = next; rewind.mClock = .25f;
    EXPECT_THROW(combat().prepareGetUpPlayback(key(), rewind), std::invalid_argument);
    EXPECT_EQ(snapshot(), after);
}

TEST_F(NativeGetUpPlaybackTest, LowProcessAllowsOnlyOwnershipCleanup)
{
    auto next = update(); const auto idle = next.mIdle;
    auto saved = snapshot(); auto& values = saved.mNativeActorValues.front();
    values.mProcess = ESM4::ActorValueProcess::Low;
    values.mProcessKnockedState.reset(); values.mProcessAction.reset();
    saved.mNativeGetUpIdles.clear(); saved.validate();
    combat().restore(saved, mFixture.mWorld.getStore());
    const auto before = snapshot();
    next.mPlayback = {}; next.mPlayback.mNextIdentity = 7;
    next.mKnockedState.reset(); next.mAction.reset(); next.mIdle.reset(); next.mClock.reset();
    auto prepared = combat().prepareGetUpPlayback(key(), next); ASSERT_TRUE(prepared->isValid());
    EXPECT_EQ(snapshot(), before); ASSERT_TRUE(prepared->commit());
    EXPECT_EQ(combat().getUpPlayback(key()), next.mPlayback);
    EXPECT_FALSE(combat().getUpIdle(key())); EXPECT_FALSE(combat().getProcessKnockedState(key()));
    const auto cleared = snapshot();
    for (unsigned field = 0; field < 3; ++field)
    {
        auto bad = next;
        if (field == 0) bad.mKnockedState = 0;
        else if (field == 1) bad.mAction = -1;
        else bad.mIdle = idle;
        EXPECT_THROW(combat().prepareGetUpPlayback(key(), bad), std::invalid_argument);
    }
    EXPECT_EQ(snapshot(), cleared);
}

TEST_F(NativeGetUpPlaybackTest, MalformedCandidatesLeaveAllAuthorityUntouched)
{
    const auto before = snapshot(); auto next = update();
    next.mRandom.mNextState ^= 1;
    EXPECT_THROW(combat().prepareGetUpPlayback(key(), next), std::invalid_argument);
    next = update(); ++next.mRandom.mExpectedState;
    EXPECT_FALSE(combat().prepareGetUpPlayback(key(), next));
    next = update(); next.mKnockedState = 7;
    EXPECT_THROW(combat().prepareGetUpPlayback(key(), next), std::invalid_argument);
    next = update(); next.mClock = std::numeric_limits<float>::infinity();
    EXPECT_THROW(combat().prepareGetUpPlayback(key(), next), std::invalid_argument);
    next = update(); next.mPlayback.mResources.at(2).mBase = ESM::FormKey::content("headless.esm", 0x810);
    EXPECT_THROW(combat().prepareGetUpPlayback(key(), next), std::invalid_argument);
    next = update(); next.mPlayback.mRequests.mPrimary->mStatus = 0;
    EXPECT_THROW(combat().prepareGetUpPlayback(key(), next), std::exception);
    EXPECT_EQ(snapshot(), before);
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

TEST_F(NativeGetUpRequestTest, PreparedRequestKeepsResidentInfoAcrossBackendEvictionAndRechecksSaveBytes)
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
    EXPECT_EQ(combat().getUpIdle(key())->mAssetHash, original.mAssetHash);
    const auto before = snapshot();
    EXPECT_THROW(world.validateOblivionSaveState(before), std::invalid_argument);
    EXPECT_EQ(snapshot(), before);
    EXPECT_EQ(mAnimation->sources(), 0); EXPECT_FALSE(mAnimation->reactionRoot());
}

TEST_F(NativeGetUpRequestTest, DifferentActorsShareResidentInfoAcrossBackendEviction)
{
    auto& world = mFixture.mWorld;
    auto first = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor); ASSERT_TRUE(first->commit());
    const auto original = *combat().getUpIdle(key());
    const auto base = original.mBase;
    ESM4::ActorCharacter reference{}; reference.mId = {0x901, 0};
    reference.mFormKey = ESM::FormKey::content("headless.esm", 0x901);
    reference.mBaseKey = base; reference.mBaseObj = world.getStore().search<ESM4::Npc>(base)->mId;
    world.getStore().getWritable<ESM4::ActorCharacter>().insertStatic(reference, reference.mFormKey);
    MWWorld::LiveCellRef<ESM4::Npc> live(reference, world.getStore().search<ESM4::Npc>(base));
    auto& cell = world.getWorldModel().getDraftCell();
    const MWWorld::Ptr second(cell.insert(&live), &cell); world.getWorldModel().registerPtr(second);
    ASSERT_TRUE(world.activateOblivionActor(second));
    const auto secondKey = second.getCellRef().getFormKey(); combat().setProcessKnockedState(secondKey, 3);
    osg::ref_ptr<IdleAnimation> secondAnimation = new IdleAnimation(second, mFixture.mResources);
    write(kf(27)); mFixture.mResources.clearCache();
    auto next = world.prepareOblivionGetUpIdleRequest(*secondAnimation, second); ASSERT_TRUE(next->isValid());
    ASSERT_TRUE(next->commit());
    const auto selected = combat().getUpIdle(secondKey); ASSERT_TRUE(selected);
    EXPECT_EQ(selected->mAssetHash, original.mAssetHash);
    EXPECT_EQ(combat().getUpIdle(key()), original);
    EXPECT_EQ(combat().getProcessKnockedState(key()), 5);
    EXPECT_EQ(combat().getProcessKnockedState(secondKey), 5);
    EXPECT_EQ(mAnimation->sources(), 0); EXPECT_EQ(secondAnimation->sources(), 0);
}

TEST_F(NativeGetUpRequestTest, IncomingCacheValidationDoesNotReuseOrReplaceLiveResidentInfo)
{
    auto& world = mFixture.mWorld;
    auto prepared = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor);
    ASSERT_TRUE(prepared->commit());
    const auto before = snapshot();
    const auto oldHash = combat().getUpIdle(key())->mAssetHash;
    write(kf(27)); mFixture.mResources.clearCache();
    auto current = MWRender::Animation::acquireNativeIdleResource(mFixture.mResources, "Outside\\Same.kf");
    ASSERT_TRUE(current);
    std::string hash;
    constexpr char digits[] = "0123456789abcdef";
    for (unsigned char c : current->fileHash()) { hash += digits[c >> 4]; hash += digits[c & 15]; }
    ASSERT_NE(hash, oldHash);
    auto incoming = before;
    incoming.mNativeGetUpIdles.at(key()).mAssetHash = hash;
    ASSERT_NO_THROW(incoming.validate());
    EXPECT_NO_THROW(world.validateOblivionSaveState(incoming));
    EXPECT_THROW(world.validateOblivionSaveState(before), std::invalid_argument);
    EXPECT_EQ(snapshot(), before);
    combat().setProcessKnockedState(key(), 3);
    prepared = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor);
    ASSERT_TRUE(prepared->commit());
    EXPECT_EQ(combat().getUpIdle(key())->mAssetHash, oldHash);
}

TEST_F(NativeGetUpRequestTest, RestoredSimulationRecoversItsCacheAfterCandidateClear)
{
    auto& world = mFixture.mWorld;
    auto prepared = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor);
    ASSERT_TRUE(prepared->commit()); prepared.reset();
    const auto before = snapshot();
    const auto oldHash = combat().getUpIdle(key())->mAssetHash;
    auto archive = world.retainSimulation();
    ASSERT_TRUE(archive.detach());
    world.clear(); world.setupPlayer();
    write(kf(27)); mFixture.mResources.clearCache();
    archive.prepareExchange();
    ASSERT_TRUE(archive.restore());
    EXPECT_EQ(snapshot(), before);
    combat().setProcessKnockedState(key(), 3);
    prepared = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor);
    ASSERT_TRUE(prepared->commit());
    EXPECT_EQ(combat().getUpIdle(key())->mAssetHash, oldHash);
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

TEST_F(NativeGetUpRequestTest, SavedNiSourceCanOutliveEveryRequestWithoutInventingLoadedOwnership)
{
    write(kf(9, "Bip01", false, false, "SpecialIdle_GetUp")); mFixture.mResources.clearCache();
    auto& world = mFixture.mWorld;
    auto ready = world.prepareOblivionGetUpIdleRequest(*mAnimation, mActor); ASSERT_TRUE(ready->commit());
    const auto before = snapshot(); auto saved = before;
    auto& graph = saved.mNativeGetUpPlayback[key()];
    graph.mNextIdentity = 10;
    graph.mResources.emplace(2, saved.mNativeGetUpIdles.at(key()));
    ESM4::RuntimeGetUpSequence sequence; sequence.mResource = 2;
    graph.mSequences.emplace(3, sequence); graph.mCachedSequence = 3;
    saved.mNativeAnimationClocks[key()] = 0;
    ASSERT_NO_THROW(saved.validate());
    EXPECT_NO_THROW(world.validateOblivionSaveState(saved));
    EXPECT_FALSE(graph.mRequests.mPrimary); EXPECT_FALSE(graph.mRequests.mSecondary);
    EXPECT_FALSE(graph.mRequests.mRetired[0]); EXPECT_FALSE(graph.mRequests.mRetired[1]);
    EXPECT_EQ(snapshot(), before);
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

TEST_F(NativeGetUpSelectionTest, SharedResourceRetainsDistinctWinningSelectionsWithoutConditionOrRngReplay)
{
    auto& world = mFixture.mWorld; auto saved = selectedGraph(); const auto before = snapshot();
    const auto wire = saved.serializeBinary(); saved = ESM4::RuntimeState::deserializeBinary(wire);
    ASSERT_NO_THROW(world.validateOblivionSaveState(saved));
    MWMechanics::OblivionCombatService restored; ASSERT_NO_THROW(restored.restore(saved, world.getStore()));
    auto captured = saved; restored.capture(captured); EXPECT_EQ(captured, saved);
    const auto graph = restored.getUpPlayback(key()); ASSERT_TRUE(graph);
    EXPECT_EQ(graph->requestSelection(4).mIdle, ESM::FormKey::content("headless.esm", 0xa01));
    EXPECT_EQ(graph->requestSelection(5).mIdle, ESM::FormKey::content("headless.esm", 0xa02));
    EXPECT_EQ(graph->mResources.size(), 1); EXPECT_EQ(graph->mSequences.size(), 1);
    EXPECT_EQ(restored.combatRandomState(), saved.mCombatRngState); EXPECT_EQ(snapshot(), before);
    auto legacy = saved; legacy.mVersion = 72; const auto unchanged = legacy;
    EXPECT_THROW(restored.capture(legacy), std::invalid_argument); EXPECT_EQ(legacy, unchanged);
    auto& store = world.getStore(); const auto selected = ESM::FormKey::content("headless.esm", 0xa02);
    auto changed = *store.search<ESM4::IdleAnimation>(selected); changed.mModel = "Outside\\changed.kf";
    store.getWritable<ESM4::IdleAnimation>().insertStatic(changed, selected);
    EXPECT_THROW(world.validateOblivionSaveState(saved), std::invalid_argument);
    EXPECT_THROW(restored.restore(saved, store), std::invalid_argument);
    auto after = captured; restored.capture(after); EXPECT_EQ(after, captured);
}

TEST_F(NativeGetUpSelectionTest, RequestSelectionReadmissionRejectsWrongActorBankAndModel)
{
    auto& world = mFixture.mWorld; auto saved = selectedGraph(); const auto before = snapshot();
    auto& selection = saved.mNativeGetUpPlayback.at(key()).mRequestSelections.at(5);
    selection.mActorModel = "Cat\\Skeleton.NIF";
    ASSERT_NO_THROW(saved.validate());
    EXPECT_THROW(world.validateOblivionSaveState(saved), std::invalid_argument);
    selection.mActorModel = saved.mNativeGetUpIdles.at(key()).mActorModel;
    const auto other = ESM::FormKey::content("headless.esm", 0xb00);
    auto& store = world.getStore(); auto changed = *store.search<ESM4::IdleAnimation>(other);
    changed.mModel = "OUTSIDE\\SAME.KF"; store.getWritable<ESM4::IdleAnimation>().insertStatic(changed, other);
    selection.mIdle = other;
    ASSERT_NO_THROW(saved.validate());
    EXPECT_THROW(world.validateOblivionSaveState(saved), std::invalid_argument);
    EXPECT_EQ(snapshot(), before);
}

namespace
{
    struct NativeIdleProjectionTest : NativeIdleResourceTest
    {
        using Layer = MWRender::Animation::NativeSequenceLayer;
        std::unique_ptr<MWRender::Animation::PreparedNativeSequenceLayers> prepare(
            IdleAnimation& actor, std::string_view path, float time = .5f)
        {
            const auto resource = actor.acquireNativeIdleResource(path);
            if (!resource) throw std::runtime_error("projection fixture resource acquisition failed");
            const std::array<const MWRender::Animation::NativeIdleResource*, 1> resources{resource.get()};
            auto batch = actor.prepareNativeIdleSources(resources);
            if (!batch) throw std::runtime_error("projection fixture source preparation failed");
            Layer layer; layer.mGroup = batch->group(0); layer.mTime = time; layer.mExposeSampleTime = true;
            return actor.prepareNativeSequenceLayers(std::span(&layer, 1), std::move(batch));
        }
        void update(IdleAnimation& actor)
        {
            osg::ref_ptr<osg::FrameStamp> stamp = new osg::FrameStamp;
            stamp->setSimulationTime(.5);
            osgUtil::UpdateVisitor visitor; visitor.setFrameStamp(stamp);
            actor.runAnimation(0); actor.mRoot->accept(visitor);
        }
    };
}

TEST_F(NativeIdleProjectionTest, StagedAliasesAndLayersPublishTogetherWithoutChangingThePreparedPose)
{
    auto actor = animation(); const auto local = actor->mBone->getMatrix();
    auto prepared = prepare(*actor, "one/same.kf"); ASSERT_TRUE(prepared); ASSERT_TRUE(prepared->isValid());
    EXPECT_EQ(actor->sources(), 0); EXPECT_FALSE(actor->reactionRoot());
    EXPECT_EQ(actor->mBone->getUpdateCallback(), nullptr); EXPECT_EQ(actor->mBone->getMatrix(), local);
    ASSERT_TRUE(prepared->commit()); EXPECT_FALSE(prepared->commit()); EXPECT_FALSE(prepared->isValid());
    EXPECT_EQ(actor->sources(), 1); EXPECT_EQ(actor->reactionRoot(), 4);
    EXPECT_EQ(actor->mBone->getMatrix(), local); ASSERT_NE(actor->mBone->getUpdateCallback(), nullptr);
    update(*actor); EXPECT_FLOAT_EQ(actor->mBone->getControllerTransform().mTranslation.x(), 9);
    const auto callback = osg::ref_ptr(actor->mBone->getUpdateCallback());
    const auto group = actor->projectedGroup(); EXPECT_FLOAT_EQ(actor->getCurrentTime(group), .5f);
    auto next = prepare(*actor, "one/same.kf", .75f); ASSERT_TRUE(next); ASSERT_TRUE(next->commit());
    EXPECT_EQ(actor->sources(), 1); EXPECT_EQ(actor->mBone->getUpdateCallback(), callback);
    EXPECT_FLOAT_EQ(actor->getCurrentTime(group), .75f);
    update(*actor); EXPECT_FLOAT_EQ(actor->mBone->getControllerTransform().mTranslation.x(), 9);
}

TEST_F(NativeIdleProjectionTest, MissingLayerAndLazyOrdinarySourceCannotPublishPartOfABatch)
{
    auto actor = animation(); actor->directory(); const auto before = actor->sources();
    Layer ordinary; ordinary.mGroup = "same"; ordinary.mTime = .5f;
    EXPECT_FALSE(actor->prepareNativeSequenceLayers(std::span(&ordinary, 1)));
    EXPECT_EQ(actor->sources(), before); EXPECT_EQ(actor->mBone->getUpdateCallback(), nullptr);
    const auto resource = actor->acquireNativeIdleResource("one/same.kf"); ASSERT_TRUE(resource);
    const std::array<const MWRender::Animation::NativeIdleResource*, 1> resources{resource.get()};
    auto batch = actor->prepareNativeIdleSources(resources); ASSERT_TRUE(batch);
    const std::array<Layer, 2> layers{{{batch->group(0), MWRender::BlendMask_All, .5f},
        {"unavailable", MWRender::BlendMask_All, .5f}}};
    EXPECT_FALSE(actor->prepareNativeSequenceLayers(layers, std::move(batch)));
    EXPECT_EQ(actor->sources(), before); EXPECT_FALSE(actor->reactionRoot());
    EXPECT_EQ(actor->mBone->getUpdateCallback(), nullptr);
}

TEST_F(NativeIdleProjectionTest, CallbackHierarchyAndBlendChangesRejectWithoutPublishingSources)
{
    for (int mutation = 0; mutation < 4; ++mutation)
    {
        SCOPED_TRACE(mutation); auto actor = animation(); auto prepared = prepare(*actor, "one/same.kf");
        ASSERT_TRUE(prepared); ASSERT_TRUE(prepared->isValid());
        if (mutation == 0) actor->mBone->addUpdateCallback(new osg::NodeCallback);
        if (mutation == 1) actor->mRoot->addChild(new osg::Group);
        if (mutation == 2) actor->mBone->setName("Renamed");
        if (mutation == 3) actor->mBone->setUserValue("nativeSequenceBlendThreshold", .5f);
        EXPECT_FALSE(prepared->isValid()); EXPECT_FALSE(prepared->commit());
        EXPECT_EQ(actor->sources(), 0); EXPECT_FALSE(actor->reactionRoot());
    }
    auto actor = animation(); actor->mBone->setUserValue("nativeSequenceBlendThreshold", .5f);
    auto prepared = prepare(*actor, "one/same.kf"); ASSERT_TRUE(prepared); ASSERT_TRUE(prepared->isValid());
    actor->mBone->setUserValue("nativeSequenceBlendThreshold", .75f);
    EXPECT_FALSE(prepared->commit()); EXPECT_EQ(actor->sources(), 0);
}

TEST_F(NativeIdleProjectionTest, ProjectionAbaSourceClearForeignOwnerAndExpiredOwnerReject)
{
    auto actor = animation(); auto prepared = prepare(*actor, "one/same.kf"); ASSERT_TRUE(prepared);
    ASSERT_TRUE(actor->setNativeSequenceLayers({}));
    EXPECT_FALSE(prepared->commit()); EXPECT_EQ(actor->sources(), 0);
    prepared = prepare(*actor, "one/same.kf"); ASSERT_TRUE(prepared); actor->clear();
    EXPECT_FALSE(prepared->commit()); EXPECT_EQ(actor->sources(), 0);
    const auto resource = actor->acquireNativeIdleResource("one/same.kf"); ASSERT_TRUE(resource);
    const std::array<const MWRender::Animation::NativeIdleResource*, 1> resources{resource.get()};
    auto batch = actor->prepareNativeIdleSources(resources); ASSERT_TRUE(batch);
    Layer layer; layer.mGroup = batch->group(0); layer.mTime = .5f;
    auto other = animation();
    EXPECT_THROW(other->prepareNativeSequenceLayers(std::span(&layer, 1), std::move(batch)), std::invalid_argument);
    EXPECT_EQ(actor->sources(), 0); EXPECT_EQ(other->sources(), 0);
    prepared = prepare(*actor, "one/same.kf"); ASSERT_TRUE(prepared); actor = nullptr;
    EXPECT_FALSE(prepared->isValid()); EXPECT_FALSE(prepared->commit());
}

namespace
{
    struct CallbackOrderAnimation : IdleAnimation
    {
        struct Observe : osg::NodeCallback
        {
            std::vector<float> mSamples;
            void operator()(osg::Node* node, osg::NodeVisitor* visitor) override
            {
                mSamples.push_back(static_cast<NifOsg::MatrixTransform*>(node)->getControllerTransform().mTranslation.x());
                traverse(node, visitor);
            }
        };
        osg::ref_ptr<Observe> mObserve = new Observe;
        unsigned mBuilds = 0;
        CallbackOrderAnimation(const MWWorld::Ptr& actor, Resource::ResourceSystem& resources)
            : IdleAnimation(actor, resources) {}
        void ordinary()
        {
            source("meshes/same.kf", "ordinary");
            play("ordinary", MWRender::AnimPriority(1), MWRender::BlendMask_All, false, 1, "start", "stop", 0, 0);
        }
        // Start a fresh native registration while retaining ordinary adapter playback.
        void restartNativeRegistration()
        {
            mNativeReactionFirstSequenceLoaded = false;
            mNativeReactionRootRecord.reset();
            ++mNativeIdleSourceGeneration;
        }
        void clearAccumulationForFixture()
        {
            if (!mNativeSequenceBaseControllerCount)
                throw std::logic_error("fixture has no ordinary accumulation callback");
            const auto reset = mActiveControllers.at(mNativeSequenceBaseControllerCount - 1);
            if (reset.first != mAccumRoot)
                throw std::logic_error("fixture accumulation callback is not the ordinary prefix tail");
            reset.first->removeUpdateCallback(reset.second); reset.second->setNestedCallback(nullptr);
            std::erase(mActiveControllers, reset);
            --mNativeSequenceBaseControllerCount;
            mAccumRoot = nullptr; mAccumCtrl = nullptr;
            ++mNativeSequenceGeneration; ++mNativeIdleSourceGeneration;
        }
        bool hasAccumulator() const { return bool(mAccumCtrl); }
        auto accumulatedRoot() const { return mAccumRoot; }
        void addControllers() override
        {
            ++mBuilds;
            mBone->addUpdateCallback(mObserve);
            mActiveControllers.emplace_back(mBone, mObserve);
        }
    };
}

TEST_F(NativeIdleProjectionTest, OrdinaryAndAdditionalCallbacksKeepTheirOrderAcrossReplacementAndRelease)
{
    osg::ref_ptr<CallbackOrderAnimation> actor = new CallbackOrderAnimation(mFixture.mWorld.getPlayerPtr(), mFixture.mResources);
    actor->ordinary(); const auto builds = actor->mBuilds;
    const auto original = osg::ref_ptr(actor->mBone->getUpdateCallback());
    auto prepared = prepare(*actor, "one/same.kf"); ASSERT_TRUE(prepared); ASSERT_TRUE(prepared->commit());
    EXPECT_EQ(actor->mBuilds, builds); EXPECT_EQ(actor->mBone->getUpdateCallback(), original);
    update(*actor); ASSERT_FALSE(actor->mObserve->mSamples.empty()); EXPECT_FLOAT_EQ(actor->mObserve->mSamples.back(), 9);
    prepared = prepare(*actor, "two/same.kf"); ASSERT_TRUE(prepared); ASSERT_TRUE(prepared->commit());
    EXPECT_EQ(actor->mBuilds, builds); EXPECT_EQ(actor->mBone->getUpdateCallback(), original);
    update(*actor); EXPECT_FLOAT_EQ(actor->mObserve->mSamples.back(), 17);
    auto release = actor->prepareNativeSequenceLayers({}); ASSERT_TRUE(release); ASSERT_TRUE(release->commit());
    EXPECT_EQ(actor->mBuilds, builds); EXPECT_EQ(actor->mBone->getUpdateCallback(), original);
    update(*actor); EXPECT_FLOAT_EQ(actor->mObserve->mSamples.back(), 0); // ordinary root accumulation remains active
}

TEST_F(NativeIdleProjectionTest, NativeFirstSequenceRootRemainsDistinctFromOrdinaryAccumulation)
{
    osg::ref_ptr<CallbackOrderAnimation> actor = new CallbackOrderAnimation(mFixture.mWorld.getPlayerPtr(), mFixture.mResources);
    actor->ordinary(); ASSERT_TRUE(actor->hasAccumulator()); ASSERT_EQ(actor->accumulatedRoot(), actor->mBone);
    const auto builds = actor->mBuilds; actor->restartNativeRegistration();
    auto prepared = prepare(*actor, "missingroot.kf"); ASSERT_TRUE(prepared);
    ASSERT_TRUE(prepared->commit()); EXPECT_FALSE(actor->reactionRoot());
    EXPECT_TRUE(actor->hasAccumulator()); EXPECT_EQ(actor->accumulatedRoot(), actor->mBone);
    update(*actor); EXPECT_FLOAT_EQ(actor->mObserve->mSamples.back(), 2);
    auto release = actor->prepareNativeSequenceLayers({}); ASSERT_TRUE(release); ASSERT_TRUE(release->commit());
    update(*actor); EXPECT_FLOAT_EQ(actor->mObserve->mSamples.back(), 0);
    EXPECT_EQ(actor->mBuilds, builds);
}

TEST_F(NativeIdleProjectionTest, StagedAccumulationFallbackRebindsAnExistingOrdinarySource)
{
    osg::ref_ptr<CallbackOrderAnimation> actor = new CallbackOrderAnimation(mFixture.mWorld.getPlayerPtr(), mFixture.mResources);
    actor->ordinary(); const auto builds = actor->mBuilds;
    // Exercise a fresh legacy accumulation selection with retained adapter playback.
    actor->clearAccumulationForFixture(); ASSERT_FALSE(actor->hasAccumulator()); ASSERT_FALSE(actor->accumulatedRoot());
    const auto original = osg::ref_ptr(actor->mBone->getUpdateCallback());
    auto prepared = prepare(*actor, "one/same.kf"); ASSERT_TRUE(prepared);
    EXPECT_FALSE(actor->hasAccumulator()); EXPECT_FALSE(actor->accumulatedRoot());
    ASSERT_TRUE(prepared->commit()); EXPECT_TRUE(actor->hasAccumulator()); EXPECT_EQ(actor->accumulatedRoot(), actor->mBone);
    EXPECT_EQ(actor->mBone->getUpdateCallback(), original);
    update(*actor); EXPECT_FLOAT_EQ(actor->mObserve->mSamples.back(), 9);
    auto release = actor->prepareNativeSequenceLayers({}); ASSERT_TRUE(release); ASSERT_TRUE(release->commit());
    update(*actor); EXPECT_FLOAT_EQ(actor->mObserve->mSamples.back(), 0);
    EXPECT_EQ(actor->mBuilds, builds);
}

TEST_F(NativeIdleProjectionTest, PhysicalProjectionKeepsCallbacksDetachedAndRetainsAnimatedTargets)
{
    for (const auto mode : {MWRender::PhysicalPoseAnimation::Frozen, MWRender::PhysicalPoseAnimation::AnimatedTargets})
    {
        SCOPED_TRACE(static_cast<int>(mode)); auto actor = animation();
        actor->mRoot->setUserValue(Misc::OsgUserValues::sFileHash, std::string("projection-test"));
        NifBullet::ActorRagdollDefinition graph; graph.mSourceHash = "projection-test";
        NifBullet::RagdollBodyDefinition body{}; body.mRecord = 12; body.mNodeRecord = 4; body.mBone = "Bip01";
        graph.mBodies.push_back(body);
        auto initial = prepare(*actor, "one/same.kf"); ASSERT_TRUE(initial); ASSERT_TRUE(initial->commit()); update(*actor);
        const auto placement = Nif::NiTransform::getIdentity();
        auto physical = actor->prepareNativePhysicalPose(graph, placement, mode); ASSERT_TRUE(physical);
        ASSERT_TRUE(actor->commitPhysicalPose(*physical));
        actor->applyPhysicalPose(std::vector<NifBullet::RagdollBoneWorldPose>{{4, osg::Matrixf::translate(99, 0, 0)}}, placement);
        const auto pose = actor->mBone->getMatrix(); const auto callback = osg::ref_ptr(actor->mBone->getUpdateCallback());
        auto next = prepare(*actor, "two/same.kf"); ASSERT_TRUE(next); ASSERT_TRUE(next->commit());
        EXPECT_EQ(actor->mBone->getMatrix(), pose); EXPECT_EQ(actor->mBone->getUpdateCallback(), callback);
        if (mode == MWRender::PhysicalPoseAnimation::AnimatedTargets)
        {
            osg::ref_ptr<osg::FrameStamp> stamp = new osg::FrameStamp; stamp->setSimulationTime(.5);
            osgUtil::UpdateVisitor visitor; visitor.setFrameStamp(stamp);
            const auto targets = actor->samplePhysicalAnimationTarget(placement, visitor);
            ASSERT_EQ(targets.size(), 1); EXPECT_FLOAT_EQ(targets[0].mPose.getTrans().x(), 17);
            EXPECT_EQ(actor->mBone->getMatrix(), pose);
        }
        else { update(*actor); EXPECT_EQ(actor->mBone->getMatrix(), pose); }
        actor->endPhysicalPose(); update(*actor);
        EXPECT_FLOAT_EQ(actor->mBone->getControllerTransform().mTranslation.x(), 17);
    }
}
