#include <components/esm/formkey.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <unordered_map>

namespace
{
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
        index.apply({ parent, "Oblivion.esm", 1, false, ESM::FormChildKind::Persistent, std::nullopt, false });
        index.apply({ base, "Oblivion.esm", 2, false, ESM::FormChildKind::Temporary, parent, true });
        index.apply({ base, "Knights.esp", 2, true, ESM::FormChildKind::Temporary, parent, true });

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
        index.apply({ child, "Oblivion.esm", 1, false, ESM::FormChildKind::Temporary, missing });
        EXPECT_EQ(index.unresolvedEnableParents(), (std::vector<ESM::FormKey>{ child }));
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
