#include <components/esm/fourcc.hpp>
#include <components/esm4/common.hpp>
#include <components/esm4/loadpack.hpp>
#include <components/esm4/loadinfo.hpp>
#include <components/esm4/loadidle.hpp>
#include <components/esm4/idletree.hpp>
#include <components/esm4/dialoguevoices.hpp>
#include <components/esm4/reader.hpp>
#include <components/esm4/readerutils.hpp>
#include <components/toutf8/toutf8.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    template <class T>
    void append(std::vector<char>& output, const T& value)
    {
        const char* begin = reinterpret_cast<const char*>(&value);
        output.insert(output.end(), begin, begin + sizeof(value));
    }

    void appendSubRecord(std::vector<char>& output, std::uint32_t type, const std::vector<char>& data)
    {
        append(output, type);
        append(output, static_cast<std::uint16_t>(data.size()));
        output.insert(output.end(), data.begin(), data.end());
    }

    void appendRecord(std::vector<char>& output, std::uint32_t type, std::uint32_t flags,
        std::uint32_t id, const std::vector<char>& payload)
    {
        append(output, type);
        append(output, static_cast<std::uint32_t>(payload.size()));
        append(output, flags);
        append(output, id);
        append(output, std::uint32_t{ 0 });
        output.insert(output.end(), payload.begin(), payload.end());
    }

    template <class T>
    std::vector<char> bytes(const T& value)
    {
        const char* begin = reinterpret_cast<const char*>(&value);
        return { begin, begin + sizeof(value) };
    }

    std::vector<char> makeHeader()
    {
        std::vector<char> payload;
        std::vector<char> hedr;
        append(hedr, 1.0f);
        append(hedr, std::int32_t{ 1 });
        append(hedr, std::uint32_t{ 0x800 });
        appendSubRecord(payload, ESM::fourCC("HEDR"), hedr);

        std::vector<char> result;
        appendRecord(result, ESM4::REC_TES4, ESM4::Rec_ESM, 0, payload);
        return result;
    }

    std::vector<char> makePackagePayload()
    {
        std::vector<char> payload;
        appendSubRecord(payload, ESM::fourCC("EDID"), { 'M', '1', '4', 0 });

        ESM4::AIPackage::PKDT data{};
        data.flags = static_cast<std::uint32_t>(ESM4::PackageFlag::AlwaysRun) | 0x80000000u;
        data.type = static_cast<std::uint8_t>(ESM4::AIPackageType::Travel);
        appendSubRecord(payload, ESM::fourCC("PKDT"), bytes(data));

        ESM4::AIPackage::PSDT schedule{ -1, 7, 0, 22, 4 };
        appendSubRecord(payload, ESM::fourCC("PSDT"), bytes(schedule));

        ESM4::AIPackage::PLDT location{ 5, 12, 128 };
        appendSubRecord(payload, ESM::fourCC("PLDT"), bytes(location));

        // This deliberately differs from PLDT.location. It is the regression
        // that catches reading PTDT through the location discriminant.
        ESM4::AIPackage::PTDT target{ 0, 0x00001234, 96 };
        appendSubRecord(payload, ESM::fourCC("PTDT"), bytes(target));

        ESM4::AIPackage::CTDA condition{};
        condition.condition = 0x67; // OR + run-on-target + global + >=.
        condition.compValue = 3.5f;
        condition.fnIndex = 18; // GetCurrentTime
        condition.param1 = 0x00005678;
        condition.param2 = 0x00009abc;
        condition.unknown4 = 2; // explicit reference run-on
        appendSubRecord(payload, ESM::fourCC("CTDA"), bytes(condition));

        const std::vector<char> conditionBytes = bytes(condition);
        std::vector<char> shortCondition(conditionBytes.begin(), conditionBytes.end() - 4);
        appendSubRecord(payload, ESM::fourCC("CTDT"), shortCondition);
        appendSubRecord(payload, ESM::fourCC("TNAM"), { 1, 2, 3 });
        return payload;
    }

    ESM4::AIPackage loadPackage(const std::vector<char>& plugin)
    {
        auto stream = std::make_unique<std::stringstream>(
            std::string(plugin.begin(), plugin.end()), std::ios::in | std::ios::binary);
        const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::WINDOWS_1252);
        ESM4::Reader reader(std::move(stream), "memory.esm", nullptr, &encoder, true);
        EXPECT_TRUE(reader.getRecordHeader());
        EXPECT_EQ(reader.hdr().record.typeId, ESM4::REC_PACK);
        reader.getRecordData();
        ESM4::AIPackage result;
        result.load(reader);
        return result;
    }

    ESM4::DialogInfo loadInfo(const std::vector<char>& payload)
    {
        std::vector<char> plugin = makeHeader();
        appendRecord(plugin, ESM4::REC_INFO, 0, 0x4321, payload);
        auto stream = std::make_unique<std::stringstream>(
            std::string(plugin.begin(), plugin.end()), std::ios::in | std::ios::binary);
        const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::WINDOWS_1252);
        ESM4::Reader reader(std::move(stream), "memory.esm", nullptr, &encoder, true);
        EXPECT_TRUE(reader.getRecordHeader());
        reader.getRecordData();
        ESM4::DialogInfo result;
        result.load(reader);
        return result;
    }

    ESM4::IdleAnimation loadIdle(const std::vector<char>& payload)
    {
        std::vector<char> plugin = makeHeader();
        appendRecord(plugin, ESM4::REC_IDLE, 0, 0x4321, payload);
        auto stream = std::make_unique<std::stringstream>(
            std::string(plugin.begin(), plugin.end()), std::ios::in | std::ios::binary);
        const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::WINDOWS_1252);
        ESM4::Reader reader(std::move(stream), "memory.esm", nullptr, &encoder, true);
        EXPECT_TRUE(reader.getRecordHeader());
        reader.getRecordData();
        ESM4::IdleAnimation result;
        result.load(reader);
        return result;
    }

    std::vector<char> makePackagePlugin()
    {
        std::vector<char> result = makeHeader();
        appendRecord(result, ESM4::REC_PACK, 0, 0x1234, makePackagePayload());
        return result;
    }
}

TEST(ESM4IdleTree, UsesAuthoredOrderAndPrunesFalseParents)
{
    ESM4::IdleAnimation first, second, child, otherRoot;
    first.mId = ESM::FormId::fromUint32(30);
    first.mParent = ESM::FormId::fromUint32(0); // Encoded zero and unset are both roots.
    first.mPrevious = ESM::FormId::fromUint32(0);
    second.mId = ESM::FormId::fromUint32(10);
    second.mPrevious = first.mId;
    child.mId = ESM::FormId::fromUint32(20);
    child.mParent = first.mId;
    otherRoot.mId = ESM::FormId::fromUint32(40);
    const std::vector<const ESM4::IdleAnimation*> records{ &second, &first, &child, &otherRoot };
    ESM4::IdleTree tree(records);
    const auto hasFile = [&](const auto& record) { return &record != &first; };
    EXPECT_EQ(tree.select([](const auto&) { return true; }, hasFile), &child);
    std::vector<ESM::FormId> visited;
    EXPECT_EQ(tree.select([&](const auto& record) {
        visited.push_back(record.mId);
        return &record != &first;
    }, hasFile), &second);
    EXPECT_EQ(visited, (std::vector{ first.mId, second.mId }));
    EXPECT_EQ(tree.select([&](const auto& record) { return &record != &child; }, hasFile), &second);
    first.mAnimationGroup = 0x80;
    EXPECT_EQ(tree.select([&](const auto& record) { return &record != &child; }, hasFile), &first);
    EXPECT_EQ(tree.select([](const auto&) { return false; }, hasFile), nullptr);
}

TEST(ESM4IdleTree, RejectsBrokenAndCyclicLinks)
{
    ESM4::IdleAnimation a, b;
    a.mId = ESM::FormId::fromUint32(1);
    b.mId = ESM::FormId::fromUint32(2);
    std::vector<const ESM4::IdleAnimation*> records{ &a, &b };
    a.mParent = b.mId;
    b.mParent = a.mId;
    EXPECT_THROW(ESM4::IdleTree{ records }, std::runtime_error);
    a.mParent = {};
    b.mParent = {};
    a.mPrevious = b.mId;
    b.mPrevious = a.mId;
    EXPECT_THROW(ESM4::IdleTree{ records }, std::runtime_error);
    a.mPrevious = {};
    b.mParent = a.mId; // Predecessor is not a sibling.
    EXPECT_THROW(ESM4::IdleTree{ records }, std::runtime_error);
    b.mParent = ESM::FormId::fromUint32(3);
    b.mPrevious = {};
    EXPECT_THROW(ESM4::IdleTree{ records }, std::runtime_error);
    b.mParent = {};
    b.mPrevious = ESM::FormId::fromUint32(3);
    EXPECT_THROW(ESM4::IdleTree{ records }, std::runtime_error);
    records.push_back(&a);
    EXPECT_THROW(ESM4::IdleTree{ records }, std::runtime_error);
    records = { nullptr };
    EXPECT_THROW(ESM4::IdleTree{ records }, std::runtime_error);
}

TEST(ESM4IdleTree, KeepsIndependentChainsAndRejectsUnknownConditionsWithoutFallback)
{
    ESM4::IdleAnimation a, b, c, d;
    a.mId = ESM::FormId::fromUint32(1);
    b.mId = ESM::FormId::fromUint32(2);
    c.mId = ESM::FormId::fromUint32(3);
    d.mId = ESM::FormId::fromUint32(4);
    b.mPrevious = a.mId;
    d.mPrevious = c.mId;
    const std::vector<const ESM4::IdleAnimation*> records{ &a, &c, &d, &b };
    const ESM4::IdleTree tree(records);
    std::vector<ESM::FormId> visited;
    EXPECT_EQ(tree.select([&](const auto& record) {
        visited.push_back(record.mId);
        return false;
    }, [](const auto&) { return true; }), nullptr);
    EXPECT_EQ(visited, (std::vector{ a.mId, b.mId, c.mId, d.mId }));
    EXPECT_THROW(tree.select([](const auto&) -> bool {
        throw std::runtime_error("Missing condition context");
    }, [](const auto&) { return true; }), std::runtime_error);
}

TEST(ESM4IdleTree, TraversesDeepParentChainsWithoutRecursiveSelection)
{
    std::vector<ESM4::IdleAnimation> storage(1024);
    std::vector<const ESM4::IdleAnimation*> records;
    for (std::size_t i = 0; i < storage.size(); ++i)
    {
        storage[i].mId = ESM::FormId::fromUint32(static_cast<std::uint32_t>(i + 1));
        storage[i].mParent = ESM::FormId::fromUint32(static_cast<std::uint32_t>(i));
        records.push_back(&storage[i]);
    }
    const ESM4::IdleTree tree(records);
    EXPECT_EQ(tree.select([](const auto&) { return true; }, [](const auto&) { return true; }), &storage.back());
}

TEST(ESM4LoadIdle, PreservesNativeHierarchyFlagsAndOrderedConditions)
{
    std::vector<char> payload;
    appendSubRecord(payload, ESM::fourCC("ANAM"), { static_cast<char>(0x84) });
    auto links = bytes(std::uint32_t{ 0x1234 });
    append(links, std::uint32_t{ 0x2345 });
    appendSubRecord(payload, ESM::fourCC("DATA"), links);
    ESM4::AIPackage::CTDA condition{};
    condition.fnIndex = 79;
    condition.param1 = 0x3456;
    condition.param2 = 11;
    condition.compValue = 10.f;
    appendSubRecord(payload, ESM::fourCC("CTDA"), bytes(condition));
    condition.fnIndex = 72;
    auto legacy = bytes(condition);
    legacy.resize(20);
    appendSubRecord(payload, ESM::fourCC("CTDT"), legacy);
    const auto idle = loadIdle(payload);
    EXPECT_EQ(idle.mAnimationGroup, 0x84);
    EXPECT_EQ(idle.mParent, ESM::FormId::fromUint32(0x1234));
    EXPECT_EQ(idle.mPrevious, ESM::FormId::fromUint32(0x2345));
    ASSERT_EQ(idle.mConditions.size(), 2u);
    EXPECT_EQ(idle.mConditions[0].mFunction, 79);
    EXPECT_EQ(idle.mConditions[0].mParameter1.mReferenceKey, ESM::FormKey::content("memory.esm", 0x3456));
    EXPECT_EQ(idle.mConditions[0].mParameter2.mNumber, 11);
    EXPECT_FLOAT_EQ(idle.mConditions[0].mComparisonValue, 10.f);
    EXPECT_EQ(idle.mConditions[1].mEncodedSize, 20);
}

TEST(ESM4LoadIdle, RejectsMalformedNativeLayouts)
{
    for (const auto& [tag, size] : std::vector<std::pair<std::uint32_t, std::size_t>>{
             { ESM::fourCC("ANAM"), 0 }, { ESM::fourCC("ANAM"), 8 }, { ESM::fourCC("DATA"), 7 },
             { ESM::fourCC("DATA"), 9 }, { ESM::fourCC("CTDA"), 20 }, { ESM::fourCC("CTDT"), 24 } })
    {
        std::vector<char> payload;
        appendSubRecord(payload, tag, std::vector<char>(size));
        EXPECT_THROW(loadIdle(payload), std::runtime_error);
    }
}

TEST(ESM4LoadInfo, RetainsNativeConditionsResponsesFlagsAndPredecessor)
{
    std::vector<char> payload;
    appendSubRecord(payload, ESM::fourCC("DATA"), { 1, 2, ESM4::INFO_RunImmediately });
    appendSubRecord(payload, ESM::fourCC("PNAM"), bytes(std::uint32_t{ 0x1234 }));
    ESM4::AIPackage::CTDA condition{};
    condition.fnIndex = 72; // GetIsID, with a stable base-record parameter.
    condition.param1 = 0x3456;
    condition.compValue = 1.f;
    appendSubRecord(payload, ESM::fourCC("CTDA"), bytes(condition));
    condition.fnIndex = 58; // GetStage must not overwrite GetIsID.
    condition.compValue = 6.f;
    auto shortCondition = bytes(condition);
    shortCondition.resize(20);
    appendSubRecord(payload, ESM::fourCC("CTDT"), shortCondition);
    for (int number = 1; number <= 2; ++number)
    {
        ESM4::TargetResponseData response{};
        response.responseNo = number;
        auto data = bytes(response);
        data.resize(16);
        appendSubRecord(payload, ESM::fourCC("TRDT"), data);
        appendSubRecord(payload, ESM::fourCC("NAM1"), { static_cast<char>('0' + number), 0 });
        if (number == 1)
            appendSubRecord(payload, ESM::fourCC("NAM2"), { 'n', 0 });
    }
    const auto info = loadInfo(payload);
    ASSERT_EQ(info.mCanonicalConditions.size(), 2u);
    EXPECT_EQ(info.mCanonicalConditions[0].mFunction, 72);
    EXPECT_EQ(info.mCanonicalConditions[0].mParameter1.mReferenceKey, ESM::FormKey::content("memory.esm", 0x3456));
    EXPECT_EQ(info.mCanonicalConditions[1].mFunction, 58);
    EXPECT_FLOAT_EQ(info.mCanonicalConditions[1].mComparisonValue, 6.f);
    ASSERT_EQ(info.mResponses.size(), 2u);
    EXPECT_EQ(info.mResponses[0].mData.responseNo, 1u);
    EXPECT_EQ(info.mResponses[0].mText, "1");
    EXPECT_EQ(info.mResponses[0].mNotes, "n");
    EXPECT_EQ(info.mResponses[1].mText, "2");
    EXPECT_TRUE(info.mResponses[1].mNotes.empty());
    EXPECT_EQ(info.mResponse, "2");
    EXPECT_EQ(info.mInfoFlags, ESM4::INFO_RunImmediately);
    EXPECT_EQ(info.mDialType, 1);
    EXPECT_EQ(info.mNextSpeaker, 2);
    EXPECT_EQ(info.mPreviousInfo, ESM::FormId::fromUint32(0x1234));
}

TEST(ESM4LoadInfo, ResolvesAllVoiceResponsesWithoutRaceSexOrNumberFallback)
{
    ESM4::DialogInfo info;
    info.mResponses.resize(2);
    info.mResponses[0].mData.responseNo = 0x01c0ed01; // Nonzero native padding.
    info.mResponses[1].mData.responseNo = 2;
    const std::vector<std::string> files{ "voice/imperial/m/line_2.mp3", "voice/imperial/m/line_1.mp3",
        "voice/breton/f/line_1.mp3", "voice/imperial/f/line_1.mp3" };
    const auto selected = ESM4::dialogueVoiceFiles(info, files, "imperial", "m");
    ASSERT_TRUE(selected);
    EXPECT_EQ(*selected, (std::vector<std::string>{ files[1], files[0] }));
    EXPECT_FALSE(ESM4::dialogueVoiceFiles(info, files, "imperial", "f"));
    EXPECT_FALSE(ESM4::dialogueVoiceFiles(info, files, "breton", "m"));
    EXPECT_FALSE(ESM4::dialogueVoiceFiles(info, files, "", "m"));
    info.mResponses[1].mData.responseNo = 3;
    EXPECT_FALSE(ESM4::dialogueVoiceFiles(info, files, "imperial", "m"));
}

TEST(ESM4LoadInfo, ResolvesSexSpecificVoiceRaceAndRejectsBrokenOrCyclicLinks)
{
    ESM4::Race source{}, male{}, female{};
    source.mId = ESM::FormId::fromUint32(1);
    male.mId = ESM::FormId::fromUint32(2);
    female.mId = ESM::FormId::fromUint32(3);
    source.mVNAM = { male.mId, female.mId };
    const auto resolve = [&](ESM::FormId id) -> const ESM4::Race* {
        for (const auto* race : { &source, &male, &female })
            if (race->mId == id)
                return race;
        return nullptr;
    };
    EXPECT_EQ(ESM4::dialogueVoiceRace(&source, false, resolve), &male);
    EXPECT_EQ(ESM4::dialogueVoiceRace(&source, true, resolve), &female);
    male.mVNAM[0] = ESM::FormId::fromUint32(0);
    EXPECT_EQ(ESM4::dialogueVoiceRace(&source, false, resolve), &male);
    male.mVNAM[0] = female.mId;
    EXPECT_EQ(ESM4::dialogueVoiceRace(&source, false, resolve), &female);
    female.mVNAM[0] = source.mId;
    EXPECT_EQ(ESM4::dialogueVoiceRace(&source, false, resolve), nullptr);
    source.mVNAM[1] = ESM::FormId::fromUint32(4);
    EXPECT_EQ(ESM4::dialogueVoiceRace(&source, true, resolve), nullptr);
}

TEST(ESM4LoadInfo, RejectsMalformedLegacyCondition)
{
    std::vector<char> payload;
    appendSubRecord(payload, ESM::fourCC("CTDT"), std::vector<char>(19));
    EXPECT_THROW(loadInfo(payload), std::runtime_error);
}

TEST(ESM4LoadInfo, RejectsWrongNativeResponseConditionAndFlagLayouts)
{
    for (const auto type : { ESM::fourCC("TRDT"), ESM::fourCC("CTDA"), ESM::fourCC("DATA") })
    {
        std::vector<char> payload;
        appendSubRecord(payload, type, std::vector<char>(19));
        EXPECT_THROW(loadInfo(payload), std::runtime_error);
    }
}

TEST(ESM4LoadInfo, ReadsLegacyTwoByteDataWithAbsentFlags)
{
    std::vector<char> payload;
    appendSubRecord(payload, ESM::fourCC("DATA"), { 1, 2 });
    const auto info = loadInfo(payload);
    EXPECT_EQ(info.mDialType, 1);
    EXPECT_EQ(info.mNextSpeaker, 2);
    EXPECT_EQ(info.mInfoFlags, 0);
}

TEST(ESM4LoadPackage, DecodesAllNativeFieldsAndBothConditionLayouts)
{
    const ESM4::AIPackage package = loadPackage(makePackagePlugin());
    EXPECT_EQ(package.mFormKey, ESM::FormKey::content("memory.esm", 0x1234));
    EXPECT_EQ(package.mEditorId, "M14");
    EXPECT_EQ(package.mPackageType, ESM4::AIPackageType::Travel);
    EXPECT_TRUE(package.mPackageFlags.has(ESM4::PackageFlag::AlwaysRun));
    EXPECT_EQ(package.mPackageFlags.mReserved, 0x80000000u);
    EXPECT_EQ(package.mScheduleData.mStartHour, 22);
    EXPECT_EQ(package.mScheduleData.mDuration, 4);
    EXPECT_EQ(package.mLocationData.mKind, ESM4::PackageLocationKind::ObjectType);
    EXPECT_EQ(package.mLocationData.mObjectType, 12u);
    EXPECT_EQ(package.mTargetData.mKind, ESM4::PackageTargetKind::SpecificReference);
    EXPECT_EQ(package.mTargetData.mReferenceKey, ESM::FormKey::content("memory.esm", 0x1234));
    ASSERT_EQ(package.mCanonicalConditions.size(), 2u);
    EXPECT_EQ(package.mCanonicalConditions[0].mEncodedSize, 24u);
    EXPECT_EQ(package.mCanonicalConditions[0].mRunOn, ESM4::ConditionRunOn::Reference);
    EXPECT_EQ(package.mCanonicalConditions[1].mEncodedSize, 20u);
    EXPECT_EQ(package.mCanonicalConditions[1].mRunOn, ESM4::ConditionRunOn::Subject);
    ASSERT_EQ(package.mSkippedSubrecords.size(), 1u);
    EXPECT_EQ(package.mSkippedSubrecords.front(), ESM::fourCC("TNAM"));
}

TEST(ESM4LoadPackage, DecodesLegacyShortPackageData)
{
    std::vector<char> payload;
    ESM4::AIPackage::PKDTShort data{ 0x0040, static_cast<std::uint8_t>(ESM4::AIPackageType::Find), 0 };
    appendSubRecord(payload, ESM::fourCC("PKDT"), bytes(data));
    ESM4::AIPackage::PSDT schedule{ -1, -1, 0, -1, 0 };
    appendSubRecord(payload, ESM::fourCC("PSDT"), bytes(schedule));

    std::vector<char> plugin = makeHeader();
    appendRecord(plugin, ESM4::REC_PACK, 0, 0x1235, payload);
    const ESM4::AIPackage package = loadPackage(plugin);
    EXPECT_TRUE(package.mUsedShortPKDT);
    EXPECT_EQ(package.mPackageType, ESM4::AIPackageType::Find);
    EXPECT_TRUE(package.mPackageFlags.has(ESM4::PackageFlag::UnlockDoorsAtStart));
}

TEST(ESM4LoadPackage, RejectsMalformedLayoutsAndDiscriminants)
{
    std::vector<char> plugin = makePackagePlugin();
    // PKDT's subrecord header starts after the 20-byte TES4 record and EDID.
    const std::size_t pkdtSize = 20 + 20 + 6 + 5 + 6;
    plugin[pkdtSize + 4] = 7;
    EXPECT_THROW(loadPackage(plugin), std::runtime_error);

    std::vector<char> invalid = makeHeader();
    std::vector<char> payload;
    ESM4::AIPackage::PKDT data{};
    data.type = static_cast<std::uint8_t>(ESM4::AIPackageType::Find);
    appendSubRecord(payload, ESM::fourCC("PKDT"), bytes(data));
    appendSubRecord(payload, ESM::fourCC("PSDT"), { 0, 0, 0, 0, 0, 0, 0 });
    appendRecord(invalid, ESM4::REC_PACK, 0, 0x1236, payload);
    EXPECT_THROW(loadPackage(invalid), std::runtime_error);
}
