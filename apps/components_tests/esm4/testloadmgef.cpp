#include <components/esm4/loadspel.hpp>
#include <components/esm4/loadmgef.hpp>
#include <components/esm4/loadrace.hpp>
#include <components/esm4/common.hpp>
#include <components/esm4/reader.hpp>
#include <components/toutf8/toutf8.hpp>

#include <gtest/gtest.h>
#include <bit>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>
#include <sstream>
#include <zlib.h>

namespace
{
    void put(std::vector<std::uint8_t>& data, std::size_t offset, std::uint32_t value)
    {
        for (unsigned i = 0; i < 4; ++i)
            data.at(offset + i) = static_cast<std::uint8_t>(value >> (i * 8));
    }
    template <typename T> void append(std::vector<std::uint8_t>& data, T value)
    {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
        data.insert(data.end(), bytes, bytes + sizeof(T));
    }
    void sub(std::vector<std::uint8_t>& data, const char* type, const std::vector<std::uint8_t>& payload)
    {
        data.insert(data.end(), type, type + 4);
        append(data, static_cast<std::uint16_t>(payload.size()));
        data.insert(data.end(), payload.begin(), payload.end());
    }
    void record(std::vector<std::uint8_t>& data, const char* type, std::uint32_t flags,
        std::uint32_t id, const std::vector<std::uint8_t>& payload)
    {
        data.insert(data.end(), type, type + 4);
        append(data, static_cast<std::uint32_t>(payload.size()));
        append(data, flags);
        append(data, id);
        append(data, std::uint32_t{});
        data.insert(data.end(), payload.begin(), payload.end());
    }
    template <typename T = ESM4::EffectSetting> T load(std::vector<std::uint8_t> payload, bool compressed = false,
        std::uint32_t flags = 0, float version = 1.f, const char* type = "MGEF")
    {
        std::vector<std::uint8_t> header, hedr, data;
        append(hedr, version);
        append(hedr, std::uint32_t{ 1 });
        append(hedr, std::uint32_t{ 0x800 });
        sub(header, "HEDR", hedr);
        record(data, "TES4", ESM4::Rec_ESM, 0, header);
        if (compressed)
        {
            uLongf length = compressBound(payload.size());
            std::vector<std::uint8_t> packed(length + 4);
            put(packed, 0, payload.size());
            if (compress(packed.data() + 4, &length, payload.data(), payload.size()) != Z_OK)
                throw std::runtime_error("test compression failed");
            packed.resize(length + 4);
            payload = std::move(packed);
            flags |= ESM4::Rec_Compressed;
        }
        record(data, type, flags, 0x800, payload);
        auto stream = std::make_unique<std::stringstream>(std::string(data.begin(), data.end()),
            std::ios::in | std::ios::binary);
        const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::WINDOWS_1252);
        ESM4::Reader reader(std::move(stream), "fixture.esm", nullptr, &encoder, true);
        if (!reader.getRecordHeader())
            throw std::runtime_error("test record missing");
        reader.getRecordData();
        T result{};
        result.load(reader);
        return result;
    }
}

namespace
{
    std::vector<std::uint8_t> effectData(unsigned size = 64)
    {
        std::vector<std::uint8_t> result(size, 0xa5);
        put(result, 0, 0x1000072); put(result, 4, std::bit_cast<std::uint32_t>(2.5f));
        put(result, 8, 9); put(result, 12, 5); put(result, 16, 0xffffffff);
        result[20] = 7; result[21] = 0; result[22] = 0xff; result[23] = 0x80;
        // Tail pointer words must be valid raw FormIds for graph tracking.
        for (unsigned offset : {24,32,36,40,44,48,52})
            if (offset+4 <= size) put(result,offset,0);
        return result;
    }
    std::vector<std::uint8_t> effectRecord(unsigned size = 64)
    {
        std::vector<std::uint8_t> result;
        sub(result,"EDID",{'F','O','S','P',0}); sub(result,"DATA",effectData(size));
        sub(result,"ZZZZ",{1,2,3});
        return result;
    }
}

TEST(ESM4EffectSetting, ReadsCommonPrefixAcrossShortExtendedAndCompressedRecordsWithoutInventingTailValues)
{
    for (unsigned size = 24; size <= 68; size += 4)
        for (bool compressed : {false,true})
        {
            const auto record=load(effectRecord(size),compressed);
            EXPECT_EQ(record.mEffectCode,ESM::fourCC("FOSP"));
            ASSERT_TRUE(record.mData);
            EXPECT_EQ(record.mData->mFlags,0x1000072);
            EXPECT_EQ(record.mData->mBaseCost,2.5f);
            EXPECT_EQ(record.mData->mAssociatedData,9);
            EXPECT_EQ(record.mData->mSchool,5);
            EXPECT_EQ(record.mData->mResistanceActorValue,-1);
            EXPECT_EQ(record.mData->mCounterCount,7);
            EXPECT_EQ(record.mData->mCounterPadding,0x80ff);
            EXPECT_FALSE(record.mData->mAssociatedForm);
            ASSERT_EQ(record.mSubRecords.size(),3);
            EXPECT_EQ(record.mSubRecords[1].mData,effectData(size));
            EXPECT_EQ(record.mSubRecords[2].mData,(std::vector<std::uint8_t>{1,2,3}));
        }
}

TEST(ESM4EffectSetting, ResolvesOnlyAuthoredItemDataReferencesAndKeepsActorValueWordsNumeric)
{
    for (unsigned flag : {0u,1u<<24,1u<<16,1u<<17,1u<<18})
    {
        auto data=effectData();put(data,0,flag);put(data,8,0x812);
        std::vector<std::uint8_t> payload;sub(payload,"EDID",{'F','O','S','P'});sub(payload,"DATA",data);
        const auto record=load(payload);
        ASSERT_TRUE(record.mData);
        EXPECT_EQ(record.mEffectCode,ESM::fourCC("FOSP"));
        EXPECT_EQ(record.mData->mAssociatedData,0x812);
        if (flag & ((1u<<16)|(1u<<17)|(1u<<18)))
            EXPECT_EQ(record.mData->mAssociatedForm,ESM::FormKey::content("fixture.esm",0x812));
        else EXPECT_FALSE(record.mData->mAssociatedForm);
    }
}

TEST(ESM4EffectSetting, RejectsInvalidLengthsDuplicateDataAndMissingOrMalformedMagicCodeExceptDeletion)
{
    EXPECT_THROW(load({}),std::runtime_error);
    const auto deleted=load({},false,ESM4::Rec_Deleted);
    EXPECT_FALSE(deleted.mData);EXPECT_FALSE(deleted.mEffectCode);
    for (unsigned size = 0;size <= 72;++size)
    {
        if (size>=24 && size<=68 && size%4==0) continue;
        std::vector<std::uint8_t> payload;sub(payload,"EDID",{'F','O','S','P',0});sub(payload,"DATA",std::vector<std::uint8_t>(size));
        EXPECT_THROW(load(payload),std::runtime_error)<<size;
    }
    auto payload=effectRecord();sub(payload,"DATA",effectData());EXPECT_THROW(load(payload),std::runtime_error);
    payload=effectRecord();sub(payload,"EDID",{'F','O','A','T',0});EXPECT_THROW(load(payload),std::runtime_error);
    payload.clear();sub(payload,"DATA",effectData());EXPECT_THROW(load(payload),std::runtime_error);
    payload.clear();sub(payload,"EDID",{'F','O','S','P',0});EXPECT_THROW(load(payload),std::runtime_error);
    for (const auto& name : std::vector<std::vector<std::uint8_t>>{{},{'F','O','S'},{'F',0,'S','P'},{'F','O','S','P','X'},{'F','O','S','P',0,0}})
    {
        payload.clear();sub(payload,"EDID",name);sub(payload,"DATA",effectData());EXPECT_THROW(load(payload),std::runtime_error);
    }
    EXPECT_THROW(load(effectRecord(),false,0,1.7f),std::runtime_error);
}

TEST(ESM4EffectSetting, PreservesAuthoredFlagAndFloatBitsForSeparateReadyGameValidation)
{
    auto data=effectData();put(data,0,0xffffffff);put(data,8,0);put(data,12,0xffffffff);
    put(data,4,0x7fc12345);put(data,16,0x80000000);
    std::vector<std::uint8_t> payload;sub(payload,"EDID",{'Z','Z','Z','Z',0});sub(payload,"DATA",data);
    const auto record=load(payload);
    ASSERT_TRUE(record.mData);
    EXPECT_EQ(record.mData->mFlags,0xffffffff);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(record.mData->mBaseCost),0x7fc12345);
    EXPECT_EQ(record.mData->mSchool,0xffffffff);
    EXPECT_EQ(record.mData->mResistanceActorValue,std::numeric_limits<std::int32_t>::min());
    EXPECT_EQ(record.mData->mAssociatedForm,ESM::FormKey{});
    EXPECT_EQ(record.mSubRecords[1].mData,data);
}

TEST(ESM4LoadEffectSetting, PassivePreparationPreservesAuthoredDataAndCompiledStaticActorValue)
{
    ESM4::EffectSetting setting{};
    setting.mEffectCode = ESM::fourCC("FOSP");
    setting.mData = ESM4::EffectSettingData{};
    setting.mData->mFlags = 0xffffffff;
    setting.mData->mAssociatedData = 123;
    setting.preparePassiveValueModifierDefinition();
    ASSERT_TRUE(setting.mPassiveValueModifierDefinition);
    EXPECT_EQ(setting.mPassiveValueModifierDefinition->mFlags, 0x0fc03c72u);
    EXPECT_EQ(setting.mPassiveValueModifierDefinition->mData, 9);
    EXPECT_EQ(setting.mData->mFlags, 0xffffffff);
    EXPECT_EQ(setting.mData->mAssociatedData, 123);
    auto unknown = setting;
    unknown.mEffectCode = ESM::fourCC("ZZZZ");
    unknown.preparePassiveValueModifierDefinition();
    EXPECT_FALSE(unknown.mPassiveValueModifierDefinition);
    setting.mFlags = ESM4::Rec_Deleted;
    setting.preparePassiveValueModifierDefinition();
    EXPECT_FALSE(setting.mPassiveValueModifierDefinition);
}

TEST(ESM4LoadEffectSetting, PassivePreparationUsesPredecessorHistoryForStickyStaticActorValue)
{
    ESM4::EffectSetting first{};
    first.mEffectCode = ESM::fourCC("FOAT");
    first.mData = ESM4::EffectSettingData{};
    first.mData->mFlags = 1u << 24;
    first.mData->mAssociatedData = 40;
    first.preparePassiveValueModifierDefinition();
    ASSERT_TRUE(first.mPassiveValueModifierDefinition);
    EXPECT_EQ(first.mPassiveValueModifierDefinition->mData, 40);
    ESM4::EffectSetting second{};
    second.mEffectCode = first.mEffectCode;
    second.mData = ESM4::EffectSettingData{};
    second.mData->mAssociatedData = 55;
    second.preparePassiveValueModifierDefinition(&first);
    ASSERT_TRUE(second.mPassiveValueModifierDefinition);
    EXPECT_TRUE(second.mPassiveValueModifierDefinition->mFlags & (1u << 24));
    EXPECT_EQ(second.mPassiveValueModifierDefinition->mData, 40);
    EXPECT_EQ(second.mData->mFlags, 0);
    EXPECT_EQ(second.mData->mAssociatedData, 55);
    ESM4::SpellEffect item{};
    item.mActorValue = 5;
    EXPECT_EQ(ESM4::resolveValueModifierEffectInputs(item, *second.mPassiveValueModifierDefinition).mActorValue, 40);
    auto withoutHistory = second;
    withoutHistory.preparePassiveValueModifierDefinition();
    EXPECT_FALSE(withoutHistory.mPassiveValueModifierDefinition->mFlags & (1u << 24));
    EXPECT_EQ(ESM4::resolveValueModifierEffectInputs(item, *withoutHistory.mPassiveValueModifierDefinition).mActorValue, 5);
}

TEST(ESM4LoadEffectSetting, PassivePreparationRejectsUnpreparedSameCodeHistoryAndResetsOnDifferentCode)
{
    ESM4::EffectSetting previous{};
    previous.mEffectCode = ESM::fourCC("FOAT");
    previous.mData = ESM4::EffectSettingData{};
    ESM4::EffectSetting current = previous;
    EXPECT_THROW(current.preparePassiveValueModifierDefinition(&previous), std::invalid_argument);
    EXPECT_FALSE(current.mPassiveValueModifierDefinition);
    current.mEffectCode = ESM::fourCC("FOSP");
    current.preparePassiveValueModifierDefinition(&previous);
    ASSERT_TRUE(current.mPassiveValueModifierDefinition);
    EXPECT_EQ(current.mPassiveValueModifierDefinition->mData, 9);
    current.mData.reset();
    current.preparePassiveValueModifierDefinition();
    EXPECT_FALSE(current.mPassiveValueModifierDefinition);
}
