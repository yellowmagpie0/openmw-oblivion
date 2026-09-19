#ifndef OPENMW_COMPONENTS_ESM4_OBSERVATION_H
#define OPENMW_COMPONENTS_ESM4_OBSERVATION_H

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace ESM4
{
    struct RuntimeState;

    // Optional, main-thread evidence sink. It has no world, actor or random
    // generator access and cannot apply state. Successful shutdown is explicit:
    // destruction during an exception must never manufacture a passing summary.
    class ObservationStream
    {
    public:
        using Deltas = std::map<std::string, double>;
        using Value = std::variant<std::string, std::uint64_t, double, bool, Deltas>;
        using Fields = std::map<std::string, Value>;
        static std::unique_ptr<ObservationStream> fromEnvironment();

        ObservationStream(const std::filesystem::path& path, std::string runId,
            std::uint64_t epoch, std::uint64_t pid);
        void advance(bool paused = false);
        void record(std::string_view event, const Fields& fields = {});
        void diagnostic(std::string_view message, bool unsupported = false);
        void stateBoundary(std::string_view event, const std::filesystem::path& save,
            const RuntimeState& liveState);
        void finish(std::uint64_t errors, std::uint64_t unsupported, std::uint64_t pending);
        std::uint64_t sequence() const { return mSequence; }

    private:
        void write(std::string_view event, const Fields& fields);
        std::filesystem::path mPath;
        std::string mRunId;
        std::uint64_t mEpoch;
        std::uint64_t mPid;
        std::uint64_t mTick = 0;
        std::uint64_t mSequence = 0;
        std::uint64_t mErrors = 0;
        std::uint64_t mUnsupported = 0;
        std::optional<double> mPreviousObservedHealth;
        bool mFinished = false;
        std::ofstream mStream;
    };
}

#endif
