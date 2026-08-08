#include "formkey.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

#include "fourcc.hpp"

namespace ESM
{
    namespace
    {
        std::uint64_t parseHex(std::string_view value)
        {
            std::uint64_t result = 0;
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result, 16);
            if (parsed.ec != std::errc() || parsed.ptr != value.data() + value.size())
                throw std::invalid_argument("Invalid FormKey hexadecimal value");
            return result;
        }

        void validateNamespace(std::string_view value)
        {
            if (value.empty() || value.find(':') != std::string_view::npos)
                throw std::invalid_argument("FormKey namespace must be non-empty and may not contain ':'");
        }

        constexpr std::array<std::uint8_t, 8> sFormKeyIndexMagic{ 'O', 'M', 'W', '4', 'F', 'K', 'I', 'X' };
        constexpr std::uint32_t sFormKeyIndexVersion = 1;
        constexpr std::size_t sMaxSerializedGraphSize = 512 * 1024 * 1024;
        constexpr std::uint32_t sMaxSerializedStringSize = 16 * 1024 * 1024;
        constexpr std::uint32_t sMaxSerializedCollectionSize = 8 * 1024 * 1024;

        class BinaryWriter
        {
        public:
            void uint8(std::uint8_t value) { mData.push_back(value); }

            void uint32(std::uint32_t value)
            {
                for (unsigned i = 0; i < 4; ++i)
                    mData.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
            }

            void string(std::string_view value)
            {
                if (value.size() > sMaxSerializedStringSize)
                    throw std::length_error("FormKey graph string exceeds the codec limit");
                uint32(static_cast<std::uint32_t>(value.size()));
                mData.insert(mData.end(), value.begin(), value.end());
            }

            std::vector<std::uint8_t> take()
            {
                if (mData.size() > sMaxSerializedGraphSize)
                    throw std::length_error("FormKey graph exceeds the codec limit");
                return std::move(mData);
            }

        private:
            std::vector<std::uint8_t> mData;
        };

        class BinaryReader
        {
        public:
            explicit BinaryReader(std::span<const std::uint8_t> data)
                : mData(data)
            {
                if (data.size() > sMaxSerializedGraphSize)
                    throw std::length_error("Serialized FormKey graph exceeds the codec limit");
            }

            std::uint8_t uint8()
            {
                require(1);
                return mData[mPosition++];
            }

            std::uint32_t uint32()
            {
                require(4);
                std::uint32_t value = 0;
                for (unsigned i = 0; i < 4; ++i)
                    value |= static_cast<std::uint32_t>(mData[mPosition++]) << (i * 8);
                return value;
            }

            std::string string()
            {
                const std::uint32_t size = uint32();
                if (size > sMaxSerializedStringSize)
                    throw std::length_error("Serialized FormKey graph string exceeds the codec limit");
                require(size);
                std::string result(reinterpret_cast<const char*>(mData.data() + mPosition), size);
                mPosition += size;
                return result;
            }

            std::uint32_t collectionSize()
            {
                const std::uint32_t size = uint32();
                if (size > sMaxSerializedCollectionSize)
                    throw std::length_error("Serialized FormKey graph collection exceeds the codec limit");
                return size;
            }

            bool empty() const { return mPosition == mData.size(); }

        private:
            void require(std::size_t size)
            {
                if (size > mData.size() - mPosition)
                    throw std::runtime_error("Truncated FormKey graph state");
            }

            std::span<const std::uint8_t> mData;
            std::size_t mPosition = 0;
        };

        void writeKey(BinaryWriter& writer, const FormKey& value)
        {
            writer.string(value.serialize());
        }

        FormKey readKey(BinaryReader& reader)
        {
            return FormKey::deserialize(reader.string());
        }

        std::string jsonEscape(std::string_view value)
        {
            std::ostringstream stream;
            for (const unsigned char c : value)
            {
                switch (c)
                {
                    case '\"':
                        stream << "\\\"";
                        break;
                    case '\\':
                        stream << "\\\\";
                        break;
                    case '\b':
                        stream << "\\b";
                        break;
                    case '\f':
                        stream << "\\f";
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
                            stream << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                                   << static_cast<unsigned>(c) << std::dec;
                        else
                            stream << static_cast<char>(c);
                }
            }
            return stream.str();
        }

        void rotateCycleToCanonicalStart(std::vector<FormKey>& cycle)
        {
            if (cycle.empty())
                return;
            const auto first = std::min_element(cycle.begin(), cycle.end());
            std::rotate(cycle.begin(), first, cycle.end());
        }
    }

    std::string normalizePluginName(std::string_view value)
    {
        std::string result = std::filesystem::path(value).filename().generic_string();
        if (result.empty())
            throw std::invalid_argument("Content plugin name must be non-empty");
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
            return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : static_cast<char>(c);
        });
        if (result.find(':') != std::string::npos)
            throw std::invalid_argument("Content plugin name may not contain ':'");
        return result;
    }

    FormKey FormKey::content(std::string_view plugin, std::uint32_t localId)
    {
        if (localId > 0x00ffffff)
            throw std::invalid_argument("Content FormKey local ID exceeds 24 bits");
        if (localId == 0)
            return {};
        return { FormKeyKind::Content, normalizePluginName(plugin), localId };
    }

    FormKey FormKey::dynamic(std::string_view nameSpace, std::uint64_t serial)
    {
        validateNamespace(nameSpace);
        if (serial == 0)
            throw std::invalid_argument("Dynamic FormKey serial must be non-zero");
        return { FormKeyKind::Dynamic, std::string(nameSpace), serial };
    }

    std::uint32_t FormKey::localId() const
    {
        if (!isContent())
            throw std::logic_error("Only content FormKeys have a local ID");
        return static_cast<std::uint32_t>(mValue);
    }

    std::string FormKey::serialize() const
    {
        if (isNull())
            return "null";
        std::ostringstream stream;
        stream << (isContent() ? "content:" : "dynamic:") << mNamespace << ':' << std::hex << std::setfill('0')
               << std::setw(isContent() ? 6 : 16) << mValue;
        return stream.str();
    }

    FormKey FormKey::deserialize(std::string_view value)
    {
        if (value == "null")
            return {};
        const std::size_t first = value.find(':');
        const std::size_t second = first == std::string_view::npos ? first : value.find(':', first + 1);
        if (first == std::string_view::npos || second == std::string_view::npos || second + 1 == value.size())
            throw std::invalid_argument("Invalid serialized FormKey");
        const std::string_view kind = value.substr(0, first);
        const std::string_view nameSpace = value.substr(first + 1, second - first - 1);
        const std::uint64_t parsed = parseHex(value.substr(second + 1));
        if (kind == "content")
        {
            if (parsed > 0x00ffffff)
                throw std::invalid_argument("Serialized content FormKey exceeds 24 bits");
            return content(nameSpace, static_cast<std::uint32_t>(parsed));
        }
        if (kind == "dynamic")
            return dynamic(nameSpace, parsed);
        throw std::invalid_argument("Unknown serialized FormKey kind");
    }

    std::ostream& operator<<(std::ostream& stream, const FormKey& value)
    {
        return stream << value.serialize();
    }

    FormKeyResolver::FormKeyResolver(std::vector<std::string> contentFiles)
    {
        mContentFiles.reserve(contentFiles.size());
        for (std::string& file : contentFiles)
        {
            std::string normalized = normalizePluginName(file);
            const auto [it, inserted] = mContentFileIndices.emplace(normalized, mContentFiles.size());
            if (!inserted)
                throw std::invalid_argument("Duplicate content plugin identity: " + normalized);
            mContentFiles.push_back(std::move(normalized));
        }
    }

    FormKey FormKeyResolver::toFormKey(FormId value) const
    {
        if (value.isZeroOrUnset())
            return {};
        if (value.hasContentFile())
        {
            if (static_cast<std::size_t>(value.mContentFile) >= mContentFiles.size())
                throw std::out_of_range("FormId content-file index is outside the current load order");
            return FormKey::content(mContentFiles[value.mContentFile], value.mIndex);
        }
        // Promote before negation so INT32_MIN is handled without signed overflow.
        const std::uint64_t namespaceIndex
            = static_cast<std::uint64_t>(-static_cast<std::int64_t>(value.mContentFile) - 1);
        return FormKey::dynamic("openmw", (namespaceIndex << 32) | value.mIndex);
    }

    std::optional<FormId> FormKeyResolver::toFormId(const FormKey& value) const
    {
        if (value.isNull())
            return FormId{};
        if (value.isContent())
        {
            const auto it = mContentFileIndices.find(normalizePluginName(value.mNamespace));
            if (it == mContentFileIndices.end())
                return std::nullopt;
            return FormId{ value.localId(), static_cast<std::int32_t>(it->second) };
        }
        if (value.mNamespace != "openmw")
            return std::nullopt;
        const std::uint64_t namespaceIndex = value.mValue >> 32;
        if (namespaceIndex > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
            return std::nullopt;
        return FormId{ static_cast<std::uint32_t>(value.mValue), -static_cast<std::int32_t>(namespaceIndex) - 1 };
    }

    FormKey FormKeyResolver::resolveRaw(
        FormId value, std::string_view sourcePlugin, const std::vector<std::string>& masters)
    {
        if (value.isZeroOrUnset())
            return {};
        if (value.hasContentFile() && static_cast<std::size_t>(value.mContentFile) < masters.size())
            return FormKey::content(masters[value.mContentFile], value.mIndex);
        return FormKey::content(sourcePlugin, value.mIndex);
    }

    DynamicFormKeyAllocator::DynamicFormKeyAllocator(std::string_view nameSpace, std::uint64_t nextSerial)
        : mNamespace(nameSpace)
        , mNextSerial(nextSerial)
    {
        validateNamespace(nameSpace);
        if (nextSerial == 0)
            throw std::invalid_argument("Dynamic FormKey allocator serial must be non-zero");
    }

    FormKey DynamicFormKeyAllocator::allocate()
    {
        if (mNextSerial == std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("Dynamic FormKey namespace is exhausted");
        return FormKey::dynamic(mNamespace, mNextSerial++);
    }

    void FormKeyIndex::apply(FormRecordMetadata value)
    {
        if (value.mKey.isNull())
            throw std::invalid_argument("Cannot index a null FormKey");
        value.mWinningPlugin = normalizePluginName(value.mWinningPlugin);
        if (value.mParent && value.mParent->isNull())
            throw std::invalid_argument("FormKey graph parent may not be null");
        if (value.mEnableParent && value.mEnableParent->isNull())
            throw std::invalid_argument("FormKey graph enable parent may not be null");
        for (const FormRecordMetadata::Reference& reference : value.mReferences)
            if (reference.mTarget.isNull())
                throw std::invalid_argument("FormKey graph reference target may not be null");
        mRecords[value.mKey].push_back(std::move(value));
    }

    const FormRecordMetadata* FormKeyIndex::winner(const FormKey& key) const
    {
        const auto it = mRecords.find(key);
        return it == mRecords.end() || it->second.empty() ? nullptr : &it->second.back();
    }

    const FormRecordMetadata* FormKeyIndex::resolve(const FormKey& key) const
    {
        const FormRecordMetadata* result = winner(key);
        return result != nullptr && !result->mDeleted ? result : nullptr;
    }

    const std::vector<FormRecordMetadata>* FormKeyIndex::history(const FormKey& key) const
    {
        const auto it = mRecords.find(key);
        return it == mRecords.end() ? nullptr : &it->second;
    }

    FormKey FormKeyIndex::resolveRecordHeader(
        const FormKey& preferred, const FormKey& sourceCandidate, std::uint32_t recordType) const
    {
        if (preferred.isNull() || sourceCandidate.isNull())
            throw std::invalid_argument("Record-header FormKeys may not be null");
        // TES4 permits index zero in an ESP to mean either its first master or
        // the ESP itself. Masters have already been indexed, so an existing
        // preferred key is an override; otherwise this is a new local form.
        const FormRecordMetadata* existing = winner(preferred);
        return preferred != sourceCandidate
                && (existing == nullptr || (recordType != 0 && existing->mRecordType != recordType))
            ? sourceCandidate
            : preferred;
    }

    std::vector<FormKey> FormKeyIndex::unresolvedEnableParents() const
    {
        std::vector<FormKey> result;
        for (const auto& [key, history] : mRecords)
        {
            const FormRecordMetadata& record = history.back();
            if (!record.mDeleted && record.mEnableParent && resolve(*record.mEnableParent) == nullptr)
                result.push_back(key);
        }
        return result;
    }

    std::vector<UnresolvedFormReference> FormKeyIndex::unresolvedReferences() const
    {
        std::vector<UnresolvedFormReference> result;
        const auto add = [&](const FormRecordMetadata& record, const FormKey& target, std::uint32_t subRecordType,
                             std::uint32_t occurrence) {
            const FormRecordMetadata* targetWinner = winner(target);
            if (targetWinner != nullptr && !targetWinner->mDeleted)
                return;
            result.push_back({ record.mKey, target, record.mWinningPlugin, record.mRecordType, subRecordType,
                occurrence, targetWinner == nullptr ? UnresolvedFormReferenceReason::Missing
                                                    : UnresolvedFormReferenceReason::Deleted });
        };

        for (const auto& [key, history] : mRecords)
        {
            const FormRecordMetadata& record = history.back();
            if (record.mDeleted)
                continue;
            for (const FormRecordMetadata::Reference& reference : record.mReferences)
                add(record, reference.mTarget, reference.mSubRecordType, reference.mOccurrence);
            if (record.mParent)
                add(record, *record.mParent, fourCC("GRUP"), 0);
            if (record.mEnableParent
                && std::none_of(record.mReferences.begin(), record.mReferences.end(), [&](const auto& reference) {
                       return reference.mTarget == *record.mEnableParent && reference.mSubRecordType == fourCC("XESP");
                   }))
                add(record, *record.mEnableParent, fourCC("XESP"), 0);
        }
        return result;
    }

    std::vector<std::vector<FormKey>> FormKeyIndex::enableParentCycles() const
    {
        enum class Visit : std::uint8_t
        {
            Unseen,
            Active,
            Done,
        };
        std::map<FormKey, Visit> visits;
        std::vector<FormKey> stack;
        std::vector<std::vector<FormKey>> result;

        const auto visit = [&](const auto& self, const FormKey& key) -> void {
            const FormRecordMetadata* record = resolve(key);
            if (record == nullptr)
                return;
            Visit& state = visits[key];
            if (state == Visit::Done)
                return;
            if (state == Visit::Active)
            {
                const auto start = std::find(stack.begin(), stack.end(), key);
                if (start != stack.end())
                {
                    std::vector<FormKey> cycle(start, stack.end());
                    rotateCycleToCanonicalStart(cycle);
                    result.push_back(std::move(cycle));
                }
                return;
            }

            state = Visit::Active;
            stack.push_back(key);
            if (record->mEnableParent && resolve(*record->mEnableParent) != nullptr)
                self(self, *record->mEnableParent);
            stack.pop_back();
            state = Visit::Done;
        };

        for (const auto& [key, history] : mRecords)
            visit(visit, key);
        std::sort(result.begin(), result.end());
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }

    std::size_t FormKeyIndex::revisionCount() const
    {
        std::size_t result = 0;
        for (const auto& [key, history] : mRecords)
            result += history.size();
        return result;
    }

    std::size_t FormKeyIndex::referenceCount() const
    {
        std::size_t result = 0;
        for (const auto& [key, history] : mRecords)
            for (const FormRecordMetadata& record : history)
                result += record.mReferences.size() + static_cast<std::size_t>(record.mParent.has_value());
        return result;
    }

    std::vector<std::uint8_t> FormKeyIndex::serialize() const
    {
        if (mRecords.size() > sMaxSerializedCollectionSize)
            throw std::length_error("FormKey graph has too many keys to serialize");

        BinaryWriter writer;
        for (const std::uint8_t value : sFormKeyIndexMagic)
            writer.uint8(value);
        writer.uint32(sFormKeyIndexVersion);
        writer.uint32(static_cast<std::uint32_t>(mRecords.size()));
        for (const auto& [key, history] : mRecords)
        {
            if (history.size() > sMaxSerializedCollectionSize)
                throw std::length_error("FormKey graph has too many override revisions");
            writeKey(writer, key);
            writer.uint32(static_cast<std::uint32_t>(history.size()));
            for (const FormRecordMetadata& record : history)
            {
                writer.string(record.mWinningPlugin);
                writer.uint32(record.mRecordType);
                writer.uint8(record.mDeleted ? 1 : 0);
                writer.uint8(static_cast<std::uint8_t>(record.mChildKind));
                writer.uint8(record.mEnableParent.has_value() ? 1 : 0);
                if (record.mEnableParent)
                    writeKey(writer, *record.mEnableParent);
                writer.uint8(record.mEnableParentInverted ? 1 : 0);
                writer.uint8(record.mParent.has_value() ? 1 : 0);
                if (record.mParent)
                    writeKey(writer, *record.mParent);
                if (record.mReferences.size() > sMaxSerializedCollectionSize)
                    throw std::length_error("FormKey graph revision has too many references");
                writer.uint32(static_cast<std::uint32_t>(record.mReferences.size()));
                for (const FormRecordMetadata::Reference& reference : record.mReferences)
                {
                    writeKey(writer, reference.mTarget);
                    writer.uint32(reference.mSubRecordType);
                    writer.uint32(reference.mOccurrence);
                }
            }
        }
        return writer.take();
    }

    FormKeyIndex FormKeyIndex::deserialize(std::span<const std::uint8_t> data)
    {
        BinaryReader reader(data);
        for (const std::uint8_t expected : sFormKeyIndexMagic)
            if (reader.uint8() != expected)
                throw std::runtime_error("Invalid FormKey graph state magic");
        if (reader.uint32() != sFormKeyIndexVersion)
            throw std::runtime_error("Unsupported FormKey graph state version");

        FormKeyIndex result;
        std::set<FormKey> keys;
        const std::uint32_t keyCount = reader.collectionSize();
        for (std::uint32_t i = 0; i < keyCount; ++i)
        {
            const FormKey key = readKey(reader);
            if (key.isNull() || !keys.insert(key).second)
                throw std::runtime_error("Invalid or duplicate key in FormKey graph state");
            const std::uint32_t historySize = reader.collectionSize();
            if (historySize == 0)
                throw std::runtime_error("FormKey graph state contains an empty override history");
            for (std::uint32_t j = 0; j < historySize; ++j)
            {
                FormRecordMetadata record;
                record.mKey = key;
                record.mWinningPlugin = reader.string();
                record.mRecordType = reader.uint32();
                record.mDeleted = reader.uint8() != 0;
                const std::uint8_t childKind = reader.uint8();
                if (childKind > static_cast<std::uint8_t>(FormChildKind::VisibleDistant))
                    throw std::runtime_error("Invalid child kind in FormKey graph state");
                record.mChildKind = static_cast<FormChildKind>(childKind);
                if (reader.uint8() != 0)
                    record.mEnableParent = readKey(reader);
                record.mEnableParentInverted = reader.uint8() != 0;
                if (reader.uint8() != 0)
                    record.mParent = readKey(reader);
                const std::uint32_t referenceCount = reader.collectionSize();
                record.mReferences.reserve(referenceCount);
                for (std::uint32_t k = 0; k < referenceCount; ++k)
                {
                    FormRecordMetadata::Reference reference;
                    reference.mTarget = readKey(reader);
                    reference.mSubRecordType = reader.uint32();
                    reference.mOccurrence = reader.uint32();
                    record.mReferences.push_back(std::move(reference));
                }
                result.apply(std::move(record));
            }
        }
        if (!reader.empty())
            throw std::runtime_error("Trailing data in FormKey graph state");
        return result;
    }

    std::string FormKeyIndex::canonicalJson() const
    {
        std::ostringstream stream;
        stream << "{\"schema_version\":1,\"records\":[";
        bool firstKey = true;
        for (const auto& [key, history] : mRecords)
        {
            if (!firstKey)
                stream << ',';
            firstKey = false;
            stream << "{\"key\":\"" << jsonEscape(key.serialize()) << "\",\"history\":[";
            bool firstRevision = true;
            for (const FormRecordMetadata& record : history)
            {
                if (!firstRevision)
                    stream << ',';
                firstRevision = false;
                stream << "{\"plugin\":\"" << jsonEscape(record.mWinningPlugin) << "\",\"type\":"
                       << record.mRecordType << ",\"deleted\":" << (record.mDeleted ? "true" : "false")
                       << ",\"child_kind\":" << static_cast<unsigned>(record.mChildKind)
                       << ",\"enable_parent\":";
                if (record.mEnableParent)
                    stream << '\"' << jsonEscape(record.mEnableParent->serialize()) << '\"';
                else
                    stream << "null";
                stream << ",\"enable_parent_inverted\":"
                       << (record.mEnableParentInverted ? "true" : "false") << ",\"parent\":";
                if (record.mParent)
                    stream << '\"' << jsonEscape(record.mParent->serialize()) << '\"';
                else
                    stream << "null";
                stream << ",\"references\":[";
                bool firstReference = true;
                for (const FormRecordMetadata::Reference& reference : record.mReferences)
                {
                    if (!firstReference)
                        stream << ',';
                    firstReference = false;
                    stream << "{\"target\":\"" << jsonEscape(reference.mTarget.serialize()) << "\",\"subrecord\":"
                           << reference.mSubRecordType << ",\"occurrence\":" << reference.mOccurrence << '}';
                }
                stream << "]}";
            }
            stream << "]}";
        }
        stream << "]}";
        return stream.str();
    }
}
