#include "corpus.hpp"

#include "ast.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace ObScript
{
    std::string_view toString(ExecutionContext value)
    {
        switch (value)
        {
            case ExecutionContext::Object:
                return "object";
            case ExecutionContext::Global:
                return "global";
            case ExecutionContext::Quest:
                return "quest";
            case ExecutionContext::Effect:
                return "effect";
            case ExecutionContext::DialogueResult:
                return "dialogue-result";
            case ExecutionContext::QuestResult:
                return "quest-result";
        }
        return "invalid";
    }

    std::string ScriptUnitId::serialize() const
    {
        std::string result
            = mOwner.serialize() + "@" + lowerCase(mRevisionPlugin) + "/" + std::string(toString(mContext));
        if (mStage)
            result += "/stage=" + std::to_string(*mStage);
        if (mStageEntry)
            result += "/entry=" + std::to_string(*mStageEntry);
        result += "/unit=" + std::to_string(mOrdinal);
        return result;
    }

    void Corpus::add(const ESM4::Script& value, std::string_view revisionPlugin)
    {
        ExecutionContext context = ExecutionContext::Object;
        if (value.mScript.hasHeader)
        {
            if (value.mScript.scriptHeader.type == 1)
                context = ExecutionContext::Quest;
            else if (value.mScript.scriptHeader.type == 0x100)
                context = ExecutionContext::Effect;
        }
        addDefinitions(value.mFormKey, value.mEditorId, revisionPlugin, context, { value.mScript });
    }

    void Corpus::add(const ESM4::DialogInfo& value, std::string_view revisionPlugin)
    {
        addDefinitions(value.mFormKey, value.mEditorId, revisionPlugin, ExecutionContext::DialogueResult,
            value.mResultScripts);
    }

    void Corpus::add(const ESM4::Quest& value, std::string_view revisionPlugin)
    {
        addDefinitions(
            value.mFormKey, value.mEditorId, revisionPlugin, ExecutionContext::QuestResult, value.mResultScripts);
    }

    void Corpus::addDefinitions(const ESM::FormKey& owner, std::string_view editorId, std::string_view revisionPlugin,
        ExecutionContext context, const std::vector<ESM4::ScriptDefinition>& definitions)
    {
        if (mFinalized)
            throw std::logic_error("Cannot add to a finalized ObScript corpus");
        if (owner.isNull())
            throw std::invalid_argument("ObScript corpus unit has a null owning form");
        std::uint32_t ordinal = 0;
        for (const ESM4::ScriptDefinition& definition : definitions)
        {
            if (!definition.hasPayload())
                continue;
            ScriptUnit unit;
            unit.mId.mOwner = owner;
            unit.mId.mRevisionPlugin = std::string(revisionPlugin);
            unit.mId.mContext = context;
            unit.mId.mStage = definition.stage;
            unit.mId.mStageEntry = definition.stageEntry;
            unit.mId.mOrdinal = ordinal++;
            unit.mEditorId = std::string(editorId);
            unit.mDefinition = definition;
            mUnits.push_back(std::move(unit));
        }
    }

    std::size_t Corpus::sourceCount() const
    {
        return std::count_if(mUnits.begin(), mUnits.end(),
            [](const ScriptUnit& value) { return value.mDefinition.sourceData.has_value(); });
    }

    std::size_t Corpus::compiledCount() const
    {
        return std::count_if(mUnits.begin(), mUnits.end(),
            [](const ScriptUnit& value) { return value.mDefinition.compiledData.has_value(); });
    }

    std::map<ExecutionContext, std::size_t> Corpus::contextCounts() const
    {
        std::map<ExecutionContext, std::size_t> result;
        for (const ScriptUnit& unit : mUnits)
            ++result[unit.mId.mContext];
        return result;
    }

    void Corpus::finalize()
    {
        std::sort(mUnits.begin(), mUnits.end(),
            [](const ScriptUnit& left, const ScriptUnit& right) { return left.mId < right.mId; });
        for (std::size_t i = 1; i < mUnits.size(); ++i)
        {
            if (mUnits[i - 1].mId == mUnits[i].mId)
                throw std::runtime_error("Duplicate ObScript corpus identity: " + mUnits[i].mId.serialize());
        }
        mFinalized = true;
    }
}
