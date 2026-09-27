#include <components/esm4/localluascripts.hpp>
#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>

#include <gtest/gtest.h>

#include <functional>
#include <limits>
#include <memory>
#include <sstream>

namespace
{
    using State = ESM4::LocalLuaScripts;

    std::string record(const std::function<void(ESM::ESMWriter&)>& write)
    {
        std::ostringstream output;
        ESM::ESMWriter writer;
        writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion);
        writer.save(output);
        writer.startRecord(ESM::REC_LUAM);
        write(writer);
        writer.endRecord(ESM::REC_LUAM);
        writer.close();
        return output.str();
    }

    std::string encode(const State& data)
    {
        return record([&](auto& writer) { ESM4::saveLocalLuaScripts(writer, data); });
    }

    State decode(const std::string& data)
    {
        ESM::ESMReader reader;
        reader.open(std::make_unique<std::istringstream>(data), "native-local-lua-test");
        if (reader.getRecName() != ESM::REC_LUAM)
            throw std::runtime_error("Expected LUAM");
        reader.getRecHeader();
        return ESM4::loadLocalLuaScripts(reader);
    }

    State sample()
    {
        State result;
        result[ESM::FormKey::content("Example.esm", 123)].mScripts = {
            { 4, std::string("state\0binary", 12), {
                { ESM::LuaTimer::Type::SIMULATION_TIME, -2.5, "", std::string("arg\0value", 9) },
                { ESM::LuaTimer::Type::GAME_TIME, 123.25, std::string("name\0binary", 11), "" } } },
            { 9, "", {} } };
        result[ESM::FormKey::dynamic("native", 5)] = {};
        return result;
    }

    void header(ESM::ESMWriter& writer, std::uint32_t owners = 1)
    {
        writer.writeHNT("NLSV", std::uint32_t{ 1 });
        writer.writeHNT("NLSC", owners);
    }

    TEST(ESM4LocalLuaScripts, roundTripPreservesBinaryDataTimersAndStableOwners)
    {
        const auto bytes = encode(sample());
        EXPECT_EQ(encode(decode(bytes)), bytes);
        auto state = decode(bytes);
        const auto& scripts = state.at(ESM::FormKey::content("example.esm", 123)).mScripts;
        ASSERT_EQ(scripts.size(), 2);
        EXPECT_EQ(scripts[0].mData, std::string("state\0binary", 12));
        EXPECT_EQ(scripts[0].mTimers[0].mCallbackName, "");
        EXPECT_EQ(scripts[0].mTimers[0].mCallbackArgument, std::string("arg\0value", 9));
        EXPECT_EQ(scripts[0].mTimers[1].mType, ESM::LuaTimer::Type::GAME_TIME);
        EXPECT_EQ(scripts[0].mTimers[1].mTime, 123.25);
        EXPECT_EQ(scripts[0].mTimers[1].mCallbackName, std::string("name\0binary", 11));
    }

    TEST(ESM4LocalLuaScripts, absentLegacySectionAndExplicitEmptySection)
    {
        EXPECT_TRUE(decode(record([](auto&) {})).empty());
        EXPECT_TRUE(decode(encode({})).empty());
    }

    TEST(ESM4LocalLuaScripts, rejectsUnknownVersionExcessiveOwnersAndTrailingSection)
    {
        EXPECT_THROW(decode(record([](auto& w) { w.writeHNT("NLSV", std::uint32_t{ 2 }); })), std::exception);
        EXPECT_THROW(decode(record([](auto& w) { header(w, 100'001); })), std::exception);
        EXPECT_THROW(decode(record([](auto& w) { header(w, 0); header(w, 0); })), std::exception);
    }

    TEST(ESM4LocalLuaScripts, rejectsNullPlayerNoncanonicalAndDuplicateOwners)
    {
        for (const std::string key : { "", "dynamic:player:1", "content:Example.esm:00007b" })
        {
            SCOPED_TRACE(key);
            EXPECT_THROW(decode(record([&](auto& w) {
                header(w); w.writeHNString("NLSK", key); w.writeHNT("NLSE", std::uint32_t{ 0 });
            })), std::exception);
        }
        EXPECT_THROW(decode(record([](auto& w) {
            header(w, 2);
            for (int i = 0; i < 2; ++i)
            {
                w.writeHNString("NLSK", ESM::FormKey::dynamic("native", 1).serialize());
                w.writeHNT("NLSE", std::uint32_t{ 0 });
            }
        })), std::exception);
        State state;
        state[{}] = {};
        EXPECT_THROW(encode(state), std::exception);
        state.clear(); state[ESM::FormKey::dynamic("player", 1)] = {};
        EXPECT_THROW(encode(state), std::exception);
    }

    TEST(ESM4LocalLuaScripts, rejectsNegativeAndDuplicateScriptIds)
    {
        for (int id : { -1, 4 })
        {
            EXPECT_THROW(decode(record([&](auto& w) {
                header(w); w.writeHNString("NLSK", ESM::FormKey::dynamic("native", 1).serialize());
                w.writeHNT("LUAS", std::int32_t{ 4 }); w.writeHNT("LUAS", id);
                w.writeHNT("NLSE", std::uint32_t{ 0 });
            })), std::exception);
            auto state = sample();
            state.begin()->second.mScripts[1].mScriptId = id;
            EXPECT_THROW(encode(state), std::exception);
        }
    }

    TEST(ESM4LocalLuaScripts, rejectsInvalidTimerBytesAndNonfiniteDeadlines)
    {
        for (std::uint8_t type : { 2, 255 })
            EXPECT_THROW(decode(record([&](auto& w) {
                header(w); w.writeHNString("NLSK", ESM::FormKey::dynamic("native", 1).serialize());
                w.writeHNT("LUAS", std::int32_t{ 4 });
                w.startSubRecord("LUAT"); w.writeT(type); w.writeT(1.0); w.endRecord("LUAT");
            })), std::exception);
        for (double time : { std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN() })
        {
            auto state = sample();
            state.begin()->second.mScripts[0].mTimers[0].mTime = time;
            EXPECT_THROW(encode(state), std::exception);
            EXPECT_THROW(decode(record([&](auto& w) {
                header(w); w.writeHNString("NLSK", ESM::FormKey::dynamic("native", 1).serialize());
                w.writeHNT("LUAS", std::int32_t{ 4 });
                w.startSubRecord("LUAT"); w.writeT(std::uint8_t{ 0 }); w.writeT(time); w.endRecord("LUAT");
                w.writeHNString("LUAC", "callback"); w.writeHNT("NLSE", std::uint32_t{ 0 });
            })), std::exception);
        }
    }

    TEST(ESM4LocalLuaScripts, rejectsWrongSizedTimerAndMissingOrInvalidTerminator)
    {
        EXPECT_THROW(decode(record([](auto& w) {
            header(w); w.writeHNString("NLSK", ESM::FormKey::dynamic("native", 1).serialize());
            w.writeHNT("LUAS", std::int32_t{ 4 }); w.writeHNT("LUAT", std::uint8_t{ 0 });
        })), std::exception);
        EXPECT_THROW(decode(record([](auto& w) {
            header(w); w.writeHNString("NLSK", ESM::FormKey::dynamic("native", 1).serialize());
        })), std::exception);
        EXPECT_THROW(decode(record([](auto& w) {
            header(w); w.writeHNString("NLSK", ESM::FormKey::dynamic("native", 1).serialize());
            w.writeHNT("NLSE", std::uint32_t{ 1 });
        })), std::exception);
    }

    TEST(ESM4LocalLuaScripts, rejectsEveryTruncationInsideSection)
    {
        const auto bytes = encode(sample());
        const auto start = bytes.find("NLSV");
        ASSERT_NE(start, std::string::npos);
        for (std::size_t end = start + 1; end < bytes.size(); ++end)
        {
            SCOPED_TRACE(end);
            EXPECT_THROW(decode(bytes.substr(0, end)), std::exception);
        }
    }

    TEST(ESM4LocalLuaScripts, boundsAggregateScriptsAndBinaryData)
    {
        State state;
        auto& scripts = state[ESM::FormKey::dynamic("native", 1)].mScripts;
        scripts.resize(100'001);
        EXPECT_THROW(encode(state), std::exception);
        scripts = { { 0, std::string(64 * 1024 * 1024, 'x'), {} } };
        // The owner key also consumes the aggregate budget.
        EXPECT_THROW(encode(state), std::exception);
        auto bytes = encode(sample());
        const auto sizeOffset = bytes.find("LUAD") + 4;
        ASSERT_NE(sizeOffset, std::string::npos + 4);
        const std::uint32_t excessive = 64 * 1024 * 1024 + 1;
        for (unsigned i = 0; i < 4; ++i)
            bytes[sizeOffset + i] = static_cast<char>(excessive >> (8 * i));
        EXPECT_THROW(decode(bytes), std::exception);
    }
}
