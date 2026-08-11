#ifndef OPENMW_COMPONENTS_OBSCRIPT_CORPUS_H
#define OPENMW_COMPONENTS_OBSCRIPT_CORPUS_H

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <components/esm/formkey.hpp>
#include <components/esm4/loadinfo.hpp>
#include <components/esm4/loadqust.hpp>
#include <components/esm4/loadscpt.hpp>

namespace ObScript
{
    enum class ExecutionContext : std::uint8_t
    {
        Object,
        Global,
        Quest,
        Effect,
        DialogueResult,
        QuestResult,
    };

    std::string_view toString(ExecutionContext value);

    struct ScriptUnitId
    {
        ESM::FormKey mOwner;
        std::string mRevisionPlugin;
        ExecutionContext mContext = ExecutionContext::Object;
        std::optional<std::uint16_t> mStage;
        std::optional<std::uint32_t> mStageEntry;
        std::uint32_t mOrdinal = 0;

        std::string serialize() const;
        friend bool operator==(const ScriptUnitId&, const ScriptUnitId&) = default;
        friend bool operator<(const ScriptUnitId& left, const ScriptUnitId& right)
        {
            return left.serialize() < right.serialize();
        }
    };

    struct ScriptUnit
    {
        ScriptUnitId mId;
        std::string mEditorId;
        ESM4::ScriptDefinition mDefinition;
    };

    class Corpus
    {
    public:
        void add(const ESM4::Script& value, std::string_view revisionPlugin);
        void add(const ESM4::DialogInfo& value, std::string_view revisionPlugin);
        void add(const ESM4::Quest& value, std::string_view revisionPlugin);

        const std::vector<ScriptUnit>& units() const { return mUnits; }
        std::size_t sourceCount() const;
        std::size_t compiledCount() const;
        std::map<ExecutionContext, std::size_t> contextCounts() const;
        void finalize();

    private:
        void addDefinitions(const ESM::FormKey& owner, std::string_view editorId, std::string_view revisionPlugin,
            ExecutionContext context, const std::vector<ESM4::ScriptDefinition>& definitions);

        std::vector<ScriptUnit> mUnits;
        bool mFinalized = false;
    };
}

#endif
