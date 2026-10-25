#include "apps/openmw/mwworld/esmstore.hpp"
#include "apps/openmw/mwworld/oblivionnativeidle.hpp"

#include <components/esm3/loadnpc.hpp>
#include <components/esm4/common.hpp>
#include <components/esm4/reader.hpp>
#include <components/esm4/loadidle.hpp>
#include <components/esm4/aiconditions.hpp>
#include <components/loadinglistener/loadinglistener.hpp>

#include <gtest/gtest.h>

#include <map>
#include <sstream>
#include <stdexcept>

namespace
{
    template <class T> std::string bytes(const T& value)
    {
        return {reinterpret_cast<const char*>(&value), sizeof(value)};
    }
    std::string sub(std::uint32_t tag, const std::string& data)
    {
        return bytes(tag) + bytes(static_cast<std::uint16_t>(data.size())) + data;
    }
    std::string record(std::uint32_t tag, std::uint32_t id, const std::string& data, bool deleted = false,
        std::uint32_t extraFlags = 0)
    {
        return bytes(tag) + bytes(static_cast<std::uint32_t>(data.size()))
            + bytes((deleted ? std::uint32_t(ESM4::Rec_Deleted) : 0u) | extraFlags) + bytes(id) + bytes(0u) + data;
    }
    std::string idle(std::uint32_t id, const std::string& model, std::uint32_t parent = 0,
        std::uint32_t previous = 0, const std::string& conditions = {})
    {
        return record(ESM4::REC_IDLE, id, sub(ESM::fourCC("MODL"), model + '\0')
            + sub(ESM::fourCC("ANAM"), std::string(1, '\5'))
            + sub(ESM::fourCC("DATA"), bytes(parent) + bytes(previous)) + conditions);
    }
    struct Fixture
    {
        MWWorld::ESMStore mStore;
        Loading::Listener mListener;
        const std::map<std::string, int> mIndices{{"prefix.esm", 0}, {"base.esm", 1}, {"patch.esp", 2}, {"other.esm", 3}};

        void load(const std::string& name, const std::vector<std::string>& masters,
            const std::vector<std::string>& records)
        {
            auto header = sub(ESM::fourCC("HEDR"), bytes(1.f)
                + bytes(static_cast<std::uint32_t>(records.size())) + bytes(0x2000u));
            for (const auto& master : masters)
                header += sub(ESM::fourCC("MAST"), master + '\0') + sub(ESM::fourCC("DATA"), std::string(8, '\0'));
            auto data = record(ESM4::REC_TES4, 0, header);
            for (const auto& item : records)
                data += item;
            auto stream = std::make_unique<std::stringstream>(data, std::ios::in | std::ios::binary);
            ESM4::Reader reader(std::move(stream), name, nullptr, nullptr, true);
            reader.setModIndex(mIndices.at(name));
            reader.updateModIndices(mIndices);
            mStore.loadESM4(reader, &mListener);
        }
    };
}

TEST(OblivionNativeIdleStore, ActualLoadOrderBuildsNativeBanksAndKeepsAuthoredDataUnchanged)
{
    Fixture f;
    f.load("prefix.esm", {}, {});
    f.load("base.esm", {}, {idle(0x1000, "Dog\\IdleAnims\\First.kf"),
        idle(0x1001, "DOG\\IdleAnims\\Second.kf"), idle(0x1002, "Outside\\Child.kf", 0x1001),
        idle(0x1003, "Dog\\IdleAnims\\Last.kf", 0, 0x1000)});
    const auto before = f.mStore.getFormKeyIndex().serialize();
    auto index = MWWorld::buildOblivionNativeIdleIndex(f.mStore);
    ASSERT_EQ(index.mNodes.size(), 4u);
    ASSERT_EQ(index.mBanks.size(), 1u);
    EXPECT_EQ(index.mBanks[0].mRoots, (std::vector<std::size_t>{1, 0, 3}));
    EXPECT_EQ(index.mNodes[1].mChildren, (std::vector<std::size_t>{2}));
    EXPECT_EQ(index.mNodes[0].mPrevious, 1u);
    EXPECT_EQ(index.mNodes[0].mRecord->mPrevious.mIndex, 0u);
    EXPECT_EQ(index.mNodes[2].mRecord->mParent, (ESM::FormId{0x1001, 1}));
    ESM4::NativeIdleSelector selector(std::move(index.mNodes), index.mBanks);
    const auto* selected = selector.select("dog\\Skeleton.nif", [](const auto&) { return true; });
    ASSERT_NE(selected, nullptr);
    EXPECT_EQ(selected->mId, (ESM::FormId{0x1002, 1}));
    EXPECT_EQ(f.mStore.getFormKeyIndex().serialize(), before);
}

TEST(OblivionNativeIdleStore, KeepsIndependentPluginIdentitiesAndResolvedParentLinks)
{
    Fixture f;
    f.load("base.esm", {}, {idle(0x1000, "Dog\\IdleAnims\\Base.kf")});
    f.load("other.esm", {}, {idle(0x1000, "Fox\\IdleAnims\\Other.kf"),
        idle(0x1001, "Outside\\Child.kf", 0x1000)});
    auto index = MWWorld::buildOblivionNativeIdleIndex(f.mStore);
    ASSERT_EQ(index.mBanks.size(), 2u);
    EXPECT_EQ(index.mNodes[2].mRecord->mParent, (ESM::FormId{0x1000, 3}));
    ESM4::NativeIdleSelector selector(std::move(index.mNodes), index.mBanks);
    EXPECT_EQ(selector.select("Dog\\Skeleton.nif", [](const auto&) { return true; })->mId,
        (ESM::FormId{0x1000, 1}));
    EXPECT_EQ(selector.select("Fox\\Skeleton.nif", [](const auto&) { return true; })->mId,
        (ESM::FormId{0x1001, 3}));
}

TEST(OblivionNativeIdleStore, CapturesOrdinaryOverridesAndSamePluginDuplicatesInActualLoadOrder)
{
    for (bool duplicate : {false, true})
    {
        Fixture f;
        const auto base = idle(0x1000, "Dog\\IdleAnims\\Base.kf");
        const auto changed = idle(0x1000, "Fox\\IdleAnims\\Changed.kf");
        f.load("base.esm", {}, duplicate ? std::vector<std::string>{base, changed} : std::vector<std::string>{base});
        if (!duplicate)
            f.load("patch.esp", {"base.esm"}, {changed});
        const auto key = ESM::FormKey::content("base.esm", 0x1000);
        ASSERT_EQ(f.mStore.getFormKeyIndex().history(key)->size(), 2u);
        const auto before = f.mStore.getFormKeyIndex().serialize();
        const auto& events = f.mStore.getOblivionNativeIdleLoadEvents();
        ASSERT_EQ(events.size(), 2u);
        EXPECT_EQ(events[0].mRegistration.mModel, "Dog\\IdleAnims\\Base.kf");
        EXPECT_EQ(events[1].mRegistration.mModel, "Fox\\IdleAnims\\Changed.kf");
        const auto index = MWWorld::buildOblivionNativeIdleIndex(f.mStore);
        ASSERT_EQ(index.mBanks.size(), 2u);
        EXPECT_TRUE(index.mBanks[0].mRoots.empty());
        EXPECT_EQ(index.mBanks[1].mRoots, (std::vector<std::size_t>{0}));
        EXPECT_EQ(index.mNodes[0].mRecord->mModel.getOriginal(), "Fox\\IdleAnims\\Changed.kf");
        EXPECT_EQ(f.mStore.getFormKeyIndex().serialize(), before);
    }
}

TEST(OblivionNativeIdleStore, RejectsTombstonesAndMissingOrDynamicTypedDefinitions)
{
    for (int fault = 0; fault < 3; ++fault)
    {
        Fixture f;
        f.load("base.esm", {}, {idle(0x1000, "Dog\\IdleAnims\\Base.kf")});
        const auto key = ESM::FormKey::content("base.esm", 0x1000);
        auto& store = f.mStore.getWritable<ESM4::IdleAnimation>();
        if (fault == 0)
            f.load("patch.esp", {"base.esm"}, {record(ESM4::REC_IDLE, 0x1000, {}, true)});
        else if (fault == 1)
            store.eraseStatic(key);
        else
            store.insert(*store.searchStatic(key), key);
        const auto before = f.mStore.getFormKeyIndex().serialize();
        EXPECT_THROW(MWWorld::buildOblivionNativeIdleIndex(f.mStore), std::invalid_argument);
        EXPECT_EQ(f.mStore.getFormKeyIndex().serialize(), before);
    }
}

TEST(OblivionNativeIdleStore, EmptyRegistryIsValidButUnindexedStaticRecordsAreNotContentHistory)
{
    Fixture f;
    EXPECT_TRUE(MWWorld::buildOblivionNativeIdleIndex(f.mStore).mNodes.empty());
    ESM4::IdleAnimation authored;
    authored.mId = {0x1000, 0};
    authored.mModel = "Dog\\IdleAnims\\Unindexed.kf";
    f.mStore.getWritable<ESM4::IdleAnimation>().insertStatic(authored);
    EXPECT_THROW(MWWorld::buildOblivionNativeIdleIndex(f.mStore), std::invalid_argument);
}

TEST(OblivionNativeIdleStore, NativeConditionPaddingCannotRedirectRegistrySelection)
{
    Fixture f;
    std::vector<std::string> definitions;
    std::vector<std::uint32_t> tails;
    for (const std::uint32_t tail : {0u, 1u, 2u, 3u, 4u, 0xffu, 0xfeedbeefu})
        for (const std::uint32_t padding : {0u, 1u, 0xffffu})
            for (const bool target : {false, true})
            {
                const auto ctda = bytes(target ? 2u : 0u) + bytes(1.f) + bytes(107u | (padding << 16))
                    + bytes(0u) + bytes(0u) + bytes(tail);
                definitions.push_back(idle(0x1000u + static_cast<std::uint32_t>(definitions.size()), "Dog\\IdleAnims\\GetUp.kf",
                    0, 0, sub(ESM::fourCC("CTDA"), ctda)));
                tails.push_back(tail);
            }
    f.load("base.esm", {}, definitions);
    const auto before = f.mStore.getFormKeyIndex().serialize();
    auto index = MWWorld::buildOblivionNativeIdleIndex(f.mStore);
    ASSERT_EQ(index.mNodes.size(), 42u);
    ESM4::ConditionEvaluationContext context;
    context.mHasTarget = true;
    context.mResolve = [](const auto& condition, ESM4::ConditionSubject subject) {
        EXPECT_EQ(condition.mFunction, 107);
        return ESM4::ConditionValue::value(subject == ESM4::ConditionSubject::Self ? 1. : 0.);
    };
    for (std::size_t i = 0; i < index.mNodes.size(); ++i)
    {
        const auto& record = *index.mNodes[i].mRecord;
        ASSERT_EQ(record.mConditions.size(), 1u);
        EXPECT_EQ(record.mConditions[0].mUnknownTail, tails[i]);
        EXPECT_EQ(record.mConditions[0].mRunOn, ESM4::ConditionRunOn::Subject);
        EXPECT_EQ(ESM4::evaluateConditions(record.mConditions, context),
            i % 2 == 0 ? ESM4::ConditionResult::True : ESM4::ConditionResult::False);
    }
    ESM4::NativeIdleSelector selector(std::move(index.mNodes), index.mBanks);
    const auto* selected = selector.select("Dog\\Skeleton.nif", [&](const auto& record) {
        return ESM4::evaluateConditions(record.mConditions, context) == ESM4::ConditionResult::True;
    });
    ASSERT_NE(selected, nullptr);
    EXPECT_EQ(selected->mId, (ESM::FormId{0x1028, 1}));
    EXPECT_EQ(f.mStore.getFormKeyIndex().serialize(), before);
}

TEST(OblivionNativeIdleStore, ReplaysOverrideAfterLaterPredecessorInsteadOfFinalWinningIterationOrder)
{
    Fixture f;
    f.load("prefix.esm", {}, {});
    f.load("base.esm", {}, {idle(0x1000, "Dog\\IdleAnims\\First.kf"),
        idle(0x1001, "Dog\\IdleAnims\\Second.kf", 0, 0x1000)});
    f.load("patch.esp", {"base.esm"}, {idle(0x01002000, "Dog\\IdleAnims\\Added.kf", 0, 0x1001),
        idle(0x1000, "Dog\\IdleAnims\\First.kf", 0, 0x01002000)});
    const auto& events = f.mStore.getOblivionNativeIdleLoadEvents();
    ASSERT_EQ(events.size(), 4u);
    EXPECT_EQ(events[0].mRegistration.mPrevious.mIndex, 0u);
    EXPECT_EQ(events[3].mRegistration.mPrevious, (ESM::FormId{0x2000, 2}));
    std::vector<const ESM4::IdleAnimation*> winners;
    for (const auto& record : f.mStore.get<ESM4::IdleAnimation>()) winners.push_back(&record);
    EXPECT_THROW(ESM4::registerNativeIdleRecords(winners), std::invalid_argument);
    const auto before = f.mStore.getFormKeyIndex().serialize();
    const auto index = MWWorld::buildOblivionNativeIdleIndex(f.mStore);
    ASSERT_EQ(index.mBanks.size(), 1u);
    EXPECT_EQ(index.mBanks[0].mRoots, (std::vector<std::size_t>{1, 2, 0}));
    EXPECT_FALSE(index.mNodes[1].mPrevious);
    EXPECT_EQ(index.mNodes[0].mPrevious, 2u);
    EXPECT_EQ(index.mNodes[0].mRecord->mPrevious, (ESM::FormId{0x2000, 2}));
    EXPECT_EQ(f.mStore.getFormKeyIndex().serialize(), before);
}

TEST(OblivionNativeIdleStore, FailedRecordReadDoesNotPublishRegistrationOrMetadata)
{
    Fixture f;
    f.load("base.esm", {}, {idle(0x1000, "Dog\\IdleAnims\\Base.kf")});
    const auto before = f.mStore.getFormKeyIndex().serialize();
    EXPECT_THROW(f.load("patch.esp", {"base.esm"},
        {record(ESM4::REC_IDLE, 0x1000, sub(ESM::fourCC("ANAM"), std::string(2, '\0')))}), std::runtime_error);
    EXPECT_EQ(f.mStore.getFormKeyIndex().serialize(), before);
    ASSERT_EQ(f.mStore.getOblivionNativeIdleLoadEvents().size(), 1u);
    const auto index = MWWorld::buildOblivionNativeIdleIndex(f.mStore);
    ASSERT_EQ(index.mNodes.size(), 1u);
    EXPECT_EQ(index.mNodes[0].mRecord->mModel.getOriginal(), "Dog\\IdleAnims\\Base.kf");
}

TEST(OblivionNativeIdleStore, RejectsSpecialFlagsAndSamePluginTypeChangesBeforeReplaying)
{
    for (const bool typeChange : {false, true})
    {
        Fixture f;
        if (typeChange)
            f.load("base.esm", {}, {idle(0x1000, "Dog\\IdleAnims\\Base.kf"),
                record(ESM4::REC_GLOB, 0x1000, sub(ESM::fourCC("EDID"), std::string("ChangedType") + '\0')
                    + sub(ESM::fourCC("FNAM"), "f") + sub(ESM::fourCC("FLTV"), bytes(1.f)))});
        else
            f.load("base.esm", {}, {record(ESM4::REC_IDLE, 0x1000,
                sub(ESM::fourCC("MODL"), std::string("Dog\\IdleAnims\\Base.kf") + '\0'), false, 0x4000)});
        EXPECT_THROW(MWWorld::buildOblivionNativeIdleIndex(f.mStore), std::invalid_argument);
    }
}

TEST(OblivionNativeIdleStore, DynamicClearRetainsContentRegistrationHistory)
{
    Fixture f;
    // clearDynamic restores the projected Player after clearing all stores.
    ESM::NPC player{};
    player.blank();
    player.mId = ESM::RefId::stringRefId("Player");
    f.mStore.insertStatic(player);
    f.load("base.esm", {}, {idle(0x1000, "Dog\\IdleAnims\\Base.kf")});
    f.load("patch.esp", {"base.esm"}, {idle(0x1000, "Fox\\IdleAnims\\Changed.kf")});
    const auto history = f.mStore.getOblivionNativeIdleLoadEvents();
    const auto key = ESM::FormKey::content("base.esm", 0x1000);
    auto& records = f.mStore.getWritable<ESM4::IdleAnimation>();
    records.insert(*records.searchStatic(key), key);
    EXPECT_THROW(MWWorld::buildOblivionNativeIdleIndex(f.mStore), std::invalid_argument);
    f.mStore.clearDynamic();
    const auto& after = f.mStore.getOblivionNativeIdleLoadEvents();
    ASSERT_EQ(after.size(), history.size());
    for (std::size_t i = 0; i < after.size(); ++i)
    {
        EXPECT_EQ(after[i].mMetadata, history[i].mMetadata);
        EXPECT_EQ(after[i].mRegistration.mModel, history[i].mRegistration.mModel);
    }
    const auto index = MWWorld::buildOblivionNativeIdleIndex(f.mStore);
    ASSERT_EQ(index.mNodes.size(), 1u);
    EXPECT_EQ(index.mBanks[1].mRoots, (std::vector<std::size_t>{0}));
}
