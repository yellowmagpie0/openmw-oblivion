#include "formkey.hpp"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

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
}
