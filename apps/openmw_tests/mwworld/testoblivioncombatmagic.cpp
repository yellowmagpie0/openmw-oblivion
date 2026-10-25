#include "apps/openmw/mwworld/esmstore.hpp"
#include "apps/openmw/mwworld/oblivioncombatmagic.hpp"

#include <components/esm4/common.hpp>
#include <components/esm4/reader.hpp>
#include <components/loadinglistener/loadinglistener.hpp>

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <sstream>
#include <vector>

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

    std::string record(std::uint32_t tag, std::uint32_t id, const std::string& data, bool deleted = false)
    {
        return bytes(tag) + bytes(static_cast<std::uint32_t>(data.size()))
            + bytes(deleted ? std::uint32_t(ESM4::Rec_Deleted) : 0u) + bytes(id) + bytes(0u) + data;
    }

    std::string effect(std::uint32_t id = 0x1887, std::uint32_t flags = 0x01000d73, std::int32_t resistance = 66)
    {
        const auto data = bytes(flags) + bytes(475.f) + bytes(48u) + bytes(3u) + bytes(resistance)
            + std::string(44, '\0'); // Full64-byte TES4 DATA; presentation/counter tail irrelevant to this query.
        return record(ESM4::REC_MGEF, id, sub(ESM::fourCC("EDID"), std::string("PARA\0", 5))
            + sub(ESM::fourCC("DATA"), data));
    }

    std::string spell(std::uint32_t duration = 10)
    {
        const auto spit = bytes(0u) + bytes(0u) + bytes(0u) + std::string("\1\0\0\0", 4);
        const auto efit = bytes(ESM::fourCC("PARA")) + bytes(0u) + bytes(0u) + bytes(duration) + bytes(0u) + bytes(48u);
        return record(ESM4::REC_SPEL, 0x137, sub(ESM::fourCC("SPIT"), spit)
            + sub(ESM::fourCC("EFID"), bytes(ESM::fourCC("PARA"))) + sub(ESM::fourCC("EFIT"), efit));
    }

    struct Fixture
    {
        MWWorld::ESMStore mStore;
        Loading::Listener mListener;
        // A preceding file shifts the resolved base index;000137 is a stable
        // Oblivion.esm key, not resolved index0 or any spell with that local ID.
        const std::map<std::string, int> mIndices{{"prefix.esm", 0}, {"oblivion.esm", 1},
            {"patch.esp", 2}, {"other.esm", 3}, {"final.esp", 4}};

        void load(const std::string& name, const std::vector<std::string>& masters,
            const std::vector<std::string>& records)
        {
            auto header = sub(ESM::fourCC("HEDR"), bytes(1.f)
                + bytes(static_cast<std::uint32_t>(records.size())) + bytes(0x2000u));
            for (const auto& master : masters)
                header += sub(ESM::fourCC("MAST"), master + '\0')
                    + sub(ESM::fourCC("DATA"), std::string(8, '\0'));
            auto data = record(ESM4::REC_TES4, 0, header);
            for (const auto& item : records)
                data += item;
            auto stream = std::make_unique<std::stringstream>(data, std::ios::in | std::ios::binary);
            ESM4::Reader reader(std::move(stream), name, nullptr, nullptr, true);
            reader.setModIndex(mIndices.at(name));
            reader.updateModIndices(mIndices);
            mStore.loadESM4(reader, &mListener);
        }

        bool supported() const { return MWWorld::supportsOblivionDefaultCombatParalysis(mStore); }
    };
}

TEST(OblivionCombatMagicStore, SelectsStableDefaultSpellAndWinningOverridesThroughActualRecordLoading)
{
    Fixture fixture;
    EXPECT_FALSE(fixture.supported()); // Do not invent a missing winning MGEF.
    fixture.load("oblivion.esm", {}, {effect()});
    EXPECT_TRUE(fixture.supported()); // No000137 selects the native fallback.
    fixture.load("other.esm", {}, {spell(99)});
    EXPECT_TRUE(fixture.supported()); // Same local ID in another plugin is unrelated.
    const auto before = fixture.mStore.getFormKeyIndex().serialize();
    EXPECT_TRUE(fixture.supported());
    EXPECT_EQ(fixture.mStore.getFormKeyIndex().serialize(), before);
}

TEST(OblivionCombatMagicStore, AuthoredDefaultSpellUsesWinningOverrides)
{
    Fixture fixture;
    fixture.load("oblivion.esm", {}, {effect(), spell()});
    EXPECT_TRUE(fixture.supported());
    fixture.load("patch.esp", {"oblivion.esm"}, {spell(11)});
    EXPECT_FALSE(fixture.supported()); // Winning authored default cannot silently use duration10.
    fixture.load("final.esp", {"oblivion.esm", "patch.esp"}, {spell()});
    EXPECT_TRUE(fixture.supported());
    const auto before = fixture.mStore.getFormKeyIndex().serialize();
    EXPECT_TRUE(fixture.supported());
    EXPECT_EQ(fixture.mStore.getFormKeyIndex().serialize(), before);
}

TEST(OblivionCombatMagicStore, DeletedDefaultSpellUsesFallbackButDeletedParalysisDefinitionStaysUnsupported)
{
    Fixture fixture;
    fixture.load("oblivion.esm", {}, {effect(), spell(11)});
    EXPECT_FALSE(fixture.supported());
    fixture.load("patch.esp", {"oblivion.esm"}, {record(ESM4::REC_SPEL, 0x137, {}, true)});
    EXPECT_TRUE(fixture.supported());
    fixture.load("final.esp", {"oblivion.esm", "patch.esp"}, {record(ESM4::REC_MGEF, 0x1887, {}, true)});
    EXPECT_FALSE(fixture.supported());
}

TEST(OblivionCombatMagicStore, ResolvesWinningEffectPropertiesAndRejectsAmbiguousCodes)
{
    Fixture fixture;
    fixture.load("oblivion.esm", {}, {effect()});
    EXPECT_TRUE(fixture.supported());
    fixture.load("patch.esp", {"oblivion.esm"}, {effect(0x1887, 0x01000d73, 64)});
    EXPECT_FALSE(fixture.supported());
    fixture.load("final.esp", {"oblivion.esm", "patch.esp"}, {effect()});
    EXPECT_TRUE(fixture.supported());
    fixture.load("other.esm", {}, {effect(0x800)});
    EXPECT_FALSE(fixture.supported()); // Different winning keys with PARA code are ambiguous.
}

TEST(OblivionCombatMagicStore, OtherDefaultFormTypeUsesOriginalSpellItemCastFallback)
{
    Fixture fixture;
    const auto gmst = record(ESM4::REC_GMST, 0x137,
        sub(ESM::fourCC("EDID"), std::string("iTest\0", 6)) + sub(ESM::fourCC("DATA"), bytes(1u)));
    fixture.load("oblivion.esm", {}, {effect(), gmst});
    EXPECT_TRUE(fixture.supported());
}
