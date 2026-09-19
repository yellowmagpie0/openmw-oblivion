#include <components/esm4/observation.hpp>
#include <components/esm4/runtimestate.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <iterator>
#include <limits>

namespace
{
    class ESM4Observation : public testing::Test
    {
    protected:
        std::filesystem::path mDirectory;
        std::filesystem::path mEvents;

        void SetUp() override
        {
            const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
            mDirectory = std::filesystem::temp_directory_path() / ("openmw-observation-" + std::to_string(nonce));
            ASSERT_TRUE(std::filesystem::create_directory(mDirectory));
            mEvents = mDirectory / "events.jsonl";
        }
        void TearDown() override { std::filesystem::remove_all(mDirectory); }
        static std::string read(const std::filesystem::path& path)
        {
            std::ifstream input(path, std::ios::binary);
            return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
        }
    };

    TEST_F(ESM4Observation, SequenceTicksAndClosingCountsAreLossless)
    {
        ESM4::ObservationStream stream(mEvents, "run-1", 2, 123);
        stream.advance();
        stream.record("input", { { "actor", std::string("dynamic:player:0000000000000001") },
            { "pressed", true }, { "amount", 0.5 } });
        stream.finish(1, 2, 3);
        const std::string data = read(mEvents);
        EXPECT_NE(data.find("\"sequence\":1,\"tick\":0,\"event\":\"run-start\""), std::string::npos);
        EXPECT_NE(data.find("\"sequence\":2,\"tick\":1,\"event\":\"input\""), std::string::npos);
        EXPECT_NE(data.find("\"sequence\":3,\"tick\":1,\"event\":\"run-end\""), std::string::npos);
        EXPECT_NE(data.find("\"event_count\":2"), std::string::npos);
        EXPECT_NE(data.find("\"error_count\":1"), std::string::npos);
        EXPECT_NE(data.find("\"unsupported_count\":2"), std::string::npos);
        EXPECT_NE(data.find("\"pending_count\":3"), std::string::npos);
        EXPECT_THROW(stream.record("late"), std::runtime_error);
        EXPECT_THROW(stream.finish(0, 0, 0), std::runtime_error);
        EXPECT_THROW(stream.advance(), std::runtime_error);
    }

    TEST_F(ESM4Observation, DestructionNeverInventsSuccessfulShutdown)
    {
        { ESM4::ObservationStream stream(mEvents, "run", 1, 123); }
        EXPECT_EQ(read(mEvents).find("run-end"), std::string::npos);
        EXPECT_THROW(ESM4::ObservationStream(mEvents, "new", 1, 124), std::runtime_error);
    }

    TEST_F(ESM4Observation, ObservedDiagnosticsCannotBeClearedByClosingCounts)
    {
        ESM4::ObservationStream stream(mEvents, "run", 1, 123);
        stream.diagnostic("frame failure");
        stream.diagnostic("deferred command", true);
        stream.finish(0, 0, 0);
        EXPECT_NE(read(mEvents).find("\"error_count\":1"), std::string::npos);
        EXPECT_NE(read(mEvents).find("\"unsupported_count\":1"), std::string::npos);
    }

    TEST_F(ESM4Observation, InvalidIdentityCannotCreateEvidence)
    {
        EXPECT_THROW(ESM4::ObservationStream(mEvents, "", 1, 123), std::runtime_error);
        EXPECT_THROW(ESM4::ObservationStream(mEvents, "line\nbreak", 1, 123), std::runtime_error);
        EXPECT_THROW(ESM4::ObservationStream(mEvents, "valid", 0, 123), std::runtime_error);
        EXPECT_THROW(ESM4::ObservationStream(mEvents, "valid", 1, 0), std::runtime_error);
        EXPECT_FALSE(std::filesystem::exists(mEvents));
    }

    TEST_F(ESM4Observation, MalformedEventsDoNotConsumeSequenceOrWritePartialLines)
    {
        ESM4::ObservationStream stream(mEvents, "run", 1, 123);
        const std::string before = read(mEvents);
        EXPECT_THROW(stream.record("run-end"), std::runtime_error);
        EXPECT_THROW(stream.record("run-start"), std::runtime_error);
        EXPECT_THROW(stream.record("bad event"), std::runtime_error);
        EXPECT_THROW(stream.record("event", { { "pid", std::uint64_t(999) } }), std::runtime_error);
        EXPECT_THROW(stream.record("event", { { "value", std::numeric_limits<double>::infinity() } }), std::runtime_error);
        EXPECT_THROW(stream.record("event", { { "value", std::numeric_limits<double>::quiet_NaN() } }), std::runtime_error);
        EXPECT_THROW(stream.record("event", { { "text", std::string(1024 * 1024, 'x') } }), std::runtime_error);
        EXPECT_EQ(stream.sequence(), 1);
        EXPECT_EQ(read(mEvents), before);
        stream.record("valid", { { "text", std::string("quote\"\\\n\t\1") } });
        EXPECT_NE(read(mEvents).find("quote\\\"\\\\\\u000a\\u0009\\u0001"), std::string::npos);
    }

    TEST_F(ESM4Observation, StateBoundaryIsReadOnlyAndAcknowledgesActualDiskBytes)
    {
        ESM4::RuntimeState state;
        state.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        state.mPlayer.mCell = ESM::FormKey::content("fixture.esm", 1);
        state.mPlayer.mRace = ESM::FormKey::content("fixture.esm", 2);
        state.mPlayer.mClass = ESM::FormKey::content("fixture.esm", 3);
        state.mPlayer.mActorValues.emplace("health.current", 42.5);
        state.mPlayer.mPosition.pos[0] = 0.1f;
        const auto before = state;
        const auto save = mDirectory / "save.omwsave";
        { std::ofstream output(save); output << "abc"; }
        ESM4::ObservationStream stream(mEvents, "run", 1, 123);
        stream.stateBoundary("save-complete", save, state);
        EXPECT_EQ(state, before);
        EXPECT_EQ(read(save), "abc");
        EXPECT_EQ(read(mDirectory / "live-1-2.json"), state.canonicalJson() + "\n");
        EXPECT_NE(read(mDirectory / "live-1-2.json").find("0.10000000149011612"), std::string::npos);
        EXPECT_NE(read(mEvents).find("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"), std::string::npos);
        stream.stateBoundary("load-complete", save, state);
        EXPECT_EQ(state, before);
        EXPECT_EQ(read(mDirectory / "live-1-3.json"), state.canonicalJson() + "\n");
        EXPECT_NE(read(mEvents).find("\"deltas\":{\"health\":0}"), std::string::npos);
    }

    TEST_F(ESM4Observation, MissingSaveOrCollidingStateNeverGetsCompletionAcknowledgment)
    {
        ESM4::ObservationStream stream(mEvents, "run", 1, 123);
        ESM4::RuntimeState state;
        const auto save = mDirectory / "save.omwsave";
        EXPECT_THROW(stream.stateBoundary("save-complete", save, state), std::runtime_error);
        { std::ofstream output(save); output << "abc"; }
        { std::ofstream output(mDirectory / "live-1-2.json"); output << "earlier"; }
        EXPECT_THROW(stream.stateBoundary("save-complete", save, state), std::runtime_error);
        EXPECT_THROW(stream.stateBoundary("fake", save, state), std::runtime_error);
        EXPECT_EQ(stream.sequence(), 1);
        EXPECT_EQ(read(mDirectory / "live-1-2.json"), "earlier");
        EXPECT_EQ(read(mEvents).find("save-complete"), std::string::npos);
    }
}
