#include "observation.hpp"

#include "runtimestate.hpp"

#include <components/files/hash.hpp>
#include <components/files/conversion.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
#include <type_traits>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace ESM4
{
    namespace
    {
        void string(std::ostream& out, std::string_view value)
        {
            constexpr char digits[] = "0123456789abcdef";
            out << '"';
            for (const unsigned char c : value)
            {
                if (c == '"' || c == '\\')
                    out << '\\' << c;
                else if (c < 0x20)
                    out << "\\u00" << digits[c >> 4] << digits[c & 15];
                else
                    out << c;
            }
            out << '"';
        }

        bool token(std::string_view value)
        {
            return !value.empty() && value.size() <= 128
                && std::all_of(value.begin(), value.end(), [](unsigned char c) {
                    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                        || (c >= '0' && c <= '9') || c == '-' || c == '_';
                });
        }

        std::string fingerprint(const std::filesystem::path& path)
        {
            std::ifstream input(path, std::ios::binary);
            if (!input)
                throw std::runtime_error("M15 observation cannot read saved game");
            return Files::getSha256(Files::pathToUnicodeString(path), input);
        }
    }

    std::unique_ptr<ObservationStream> ObservationStream::fromEnvironment()
    {
        const char* path = std::getenv("OPENMW_M15_EVENTS");
        if (path == nullptr || *path == '\0')
            return nullptr;
        const char* run = std::getenv("OPENMW_M15_RUN_ID");
        const char* epochText = std::getenv("OPENMW_M15_EPOCH");
        if (run == nullptr || epochText == nullptr)
            throw std::runtime_error("M15 observation environment has incomplete identity");
        const std::string_view value(epochText);
        std::uint64_t epoch = 0;
        const auto result = std::from_chars(value.data(), value.data() + value.size(), epoch);
        if (result.ec != std::errc{} || result.ptr != value.data() + value.size())
            throw std::runtime_error("M15 observation environment has invalid epoch");
#ifdef _WIN32
        const auto pid = _getpid();
#else
        const auto pid = getpid();
#endif
        return std::make_unique<ObservationStream>(Files::pathFromUnicodeString(path), run, epoch, pid);
    }

    ObservationStream::ObservationStream(const std::filesystem::path& path, std::string runId,
        std::uint64_t epoch, std::uint64_t pid)
        : mPath(path), mRunId(std::move(runId)), mEpoch(epoch), mPid(pid)
    {
        if (!token(mRunId) || epoch == 0 || pid == 0 || std::filesystem::exists(path))
            throw std::runtime_error("M15 observation requires fresh output and valid process identity");
        mStream.exceptions(std::ios::failbit | std::ios::badbit);
        mStream.open(path, std::ios::out | std::ios::binary);
        write("run-start", {});
    }

    void ObservationStream::advance()
    {
        if (mFinished || mTick == std::numeric_limits<std::uint64_t>::max())
            throw std::runtime_error("M15 observation tick outside active run");
        ++mTick;
    }

    void ObservationStream::record(std::string_view event, const Fields& fields)
    {
        if (event == "run-start" || event == "run-end")
            throw std::runtime_error("M15 observation boundary is reserved");
        write(event, fields);
    }

    void ObservationStream::write(std::string_view event, const Fields& fields)
    {
        static const std::set<std::string, std::less<>> reserved{
            "run_id", "epoch", "pid", "sequence", "tick", "event" };
        if (mFinished || !token(event) || mSequence >= 1'000'000)
            throw std::runtime_error("M15 observation outside active bounded stream");
        std::ostringstream line;
        line.imbue(std::locale::classic());
        line << "{\"run_id\":";
        string(line, mRunId);
        line << ",\"epoch\":" << mEpoch << ",\"pid\":" << mPid
             << ",\"sequence\":" << mSequence + 1 << ",\"tick\":" << mTick << ",\"event\":";
        string(line, event);
        for (const auto& [key, value] : fields)
        {
            if (!token(key) || reserved.contains(key))
                throw std::runtime_error("M15 observation field collides with protocol identity");
            line << ',';
            string(line, key);
            line << ':';
            std::visit([&line](const auto& item) {
                using T = std::decay_t<decltype(item)>;
                if constexpr (std::is_same_v<T, std::string>)
                    string(line, item);
                else if constexpr (std::is_same_v<T, bool>)
                    line << (item ? "true" : "false");
                else if constexpr (std::is_same_v<T, Deltas>)
                {
                    line << '{';
                    bool first = true;
                    for (const auto& [name, delta] : item)
                    {
                        if (!token(name) || !std::isfinite(delta))
                            throw std::runtime_error("M15 observation rejects invalid delta");
                        if (!first)
                            line << ',';
                        first = false;
                        string(line, name);
                        line << ':' << std::setprecision(17) << delta;
                    }
                    line << '}';
                }
                else
                {
                    if constexpr (std::is_same_v<T, double>)
                        if (!std::isfinite(item))
                            throw std::runtime_error("M15 observation rejects nonfinite evidence");
                    line << std::setprecision(17) << item;
                }
            }, value);
        }
        line << "}\n";
        const std::string bytes = line.str();
        if (bytes.size() > 1024 * 1024)
            throw std::runtime_error("M15 observation event exceeds size limit");
        mStream << bytes;
        mStream.flush();
        ++mSequence;
    }

    void ObservationStream::stateBoundary(std::string_view event, const std::filesystem::path& save,
        const RuntimeState& liveState)
    {
        if (event != "save-complete" && event != "load-complete")
            throw std::runtime_error("M15 observation requires a completed save/load boundary");
        // A separate serialization of live authoritative state accompanies the
        // digest of the actual disk save; neither operation modifies the state.
        const std::string saveHash = fingerprint(save);
        const std::filesystem::path livePath = mPath.parent_path()
            / ("live-" + std::to_string(mEpoch) + "-" + std::to_string(mSequence + 1) + ".json");
        if (std::filesystem::exists(livePath))
            throw std::runtime_error("M15 observation would overwrite prior state evidence");
        std::ofstream live;
        live.exceptions(std::ios::failbit | std::ios::badbit);
        live.open(livePath, std::ios::out | std::ios::binary);
        live << liveState.canonicalJson() << '\n';
        live.close();
        Deltas deltas;
        const auto health = liveState.mPlayer.mActorValues.find("health.current");
        if (health != liveState.mPlayer.mActorValues.end() && mPreviousObservedHealth)
            deltas.emplace("health", health->second - *mPreviousObservedHealth);
        record(event, { { "save", Files::pathToUnicodeString(std::filesystem::absolute(save)) },
            { "save_sha256", saveHash }, { "live", Files::pathToUnicodeString(livePath.filename()) },
            { "live_sha256", fingerprint(livePath) }, { "actor", liveState.mPlayer.mReference.serialize() },
            { "target", liveState.mPlayer.mReference.serialize() },
            { "cause", std::string(event == "save-complete" ? "normal-save" : "normal-load") },
            { "result", std::string("committed") },
            { "action_id", "observation-" + std::to_string(mSequence + 1) }, { "deltas", deltas } });
        mPreviousObservedHealth = health == liveState.mPlayer.mActorValues.end()
            ? std::nullopt : std::optional<double>(health->second);
    }

    void ObservationStream::diagnostic(std::string_view message, bool unsupported)
    {
        record(unsupported ? "unsupported-command" : "engine-error", { { "message", std::string(message) } });
        if (unsupported)
            ++mUnsupported;
        else
            ++mErrors;
    }

    void ObservationStream::finish(std::uint64_t errors, std::uint64_t unsupported, std::uint64_t pending)
    {
        write("run-end", { { "event_count", mSequence }, { "error_count", errors + mErrors },
            { "unsupported_count", unsupported + mUnsupported }, { "pending_count", pending } });
        mFinished = true;
        mStream.close();
    }
}
