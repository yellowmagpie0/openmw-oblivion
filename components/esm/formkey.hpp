#ifndef OPENMW_COMPONENTS_ESM_FORMKEY_H
#define OPENMW_COMPONENTS_ESM_FORMKEY_H

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "formid.hpp"

namespace ESM
{
    enum class FormKeyKind : std::uint8_t
    {
        Null,
        Content,
        Dynamic,
    };

    // Load-order-independent TES4 identity. Content keys name the plugin that
    // originally defined a form, while dynamic keys use a durable namespace
    // and serial number suitable for native saves.
    struct FormKey
    {
        FormKeyKind mKind = FormKeyKind::Null;
        std::string mNamespace;
        std::uint64_t mValue = 0;

        static FormKey content(std::string_view plugin, std::uint32_t localId);
        static FormKey dynamic(std::string_view nameSpace, std::uint64_t serial);
        static FormKey deserialize(std::string_view value);

        bool isNull() const { return mKind == FormKeyKind::Null; }
        bool isContent() const { return mKind == FormKeyKind::Content; }
        bool isDynamic() const { return mKind == FormKeyKind::Dynamic; }
        std::uint32_t localId() const;
        std::string serialize() const;

        friend bool operator==(const FormKey&, const FormKey&) = default;
        friend bool operator<(const FormKey& left, const FormKey& right)
        {
            return std::tie(left.mKind, left.mNamespace, left.mValue)
                < std::tie(right.mKind, right.mNamespace, right.mValue);
        }
    };

    std::string normalizePluginName(std::string_view value);
    std::ostream& operator<<(std::ostream& stream, const FormKey& value);

    class FormKeyResolver
    {
    public:
        explicit FormKeyResolver(std::vector<std::string> contentFiles);

        FormKey toFormKey(FormId value) const;
        std::optional<FormId> toFormId(const FormKey& value) const;

        // Resolve a raw on-disk FormID using its source file and ordered master list.
        static FormKey resolveRaw(FormId value, std::string_view sourcePlugin,
            const std::vector<std::string>& masters);

        const std::vector<std::string>& contentFiles() const { return mContentFiles; }

    private:
        std::vector<std::string> mContentFiles;
        std::map<std::string, std::uint32_t, std::less<>> mContentFileIndices;
    };

    class DynamicFormKeyAllocator
    {
    public:
        explicit DynamicFormKeyAllocator(std::string_view nameSpace, std::uint64_t nextSerial = 1);

        FormKey allocate();
        std::uint64_t nextSerial() const { return mNextSerial; }

    private:
        std::string mNamespace;
        std::uint64_t mNextSerial;
    };

    enum class FormChildKind : std::uint8_t
    {
        None,
        Persistent,
        Temporary,
        VisibleDistant,
    };

    struct FormRecordMetadata
    {
        FormKey mKey;
        std::string mWinningPlugin;
        std::uint32_t mRecordType = 0;
        bool mDeleted = false;
        FormChildKind mChildKind = FormChildKind::None;
        std::optional<FormKey> mEnableParent;
        bool mEnableParentInverted = false;
    };

    // Ordered override graph. Applying records in load order makes the final
    // entry the winner while retaining history for diagnostics and tests.
    class FormKeyIndex
    {
    public:
        void apply(FormRecordMetadata value);
        const FormRecordMetadata* winner(const FormKey& key) const;
        const FormRecordMetadata* resolve(const FormKey& key) const;
        const std::vector<FormRecordMetadata>* history(const FormKey& key) const;
        std::vector<FormKey> unresolvedEnableParents() const;

    private:
        std::map<FormKey, std::vector<FormRecordMetadata>> mRecords;
    };
}

namespace std
{
    template <>
    struct hash<ESM::FormKey>
    {
        std::size_t operator()(const ESM::FormKey& value) const noexcept
        {
            std::size_t result = std::hash<std::string>{}(value.mNamespace);
            result ^= std::hash<std::uint64_t>{}(value.mValue) + 0x9e3779b9 + (result << 6) + (result >> 2);
            result ^= std::hash<unsigned>{}(static_cast<unsigned>(value.mKind)) + 0x9e3779b9 + (result << 6)
                + (result >> 2);
            return result;
        }
    };
}

#endif
