#include <components/esm4/loadrefr.hpp>
#include <components/esm4/runtimestate.hpp>

#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <memory>
#include <sstream>

namespace
{
    TEST(ESM4RuntimeState, nativeReferenceDefaultsToUnlocked)
    {
        ESM4::Reference reference;
        EXPECT_FALSE(reference.mIsLocked);
        EXPECT_EQ(reference.mLockLevel, 0);
    }

    ESM4::RuntimeState makeState()
    {
        ESM4::RuntimeState state;
        state.mNextDynamicSerial = 73;
        state.mContent = { { "oblivion.esm", "sha256:base" }, { "knights.esp", "sha256:knights" } };
        state.mClock = { 3, 8, 17, 13.5, 30.0 };
        state.mPlayer.mReference = ESM::FormKey::dynamic("save-1", 1);
        state.mPlayer.mCell = ESM::FormKey::content("Oblivion.esm", 0x1650f);
        state.mPlayer.mPosition.pos[0] = 12.5f;
        state.mPlayer.mPosition.pos[1] = -3.f;
        state.mPlayer.mPosition.rot[2] = 1.25f;
        state.mPlayer.mActorValues = { { "health", 42.25 }, { "magicka", 31.0 } };
        state.mPlayer.mInventory = { { ESM::FormKey::content("Oblivion.esm", 0x18baa), 2 } };
        state.mPlayer.mName = "Bendu Olo";
        state.mPlayer.mRace = ESM::FormKey::content("Oblivion.esm", 0x907);
        state.mPlayer.mClass = ESM::FormKey::content("Oblivion.esm", 0x237a8);
        state.mPlayer.mBirthSign = ESM::FormKey::content("Oblivion.esm", 0x22a37);
        state.mPlayer.mCharacterGenerationFlags = 15;
        state.mGlobals.emplace(ESM::FormKey::content("Oblivion.esm", 0x33), std::int64_t(9));
        state.mGlobals.emplace(ESM::FormKey::content("Oblivion.esm", 0x34), 1.5);

        ESM4::RuntimeReferenceState reference;
        reference.mKey = ESM::FormKey::content("Oblivion.esm", 0x100);
        reference.mBase = ESM::FormKey::content("Oblivion.esm", 0x200);
        reference.mCell = state.mPlayer.mCell;
        reference.mEnabled = false;
        reference.mPosition.pos[2] = 64.f;
        reference.mOwner = ESM::FormKey::content("Oblivion.esm", 0x300);
        reference.mLockLevel = 40;
        reference.mInventory = { { ESM::FormKey::content("Oblivion.esm", 0x400), -1 } };
        reference.mCustomState = { { "harvested", true }, { "label", std::string("opened") } };
        state.mReferences.push_back(std::move(reference));
        state.mScriptEventSequence = 91;
        ESM4::RuntimeScriptInstance script;
        script.mUnit = "content:oblivion.esm:04e90e";
        script.mContext = ESM::FormKey::content("Oblivion.esm", 0x1fc41);
        script.mOnLoadFired = true;
        script.mLocals = { std::int64_t(7), 2.5, std::string("named"),
            ESM::FormKey::content("Oblivion.esm", 0x2466e), std::monostate{} };
        state.mScriptInstances.push_back(std::move(script));
        state.mQuests.push_back({ ESM::FormKey::content("Oblivion.esm", 0x32a15), 19, true, { 10, 19 } });
        return state;
    }

    TEST(ESM4RuntimeState, binaryRoundTripPreservesEveryStateFamily)
    {
        const ESM4::RuntimeState expected = makeState();
        const auto bytes = expected.serializeBinary();
        const ESM4::RuntimeState actual = ESM4::RuntimeState::deserializeBinary(bytes);
        EXPECT_EQ(actual, expected);
        EXPECT_EQ(actual.serializeBinary(), bytes);
        EXPECT_EQ(actual.canonicalJson(), expected.canonicalJson());
    }

    TEST(ESM4RuntimeState, openMwSaveRecordSupportsChunkedPayloads)
    {
        ESM4::RuntimeState expected = makeState();
        expected.mReferences[0].mCustomState["large"] = std::string(70'000, 'x');

        auto output = std::make_unique<std::stringstream>();
        ESM::ESMWriter writer;
        writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion);
        writer.save(*output);
        writer.startRecord(ESM4::RuntimeState::sRecordId);
        expected.save(writer);
        writer.endRecord(ESM4::RuntimeState::sRecordId);
        writer.close();

        std::unique_ptr<std::istream> input = std::move(output);
        ESM::ESMReader reader;
        reader.open(std::move(input), "runtime-state-stream");
        ASSERT_TRUE(reader.hasMoreRecs());
        EXPECT_EQ(reader.getRecName(), ESM4::RuntimeState::sRecordId);
        reader.getRecHeader();
        ESM4::RuntimeState actual;
        actual.load(reader);
        EXPECT_EQ(actual, expected);
    }

    TEST(ESM4RuntimeState, formKeysAndDiagnosticsSurvivePluginReordering)
    {
        const ESM4::RuntimeState state = makeState();
        ESM::FormKeyResolver oldOrder({ "Oblivion.esm", "Knights.esp" });
        ESM::FormKeyResolver newOrder({ "Knights.esp", "Oblivion.esm" });
        const ESM::FormKey stable = oldOrder.toFormKey({ 0x100, 0 });
        ASSERT_EQ(newOrder.toFormId(stable), (ESM::FormId{ 0x100, 1 }));
        EXPECT_TRUE(state.getMissingContentFiles({ "KNIGHTS.ESP", "OBLIVION.ESM" }).empty());
        EXPECT_EQ(state.getMissingContentFiles({ "Oblivion.esm" }), (std::vector<std::string>{ "knights.esp" }));
        EXPECT_NO_THROW(state.validateContent(
            { { "KNIGHTS.ESP", "sha256:knights" }, { "OBLIVION.ESM", "sha256:base" } }));
        EXPECT_THROW(state.validateContent({ { "Oblivion.esm", "sha256:base" } }), std::runtime_error);
        try
        {
            state.validateContent(
                { { "Oblivion.esm", "sha256:different" }, { "Knights.esp", "sha256:knights" } });
            FAIL() << "Fingerprint mismatch was accepted";
        }
        catch (const std::runtime_error& error)
        {
            EXPECT_NE(std::string(error.what()).find("fingerprint mismatch for oblivion.esm"), std::string::npos);
        }
    }

    TEST(ESM4RuntimeState, rejectsTruncationCorruptionAndTrailingData)
    {
        const auto valid = makeState().serializeBinary();
        auto truncated = valid;
        truncated.pop_back();
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(truncated), std::runtime_error);

        auto badMagic = valid;
        badMagic[0] ^= 0xff;
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(badMagic), std::runtime_error);

        auto trailing = valid;
        trailing.push_back(0);
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(trailing), std::runtime_error);
    }

    TEST(ESM4RuntimeState, rejectsWrongProfileSchemaAndDynamicSerial)
    {
        auto state = makeState();
        state.mProfile = ESM::GameProfile::Morrowind;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        state.mProfile = ESM::GameProfile::Oblivion;
        state.mVersion = ESM4::CurrentRuntimeStateVersion + 1;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        state.mVersion = ESM4::CurrentRuntimeStateVersion;
        state.mNextDynamicSerial = 0;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        state = makeState();
        state.mPlayer.mCell = {};
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        state = makeState();
        state.mPlayer.mCharacterGenerationFlags = 0x20;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    }

    TEST(ESM4RuntimeState, rejectsDuplicateReferencesAndInvalidInventory)
    {
        auto state = makeState();
        state.mReferences.push_back(state.mReferences.front());
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        state.mReferences.pop_back();
        state.mReferences.front().mInventory.front().mCount = 0;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    }

    TEST(ESM4RuntimeState, rejectsNonFiniteState)
    {
        auto state = makeState();
        state.mPlayer.mPosition.pos[0] = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        state = makeState();
        state.mGlobals.begin()->second = std::numeric_limits<double>::infinity();
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    }

    TEST(ESM4RuntimeState, canonicalJsonIsStableAndContainsStableKeys)
    {
        const std::string json = makeState().canonicalJson();
        EXPECT_NE(json.find("\"schema_version\":3"), std::string::npos);
        EXPECT_NE(json.find("content:oblivion.esm:01650f"), std::string::npos);
        EXPECT_NE(json.find("dynamic:save-1:0000000000000001"), std::string::npos);
        EXPECT_NE(json.find("\"inventory\":[{\"base\":\"content:oblivion.esm:018baa\",\"count\":2}]"),
            std::string::npos);
        EXPECT_NE(json.find("\"script_event_sequence\":91"), std::string::npos);
        EXPECT_NE(json.find("\"stage\":19"), std::string::npos);
        EXPECT_EQ(json, makeState().canonicalJson());
    }

    TEST(ESM4RuntimeState, versionOnePayloadMigratesWithEmptyScriptState)
    {
        auto state = makeState();
        state.mVersion = 1;
        state.mScriptEventSequence = 0;
        state.mScriptInstances.clear();
        state.mQuests.clear();
        state.mPlayer.mName.clear();
        state.mPlayer.mRace = {};
        state.mPlayer.mClass = {};
        state.mPlayer.mBirthSign = {};
        state.mPlayer.mCharacterGenerationFlags = 0;
        const auto bytes = state.serializeBinary();
        const ESM4::RuntimeState loaded = ESM4::RuntimeState::deserializeBinary(bytes);
        EXPECT_EQ(loaded.mVersion, 1u);
        EXPECT_TRUE(loaded.mScriptInstances.empty());
        EXPECT_TRUE(loaded.mQuests.empty());
    }

    TEST(ESM4RuntimeState, versionTwoPayloadMigratesWithoutCharacterGenerationState)
    {
        auto state = makeState();
        state.mVersion = 2;
        state.mPlayer.mName.clear();
        state.mPlayer.mRace = {};
        state.mPlayer.mClass = {};
        state.mPlayer.mBirthSign = {};
        state.mPlayer.mCharacterGenerationFlags = 0;
        const ESM4::RuntimeState loaded = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
        EXPECT_EQ(loaded.mVersion, 2u);
        EXPECT_TRUE(loaded.mPlayer.mRace.isNull());
        EXPECT_EQ(loaded.canonicalJson().find("character_generation_flags"), std::string::npos);
    }
}
