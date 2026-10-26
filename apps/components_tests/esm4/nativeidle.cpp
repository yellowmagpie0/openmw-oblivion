#include <components/esm4/nativeidle.hpp>
#include <components/esm4/common.hpp>
#include <components/esm4/loadidle.hpp>

#include <gtest/gtest.h>

#include <array>
#include <stdexcept>
#include <utility>

namespace
{
    struct PathCase { const char* mPath; const char* mKey; };
    constexpr PathCase sPaths[] = {
#include "native_idle_path_expected.inc"
    };
    struct FileCase { const char* mPath; bool mFile; };
    constexpr FileCase sFiles[] = {
#include "native_idle_file_expected.inc"
    };
    struct TreeCase
    {
        unsigned mConditions, mDeleted, mOptional, mFiles;
        bool mEmpty;
        int mSelected;
        std::array<int, 4> mOrder;
        std::size_t mCount;
    };
    constexpr TreeCase sTrees[] = {
#include "native_idle_tree_expected.inc"
    };
    struct SelectorCase
    {
        const char* mModel;
        const char* mBank;
        bool mOptional, mFile, mCondition, mDeleted, mReverse;
        int mSelected;
        std::array<int, 2> mOrder;
        std::size_t mCount;
    };
    constexpr SelectorCase sSelectors[] = {
#include "native_idle_selector_expected.inc"
    };
    struct BoundaryCase
    {
        const char* mBytes;
        std::size_t mSize;
        const char* mKey;
        bool mSelected, mFile;
    };
    constexpr BoundaryCase sBoundaries[] = {
#include "native_idle_boundaries_expected.inc"
    };
    struct FallbackCase
    {
        bool mOptional, mFile;
        int mChangeFile, mChangeOptional;
        bool mSelected;
    };
    constexpr FallbackCase sFallbacks[] = {
#include "native_idle_fallback_expected.inc"
    };

    template <std::size_t N>
    void identities(std::array<ESM4::IdleAnimation, N>& records)
    {
        for (std::size_t i = 0; i < N; ++i)
            records[i].mId = {static_cast<std::uint32_t>(i + 1), 0};
    }

    const ESM4::IdleAnimation* selected(const auto& records, int index)
    {
        return index < 0 ? nullptr : &records.at(static_cast<std::size_t>(index));
    }
}

TEST(ESM4NativeIdleSelector, ModelKeysMatch237OriginalExecutionsWithoutVfsNormalization)
{
    static_assert(std::size(sPaths) == 237);
    for (const auto& row : sPaths)
    {
        SCOPED_TRACE(row.mPath);
        EXPECT_EQ(ESM4::nativeIdleModelKey(row.mPath), row.mKey);
    }
}

TEST(ESM4NativeIdleSelector, FilePredicateAndOuterPostcheckMatch3584OriginalFlagProfiles)
{
    static_assert(std::size(sFiles) == 14);
    for (const auto& row : sFiles)
        for (unsigned flag = 0; flag < 256; ++flag)
        {
            SCOPED_TRACE(row.mPath);
            SCOPED_TRACE(flag);
            std::array<ESM4::IdleAnimation, 1> records;
            identities(records);
            records[0].mModel = row.mPath;
            records[0].mAnimationGroup = flag;
            const std::array<ESM4::NativeIdleModelBank, 1> banks{{{"Dog\\IdleAnims", {0}}}};
            ESM4::NativeIdleSelector selector({{&records[0], {}}}, banks);
            EXPECT_EQ(ESM4::nativeIdleHasFile(row.mPath), row.mFile);
            EXPECT_EQ(selector.select("Dog\\Skeleton.nif", [](const auto&) { return true; }),
                row.mFile ? &records[0] : nullptr);
        }
}

TEST(ESM4NativeIdleSelector, SubtreeSelectionAndConditionOrderMatch2048OriginalProfiles)
{
    static_assert(std::size(sTrees) == 2048);
    for (std::size_t i = 0; i < std::size(sTrees); ++i)
    {
        SCOPED_TRACE(i);
        const auto& row = sTrees[i];
        std::array<ESM4::IdleAnimation, 4> records;
        identities(records);
        std::vector<ESM4::NativeIdleNode> nodes;
        for (std::size_t n = 0; n < records.size(); ++n)
        {
            auto& record = records[n];
            record.mFlags = row.mDeleted & (1u << n) ? std::uint32_t(ESM4::Rec_Deleted) : 0;
            record.mAnimationGroup = row.mOptional & (1u << n) ? 0x80 : 5;
            record.mModel = row.mFiles & (1u << n) ? "Dog\\IdleAnims\\a.kF" : "Dog\\IdleAnims\\folder";
            nodes.push_back({&record, {}});
        }
        if (!row.mEmpty)
        {
            nodes[0].mChildren = {1, 2};
            nodes[1].mChildren = {3};
        }
        const std::array<ESM4::NativeIdleModelBank, 1> banks{{{"Dog\\IdleAnims", {0}}}};
        ESM4::NativeIdleSelector selector(std::move(nodes), banks);
        std::vector<int> order;
        const auto* actual = selector.select("Dog\\Skeleton.nif", [&](const auto& record) {
            const auto index = &record - records.data();
            order.push_back(static_cast<int>(index));
            return (row.mConditions & (1u << index)) != 0;
        });
        // The native subtree may return an optional no-file record; the
        // actual outer selector then rejects it without trying a later root.
        const int expected = row.mSelected >= 0 && (row.mFiles & (1u << row.mSelected))
            ? row.mSelected : -1;
        EXPECT_EQ(actual, selected(records, expected));
        EXPECT_EQ(order, (std::vector<int>(row.mOrder.begin(), row.mOrder.begin() + row.mCount)));
    }
}

TEST(ESM4NativeIdleSelector, CompleteModelBankSelectionMatches1280OriginalProfiles)
{
    static_assert(std::size(sSelectors) == 1280);
    for (std::size_t i = 0; i < std::size(sSelectors); ++i)
    {
        SCOPED_TRACE(i);
        const auto& row = sSelectors[i];
        std::array<ESM4::IdleAnimation, 2> records;
        identities(records);
        records[0].mFlags = row.mDeleted ? std::uint32_t(ESM4::Rec_Deleted) : 0;
        records[0].mAnimationGroup = row.mOptional ? 0x80 : 5;
        records[0].mModel = row.mFile ? "Completely\\Different\\X.kf" : "Completely\\Different\\Folder";
        records[1].mAnimationGroup = 5;
        records[1].mModel = "Completely\\Different\\X.kf";
        const std::array<ESM4::NativeIdleModelBank, 1> banks{{{row.mBank,
            row.mReverse ? std::vector<std::size_t>{1, 0} : std::vector<std::size_t>{0, 1}}}};
        ESM4::NativeIdleSelector selector({{&records[0], {}}, {&records[1], {}}}, banks);
        std::vector<int> order;
        const auto* actual = selector.select(row.mModel, [&](const auto& record) {
            const auto index = &record - records.data();
            order.push_back(static_cast<int>(index));
            return index != 0 || row.mCondition;
        });
        EXPECT_EQ(actual, selected(records, row.mSelected));
        EXPECT_EQ(order, (std::vector<int>(row.mOrder.begin(), row.mOrder.begin() + row.mCount)));
    }
}

TEST(ESM4NativeIdleSelector, RejectsMalformedForestsAndCaseAliasedBanks)
{
    std::array<ESM4::IdleAnimation, 2> records;
    identities(records);
    const std::array<ESM4::NativeIdleModelBank, 1> banks{{{"Dog\\IdleAnims", {0}}}};
    EXPECT_THROW((ESM4::NativeIdleSelector({{nullptr, {}}}, banks)), std::invalid_argument);
    EXPECT_THROW((ESM4::NativeIdleSelector({{&records[0], {}}, {&records[0], {}}}, banks)), std::invalid_argument);
    EXPECT_THROW((ESM4::NativeIdleSelector({{&records[0], {2}}}, banks)), std::invalid_argument);
    EXPECT_THROW((ESM4::NativeIdleSelector({{&records[0], {1, 1}}, {&records[1], {}}}, banks)), std::invalid_argument);
    EXPECT_THROW((ESM4::NativeIdleSelector({{&records[0], {1}}, {&records[1], {0}}}, banks)), std::invalid_argument);
    const std::array<ESM4::NativeIdleModelBank, 2> aliases{{{"DOG\\IdleAnims", {0}}, {"dog\\idleanims", {0}}}};
    EXPECT_THROW((ESM4::NativeIdleSelector({{&records[0], {}}}, aliases)), std::invalid_argument);
    const std::array<ESM4::NativeIdleModelBank, 1> childRoot{{{"Dog\\IdleAnims", {1}}}};
    EXPECT_THROW((ESM4::NativeIdleSelector({{&records[0], {1}}, {&records[1], {}}}, childRoot)), std::invalid_argument);
    EXPECT_THROW(ESM4::nativeIdleModelKey(std::string(260, 'a')), std::invalid_argument);
    EXPECT_THROW(ESM4::nativeIdleHasFile(std::string("a.") + char(0x80)), std::invalid_argument);
}

TEST(ESM4NativeIdleSelector, LongDerivedKeysAndNulTailsMatch26OriginalBoundaryProfiles)
{
    static_assert(std::size(sBoundaries) == 26);
    for (const auto& row : sBoundaries)
    {
        SCOPED_TRACE(row.mSize);
        const std::string_view path(row.mBytes, row.mSize);
        EXPECT_EQ(ESM4::nativeIdleModelKey(path), row.mKey);
        EXPECT_EQ(ESM4::nativeIdleHasFile(path), row.mFile);
        std::array<ESM4::IdleAnimation, 1> records;
        identities(records);
        records[0].mModel = "Outside\\X.kf";
        const std::array<ESM4::NativeIdleModelBank, 1> banks{{{row.mKey, {0}}}};
        ESM4::NativeIdleSelector selector({{&records[0], {}}}, banks);
        EXPECT_EQ(selector.select(path, [](const auto&) { return true; }), row.mSelected ? &records[0] : nullptr);
    }
}

TEST(ESM4NativeIdleSelector, ConsumesFileInputsOnlyAtTheNativeBranchAndCanRetryAfterFailure)
{
    std::array<ESM4::IdleAnimation, 2> records;
    identities(records);
    records[0].mModel = std::string(260, 'x'); // Unsupported if the native path consumes it.
    records[1].mModel = "Outside\\X.kf";
    const std::array<ESM4::NativeIdleModelBank, 1> banks{{{"Dog\\IdleAnims", {0}}}};
    ESM4::NativeIdleSelector selector({{&records[0], {1}}, {&records[1], {}}}, banks);
    unsigned queries = 0;
    const auto matches = [&](const auto&) { ++queries; return true; };
    records[0].mFlags = ESM4::Rec_Deleted;
    EXPECT_EQ(selector.select("Dog\\Skeleton.nif", matches), nullptr);
    EXPECT_EQ(queries, 0u);
    records[0].mFlags = 0;
    EXPECT_EQ(selector.select("Dog\\Skeleton.nif", [](const auto&) { return false; }), nullptr);
    EXPECT_THROW(selector.select("Dog\\Skeleton.nif", matches), std::invalid_argument);
    EXPECT_EQ(queries, 1u); // Parent file query precedes child conditions.
    records[0].mAnimationGroup = 0x80;
    EXPECT_EQ(selector.select("Dog\\Skeleton.nif", matches), &records[1]);
    EXPECT_EQ(queries, 3u); // Optional parent bypasses its unused invalid filename.
    EXPECT_THROW(selector.select("Dog\\Skeleton.nif", [](const auto&) -> bool {
        throw std::runtime_error("condition adapter rejected");
    }), std::runtime_error);
    EXPECT_EQ(selector.select("Dog\\Skeleton.nif", matches), &records[1]);
}

TEST(ESM4NativeIdleSelector, TraversesDeepValidatedForestsWithoutRecursiveStackGrowth)
{
    constexpr std::size_t count = 10000;
    std::vector<ESM4::IdleAnimation> records(count);
    std::vector<ESM4::NativeIdleNode> nodes;
    nodes.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        records[i].mId = {static_cast<std::uint32_t>(i + 1), 0};
        records[i].mModel = i + 1 == count ? "Outside\\X.kf" : "Folder";
        nodes.push_back({&records[i], i + 1 == count ? std::vector<std::size_t>{}
            : std::vector<std::size_t>{i + 1}});
    }
    const std::array<ESM4::NativeIdleModelBank, 1> banks{{{"Dog\\IdleAnims", {0}}}};
    ESM4::NativeIdleSelector selector(std::move(nodes), banks);
    std::size_t queries = 0;
    EXPECT_EQ(selector.select("Dog\\Skeleton.nif", [&](const auto&) { ++queries; return true; }), &records.back());
    EXPECT_EQ(queries, count);
}

TEST(ESM4NativeIdleSelector, OptionalOnlyPostcheckMatches36OriginalReadConsumptionProfiles)
{
    static_assert(std::size(sFallbacks) == 36);
    for (std::size_t i = 0; i < std::size(sFallbacks); ++i)
    {
        SCOPED_TRACE(i);
        const auto& row = sFallbacks[i];
        std::array<ESM4::IdleAnimation, 2> records;
        identities(records);
        records[0].mModel = row.mFile ? "Parent\\A.kf" : "Parent\\Folder";
        records[0].mAnimationGroup = row.mOptional ? 0x80 : 5;
        records[1].mModel = "Child\\B.kf";
        const std::array<ESM4::NativeIdleModelBank, 1> banks{{{"Dog\\IdleAnims", {0}}}};
        ESM4::NativeIdleSelector selector({{&records[0], {1}}, {&records[1], {}}}, banks);
        std::vector<int> conditions;
        const auto* actual = selector.select("Dog\\Skeleton.nif", [&](const auto& record) {
            const auto index = &record - records.data();
            conditions.push_back(static_cast<int>(index));
            if (index == 0)
                return true;
            // Synthetic condition-boundary mutations distinguish native file
            // read timing. Ordinary CTDA mutation is not claimed reachable.
            if (row.mChangeFile >= 0)
                records[0].mModel = row.mChangeFile ? "Parent\\A.kf" : "Parent\\Folder";
            if (row.mChangeOptional >= 0)
                records[0].mAnimationGroup = row.mChangeOptional ? 0x80 : 5;
            return false;
        });
        EXPECT_EQ(actual, row.mSelected ? &records[0] : nullptr);
        EXPECT_EQ(conditions, (std::vector<int>{0, 1}));
    }
}

TEST(ESM4NativeIdleSelector, LoadedResourceKeysFoldCaseAndPreserveNativePathSpelling)
{
    EXPECT_EQ(ESM4::nativeIdleResourceKey("Creatures\\Dog\\IdleAnims\\GetUp0.KF"),
        "meshes\\creatures\\dog\\idleanims\\getup0.kf");
    EXPECT_EQ(ESM4::nativeIdleResourceKey("Outside\\same.kf"),
        ESM4::nativeIdleResourceKey("OUTSIDE\\SAME.KF"));
    EXPECT_NE(ESM4::nativeIdleResourceKey("Outside\\same.kf"),
        ESM4::nativeIdleResourceKey("Outside/same.kf"));
    EXPECT_NE(ESM4::nativeIdleResourceKey("Outside\\same.kf"),
        ESM4::nativeIdleResourceKey("Outside\\.\\same.kf"));
    EXPECT_EQ(ESM4::nativeIdleResourceKey(std::string_view("A.KF\0ignored", 12)), "meshes\\a.kf");
    EXPECT_THROW(ESM4::nativeIdleResourceKey(std::string(260, 'a')), std::invalid_argument);
    EXPECT_THROW(ESM4::nativeIdleResourceKey(std::string("a.") + char(0x80)), std::invalid_argument);
}
