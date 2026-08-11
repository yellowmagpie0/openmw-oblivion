#include "runtimestate.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <type_traits>

#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>

namespace ESM4
{
    namespace
    {
        constexpr std::string_view sMagic = "OMW4STATE";
        constexpr std::uint32_t sMaximumCollectionSize = 1'000'000;
        constexpr std::uint32_t sMaximumStringSize = 16 * 1024 * 1024;
        constexpr std::size_t sMaximumPayloadSize = 256 * 1024 * 1024;
        constexpr std::size_t sChunkSize = 60 * 1024;

        class BinaryWriter
        {
        public:
            void bytes(const void* value, std::size_t size)
            {
                const auto* begin = static_cast<const std::uint8_t*>(value);
                mData.insert(mData.end(), begin, begin + size);
            }

            template <class T>
            void integer(T value)
            {
                using U = std::make_unsigned_t<T>;
                U bits = static_cast<U>(value);
                for (std::size_t i = 0; i < sizeof(T); ++i)
                    mData.push_back(static_cast<std::uint8_t>(bits >> (i * 8)));
            }

            void floating(float value) { integer(std::bit_cast<std::uint32_t>(value)); }
            void floating(double value) { integer(std::bit_cast<std::uint64_t>(value)); }

            void string(std::string_view value)
            {
                if (value.size() > sMaximumStringSize)
                    throw std::runtime_error("TES4 runtime-state string exceeds the size limit");
                integer<std::uint32_t>(static_cast<std::uint32_t>(value.size()));
                bytes(value.data(), value.size());
            }

            std::vector<std::uint8_t> take() { return std::move(mData); }

        private:
            std::vector<std::uint8_t> mData;
        };

        class BinaryReader
        {
        public:
            explicit BinaryReader(const std::vector<std::uint8_t>& data)
                : mData(data)
            {
            }

            void bytes(void* value, std::size_t size)
            {
                require(size);
                std::copy_n(mData.data() + mOffset, size, static_cast<std::uint8_t*>(value));
                mOffset += size;
            }

            template <class T>
            T integer()
            {
                require(sizeof(T));
                using U = std::make_unsigned_t<T>;
                U result = 0;
                for (std::size_t i = 0; i < sizeof(T); ++i)
                    result |= static_cast<U>(mData[mOffset++]) << (i * 8);
                return static_cast<T>(result);
            }

            float float32() { return std::bit_cast<float>(integer<std::uint32_t>()); }
            double float64() { return std::bit_cast<double>(integer<std::uint64_t>()); }

            std::string string()
            {
                const std::uint32_t size = integer<std::uint32_t>();
                if (size > sMaximumStringSize)
                    throw std::runtime_error("TES4 runtime-state string exceeds the size limit");
                require(size);
                std::string result(reinterpret_cast<const char*>(mData.data() + mOffset), size);
                mOffset += size;
                return result;
            }

            std::uint32_t count()
            {
                const std::uint32_t result = integer<std::uint32_t>();
                if (result > sMaximumCollectionSize)
                    throw std::runtime_error("TES4 runtime-state collection exceeds the size limit");
                return result;
            }

            bool eof() const { return mOffset == mData.size(); }

        private:
            void require(std::size_t size) const
            {
                if (size > mData.size() - mOffset)
                    throw std::runtime_error("Truncated TES4 runtime-state payload");
            }

            const std::vector<std::uint8_t>& mData;
            std::size_t mOffset = 0;
        };

        void writeKey(BinaryWriter& writer, const ESM::FormKey& key)
        {
            writer.string(key.serialize());
        }

        ESM::FormKey readKey(BinaryReader& reader)
        {
            return ESM::FormKey::deserialize(reader.string());
        }

        void writePosition(BinaryWriter& writer, const ESM::Position& position)
        {
            for (float value : position.pos)
                writer.floating(value);
            for (float value : position.rot)
                writer.floating(value);
        }

        ESM::Position readPosition(BinaryReader& reader)
        {
            ESM::Position result;
            for (float& value : result.pos)
                value = reader.float32();
            for (float& value : result.rot)
                value = reader.float32();
            return result;
        }

        void writeValue(BinaryWriter& writer, const RuntimeValue& value)
        {
            std::visit(
                [&writer](const auto& item) {
                    using T = std::decay_t<decltype(item)>;
                    if constexpr (std::is_same_v<T, bool>)
                    {
                        writer.integer<std::uint8_t>(1);
                        writer.integer<std::uint8_t>(item ? 1 : 0);
                    }
                    else if constexpr (std::is_same_v<T, std::int64_t>)
                    {
                        writer.integer<std::uint8_t>(2);
                        writer.integer(item);
                    }
                    else if constexpr (std::is_same_v<T, double>)
                    {
                        writer.integer<std::uint8_t>(3);
                        writer.floating(item);
                    }
                    else
                    {
                        writer.integer<std::uint8_t>(4);
                        writer.string(item);
                    }
                },
                value);
        }

        RuntimeValue readValue(BinaryReader& reader)
        {
            switch (reader.integer<std::uint8_t>())
            {
                case 1:
                {
                    const std::uint8_t value = reader.integer<std::uint8_t>();
                    if (value > 1)
                        throw std::runtime_error("Invalid TES4 runtime-state boolean");
                    return value != 0;
                }
                case 2:
                    return reader.integer<std::int64_t>();
                case 3:
                    return reader.float64();
                case 4:
                    return reader.string();
                default:
                    throw std::runtime_error("Unknown TES4 runtime-state value type");
            }
        }

        void writeScriptValue(BinaryWriter& writer, const RuntimeScriptValue& value)
        {
            std::visit(
                [&writer](const auto& item) {
                    using T = std::decay_t<decltype(item)>;
                    if constexpr (std::is_same_v<T, std::monostate>)
                        writer.integer<std::uint8_t>(0);
                    else if constexpr (std::is_same_v<T, std::int64_t>)
                    {
                        writer.integer<std::uint8_t>(1);
                        writer.integer(item);
                    }
                    else if constexpr (std::is_same_v<T, double>)
                    {
                        writer.integer<std::uint8_t>(2);
                        writer.floating(item);
                    }
                    else if constexpr (std::is_same_v<T, std::string>)
                    {
                        writer.integer<std::uint8_t>(3);
                        writer.string(item);
                    }
                    else
                    {
                        writer.integer<std::uint8_t>(4);
                        writeKey(writer, item);
                    }
                },
                value);
        }

        RuntimeScriptValue readScriptValue(BinaryReader& reader)
        {
            switch (reader.integer<std::uint8_t>())
            {
                case 0:
                    return std::monostate{};
                case 1:
                    return reader.integer<std::int64_t>();
                case 2:
                    return reader.float64();
                case 3:
                    return reader.string();
                case 4:
                    return readKey(reader);
                default:
                    throw std::runtime_error("Unknown TES4 runtime-state script value type");
            }
        }

        void writeInventory(BinaryWriter& writer, const std::vector<RuntimeInventoryItem>& inventory)
        {
            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(inventory.size()));
            for (const RuntimeInventoryItem& item : inventory)
            {
                writeKey(writer, item.mBase);
                writer.integer(item.mCount);
            }
        }

        std::vector<RuntimeInventoryItem> readInventory(BinaryReader& reader)
        {
            std::vector<RuntimeInventoryItem> result;
            const std::uint32_t count = reader.count();
            result.reserve(count);
            for (std::uint32_t i = 0; i < count; ++i)
                result.push_back({ readKey(reader), reader.integer<std::int32_t>() });
            return result;
        }

        void validatePosition(const ESM::Position& position)
        {
            for (float value : position.pos)
                if (!std::isfinite(value))
                    throw std::runtime_error("TES4 runtime-state position is not finite");
            for (float value : position.rot)
                if (!std::isfinite(value))
                    throw std::runtime_error("TES4 runtime-state rotation is not finite");
        }

        void validateValue(const RuntimeValue& value)
        {
            if (const double* number = std::get_if<double>(&value); number != nullptr && !std::isfinite(*number))
                throw std::runtime_error("TES4 runtime-state value is not finite");
        }

        void validateScriptValue(const RuntimeScriptValue& value)
        {
            if (const double* number = std::get_if<double>(&value); number != nullptr && !std::isfinite(*number))
                throw std::runtime_error("TES4 runtime-state script value is not finite");
            if (const ESM::FormKey* key = std::get_if<ESM::FormKey>(&value); key != nullptr && key->isNull())
                throw std::runtime_error("TES4 runtime-state script reference value is null");
        }

        std::string escapeJson(std::string_view value)
        {
            std::ostringstream stream;
            for (const unsigned char c : value)
            {
                switch (c)
                {
                    case '\\':
                        stream << "\\\\";
                        break;
                    case '"':
                        stream << "\\\"";
                        break;
                    case '\n':
                        stream << "\\n";
                        break;
                    case '\r':
                        stream << "\\r";
                        break;
                    case '\t':
                        stream << "\\t";
                        break;
                    default:
                        if (c < 0x20)
                            stream << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c);
                        else
                            stream << c;
                }
            }
            return stream.str();
        }

        void writeJsonValue(std::ostream& stream, const RuntimeValue& value)
        {
            std::visit(
                [&stream](const auto& item) {
                    using T = std::decay_t<decltype(item)>;
                    if constexpr (std::is_same_v<T, bool>)
                        stream << (item ? "true" : "false");
                    else if constexpr (std::is_same_v<T, std::string>)
                        stream << '"' << escapeJson(item) << '"';
                    else
                        stream << std::setprecision(17) << item;
                },
                value);
        }

        void writeJsonScriptValue(std::ostream& stream, const RuntimeScriptValue& value)
        {
            std::visit(
                [&stream](const auto& item) {
                    using T = std::decay_t<decltype(item)>;
                    if constexpr (std::is_same_v<T, std::monostate>)
                        stream << "null";
                    else if constexpr (std::is_same_v<T, std::string>)
                        stream << "{\"type\":\"string\",\"value\":\"" << escapeJson(item) << "\"}";
                    else if constexpr (std::is_same_v<T, ESM::FormKey>)
                        stream << "{\"type\":\"reference\",\"value\":\"" << escapeJson(item.serialize())
                               << "\"}";
                    else
                        stream << "{\"type\":\"number\",\"value\":" << std::setprecision(17) << item << '}';
                },
                value);
        }

        void writeJsonPosition(std::ostream& stream, const ESM::Position& position)
        {
            stream << '[';
            for (int i = 0; i < 3; ++i)
                stream << (i == 0 ? "" : ",") << std::setprecision(9) << position.pos[i];
            for (int i = 0; i < 3; ++i)
                stream << ',' << std::setprecision(9) << position.rot[i];
            stream << ']';
        }
    }

    void RuntimeState::validate() const
    {
        if (mVersion < 1 || mVersion > CurrentRuntimeStateVersion)
            throw std::runtime_error("Unsupported TES4 runtime-state version " + std::to_string(mVersion));
        if (mProfile != ESM::GameProfile::Oblivion)
            throw std::runtime_error("TES4 runtime state requires the Oblivion game profile");
        if (mNextDynamicSerial == 0)
            throw std::runtime_error("TES4 runtime-state dynamic serial must be non-zero");
        if (!std::isfinite(mClock.mHour) || !std::isfinite(mClock.mTimeScale))
            throw std::runtime_error("TES4 runtime-state clock is not finite");
        const auto checkSize = [](std::size_t size, std::string_view name) {
            if (size > sMaximumCollectionSize)
                throw std::runtime_error("TES4 runtime-state " + std::string(name) + " exceeds the size limit");
        };
        checkSize(mContent.size(), "content list");
        checkSize(mPlayer.mActorValues.size(), "player actor-value list");
        checkSize(mPlayer.mInventory.size(), "player inventory");
        checkSize(mGlobals.size(), "global list");
        checkSize(mReferences.size(), "reference list");
        checkSize(mScriptInstances.size(), "script instance list");
        checkSize(mQuests.size(), "quest list");
        if (mVersion < 2 && (mScriptEventSequence != 0 || !mScriptInstances.empty() || !mQuests.empty()))
            throw std::runtime_error("TES4 runtime-state version 1 cannot contain ObScript state");

        if (mPlayer.mReference.isNull() || mPlayer.mCell.isNull())
            throw std::runtime_error("TES4 runtime-state player has a null required FormKey");

        std::set<std::string> plugins;
        for (const RuntimeContentIdentity& content : mContent)
        {
            const std::string plugin = ESM::normalizePluginName(content.mPlugin);
            if (!plugins.insert(plugin).second)
                throw std::runtime_error("Duplicate TES4 runtime-state content identity: " + plugin);
            if (content.mFingerprint.empty())
                throw std::runtime_error("TES4 runtime-state content fingerprint is empty");
        }

        validatePosition(mPlayer.mPosition);
        for (const auto& [name, value] : mPlayer.mActorValues)
        {
            if (name.empty() || !std::isfinite(value))
                throw std::runtime_error("Invalid TES4 runtime-state player actor value");
        }
        for (const RuntimeInventoryItem& item : mPlayer.mInventory)
            if (item.mBase.isNull() || item.mCount == 0)
                throw std::runtime_error("Invalid TES4 runtime-state player inventory entry");

        for (const auto& [key, value] : mGlobals)
        {
            if (key.isNull())
                throw std::runtime_error("TES4 runtime-state global has a null FormKey");
            validateValue(value);
        }

        std::set<ESM::FormKey> referenceKeys;
        for (const RuntimeReferenceState& reference : mReferences)
        {
            if (reference.mKey.isNull() || reference.mBase.isNull() || reference.mCell.isNull())
                throw std::runtime_error("TES4 runtime-state reference has a null required FormKey");
            if (!referenceKeys.insert(reference.mKey).second)
                throw std::runtime_error("Duplicate TES4 runtime-state reference: " + reference.mKey.serialize());
            if (reference.mOwner && reference.mOwner->isNull())
                throw std::runtime_error("TES4 runtime-state reference has a null owner");
            validatePosition(reference.mPosition);
            checkSize(reference.mInventory.size(), "reference inventory");
            checkSize(reference.mCustomState.size(), "reference custom state");
            for (const RuntimeInventoryItem& item : reference.mInventory)
                if (item.mBase.isNull() || item.mCount == 0)
                    throw std::runtime_error("Invalid TES4 runtime-state reference inventory entry");
            for (const auto& [name, value] : reference.mCustomState)
            {
                if (name.empty())
                    throw std::runtime_error("TES4 runtime-state custom-state key is empty");
                validateValue(value);
            }
        }

        std::set<std::pair<std::string, ESM::FormKey>> scriptKeys;
        for (const RuntimeScriptInstance& script : mScriptInstances)
        {
            if (script.mUnit.empty() || script.mContext.isNull())
                throw std::runtime_error("TES4 runtime-state script instance has a null identity");
            if (!scriptKeys.emplace(script.mUnit, script.mContext).second)
                throw std::runtime_error("Duplicate TES4 runtime-state script instance: " + script.mUnit);
            checkSize(script.mLocals.size(), "script local list");
            for (const RuntimeScriptValue& value : script.mLocals)
                validateScriptValue(value);
        }

        std::set<ESM::FormKey> questKeys;
        for (const RuntimeQuestState& quest : mQuests)
        {
            if (quest.mQuest.isNull() || !questKeys.insert(quest.mQuest).second)
                throw std::runtime_error("Invalid or duplicate TES4 runtime-state quest");
            checkSize(quest.mCompletedStages.size(), "completed quest stage list");
            if (!std::is_sorted(quest.mCompletedStages.begin(), quest.mCompletedStages.end())
                || std::adjacent_find(quest.mCompletedStages.begin(), quest.mCompletedStages.end())
                    != quest.mCompletedStages.end())
                throw std::runtime_error("TES4 runtime-state completed quest stages are not sorted and unique");
        }
    }

    std::vector<std::uint8_t> RuntimeState::serializeBinary() const
    {
        validate();
        BinaryWriter writer;
        writer.bytes(sMagic.data(), sMagic.size());
        writer.integer(mVersion);
        writer.integer<std::uint8_t>(static_cast<std::uint8_t>(mProfile));
        writer.integer(mNextDynamicSerial);

        writer.integer<std::uint32_t>(static_cast<std::uint32_t>(mContent.size()));
        for (const RuntimeContentIdentity& content : mContent)
        {
            writer.string(ESM::normalizePluginName(content.mPlugin));
            writer.string(content.mFingerprint);
        }

        writer.integer(mClock.mYear);
        writer.integer(mClock.mMonth);
        writer.integer(mClock.mDay);
        writer.floating(mClock.mHour);
        writer.floating(mClock.mTimeScale);

        writeKey(writer, mPlayer.mReference);
        writeKey(writer, mPlayer.mCell);
        writePosition(writer, mPlayer.mPosition);
        writer.integer<std::uint32_t>(static_cast<std::uint32_t>(mPlayer.mActorValues.size()));
        for (const auto& [name, value] : mPlayer.mActorValues)
        {
            writer.string(name);
            writer.floating(value);
        }
        writeInventory(writer, mPlayer.mInventory);

        writer.integer<std::uint32_t>(static_cast<std::uint32_t>(mGlobals.size()));
        for (const auto& [key, value] : mGlobals)
        {
            writeKey(writer, key);
            writeValue(writer, value);
        }

        writer.integer<std::uint32_t>(static_cast<std::uint32_t>(mReferences.size()));
        for (const RuntimeReferenceState& reference : mReferences)
        {
            writeKey(writer, reference.mKey);
            writeKey(writer, reference.mBase);
            writeKey(writer, reference.mCell);
            writer.integer<std::uint8_t>(reference.mEnabled ? 1 : 0);
            writer.integer<std::uint8_t>(reference.mDeleted ? 1 : 0);
            writePosition(writer, reference.mPosition);
            writer.integer<std::uint8_t>(reference.mOwner.has_value() ? 1 : 0);
            if (reference.mOwner)
                writeKey(writer, *reference.mOwner);
            writer.integer(reference.mLockLevel);
            writeInventory(writer, reference.mInventory);
            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(reference.mCustomState.size()));
            for (const auto& [name, value] : reference.mCustomState)
            {
                writer.string(name);
                writeValue(writer, value);
            }
        }

        if (mVersion >= 2)
        {
            writer.integer(mScriptEventSequence);
            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(mScriptInstances.size()));
            for (const RuntimeScriptInstance& script : mScriptInstances)
            {
                writer.string(script.mUnit);
                writeKey(writer, script.mContext);
                writer.integer<std::uint8_t>(script.mOnLoadFired ? 1 : 0);
                writer.integer<std::uint32_t>(static_cast<std::uint32_t>(script.mLocals.size()));
                for (const RuntimeScriptValue& value : script.mLocals)
                    writeScriptValue(writer, value);
            }
            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(mQuests.size()));
            for (const RuntimeQuestState& quest : mQuests)
            {
                writeKey(writer, quest.mQuest);
                writer.integer(quest.mStage);
                writer.integer<std::uint8_t>(quest.mRunning ? 1 : 0);
                writer.integer<std::uint32_t>(static_cast<std::uint32_t>(quest.mCompletedStages.size()));
                for (const std::int32_t stage : quest.mCompletedStages)
                    writer.integer(stage);
            }
        }

        std::vector<std::uint8_t> result = writer.take();
        if (result.size() > sMaximumPayloadSize)
            throw std::runtime_error("TES4 runtime-state payload exceeds the size limit");
        return result;
    }

    RuntimeState RuntimeState::deserializeBinary(const std::vector<std::uint8_t>& data)
    {
        if (data.size() > sMaximumPayloadSize)
            throw std::runtime_error("TES4 runtime-state payload exceeds the size limit");
        BinaryReader reader(data);
        std::string magic(sMagic.size(), '\0');
        reader.bytes(magic.data(), magic.size());
        if (magic != sMagic)
            throw std::runtime_error("Invalid TES4 runtime-state magic");

        RuntimeState result;
        result.mVersion = reader.integer<std::uint32_t>();
        const std::uint8_t profile = reader.integer<std::uint8_t>();
        if (profile > static_cast<std::uint8_t>(ESM::GameProfile::Oblivion))
            throw std::runtime_error("Unknown TES4 runtime-state game profile");
        result.mProfile = static_cast<ESM::GameProfile>(profile);
        result.mNextDynamicSerial = reader.integer<std::uint64_t>();

        const std::uint32_t contentCount = reader.count();
        result.mContent.reserve(contentCount);
        for (std::uint32_t i = 0; i < contentCount; ++i)
            result.mContent.push_back({ reader.string(), reader.string() });

        result.mClock.mYear = reader.integer<std::int32_t>();
        result.mClock.mMonth = reader.integer<std::int32_t>();
        result.mClock.mDay = reader.integer<std::int32_t>();
        result.mClock.mHour = reader.float64();
        result.mClock.mTimeScale = reader.float64();

        result.mPlayer.mReference = readKey(reader);
        result.mPlayer.mCell = readKey(reader);
        result.mPlayer.mPosition = readPosition(reader);
        const std::uint32_t actorValueCount = reader.count();
        for (std::uint32_t i = 0; i < actorValueCount; ++i)
        {
            const std::string name = reader.string();
            if (!result.mPlayer.mActorValues.emplace(name, reader.float64()).second)
                throw std::runtime_error("Duplicate TES4 runtime-state player actor value");
        }
        result.mPlayer.mInventory = readInventory(reader);

        const std::uint32_t globalCount = reader.count();
        for (std::uint32_t i = 0; i < globalCount; ++i)
        {
            const ESM::FormKey key = readKey(reader);
            if (!result.mGlobals.emplace(key, readValue(reader)).second)
                throw std::runtime_error("Duplicate TES4 runtime-state global");
        }

        const std::uint32_t referenceCount = reader.count();
        result.mReferences.reserve(referenceCount);
        for (std::uint32_t i = 0; i < referenceCount; ++i)
        {
            RuntimeReferenceState reference;
            reference.mKey = readKey(reader);
            reference.mBase = readKey(reader);
            reference.mCell = readKey(reader);
            const std::uint8_t enabled = reader.integer<std::uint8_t>();
            const std::uint8_t deleted = reader.integer<std::uint8_t>();
            if (enabled > 1 || deleted > 1)
                throw std::runtime_error("Invalid TES4 runtime-state reference flags");
            reference.mEnabled = enabled != 0;
            reference.mDeleted = deleted != 0;
            reference.mPosition = readPosition(reader);
            const std::uint8_t hasOwner = reader.integer<std::uint8_t>();
            if (hasOwner > 1)
                throw std::runtime_error("Invalid TES4 runtime-state owner flag");
            if (hasOwner)
                reference.mOwner = readKey(reader);
            reference.mLockLevel = reader.integer<std::int32_t>();
            reference.mInventory = readInventory(reader);
            const std::uint32_t customCount = reader.count();
            for (std::uint32_t j = 0; j < customCount; ++j)
            {
                const std::string name = reader.string();
                if (!reference.mCustomState.emplace(name, readValue(reader)).second)
                    throw std::runtime_error("Duplicate TES4 runtime-state custom-state key");
            }
            result.mReferences.push_back(std::move(reference));
        }

        if (result.mVersion >= 2)
        {
            result.mScriptEventSequence = reader.integer<std::uint64_t>();
            const std::uint32_t scriptCount = reader.count();
            result.mScriptInstances.reserve(scriptCount);
            for (std::uint32_t i = 0; i < scriptCount; ++i)
            {
                RuntimeScriptInstance script;
                script.mUnit = reader.string();
                script.mContext = readKey(reader);
                const std::uint8_t onLoad = reader.integer<std::uint8_t>();
                if (onLoad > 1)
                    throw std::runtime_error("Invalid TES4 runtime-state OnLoad flag");
                script.mOnLoadFired = onLoad != 0;
                const std::uint32_t localCount = reader.count();
                script.mLocals.reserve(localCount);
                for (std::uint32_t j = 0; j < localCount; ++j)
                    script.mLocals.push_back(readScriptValue(reader));
                result.mScriptInstances.push_back(std::move(script));
            }
            const std::uint32_t questCount = reader.count();
            result.mQuests.reserve(questCount);
            for (std::uint32_t i = 0; i < questCount; ++i)
            {
                RuntimeQuestState quest;
                quest.mQuest = readKey(reader);
                quest.mStage = reader.integer<std::int32_t>();
                const std::uint8_t running = reader.integer<std::uint8_t>();
                if (running > 1)
                    throw std::runtime_error("Invalid TES4 runtime-state quest running flag");
                quest.mRunning = running != 0;
                const std::uint32_t completedCount = reader.count();
                quest.mCompletedStages.reserve(completedCount);
                for (std::uint32_t j = 0; j < completedCount; ++j)
                    quest.mCompletedStages.push_back(reader.integer<std::int32_t>());
                result.mQuests.push_back(std::move(quest));
            }
        }

        if (!reader.eof())
            throw std::runtime_error("TES4 runtime-state payload has trailing data");
        result.validate();
        return result;
    }

    void RuntimeState::save(ESM::ESMWriter& writer) const
    {
        const std::vector<std::uint8_t> payload = serializeBinary();
        writer.writeHNT("VERS", mVersion);
        for (std::size_t offset = 0; offset < payload.size(); offset += sChunkSize)
        {
            const std::size_t size = std::min(sChunkSize, payload.size() - offset);
            writer.startSubRecord("DATA");
            writer.write(reinterpret_cast<const char*>(payload.data() + offset), size);
            writer.endRecord("DATA");
        }
    }

    void RuntimeState::load(ESM::ESMReader& reader)
    {
        std::uint32_t version = 0;
        reader.getHNT(version, "VERS");
        std::vector<std::uint8_t> payload;
        while (reader.isNextSub("DATA"))
        {
            reader.getSubHeader();
            const std::size_t size = reader.getSubSize();
            if (size > sMaximumPayloadSize - payload.size())
                throw std::runtime_error("TES4 runtime-state payload exceeds the size limit");
            const std::size_t offset = payload.size();
            payload.resize(offset + size);
            reader.getExact(payload.data() + offset, size);
        }
        RuntimeState parsed = deserializeBinary(payload);
        if (parsed.mVersion != version)
            throw std::runtime_error("TES4 runtime-state record version does not match its payload");
        *this = std::move(parsed);
    }

    std::vector<std::string> RuntimeState::getMissingContentFiles(
        const std::vector<std::string>& currentContent) const
    {
        std::set<std::string> current;
        for (const std::string& value : currentContent)
            current.insert(ESM::normalizePluginName(value));
        std::vector<std::string> result;
        for (const RuntimeContentIdentity& content : mContent)
        {
            const std::string normalized = ESM::normalizePluginName(content.mPlugin);
            if (!current.contains(normalized))
                result.push_back(normalized);
        }
        return result;
    }

    void RuntimeState::validateContent(const std::vector<RuntimeContentIdentity>& currentContent) const
    {
        std::map<std::string, std::string, std::less<>> current;
        for (const RuntimeContentIdentity& content : currentContent)
        {
            const std::string plugin = ESM::normalizePluginName(content.mPlugin);
            if (!current.emplace(plugin, content.mFingerprint).second)
                throw std::runtime_error("Duplicate current TES4 content identity: " + plugin);
        }
        for (const RuntimeContentIdentity& saved : mContent)
        {
            const std::string plugin = ESM::normalizePluginName(saved.mPlugin);
            const auto found = current.find(plugin);
            if (found == current.end())
                throw std::runtime_error("TES4 runtime state requires missing content file " + plugin);
            if (found->second != saved.mFingerprint)
                throw std::runtime_error("TES4 runtime state content fingerprint mismatch for " + plugin
                    + ": saved " + saved.mFingerprint + ", current " + found->second);
        }
    }

    std::string RuntimeState::canonicalJson() const
    {
        validate();
        std::ostringstream stream;
        stream << "{\"schema_version\":" << mVersion << ",\"profile\":\"oblivion\",\"next_dynamic_serial\":"
               << mNextDynamicSerial << ",\"content\":[";
        for (std::size_t i = 0; i < mContent.size(); ++i)
        {
            if (i)
                stream << ',';
            stream << "{\"plugin\":\"" << escapeJson(ESM::normalizePluginName(mContent[i].mPlugin))
                   << "\",\"fingerprint\":\"" << escapeJson(mContent[i].mFingerprint) << "\"}";
        }
        stream << "],\"clock\":{\"year\":" << mClock.mYear << ",\"month\":" << mClock.mMonth
               << ",\"day\":" << mClock.mDay << ",\"hour\":" << std::setprecision(17) << mClock.mHour
               << ",\"time_scale\":" << mClock.mTimeScale << "},\"player\":{\"reference\":\""
               << escapeJson(mPlayer.mReference.serialize()) << "\",\"cell\":\""
               << escapeJson(mPlayer.mCell.serialize()) << "\",\"position\":";
        writeJsonPosition(stream, mPlayer.mPosition);
        stream << ",\"actor_values\":{";
        std::size_t index = 0;
        for (const auto& [name, value] : mPlayer.mActorValues)
            stream << (index++ ? "," : "") << '"' << escapeJson(name) << "\":" << std::setprecision(17) << value;
        stream << "},\"inventory\":[";
        for (std::size_t i = 0; i < mPlayer.mInventory.size(); ++i)
        {
            if (i)
                stream << ',';
            stream << "{\"base\":\"" << escapeJson(mPlayer.mInventory[i].mBase.serialize())
                   << "\",\"count\":" << mPlayer.mInventory[i].mCount << '}';
        }
        stream << "]},\"globals\":{";
        index = 0;
        for (const auto& [key, value] : mGlobals)
        {
            stream << (index++ ? "," : "") << '"' << escapeJson(key.serialize()) << "\":";
            writeJsonValue(stream, value);
        }
        stream << "},\"references\":[";
        for (std::size_t i = 0; i < mReferences.size(); ++i)
        {
            const RuntimeReferenceState& reference = mReferences[i];
            if (i)
                stream << ',';
            stream << "{\"key\":\"" << escapeJson(reference.mKey.serialize()) << "\",\"base\":\""
                   << escapeJson(reference.mBase.serialize()) << "\",\"cell\":\""
                   << escapeJson(reference.mCell.serialize()) << "\",\"enabled\":"
                   << (reference.mEnabled ? "true" : "false") << ",\"deleted\":"
                   << (reference.mDeleted ? "true" : "false") << ",\"position\":";
            writeJsonPosition(stream, reference.mPosition);
            stream << ",\"owner\":";
            if (reference.mOwner)
                stream << '"' << escapeJson(reference.mOwner->serialize()) << '"';
            else
                stream << "null";
            stream << ",\"lock_level\":" << reference.mLockLevel << ",\"inventory\":[";
            for (std::size_t j = 0; j < reference.mInventory.size(); ++j)
            {
                if (j)
                    stream << ',';
                stream << "{\"base\":\"" << escapeJson(reference.mInventory[j].mBase.serialize())
                       << "\",\"count\":" << reference.mInventory[j].mCount << '}';
            }
            stream << "],\"custom_state\":{";
            index = 0;
            for (const auto& [name, value] : reference.mCustomState)
            {
                stream << (index++ ? "," : "") << '"' << escapeJson(name) << "\":";
                writeJsonValue(stream, value);
            }
            stream << "}}";
        }
        stream << "],\"script_event_sequence\":" << mScriptEventSequence << ",\"script_instances\":[";
        for (std::size_t i = 0; i < mScriptInstances.size(); ++i)
        {
            const RuntimeScriptInstance& script = mScriptInstances[i];
            if (i)
                stream << ',';
            stream << "{\"unit\":\"" << escapeJson(script.mUnit) << "\",\"context\":\""
                   << escapeJson(script.mContext.serialize()) << "\",\"on_load_fired\":"
                   << (script.mOnLoadFired ? "true" : "false") << ",\"locals\":[";
            for (std::size_t j = 0; j < script.mLocals.size(); ++j)
            {
                if (j)
                    stream << ',';
                writeJsonScriptValue(stream, script.mLocals[j]);
            }
            stream << "]}";
        }
        stream << "],\"quests\":[";
        for (std::size_t i = 0; i < mQuests.size(); ++i)
        {
            const RuntimeQuestState& quest = mQuests[i];
            if (i)
                stream << ',';
            stream << "{\"quest\":\"" << escapeJson(quest.mQuest.serialize()) << "\",\"stage\":"
                   << quest.mStage << ",\"running\":" << (quest.mRunning ? "true" : "false")
                   << ",\"completed_stages\":[";
            for (std::size_t j = 0; j < quest.mCompletedStages.size(); ++j)
                stream << (j ? "," : "") << quest.mCompletedStages[j];
            stream << "]}";
        }
        stream << "]}";
        return stream.str();
    }
}
