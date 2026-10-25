#include <components/esm4/nativeidle.hpp>
#include <components/esm4/loadidle.hpp>
#include <components/esm4/common.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <stdexcept>

namespace
{
    struct RegistrationCase
    {
        bool mChild;
        std::array<int, 3> mBefore;
        std::size_t mBeforeSize;
        int mPrevious;
        bool mDeleted;
        std::array<int, 4> mAfter, mPreviousLinks;
        std::size_t mAfterSize;
    };
    constexpr RegistrationCase sCases[] = {
#include "native_idle_registration_expected.inc"
    };

    int identity(const ESM4::NativeIdleRegistrationIndex& index, std::optional<std::size_t> value)
    {
        return value ? static_cast<int>(index.mNodes.at(*value).mRecord->mId.mIndex) - 1 : -1;
    }
}

TEST(ESM4NativeIdleRegistration, OrderedInsertionAndLinkRepairMatch168OriginalProfiles)
{
    static_assert(std::size(sCases) == 168);
    for (std::size_t c = 0; c < std::size(sCases); ++c)
    {
        SCOPED_TRACE(c);
        const auto& row = sCases[c];
        std::array<ESM4::IdleAnimation, 5> records;
        for (std::size_t i = 0; i < records.size(); ++i)
        {
            records[i].mId = {static_cast<std::uint32_t>(i + 1), 0};
            records[i].mModel = "Other\\IdleAnims\\Node.kf";
        }
        std::vector<const ESM4::IdleAnimation*> order;
        // A declared predecessor absent from the target array remains a real
        // earlier typed form in another model bank, as in the native fixture.
        for (int i = 0; i < 3; ++i)
            if (std::find(row.mBefore.begin(), row.mBefore.begin() + row.mBeforeSize, i)
                == row.mBefore.begin() + row.mBeforeSize)
                order.push_back(&records[i]);
        records[4].mModel = "Dog\\IdleAnims\\Parent.kf";
        if (row.mChild)
            order.push_back(&records[4]);
        for (std::size_t i = 0; i < row.mBeforeSize; ++i)
        {
            auto& record = records[row.mBefore[i]];
            record.mModel = "Dog\\IdleAnims\\Node.kf";
            record.mParent = row.mChild ? records[4].mId : ESM::FormId{};
            record.mPrevious = i == 0 ? ESM::FormId{} : records[row.mBefore[i - 1]].mId;
            order.push_back(&record);
        }
        records[3].mModel = "Dog\\IdleAnims\\New.kf";
        records[3].mFlags = row.mDeleted ? std::uint32_t(ESM4::Rec_Deleted) : 0;
        records[3].mParent = row.mChild ? records[4].mId : ESM::FormId{};
        records[3].mPrevious = row.mPrevious < 0 ? ESM::FormId{} : records[row.mPrevious].mId;
        order.push_back(&records[3]);
        const auto authored = records;
        const auto index = ESM4::registerNativeIdleRecords(order);
        const std::vector<std::size_t>* actual = nullptr;
        if (row.mChild)
            for (const auto& node : index.mNodes)
                if (node.mRecord == &records[4])
                    actual = &node.mChildren;
        if (!row.mChild)
            for (const auto& bank : index.mBanks)
                if (bank.mKey == "Dog\\IdleAnims")
                    actual = &bank.mRoots;
        ASSERT_NE(actual, nullptr);
        ASSERT_EQ(actual->size(), row.mAfterSize);
        for (std::size_t i = 0; i < actual->size(); ++i)
        {
            const auto node = (*actual)[i];
            EXPECT_EQ(identity(index, node), row.mAfter[i]);
            EXPECT_EQ(identity(index, index.mNodes[node].mPrevious), row.mPreviousLinks[i]);
            EXPECT_EQ(identity(index, index.mNodes[node].mParent), row.mChild ? 4 : -1);
        }
        for (std::size_t i = 0; i < records.size(); ++i)
        {
            EXPECT_EQ(records[i].mParent, authored[i].mParent);
            EXPECT_EQ(records[i].mPrevious, authored[i].mPrevious);
        }
    }
}

TEST(ESM4NativeIdleRegistration, BuildsCaseFoldedBanksAndSelectionWithoutMutatingAuthoredLinks)
{
    ESM4::IdleAnimation first, second, child;
    first.mId = {1, 3}; second.mId = {2, 3}; child.mId = {3, 3};
    first.mModel = "Dog\\IdleAnims\\First.kf";
    second.mModel = "DOG\\IdleAnims\\Second.kf";
    child.mModel = "Outside\\Child.kf";
    child.mParent = second.mId;
    const std::array<const ESM4::IdleAnimation*, 3> records{&first, &second, &child};
    auto index = ESM4::registerNativeIdleRecords(records);
    ASSERT_EQ(index.mBanks.size(), 1u);
    EXPECT_EQ(index.mBanks[0].mRoots, (std::vector<std::size_t>{1, 0}));
    EXPECT_EQ(index.mNodes[0].mPrevious, 1u);
    EXPECT_EQ(first.mPrevious.mIndex, 0u);
    ESM4::NativeIdleSelector selector(std::move(index.mNodes), index.mBanks);
    EXPECT_EQ(selector.select("dog\\Skeleton.nif", [](const auto&) { return true; }), &child);
}

TEST(ESM4NativeIdleRegistration, RejectsUnsupportedDefinitionsAndForwardLinksWithoutChangingRecords)
{
    ESM4::IdleAnimation first, second;
    first.mId = {1, 0}; second.mId = {2, 0};
    first.mModel = second.mModel = "Dog\\IdleAnims\\Node.kf";
    const std::array<const ESM4::IdleAnimation*, 2> duplicate{&first, &first};
    EXPECT_THROW(ESM4::registerNativeIdleRecords(duplicate), std::invalid_argument);
    const std::array<const ESM4::IdleAnimation*, 1> missing{nullptr};
    EXPECT_THROW(ESM4::registerNativeIdleRecords(missing), std::invalid_argument);
    const std::array<const ESM4::IdleAnimation*, 2> order{&first, &second};
    first.mParent = second.mId;
    EXPECT_THROW(ESM4::registerNativeIdleRecords(order), std::invalid_argument);
    EXPECT_EQ(first.mParent, second.mId);
    first.mParent = {};
    first.mPrevious = second.mId;
    EXPECT_THROW(ESM4::registerNativeIdleRecords(order), std::invalid_argument);
    first.mPrevious = {};
    first.mModel = "Unresolved";
    EXPECT_THROW(ESM4::registerNativeIdleRecords(order), std::invalid_argument);
    EXPECT_TRUE(ESM4::registerNativeIdleRecords({}).mNodes.empty());
}

TEST(ESM4NativeIdleRegistration, ReplaysOrdinaryOverridesUsingHistoricalInputsAndRepairsBothSiblingChains)
{
    ESM4::IdleAnimation first, second, inserted;
    first.mId = {1, 3}; second.mId = {2, 3}; inserted.mId = {3, 3};
    first.mModel = second.mModel = inserted.mModel = "Dog\\IdleAnims\\Node.kf";
    auto initial = ESM4::nativeIdleRegistrationEvent(first);
    second.mPrevious = first.mId;
    const auto sibling = ESM4::nativeIdleRegistrationEvent(second);
    inserted.mPrevious = second.mId;
    const auto addition = ESM4::nativeIdleRegistrationEvent(inserted);
    first.mPrevious = inserted.mId;
    const auto override = ESM4::nativeIdleRegistrationEvent(first);
    const std::array<const ESM4::IdleAnimation*, 3> records{&first, &second, &inserted};
    EXPECT_THROW(ESM4::registerNativeIdleRecords(records), std::invalid_argument);
    const std::array events{initial, sibling, addition, override};
    const auto index = ESM4::replayNativeIdleRegistration(records, events);
    ASSERT_EQ(index.mBanks.size(), 1u);
    EXPECT_EQ(index.mBanks[0].mRoots, (std::vector<std::size_t>{1, 2, 0}));
    EXPECT_FALSE(index.mNodes[1].mPrevious);
    EXPECT_EQ(index.mNodes[2].mPrevious, 1u);
    EXPECT_EQ(index.mNodes[0].mPrevious, 2u);
    EXPECT_EQ(first.mPrevious, inserted.mId);
    EXPECT_EQ(second.mPrevious, first.mId);
}

TEST(ESM4NativeIdleRegistration, MovesRegistrationsAcrossBanksAndParentsWithoutRewritingAuthoredData)
{
    ESM4::IdleAnimation root, other, child;
    root.mId = {1, 3}; other.mId = {2, 3}; child.mId = {3, 3};
    root.mModel = "Dog\\IdleAnims\\Root.kf";
    other.mModel = "Fox\\IdleAnims\\Other.kf";
    child.mModel = "Outside\\Child.kf";
    child.mParent = root.mId;
    std::vector events{ESM4::nativeIdleRegistrationEvent(root), ESM4::nativeIdleRegistrationEvent(other),
        ESM4::nativeIdleRegistrationEvent(child)};
    root.mModel = "Wolf\\IdleAnims\\Root.kf";
    events.push_back(ESM4::nativeIdleRegistrationEvent(root));
    const std::array<const ESM4::IdleAnimation*, 3> records{&root, &other, &child};
    const auto movedRoot = ESM4::replayNativeIdleRegistration(records, events);
    EXPECT_EQ(movedRoot.mNodes[0].mChildren, (std::vector<std::size_t>{2}));
    EXPECT_EQ(movedRoot.mNodes[2].mParent, 0u);
    child.mParent = other.mId;
    events.push_back(ESM4::nativeIdleRegistrationEvent(child));
    const auto index = ESM4::replayNativeIdleRegistration(records, events);
    ASSERT_EQ(index.mBanks.size(), 3u);
    EXPECT_TRUE(index.mBanks[0].mRoots.empty());
    EXPECT_EQ(index.mBanks[1].mRoots, (std::vector<std::size_t>{1}));
    EXPECT_EQ(index.mBanks[2].mRoots, (std::vector<std::size_t>{0}));
    EXPECT_TRUE(index.mNodes[0].mChildren.empty());
    EXPECT_EQ(index.mNodes[1].mChildren, (std::vector<std::size_t>{2}));
    EXPECT_EQ(index.mNodes[2].mParent, 1u);
    EXPECT_EQ(child.mParent, other.mId);
    EXPECT_EQ(root.mModel.getOriginal(), "Wolf\\IdleAnims\\Root.kf");
}

TEST(ESM4NativeIdleRegistration, RejectsIncompleteEventsSpecialOverridesAndCyclesBeforePublishing)
{
    ESM4::IdleAnimation root, child;
    root.mId = {1, 3}; child.mId = {2, 3};
    root.mModel = child.mModel = "Dog\\IdleAnims\\Node.kf";
    child.mParent = root.mId;
    const std::array<const ESM4::IdleAnimation*, 2> records{&root, &child};
    std::vector events{ESM4::nativeIdleRegistrationEvent(root), ESM4::nativeIdleRegistrationEvent(child)};
    EXPECT_THROW(ESM4::replayNativeIdleRegistration(records, {}), std::invalid_argument);
    for (const auto flag : {std::uint32_t(ESM4::Rec_Deleted), 0x4000u})
    {
        auto special = events.front(); special.mFlags = flag;
        events.push_back(special);
        EXPECT_THROW(ESM4::replayNativeIdleRegistration(records, events), std::invalid_argument);
        events.pop_back();
    }
    auto cycle = events.front(); cycle.mParent = child.mId;
    events.push_back(cycle);
    EXPECT_THROW(ESM4::replayNativeIdleRegistration(records, events), std::invalid_argument);
    EXPECT_TRUE(root.mParent.mIndex == 0 && child.mParent == root.mId);
}

TEST(ESM4NativeIdleRegistration, OrdinaryOverrideRemovalMatchesOriginalRootAndChildProfiles)
{
    struct RemovalCase
    {
        std::array<int, 3> mBefore;
        std::size_t mBeforeSize;
        int mSelected;
        bool mChild;
        std::array<int, 3> mAfter;
        std::size_t mAfterSize;
        std::array<int, 3> mPrevious;
    };
    constexpr RemovalCase cases[]{
#include "native_idle_override_removal_expected.inc"
    };
    for (const auto& row : cases)
    {
        std::array<ESM4::IdleAnimation, 3> storage;
        ESM4::IdleAnimation parent;
        parent.mId = {10, 3}; parent.mModel = "Fox\\IdleAnims\\Parent.kf";
        std::vector<const ESM4::IdleAnimation*> records;
        std::vector<ESM4::NativeIdleRegistrationEvent> events;
        if (row.mChild)
        {
            records.push_back(&parent);
            events.push_back(ESM4::nativeIdleRegistrationEvent(parent));
        }
        ESM::FormId previous;
        for (std::size_t i = 0; i < row.mBeforeSize; ++i)
        {
            const auto id = row.mBefore[i];
            auto& record = storage[id];
            record.mId = {static_cast<std::uint32_t>(id + 1), 3};
            record.mModel = "Dog\\IdleAnims\\Node.kf";
            record.mParent = row.mChild ? parent.mId : ESM::FormId{};
            record.mPrevious = previous;
            previous = record.mId;
            records.push_back(&record);
            events.push_back(ESM4::nativeIdleRegistrationEvent(record));
        }
        auto& selected = storage[row.mSelected];
        selected.mModel = "Wolf\\IdleAnims\\Moved.kf";
        selected.mParent = {}; selected.mPrevious = {};
        events.push_back(ESM4::nativeIdleRegistrationEvent(selected));
        const auto authored = storage;
        const auto index = ESM4::replayNativeIdleRegistration(records, events);
        const std::vector<std::size_t>* remaining = nullptr;
        if (row.mChild)
            remaining = &index.mNodes[0].mChildren;
        else
            for (const auto& bank : index.mBanks)
                if (bank.mKey == "Dog\\IdleAnims") remaining = &bank.mRoots;
        ASSERT_NE(remaining, nullptr);
        ASSERT_EQ(remaining->size(), row.mAfterSize);
        for (std::size_t i = 0; i < remaining->size(); ++i)
        {
            const auto node = (*remaining)[i];
            EXPECT_EQ(identity(index, node), row.mAfter[i]);
            EXPECT_EQ(identity(index, index.mNodes[node].mPrevious), row.mPrevious[i]);
        }
        for (std::size_t i = 0; i < storage.size(); ++i)
        {
            EXPECT_EQ(storage[i].mParent, authored[i].mParent);
            EXPECT_EQ(storage[i].mPrevious, authored[i].mPrevious);
        }
    }
}
