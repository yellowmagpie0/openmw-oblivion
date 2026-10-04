#include "runtimestate.hpp"

#include "ability.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <functional>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <tuple>

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

        void writeCalendar(BinaryWriter& writer, const CalendarInstant& instant)
        {
            writer.integer(instant.mYear);
            writer.integer(instant.mMonth);
            writer.integer(instant.mDay);
            writer.floating(instant.mHour);
        }

        CalendarInstant readCalendar(BinaryReader& reader)
        {
            CalendarInstant result;
            result.mYear = reader.integer<std::int32_t>();
            result.mMonth = reader.integer<std::int32_t>();
            result.mDay = reader.integer<std::int32_t>();
            result.mHour = reader.float64();
            return result;
        }

        bool validCalendar(const CalendarInstant& instant)
        {
            return isValidCalendarInstant(instant);
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

        std::string escapeJson(std::string_view value);

        void writeInventory(BinaryWriter& writer, const std::vector<RuntimeInventoryItem>& inventory,
            std::uint32_t version)
        {
            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(inventory.size()));
            for (const RuntimeInventoryItem& item : inventory)
            {
                writeKey(writer, item.mBase);
                writer.integer(item.mCount);
                if (version >= 4)
                {
                    if (version >= 24) writer.floating(static_cast<float>(item.mCondition));
                    else writer.integer(static_cast<std::int32_t>(item.mCondition));
                    writer.floating(item.mCharge);
                    writer.integer(item.mEquippedSlots);
                    writer.integer(item.mHotkey);
                    writeKey(writer, item.mOwner);
                    writer.floating(item.mRemainingUsageTime);
                }
            }
        }

        std::vector<RuntimeInventoryItem> readInventory(BinaryReader& reader, std::uint32_t version)
        {
            std::vector<RuntimeInventoryItem> result;
            const std::uint32_t count = reader.count();
            result.reserve(count);
            for (std::uint32_t i = 0; i < count; ++i)
            {
                RuntimeInventoryItem item;
                item.mBase = readKey(reader);
                item.mCount = reader.integer<std::int32_t>();
                if (version >= 4)
                {
                    item.mCondition = version >= 24 ? reader.float32()
                        : static_cast<double>(reader.integer<std::int32_t>());
                    item.mCharge = reader.float32();
                    item.mEquippedSlots = reader.integer<std::uint32_t>();
                    item.mHotkey = reader.integer<std::int8_t>();
                    item.mOwner = readKey(reader);
                    item.mRemainingUsageTime = reader.float32();
                }
                result.push_back(std::move(item));
            }
            return result;
        }

        void writeJsonInventory(
            std::ostream& stream, const std::vector<RuntimeInventoryItem>& inventory, std::uint32_t version)
        {
            stream << '[';
            for (std::size_t i = 0; i < inventory.size(); ++i)
            {
                if (i)
                    stream << ',';
                const RuntimeInventoryItem& item = inventory[i];
                stream << "{\"base\":\"" << escapeJson(item.mBase.serialize()) << "\",\"count\":"
                       << item.mCount;
                if (version >= 4)
                {
                    const double condition = version >= 24 ? static_cast<float>(item.mCondition) : item.mCondition;
                    stream << ",\"condition\":" << std::setprecision(17);
                    if (condition == 0 && std::signbit(condition)) stream << "-0.0";
                    else stream << condition;
                    stream << ",\"charge\":"
                           << std::setprecision(17) << item.mCharge << ",\"equipped_slots\":"
                           << item.mEquippedSlots << ",\"hotkey\":" << static_cast<int>(item.mHotkey)
                           << ",\"owner\":\"" << escapeJson(item.mOwner.serialize())
                           << "\",\"remaining_usage_time\":" << std::setprecision(17)
                           << item.mRemainingUsageTime;
                }
                stream << '}';
            }
            stream << ']';
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
                stream << (i == 0 ? "" : ",") << std::setprecision(17) << position.pos[i];
            for (int i = 0; i < 3; ++i)
                stream << ',' << std::setprecision(17) << position.rot[i];
            stream << ']';
        }
    }

    void RuntimePassiveAbility::validate() const
    {
        if (mSpell.isNull() || mEffects.empty())
            throw std::runtime_error("Invalid TES4 passive ability identity or empty effects");
        if (mEffects.size() > sMaximumCollectionSize)
            throw std::runtime_error("TES4 passive ability effect list exceeds the size limit");
        std::set<std::uint32_t> indices;
        for (const auto& effect : mEffects)
            if (!indices.insert(effect.mEffectIndex).second
                || !compiledPassiveValueModifierDefinition(effect.mCode)
                || effect.mActorValue >= 72 || !std::isfinite(effect.mStoredMagnitude)
                || (effect.mInitialMagnitude && !std::isfinite(*effect.mInitialMagnitude)))
                throw std::runtime_error("Invalid TES4 passive value-modifier ownership");
    }

    void RuntimeActorValues::validate() const
    {
        if (mActor.isNull() || mBase.isNull())
            throw std::runtime_error("Invalid TES4 native actor-value identity");
        if (mProcessAction && mProcess != ActorValueProcess::Active)
            throw std::runtime_error("TES4 native action code requires an Active process");
        if (mProcessKnockedState && mProcess != ActorValueProcess::Active)
            throw std::runtime_error("TES4 native knocked byte requires an Active process");
        if (mPlayerFormValues && mOwner != ActorValueOwner::Player)
            throw std::runtime_error("TES4 player form values require player ownership");
        if (mNonPlayerFormHealth && (mOwner != ActorValueOwner::NonPlayer
            || mValues[8].mBase != static_cast<float>(*mNonPlayerFormHealth)))
            throw std::runtime_error("TES4 nonplayer form Health conflicts with owner or resolved float base");
        if (mPassiveAbilities)
        {
            if (mPassiveAbilities->size() > sMaximumCollectionSize)
                throw std::runtime_error("TES4 passive ability ownership list exceeds the size limit");
            std::set<ESM::FormKey> spells;
            for (const auto& ability : *mPassiveAbilities)
            {
                ability.validate();
                if (!spells.insert(ability.mSpell).second)
                    throw std::runtime_error("Duplicate TES4 passive ability ownership");
            }
        }
        try
        {
            for (const auto& value : mValues)
                composeActorValue(value, mOwner, mProcess);
        }
        catch (const std::invalid_argument& error)
        {
            throw std::runtime_error(std::string("Invalid TES4 native actor values: ") + error.what());
        }
    }

    void RuntimeActorBaseOverride::validate() const
    {
        if (mBase.isNull() || mValues.empty() || mValues.size() > 72)
            throw std::runtime_error("Invalid TES4 native actor base override");
        std::set<std::uint8_t> seen;
        try
        {
            for (const auto& value : mValues)
            {
                const auto shape = prepareActorBaseValueSet(mKind, value.mActorValue, 0);
                if (!shape || shape->mActorValue != value.mActorValue || shape->mValue.index() != value.mValue.index()
                    || !seen.insert(value.mActorValue).second)
                    throw std::invalid_argument("invalid or duplicate base value storage");
                if (const auto* integer = std::get_if<std::int32_t>(&value.mValue))
                {
                    if (prepareActorBaseValueSet(mKind, value.mActorValue, *integer) != value)
                        throw std::invalid_argument("base value exceeds its native storage width");
                }
                else if (!std::isfinite(std::get<float>(value.mValue)))
                    throw std::invalid_argument("nonfinite base value");
            }
        }
        catch (const std::invalid_argument& error)
        {
            throw std::runtime_error(std::string("Invalid TES4 native actor base override: ") + error.what());
        }
    }

    void RuntimeActorRagdoll::validate() const
    {
        if (mBase.isNull() || mModel.empty() || mModel.size() > 4096
            || mModel.front() == '/' || mModel.back() == '/'
            || mModel.find("//") != std::string::npos
            || std::any_of(mModel.begin(), mModel.end(), [](unsigned char c) {
                return c < 32 || c >= 127 || c == '\\' || c == ':' || (c >= 'A' && c <= 'Z');
            }))
            throw std::runtime_error("Invalid TES4 ragdoll model identity");
        for (std::size_t start = 0; start < mModel.size();)
        {
            const auto end = mModel.find('/', start);
            const auto component = std::string_view(mModel).substr(start,
                end == std::string::npos ? mModel.size() - start : end - start);
            if (component == "." || component == "..")
                throw std::runtime_error("Noncanonical TES4 ragdoll model path");
            if (end == std::string::npos)
                break;
            start = end + 1;
        }
        if (mAssetHash.size() != 32 || std::any_of(mAssetHash.begin(), mAssetHash.end(), [](char c) {
                return !(c >= '0' && c <= '9') && !(c >= 'a' && c <= 'f');
            }) || mBodies.empty() || mBodies.size() > sMaximumCollectionSize)
            throw std::runtime_error("Invalid TES4 ragdoll asset hash or body count");
        std::optional<std::uint32_t> previous;
        std::set<std::uint32_t> nodes;
        for (const auto& body : mBodies)
        {
            if (body.mNativePackedVelocity.has_value() != mBodies.front().mNativePackedVelocity.has_value())
                throw std::runtime_error("Incomplete TES4 packed ragdoll snapshot");
            if (body.mNativeMotion.has_value() != mBodies.front().mNativeMotion.has_value())
                throw std::runtime_error("Incomplete TES4 ragdoll motion snapshot");
            if (body.mNativeMotion && (!body.mNativePackedVelocity
                || (*body.mNativeMotion != RuntimeRagdollMotion::Dynamic
                    && *body.mNativeMotion != RuntimeRagdollMotion::Keyframed)))
                throw std::runtime_error("Invalid TES4 logical ragdoll motion snapshot");
            if (body.mNativePackedVelocity)
                for (const auto& vector : {body.mNativePackedVelocity->mLinear, body.mNativePackedVelocity->mAngular})
                    for (float value : vector)
                        if (!std::isfinite(value))
                            throw std::runtime_error("Nonfinite TES4 packed ragdoll velocity");
            if ((previous && body.mRecord <= *previous)
                || body.mRecord > std::numeric_limits<std::int32_t>::max()
                || body.mNodeRecord > std::numeric_limits<std::int32_t>::max()
                || !nodes.insert(body.mNodeRecord).second)
                throw std::runtime_error("Invalid TES4 ragdoll body identity or order");
            previous = body.mRecord;
            for (float component : body.mRotation)
                if (!std::isfinite(component))
                    throw std::runtime_error("Nonfinite TES4 ragdoll rotation");
            for (const auto& vector : {body.mPosition, body.mLinearVelocity, body.mAngularVelocity})
                for (float component : vector)
                    if (!std::isfinite(component))
                        throw std::runtime_error("Nonfinite TES4 ragdoll position or velocity");
            const auto& r = body.mRotation;
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned other = row; other < 3; ++other)
                {
                    double dot = 0;
                    for (unsigned column = 0; column < 3; ++column)
                        dot += double(r[row * 3 + column]) * r[other * 3 + column];
                    if (std::abs(dot - (row == other ? 1.0 : 0.0)) > 1e-4)
                        throw std::runtime_error("Nonrigid TES4 ragdoll rotation");
                }
            const double determinant = double(r[0]) * (double(r[4]) * r[8] - double(r[5]) * r[7])
                - double(r[1]) * (double(r[3]) * r[8] - double(r[5]) * r[6])
                + double(r[2]) * (double(r[3]) * r[7] - double(r[4]) * r[6]);
            if (std::abs(determinant - 1.0) > 1e-4)
                throw std::runtime_error("Improper TES4 ragdoll rotation");
        }
        if (mNativeBlends)
        {
            if (!mBodies.front().mNativePackedVelocity || !mBodies.front().mNativeMotion
                || mNativeBlends->size() > mBodies.size())
                throw std::runtime_error("Incomplete TES4 native blend snapshot");
            std::optional<std::uint32_t> previousBlend;
            for (const auto& blend : *mNativeBlends)
            {
                const auto body = std::lower_bound(mBodies.begin(), mBodies.end(), blend.mBodyRecord,
                    [](const RuntimeRagdollBody& value, std::uint32_t record) { return value.mRecord < record; });
                if ((previousBlend && blend.mBodyRecord <= *previousBlend)
                    || body == mBodies.end() || body->mRecord != blend.mBodyRecord
                    || !std::isfinite(blend.mHierarchyGain) || !std::isfinite(blend.mVelocityGain))
                    throw std::runtime_error("Invalid TES4 native blend identity, order or gain");
                previousBlend = blend.mBodyRecord;
            }
        }
        if (mNativeControllers)
        {
            if (!mNativeBlends || !mBodies.front().mNativePackedVelocity || !mBodies.front().mNativeMotion
                || mNativeControllers->mBlends.size() > mBodies.size()
                || mNativeControllers->mVelocities.size() > mBodies.size())
                throw std::runtime_error("Incomplete TES4 ragdoll controller snapshot");
            const auto common = [](const PhysicalBlendTiming& timing, const PhysicalBlendClock& clock) {
                for (float value : {timing.mFrequency, timing.mPhase, timing.mStartKey, timing.mStopKey,
                        clock.mStartTime, clock.mPreviousTime, clock.mElapsed})
                    if (!std::isfinite(value))
                        throw std::runtime_error("Nonfinite TES4 ragdoll controller timing or clock");
            };
            std::optional<std::uint32_t> previousController;
            std::set<std::uint32_t> attachments;
            for (const auto& controller : mNativeControllers->mBlends)
            {
                if ((previousController && controller.mRecord <= *previousController)
                    || controller.mRecord > std::numeric_limits<std::int32_t>::max()
                    || !nodes.contains(controller.mAttachedNode) || !attachments.insert(controller.mAttachedNode).second
                    || (controller.mTargetNode && !nodes.contains(*controller.mTargetNode)))
                    throw std::runtime_error("Invalid TES4 authored controller identity or order");
                previousController = controller.mRecord;
                const auto& state = controller.mState;
                common(state.mTiming, state.mClock);
                if (!std::isfinite(state.mCachedGains.mHierarchy) || !std::isfinite(state.mCachedGains.mVelocity)
                    || state.mKeys.size() > sMaximumCollectionSize
                    || (state.mKeys.size() >= 2 && state.mCursor >= state.mKeys.size() - 1))
                    throw std::runtime_error("Invalid TES4 authored controller cache or cursor");
                for (std::size_t i = 0; i < state.mKeys.size(); ++i)
                {
                    const auto& key = state.mKeys[i];
                    if (!std::isfinite(key.mTime) || !std::isfinite(key.mGains.mHierarchy)
                        || !std::isfinite(key.mGains.mVelocity) || (i != 0 && state.mKeys[i - 1].mTime > key.mTime))
                        throw std::runtime_error("Invalid TES4 authored controller key or order");
                }
            }
            previousController.reset();
            for (const auto& controller : mNativeControllers->mVelocities)
            {
                if ((previousController && controller.mAttachedNode <= *previousController)
                    || !nodes.contains(controller.mAttachedNode)
                    || (controller.mTargetNode && !nodes.contains(*controller.mTargetNode)))
                    throw std::runtime_error("Invalid TES4 generated controller identity or order");
                previousController = controller.mAttachedNode;
                const auto& state = controller.mState;
                common(state.mTiming, state.mClock);
                if (state.mTiming.mStartKey > state.mTiming.mStopKey
                    || !std::isfinite(state.mFrameDelta) || state.mFrameDelta < 0
                    || std::any_of(state.mForceVector.begin(), state.mForceVector.end(),
                        [](float value) { return !std::isfinite(value); }))
                    throw std::runtime_error("Invalid TES4 generated controller timing, delta or force");
            }
        }
    }

    void RuntimeActorLife::validate() const
    {
        if (mActor.isNull() || mBase.isNull()
            || (mPhase != ActorLifePhase::Alive && mPhase != ActorLifePhase::Dead
                && mPhase != ActorLifePhase::EssentialUnconscious)
            || !std::isfinite(mRecoveryRemaining) || mRecoveryRemaining < 0
            || (mPhase != ActorLifePhase::EssentialUnconscious && mRecoveryRemaining != 0)
            || (mPhase == ActorLifePhase::Alive && !mKiller.isNull()))
            throw std::runtime_error("Invalid TES4 native actor life state");
    }

    void RuntimeMeleeInput::validate() const
    {
        if (!std::isfinite(mHeldSeconds) || mHeldSeconds < 0
            || static_cast<unsigned>(mQueued) > static_cast<unsigned>(MeleeQueuedStrike::Power))
            throw std::runtime_error("Invalid TES4 melee input state");
    }

    void RuntimeMeleeStrike::validate() const
    {
        if (mActionId == 0 || mAnimationGroup.empty() || mAnimationGroup.size() > sMaximumStringSize
            || mAnimationGroup.find('\0') != std::string::npos || !std::isfinite(mPlaybackSpeed) || mPlaybackSpeed <= 0
            || !std::isfinite(mAnimationTime) || mAnimationTime < 0
            || static_cast<unsigned>(mKind) > static_cast<unsigned>(MeleeStrikeKind::RightPower)
            || static_cast<unsigned>(mOrdinaryPhase) > static_cast<unsigned>(OrdinaryMeleePhase::End))
            throw std::runtime_error("Invalid TES4 melee strike state");
        if (mSequenceTiming)
        {
            const auto& timing = *mSequenceTiming;
            if (!std::isfinite(timing.mEaseEnd) || !std::isfinite(timing.mWeightedTime)
                || !std::isfinite(timing.mOutputTime)
                || timing.mOffset.has_value() != timing.mEaseStart.has_value()
                || timing.mOffset.has_value() != timing.mLastInput.has_value())
                throw std::runtime_error("Invalid TES4 melee sequence timing");
            for (const auto value : {timing.mOffset, timing.mEaseStart, timing.mLastInput})
                if (value && !std::isfinite(*value))
                    throw std::runtime_error("Nonfinite TES4 melee sequence timing");
        }
    }

    void RuntimeMeleeAiIntent::validate() const
    {
        if (mTarget.isNull() || mStyle.isNull())
            throw std::runtime_error("Null TES4 melee AI target or style");
    }

    void RuntimeMeleeState::validate() const
    {
        mInput.validate();
        if (mStrike) mStrike->validate();
        if (mAiIntent) mAiIntent->validate();
    }

    void RuntimeState::validate() const
    {
        if (mVersion < 1 || mVersion > CurrentRuntimeStateVersion)
            throw std::runtime_error("Unsupported TES4 runtime-state version " + std::to_string(mVersion));
        if (mProfile != ESM::GameProfile::Oblivion)
            throw std::runtime_error("TES4 runtime state requires the Oblivion game profile");
        if (mNativePlayerBowTimer)
        {
            if (mVersion < 37 || !std::isfinite(*mNativePlayerBowTimer))
                throw std::runtime_error("Invalid native Player bow timer version or value");
        }
        if (mNativePhysicalBlendTimeCache)
        {
            if (mVersion < 36)
                throw std::runtime_error("Native physical cache requires runtime schema36");
            const auto& cache = *mNativePhysicalBlendTimeCache;
            for (float value : {cache.mStopKey, cache.mStartKey, cache.mKeyTime, cache.mResult})
                if (!std::isfinite(value))
                    throw std::runtime_error("Nonfinite native physical cache state");
        }
        if (mVersion < 27 && mCombatRngState != 1)
            throw std::runtime_error("Native combat random state requires runtime schema27");
        if (mNextDynamicSerial == 0)
            throw std::runtime_error("TES4 runtime-state dynamic serial must be non-zero");
        if (!validCalendar(CalendarInstant{ mClock.mYear, mClock.mMonth, mClock.mDay, mClock.mHour })
            || !std::isfinite(mClock.mTimeScale))
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
        checkSize(mActorAi.size(), "actor AI list");
        checkSize(mPathPoints.size(), "path-point overlay list");
        checkSize(mCompanions.size(), "companion relation list");
        checkSize(mMounts.size(), "mount relation list");
        checkSize(mDetectionVectors.size(), "detection vector list");
        checkSize(mPendingPackageDone.size(), "pending package completion list");
        checkSize(mPhysicalActions.mPending.size(), "pending physical action list");
        checkSize(mPhysicalActionOwners.size(), "physical action owner list");
        checkSize(mNativeMeleeStates.size(), "native melee state list");
        checkSize(mNativeAnimationClocks.size(), "native animation clock list");
        checkSize(mNativeActorValues.size(), "native actor-value list");
        checkSize(mNativeActorBases.size(), "native actor-base list");
        if (mVersion < 11 && !mNativeActorBases.empty())
            throw std::runtime_error("TES4 native actor base overrides require runtime-state version 11");
        std::set<ESM::FormKey> overriddenBases;
        for (const auto& base : mNativeActorBases)
        {
            base.validate();
            if (!overriddenBases.insert(base.mBase).second)
                throw std::runtime_error("Duplicate TES4 native actor base override");
        }
        if (mVersion < 9 && !mNativeActorValues.empty())
            throw std::runtime_error("TES4 runtime-state versions before 9 cannot contain native actor values");
        std::map<ESM::FormKey, ESM::FormKey> actorBases;
        for (const auto& reference : mReferences)
            actorBases.emplace(reference.mKey, reference.mBase);
        std::map<ESM::FormKey, ESM::FormKey> nativeActors;
        for (const auto& actor : mNativeActorValues)
        {
            actor.validate();
            if (mVersion < 26 && actor.mProcessAction)
                throw std::runtime_error("TES4 native action code requires runtime-state version 26");
            if (mVersion < 25 && actor.mProcessKnockedState)
                throw std::runtime_error("TES4 native knocked byte requires runtime-state version 25");
            if (mVersion < 10 && actor.mPlayerFormValues)
                throw std::runtime_error("TES4 player form values require runtime-state version 10");
            if (mVersion < 18 && actor.mPassiveAbilities)
                throw std::runtime_error("TES4 passive ability ownership requires runtime-state version 18");
            if (mVersion < 19 && actor.mPassiveAbilities)
                for (const auto& ability : *actor.mPassiveAbilities)
                    for (const auto& effect : ability.mEffects)
                        if (effect.mInitialMagnitude)
                            throw std::runtime_error("TES4 passive initial magnitude requires runtime-state version 19");
            if (mVersion < 17 && actor.mNonPlayerFormHealth)
                throw std::runtime_error("TES4 nonplayer form Health requires runtime-state version 17");
            if (!nativeActors.emplace(actor.mActor, actor.mBase).second)
                throw std::runtime_error("Duplicate TES4 native actor-value identity");
            const bool player = actor.mActor == mPlayer.mReference;
            if ((actor.mOwner == ActorValueOwner::Player) != player)
                throw std::runtime_error("TES4 native actor-value player identity mismatch");
            if (!player)
            {
                const auto reference = actorBases.find(actor.mActor);
                if (reference == actorBases.end() || reference->second != actor.mBase)
                    throw std::runtime_error("Dangling or mismatched TES4 native actor-value reference");
            }
        }
        checkSize(mNativeActorUpdateTimes.size(), "native actor update time map");
        if (mVersion < 16 && (mNativeActorManagerTime != 0.f || !mNativeActorUpdateTimes.empty()))
            throw std::runtime_error("TES4 native actor clocks require runtime-state version 16");
        if (!std::isfinite(mNativeActorManagerTime) || mNativeActorManagerTime > 100000.f)
            throw std::runtime_error("Invalid TES4 native actor manager time");
        for (const auto& [actor, time] : mNativeActorUpdateTimes)
            if (!nativeActors.contains(actor) || !std::isfinite(time) || time > 100000.f)
                throw std::runtime_error("Invalid or dangling TES4 native actor update time");
        checkSize(mNativeActorBreath.size(), "native actor breath map");
        if (mVersion < 14 && !mNativeActorBreath.empty())
            throw std::runtime_error("TES4 native actor breath requires runtime-state version 14");
        for (const auto& [actor, remaining] : mNativeActorBreath)
            if (!nativeActors.contains(actor) || !std::isfinite(remaining))
                throw std::runtime_error("Invalid or dangling TES4 native actor breath");
        checkSize(mNativeDeathCounts.size(), "native death count map");
        if (mVersion < 13 && !mNativeDeathCounts.empty())
            throw std::runtime_error("TES4 native death counts require runtime-state version 13");
        for (const auto& [base, count] : mNativeDeathCounts)
            if (base.isNull())
                throw std::runtime_error("Invalid TES4 native death count base");
        checkSize(mNativeActorLife.size(), "native actor life list");
        checkSize(mPendingDeathEvents.size(), "pending death event list");
        if (mVersion < 12 && (!mNativeActorLife.empty() || !mPendingDeathEvents.empty() || mNextDeathEvent != 1))
            throw std::runtime_error("TES4 native actor lifecycle requires runtime-state version 12");
        if (mNextDeathEvent == 0)
            throw std::runtime_error("Invalid TES4 next death event identity");
        const auto knownSource = [&](const ESM::FormKey& source) {
            return source.isNull() || source == mPlayer.mReference || actorBases.contains(source);
        };
        std::map<ESM::FormKey, const RuntimeActorLife*> lives;
        for (const auto& life : mNativeActorLife)
        {
            life.validate();
            if (!lives.emplace(life.mActor, &life).second)
                throw std::runtime_error("Duplicate TES4 native actor life identity");
            const auto reference = actorBases.find(life.mActor);
            if ((life.mActor != mPlayer.mReference && (reference == actorBases.end() || reference->second != life.mBase))
                || !knownSource(life.mKiller))
                throw std::runtime_error("Dangling or mismatched TES4 native actor life reference");
            const auto values = nativeActors.find(life.mActor);
            if (values != nativeActors.end() && values->second != life.mBase)
                throw std::runtime_error("TES4 native actor life and value base conflict");
        }
        checkSize(mNativeCombatEngagements.size(), "native combat engagement set");
        for (const auto& reference : mReferences)
            if (reference.mActorDrawState)
            {
                if (mVersion < 29)
                    throw std::runtime_error("TES4 native actor draw state requires runtime-state version 29");
                if (static_cast<std::uint8_t>(*reference.mActorDrawState)
                    > static_cast<std::uint8_t>(ActorDrawState::Spell))
                    throw std::runtime_error("Invalid TES4 native actor draw state");
                const auto values = nativeActors.find(reference.mKey);
                const auto life = lives.find(reference.mKey);
                if (reference.mKey == mPlayer.mReference || values == nativeActors.end()
                    || values->second != reference.mBase || life == lives.end()
                    || life->second->mBase != reference.mBase)
                    throw std::runtime_error("Dangling or mismatched TES4 native actor draw state owner");
            }
        if (mVersion < 15 && !mNativeCombatEngagements.empty())
            throw std::runtime_error("TES4 native combat engagements require runtime-state version 15");
        for (const auto& [first, second] : mNativeCombatEngagements)
        {
            const auto activeActor = [&](const ESM::FormKey& actor) {
                const auto life = lives.find(actor);
                return nativeActors.contains(actor) && life != lives.end()
                    && life->second->mPhase != ActorLifePhase::Dead;
            };
            if (!(first < second) || !activeActor(first) || !activeActor(second))
                throw std::runtime_error("Invalid, dangling or terminal TES4 combat engagement");
        }
        for (const auto& reference : mReferences)
            if (const auto life = lives.find(reference.mKey); life != lives.end())
                if (const auto old = reference.mCustomState.find("obscript.dead"); old != reference.mCustomState.end())
                {
                    const auto* dead = std::get_if<bool>(&old->second);
                    if (!dead || *dead != (life->second->mPhase == ActorLifePhase::Dead))
                        throw std::runtime_error("TES4 native actor life conflicts with legacy obscript.dead");
                }
        std::uint64_t previousEvent = 0;
        for (const auto& event : mPendingDeathEvents)
        {
            if (event.mId <= previousEvent || event.mId >= mNextDeathEvent || !lives.contains(event.mActor)
                || !knownSource(event.mKiller))
                throw std::runtime_error("Invalid TES4 pending death event identity, order or reference");
            previousEvent = event.mId;
        }
        if (mVersion < 8 && mPhysicalActions != ActionLedgerState{})
            throw std::runtime_error("TES4 runtime-state versions before 8 cannot contain physical actions");
        try
        {
            ActionLedger{}.restore(mPhysicalActions);
        }
        catch (const std::invalid_argument& error)
        {
            throw std::runtime_error(std::string("Invalid TES4 physical actions: ") + error.what());
        }
        if (mVersion < 20 && !mPhysicalActionOwners.empty())
            throw std::runtime_error("TES4 physical action owners require runtime-state version 20");
        const std::set<std::uint64_t> pendingActions(mPhysicalActions.mPending.begin(), mPhysicalActions.mPending.end());
        for (const auto& [id, actor] : mPhysicalActionOwners)
        {
            const auto life = lives.find(actor);
            if (!pendingActions.contains(id) || !nativeActors.contains(actor) || life == lives.end()
                || life->second->mPhase != ActorLifePhase::Alive)
                throw std::runtime_error("Invalid, dangling or incapacitated TES4 physical action owner");
        }
        if (mVersion < 21 && !mNativeMeleeStates.empty())
            throw std::runtime_error("TES4 melee state requires runtime-state version 21");
        if (mVersion < 23 && !mNativeAnimationClocks.empty())
            throw std::runtime_error("TES4 animation clocks require runtime-state version23");
        checkSize(mNativeActorKnockback.size(), "native actor knockback list");
        checkSize(mNativeActorRagdolls.size(), "native actor ragdoll list");
        if (mVersion < 31 && !mNativeActorRagdolls.empty())
            throw std::runtime_error("TES4 actor ragdolls require runtime-state version31");
        for (const auto& [actor, pose] : mNativeActorRagdolls)
        {
            pose.validate();
            if (mVersion < 32 && pose.mBodies.front().mNativePackedVelocity)
                throw std::runtime_error("TES4 packed ragdoll velocities require runtime-state version32");
            if (mVersion < 33 && pose.mBodies.front().mNativeMotion)
                throw std::runtime_error("TES4 logical ragdoll motion requires runtime-state version33");
            if (mVersion < 34 && pose.mNativeBlends)
                throw std::runtime_error("TES4 native blend snapshots require runtime-state version34");
            if (mVersion < 35 && pose.mNativeControllers)
                throw std::runtime_error("TES4 ragdoll controllers require runtime-state version35");
            const auto values = nativeActors.find(actor);
            if (values == nativeActors.end() || !lives.contains(actor) || values->second != pose.mBase)
                throw std::runtime_error("Dangling or mismatched TES4 ragdoll owner");
        }
        if (mVersion < 30 && !mNativeActorKnockback.empty())
            throw std::runtime_error("TES4 actor knockback requires runtime-state version30");
        for (const auto& [actor, pulse] : mNativeActorKnockback)
        {
            if (!nativeActors.contains(actor) || !lives.contains(actor)
                || !std::isfinite(pulse.mRemaining) || pulse.mRemaining < 0)
                throw std::runtime_error("Invalid or dangling TES4 native actor knockback");
            for (float component : pulse.mAcceleration)
                if (!std::isfinite(component))
                    throw std::runtime_error("Nonfinite TES4 native actor knockback");
        }
        for (const auto& [actor, clock] : mNativeAnimationClocks)
            if (!nativeActors.contains(actor) || !lives.contains(actor) || !std::isfinite(clock) || clock < 0)
                throw std::runtime_error("Invalid or dangling TES4 native animation clock");
        std::set<std::uint64_t> meleeIds;
        for (const auto& [actor, melee] : mNativeMeleeStates)
        {
            melee.validate();
            const auto life = lives.find(actor);
            if (!nativeActors.contains(actor) || life == lives.end() || life->second->mPhase != ActorLifePhase::Alive)
                throw std::runtime_error("Dangling or incapacitated TES4 melee state owner");
            if (melee.mAiIntent)
            {
                const auto& target = melee.mAiIntent->mTarget;
                const auto targetLife = lives.find(target);
                const auto pair = actor < target ? std::make_pair(actor, target) : std::make_pair(target, actor);
                if (mVersion < 28 || actor == mPlayer.mReference || target == actor
                    || !nativeActors.contains(target) || targetLife == lives.end()
                    || targetLife->second->mPhase != ActorLifePhase::Alive
                    || !mNativeCombatEngagements.contains(pair))
                    throw std::runtime_error("Invalid, dangling or unsupported TES4 melee AI intent");
            }
            if (melee.mStrike)
            {
                const auto& strike = *melee.mStrike;
                if (mVersion < 22 && strike.mOrdinaryPhase != OrdinaryMeleePhase::Start)
                    throw std::runtime_error("TES4 ordinary melee phase requires runtime-state version 22");
                if (strike.mSequenceTiming && (mVersion < 23 || !mNativeAnimationClocks.contains(actor)))
                    throw std::runtime_error("TES4 sequence timing requires version23 and an actor clock");
                const auto owner = mPhysicalActionOwners.find(strike.mActionId);
                const bool pending = pendingActions.contains(strike.mActionId);
                if (!meleeIds.insert(strike.mActionId).second || strike.mActionId >= mPhysicalActions.mNext
                    || (strike.mContactCommitted ? pending
                        : !pending || owner == mPhysicalActionOwners.end() || owner->second != actor))
                    throw std::runtime_error("Invalid, duplicate or replaying TES4 melee action identity");
            }
        }
        if (mVersion < 6 && !mPendingPackageDone.empty())
            throw std::runtime_error("TES4 runtime-state versions before 6 cannot contain pending package events");
        for (const RuntimePackageDoneEvent& event : mPendingPackageDone)
            if (event.mActor.isNull() || event.mPackage.isNull())
                throw std::runtime_error("Invalid TES4 pending package completion identity");
        if (mVersion < 2 && (mScriptEventSequence != 0 || !mScriptInstances.empty() || !mQuests.empty()))
            throw std::runtime_error("TES4 runtime-state version 1 cannot contain ObScript state");
        if (mVersion < 5 && !mDetectionVectors.empty())
            throw std::runtime_error("TES4 runtime-state version 1/2/3/4 cannot contain detection vectors");
        if (mVersion < 3 && (!mPlayer.mName.empty() || !mPlayer.mRace.isNull() || !mPlayer.mClass.isNull()
                || !mPlayer.mBirthSign.isNull() || mPlayer.mFemale || mPlayer.mCharacterGenerationFlags != 0))
            throw std::runtime_error("TES4 runtime-state version 1/2 cannot contain character-generation state");
        if (mVersion >= 3
            && (mPlayer.mName.size() > 1024 || mPlayer.mRace.isNull() || mPlayer.mClass.isNull()))
            throw std::runtime_error("Invalid TES4 runtime-state character-generation state");
        if (mVersion >= 3 && mPlayer.mCharacterGenerationFlags > 0x1f)
            throw std::runtime_error("Invalid TES4 runtime-state character-generation flags");

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
        const auto validateInventory = [&](const std::vector<RuntimeInventoryItem>& inventory,
                                           std::string_view label, bool actorInventory) {
            std::uint32_t occupiedSlots = 0;
            std::uint8_t occupiedHotkeys = 0;
            for (const RuntimeInventoryItem& item : inventory)
            {
                if (item.mBase.isNull() || item.mCount == 0 || (mVersion >= 4 && item.mCount < 0))
                    throw std::runtime_error("Invalid TES4 runtime-state " + std::string(label) + " entry");
                if (mVersion < 4)
                {
                    if (item.mCondition != -1 || item.mCharge != -1.f || item.mEquippedSlots != 0
                        || item.mHotkey != -1 || !item.mOwner.isNull() || item.mRemainingUsageTime != -1.f)
                        throw std::runtime_error("TES4 runtime-state version 1/2/3 cannot contain M13 item state");
                    continue;
                }
                if (!std::isfinite(item.mCondition) || (item.mCondition < 0 && item.mCondition != -1.f)
                    || item.mCondition > std::numeric_limits<float>::max()
                    || !std::isfinite(item.mCharge) || item.mCharge < -1.f
                    || !std::isfinite(item.mRemainingUsageTime) || item.mRemainingUsageTime < -1.f
                    || (item.mEquippedSlots & ~0x7ffffu) != 0 || item.mHotkey < -1 || item.mHotkey > 7)
                    throw std::runtime_error("Invalid TES4 runtime-state " + std::string(label) + " metadata");
                if (mVersion < 24 && (double(item.mCondition) > std::numeric_limits<std::int32_t>::max()
                    || std::trunc(item.mCondition) != item.mCondition
                    || (item.mCondition == 0 && std::signbit(item.mCondition))))
                    throw std::runtime_error("Fractional TES4 condition requires runtime-state version24");
                if (!actorInventory && item.mHotkey != -1)
                    throw std::runtime_error("TES4 reference inventory cannot contain player hotkeys");
                if ((occupiedSlots & item.mEquippedSlots) != 0)
                    throw std::runtime_error("Conflicting TES4 runtime-state equipped slots");
                occupiedSlots |= item.mEquippedSlots;
                if (item.mHotkey >= 0)
                {
                    const std::uint8_t bit = static_cast<std::uint8_t>(1u << item.mHotkey);
                    if ((occupiedHotkeys & bit) != 0)
                        throw std::runtime_error("Duplicate TES4 runtime-state inventory hotkey");
                    occupiedHotkeys |= bit;
                }
            }
        };
        validateInventory(mPlayer.mInventory, "player inventory", true);

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
            validateInventory(reference.mInventory, "reference inventory", false);
            for (const auto& [name, value] : reference.mCustomState)
            {
                if (name.empty())
                    throw std::runtime_error("TES4 runtime-state custom-state key is empty");
                validateValue(value);
                if (name == "obscript.look_target")
                {
                    const auto* saved = std::get_if<std::string>(&value);
                    try
                    {
                        if (saved == nullptr || *saved == "null"
                            || ESM::FormKey::deserialize(*saved).serialize() != *saved)
                            throw std::invalid_argument("expected a canonical non-null FormKey");
                    }
                    catch (const std::invalid_argument&)
                    {
                        throw std::runtime_error("Invalid TES4 scripted Look target");
                    }
                }
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

        if (mVersion >= 5)
        {
            if (mAiRngState == 0)
                throw std::runtime_error("TES4 runtime-state AI RNG state must be non-zero");

            std::set<ESM::FormKey> actorKeys;
            std::map<ESM::FormKey, const RuntimeActorAiState*> actorAiByKey;
            for (const RuntimeActorAiState& actor : mActorAi)
            {
                if (actor.mActor.isNull() || actor.mBase.isNull() || actor.mCell.isNull()
                    || !actorKeys.insert(actor.mActor).second)
                    throw std::runtime_error("Invalid or duplicate TES4 runtime-state actor AI identity");
                actorAiByKey.emplace(actor.mActor, &actor);
                if (!actor.mCompanionSideWith.isNull() && actor.mCompanionSideWith == actor.mActor)
                    throw std::runtime_error("TES4 actor companion side-with points to itself");
                if (actor.mSource == PackageSource::None)
                {
                    if (!actor.mPackage.isNull() || actor.mPackageType != AIPackageType::Unknown
                        || actor.mProcedure != PackageProcedure::None)
                        throw std::runtime_error("TES4 idle actor AI state contains a package");
                }
                else if (actor.mPackage.isNull() || actor.mPackageType == AIPackageType::Unknown
                    || actor.mProcedure != packageProcedure(actor.mPackageType))
                    throw std::runtime_error("TES4 active actor AI state has an invalid package identity");
                if (actor.mSource == PackageSource::Script && actor.mScriptPackage.isNull())
                    throw std::runtime_error("TES4 script-owned actor AI state has no script package");
                if (actor.mPathgrid.isNull() && actor.mPathNode != 0)
                    throw std::runtime_error("TES4 actor AI state has a node without a pathgrid");
                if (actor.mFormationIndex < -1 || actor.mRepathAttempts > 8
                    || actor.mInterruptionReason.size() > 1024)
                    throw std::runtime_error("Invalid TES4 actor AI counters: " + actor.mActor.serialize()
                        + " repath=" + std::to_string(actor.mRepathAttempts)
                        + " formation=" + std::to_string(actor.mFormationIndex)
                        + " interruption_size=" + std::to_string(actor.mInterruptionReason.size()));
                if (actor.mDoorAnimationStarted
                    && (mVersion < 7 || actor.mPhase != PackagePhase::Door || actor.mDoor.isNull()))
                    throw std::runtime_error(
                        "Invalid TES4 actor AI door-animation state: " + actor.mActor.serialize());
                const auto validTimer = [&](float value, std::string_view name) {
                    if (!std::isfinite(value) || value < 0.f)
                        throw std::runtime_error("Invalid TES4 actor AI " + std::string(name) + ": "
                            + std::to_string(value) + " for " + actor.mActor.serialize());
                };
                validTimer(actor.mActionTimer, "action timer");
                validTimer(actor.mDurationRemaining, "duration");
                validTimer(actor.mNoProgressSeconds, "no-progress timer");
                validTimer(actor.mDoorCooldown, "door cooldown");
                validTimer(actor.mLowProcessTimer, "low-process timer");
                validTimer(actor.mNextLowProcessTick, "next low-process tick");
                validatePosition(actor.mDestinationPosition);
                validatePosition(actor.mLastValidPosition);
                if (actor.mHasDestination && actor.mDestinationCell.isNull())
                    throw std::runtime_error("TES4 actor AI destination has no destination cell");
                if (static_cast<std::uint8_t>(actor.mConditionResult)
                    > static_cast<std::uint8_t>(ConditionResult::Unsupported))
                    throw std::runtime_error("Invalid TES4 actor AI condition result");
                if (actor.mScheduleWindow
                    && (!validCalendar(actor.mScheduleWindow->mStart)
                        || !validCalendar(actor.mScheduleWindow->mEnd)
                        || !std::isfinite(actor.mScheduleWindow->mDurationHours)
                        || actor.mScheduleWindow->mDurationHours < 0.0))
                    throw std::runtime_error("Invalid TES4 actor AI schedule window");
                if (static_cast<std::uint8_t>(actor.mSource) > static_cast<std::uint8_t>(PackageSource::Script)
                    || (actor.mSource != PackageSource::None
                        && static_cast<std::uint8_t>(actor.mPackageType)
                        > static_cast<std::uint8_t>(AIPackageType::Pursue))
                    || static_cast<std::uint16_t>(actor.mProcedure) > static_cast<std::uint16_t>(PackageProcedure::Pursue)
                    || static_cast<std::uint8_t>(actor.mPhase) > static_cast<std::uint8_t>(PackagePhase::Stalled)
                    || static_cast<std::uint8_t>(actor.mTier) > static_cast<std::uint8_t>(ProcessTier::Low)
                    || static_cast<std::uint8_t>(actor.mBoundary) > static_cast<std::uint8_t>(PhaseBoundary::ReadyForDialogue))
                    throw std::runtime_error("Invalid TES4 actor AI enum value");
            }

            std::set<std::pair<ESM::FormKey, std::uint32_t>> pathPoints;
            for (const RuntimePathPointState& point : mPathPoints)
            {
                if (point.mPathgrid.isNull() || !pathPoints.emplace(point.mPathgrid, point.mNode).second)
                    throw std::runtime_error("Invalid or duplicate TES4 path-point overlay");
            }

            std::set<std::pair<ESM::FormKey, ESM::FormKey>> companionPairs;
            std::map<ESM::FormKey, std::vector<ESM::FormKey>> companionEdges;
            for (const RuntimeCompanionRelation& relation : mCompanions)
            {
                if (relation.mLeader.isNull() || relation.mMember.isNull() || relation.mGroup.isNull()
                    || relation.mLeader == relation.mMember
                    || (relation.mFormationIndex < -1)
                    || (!relation.mSideWith.isNull() && relation.mSideWith == relation.mLeader)
                    || !companionPairs.emplace(relation.mLeader, relation.mMember).second)
                    throw std::runtime_error("Invalid or duplicate TES4 companion relation");
                if (const auto member = actorAiByKey.find(relation.mMember); member != actorAiByKey.end())
                {
                    if ((!member->second->mTarget.isNull() && member->second->mTarget != relation.mLeader)
                        || (!member->second->mCompanionGroup.isNull()
                            && member->second->mCompanionGroup != relation.mGroup)
                        || member->second->mCompanionSideWith != relation.mSideWith)
                        throw std::runtime_error("TES4 companion relation is not reciprocal in actor AI state");
                }
                companionEdges[relation.mLeader].push_back(relation.mMember);
            }
            std::map<ESM::FormKey, std::uint8_t> visit;
            std::function<bool(const ESM::FormKey&)> hasCycle = [&](const ESM::FormKey& key) -> bool {
                auto& mark = visit[key];
                if (mark == 1)
                    return true;
                if (mark == 2)
                    return false;
                mark = 1;
                const auto found = companionEdges.find(key);
                if (found != companionEdges.end())
                    for (const ESM::FormKey& member : found->second)
                        if (hasCycle(member))
                            return true;
                mark = 2;
                return false;
            };
            for (const auto& [leader, members] : companionEdges)
                if (hasCycle(leader))
                    throw std::runtime_error("TES4 companion relations contain a cycle");

            std::set<std::pair<ESM::FormKey, ESM::FormKey>> mountPairs;
            std::set<ESM::FormKey> horses;
            std::set<ESM::FormKey> riders;
            for (const RuntimeMountRelation& relation : mMounts)
            {
                if (relation.mHorse.isNull() || relation.mRider.isNull() || relation.mHorse == relation.mRider
                    || !mountPairs.emplace(relation.mHorse, relation.mRider).second
                    || !horses.insert(relation.mHorse).second || !riders.insert(relation.mRider).second)
                    throw std::runtime_error("Invalid or duplicate TES4 mount relation");

                const auto horse = actorAiByKey.find(relation.mHorse);
                const auto rider = actorAiByKey.find(relation.mRider);
                if (relation.mMounted)
                {
                    if ((horse != actorAiByKey.end() && horse->second->mRider != relation.mRider)
                        || (rider != actorAiByKey.end() && rider->second->mMount != relation.mHorse))
                        throw std::runtime_error("TES4 mounted relation is not reciprocal in actor AI state");
                }
                else if ((horse != actorAiByKey.end() && horse->second->mRider == relation.mRider)
                    || (rider != actorAiByKey.end() && rider->second->mMount == relation.mHorse))
                    throw std::runtime_error("TES4 inactive mount relation has active actor AI state");
            }

            // Actor state is authoritative while an actor is loaded.  A
            // relation may refer to an unloaded actor, so only validate the
            // reciprocal side when that side is present in this save chunk.
            for (const auto& [key, actor] : actorAiByKey)
            {
                if (!actor->mMount.isNull())
                {
                    const auto horse = actorAiByKey.find(actor->mMount);
                    if (horse != actorAiByKey.end() && horse->second->mRider != key)
                        throw std::runtime_error("TES4 actor mount state is not reciprocal");
                }
                if (!actor->mRider.isNull())
                {
                    const auto rider = actorAiByKey.find(actor->mRider);
                    if (rider != actorAiByKey.end() && rider->second->mMount != key)
                        throw std::runtime_error("TES4 actor rider state is not reciprocal");
                }
            }

            std::set<std::pair<ESM::FormKey, ESM::FormKey>> detectionPairs;
            for (const RuntimeDetectionVector& vector : mDetectionVectors)
            {
                if (vector.mObserver.isNull() || vector.mTarget.isNull() || vector.mObserver == vector.mTarget
                    || !detectionPairs.emplace(vector.mObserver, vector.mTarget).second
                    || !std::isfinite(vector.mScore) || vector.mScore < 0.0 || vector.mScore > 100.0
                    || (vector.mDetected && !vector.mLineOfSight))
                    throw std::runtime_error("Invalid or duplicate TES4 detection vector");
            }
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
        writeInventory(writer, mPlayer.mInventory, mVersion);
        if (mVersion >= 3)
        {
            writer.string(mPlayer.mName);
            writeKey(writer, mPlayer.mRace);
            writeKey(writer, mPlayer.mClass);
            writeKey(writer, mPlayer.mBirthSign);
            writer.integer<std::uint8_t>(mPlayer.mFemale ? 1 : 0);
            writer.integer(mPlayer.mCharacterGenerationFlags);
        }

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
            writeInventory(writer, reference.mInventory, mVersion);
            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(reference.mCustomState.size()));
            for (const auto& [name, value] : reference.mCustomState)
            {
                writer.string(name);
                writeValue(writer, value);
            }
            if (mVersion >= 29)
            {
                writer.integer<std::uint8_t>(reference.mActorDrawState.has_value() ? 1 : 0);
                if (reference.mActorDrawState)
                    writer.integer<std::uint8_t>(static_cast<std::uint8_t>(*reference.mActorDrawState));
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

        if (mVersion >= 5)
        {
            writer.integer(mAiRngState);

            std::vector<RuntimeActorAiState> actors = mActorAi;
            std::sort(actors.begin(), actors.end(), [](const auto& left, const auto& right) {
                return left.mActor < right.mActor;
            });
            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(actors.size()));
            for (const RuntimeActorAiState& actor : actors)
            {
                writeKey(writer, actor.mActor);
                writeKey(writer, actor.mBase);
                writeKey(writer, actor.mPackage);
                writeKey(writer, actor.mScriptPackage);
                writeKey(writer, actor.mTarget);
                writeKey(writer, actor.mTargetBase);
                writeKey(writer, actor.mCell);
                writeKey(writer, actor.mPathgrid);
                writeKey(writer, actor.mDoor);
                writeKey(writer, actor.mDestinationCell);
                writeKey(writer, actor.mLastValidCell);
                writeKey(writer, actor.mActionItem);
                writeKey(writer, actor.mLastTransitionDoor);
                writeKey(writer, actor.mCompanionGroup);
                writeKey(writer, actor.mCompanionSideWith);
                writeKey(writer, actor.mMount);
                writeKey(writer, actor.mRider);
                writer.integer<std::uint8_t>(actor.mScheduleWindow ? 1 : 0);
                if (actor.mScheduleWindow)
                {
                    writeCalendar(writer, actor.mScheduleWindow->mStart);
                    writeCalendar(writer, actor.mScheduleWindow->mEnd);
                    writer.floating(actor.mScheduleWindow->mDurationHours);
                }
                writer.integer<std::uint8_t>(static_cast<std::uint8_t>(actor.mConditionResult));
                writePosition(writer, actor.mDestinationPosition);
                writePosition(writer, actor.mLastValidPosition);
                writer.integer<std::uint8_t>(static_cast<std::uint8_t>(actor.mSource));
                writer.integer<std::uint8_t>(static_cast<std::uint8_t>(actor.mPackageType));
                writer.integer<std::uint16_t>(static_cast<std::uint16_t>(actor.mProcedure));
                writer.integer<std::uint8_t>(static_cast<std::uint8_t>(actor.mPhase));
                writer.integer<std::uint8_t>(static_cast<std::uint8_t>(actor.mTier));
                writer.integer<std::uint8_t>(static_cast<std::uint8_t>(actor.mBoundary));
                writer.integer(actor.mListIndex);
                writer.integer(actor.mPathNode);
                writer.integer(actor.mRepathAttempts);
                writer.integer(actor.mFormationIndex);
                writer.integer(actor.mSelectionGeneration);
                writer.integer(actor.mRouteGeneration);
                writer.integer(actor.mTransitionGeneration);
                writer.floating(actor.mActionTimer);
                writer.floating(actor.mDurationRemaining);
                writer.floating(actor.mNoProgressSeconds);
                writer.floating(actor.mDoorCooldown);
                writer.floating(actor.mLowProcessTimer);
                writer.floating(actor.mNextLowProcessTick);
                writer.integer<std::uint8_t>(actor.mRestrained ? 1 : 0);
                writer.integer<std::uint8_t>(actor.mActionReserved ? 1 : 0);
                writer.integer<std::uint8_t>(actor.mHasDestination ? 1 : 0);
                if (mVersion >= 7)
                    writer.integer<std::uint8_t>(actor.mDoorAnimationStarted ? 1 : 0);
                writer.string(actor.mInterruptionReason);
            }

            std::vector<RuntimePathPointState> points = mPathPoints;
            std::sort(points.begin(), points.end(), [](const auto& left, const auto& right) {
                return std::tie(left.mPathgrid, left.mNode) < std::tie(right.mPathgrid, right.mNode);
            });
            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(points.size()));
            for (const RuntimePathPointState& point : points)
            {
                writeKey(writer, point.mPathgrid);
                writer.integer(point.mNode);
                writer.integer<std::uint8_t>(point.mEnabled ? 1 : 0);
            }

            std::vector<RuntimeCompanionRelation> companions = mCompanions;
            std::sort(companions.begin(), companions.end(), [](const auto& left, const auto& right) {
                return std::tie(left.mLeader, left.mMember) < std::tie(right.mLeader, right.mMember);
            });
            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(companions.size()));
            for (const RuntimeCompanionRelation& relation : companions)
            {
                writeKey(writer, relation.mLeader);
                writeKey(writer, relation.mMember);
                writeKey(writer, relation.mGroup);
                writeKey(writer, relation.mSideWith);
                writer.integer(relation.mFormationIndex);
            }

            std::vector<RuntimeMountRelation> mounts = mMounts;
            std::sort(mounts.begin(), mounts.end(), [](const auto& left, const auto& right) {
                return std::tie(left.mHorse, left.mRider) < std::tie(right.mHorse, right.mRider);
            });
            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(mounts.size()));
            for (const RuntimeMountRelation& relation : mounts)
            {
                writeKey(writer, relation.mHorse);
                writeKey(writer, relation.mRider);
                writeKey(writer, relation.mOwner);
                writeKey(writer, relation.mLastRidden);
                writer.integer<std::uint8_t>(relation.mMounted ? 1 : 0);
            }

            std::vector<RuntimeDetectionVector> detectionVectors = mDetectionVectors;
            std::sort(detectionVectors.begin(), detectionVectors.end(), [](const auto& left, const auto& right) {
                return std::tie(left.mObserver, left.mTarget) < std::tie(right.mObserver, right.mTarget);
            });
            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(detectionVectors.size()));
            for (const RuntimeDetectionVector& vector : detectionVectors)
            {
                writeKey(writer, vector.mObserver);
                writeKey(writer, vector.mTarget);
                writer.floating(vector.mScore);
                writer.integer<std::uint8_t>(vector.mDetected ? 1 : 0);
                writer.integer<std::uint8_t>(vector.mLineOfSight ? 1 : 0);
            }
        }

        if (mVersion >= 6)
        {
            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(mPendingPackageDone.size()));
            for (const RuntimePackageDoneEvent& event : mPendingPackageDone)
            {
                writeKey(writer, event.mActor);
                writeKey(writer, event.mPackage);
            }
        }
        if (mVersion >= 8)
        {
            writer.integer(mPhysicalActions.mNext);
            auto pending = mPhysicalActions.mPending;
            std::sort(pending.begin(), pending.end());
            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(pending.size()));
            for (const auto id : pending)
                writer.integer(id);
        }
        if (mVersion >= 9)
        {
            auto actors = mNativeActorValues;
            std::sort(actors.begin(), actors.end(), [](const auto& a, const auto& b) { return a.mActor < b.mActor; });
            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(actors.size()));
            for (const auto& actor : actors)
            {
                writeKey(writer, actor.mActor);
                writeKey(writer, actor.mBase);
                writer.integer<std::uint8_t>(static_cast<std::uint8_t>(actor.mOwner));
                writer.integer<std::uint8_t>(static_cast<std::uint8_t>(actor.mProcess));
                for (const auto& value : actor.mValues)
                {
                    writer.floating(value.mBase);
                    std::uint8_t mask = 0;
                    for (std::size_t i = 0; i < value.mModifiers.size(); ++i)
                        if (value.mModifiers[i])
                            mask |= 1 << i;
                    writer.integer(mask);
                    for (const auto& modifier : value.mModifiers)
                        if (modifier)
                            writer.floating(*modifier);
                }
                if (mVersion >= 25)
                {
                    writer.integer<std::uint8_t>(actor.mProcessKnockedState.has_value());
                    if (actor.mProcessKnockedState)
                        writer.integer<std::int8_t>(*actor.mProcessKnockedState);
                }
                if (mVersion >= 26)
                {
                    writer.integer<std::uint8_t>(actor.mProcessAction.has_value());
                    if (actor.mProcessAction)
                        writer.integer<std::int16_t>(*actor.mProcessAction);
                }
                if (mVersion >= 10)
                {
                    writer.integer<std::uint8_t>(actor.mPlayerFormValues.has_value());
                    if (actor.mPlayerFormValues)
                        for (const auto value : *actor.mPlayerFormValues)
                            writer.integer(value);
                }
                if (mVersion >= 17)
                {
                    writer.integer<std::uint8_t>(actor.mNonPlayerFormHealth.has_value());
                    if (actor.mNonPlayerFormHealth)
                        writer.integer(*actor.mNonPlayerFormHealth);
                }
                if (mVersion >= 18)
                {
                    writer.integer<std::uint8_t>(actor.mPassiveAbilities.has_value());
                    if (actor.mPassiveAbilities)
                    {
                        writer.integer<std::uint32_t>(static_cast<std::uint32_t>(actor.mPassiveAbilities->size()));
                        for (const auto& ability : *actor.mPassiveAbilities)
                        {
                            writeKey(writer, ability.mSpell);
                            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(ability.mEffects.size()));
                            for (const auto& effect : ability.mEffects)
                            {
                                writer.integer(effect.mEffectIndex);
                                writer.integer(effect.mCode);
                                writer.integer(effect.mActorValue);
                                writer.floating(effect.mStoredMagnitude);
                                if (mVersion >= 19)
                                {
                                    writer.integer<std::uint8_t>(effect.mInitialMagnitude.has_value());
                                    if (effect.mInitialMagnitude)
                                        writer.floating(*effect.mInitialMagnitude);
                                }
                            }
                        }
                    }
                }
            }
        }
        if (mVersion >= 11)
        {
            auto bases = mNativeActorBases;
            std::sort(bases.begin(), bases.end(), [](const auto& a, const auto& b) { return a.mBase < b.mBase; });
            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(bases.size()));
            for (auto& base : bases)
            {
                writeKey(writer, base.mBase);
                writer.integer<std::uint8_t>(static_cast<std::uint8_t>(base.mKind));
                std::sort(base.mValues.begin(), base.mValues.end(),
                    [](const auto& a, const auto& b) { return a.mActorValue < b.mActorValue; });
                writer.integer<std::uint32_t>(static_cast<std::uint32_t>(base.mValues.size()));
                for (const auto& value : base.mValues)
                {
                    writer.integer(value.mActorValue);
                    writer.integer<std::uint8_t>(static_cast<std::uint8_t>(value.mValue.index()));
                    if (const auto* integer = std::get_if<std::int32_t>(&value.mValue))
                        writer.integer(*integer);
                    else
                        writer.floating(std::get<float>(value.mValue));
                }
            }
        }
        if (mVersion >= 12)
        {
            auto lives = mNativeActorLife;
            std::sort(lives.begin(), lives.end(), [](const auto& a, const auto& b) { return a.mActor < b.mActor; });
            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(lives.size()));
            for (const auto& life : lives)
            {
                writeKey(writer, life.mActor);
                writeKey(writer, life.mBase);
                writer.integer<std::uint8_t>(static_cast<std::uint8_t>(life.mPhase));
                writer.floating(life.mRecoveryRemaining);
                writeKey(writer, life.mKiller);
            }
            writer.integer(mNextDeathEvent);
            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(mPendingDeathEvents.size()));
            for (const auto& event : mPendingDeathEvents)
            {
                writer.integer(event.mId);
                writeKey(writer, event.mActor);
                writeKey(writer, event.mKiller);
            }
        }
        if (mVersion >= 13)
        {
            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(mNativeDeathCounts.size()));
            for (const auto& [base, count] : mNativeDeathCounts)
            {
                writeKey(writer, base);
                writer.integer(count);
            }
        }
        if (mVersion >= 14)
        {
            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(mNativeActorBreath.size()));
            for (const auto& [actor, remaining] : mNativeActorBreath)
            {
                writeKey(writer, actor);
                writer.floating(remaining);
            }
        }
        if (mVersion >= 15)
        {
            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(mNativeCombatEngagements.size()));
            for (const auto& [first, second] : mNativeCombatEngagements)
            {
                writeKey(writer, first);
                writeKey(writer, second);
            }
        }
        if (mVersion >= 16)
        {
            writer.floating(mNativeActorManagerTime);
            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(mNativeActorUpdateTimes.size()));
            for (const auto& [actor, time] : mNativeActorUpdateTimes)
            {
                writeKey(writer, actor);
                writer.floating(time);
            }
        }
        if (mVersion >= 20)
        {
            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(mPhysicalActionOwners.size()));
            for (const auto& [id, actor] : mPhysicalActionOwners)
            {
                writer.integer(id);
                writeKey(writer, actor);
            }
        }
        if (mVersion >= 21)
        {
            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(mNativeMeleeStates.size()));
            for (const auto& [actor, melee] : mNativeMeleeStates)
            {
                writeKey(writer, actor);
                writer.floating(melee.mInput.mHeldSeconds);
                writer.integer<std::uint8_t>(melee.mInput.mInputHeld);
                writer.integer<std::uint8_t>(melee.mInput.mPreferLeft);
                writer.integer<std::uint8_t>(static_cast<std::uint8_t>(melee.mInput.mQueued));
                writer.integer<std::uint8_t>(melee.mStrike.has_value());
                if (melee.mStrike)
                {
                    const auto& strike = *melee.mStrike;
                    writer.integer(strike.mActionId);
                    writer.integer<std::uint8_t>(static_cast<std::uint8_t>(strike.mKind));
                    writeKey(writer, strike.mWeaponBase);
                    writer.string(strike.mAnimationGroup);
                    writer.floating(strike.mPlaybackSpeed);
                    writer.floating(strike.mAnimationTime);
                    writer.integer<std::uint8_t>(strike.mContactCommitted);
                    if (mVersion >= 22)
                        writer.integer<std::uint8_t>(static_cast<std::uint8_t>(strike.mOrdinaryPhase));
                    if (mVersion >= 23)
                    {
                        writer.integer<std::uint8_t>(strike.mSequenceTiming.has_value());
                        if (strike.mSequenceTiming)
                        {
                            const auto& timing = *strike.mSequenceTiming;
                            writer.integer<std::uint8_t>(timing.mEasing);
                            for (const auto value : {timing.mOffset, timing.mEaseStart, timing.mLastInput})
                            {
                                writer.integer<std::uint8_t>(value.has_value());
                                if (value) writer.floating(*value);
                            }
                            writer.floating(timing.mEaseEnd);
                            writer.floating(timing.mWeightedTime);
                            writer.floating(timing.mOutputTime);
                        }
                    }
                }
                if (mVersion >= 28)
                {
                    writer.integer<std::uint8_t>(melee.mAiIntent.has_value());
                    if (melee.mAiIntent)
                    {
                        writeKey(writer, melee.mAiIntent->mTarget);
                        writeKey(writer, melee.mAiIntent->mStyle);
                    }
                }
            }
        }
        if (mVersion >= 23)
        {
            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(mNativeAnimationClocks.size()));
            for (const auto& [actor, clock] : mNativeAnimationClocks)
            {
                writeKey(writer, actor);
                writer.floating(clock);
            }
        }
        if (mVersion >= 27)
            writer.integer(mCombatRngState);
        if (mVersion >= 30)
        {
            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(mNativeActorKnockback.size()));
            for (const auto& [actor, pulse] : mNativeActorKnockback)
            {
                writeKey(writer, actor);
                for (float component : pulse.mAcceleration)
                    writer.floating(component);
                writer.floating(pulse.mRemaining);
            }
        }
        if (mVersion >= 31)
        {
            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(mNativeActorRagdolls.size()));
            for (const auto& [actor, pose] : mNativeActorRagdolls)
            {
                writeKey(writer, actor);
                writeKey(writer, pose.mBase);
                writer.string(pose.mModel);
                writer.string(pose.mAssetHash);
                writer.integer<std::uint32_t>(static_cast<std::uint32_t>(pose.mBodies.size()));
                for (const auto& body : pose.mBodies)
                {
                    writer.integer(body.mRecord);
                    writer.integer(body.mNodeRecord);
                    for (float value : body.mRotation)
                        writer.floating(value);
                    for (const auto& vector : {body.mPosition, body.mLinearVelocity, body.mAngularVelocity})
                        for (float value : vector)
                            writer.floating(value);
                    if (mVersion >= 32)
                    {
                        writer.integer<std::uint8_t>(body.mNativePackedVelocity.has_value());
                        if (body.mNativePackedVelocity)
                            for (const auto& vector : {body.mNativePackedVelocity->mLinear, body.mNativePackedVelocity->mAngular})
                                for (float value : vector)
                                    writer.floating(value);
                    }
                    if (mVersion >= 33)
                    {
                        writer.integer<std::uint8_t>(body.mNativeMotion.has_value());
                        if (body.mNativeMotion)
                            writer.integer<std::uint8_t>(static_cast<std::uint8_t>(*body.mNativeMotion));
                    }
                }
                if (mVersion >= 34)
                {
                    writer.integer<std::uint8_t>(pose.mNativeBlends.has_value());
                    if (pose.mNativeBlends)
                    {
                        writer.integer<std::uint32_t>(static_cast<std::uint32_t>(pose.mNativeBlends->size()));
                        for (const auto& blend : *pose.mNativeBlends)
                        {
                            writer.integer(blend.mBodyRecord);
                            writer.integer(blend.mCollisionFlags);
                            writer.integer(blend.mRequestedMotion);
                            writer.floating(blend.mHierarchyGain);
                            writer.floating(blend.mVelocityGain);
                        }
                    }
                }
                if (mVersion >= 35)
                {
                    writer.integer<std::uint8_t>(pose.mNativeControllers.has_value());
                    if (pose.mNativeControllers)
                    {
                        const auto target = [&](std::optional<std::uint32_t> node) {
                            writer.integer<std::uint8_t>(node.has_value());
                            if (node) writer.integer(*node);
                        };
                        const auto common = [&](const PhysicalBlendTiming& timing, const PhysicalBlendClock& clock) {
                            writer.integer(timing.mFlags);
                            for (float value : {timing.mFrequency, timing.mPhase, timing.mStartKey, timing.mStopKey,
                                    clock.mStartTime, clock.mPreviousTime, clock.mElapsed}) writer.floating(value);
                        };
                        writer.integer<std::uint32_t>(static_cast<std::uint32_t>(pose.mNativeControllers->mBlends.size()));
                        for (const auto& controller : pose.mNativeControllers->mBlends)
                        {
                            writer.integer(controller.mRecord); writer.integer(controller.mAttachedNode);
                            target(controller.mTargetNode); const auto& state = controller.mState;
                            common(state.mTiming, state.mClock); writer.integer(state.mCursor);
                            writer.floating(state.mCachedGains.mHierarchy); writer.floating(state.mCachedGains.mVelocity);
                            writer.integer(state.mSetupState);
                            writer.integer<std::uint32_t>(static_cast<std::uint32_t>(state.mKeys.size()));
                            for (const auto& key : state.mKeys)
                            {
                                writer.floating(key.mTime); writer.floating(key.mGains.mHierarchy); writer.floating(key.mGains.mVelocity);
                            }
                        }
                        writer.integer<std::uint32_t>(static_cast<std::uint32_t>(pose.mNativeControllers->mVelocities.size()));
                        for (const auto& controller : pose.mNativeControllers->mVelocities)
                        {
                            writer.integer(controller.mAttachedNode); target(controller.mTargetNode);
                            writer.integer<std::uint8_t>(controller.mPrecedesBlend); const auto& state = controller.mState;
                            common(state.mTiming, state.mClock);
                            for (float value : state.mForceVector) writer.floating(value);
                            writer.floating(state.mFrameDelta);
                        }
                    }
                }
            }
        }
        if (mVersion >= 36)
        {
            writer.integer<std::uint8_t>(mNativePhysicalBlendTimeCache.has_value());
            if (mNativePhysicalBlendTimeCache)
            {
                const auto& cache = *mNativePhysicalBlendTimeCache;
                writer.integer(cache.mCycle);
                for (float value : {cache.mStopKey, cache.mStartKey, cache.mKeyTime, cache.mResult})
                    writer.floating(value);
            }
        }
        if (mVersion >= 37)
        {
            writer.integer<std::uint8_t>(mNativePlayerBowTimer.has_value());
            if (mNativePlayerBowTimer)
                writer.floating(*mNativePlayerBowTimer);
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
        result.mPlayer.mInventory = readInventory(reader, result.mVersion);
        if (result.mVersion >= 3)
        {
            result.mPlayer.mName = reader.string();
            result.mPlayer.mRace = readKey(reader);
            result.mPlayer.mClass = readKey(reader);
            result.mPlayer.mBirthSign = readKey(reader);
            const std::uint8_t female = reader.integer<std::uint8_t>();
            if (female > 1)
                throw std::runtime_error("Invalid TES4 runtime-state player sex");
            result.mPlayer.mFemale = female != 0;
            result.mPlayer.mCharacterGenerationFlags = reader.integer<std::uint8_t>();
        }

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
            reference.mInventory = readInventory(reader, result.mVersion);
            const std::uint32_t customCount = reader.count();
            for (std::uint32_t j = 0; j < customCount; ++j)
            {
                const std::string name = reader.string();
                if (!reference.mCustomState.emplace(name, readValue(reader)).second)
                    throw std::runtime_error("Duplicate TES4 runtime-state custom-state key");
            }
            if (result.mVersion >= 29)
            {
                const auto hasDrawState = reader.integer<std::uint8_t>();
                if (hasDrawState > 1)
                    throw std::runtime_error("Invalid TES4 native actor draw state flag");
                if (hasDrawState)
                    reference.mActorDrawState = static_cast<ActorDrawState>(reader.integer<std::uint8_t>());
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

        if (result.mVersion >= 5)
        {
            result.mAiRngState = reader.integer<std::uint64_t>();
            const std::uint32_t actorCount = reader.count();
            result.mActorAi.reserve(actorCount);
            for (std::uint32_t i = 0; i < actorCount; ++i)
            {
                RuntimeActorAiState actor;
                actor.mActor = readKey(reader);
                actor.mBase = readKey(reader);
                actor.mPackage = readKey(reader);
                actor.mScriptPackage = readKey(reader);
                actor.mTarget = readKey(reader);
                actor.mTargetBase = readKey(reader);
                actor.mCell = readKey(reader);
                actor.mPathgrid = readKey(reader);
                actor.mDoor = readKey(reader);
                actor.mDestinationCell = readKey(reader);
                actor.mLastValidCell = readKey(reader);
                actor.mActionItem = readKey(reader);
                actor.mLastTransitionDoor = readKey(reader);
                actor.mCompanionGroup = readKey(reader);
                actor.mCompanionSideWith = readKey(reader);
                actor.mMount = readKey(reader);
                actor.mRider = readKey(reader);
                const std::uint8_t hasScheduleWindow = reader.integer<std::uint8_t>();
                if (hasScheduleWindow > 1)
                    throw std::runtime_error("Invalid TES4 actor AI schedule-window flag");
                if (hasScheduleWindow)
                {
                    ScheduleWindow window;
                    window.mStart = readCalendar(reader);
                    window.mEnd = readCalendar(reader);
                    window.mDurationHours = reader.float64();
                    actor.mScheduleWindow = std::move(window);
                }
                const std::uint8_t conditionResult = reader.integer<std::uint8_t>();
                if (conditionResult > static_cast<std::uint8_t>(ConditionResult::Unsupported))
                    throw std::runtime_error("Invalid TES4 actor AI condition result");
                actor.mConditionResult = static_cast<ConditionResult>(conditionResult);
                actor.mDestinationPosition = readPosition(reader);
                actor.mLastValidPosition = readPosition(reader);
                actor.mSource = static_cast<PackageSource>(reader.integer<std::uint8_t>());
                actor.mPackageType = static_cast<AIPackageType>(reader.integer<std::uint8_t>());
                actor.mProcedure = static_cast<PackageProcedure>(reader.integer<std::uint16_t>());
                actor.mPhase = static_cast<PackagePhase>(reader.integer<std::uint8_t>());
                actor.mTier = static_cast<ProcessTier>(reader.integer<std::uint8_t>());
                actor.mBoundary = static_cast<PhaseBoundary>(reader.integer<std::uint8_t>());
                actor.mListIndex = reader.integer<std::uint32_t>();
                actor.mPathNode = reader.integer<std::uint32_t>();
                actor.mRepathAttempts = reader.integer<std::uint32_t>();
                actor.mFormationIndex = reader.integer<std::int32_t>();
                actor.mSelectionGeneration = reader.integer<std::uint64_t>();
                actor.mRouteGeneration = reader.integer<std::uint64_t>();
                actor.mTransitionGeneration = reader.integer<std::uint64_t>();
                actor.mActionTimer = reader.float32();
                actor.mDurationRemaining = reader.float32();
                actor.mNoProgressSeconds = reader.float32();
                actor.mDoorCooldown = reader.float32();
                actor.mLowProcessTimer = reader.float32();
                actor.mNextLowProcessTick = reader.float32();
                const std::uint8_t restrained = reader.integer<std::uint8_t>();
                const std::uint8_t reserved = reader.integer<std::uint8_t>();
                const std::uint8_t hasDestination = reader.integer<std::uint8_t>();
                if (restrained > 1 || reserved > 1 || hasDestination > 1)
                    throw std::runtime_error("Invalid TES4 actor AI flags");
                actor.mRestrained = restrained != 0;
                actor.mActionReserved = reserved != 0;
                actor.mHasDestination = hasDestination != 0;
                if (result.mVersion >= 7)
                {
                    const std::uint8_t doorAnimationStarted = reader.integer<std::uint8_t>();
                    if (doorAnimationStarted > 1)
                        throw std::runtime_error("Invalid TES4 actor AI door-animation flag");
                    actor.mDoorAnimationStarted = doorAnimationStarted != 0;
                }
                actor.mInterruptionReason = reader.string();
                result.mActorAi.push_back(std::move(actor));
            }

            const std::uint32_t pointCount = reader.count();
            result.mPathPoints.reserve(pointCount);
            for (std::uint32_t i = 0; i < pointCount; ++i)
            {
                RuntimePathPointState point;
                point.mPathgrid = readKey(reader);
                point.mNode = reader.integer<std::uint32_t>();
                const std::uint8_t enabled = reader.integer<std::uint8_t>();
                if (enabled > 1)
                    throw std::runtime_error("Invalid TES4 path-point overlay flag");
                point.mEnabled = enabled != 0;
                result.mPathPoints.push_back(std::move(point));
            }

            const std::uint32_t companionCount = reader.count();
            result.mCompanions.reserve(companionCount);
            for (std::uint32_t i = 0; i < companionCount; ++i)
            {
                RuntimeCompanionRelation relation;
                relation.mLeader = readKey(reader);
                relation.mMember = readKey(reader);
                relation.mGroup = readKey(reader);
                relation.mSideWith = readKey(reader);
                relation.mFormationIndex = reader.integer<std::int32_t>();
                result.mCompanions.push_back(std::move(relation));
            }

            const std::uint32_t mountCount = reader.count();
            result.mMounts.reserve(mountCount);
            for (std::uint32_t i = 0; i < mountCount; ++i)
            {
                RuntimeMountRelation relation;
                relation.mHorse = readKey(reader);
                relation.mRider = readKey(reader);
                relation.mOwner = readKey(reader);
                relation.mLastRidden = readKey(reader);
                const std::uint8_t mounted = reader.integer<std::uint8_t>();
                if (mounted > 1)
                    throw std::runtime_error("Invalid TES4 mount relation flag");
                relation.mMounted = mounted != 0;
                result.mMounts.push_back(std::move(relation));
            }

            const std::uint32_t detectionCount = reader.count();
            result.mDetectionVectors.reserve(detectionCount);
            for (std::uint32_t i = 0; i < detectionCount; ++i)
            {
                RuntimeDetectionVector vector;
                vector.mObserver = readKey(reader);
                vector.mTarget = readKey(reader);
                vector.mScore = reader.float64();
                const std::uint8_t detected = reader.integer<std::uint8_t>();
                const std::uint8_t lineOfSight = reader.integer<std::uint8_t>();
                if (detected > 1 || lineOfSight > 1)
                    throw std::runtime_error("Invalid TES4 runtime-state detection vector flags");
                vector.mDetected = detected != 0;
                vector.mLineOfSight = lineOfSight != 0;
                result.mDetectionVectors.push_back(std::move(vector));
            }
        }
        else
        {
            // Older saves did not persist an AI stream. Derive a stable seed
            // from state that already existed instead of consuming gameplay RNG.
            result.mAiRngState = result.mNextDynamicSerial == 0 ? 1 : result.mNextDynamicSerial;
        }

        if (result.mVersion >= 6)
        {
            const std::uint32_t count = reader.count();
            result.mPendingPackageDone.reserve(count);
            for (std::uint32_t i = 0; i < count; ++i)
                result.mPendingPackageDone.push_back({ readKey(reader), readKey(reader) });
        }
        if (result.mVersion >= 8)
        {
            result.mPhysicalActions.mNext = reader.integer<std::uint64_t>();
            const auto count = reader.count();
            result.mPhysicalActions.mPending.reserve(count);
            for (std::uint32_t i = 0; i < count; ++i)
                result.mPhysicalActions.mPending.push_back(reader.integer<std::uint64_t>());
        }
        if (result.mVersion >= 9)
        {
            const auto count = reader.count();
            for (std::uint32_t i = 0; i < count; ++i)
            {
                RuntimeActorValues actor;
                const auto nativeKey = [&reader]() {
                    const auto text = reader.string();
                    try
                    {
                        const auto key = ESM::FormKey::deserialize(text);
                        if (!key.isNull() && key.serialize() == text)
                            return key;
                    }
                    catch (const std::invalid_argument&)
                    {
                    }
                    throw std::runtime_error("Invalid or noncanonical TES4 native actor-value identity");
                };
                actor.mActor = nativeKey();
                actor.mBase = nativeKey();
                actor.mOwner = static_cast<ActorValueOwner>(reader.integer<std::uint8_t>());
                actor.mProcess = static_cast<ActorValueProcess>(reader.integer<std::uint8_t>());
                for (auto& value : actor.mValues)
                {
                    value.mBase = reader.float32();
                    const auto mask = reader.integer<std::uint8_t>();
                    if (mask > 7)
                        throw std::runtime_error("Invalid TES4 native actor-value modifier mask");
                    for (std::size_t j = 0; j < value.mModifiers.size(); ++j)
                        if (mask & (1 << j))
                            value.mModifiers[j] = reader.float32();
                }
                if (result.mVersion >= 25)
                {
                    const auto present = reader.integer<std::uint8_t>();
                    if (present > 1)
                        throw std::runtime_error("Invalid TES4 native knocked byte presence");
                    if (present)
                        actor.mProcessKnockedState = reader.integer<std::int8_t>();
                }
                if (result.mVersion >= 26)
                {
                    const auto present = reader.integer<std::uint8_t>();
                    if (present > 1)
                        throw std::runtime_error("Invalid TES4 native action-code presence");
                    if (present)
                        actor.mProcessAction = reader.integer<std::int16_t>();
                }
                if (result.mVersion >= 10)
                {
                    const auto present = reader.integer<std::uint8_t>();
                    if (present > 1)
                        throw std::runtime_error("Invalid TES4 player form-value presence");
                    if (present)
                    {
                        actor.mPlayerFormValues.emplace();
                        for (auto& value : *actor.mPlayerFormValues)
                            value = reader.integer<std::int32_t>();
                    }
                }
                if (result.mVersion >= 17)
                {
                    const auto present = reader.integer<std::uint8_t>();
                    if (present > 1)
                        throw std::runtime_error("Invalid TES4 nonplayer form Health presence");
                    if (present)
                        actor.mNonPlayerFormHealth = reader.integer<std::int32_t>();
                }
                if (result.mVersion >= 18)
                {
                    const auto present = reader.integer<std::uint8_t>();
                    if (present > 1)
                        throw std::runtime_error("Invalid TES4 passive ability ownership presence");
                    if (present)
                    {
                        actor.mPassiveAbilities.emplace();
                        const auto abilityCount = reader.count();
                        for (std::uint32_t j = 0; j < abilityCount; ++j)
                        {
                            RuntimePassiveAbility ability;
                            ability.mSpell = nativeKey();
                            const auto effectCount = reader.count();
                            for (std::uint32_t k = 0; k < effectCount; ++k)
                            {
                                RuntimePassiveValueModifier effect{reader.integer<std::uint32_t>(),
                                    reader.integer<std::uint32_t>(), reader.integer<std::uint32_t>(), reader.float32()};
                                if (result.mVersion >= 19)
                                {
                                    const auto initialPresent = reader.integer<std::uint8_t>();
                                    if (initialPresent > 1)
                                        throw std::runtime_error("Invalid TES4 passive initial magnitude presence");
                                    if (initialPresent)
                                        effect.mInitialMagnitude = reader.float32();
                                }
                                ability.mEffects.push_back(std::move(effect));
                            }
                            actor.mPassiveAbilities->push_back(std::move(ability));
                        }
                    }
                }
                result.mNativeActorValues.push_back(std::move(actor));
            }
        }
        if (result.mVersion >= 11)
        {
            const auto count = reader.count();
            for (std::uint32_t i = 0; i < count; ++i)
            {
                RuntimeActorBaseOverride base;
                const auto key = reader.string();
                try
                {
                    base.mBase = ESM::FormKey::deserialize(key);
                }
                catch (const std::invalid_argument&)
                {
                    throw std::runtime_error("Invalid TES4 native actor base identity");
                }
                if (base.mBase.isNull() || base.mBase.serialize() != key)
                    throw std::runtime_error("Invalid or noncanonical TES4 native actor base identity");
                base.mKind = static_cast<ActorBaseKind>(reader.integer<std::uint8_t>());
                const auto size = reader.count();
                if (size == 0 || size > 72)
                    throw std::runtime_error("Invalid TES4 native actor base value count");
                for (std::uint32_t j = 0; j < size; ++j)
                {
                    ActorBaseValueSet value;
                    value.mActorValue = reader.integer<std::uint8_t>();
                    const auto type = reader.integer<std::uint8_t>();
                    if (type == 0)
                        value.mValue = reader.integer<std::int32_t>();
                    else if (type == 1)
                        value.mValue = reader.float32();
                    else
                        throw std::runtime_error("Invalid TES4 native actor base storage type");
                    base.mValues.push_back(value);
                }
                result.mNativeActorBases.push_back(std::move(base));
            }
        }
        if (result.mVersion >= 12)
        {
            const auto nativeKey = [&reader](bool nullable = false) {
                const auto text = reader.string();
                try
                {
                    const auto key = ESM::FormKey::deserialize(text);
                    if ((nullable || !key.isNull()) && key.serialize() == text)
                        return key;
                }
                catch (const std::invalid_argument&) {}
                throw std::runtime_error("Invalid or noncanonical TES4 actor lifecycle identity");
            };
            const auto count = reader.count();
            for (std::uint32_t i = 0; i < count; ++i)
            {
                RuntimeActorLife life;
                life.mActor = nativeKey();
                life.mBase = nativeKey();
                life.mPhase = static_cast<ActorLifePhase>(reader.integer<std::uint8_t>());
                life.mRecoveryRemaining = reader.float32();
                life.mKiller = nativeKey(true);
                result.mNativeActorLife.push_back(std::move(life));
            }
            result.mNextDeathEvent = reader.integer<std::uint64_t>();
            const auto events = reader.count();
            for (std::uint32_t i = 0; i < events; ++i)
                result.mPendingDeathEvents.push_back({reader.integer<std::uint64_t>(), nativeKey(), nativeKey(true)});
        }
        if (result.mVersion >= 13)
        {
            const auto count = reader.count();
            for (std::uint32_t i = 0; i < count; ++i)
            {
                const auto text = reader.string();
                ESM::FormKey base;
                try { base = ESM::FormKey::deserialize(text); }
                catch (const std::invalid_argument&)
                { throw std::runtime_error("Invalid TES4 native death count base"); }
                if (base.isNull() || base.serialize() != text)
                    throw std::runtime_error("Invalid or noncanonical TES4 native death count base");
                const auto value = reader.integer<std::uint16_t>();
                if (!result.mNativeDeathCounts.emplace(std::move(base), value).second)
                    throw std::runtime_error("Duplicate TES4 native death count base");
            }
        }
        if (result.mVersion >= 14)
        {
            const auto count = reader.count();
            for (std::uint32_t i = 0; i < count; ++i)
            {
                const auto text = reader.string();
                ESM::FormKey actor;
                try { actor = ESM::FormKey::deserialize(text); }
                catch (const std::invalid_argument&)
                { throw std::runtime_error("Invalid TES4 native breath actor"); }
                if (actor.isNull() || actor.serialize() != text)
                    throw std::runtime_error("Invalid or noncanonical TES4 native breath actor");
                const float remaining = reader.float32();
                if (!result.mNativeActorBreath.emplace(std::move(actor), remaining).second)
                    throw std::runtime_error("Duplicate TES4 native breath actor");
            }
        }
        if (result.mVersion >= 15)
        {
            const auto count = reader.count();
            const auto actorKey = [&]() {
                const auto text = reader.string();
                ESM::FormKey key;
                try { key = ESM::FormKey::deserialize(text); }
                catch (const std::invalid_argument&)
                { throw std::runtime_error("Invalid TES4 combat actor"); }
                if (key.isNull() || key.serialize() != text)
                    throw std::runtime_error("Invalid or noncanonical TES4 combat actor");
                return key;
            };
            for (std::uint32_t i = 0; i < count; ++i)
            {
                auto first = actorKey();
                auto second = actorKey();
                if (!result.mNativeCombatEngagements.emplace(std::move(first), std::move(second)).second)
                    throw std::runtime_error("Duplicate TES4 combat engagement");
            }
        }
        if (result.mVersion >= 16)
        {
            result.mNativeActorManagerTime = reader.float32();
            const auto count = reader.count();
            for (std::uint32_t i = 0; i < count; ++i)
            {
                const auto text = reader.string();
                ESM::FormKey actor;
                try { actor = ESM::FormKey::deserialize(text); }
                catch (const std::invalid_argument&)
                { throw std::runtime_error("Invalid TES4 actor clock owner"); }
                if (actor.isNull() || actor.serialize() != text)
                    throw std::runtime_error("Invalid or noncanonical TES4 actor clock owner");
                const float time = reader.float32();
                if (!result.mNativeActorUpdateTimes.emplace(std::move(actor), time).second)
                    throw std::runtime_error("Duplicate TES4 actor clock owner");
            }
        }
        if (result.mVersion >= 20)
        {
            const auto count = reader.count();
            for (std::uint32_t i = 0; i < count; ++i)
            {
                const auto id = reader.integer<std::uint64_t>();
                const auto text = reader.string();
                ESM::FormKey actor;
                try { actor = ESM::FormKey::deserialize(text); }
                catch (const std::invalid_argument&)
                { throw std::runtime_error("Invalid TES4 physical action owner"); }
                if (actor.isNull() || actor.serialize() != text)
                    throw std::runtime_error("Invalid or noncanonical TES4 physical action owner");
                if (!result.mPhysicalActionOwners.emplace(id, std::move(actor)).second)
                    throw std::runtime_error("Duplicate TES4 physical action owner identity");
            }
        }
        if (result.mVersion >= 21)
        {
            const auto boolean = [&reader]() {
                const auto value = reader.integer<std::uint8_t>();
                if (value > 1) throw std::runtime_error("Invalid TES4 melee boolean");
                return value != 0;
            };
            const auto key = [&reader]() {
                const auto text = reader.string();
                ESM::FormKey value;
                try { value = ESM::FormKey::deserialize(text); }
                catch (const std::invalid_argument&) { throw std::runtime_error("Invalid TES4 melee key"); }
                if (value.serialize() != text) throw std::runtime_error("Noncanonical TES4 melee key");
                return value;
            };
            for (std::uint32_t i = 0, count = reader.count(); i < count; ++i)
            {
                auto actor = key();
                RuntimeMeleeState melee;
                melee.mInput.mHeldSeconds = reader.float32();
                melee.mInput.mInputHeld = boolean();
                melee.mInput.mPreferLeft = boolean();
                melee.mInput.mQueued = static_cast<MeleeQueuedStrike>(reader.integer<std::uint8_t>());
                if (boolean())
                {
                    RuntimeMeleeStrike strike;
                    strike.mActionId = reader.integer<std::uint64_t>();
                    strike.mKind = static_cast<MeleeStrikeKind>(reader.integer<std::uint8_t>());
                    strike.mWeaponBase = key();
                    strike.mAnimationGroup = reader.string();
                    strike.mPlaybackSpeed = reader.float32();
                    strike.mAnimationTime = reader.float32();
                    strike.mContactCommitted = boolean();
                    if (result.mVersion >= 22)
                        strike.mOrdinaryPhase = static_cast<OrdinaryMeleePhase>(reader.integer<std::uint8_t>());
                    if (result.mVersion >= 23 && boolean())
                    {
                        MeleeSequenceTiming timing;
                        timing.mEasing = boolean();
                        for (auto* value : {&timing.mOffset, &timing.mEaseStart, &timing.mLastInput})
                            if (boolean()) *value = reader.float32();
                        timing.mEaseEnd = reader.float32();
                        timing.mWeightedTime = reader.float32();
                        timing.mOutputTime = reader.float32();
                        strike.mSequenceTiming = timing;
                    }
                    melee.mStrike = std::move(strike);
                }
                if (result.mVersion >= 28 && boolean())
                    melee.mAiIntent = RuntimeMeleeAiIntent{key(), key()};
                if (!result.mNativeMeleeStates.emplace(std::move(actor), std::move(melee)).second)
                    throw std::runtime_error("Duplicate TES4 melee state owner");
            }
        }
        if (result.mVersion >= 23)
        {
            const auto count = reader.count();
            for (std::uint32_t i = 0; i < count; ++i)
            {
                const auto text = reader.string();
                ESM::FormKey actor;
                try { actor = ESM::FormKey::deserialize(text); }
                catch (const std::invalid_argument&) { throw std::runtime_error("Invalid TES4 native animation clock actor"); }
                if (actor.serialize() != text)
                    throw std::runtime_error("Noncanonical TES4 native animation clock actor");
                const auto clock = reader.float32();
                if (!result.mNativeAnimationClocks.emplace(std::move(actor), clock).second)
                    throw std::runtime_error("Duplicate TES4 native animation clock");
            }
        }
        if (result.mVersion >= 27)
            result.mCombatRngState = reader.integer<std::uint32_t>();
        if (result.mVersion >= 30)
        {
            const auto count = reader.count();
            for (std::uint32_t i = 0; i < count; ++i)
            {
                const auto text = reader.string();
                ESM::FormKey actor;
                try { actor = ESM::FormKey::deserialize(text); }
                catch (const std::invalid_argument&) { throw std::runtime_error("Invalid TES4 native knockback actor"); }
                if (actor.serialize() != text)
                    throw std::runtime_error("Noncanonical TES4 native knockback actor");
                TimedKnockbackState pulse;
                for (float& component : pulse.mAcceleration)
                    component = reader.float32();
                pulse.mRemaining = reader.float32();
                if (!result.mNativeActorKnockback.emplace(std::move(actor), pulse).second)
                    throw std::runtime_error("Duplicate TES4 native actor knockback");
            }
        }
        if (result.mVersion >= 31)
        {
            const auto readRagdollKey = [&] {
                const auto text = reader.string();
                ESM::FormKey key;
                try { key = ESM::FormKey::deserialize(text); }
                catch (const std::invalid_argument&) { throw std::runtime_error("Invalid TES4 ragdoll identity"); }
                if (key.serialize() != text)
                    throw std::runtime_error("Noncanonical TES4 ragdoll identity");
                return key;
            };
            const auto count = reader.count();
            for (std::uint32_t i = 0; i < count; ++i)
            {
                auto actor = readRagdollKey();
                RuntimeActorRagdoll pose;
                pose.mBase = readRagdollKey();
                pose.mModel = reader.string();
                pose.mAssetHash = reader.string();
                const auto bodies = reader.count();
                for (std::uint32_t j = 0; j < bodies; ++j)
                {
                    RuntimeRagdollBody body;
                    body.mRecord = reader.integer<std::uint32_t>();
                    body.mNodeRecord = reader.integer<std::uint32_t>();
                    for (float& value : body.mRotation)
                        value = reader.float32();
                    for (auto* vector : {&body.mPosition, &body.mLinearVelocity, &body.mAngularVelocity})
                        for (float& value : *vector)
                            value = reader.float32();
                    if (result.mVersion >= 32)
                    {
                        const auto present = reader.integer<std::uint8_t>();
                        if (present > 1)
                            throw std::runtime_error("Invalid TES4 packed ragdoll presence flag");
                        if (present)
                        {
                            body.mNativePackedVelocity.emplace();
                            for (auto* vector : {&body.mNativePackedVelocity->mLinear, &body.mNativePackedVelocity->mAngular})
                                for (float& value : *vector)
                                    value = reader.float32();
                        }
                    }
                    if (result.mVersion >= 33)
                    {
                        const auto present = reader.integer<std::uint8_t>();
                        if (present > 1)
                            throw std::runtime_error("Invalid TES4 ragdoll motion presence flag");
                        if (present)
                            body.mNativeMotion = static_cast<RuntimeRagdollMotion>(reader.integer<std::uint8_t>());
                    }
                    pose.mBodies.push_back(body);
                }
                if (result.mVersion >= 34)
                {
                    const auto present = reader.integer<std::uint8_t>();
                    if (present > 1)
                        throw std::runtime_error("Invalid TES4 native blend presence flag");
                    if (present)
                    {
                        const auto blendCount = reader.count();
                        if (blendCount > pose.mBodies.size())
                            throw std::runtime_error("Excessive TES4 native blend count");
                        pose.mNativeBlends.emplace();
                        for (std::uint32_t j = 0; j < blendCount; ++j)
                        {
                            RuntimeRagdollBlendState blend;
                            blend.mBodyRecord = reader.integer<std::uint32_t>();
                            blend.mCollisionFlags = reader.integer<std::uint16_t>();
                            blend.mRequestedMotion = reader.integer<std::uint32_t>();
                            blend.mHierarchyGain = reader.float32();
                            blend.mVelocityGain = reader.float32();
                            pose.mNativeBlends->push_back(blend);
                        }
                    }
                }
                if (result.mVersion >= 35)
                {
                    const auto boolean = [&]() {
                        const auto value = reader.integer<std::uint8_t>();
                        if (value > 1) throw std::runtime_error("Invalid TES4 ragdoll controller presence or boolean");
                        return value != 0;
                    };
                    const auto target = [&]() -> std::optional<std::uint32_t> {
                        if (!boolean()) return std::nullopt;
                        return reader.integer<std::uint32_t>();
                    };
                    const auto common = [&](PhysicalBlendTiming& timing, PhysicalBlendClock& clock) {
                        timing.mFlags = reader.integer<std::uint16_t>();
                        for (auto* value : {&timing.mFrequency, &timing.mPhase, &timing.mStartKey, &timing.mStopKey,
                                &clock.mStartTime, &clock.mPreviousTime, &clock.mElapsed}) *value = reader.float32();
                    };
                    if (boolean())
                    {
                        pose.mNativeControllers.emplace();
                        const auto blendControllerCount = reader.count();
                        if (blendControllerCount > pose.mBodies.size())
                            throw std::runtime_error("Excessive TES4 authored controller count");
                        for (std::uint32_t j = 0; j < blendControllerCount; ++j)
                        {
                            RuntimeRagdollBlendController controller;
                            controller.mRecord = reader.integer<std::uint32_t>(); controller.mAttachedNode = reader.integer<std::uint32_t>();
                            controller.mTargetNode = target(); auto& state = controller.mState;
                            common(state.mTiming, state.mClock); state.mCursor = reader.integer<std::uint32_t>();
                            state.mCachedGains = {reader.float32(), reader.float32()}; state.mSetupState = reader.integer<std::uint32_t>();
                            const auto keyCount = reader.count();
                            for (std::uint32_t k = 0; k < keyCount; ++k)
                                state.mKeys.push_back({reader.float32(), {reader.float32(), reader.float32()}});
                            pose.mNativeControllers->mBlends.push_back(std::move(controller));
                        }
                        const auto velocityControllerCount = reader.count();
                        if (velocityControllerCount > pose.mBodies.size())
                            throw std::runtime_error("Excessive TES4 generated controller count");
                        for (std::uint32_t j = 0; j < velocityControllerCount; ++j)
                        {
                            RuntimeRagdollVelocityController controller;
                            controller.mAttachedNode = reader.integer<std::uint32_t>(); controller.mTargetNode = target();
                            controller.mPrecedesBlend = boolean(); auto& state = controller.mState;
                            common(state.mTiming, state.mClock);
                            for (float& value : state.mForceVector) value = reader.float32();
                            state.mFrameDelta = reader.float32();
                            pose.mNativeControllers->mVelocities.push_back(std::move(controller));
                        }
                    }
                }
                if (!result.mNativeActorRagdolls.emplace(std::move(actor), std::move(pose)).second)
                    throw std::runtime_error("Duplicate TES4 ragdoll owner");
            }
        }
        if (result.mVersion >= 36)
        {
            const auto present = reader.integer<std::uint8_t>();
            if (present > 1)
                throw std::runtime_error("Invalid native physical cache presence marker");
            if (present)
                result.mNativePhysicalBlendTimeCache = PhysicalBlendTimeCache{reader.integer<std::uint32_t>(),
                    reader.float32(), reader.float32(), reader.float32(), reader.float32()};
        }
        if (result.mVersion >= 37)
        {
            const auto present = reader.integer<std::uint8_t>();
            if (present > 1)
                throw std::runtime_error("Invalid native Player bow timer presence marker");
            if (present)
                result.mNativePlayerBowTimer = reader.float32();
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
        stream.imbue(std::locale::classic());
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
        stream << "},\"inventory\":";
        writeJsonInventory(stream, mPlayer.mInventory, mVersion);
        if (mVersion >= 3)
            stream << ",\"name\":\"" << escapeJson(mPlayer.mName) << "\",\"race\":\""
                   << escapeJson(mPlayer.mRace.serialize()) << "\",\"class\":\""
                   << escapeJson(mPlayer.mClass.serialize()) << "\",\"birthsign\":\""
                   << escapeJson(mPlayer.mBirthSign.serialize()) << "\",\"female\":"
                   << (mPlayer.mFemale ? "true" : "false") << ",\"character_generation_flags\":"
                   << static_cast<unsigned>(mPlayer.mCharacterGenerationFlags);
        stream << "},\"globals\":{";
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
            stream << ",\"lock_level\":" << reference.mLockLevel << ",\"inventory\":";
            writeJsonInventory(stream, reference.mInventory, mVersion);
            stream << ",\"custom_state\":{";
            index = 0;
            for (const auto& [name, value] : reference.mCustomState)
            {
                stream << (index++ ? "," : "") << '"' << escapeJson(name) << "\":";
                writeJsonValue(stream, value);
            }
            stream << '}';
            if (mVersion >= 29)
            {
                stream << ",\"actor_draw_state\":";
                if (reference.mActorDrawState)
                    stream << static_cast<unsigned>(*reference.mActorDrawState);
                else
                    stream << "null";
            }
            stream << '}';
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
        stream << "]";
        if (mVersion >= 5)
        {
            stream << ",\"ai_rng_state\":" << mAiRngState << ",\"actor_ai\":[";
            std::vector<RuntimeActorAiState> actors = mActorAi;
            std::sort(actors.begin(), actors.end(), [](const auto& left, const auto& right) {
                return left.mActor < right.mActor;
            });
            for (std::size_t i = 0; i < actors.size(); ++i)
            {
                const RuntimeActorAiState& actor = actors[i];
                if (i)
                    stream << ',';
                stream << "{\"actor\":\"" << escapeJson(actor.mActor.serialize()) << "\",\"base\":\""
                       << escapeJson(actor.mBase.serialize()) << "\",\"package\":\""
                       << escapeJson(actor.mPackage.serialize()) << "\",\"script_package\":\""
                       << escapeJson(actor.mScriptPackage.serialize())
                       << "\",\"target\":\""
                       << escapeJson(actor.mTarget.serialize()) << "\",\"target_base\":\""
                       << escapeJson(actor.mTargetBase.serialize()) << "\",\"cell\":\""
                       << escapeJson(actor.mCell.serialize()) << "\",\"pathgrid\":\""
                       << escapeJson(actor.mPathgrid.serialize()) << "\",\"door\":\""
                       << escapeJson(actor.mDoor.serialize()) << "\",\"destination_cell\":\""
                       << escapeJson(actor.mDestinationCell.serialize()) << "\",\"destination_position\":";
                writeJsonPosition(stream, actor.mDestinationPosition);
                stream << ",\"last_valid_cell\":\"" << escapeJson(actor.mLastValidCell.serialize())
                       << "\",\"last_valid_position\":";
                writeJsonPosition(stream, actor.mLastValidPosition);
                stream << ",\"action_item\":\"" << escapeJson(actor.mActionItem.serialize())
                       << "\",\"last_transition_door\":\""
                       << escapeJson(actor.mLastTransitionDoor.serialize()) << "\",\"companion_group\":\""
                       << escapeJson(actor.mCompanionGroup.serialize()) << "\",\"companion_side_with\":\""
                       << escapeJson(actor.mCompanionSideWith.serialize()) << "\",\"mount\":\""
                       << escapeJson(actor.mMount.serialize()) << "\",\"rider\":\""
                       << escapeJson(actor.mRider.serialize()) << "\",\"schedule_window\":";
                if (!actor.mScheduleWindow)
                    stream << "null";
                else
                {
                    const auto writeJsonCalendar = [&stream](const CalendarInstant& instant) {
                        stream << "[" << instant.mYear << "," << instant.mMonth << "," << instant.mDay << ","
                               << std::setprecision(17) << instant.mHour << "]";
                    };
                    stream << "{\"start\":";
                    writeJsonCalendar(actor.mScheduleWindow->mStart);
                    stream << ",\"end\":";
                    writeJsonCalendar(actor.mScheduleWindow->mEnd);
                    stream << ",\"duration_hours\":" << std::setprecision(17)
                           << actor.mScheduleWindow->mDurationHours << "}";
                }
                stream << ",\"condition_result\":" << static_cast<unsigned>(actor.mConditionResult)
                       << ",\"source\":"
                       << static_cast<unsigned>(actor.mSource) << ",\"package_type\":"
                       << static_cast<unsigned>(actor.mPackageType) << ",\"procedure\":"
                       << static_cast<unsigned>(actor.mProcedure) << ",\"phase\":"
                       << static_cast<unsigned>(actor.mPhase) << ",\"tier\":"
                       << static_cast<unsigned>(actor.mTier) << ",\"boundary\":"
                       << static_cast<unsigned>(actor.mBoundary) << ",\"list_index\":" << actor.mListIndex
                       << ",\"path_node\":" << actor.mPathNode << ",\"repath_attempts\":"
                       << actor.mRepathAttempts << ",\"formation_index\":" << actor.mFormationIndex
                       << ",\"selection_generation\":" << actor.mSelectionGeneration
                       << ",\"route_generation\":" << actor.mRouteGeneration
                       << ",\"transition_generation\":" << actor.mTransitionGeneration
                       << ",\"action_timer\":" << std::setprecision(17) << actor.mActionTimer
                       << ",\"duration_remaining\":" << actor.mDurationRemaining
                       << ",\"no_progress_seconds\":" << actor.mNoProgressSeconds
                       << ",\"door_cooldown\":" << actor.mDoorCooldown
                       << ",\"low_process_timer\":" << actor.mLowProcessTimer
                       << ",\"next_low_process_tick\":" << actor.mNextLowProcessTick
                       << ",\"has_destination\":" << (actor.mHasDestination ? "true" : "false");
                if (mVersion >= 7)
                    stream << ",\"door_animation_started\":"
                           << (actor.mDoorAnimationStarted ? "true" : "false");
                stream << ",\"restrained\":"
                       << (actor.mRestrained ? "true" : "false") << ",\"action_reserved\":"
                       << (actor.mActionReserved ? "true" : "false") << ",\"interruption_reason\":\""
                       << escapeJson(actor.mInterruptionReason) << "\"}";
            }
            stream << "],\"path_points\":[";
            std::vector<RuntimePathPointState> points = mPathPoints;
            std::sort(points.begin(), points.end(), [](const auto& left, const auto& right) {
                return std::tie(left.mPathgrid, left.mNode) < std::tie(right.mPathgrid, right.mNode);
            });
            for (std::size_t i = 0; i < points.size(); ++i)
            {
                if (i)
                    stream << ',';
                stream << "{\"pathgrid\":\"" << escapeJson(points[i].mPathgrid.serialize())
                       << "\",\"node\":" << points[i].mNode << ",\"enabled\":"
                       << (points[i].mEnabled ? "true" : "false") << "}";
            }
            stream << "],\"companions\":[";
            std::vector<RuntimeCompanionRelation> companions = mCompanions;
            std::sort(companions.begin(), companions.end(), [](const auto& left, const auto& right) {
                return std::tie(left.mLeader, left.mMember) < std::tie(right.mLeader, right.mMember);
            });
            for (std::size_t i = 0; i < companions.size(); ++i)
            {
                if (i)
                    stream << ',';
                stream << "{\"leader\":\"" << escapeJson(companions[i].mLeader.serialize())
                       << "\",\"member\":\"" << escapeJson(companions[i].mMember.serialize())
                       << "\",\"group\":\"" << escapeJson(companions[i].mGroup.serialize())
                       << "\",\"side_with\":\"" << escapeJson(companions[i].mSideWith.serialize())
                       << "\",\"formation_index\":" << companions[i].mFormationIndex << "}";
            }
            stream << "],\"mounts\":[";
            std::vector<RuntimeMountRelation> mounts = mMounts;
            std::sort(mounts.begin(), mounts.end(), [](const auto& left, const auto& right) {
                return std::tie(left.mHorse, left.mRider) < std::tie(right.mHorse, right.mRider);
            });
            for (std::size_t i = 0; i < mounts.size(); ++i)
            {
                if (i)
                    stream << ',';
                stream << "{\"horse\":\"" << escapeJson(mounts[i].mHorse.serialize())
                       << "\",\"rider\":\"" << escapeJson(mounts[i].mRider.serialize())
                       << "\",\"owner\":\"" << escapeJson(mounts[i].mOwner.serialize())
                       << "\",\"last_ridden\":\"" << escapeJson(mounts[i].mLastRidden.serialize())
                       << "\",\"mounted\":" << (mounts[i].mMounted ? "true" : "false") << "}";
            }
            stream << "],\"detection_vectors\":[";
            std::vector<RuntimeDetectionVector> detectionVectors = mDetectionVectors;
            std::sort(detectionVectors.begin(), detectionVectors.end(), [](const auto& left, const auto& right) {
                return std::tie(left.mObserver, left.mTarget) < std::tie(right.mObserver, right.mTarget);
            });
            for (std::size_t i = 0; i < detectionVectors.size(); ++i)
            {
                if (i)
                    stream << ',';
                const RuntimeDetectionVector& vector = detectionVectors[i];
                stream << "{\"observer\":\"" << escapeJson(vector.mObserver.serialize())
                       << "\",\"target\":\"" << escapeJson(vector.mTarget.serialize())
                       << "\",\"score\":" << std::setprecision(17) << vector.mScore
                       << ",\"detected\":" << (vector.mDetected ? "true" : "false")
                       << ",\"line_of_sight\":" << (vector.mLineOfSight ? "true" : "false") << "}";
            }
            stream << "]";
        }
        if (mVersion >= 6)
        {
            stream << ",\"pending_package_done\":[";
            for (std::size_t i = 0; i < mPendingPackageDone.size(); ++i)
            {
                if (i)
                    stream << ',';
                const RuntimePackageDoneEvent& event = mPendingPackageDone[i];
                stream << "{\"actor\":\"" << escapeJson(event.mActor.serialize())
                       << "\",\"package\":\"" << escapeJson(event.mPackage.serialize()) << "\"}";
            }
            stream << "]";
        }
        if (mVersion >= 8)
        {
            stream << ",\"physical_actions\":{\"next\":" << mPhysicalActions.mNext << ",\"pending\":[";
            auto pending = mPhysicalActions.mPending;
            std::sort(pending.begin(), pending.end());
            for (std::size_t i = 0; i < pending.size(); ++i)
            {
                if (i)
                    stream << ',';
                stream << pending[i];
            }
            stream << "]}";
        }
        if (mVersion >= 9)
        {
            stream << ",\"native_actor_values\":[";
            auto actors = mNativeActorValues;
            std::sort(actors.begin(), actors.end(), [](const auto& a, const auto& b) { return a.mActor < b.mActor; });
            for (std::size_t i = 0; i < actors.size(); ++i)
            {
                if (i)
                    stream << ',';
                const auto& actor = actors[i];
                stream << "{\"actor\":\"" << escapeJson(actor.mActor.serialize())
                       << "\",\"base\":\"" << escapeJson(actor.mBase.serialize())
                       << "\",\"owner\":" << static_cast<unsigned>(actor.mOwner)
                       << ",\"process\":" << static_cast<unsigned>(actor.mProcess) << ",\"values\":[";
                for (std::size_t j = 0; j < actor.mValues.size(); ++j)
                {
                    if (j)
                        stream << ',';
                    const auto& value = actor.mValues[j];
                    stream << '[' << value.mBase;
                    for (const auto& modifier : value.mModifiers)
                    {
                        stream << ',';
                        if (modifier)
                            stream << *modifier;
                        else
                            stream << "null";
                    }
                    stream << ']';
                }
                stream << ']';
                if (mVersion >= 25)
                {
                    stream << ",\"process_knocked_state\":";
                    if (actor.mProcessKnockedState)
                        stream << static_cast<int>(*actor.mProcessKnockedState);
                    else
                        stream << "null";
                }
                if (mVersion >= 26)
                {
                    stream << ",\"process_action\":";
                    if (actor.mProcessAction)
                        stream << *actor.mProcessAction;
                    else
                        stream << "null";
                }
                if (mVersion >= 10)
                {
                    stream << ",\"player_form_values\":";
                    if (actor.mPlayerFormValues)
                    {
                        stream << '[';
                        for (std::size_t j = 0; j < actor.mPlayerFormValues->size(); ++j)
                        {
                            if (j)
                                stream << ',';
                            stream << (*actor.mPlayerFormValues)[j];
                        }
                        stream << ']';
                    }
                    else
                        stream << "null";
                }
                if (mVersion >= 17)
                {
                    stream << ",\"nonplayer_form_health\":";
                    if (actor.mNonPlayerFormHealth)
                        stream << *actor.mNonPlayerFormHealth;
                    else
                        stream << "null";
                }
                if (mVersion >= 18)
                {
                    stream << ",\"passive_abilities\":";
                    if (!actor.mPassiveAbilities)
                        stream << "null";
                    else
                    {
                        stream << '[';
                        for (std::size_t j = 0; j < actor.mPassiveAbilities->size(); ++j)
                        {
                            if (j)
                                stream << ',';
                            const auto& ability = (*actor.mPassiveAbilities)[j];
                            stream << "{\"spell\":";
                            stream << '"' << escapeJson(ability.mSpell.serialize()) << '"';
                            stream << ",\"effects\":[";
                            for (std::size_t k = 0; k < ability.mEffects.size(); ++k)
                            {
                                if (k)
                                    stream << ',';
                                const auto& effect = ability.mEffects[k];
                                stream << '[' << effect.mEffectIndex << ',' << effect.mCode << ','
                                    << effect.mActorValue << ',' << effect.mStoredMagnitude;
                                if (mVersion >= 19)
                                {
                                    stream << ',';
                                    if (effect.mInitialMagnitude)
                                        stream << *effect.mInitialMagnitude;
                                    else
                                        stream << "null";
                                }
                                stream << ']';
                            }
                            stream << "]}";
                        }
                        stream << ']';
                    }
                }
                stream << '}';
            }
            stream << ']';
        }
        if (mVersion >= 11)
        {
            stream << ",\"native_actor_bases\":[";
            auto bases = mNativeActorBases;
            std::sort(bases.begin(), bases.end(), [](const auto& a, const auto& b) { return a.mBase < b.mBase; });
            for (std::size_t i = 0; i < bases.size(); ++i)
            {
                if (i)
                    stream << ',';
                auto& base = bases[i];
                stream << "{\"base\":\"" << escapeJson(base.mBase.serialize()) << "\",\"kind\":"
                       << static_cast<unsigned>(base.mKind) << ",\"values\":[";
                std::sort(base.mValues.begin(), base.mValues.end(),
                    [](const auto& a, const auto& b) { return a.mActorValue < b.mActorValue; });
                for (std::size_t j = 0; j < base.mValues.size(); ++j)
                {
                    if (j)
                        stream << ',';
                    const auto& value = base.mValues[j];
                    stream << '[' << static_cast<unsigned>(value.mActorValue) << ',' << value.mValue.index() << ',';
                    std::visit([&](auto number) { stream << number; }, value.mValue);
                    stream << ']';
                }
                stream << "]}";
            }
            stream << ']';
        }
        if (mVersion >= 12)
        {
            stream << ",\"native_actor_life\":[";
            auto lives = mNativeActorLife;
            std::sort(lives.begin(), lives.end(), [](const auto& a, const auto& b) { return a.mActor < b.mActor; });
            for (std::size_t i = 0; i < lives.size(); ++i)
            {
                const auto& life = lives[i];
                if (i) stream << ',';
                stream << "{\"actor\":\"" << escapeJson(life.mActor.serialize()) << "\",\"base\":\""
                       << escapeJson(life.mBase.serialize()) << "\",\"phase\":" << static_cast<unsigned>(life.mPhase)
                       << ",\"recovery_remaining\":" << std::setprecision(17) << life.mRecoveryRemaining
                       << ",\"killer\":\"" << escapeJson(life.mKiller.serialize()) << "\"}";
            }
            stream << "],\"next_death_event\":" << mNextDeathEvent << ",\"pending_death_events\":[";
            for (std::size_t i = 0; i < mPendingDeathEvents.size(); ++i)
            {
                const auto& event = mPendingDeathEvents[i];
                if (i) stream << ',';
                stream << "{\"id\":" << event.mId << ",\"actor\":\"" << escapeJson(event.mActor.serialize())
                       << "\",\"killer\":\"" << escapeJson(event.mKiller.serialize()) << "\"}";
            }
            stream << ']';
        }
        if (mVersion >= 13)
        {
            stream << ",\"native_death_counts\":[";
            bool first = true;
            for (const auto& [base, count] : mNativeDeathCounts)
            {
                if (!first) stream << ',';
                first = false;
                stream << "{\"base\":\"" << escapeJson(base.serialize()) << "\",\"count\":" << count << '}';
            }
            stream << ']';
        }
        if (mVersion >= 14)
        {
            stream << ",\"native_actor_breath\":[";
            bool first = true;
            for (const auto& [actor, remaining] : mNativeActorBreath)
            {
                if (!first) stream << ',';
                first = false;
                stream << "{\"actor\":\"" << escapeJson(actor.serialize())
                       << "\",\"remaining\":" << std::setprecision(17) << remaining << '}';
            }
            stream << ']';
        }
        if (mVersion >= 15)
        {
            stream << ",\"native_combat_engagements\":[";
            bool firstEntry = true;
            for (const auto& [first, second] : mNativeCombatEngagements)
            {
                if (!firstEntry) stream << ',';
                firstEntry = false;
                stream << "{\"first\":\"" << escapeJson(first.serialize())
                       << "\",\"second\":\"" << escapeJson(second.serialize()) << "\"}";
            }
            stream << ']';
        }
        if (mVersion >= 16)
        {
            // JSON readers commonly parse "-0" as an integer and lose its
            // sign. Keep these binary32 clock values explicitly fractional.
            stream << ",\"native_actor_manager_time\":" << std::showpoint << std::setprecision(17) << mNativeActorManagerTime
                   << ",\"native_actor_update_times\":[";
            bool first = true;
            for (const auto& [actor, time] : mNativeActorUpdateTimes)
            {
                if (!first) stream << ',';
                first = false;
                stream << "{\"actor\":\"" << escapeJson(actor.serialize())
                       << "\",\"time\":" << std::setprecision(17) << time << '}';
            }
            stream << ']' << std::noshowpoint;
        }
        if (mVersion >= 20)
        {
            stream << ",\"physical_action_owners\":[";
            bool first = true;
            for (const auto& [id, actor] : mPhysicalActionOwners)
            {
                if (!first) stream << ',';
                first = false;
                stream << "{\"id\":" << id << ",\"actor\":\"" << escapeJson(actor.serialize()) << "\"}";
            }
            stream << ']';
        }
        if (mVersion >= 21)
        {
            stream << ",\"native_melee_states\":[";
            bool first = true;
            for (const auto& [actor, melee] : mNativeMeleeStates)
            {
                if (!first) stream << ',';
                first = false;
                stream << "{\"actor\":\"" << escapeJson(actor.serialize()) << '\"';
                if (mVersion >= 28)
                {
                    stream << ",\"ai_intent\":";
                    if (!melee.mAiIntent) stream << "null";
                    else stream << "{\"target\":\"" << escapeJson(melee.mAiIntent->mTarget.serialize())
                        << "\",\"style\":\"" << escapeJson(melee.mAiIntent->mStyle.serialize()) << "\"}";
                }
                stream << ",\"input\":{\"held_seconds\":" << std::setprecision(17) << melee.mInput.mHeldSeconds
                    << ",\"input_held\":" << (melee.mInput.mInputHeld ? "true" : "false")
                    << ",\"prefer_left\":" << (melee.mInput.mPreferLeft ? "true" : "false")
                    << ",\"queued\":" << static_cast<unsigned>(melee.mInput.mQueued) << "},\"strike\":";
                if (!melee.mStrike) stream << "null";
                else
                {
                    const auto& strike = *melee.mStrike;
                    stream << "{\"id\":" << strike.mActionId << ",\"kind\":" << static_cast<unsigned>(strike.mKind)
                        << ",\"weapon_base\":\"" << escapeJson(strike.mWeaponBase.serialize())
                        << "\",\"animation_group\":\"" << escapeJson(strike.mAnimationGroup)
                        << "\",\"playback_speed\":" << std::setprecision(17) << strike.mPlaybackSpeed
                        << ",\"animation_time\":" << std::setprecision(17) << strike.mAnimationTime
                        << ",\"contact_committed\":" << (strike.mContactCommitted ? "true" : "false");
                    if (mVersion >= 22)
                        stream << ",\"ordinary_phase\":" << static_cast<unsigned>(strike.mOrdinaryPhase);
                    if (mVersion >= 23)
                    {
                        const auto floating = [&](float value) {
                            if (value == 0 && std::signbit(value)) stream << "-0.0";
                            else stream << std::setprecision(17) << value;
                        };
                        stream << ",\"sequence_timing\":";
                        if (!strike.mSequenceTiming) stream << "null";
                        else
                        {
                            const auto& timing = *strike.mSequenceTiming;
                            stream << "{\"easing\":" << (timing.mEasing ? "true" : "false");
                            const auto optional = [&](std::string_view name, std::optional<float> value) {
                                stream << ",\"" << name << "\":";
                                if (value) floating(*value);
                                else stream << "null";
                            };
                            optional("offset", timing.mOffset);
                            optional("ease_start", timing.mEaseStart);
                            optional("last_input", timing.mLastInput);
                            stream << ",\"ease_end\":"; floating(timing.mEaseEnd);
                            stream << ",\"weighted_time\":"; floating(timing.mWeightedTime);
                            stream << ",\"output_time\":"; floating(timing.mOutputTime);
                            stream << '}';
                        }
                    }
                    stream << '}';
                }
                stream << '}';
            }
            stream << ']';
        }
        if (mVersion >= 23)
        {
            stream << ",\"native_animation_clocks\":[";
            bool first = true;
            for (const auto& [actor, clock] : mNativeAnimationClocks)
            {
                if (!first) stream << ',';
                first = false;
                stream << "{\"actor\":\"" << escapeJson(actor.serialize()) << "\",\"clock\":";
                if (clock == 0 && std::signbit(clock)) stream << "-0.0";
                else stream << std::setprecision(17) << clock;
                stream << '}';
            }
            stream << ']';
        }
        if (mVersion >= 27)
            stream << ",\"combat_rng_state\":" << mCombatRngState;
        if (mVersion >= 30)
        {
            stream << ",\"native_actor_knockback\":[";
            bool first = true;
            for (const auto& [actor, pulse] : mNativeActorKnockback)
            {
                if (!first) stream << ',';
                first = false;
                stream << "{\"actor\":\"" << escapeJson(actor.serialize()) << "\",\"acceleration\":[";
                const auto floating = [&](float value) {
                    if (value == 0 && std::signbit(value)) stream << "-0.0";
                    else stream << std::setprecision(17) << value;
                };
                for (std::size_t i = 0; i < pulse.mAcceleration.size(); ++i)
                {
                    if (i) stream << ',';
                    floating(pulse.mAcceleration[i]);
                }
                stream << "],\"remaining\":";
                floating(pulse.mRemaining);
                stream << '}';
            }
            stream << ']';
        }
        if (mVersion >= 31)
        {
            stream << ",\"native_actor_ragdolls\":[";
            bool first = true;
            const auto vector = [&](const auto& values) {
                stream << '[';
                for (std::size_t i = 0; i < values.size(); ++i)
                {
                    if (i) stream << ',';
                    const float value = values[i];
                    if (value == 0 && std::signbit(value)) stream << "-0.0";
                    else stream << std::setprecision(17) << value;
                }
                stream << ']';
            };
            for (const auto& [actor, pose] : mNativeActorRagdolls)
            {
                if (!first) stream << ',';
                first = false;
                stream << "{\"actor\":\"" << escapeJson(actor.serialize()) << "\",\"base\":\""
                       << escapeJson(pose.mBase.serialize()) << "\",\"model\":\"" << escapeJson(pose.mModel)
                       << "\",\"asset_hash\":\"" << pose.mAssetHash << "\",\"bodies\":[";
                for (std::size_t i = 0; i < pose.mBodies.size(); ++i)
                {
                    if (i) stream << ',';
                    const auto& body = pose.mBodies[i];
                    stream << "{\"record\":" << body.mRecord << ",\"node_record\":" << body.mNodeRecord
                           << ",\"rotation\":";
                    vector(body.mRotation);
                    stream << ",\"position\":";
                    vector(body.mPosition);
                    stream << ",\"linear_velocity\":";
                    vector(body.mLinearVelocity);
                    stream << ",\"angular_velocity\":";
                    vector(body.mAngularVelocity);
                    if (body.mNativePackedVelocity)
                    {
                        stream << ",\"native_packed_velocity\":{\"linear\":";
                        vector(body.mNativePackedVelocity->mLinear);
                        stream << ",\"angular\":";
                        vector(body.mNativePackedVelocity->mAngular);
                        stream << '}';
                    }
                    if (body.mNativeMotion)
                        stream << ",\"native_motion\":" << static_cast<unsigned>(*body.mNativeMotion);
                    stream << '}';
                }
                stream << ']';
                if (pose.mNativeBlends)
                {
                    stream << ",\"native_blends\":[";
                    for (std::size_t i = 0; i < pose.mNativeBlends->size(); ++i)
                    {
                        if (i) stream << ',';
                        const auto& blend = (*pose.mNativeBlends)[i];
                        const auto scalar = [&](float value) {
                            if (value == 0 && std::signbit(value)) stream << "-0.0";
                            else stream << std::setprecision(17) << value;
                        };
                        stream << "{\"body_record\":" << blend.mBodyRecord
                            << ",\"collision_flags\":" << blend.mCollisionFlags
                            << ",\"requested_motion\":" << blend.mRequestedMotion
                            << ",\"hierarchy_gain\":";
                        scalar(blend.mHierarchyGain);
                        stream << ",\"velocity_gain\":"; scalar(blend.mVelocityGain);
                        stream << '}';
                    }
                    stream << ']';
                }
                if (pose.mNativeControllers)
                {
                    const auto scalar = [&](float value) {
                        if (value == 0 && std::signbit(value)) stream << "-0.0";
                        else stream << std::setprecision(17) << value;
                    };
                    const auto target = [&](std::optional<std::uint32_t> node) {
                        if (node) stream << *node; else stream << "null";
                    };
                    const auto common = [&](const PhysicalBlendTiming& timing, const PhysicalBlendClock& clock) {
                        stream << "\"timing\":{\"flags\":" << timing.mFlags << ",\"frequency\":"; scalar(timing.mFrequency);
                        stream << ",\"phase\":"; scalar(timing.mPhase); stream << ",\"start_key\":"; scalar(timing.mStartKey);
                        stream << ",\"stop_key\":"; scalar(timing.mStopKey);
                        stream << "},\"clock\":{\"start_time\":"; scalar(clock.mStartTime);
                        stream << ",\"previous_time\":"; scalar(clock.mPreviousTime); stream << ",\"elapsed\":"; scalar(clock.mElapsed);
                        stream << '}';
                    };
                    stream << ",\"native_controllers\":{\"blends\":[";
                    for (std::size_t i = 0; i < pose.mNativeControllers->mBlends.size(); ++i)
                    {
                        if (i) stream << ',';
                        const auto& controller = pose.mNativeControllers->mBlends[i]; const auto& state = controller.mState;
                        stream << "{\"record\":" << controller.mRecord << ",\"attached_node\":" << controller.mAttachedNode << ",\"target_node\":";
                        target(controller.mTargetNode); stream << ",\"state\":{"; common(state.mTiming, state.mClock);
                        stream << ",\"keys\":[";
                        for (std::size_t k = 0; k < state.mKeys.size(); ++k)
                        {
                            if (k) stream << ',';
                            const auto& key = state.mKeys[k];
                            stream << "{\"time\":"; scalar(key.mTime); stream << ",\"hierarchy_gain\":"; scalar(key.mGains.mHierarchy);
                            stream << ",\"velocity_gain\":"; scalar(key.mGains.mVelocity); stream << '}';
                        }
                        stream << "],\"cursor\":" << state.mCursor << ",\"cached_gains\":{\"hierarchy\":"; scalar(state.mCachedGains.mHierarchy);
                        stream << ",\"velocity\":"; scalar(state.mCachedGains.mVelocity);
                        stream << "},\"setup_state\":" << state.mSetupState << "}}";
                    }
                    stream << "],\"velocities\":[";
                    for (std::size_t i = 0; i < pose.mNativeControllers->mVelocities.size(); ++i)
                    {
                        if (i) stream << ',';
                        const auto& controller = pose.mNativeControllers->mVelocities[i]; const auto& state = controller.mState;
                        stream << "{\"attached_node\":" << controller.mAttachedNode << ",\"target_node\":"; target(controller.mTargetNode);
                        stream << ",\"precedes_blend\":" << (controller.mPrecedesBlend ? "true" : "false") << ",\"state\":{";
                        common(state.mTiming, state.mClock); stream << ",\"force_vector\":"; vector(state.mForceVector);
                        stream << ",\"frame_delta\":"; scalar(state.mFrameDelta); stream << "}}";
                    }
                    stream << "]}";
                }
                stream << '}';
            }
            stream << ']';
        }
        if (mNativePhysicalBlendTimeCache)
        {
            const auto& cache = *mNativePhysicalBlendTimeCache;
            const auto scalar = [&](float value) {
                if (value == 0.f && std::signbit(value)) stream << "-0.0";
                else stream << value;
            };
            stream << ",\"native_physical_blend_time_cache\":{\"cycle\":" << cache.mCycle;
            stream << ",\"stop_key\":"; scalar(cache.mStopKey);
            stream << ",\"start_key\":"; scalar(cache.mStartKey);
            stream << ",\"key_time\":"; scalar(cache.mKeyTime);
            stream << ",\"result\":"; scalar(cache.mResult); stream << '}';
        }
        if (mNativePlayerBowTimer)
        {
            stream << ",\"native_player_bow_timer\":";
            if (*mNativePlayerBowTimer == 0 && std::signbit(*mNativePlayerBowTimer))
                stream << "-0.0";
            else
                stream << *mNativePlayerBowTimer;
        }
        stream << "}";
        return stream.str();
    }
}
