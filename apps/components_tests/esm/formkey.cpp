#include <components/esm/formkey.hpp>
#include <components/esm/fourcc.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>

namespace
{
    ESM::FormRecordMetadata makeMetadata(const ESM::FormKey& key, std::string plugin, std::uint32_t recordType,
        bool deleted = false, ESM::FormChildKind childKind = ESM::FormChildKind::None,
        std::optional<ESM::FormKey> enableParent = std::nullopt, bool enableParentInverted = false,
        std::optional<ESM::FormKey> parent = std::nullopt)
    {
        ESM::FormRecordMetadata result;
        result.mKey = key;
        result.mWinningPlugin = std::move(plugin);
        result.mRecordType = recordType;
        result.mDeleted = deleted;
        result.mChildKind = childKind;
        result.mEnableParent = std::move(enableParent);
        result.mEnableParentInverted = enableParentInverted;
        result.mParent = std::move(parent);
        return result;
    }

    TEST(FormKeyTest, contentIdentityIsNormalizedAndSerializable)
    {
        const ESM::FormKey key = ESM::FormKey::content("Data/Oblivion.ESM", 0x1234);
        EXPECT_EQ(key.mNamespace, "oblivion.esm");
        EXPECT_EQ(key.localId(), 0x1234);
        EXPECT_EQ(key.serialize(), "content:oblivion.esm:001234");
        EXPECT_EQ(ESM::FormKey::deserialize(key.serialize()), key);
    }

    TEST(FormKeyTest, dynamicIdentityRoundTrips)
    {
        const ESM::FormKey key = ESM::FormKey::dynamic("openmw", 0x123456789abcdef0);
        EXPECT_EQ(key.serialize(), "dynamic:openmw:123456789abcdef0");
        EXPECT_EQ(ESM::FormKey::deserialize(key.serialize()), key);
    }

    TEST(FormKeyTest, resolverSurvivesLoadOrderChanges)
    {
        const ESM::FormKeyResolver first({ "Oblivion.esm", "Knights.esp" });
        const ESM::FormKeyResolver second({ "Knights.esp", "Oblivion.esm" });
        const ESM::FormKey stable = first.toFormKey({ 0x42, 0 });
        EXPECT_EQ(stable, second.toFormKey({ 0x42, 1 }));
        EXPECT_EQ(first.toFormId(stable), (ESM::FormId{ 0x42, 0 }));
        EXPECT_EQ(second.toFormId(stable), (ESM::FormId{ 0x42, 1 }));
    }

    TEST(FormKeyTest, missingPluginDoesNotResolveBackToRuntimeId)
    {
        const ESM::FormKeyResolver resolver({ "Oblivion.esm" });
        EXPECT_EQ(resolver.toFormId(ESM::FormKey::content("Knights.esp", 1)), std::nullopt);
    }

    TEST(FormKeyTest, rawMasterReferencesResolveToDefiningPlugin)
    {
        const std::vector<std::string> masters{ "Oblivion.esm", "DLCShiveringIsles.esp" };
        EXPECT_EQ(ESM::FormKeyResolver::resolveRaw({ 0x123, 0 }, "Knights.esp", masters),
            ESM::FormKey::content("Oblivion.esm", 0x123));
        EXPECT_EQ(ESM::FormKeyResolver::resolveRaw({ 0x456, 2 }, "Knights.esp", masters),
            ESM::FormKey::content("Knights.esp", 0x456));
    }

    TEST(FormKeyTest, disambiguatesTes4ModIndexZeroRecordHeaders)
    {
        const ESM::FormKey master = ESM::FormKey::content("Oblivion.esm", 0x123);
        const ESM::FormKey local = ESM::FormKey::content("DLCHorseArmor.esp", 0x123);
        ESM::FormKeyIndex index;
        EXPECT_EQ(index.resolveRecordHeader(master, local, ESM::fourCC("QUST")), local);
        ESM::FormRecordMetadata base;
        base.mKey = master;
        base.mWinningPlugin = "Oblivion.esm";
        base.mRecordType = ESM::fourCC("QUST");
        index.apply(std::move(base));
        EXPECT_EQ(index.resolveRecordHeader(master, local, ESM::fourCC("QUST")), master);
        EXPECT_EQ(index.resolveRecordHeader(master, local, ESM::fourCC("SPEL")), local);
    }

    TEST(FormKeyTest, dynamicAllocatorCanResumeFromSavedSerial)
    {
        ESM::DynamicFormKeyAllocator allocator("save-7", 41);
        EXPECT_EQ(allocator.allocate(), ESM::FormKey::dynamic("save-7", 41));
        EXPECT_EQ(allocator.allocate(), ESM::FormKey::dynamic("save-7", 42));
        EXPECT_EQ(allocator.nextSerial(), 43);
    }

    TEST(FormKeyTest, extremeRuntimeFormIdDoesNotOverflow)
    {
        const ESM::FormKeyResolver resolver({ "Oblivion.esm" });
        const ESM::FormId extreme{ 0xffffffff, std::numeric_limits<std::int32_t>::min() };
        const ESM::FormKey stable = resolver.toFormKey(extreme);
        EXPECT_TRUE(stable.isDynamic());
        EXPECT_EQ(resolver.toFormId(stable), extreme);
    }

    TEST(FormKeyTest, overrideDeletionAndEnableParentGraphAreExplicit)
    {
        const ESM::FormKey base = ESM::FormKey::content("Oblivion.esm", 0x100);
        const ESM::FormKey parent = ESM::FormKey::content("Oblivion.esm", 0x200);
        ESM::FormKeyIndex index;
        index.apply(makeMetadata(parent, "Oblivion.esm", 1, false, ESM::FormChildKind::Persistent));
        index.apply(makeMetadata(base, "Oblivion.esm", 2, false, ESM::FormChildKind::Temporary, parent, true));
        index.apply(makeMetadata(base, "Knights.esp", 2, true, ESM::FormChildKind::Temporary, parent, true));

        ASSERT_NE(index.winner(base), nullptr);
        EXPECT_EQ(index.winner(base)->mWinningPlugin, "knights.esp");
        EXPECT_EQ(index.resolve(base), nullptr);
        ASSERT_NE(index.history(base), nullptr);
        EXPECT_EQ(index.history(base)->size(), 2);
        EXPECT_TRUE(index.unresolvedEnableParents().empty());
    }

    TEST(FormKeyTest, reportsUnresolvedEnableParent)
    {
        const ESM::FormKey child = ESM::FormKey::content("Oblivion.esm", 1);
        const ESM::FormKey missing = ESM::FormKey::content("Oblivion.esm", 2);
        ESM::FormKeyIndex index;
        index.apply(makeMetadata(child, "Oblivion.esm", 1, false, ESM::FormChildKind::Temporary, missing));
        EXPECT_EQ(index.unresolvedEnableParents(), (std::vector<ESM::FormKey>{ child }));
    }

    TEST(FormKeyTest, authoritativeGraphReportsMissingAndDeletedWinningTargets)
    {
        const ESM::FormKey source = ESM::FormKey::content("Oblivion.esm", 1);
        const ESM::FormKey missing = ESM::FormKey::content("Oblivion.esm", 2);
        const ESM::FormKey deleted = ESM::FormKey::content("Oblivion.esm", 3);
        ESM::FormKeyIndex index;
        index.apply(makeMetadata(deleted, "Oblivion.esm", ESM::fourCC("STAT"), true));

        ESM::FormRecordMetadata record = makeMetadata(source, "Oblivion.esm", ESM::fourCC("REFR"));
        record.mReferences = {
            { missing, ESM::fourCC("NAME"), 0 },
            { deleted, ESM::fourCC("XESP"), 1 },
        };
        index.apply(std::move(record));

        const std::vector<ESM::UnresolvedFormReference> unresolved = index.unresolvedReferences();
        ASSERT_EQ(unresolved.size(), 2);
        EXPECT_EQ(unresolved[0].mTarget, missing);
        EXPECT_EQ(unresolved[0].mReason, ESM::UnresolvedFormReferenceReason::Missing);
        EXPECT_EQ(unresolved[1].mTarget, deleted);
        EXPECT_EQ(unresolved[1].mReason, ESM::UnresolvedFormReferenceReason::Deleted);
    }

    TEST(FormKeyTest, enableParentCyclesAreCanonicalAndDeterministic)
    {
        const ESM::FormKey a = ESM::FormKey::content("Oblivion.esm", 1);
        const ESM::FormKey b = ESM::FormKey::content("Oblivion.esm", 2);
        const ESM::FormKey c = ESM::FormKey::content("Oblivion.esm", 3);
        ESM::FormKeyIndex index;
        index.apply(makeMetadata(b, "Oblivion.esm", ESM::fourCC("REFR"), false,
            ESM::FormChildKind::Temporary, c));
        index.apply(makeMetadata(c, "Oblivion.esm", ESM::fourCC("REFR"), false,
            ESM::FormChildKind::Temporary, a));
        index.apply(makeMetadata(a, "Oblivion.esm", ESM::fourCC("REFR"), false,
            ESM::FormChildKind::Temporary, b));

        EXPECT_EQ(index.enableParentCycles(), (std::vector<std::vector<ESM::FormKey>>{ { a, b, c } }));
    }

    TEST(FormKeyTest, fullGraphSurvivesDeterministicSaveReload)
    {
        const ESM::FormKey base = ESM::FormKey::content("Oblivion.esm", 0x100);
        const ESM::FormKey parent = ESM::FormKey::content("Oblivion.esm", 0x200);
        ESM::FormKeyIndex expected;
        expected.apply(makeMetadata(
            parent, "Oblivion.esm", ESM::fourCC("CELL"), false, ESM::FormChildKind::Persistent));
        ESM::FormRecordMetadata record = makeMetadata(base, "Knights.esp", ESM::fourCC("REFR"), false,
            ESM::FormChildKind::Temporary, parent, true, parent);
        record.mReferences.push_back({ parent, ESM::fourCC("NAME"), 4 });
        expected.apply(std::move(record));

        const std::vector<std::uint8_t> bytes = expected.serialize();
        const ESM::FormKeyIndex actual = ESM::FormKeyIndex::deserialize(bytes);
        EXPECT_EQ(actual.canonicalJson(), expected.canonicalJson());
        EXPECT_EQ(actual.serialize(), bytes);
        EXPECT_EQ(actual.keyCount(), 2);
        EXPECT_EQ(actual.revisionCount(), 2);
        EXPECT_EQ(actual.referenceCount(), 2);
    }

    TEST(FormKeyTest, graphCodecRejectsTruncationAndTrailingData)
    {
        ESM::FormKeyIndex index;
        index.apply(makeMetadata(
            ESM::FormKey::content("Oblivion.esm", 1), "Oblivion.esm", ESM::fourCC("STAT")));
        std::vector<std::uint8_t> bytes = index.serialize();
        bytes.pop_back();
        EXPECT_THROW(ESM::FormKeyIndex::deserialize(bytes), std::runtime_error);
        bytes = index.serialize();
        bytes.push_back(0);
        EXPECT_THROW(ESM::FormKeyIndex::deserialize(bytes), std::runtime_error);
    }

    TEST(FormKeyTest, randomizedPluginMatricesPreserveIdentityAndAuthoritativeWinners)
    {
        const std::vector<std::string> plugins{
            "Oblivion.esm", "DLCFrostcrag.esp", "DLCMehrunesRazor.esp", "DLCVileLair.esp", "Knights.esp"
        };
        for (std::uint32_t seed = 0; seed < 128; ++seed)
        {
            std::mt19937 random(seed);
            std::vector<std::string> runtimeOrder = plugins;
            std::shuffle(runtimeOrder.begin(), runtimeOrder.end(), random);
            const ESM::FormKeyResolver resolver(runtimeOrder);

            ESM::FormKeyIndex graph;
            std::map<ESM::FormKey, std::string> expectedWinner;
            std::vector<ESM::FormKey> keys;
            for (std::uint32_t local = 1; local <= 24; ++local)
            {
                const ESM::FormKey key = ESM::FormKey::content("Oblivion.esm", local);
                keys.push_back(key);
                ESM::FormRecordMetadata record;
                record.mKey = key;
                record.mWinningPlugin = "Oblivion.esm";
                record.mRecordType = local % 3 == 0 ? ESM::fourCC("REFR") : ESM::fourCC("STAT");
                record.mChildKind = static_cast<ESM::FormChildKind>(local % 4);
                graph.apply(record);
                expectedWinner[key] = "oblivion.esm";
            }

            std::vector<std::size_t> semanticOrder(plugins.size() - 1);
            std::iota(semanticOrder.begin(), semanticOrder.end(), 1);
            std::shuffle(semanticOrder.begin(), semanticOrder.end(), random);
            for (const std::size_t pluginIndex : semanticOrder)
            {
                for (unsigned revision = 0; revision < 16; ++revision)
                {
                    const ESM::FormKey key = keys[random() % keys.size()];
                    ESM::FormRecordMetadata record;
                    record.mKey = key;
                    record.mWinningPlugin = plugins[pluginIndex];
                    record.mRecordType = ESM::fourCC("REFR");
                    record.mDeleted = (random() % 7) == 0;
                    record.mChildKind = static_cast<ESM::FormChildKind>(random() % 4);
                    record.mParent = keys[random() % keys.size()];
                    if ((random() & 1) != 0)
                    {
                        record.mEnableParent = keys[random() % keys.size()];
                        record.mEnableParentInverted = (random() & 1) != 0;
                    }
                    record.mReferences.push_back(
                        { keys[random() % keys.size()], ESM::fourCC("NAME"), revision });
                    graph.apply(std::move(record));
                    expectedWinner[key] = ESM::normalizePluginName(plugins[pluginIndex]);
                }
            }

            for (const auto& [key, plugin] : expectedWinner)
            {
                ASSERT_NE(graph.winner(key), nullptr);
                EXPECT_EQ(graph.winner(key)->mWinningPlugin, plugin) << "seed=" << seed << " key=" << key;
            }
            const std::vector<std::uint8_t> state = graph.serialize();
            EXPECT_EQ(ESM::FormKeyIndex::deserialize(state).serialize(), state) << "seed=" << seed;
            EXPECT_NO_THROW(graph.unresolvedReferences()) << "seed=" << seed;
            EXPECT_NO_THROW(graph.enableParentCycles()) << "seed=" << seed;

            const ESM::FormKey stable = ESM::FormKey::content("Oblivion.esm", 7);
            const auto runtimeId = resolver.toFormId(stable);
            ASSERT_TRUE(runtimeId.has_value());
            EXPECT_EQ(resolver.toFormKey(*runtimeId), stable);
            std::vector<std::string> removed = runtimeOrder;
            removed.erase(std::find(removed.begin(), removed.end(), "Knights.esp"));
            const ESM::FormKeyResolver reduced(removed);
            EXPECT_EQ(reduced.toFormId(stable).has_value(), true);
            EXPECT_EQ(reduced.toFormId(ESM::FormKey::content("Knights.esp", 1)), std::nullopt);
        }
    }

    TEST(FormKeyTest, canBeUsedAsUnorderedMapKey)
    {
        std::unordered_map<ESM::FormKey, int> values;
        values[ESM::FormKey::content("Oblivion.esm", 1)] = 7;
        EXPECT_EQ(values.at(ESM::FormKey::content("OBLIVION.ESM", 1)), 7);
    }

    TEST(FormKeyTest, rejectsMalformedKeys)
    {
        EXPECT_THROW(ESM::FormKey::content("Oblivion.esm", 0x1000000), std::invalid_argument);
        EXPECT_THROW(ESM::FormKey::dynamic("", 1), std::invalid_argument);
        EXPECT_THROW(ESM::FormKey::deserialize("content:bad"), std::invalid_argument);
        EXPECT_THROW(ESM::FormKeyResolver({ "Oblivion.esm", "OBLIVION.ESM" }), std::invalid_argument);
    }
}
