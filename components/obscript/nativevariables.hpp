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
                if (variable != nullptr)
                    return std::nullopt;
                variable = &entry;
            }
        if (variable == nullptr || variable->variableName.empty())
            return std::nullopt;
        std::optional<std::size_t> result;
        for (std::size_t i = 0; i < program.mLocals.size(); ++i)
            if (Misc::StringUtils::ciEqual(program.mLocals[i].mName, variable->variableName))
            {
                if (result)
                    return std::nullopt;
                result = i;
            }
        return result;
    }
}

#endif
