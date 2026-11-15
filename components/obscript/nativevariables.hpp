#ifndef OPENMW_COMPONENTS_OBSCRIPT_NATIVEVARIABLES_H
#define OPENMW_COMPONENTS_OBSCRIPT_NATIVEVARIABLES_H

#include <components/esm4/script.hpp>
#include <components/misc/strings/algorithm.hpp>

#include "program.hpp"

#include <optional>

namespace ObScript
{
    // CTDA/CTDT use SLSD IDs, not source declaration offsets. Resolve through
    // SCVR names without changing the VM's persisted source-local layout.
    inline std::optional<std::size_t> nativeLocalIndex(
        const ESM4::ScriptDefinition& definition, const Program& program, std::int32_t nativeIndex)
    {
        if (nativeIndex <= 0)
            return std::nullopt;
        const ESM4::ScriptLocalVariableData* variable = nullptr;
        for (const auto& entry : definition.localVarData)
            if (entry.index == static_cast<std::uint32_t>(nativeIndex))
            {
                // Original scripts can repeat the same native ID/name (for
                // example ArenaGrandChampionMatchScript's FightOver). These
                // aliases still identify one source local. Conflicting names
                // cannot be mapped to a unique persisted VM local.
                if (variable != nullptr
                    && !Misc::StringUtils::ciEqual(variable->variableName, entry.variableName))
                    return std::nullopt;
                variable = &entry;
            }
        if (variable == nullptr || variable->variableName.empty())
            return std::nullopt;
        // Match Compiler::findLocal: vanilla sources can repeat a declaration,
        // and compiled reads/writes target its first source slot. Preserve the
        // complete persisted layout rather than inventing another local owner.
        for (std::size_t i = 0; i < program.mLocals.size(); ++i)
            if (Misc::StringUtils::ciEqual(program.mLocals[i].mName, variable->variableName))
                return i;
        return std::nullopt;
    }
}

#endif
