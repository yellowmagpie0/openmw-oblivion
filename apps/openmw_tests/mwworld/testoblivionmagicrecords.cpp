#include <gtest/gtest.h>

#include <components/esm4/common.hpp>
#include <components/esm4/reader.hpp>
#include <components/loadinglistener/loadinglistener.hpp>

#include "apps/openmw/mwworld/esmstore.hpp"

namespace
{
    Loading::Listener listener;
}

TEST(OblivionMagicRecordStore, EffectDefinitionsKeepAuthoredPrefixAndWinningKeysAcrossMasterReorderingAndDeletion)
{
    const auto bytes = [](const auto& value) {
        return std::string(reinterpret_cast<const char*>(&value), sizeof(value));
    };
    const auto sub = [&](std::uint32_t tag, const std::string& data) {
        return bytes(tag) + bytes(static_cast<std::uint16_t>(data.size())) + data;
    };
    const auto record = [&](std::uint32_t tag, std::uint32_t id, std::uint32_t flags, const std::string& data) {
        return bytes(tag) + bytes(static_cast<std::uint32_t>(data.size())) + bytes(flags)
            + bytes(id) + bytes(std::uint32_t{}) + data;
    };
    MWWorld::ESMStore store;
    const std::map<std::string,int> indices{{"base.esm",0},{"other.esm",1},{"patch.esp",2}};
    const auto load = [&](const std::string& name, const std::vector<std::string>& masters,
                          std::uint32_t form, std::uint32_t flags, std::uint32_t data, bool deleted) {
        auto header=sub(ESM::fourCC("HEDR"),bytes(1.f)+bytes(std::uint32_t{1})+bytes(std::uint32_t{0x900}));
        for (const auto& master : masters)
            header+=sub(ESM::fourCC("MAST"),master+'\0')+sub(ESM::fourCC("DATA"),std::string(8,'\0'));
        const auto payload=deleted ? std::string{} : sub(ESM::fourCC("EDID"),std::string("FOSP\0",5))
            +sub(ESM::fourCC("DATA"),bytes(flags)+bytes(1.f)+bytes(data)+bytes(std::uint32_t{5})
                +bytes(std::uint32_t{0xffffffff})+std::string(4,'\0'));
        auto stream=std::make_unique<std::stringstream>(record(ESM4::REC_TES4,0,1,header)
            +record(ESM4::REC_MGEF,form,deleted ? static_cast<std::uint32_t>(ESM4::Rec_Deleted) : 0u,payload),
            std::ios::in | std::ios::binary);
        ESM4::Reader reader(std::move(stream),name,nullptr,nullptr,true);
        reader.setModIndex(indices.at(name));reader.updateModIndices(indices);
        store.loadESM4(reader,&listener);
    };
    load("base.esm",{},0x800,1u<<24,9,false);
    load("other.esm",{},0x800,1u<<24,64,false);
    const auto base=ESM::FormKey::content("base.esm",0x800);
    const auto other=ESM::FormKey::content("other.esm",0x800);
    ASSERT_NE(store.search<ESM4::EffectSetting>(base),nullptr);
    ASSERT_NE(store.search<ESM4::EffectSetting>(other),nullptr);
    EXPECT_EQ(store.search<ESM4::EffectSetting>(base)->mData->mAssociatedData,9);
    EXPECT_FALSE(store.search<ESM4::EffectSetting>(base)->mData->mAssociatedForm);
    load("patch.esp",{"other.esm","base.esm"},0x01000800,1u<<16,0x00000812,false);
    const auto* effect=store.search<ESM4::EffectSetting>(base);
    ASSERT_NE(effect,nullptr);ASSERT_TRUE(effect->mData);
    EXPECT_EQ(effect->mFormKey,base);EXPECT_EQ(effect->mEffectCode,ESM::fourCC("FOSP"));
    EXPECT_EQ(effect->mData->mFlags,1u<<16); // Authored, not merged ready-game flags.
    EXPECT_EQ(effect->mData->mAssociatedData,0x00000812);
    EXPECT_EQ(effect->mData->mAssociatedForm,ESM::FormKey::content("other.esm",0x812));
    EXPECT_EQ(store.search<ESM4::EffectSetting>(other)->mData->mAssociatedData,64);
    load("patch.esp",{"other.esm","base.esm"},0x01000800,0,0,true);
    EXPECT_EQ(store.search<ESM4::EffectSetting>(base),nullptr);
    EXPECT_NE(store.search<ESM4::EffectSetting>(other),nullptr);
}
