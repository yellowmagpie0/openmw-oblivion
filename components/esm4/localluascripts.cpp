#include "localluascripts.hpp"

#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>

#include <cmath>
#include <set>
#include <stdexcept>

namespace ESM4
{
    namespace
    {
        constexpr std::uint32_t Version = 1;
        constexpr std::size_t MaxOwners = 100'000;
        constexpr std::size_t MaxScripts = 100'000;
        constexpr std::size_t MaxTimers = 1'000'000;
        constexpr std::size_t MaxBytes = 64 * 1024 * 1024;

        void require(bool valid)
        {
            if (!valid)
                throw std::runtime_error("Invalid native local Lua save state");
        }

        struct Budget
        {
            std::size_t mScripts = MaxScripts;
            std::size_t mTimers = MaxTimers;
            std::size_t mBytes = MaxBytes;

            static void consume(std::size_t& remaining, std::size_t count)
            {
                require(count <= remaining);
                remaining -= count;
            }

            std::string readBytes(ESM::ESMReader& reader)
            {
                reader.getSubHeader();
                consume(mBytes, reader.getSubSize());
                std::string result(reader.getSubSize(), '\0');
                reader.getExact(result.data(), result.size());
                return result;
            }

            std::string readData(ESM::ESMReader& reader)
            {
                return reader.isNextSub("LUAD") ? readBytes(reader) : std::string{};
            }
        };

        void validateKey(const ESM::FormKey& key)
        {
            require(!key.isNull() && key != ESM::FormKey::dynamic("player", 1));
            require(ESM::FormKey::deserialize(key.serialize()) == key);
        }

        void validateTimer(const ESM::LuaTimer& timer)
        {
            require(std::isfinite(timer.mTime));
        }
    }

    LocalLuaScripts loadLocalLuaScripts(ESM::ESMReader& reader)
    {
        LocalLuaScripts result;
        if (!reader.isNextSub("NLSV"))
        {
            require(!reader.hasMoreSubs());
            return result;
        }
        std::uint32_t version, owners;
        reader.getHT(version);
        require(version == Version);
        reader.getHNT(owners, "NLSC");
        require(owners <= MaxOwners);
        Budget budget;
        for (std::uint32_t i = 0; i < owners; ++i)
        {
            reader.getSubNameIs("NLSK");
            const auto text = budget.readBytes(reader);
            const auto key = ESM::FormKey::deserialize(text);
            validateKey(key);
            require(key.serialize() == text);
            auto [it, inserted] = result.emplace(key, ESM::LuaScripts{});
            require(inserted);
            std::set<std::int32_t> ids;
            while (reader.isNextSub("LUAS"))
            {
                Budget::consume(budget.mScripts, 1);
                ESM::LuaScript script;
                reader.getHT(script.mScriptId);
                require(script.mScriptId >= 0 && ids.insert(script.mScriptId).second);
                script.mData = budget.readData(reader);
                while (reader.isNextSub("LUAT"))
                {
                    Budget::consume(budget.mTimers, 1);
                    ESM::LuaTimer timer;
                    std::uint8_t type;
                    reader.getHT(type, timer.mTime);
                    require(type <= 1);
                    timer.mType = static_cast<ESM::LuaTimer::Type>(type);
                    reader.getSubNameIs("LUAC");
                    timer.mCallbackName = budget.readBytes(reader);
                    timer.mCallbackArgument = budget.readData(reader);
                    validateTimer(timer);
                    script.mTimers.push_back(std::move(timer));
                }
                it->second.mScripts.push_back(std::move(script));
            }
            std::uint32_t end;
            reader.getHNT(end, "NLSE");
            require(end == 0);
        }
        // This companion owns the tail of LUAM; reject duplicate sections and
        // unknown/trailing records instead of silently dropping saved scripts.
        require(!reader.hasMoreSubs());
        return result;
    }

    void saveLocalLuaScripts(ESM::ESMWriter& writer, const LocalLuaScripts& scripts)
    {
        require(scripts.size() <= MaxOwners);
        Budget budget;
        // Preflight the complete section before emitting it.
        for (const auto& [key, data] : scripts)
        {
            validateKey(key);
            Budget::consume(budget.mBytes, key.serialize().size());
            Budget::consume(budget.mScripts, data.mScripts.size());
            std::set<std::int32_t> ids;
            for (const auto& script : data.mScripts)
            {
                require(script.mScriptId >= 0 && ids.insert(script.mScriptId).second);
                Budget::consume(budget.mBytes, script.mData.size());
                Budget::consume(budget.mTimers, script.mTimers.size());
                for (const auto& timer : script.mTimers)
                {
                    validateTimer(timer);
                    Budget::consume(budget.mBytes, timer.mCallbackName.size());
                    Budget::consume(budget.mBytes, timer.mCallbackArgument.size());
                }
            }
        }
        writer.writeHNT("NLSV", Version);
        writer.writeHNT("NLSC", static_cast<std::uint32_t>(scripts.size()));
        for (const auto& [key, data] : scripts)
        {
            writer.writeHNString("NLSK", key.serialize());
            for (const auto& script : data.mScripts)
            {
                writer.writeHNT("LUAS", script.mScriptId);
                ESM::saveLuaBinaryData(writer, script.mData);
                for (const auto& timer : script.mTimers)
                {
                    writer.startSubRecord("LUAT");
                    writer.writeT(timer.mType);
                    writer.writeT(timer.mTime);
                    writer.endRecord("LUAT");
                    // Names are Lua strings, including empty/binary strings.
                    writer.startSubRecord("LUAC");
                    writer.write(timer.mCallbackName.data(), timer.mCallbackName.size());
                    writer.endRecord("LUAC");
                    ESM::saveLuaBinaryData(writer, timer.mCallbackArgument);
                }
            }
            writer.writeHNT("NLSE", std::uint32_t{ 0 });
        }
    }
}
